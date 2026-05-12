/*
 * Solidify - texture push-pull processing utility
 * Copyright (c) 2023-2026 Erium Vladlen.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "pch.h"

#include "ui.h"

#include "processing.h"
#include "settings.h"

static std::atomic<bool> g_isProcessing = false;
static std::atomic<bool> g_dragging     = false;
static std::atomic<float> g_progress    = 0.0f;
static std::mutex g_statusMutex;
static std::string g_statusText   = "Waiting for user inputs...";
static bool g_openEncoderSettings = false;

static void
SetStatus(std::string text)
{
    std::lock_guard<std::mutex> lock(g_statusMutex);
    g_statusText = std::move(text);
}

void
SetDragging(bool dragging)
{
    g_dragging = dragging;
}

bool
IsDragging()
{
    return g_dragging;
}

void
StartProcessing(const std::vector<std::string>& files)
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
static void
MenuRadio(const char* label, T& variable, T value)
{
    if (ImGui::MenuItem(label, nullptr, variable == value)) {
        variable = value;
        spdlog::info("{} set to {}", label, static_cast<int>(value));
    }
}

static void
MenuRadioInt(const char* label, int& variable, int value)
{
    if (ImGui::MenuItem(label, nullptr, variable == value)) {
        variable = value;
        spdlog::info("{} set to {}", label, value);
    }
}

static void
SetAlphaMode(uint mode, const char* label)
{
    settings.alphaMode = mode;
    SetStatus(std::string("Alpha/Mask set to ") + label);

    if (mode == 2) {
        settings.isSolidify = false;
        settings.normMode   = 0;
        settings.repairMode = 0;
    }
}

static void
SetRepairMode(uint mode, const char* label)
{
    settings.repairMode = mode;
    SetStatus(std::string("Repair mode set to ") + label);
    if (mode != 0) {
        settings.normMode = 0;
    }
}

static constexpr const char* kSwapBasisLabels[6] = { "XYZ", "XZY", "YXZ", "YZX", "ZXY", "ZYX" };
static constexpr int kSwapBasisOrders[6][3]      = {
    { 0, 1, 2 }, { 0, 2, 1 }, { 1, 0, 2 }, { 1, 2, 0 }, { 2, 0, 1 }, { 2, 1, 0 },
};

static char
AxisName(int axis)
{
    return static_cast<char>('X' + axis);
}

static std::string
SwapInvertLabel(uint basis, uint invertMask)
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

static bool
SwapInvertIsDisabled()
{
    return settings.swapBasis == 0 && settings.swapInvertMask == 0;
}

static void
SetSwapBasis(uint basis)
{
    settings.swapBasis = std::clamp<uint>(basis, 0, 5);
    if (SwapInvertIsDisabled()) {
        SetStatus("Swap and invert disabled");
    } else {
        SetStatus(std::string("Swap basis set to ") + kSwapBasisLabels[settings.swapBasis]);
    }
}

static void
SetSwapInvertMask(uint invertMask)
{
    settings.swapInvertMask = std::clamp<uint>(invertMask, 0, 7);
    if (SwapInvertIsDisabled()) {
        SetStatus("Swap and invert disabled");
    } else {
        SetStatus(std::string("Swap/invert set to ") + SwapInvertLabel(settings.swapBasis, settings.swapInvertMask));
    }
}

static void
SetGrayscaleMode(uint mode, const char* label)
{
    settings.grayscaleMode = mode;
    SetStatus(std::string("Grayscale set to ") + label);
}

static void
ApplyVerbosity()
{
    const int verbosity = std::clamp<int>(static_cast<int>(settings.verbosity), 0, 5);
    spdlog::set_level(static_cast<spdlog::level::level_enum>(5 - verbosity));
}

static void
ApplyConsoleVisibility()
{
#ifdef _WIN32
    ShowWindow(GetConsoleWindow(), settings.conEnable ? SW_SHOW : SW_HIDE);
#endif
}

static constexpr const char* kTiffCompressionLabels[] = { "ZIP/Deflate", "LZW", "PackBits", "None" };
static constexpr const char* kExrCompressionLabels[]  = {
    "ZIP", "ZIPS", "PIZ", "PXR24", "RLE", "B44", "B44A", "DWAA", "DWAB", "HTJ2K256", "HTJ2K32", "None",
};
static constexpr const char* kPngStrategyLabels[] = {
    "Default", "Filtered", "Huffman", "RLE", "Fixed", "PNG fast", "None",
};
static constexpr const char* kJpegSubsamplingLabels[] = { "4:4:4", "4:2:2", "4:2:0", "4:1:1" };

static void
DisabledDash()
{
    ImGui::BeginDisabled();
    ImGui::TextUnformatted("-");
    ImGui::EndDisabled();
}

static void
SetControlWidth(float width)
{
    ImGui::SetNextItemWidth(width);
}

static void
RenderEncoderSettingsWindow()
{
    if (!g_openEncoderSettings) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(820.0f, 430.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Encoder Settings", &g_openEncoderSettings)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTable("EncoderSettingsTable", 4,
                          ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Format", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn("Codec", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Extra", ImGuiTableColumnFlags_WidthStretch, 1.15f);
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("TIFF");
        ImGui::TableSetColumnIndex(1);
        SetControlWidth(170.0f);
        ImGui::Combo("##tiffCompression", &settings.tiffCompression, kTiffCompressionLabels,
                     IM_ARRAYSIZE(kTiffCompressionLabels));
        ImGui::TableSetColumnIndex(2);
        if (settings.tiffCompression == TiffCompression_Zip) {
            SetControlWidth(150.0f);
            ImGui::SliderInt("##tiffZipLevel", &settings.tiffZipLevel, 1, 9);
        } else {
            DisabledDash();
        }
        ImGui::TableSetColumnIndex(3);
        ImGui::BeginDisabled();
        ImGui::TextUnformatted("JPEG unavailable");
        ImGui::EndDisabled();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("OpenEXR");
        ImGui::TableSetColumnIndex(1);
        SetControlWidth(170.0f);
        ImGui::Combo("##exrCompression", &settings.exrCompression, kExrCompressionLabels,
                     IM_ARRAYSIZE(kExrCompressionLabels));
        ImGui::TableSetColumnIndex(2);
        if (settings.exrCompression == ExrCompression_Zip || settings.exrCompression == ExrCompression_Zips) {
            SetControlWidth(150.0f);
            ImGui::SliderInt("##exrZipLevel", &settings.exrZipLevel, 1, 9);
        } else if (settings.exrCompression == ExrCompression_Dwaa || settings.exrCompression == ExrCompression_Dwab) {
            SetControlWidth(150.0f);
            ImGui::SliderInt("##exrDwaLevel", &settings.exrDwaLevel, 1, 100);
        } else {
            DisabledDash();
        }
        ImGui::TableSetColumnIndex(3);
        DisabledDash();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("PNG");
        ImGui::TableSetColumnIndex(1);
        SetControlWidth(170.0f);
        ImGui::Combo("##pngStrategy", &settings.pngStrategy, kPngStrategyLabels, IM_ARRAYSIZE(kPngStrategyLabels));
        ImGui::TableSetColumnIndex(2);
        if (settings.pngStrategy != PngCompression_Fast && settings.pngStrategy != PngCompression_None) {
            SetControlWidth(150.0f);
            ImGui::SliderInt("##pngLevel", &settings.pngCompressionLevel, 0, 9);
        } else {
            DisabledDash();
        }
        ImGui::TableSetColumnIndex(3);
        DisabledDash();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("JPEG");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted("JPEG");
        ImGui::TableSetColumnIndex(2);
        SetControlWidth(150.0f);
        ImGui::SliderInt("##jpegQuality", &settings.jpegQuality, 1, 100);
        ImGui::TableSetColumnIndex(3);
        SetControlWidth(150.0f);
        ImGui::Combo("##jpegSubsampling", &settings.jpegSubsampling, kJpegSubsamplingLabels,
                     IM_ARRAYSIZE(kJpegSubsamplingLabels));

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("JPEG-2000");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted("OpenJPEG");
        ImGui::TableSetColumnIndex(2);
        DisabledDash();
        ImGui::TableSetColumnIndex(3);
        DisabledDash();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("HTJ2K");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted("OpenJPH");
        ImGui::TableSetColumnIndex(2);
        SetControlWidth(150.0f);
        ImGui::DragFloat("##jpeg2000QStep", &settings.jpeg2000QStep, 0.001f, -1.0f, 10.0f, "%.4f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("0 or less uses reversible output.");
        }
        ImGui::TableSetColumnIndex(3);
        DisabledDash();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("HEIC");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted("HEIC");
        ImGui::TableSetColumnIndex(2);
        SetControlWidth(150.0f);
        ImGui::SliderInt("##heicQuality", &settings.heicQuality, 1, 100);
        ImGui::TableSetColumnIndex(3);
        DisabledDash();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("JPEG XL");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted("JPEG XL");
        ImGui::TableSetColumnIndex(2);
        SetControlWidth(150.0f);
        ImGui::SliderInt("##jpegxlQuality", &settings.jpegxlQuality, 1, 100);
        ImGui::TableSetColumnIndex(3);
        SetControlWidth(92.0f);
        ImGui::SliderInt("Effort##jpegxlEffort", &settings.jpegxlEffort, 1, 9);
        ImGui::SameLine();
        SetControlWidth(92.0f);
        ImGui::SliderInt("Speed##jpegxlSpeed", &settings.jpegxlSpeed, 0, 4);

        ImGui::EndTable();
    }

    settings.tiffZipLevel        = std::clamp(settings.tiffZipLevel, 1, 9);
    settings.exrZipLevel         = std::clamp(settings.exrZipLevel, 1, 9);
    settings.exrDwaLevel         = std::clamp(settings.exrDwaLevel, 1, 100);
    settings.pngCompressionLevel = std::clamp(settings.pngCompressionLevel, 0, 9);
    settings.jpegQuality         = std::clamp(settings.jpegQuality, 1, 100);
    settings.jpeg2000QStep       = std::clamp(settings.jpeg2000QStep, -1.0f, 10.0f);
    settings.heicQuality         = std::clamp(settings.heicQuality, 1, 100);
    settings.jpegxlQuality       = std::clamp(settings.jpegxlQuality, 1, 100);
    settings.jpegxlEffort        = std::clamp(settings.jpegxlEffort, 1, 9);
    settings.jpegxlSpeed         = std::clamp(settings.jpegxlSpeed, 0, 4);

    ImGui::Separator();
    if (ImGui::Button("Reset", ImVec2(120.0f, 0.0f))) {
        resetEncoderSettingsToDefaults();
        SetStatus("Encoder settings reset to config defaults.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Close", ImVec2(120.0f, 0.0f))) {
        g_openEncoderSettings = false;
    }
    ImGui::End();
}

void
AppMenuBar()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Files")) {
            if (ImGui::MenuItem("Reload Config")) {
                if (loadSettingsDefaults(settingsConfigPath)) {
                    ApplyConsoleVisibility();
                    ApplyVerbosity();
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
            if (ImGui::MenuItem("Use Alpha", nullptr, settings.useAlpha)) {
                settings.useAlpha = !settings.useAlpha;
                SetStatus(settings.useAlpha ? "Use Alpha Enabled" : "Use Alpha Disabled");
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
                const ImGuiStyle& style = ImGui::GetStyle();
                const float targetFrameHeight = ImGui::GetFrameHeight() / 1.5f;
                const float arbitraryPaddingY = std::max(0.0f, (targetFrameHeight - ImGui::GetFontSize()) * 0.5f);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, arbitraryPaddingY));
                ImGui::SetNextItemWidth(220.0f);
                if (ImGui::InputFloat3("Weights", settings.grayscaleWeights, "%.4f")) {
                    settings.grayscaleMode = 7;
                }
                ImGui::PopStyleVar();
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
                MenuRadioInt("HTJ2K", settings.fileFormat, 5);
                MenuRadioInt("HEIC", settings.fileFormat, 6);
                MenuRadioInt("JPEG XL", settings.fileFormat, 7);
                MenuRadioInt("PPM", settings.fileFormat, 8);
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

            ImGui::Separator();
            if (ImGui::MenuItem("Encoder Settings...")) {
                g_openEncoderSettings = true;
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Settings")) {
            if (ImGui::MenuItem("Enable Console", nullptr, settings.conEnable)) {
                settings.conEnable = !settings.conEnable;
                ApplyConsoleVisibility();
            }
            if (ImGui::MenuItem("Reset")) {
                resetSettingsToDefaults();
                ApplyConsoleVisibility();
                ApplyVerbosity();
                SetStatus("Settings reset to config defaults.");
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

void
RenderUI()
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

        ImVec2 dropAreaSize = ImVec2(content_width,
                                     regionSize.y - progressBarHeight - statusHeight - 2.0f * pad - 2.0f * space);

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
            ImGui::BeginChild("StatusArea", ImVec2(content_width, statusHeight),
                              ImGuiChildFlags_AlwaysUseWindowPadding);

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
    RenderEncoderSettingsWindow();
}
