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

#include "settings.h"
#include "ui.h"

static void glfw_error_callback(int error, const char* description)
{
    spdlog::error("Glfw Error {}: {}", error, description);
}

static void onDragEnter(GLFWwindow* window, const dnd_glfw::DragEvent& event, void* userData)
{
    (void)window;
    (void)userData;
    if (event.kind == dnd_glfw::PayloadKind::Files) {
        SetDragging(true);
    }
}

static void onDragLeave(GLFWwindow* window, void* userData)
{
    (void)window;
    (void)userData;
    SetDragging(false);
}

static void onDrop(GLFWwindow* window, const dnd_glfw::DropEvent& event, void* userData)
{
    (void)window;
    (void)userData;
    SetDragging(false);
    if (event.kind == dnd_glfw::PayloadKind::Files) {
        StartProcessing(event.paths);
    }
}

static std::string findFontPath(const char* argv0)
{
    std::vector<std::filesystem::path> candidates;
#ifdef SOLIDIFY_GUI_FONT_PATH
    candidates.emplace_back(std::filesystem::path(SOLIDIFY_GUI_FONT_PATH));
#endif

    std::filesystem::path exeDir = std::filesystem::current_path();
    if (argv0 != nullptr && argv0[0] != '\0') {
        std::error_code ec;
        std::filesystem::path exePath = std::filesystem::weakly_canonical(std::filesystem::path(argv0), ec);
        if (!ec && !exePath.empty()) {
            exeDir = exePath.parent_path();
        }
    }

    candidates.emplace_back(exeDir / "fonts" / "FiraSans-Regular.otf");
    candidates.emplace_back(exeDir / ".." / "share" / "solidify" / "fonts" / "FiraSans-Regular.otf");

    for (const std::filesystem::path& candidate : candidates) {
        std::error_code ec;
        if (!candidate.empty() && std::filesystem::exists(candidate, ec)) {
            return candidate.string();
        }
    }

    return {};
}

int main(int argc, char* argv[])
{
    (void)argc;

#ifdef _WIN32
    AllocConsole();
    FILE* ignored = nullptr;
    freopen_s(&ignored, "CONOUT$", "w", stdout);
    freopen_s(&ignored, "CONOUT$", "w", stderr);
#endif

    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("%^[%l]%$<%t> %v");

    time_t timestamp;
    time(&timestamp);

    spdlog::info("Solidify {}.{}.{}", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    spdlog::info("Build from: {} {}", __DATE__, __TIME__);
    spdlog::info("Log started at: {}", ctime(&timestamp));

    if (!loadSettings(settings, "sldf_config.toml")) {
        spdlog::error("Can not load [sldf_config.toml]. Using default settings.");
        settings.reSettings();
    }

#ifdef _WIN32
    ShowWindow(GetConsoleWindow(), (settings.conEnable) ? SW_SHOW : SW_HIDE);
#endif

    printSettings(settings);

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        spdlog::critical("Failed to initialize GLFW");
        return 1;
    }

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(500, 500, "Solidify ToolBox", nullptr, nullptr);
    if (window == nullptr) {
        spdlog::critical("Failed to create GLFW window");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    GLFWmonitor* monitor    = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (mode != nullptr) {
        int windowWidth = 0;
        int windowHeight = 0;
        glfwGetWindowSize(window, &windowWidth, &windowHeight);
        glfwSetWindowPos(window, (mode->width - windowWidth) / 2, (mode->height - windowHeight) / 2);
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();
    ImGuiStyle& style      = ImGui::GetStyle();
    style.FrameBorderSize  = 0.0f;
    style.PopupBorderSize  = 0.0f;
    style.WindowBorderSize = 0.0f;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    static void (*s_originalCreateWindow)(ImGuiViewport*) = platform_io.Platform_CreateWindow;
    platform_io.Platform_CreateWindow = [](ImGuiViewport* viewport) {
        if (s_originalCreateWindow) {
            s_originalCreateWindow(viewport);
        }
        GLFWwindow* glfw_window = static_cast<GLFWwindow*>(viewport->PlatformHandle);
        if (glfw_window != nullptr) {
            glfwSetWindowAttrib(glfw_window, GLFW_FLOATING, GLFW_TRUE);
        }
    };

    const ImWchar* glyphRanges = io.Fonts->GetGlyphRangesDefault();
    std::string fontPath       = findFontPath((argv != nullptr) ? argv[0] : nullptr);
    if (!fontPath.empty()) {
        ImFont* font = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f, nullptr, glyphRanges);
        if (font != nullptr) {
            io.FontDefault = font;
        } else {
            spdlog::warn("Failed to load font at {}, using default ImGui font.", fontPath);
        }
    } else {
        spdlog::warn("No FiraSans font found; using default ImGui font.");
    }

    dnd_glfw::Callbacks dnd_cbs;
    dnd_cbs.dragEnter  = onDragEnter;
    dnd_cbs.dragLeave  = onDragLeave;
    dnd_cbs.drop       = onDrop;
    dnd_cbs.dragCancel = onDragLeave;
    if (!dnd_glfw::init(window, dnd_cbs, nullptr)) {
        spdlog::error("Failed to initialize dnd_glfw");
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        RenderUI();

        ImGui::Render();
        int display_w = 0;
        int display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }

    dnd_glfw::shutdown(window);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
