/*
 * Solidify (Push Pull) algorithm implementation using OpenImageIO
 * Copyright (c) 2023 Erium Vladlen.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "pch.h"

#include "ui.h"

#include "processing.h"
#include "settings.h"

static std::atomic<bool> g_isProcessing = false;
static std::atomic<bool> g_dragging     = false;
static std::atomic<float> g_progress    = 0.0f;
static std::mutex g_statusMutex;
static std::string g_statusText = "Waiting for user inputs...";

static void SetStatus(std::string text)
{
    std::lock_guard<std::mutex> lock(g_statusMutex);
    g_statusText = std::move(text);
}

void SetDragging(bool dragging)
{
    g_dragging = dragging;
}

bool IsDragging()
{
    return g_dragging;
}

void StartProcessing(const std::vector<std::string>& files)
{
    bool expected = false;
    if (!g_isProcessing.compare_exchange_strong(expected, true)) {
        return;
    }

    g_progress = 0.0f;
    SetStatus("Processing " + std::to_string(files.size()) + " paths...");

    std::vector<std::string> taskFiles = files;
    std::thread([taskFiles]() {
        bool success = doProcessing(taskFiles, [](float p, std::string s) {
            g_progress = std::clamp(p, 0.0f, 1.0f);
            if (!s.empty()) {
                SetStatus(std::move(s));
            }
        });

        g_progress     = success ? 1.0f : 0.0f;
        g_isProcessing = false;
        SetStatus(success ? "Everything Done!" : "Finished with errors.");
    }).detach();
}

template<typename T>
static void MenuRadio(const char* label, T& variable, T value)
{
    if (ImGui::MenuItem(label, nullptr, variable == value)) {
        variable = value;
        spdlog::info("{} set to {}", label, static_cast<int>(value));
    }
}

static void MenuRadioInt(const char* label, int& variable, int value)
{
    if (ImGui::MenuItem(label, nullptr, variable == value)) {
        variable = value;
        spdlog::info("{} set to {}", label, value);
    }
}

static void SetAlphaMode(uint mode, const char* label)
{
    settings.alphaMode = mode;
    SetStatus(std::string("Alpha/Mask set to ") + label);

    if (mode == 2) {
        settings.isSolidify = false;
        settings.normMode   = 0;
        settings.repairMode = 0;
    }
}

static void SetRepairMode(uint mode, const char* label)
{
    settings.repairMode = mode;
    SetStatus(std::string("Repair mode set to ") + label);
    if (mode != 0) {
        settings.normMode = 0;
    }
}

static constexpr const char* kSwapBasisLabels[6] = { "XYZ", "XZY", "YXZ", "YZX", "ZXY", "ZYX" };
static constexpr int kSwapBasisOrders[6][3]      = {
    { 0, 1, 2 },
    { 0, 2, 1 },
    { 1, 0, 2 },
    { 1, 2, 0 },
    { 2, 0, 1 },
    { 2, 1, 0 },
};

static char AxisName(int axis)
{
    return static_cast<char>('X' + axis);
}

static std::string SwapInvertLabel(uint basis, uint invertMask)
{
    basis = std::clamp<uint>(basis, 0, 5);
    std::string label;
    label.reserve(6);
    for (int c = 0; c < 3; ++c) {
        label.push_back((invertMask & (1u << c)) != 0 ? '-' : '+');
        label.push_back(AxisName(kSwapBasisOrders[basis][c]));
    }
    return label;
}

static void SetSwapBasis(uint basis)
{
    settings.swapBasis = std::clamp<uint>(basis, 0, 5);
    SetStatus(std::string("Swap basis set to ") + kSwapBasisLabels[settings.swapBasis]);
}

static void SetSwapInvertMask(uint invertMask)
{
    settings.swapInvertMask = std::clamp<uint>(invertMask, 0, 7);
    SetStatus(std::string("Swap/invert set to ") + SwapInvertLabel(settings.swapBasis, settings.swapInvertMask));
}

static void SetGrayscaleMode(uint mode, const char* label)
{
    settings.grayscaleMode = mode;
    SetStatus(std::string("Grayscale set to ") + label);
}

static void ApplyVerbosity()
{
    const int verbosity = std::clamp<int>(static_cast<int>(settings.verbosity), 0, 5);
    spdlog::set_level(static_cast<spdlog::level::level_enum>(5 - verbosity));
}

void AppMenuBar()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Files")) {
            if (ImGui::MenuItem("Reload Config")) {
                if (loadSettings(settings, "sldf_config.toml")) {
                    printSettings(settings);
                    SetStatus("Config reloaded.");
                } else {
                    SetStatus("Config reload failed. Check console for details.");
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                glfwSetWindowShouldClose(glfwGetCurrentContext(), GLFW_TRUE);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Processing")) {
            if (ImGui::MenuItem("Solidify", nullptr, settings.isSolidify)) {
                settings.isSolidify = !settings.isSolidify;
                SetStatus(settings.isSolidify ? "Solidify Enabled" : "Solidify Disabled");
            }

            if (ImGui::BeginMenu("Alpha/Mask")) {
                if (ImGui::MenuItem("Disable", nullptr, settings.alphaMode == 0)) {
                    SetAlphaMode(0, "Disable");
                }
                if (ImGui::MenuItem("Preserve", nullptr, settings.alphaMode == 1)) {
                    SetAlphaMode(1, "Preserve");
                }
                if (ImGui::MenuItem("Export", nullptr, settings.alphaMode == 2)) {
                    SetAlphaMode(2, "Export");
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Normalize")) {
                MenuRadio("Disable", settings.normMode, static_cast<uint>(0));
                MenuRadio("Smart", settings.normMode, static_cast<uint>(1));
                MenuRadio("Force", settings.normMode, static_cast<uint>(2));
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Repair")) {
                if (ImGui::MenuItem("Disable", nullptr, settings.repairMode == 0)) {
                    SetRepairMode(0, "Disable");
                }
                if (ImGui::MenuItem("X", nullptr, settings.repairMode == 1)) {
                    SetRepairMode(1, "X");
                }
                if (ImGui::MenuItem("Y", nullptr, settings.repairMode == 2)) {
                    SetRepairMode(2, "Y");
                }
                if (ImGui::MenuItem("Z", nullptr, settings.repairMode == 3)) {
                    SetRepairMode(3, "Z");
                }
                if (ImGui::MenuItem("-X", nullptr, settings.repairMode == 4)) {
                    SetRepairMode(4, "-X");
                }
                if (ImGui::MenuItem("-Y", nullptr, settings.repairMode == 5)) {
                    SetRepairMode(5, "-Y");
                }
                if (ImGui::MenuItem("-Z", nullptr, settings.repairMode == 6)) {
                    SetRepairMode(6, "-Z");
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Swap and invert")) {
                if (ImGui::MenuItem("Disable", nullptr, settings.swapBasis == 0 && settings.swapInvertMask == 0)) {
                    settings.swapBasis      = 0;
                    settings.swapInvertMask = 0;
                    SetStatus("Swap and invert disabled");
                }

                if (ImGui::BeginMenu("Basis")) {
                    for (uint basis = 0; basis < 6; ++basis) {
                        if (ImGui::MenuItem(kSwapBasisLabels[basis], nullptr, settings.swapBasis == basis)) {
                            SetSwapBasis(basis);
                        }
                    }
                    ImGui::EndMenu();
                }

                const std::string currentSignLabel = SwapInvertLabel(settings.swapBasis, settings.swapInvertMask);
                if (ImGui::BeginMenu(currentSignLabel.c_str())) {
                    for (uint mask = 0; mask < 8; ++mask) {
                        const std::string label = SwapInvertLabel(settings.swapBasis, mask);
                        if (ImGui::MenuItem(label.c_str(), nullptr, settings.swapInvertMask == mask)) {
                            SetSwapInvertMask(mask);
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Grayscale")) {
                if (ImGui::MenuItem("Disable", nullptr, settings.grayscaleMode == 0)) {
                    SetGrayscaleMode(0, "Disable");
                }
                if (ImGui::MenuItem("R only", nullptr, settings.grayscaleMode == 1)) {
                    SetGrayscaleMode(1, "R only");
                }
                if (ImGui::MenuItem("G only", nullptr, settings.grayscaleMode == 2)) {
                    SetGrayscaleMode(2, "G only");
                }
                if (ImGui::MenuItem("B only", nullptr, settings.grayscaleMode == 3)) {
                    SetGrayscaleMode(3, "B only");
                }
                if (ImGui::MenuItem("Mask (alpha)", nullptr, settings.grayscaleMode == 4)) {
                    SetGrayscaleMode(4, "Mask (alpha)");
                }
                if (ImGui::MenuItem("Luminosity", nullptr, settings.grayscaleMode == 5)) {
                    SetGrayscaleMode(5, "Luminosity");
                }
                if (ImGui::MenuItem("Equal", nullptr, settings.grayscaleMode == 6)) {
                    SetGrayscaleMode(6, "Equal");
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Arbitrary", nullptr, settings.grayscaleMode == 7)) {
                    SetGrayscaleMode(7, "Arbitrary");
                }
                ImGui::SetNextItemWidth(220.0f);
                if (ImGui::InputFloat3("Weights", settings.grayscaleWeights, "%.4f")) {
                    settings.grayscaleMode = 7;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Camera Raw")) {
                MenuRadioInt("Auto EXIF", settings.rawRot, -1);
                MenuRadioInt("0 Horizontal", settings.rawRot, 0);
                MenuRadioInt("180 Horizontal", settings.rawRot, 3);
                MenuRadioInt("-90 Vertical", settings.rawRot, 5);
                MenuRadioInt("+90 Vertical", settings.rawRot, 6);
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Outputs")) {
            if (ImGui::BeginMenu("Floats type")) {
                MenuRadio("Unsigned", settings.rangeMode, static_cast<uint>(0));
                MenuRadio("Signed", settings.rangeMode, static_cast<uint>(1));
                MenuRadio("Signed > Unsigned", settings.rangeMode, static_cast<uint>(2));
                MenuRadio("Unsigned > Signed", settings.rangeMode, static_cast<uint>(3));
                ImGui::EndMenu();
            }

            ImGui::Separator();

            if (ImGui::BeginMenu("Formats")) {
                MenuRadioInt("Original", settings.fileFormat, -1);
                MenuRadioInt("TIFF", settings.fileFormat, 0);
                MenuRadioInt("OpenEXR", settings.fileFormat, 1);
                MenuRadioInt("PNG", settings.fileFormat, 2);
                MenuRadioInt("JPEG", settings.fileFormat, 3);
                MenuRadioInt("JPEG-2000", settings.fileFormat, 4);
                MenuRadioInt("HEIC", settings.fileFormat, 5);
                MenuRadioInt("JPEG XL", settings.fileFormat, 6);
                MenuRadioInt("PPM", settings.fileFormat, 7);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Bits Depth")) {
                MenuRadioInt("Original", settings.bitDepth, -1);
                MenuRadioInt("8 bits int", settings.bitDepth, 0);
                MenuRadioInt("16 bits int", settings.bitDepth, 1);
                MenuRadioInt("32 bits int", settings.bitDepth, 2);
                MenuRadioInt("64 bits int", settings.bitDepth, 3);
                MenuRadioInt("16 bits float", settings.bitDepth, 4);
                MenuRadioInt("32 bits float", settings.bitDepth, 5);
                MenuRadioInt("64 bits float", settings.bitDepth, 6);
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Settings")) {
            if (ImGui::MenuItem("Enable Console", nullptr, settings.conEnable)) {
                settings.conEnable = !settings.conEnable;
#ifdef _WIN32
                ShowWindow(GetConsoleWindow(), settings.conEnable ? SW_SHOW : SW_HIDE);
#endif
            }
            if (ImGui::MenuItem("Print Settings")) {
                printSettings(settings);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Debug")) {
            if (ImGui::BeginMenu("Verbosity")) {
                MenuRadio("0 - Fatal", settings.verbosity, static_cast<uint>(0));
                MenuRadio("1 - Error", settings.verbosity, static_cast<uint>(1));
                MenuRadio("2 - Warning", settings.verbosity, static_cast<uint>(2));
                MenuRadio("3 - Info", settings.verbosity, static_cast<uint>(3));
                MenuRadio("4 - Debug", settings.verbosity, static_cast<uint>(4));
                MenuRadio("5 - Trace", settings.verbosity, static_cast<uint>(5));
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    ImGui::PopStyleVar(3);
}

void RenderUI()
{
    ApplyVerbosity();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
                                    | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar
                                    | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.06f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.09f, 0.09f, 0.09f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 10.0f));

    if (ImGui::Begin("Solidify Main Window", nullptr, window_flags)) {
        AppMenuBar();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));

        ImVec2 regionSize = ImGui::GetContentRegionAvail();

        const float pad               = 10.0f;
        const float space             = 8.0f;
        const float progressBarHeight = 16.0f;
        const float statusHeight      = 100.0f;
        const float content_width     = regionSize.x - 2.0f * pad;
        float cursor_y                = ImGui::GetFrameHeight() + pad * 3.0f;

        ImVec2 dropAreaSize =
            ImVec2(content_width, regionSize.y - progressBarHeight - statusHeight - 2.0f * pad - 2.0f * space);

        {
            ImGui::SetCursorPos(ImVec2(pad, cursor_y));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.094f, 0.094f, 0.094f, 1.0f));
            ImGui::BeginChild("DropArea", dropAreaSize, false);

            const bool isBusy = g_isProcessing.load();
            const char* text  = isBusy ? "Processing files..." : "Drag & drop files here";
            ImGui::SetWindowFontScale(1.5f);
            ImVec2 textSize  = ImGui::CalcTextSize(text);
            ImVec2 childSize = ImGui::GetWindowSize();
            ImGui::SetCursorPos(ImVec2((childSize.x - textSize.x) * 0.5f, (childSize.y - textSize.y) * 0.5f));
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", text);
            ImGui::SetWindowFontScale(1.0f);

            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
        cursor_y += dropAreaSize.y + space;

        {
            ImGui::SetCursorPos(ImVec2(pad, cursor_y));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.02f, 0.72f, 0.8f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
            ImGui::ProgressBar(g_progress.load(), ImVec2(content_width, progressBarHeight), "");
            ImGui::PopStyleColor(2);
        }
        cursor_y += progressBarHeight + space;

        {
            ImGui::SetCursorPos(ImVec2(pad, cursor_y));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.094f, 0.094f, 0.094f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, pad));
            ImGui::BeginChild("StatusArea", ImVec2(content_width, statusHeight), ImGuiChildFlags_AlwaysUseWindowPadding);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.88f, 0.88f, 0.88f, 1.0f));
            std::string statusTextCopy;
            {
                std::lock_guard<std::mutex> lock(g_statusMutex);
                statusTextCopy = g_statusText;
            }
            ImGui::TextUnformatted(statusTextCopy.c_str());

            ImGui::PopStyleColor();
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }

        if (g_dragging) {
            ImDrawList* drawList = ImGui::GetForegroundDrawList();
            ImVec2 pMin          = viewport->Pos;
            ImVec2 pMax          = ImVec2(pMin.x + viewport->Size.x, pMin.y + viewport->Size.y);

            drawList->AddRectFilled(pMin, pMax, IM_COL32(0, 0, 0, 192));

            const char* text = "DROP FILES TO START";
            ImFont* font     = ImGui::GetFont();
            float fontSize   = ImGui::GetFontSize() * 3.0f;

            ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
            ImVec2 textPos  = ImVec2(pMin.x + (viewport->Size.x - textSize.x) * 0.5f,
                                     pMin.y + (viewport->Size.y - textSize.y) * 0.5f);

            drawList->AddText(font, fontSize, textPos, IM_COL32(255, 255, 255, 255), text);
        }

        ImGui::PopStyleVar();
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}
