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

#ifdef _WIN32
#    include "../resource.h"
#endif

#include "settings.h"
#include "ui.h"

#ifndef _WIN32
extern const unsigned char solidify_embedded_font[];
extern const unsigned int solidify_embedded_font_size;
#endif

static void
glfw_error_callback(int error, const char* description)
{
    spdlog::error("Glfw Error {}: {}", error, description);
}

static void
onDragEnter(GLFWwindow* window, const dnd_glfw::DragEvent& event, void* userData)
{
    (void)window;
    (void)userData;
    if (event.kind == dnd_glfw::PayloadKind::Files) {
        SetDragging(true);
    }
}

static void
onDragLeave(GLFWwindow* window, void* userData)
{
    (void)window;
    (void)userData;
    SetDragging(false);
}

static void
onDrop(GLFWwindow* window, const dnd_glfw::DropEvent& event, void* userData)
{
    (void)window;
    (void)userData;
    SetDragging(false);
    if (event.kind == dnd_glfw::PayloadKind::Files) {
        StartProcessing(event.paths);
    }
}

static bool
loadEmbeddedGuiFont(ImGuiIO& io, const ImWchar* glyphRanges)
{
#ifdef _WIN32
    HRSRC fontResource = FindResourceW(nullptr, MAKEINTRESOURCEW(IDR_FIRA_SANS_REGULAR), MAKEINTRESOURCEW(10));
    if (fontResource == nullptr) {
        return false;
    }
    HGLOBAL fontHandle = LoadResource(nullptr, fontResource);
    if (fontHandle == nullptr) {
        return false;
    }
    const DWORD fontSize = SizeofResource(nullptr, fontResource);
    const void* fontData = LockResource(fontHandle);
    if (fontData == nullptr || fontSize == 0 || fontSize > static_cast<DWORD>(std::numeric_limits<int>::max())) {
        return false;
    }
    void* fontBytes         = const_cast<void*>(fontData);
    const int fontByteCount = static_cast<int>(fontSize);
#else
    if (solidify_embedded_font_size == 0
        || solidify_embedded_font_size > static_cast<unsigned int>(std::numeric_limits<int>::max())) {
        return false;
    }
    void* fontBytes         = const_cast<unsigned char*>(solidify_embedded_font);
    const int fontByteCount = static_cast<int>(solidify_embedded_font_size);
#endif

    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;
    ImFont* font = io.Fonts->AddFontFromMemoryTTF(fontBytes, fontByteCount, 16.0f, &fontConfig, glyphRanges);
    if (font == nullptr) {
        return false;
    }
    io.FontDefault = font;
    return true;
}

int
main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

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

    if (!loadSettingsDefaults("sldf_config.toml")) {
        spdlog::error("Can not load [sldf_config.toml]. Using default settings.");
        settings.reSettings();
        settingsDefaults = settings;
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
        int windowWidth  = 0;
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

    ImGuiPlatformIO& platform_io                          = ImGui::GetPlatformIO();
    static void (*s_originalCreateWindow)(ImGuiViewport*) = platform_io.Platform_CreateWindow;
    platform_io.Platform_CreateWindow                     = [](ImGuiViewport* viewport) {
        if (s_originalCreateWindow) {
            s_originalCreateWindow(viewport);
        }
        GLFWwindow* glfw_window = static_cast<GLFWwindow*>(viewport->PlatformHandle);
        if (glfw_window != nullptr) {
            glfwSetWindowAttrib(glfw_window, GLFW_FLOATING, GLFW_TRUE);
        }
    };

    const ImWchar* glyphRanges = io.Fonts->GetGlyphRangesDefault();
    if (!loadEmbeddedGuiFont(io, glyphRanges)) {
        spdlog::warn("Failed to load embedded FiraSans font; using default ImGui font.");
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
