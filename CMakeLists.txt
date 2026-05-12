cmake_minimum_required(VERSION 3.20)

project(Solidify LANGUAGES CXX C VERSION 2.0.5)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Available build configurations" FORCE)

option(SOLIDIFY_STATIC_LINK "Link against static third-party libraries" ON)
set(CMAKE_DEBUG_POSTFIX "d" CACHE STRING "Postfix for Debug configuration")
set(SOLIDIFY_TOML11_DIR "" CACHE PATH "Path to toml11 headers if not provided by your toolchain")
set(SOLIDIFY_DEPENDENCY_PREFIX "" CACHE PATH "Optional prefix containing Solidify dependencies")
set(IMGUI_INCLUDE_DIR "" CACHE PATH "Path to Dear ImGui source directory")
set(DND_GLFW_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/dnd_glfw" CACHE PATH "Path to dnd_glfw headers")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules"
)

if(UNIX AND NOT APPLE)
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    endif()
endif()

if(SOLIDIFY_DEPENDENCY_PREFIX)
    list(PREPEND CMAKE_PREFIX_PATH "${SOLIDIFY_DEPENDENCY_PREFIX}")
    list(PREPEND CMAKE_LIBRARY_PATH "${SOLIDIFY_DEPENDENCY_PREFIX}/lib")
    list(PREPEND CMAKE_INCLUDE_PATH "${SOLIDIFY_DEPENDENCY_PREFIX}/include")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(UNIX AND NOT APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>")
    add_link_options("-stdlib=libc++")
endif()

if(MSVC AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

find_package(Threads REQUIRED)
find_package(glfw3 3.3 REQUIRED)

if(TARGET glfw3::glfw3)
elseif(TARGET glfw)
    add_library(glfw3::glfw3 ALIAS glfw)
elseif(TARGET glfw3)
    add_library(glfw3::glfw3 ALIAS glfw3)
elseif(TARGET GLFW3::glfw3)
    add_library(glfw3::glfw3 ALIAS GLFW3::glfw3)
else()
    message(FATAL_ERROR "GLFW3 found but no glfw target is available.")
endif()

find_package(OpenGL REQUIRED)

if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    find_package(OpenMP QUIET)
endif()

if(NOT IMGUI_INCLUDE_DIR)
    find_path(IMGUI_INCLUDE_DIR
        NAMES imgui.h
        HINTS
            "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui"
            "$ENV{IMGUI_DIR}"
            "$ENV{IMGUI_INCLUDE_DIR}"
        PATH_SUFFIXES
            imgui
    )
endif()

if(NOT EXISTS "${IMGUI_INCLUDE_DIR}/imgui.h"
   OR NOT EXISTS "${IMGUI_INCLUDE_DIR}/backends/imgui_impl_glfw.cpp"
   OR NOT EXISTS "${IMGUI_INCLUDE_DIR}/backends/imgui_impl_opengl3.cpp")
    message(FATAL_ERROR "ImGui not found. Set IMGUI_INCLUDE_DIR to the Dear ImGui source directory.")
endif()

if(NOT EXISTS "${DND_GLFW_INCLUDE_DIR}/dnd_glfw.h")
    message(FATAL_ERROR "dnd_glfw.h not found. Set DND_GLFW_INCLUDE_DIR to its location.")
endif()

set(SOLIDIFY_DND_GLFW_TARGET "")
set(SOLIDIFY_DND_GLFW_SOURCE "")
if(APPLE)
    enable_language(OBJCXX)
    if(NOT TARGET dnd_glfw AND EXISTS "${DND_GLFW_INCLUDE_DIR}/CMakeLists.txt")
        add_subdirectory("${DND_GLFW_INCLUDE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/dnd_glfw")
    endif()
    if(TARGET dnd_glfw)
        if(TARGET dnd_glfw_macos)
            target_link_libraries(dnd_glfw_macos PUBLIC glfw3::glfw3)
        endif()
        set(SOLIDIFY_DND_GLFW_TARGET dnd_glfw)
    elseif(EXISTS "${DND_GLFW_INCLUDE_DIR}/dnd_glfw_macos.mm")
        set(SOLIDIFY_DND_GLFW_SOURCE "${DND_GLFW_INCLUDE_DIR}/dnd_glfw_macos.mm")
    else()
        message(FATAL_ERROR "dnd_glfw macOS implementation not found. Expected dnd_glfw_macos.mm or a dnd_glfw CMake target.")
    endif()
endif()

if(NOT SOLIDIFY_TOML11_DIR)
    find_path(TOML11_INCLUDE_DIR
        NAMES toml.hpp toml11/toml.hpp
        HINTS
            ${CMAKE_PREFIX_PATH}/include
            /usr/include
            /usr/local/include
    )
    if(TOML11_INCLUDE_DIR)
        set(SOLIDIFY_TOML11_DIR "${TOML11_INCLUDE_DIR}")
    endif()
endif()

if(NOT SOLIDIFY_TOML11_DIR)
    message(FATAL_ERROR "toml11 not found. Set SOLIDIFY_TOML11_DIR to the toml11 include directory.")
endif()

find_package(Imath CONFIG REQUIRED)
find_package(OpenEXR CONFIG REQUIRED)

find_package(GIF QUIET)
if(NOT TARGET GIF::GIF)
    find_library(GIF_LIBRARY NAMES gif HINTS ${CMAKE_LIBRARY_PATH})
    find_path(GIF_INCLUDE_DIR NAMES gif_lib.h HINTS ${CMAKE_INCLUDE_PATH})
    if(GIF_LIBRARY)
        add_library(GIF::GIF UNKNOWN IMPORTED)
        set_target_properties(GIF::GIF PROPERTIES IMPORTED_LOCATION "${GIF_LIBRARY}")
        if(GIF_INCLUDE_DIR)
            set_target_properties(GIF::GIF PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${GIF_INCLUDE_DIR}")
        endif()
    endif()
endif()

find_package(Ptex CONFIG QUIET)
if(NOT TARGET Ptex::Ptex_static AND TARGET Ptex::Ptex)
    add_library(Ptex::Ptex_static ALIAS Ptex::Ptex)
endif()

find_package(OpenJPEG CONFIG QUIET)
if(NOT TARGET openjp2)
    find_library(OPENJPEG_LIBRARY NAMES openjp2 HINTS ${CMAKE_LIBRARY_PATH})
    find_path(OPENJPEG_INCLUDE_DIR NAMES openjpeg.h PATH_SUFFIXES openjpeg-2.5 HINTS ${CMAKE_INCLUDE_PATH})
    if(OPENJPEG_LIBRARY)
        add_library(openjp2 UNKNOWN IMPORTED)
        set_target_properties(openjp2 PROPERTIES IMPORTED_LOCATION "${OPENJPEG_LIBRARY}")
        if(OPENJPEG_INCLUDE_DIR)
            set_target_properties(openjp2 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${OPENJPEG_INCLUDE_DIR}")
        endif()
    endif()
endif()

find_package(openjph CONFIG QUIET)
if(NOT TARGET openjph)
    find_library(OPENJPH_LIBRARY NAMES openjph libopenjph HINTS ${CMAKE_LIBRARY_PATH})
    find_path(OPENJPH_INCLUDE_DIR NAMES ojph_arch.h PATH_SUFFIXES openjph HINTS ${CMAKE_INCLUDE_PATH})
    if(OPENJPH_LIBRARY)
        add_library(openjph UNKNOWN IMPORTED)
        set_target_properties(openjph PROPERTIES IMPORTED_LOCATION "${OPENJPH_LIBRARY}")
        if(OPENJPH_INCLUDE_DIR)
            set_target_properties(openjph PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${OPENJPH_INCLUDE_DIR}")
        endif()
    endif()
endif()

find_package(libheif CONFIG QUIET)
if(NOT TARGET heif)
    find_library(HEIF_LIBRARY NAMES heif libheif HINTS ${CMAKE_LIBRARY_PATH})
    find_path(HEIF_INCLUDE_DIR NAMES libheif/heif.h HINTS ${CMAKE_INCLUDE_PATH})
    if(HEIF_LIBRARY)
        add_library(heif UNKNOWN IMPORTED)
        set_target_properties(heif PROPERTIES IMPORTED_LOCATION "${HEIF_LIBRARY}")
        if(HEIF_INCLUDE_DIR)
            set_target_properties(heif PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${HEIF_INCLUDE_DIR}")
        endif()
    endif()
endif()

find_package(pugixml CONFIG QUIET)
if(NOT TARGET pugixml::pugixml)
    find_library(PUGIXML_LIBRARY NAMES pugixml HINTS ${CMAKE_LIBRARY_PATH})
    find_path(PUGIXML_INCLUDE_DIR NAMES pugixml.hpp HINTS ${CMAKE_INCLUDE_PATH})
    if(PUGIXML_LIBRARY)
        add_library(pugixml::pugixml UNKNOWN IMPORTED)
        set_target_properties(pugixml::pugixml PROPERTIES IMPORTED_LOCATION "${PUGIXML_LIBRARY}")
        if(PUGIXML_INCLUDE_DIR)
            set_target_properties(pugixml::pugixml PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${PUGIXML_INCLUDE_DIR}")
        endif()
    endif()
endif()

find_package(libuhdr CONFIG QUIET)
if(NOT TARGET libuhdr::libuhdr)
    find_library(UHDR_LIBRARY NAMES uhdr libuhdr HINTS ${CMAKE_LIBRARY_PATH})
    find_path(UHDR_INCLUDE_DIR NAMES ultrahdr/ultrahdr_api.h HINTS ${CMAKE_INCLUDE_PATH})
    if(UHDR_LIBRARY)
        add_library(libuhdr::libuhdr UNKNOWN IMPORTED)
        set_target_properties(libuhdr::libuhdr PROPERTIES IMPORTED_LOCATION "${UHDR_LIBRARY}")
        if(UHDR_INCLUDE_DIR)
            set_target_properties(libuhdr::libuhdr PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${UHDR_INCLUDE_DIR}")
        endif()
    endif()
endif()

find_package(BZip2 CONFIG QUIET)
if(NOT TARGET BZip2::BZip2)
    find_library(BZIP2_LIBRARY NAMES bz2_static bz2 bzip2 HINTS ${CMAKE_LIBRARY_PATH})
    find_path(BZIP2_INCLUDE_DIR NAMES bzlib.h HINTS ${CMAKE_INCLUDE_PATH})
    if(BZIP2_LIBRARY)
        add_library(BZip2::BZip2 UNKNOWN IMPORTED)
        set_target_properties(BZip2::BZip2 PROPERTIES IMPORTED_LOCATION "${BZIP2_LIBRARY}")
        if(BZIP2_INCLUDE_DIR)
            set_target_properties(BZip2::BZip2 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${BZIP2_INCLUDE_DIR}")
        endif()
    endif()
endif()

find_package(freetype CONFIG QUIET)
if(NOT TARGET Freetype::Freetype AND NOT TARGET freetype)
    find_package(Freetype QUIET)
endif()
if(NOT TARGET Freetype::Freetype AND NOT TARGET freetype)
    find_library(FREETYPE_LIBRARY NAMES freetype libfreetype HINTS ${CMAKE_LIBRARY_PATH})
    find_path(FREETYPE_INCLUDE_DIR NAMES ft2build.h PATH_SUFFIXES freetype2 HINTS ${CMAKE_INCLUDE_PATH})
    if(FREETYPE_LIBRARY)
        add_library(Freetype::Freetype UNKNOWN IMPORTED)
        set_target_properties(Freetype::Freetype PROPERTIES IMPORTED_LOCATION "${FREETYPE_LIBRARY}")
        if(FREETYPE_INCLUDE_DIR)
            set_target_properties(Freetype::Freetype PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${FREETYPE_INCLUDE_DIR}")
        endif()
    endif()
endif()

find_package(dng_sdk CONFIG QUIET)
if(NOT TARGET dng_sdk::dng_sdk)
    find_library(DNG_SDK_LIBRARY NAMES dng_sdk HINTS ${CMAKE_LIBRARY_PATH})
    find_path(DNG_SDK_INCLUDE_DIR NAMES dng_host.h PATH_SUFFIXES dng_sdk HINTS ${CMAKE_INCLUDE_PATH})
    if(DNG_SDK_LIBRARY)
        add_library(dng_sdk::dng_sdk UNKNOWN IMPORTED)
        set_target_properties(dng_sdk::dng_sdk PROPERTIES IMPORTED_LOCATION "${DNG_SDK_LIBRARY}")
        if(DNG_SDK_INCLUDE_DIR)
            set_target_properties(dng_sdk::dng_sdk PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${DNG_SDK_INCLUDE_DIR}")
        endif()
    endif()
endif()

find_library(WEBP_LIBRARY NAMES webp libwebp HINTS ${CMAKE_LIBRARY_PATH})
find_library(WEBPDEMUX_LIBRARY NAMES webpdemux libwebpdemux HINTS ${CMAKE_LIBRARY_PATH})
find_library(WEBPMUX_LIBRARY NAMES webpmux libwebpmux HINTS ${CMAKE_LIBRARY_PATH})
find_path(WEBP_INCLUDE_DIR NAMES webp/decode.h HINTS ${CMAKE_INCLUDE_PATH})
if(WEBP_LIBRARY AND NOT TARGET WebP::webp)
    add_library(WebP::webp UNKNOWN IMPORTED)
    set_target_properties(WebP::webp PROPERTIES IMPORTED_LOCATION "${WEBP_LIBRARY}")
    if(WEBP_INCLUDE_DIR)
        set_target_properties(WebP::webp PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${WEBP_INCLUDE_DIR}")
    endif()
endif()
if(WEBPDEMUX_LIBRARY AND NOT TARGET WebP::webpdemux)
    add_library(WebP::webpdemux UNKNOWN IMPORTED)
    set_target_properties(WebP::webpdemux PROPERTIES IMPORTED_LOCATION "${WEBPDEMUX_LIBRARY}")
    if(WEBP_INCLUDE_DIR)
        set_target_properties(WebP::webpdemux PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${WEBP_INCLUDE_DIR}")
    endif()
endif()
if(WEBPMUX_LIBRARY AND NOT TARGET WebP::libwebpmux)
    add_library(WebP::libwebpmux UNKNOWN IMPORTED)
    set_target_properties(WebP::libwebpmux PROPERTIES IMPORTED_LOCATION "${WEBPMUX_LIBRARY}")
    if(WEBP_INCLUDE_DIR)
        set_target_properties(WebP::libwebpmux PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${WEBP_INCLUDE_DIR}")
    endif()
endif()

find_package(OpenImageIO CONFIG REQUIRED)
if(NOT TARGET hwy::hwy)
    find_package(hwy CONFIG REQUIRED)
endif()
find_package(spdlog CONFIG REQUIRED)

find_library(SOLIDIFY_OPENJPH_LINK_LIBRARY NAMES openjph libopenjph HINTS ${CMAKE_LIBRARY_PATH})
find_library(SOLIDIFY_BZIP2_LINK_LIBRARY NAMES bz2_static bz2 bzip2 HINTS ${CMAKE_LIBRARY_PATH})
find_library(SOLIDIFY_PUGIXML_LINK_LIBRARY NAMES pugixml HINTS ${CMAKE_LIBRARY_PATH})
find_library(SOLIDIFY_UHDR_LINK_LIBRARY NAMES uhdr libuhdr HINTS ${CMAKE_LIBRARY_PATH})

set(SOLIDIFY_FREETYPE_LINK_ITEM "")
if(TARGET Freetype::Freetype)
    set(SOLIDIFY_FREETYPE_LINK_ITEM Freetype::Freetype)
elseif(TARGET freetype)
    set(SOLIDIFY_FREETYPE_LINK_ITEM freetype)
endif()

set(SOLIDIFY_OPENJPH_LINK_ITEM "")
if(SOLIDIFY_OPENJPH_LINK_LIBRARY)
    set(SOLIDIFY_OPENJPH_LINK_ITEM "${SOLIDIFY_OPENJPH_LINK_LIBRARY}")
elseif(TARGET openjph)
    set(SOLIDIFY_OPENJPH_LINK_ITEM openjph)
endif()

set(SOLIDIFY_PUGIXML_LINK_ITEM "")
if(TARGET pugixml::pugixml)
    set(SOLIDIFY_PUGIXML_LINK_ITEM pugixml::pugixml)
elseif(SOLIDIFY_PUGIXML_LINK_LIBRARY)
    set(SOLIDIFY_PUGIXML_LINK_ITEM "${SOLIDIFY_PUGIXML_LINK_LIBRARY}")
endif()

set(SOLIDIFY_UHDR_LINK_ITEM "")
if(TARGET libuhdr::libuhdr)
    set(SOLIDIFY_UHDR_LINK_ITEM libuhdr::libuhdr)
elseif(SOLIDIFY_UHDR_LINK_LIBRARY)
    set(SOLIDIFY_UHDR_LINK_ITEM "${SOLIDIFY_UHDR_LINK_LIBRARY}")
endif()

set(SOLIDIFY_BZIP2_LINK_ITEM "")
if(TARGET BZip2::BZip2)
    set(SOLIDIFY_BZIP2_LINK_ITEM BZip2::BZip2)
elseif(SOLIDIFY_BZIP2_LINK_LIBRARY)
    set(SOLIDIFY_BZIP2_LINK_ITEM "${SOLIDIFY_BZIP2_LINK_LIBRARY}")
endif()

function(solidify_link_oiio_static_tail target_name)
    target_link_libraries(${target_name} PRIVATE
        ${SOLIDIFY_FREETYPE_LINK_ITEM}
        ${SOLIDIFY_PUGIXML_LINK_ITEM}
        ${SOLIDIFY_UHDR_LINK_ITEM}
        ${SOLIDIFY_BZIP2_LINK_ITEM}
    )

    if(APPLE AND SOLIDIFY_OPENJPH_LINK_LIBRARY)
        target_link_libraries(${target_name} PRIVATE
            "-Wl,-force_load,${SOLIDIFY_OPENJPH_LINK_LIBRARY}"
        )
    elseif(SOLIDIFY_OPENJPH_LINK_ITEM)
        target_link_libraries(${target_name} PRIVATE ${SOLIDIFY_OPENJPH_LINK_ITEM})
    endif()

    if(UNIX AND NOT APPLE AND SOLIDIFY_BZIP2_LINK_LIBRARY)
        target_link_libraries(${target_name} PRIVATE
            "-Wl,--whole-archive"
            "${SOLIDIFY_BZIP2_LINK_LIBRARY}"
            "-Wl,--no-whole-archive"
        )
    endif()
endfunction()

file(GLOB IMGUI_SOURCES CONFIGURE_DEPENDS
    "${IMGUI_INCLUDE_DIR}/imgui.cpp"
    "${IMGUI_INCLUDE_DIR}/imgui_draw.cpp"
    "${IMGUI_INCLUDE_DIR}/imgui_tables.cpp"
    "${IMGUI_INCLUDE_DIR}/imgui_widgets.cpp"
    "${IMGUI_INCLUDE_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_INCLUDE_DIR}/backends/imgui_impl_opengl3.cpp"
)

set(SOLIDIFY_GUI_FONT "${CMAKE_CURRENT_SOURCE_DIR}/external/Fira/FiraSans-Regular.otf")
if(NOT EXISTS "${SOLIDIFY_GUI_FONT}")
    message(FATAL_ERROR "Expected font not found at ${SOLIDIFY_GUI_FONT}")
endif()

set(SOLIDIFY_EMBEDDED_FONT_SOURCE "")
if(NOT WIN32)
    set(SOLIDIFY_EMBEDDED_FONT_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/solidify_embedded_font.cpp")
    add_custom_command(
        OUTPUT "${SOLIDIFY_EMBEDDED_FONT_SOURCE}"
        COMMAND ${CMAKE_COMMAND}
                -DINPUT_FILE=${SOLIDIFY_GUI_FONT}
                -DOUTPUT_FILE=${SOLIDIFY_EMBEDDED_FONT_SOURCE}
                -DSYMBOL=solidify_embedded_font
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_binary.cmake"
        DEPENDS "${SOLIDIFY_GUI_FONT}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_binary.cmake"
        COMMENT "Embed FiraSans font"
        VERBATIM
    )
endif()

set(SOLIDIFY_SOURCES
    Solidify/src/main.cpp
    Solidify/src/ui.cpp
    Solidify/src/processing.cpp
    Solidify/src/settings.cpp
    Solidify/src/imageio.cpp
    Solidify/src/imageops.cpp
    Solidify/src/pushpull.cpp
    Solidify/src/solidify.cpp
    Solidify/src/pch.cpp
    ${SOLIDIFY_DND_GLFW_SOURCE}
    ${SOLIDIFY_EMBEDDED_FONT_SOURCE}
)

if(WIN32)
    list(APPEND SOLIDIFY_SOURCES Solidify/Solidify.rc)
endif()

add_executable(Solidify ${SOLIDIFY_SOURCES} ${IMGUI_SOURCES})
set_target_properties(Solidify PROPERTIES DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}")

target_include_directories(Solidify PRIVATE
    Solidify/src
    "${DND_GLFW_INCLUDE_DIR}"
    "${IMGUI_INCLUDE_DIR}"
    "${IMGUI_INCLUDE_DIR}/backends"
    "${SOLIDIFY_TOML11_DIR}"
)

target_link_libraries(Solidify PRIVATE
    Threads::Threads
    OpenImageIO::OpenImageIO
    hwy::hwy
    spdlog::spdlog
    glfw3::glfw3
    OpenGL::GL
)

if(SOLIDIFY_DND_GLFW_TARGET)
    target_link_libraries(Solidify PRIVATE "${SOLIDIFY_DND_GLFW_TARGET}")
endif()

solidify_link_oiio_static_tail(Solidify)

if(UNIX AND NOT APPLE)
    target_link_libraries(Solidify PRIVATE
        dl
        m
        ${X11_LIBRARIES}
        ${X11_Xrandr_LIB}
        ${X11_Xinerama_LIB}
        ${X11_Xcursor_LIB}
        ${X11_Xi_LIB}
    )

    if(OpenMP_CXX_FOUND)
        target_link_libraries(Solidify PRIVATE OpenMP::OpenMP_CXX)
    endif()
endif()

if(WIN32)
    target_compile_definitions(Solidify PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        JXL_STATIC_DEFINE=1
        OPJ_STATIC
        LIBDE265_STATIC_BUILD
        KVZ_STATIC_LIB
        LIBHEIF_STATIC_BUILD
        LIBRAW_NODLL
        OIIO_STATIC_DEFINE=1
        PCRE2_STATIC
    )

    target_link_libraries(Solidify PRIVATE
        advapi32 Authz comdlg32 crypt32 d3d11 d3d12 D3d9 Dwmapi dxgi dxguid
        gdi32 Imm32 kernel32 mfplat mfuuid mpr ncrypt Ole32 ole32 Oleaut32
        psapi RuntimeObject secur32 Setupapi Shcore shell32 Shlwapi strmiids
        user32 userenv Uuid UxTheme Version WindowsApp windowscodecs Winmm
        winspool Ws2_32 Wtsapi32
    )
endif()

if(MSVC)
    set_property(TARGET Solidify PROPERTY MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}")
    target_compile_options(Solidify PRIVATE
        "/permissive-"
        "/Zc:__cplusplus"
        "/std:c++20"
        "/utf-8"
    )
endif()

function(solidify_configure_image_tool target_name)
    target_compile_definitions(${target_name} PRIVATE
        SOLIDIFY_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tmp"
    )
    target_include_directories(${target_name} PRIVATE
        Solidify/src
        "${DND_GLFW_INCLUDE_DIR}"
        "${IMGUI_INCLUDE_DIR}"
        "${IMGUI_INCLUDE_DIR}/backends"
        "${SOLIDIFY_TOML11_DIR}"
    )
    target_link_libraries(${target_name} PRIVATE
        Threads::Threads
        OpenImageIO::OpenImageIO
        hwy::hwy
        spdlog::spdlog
    )
    solidify_link_oiio_static_tail(${target_name})
    if(UNIX AND NOT APPLE)
        target_link_libraries(${target_name} PRIVATE
            dl
            m
        )
    endif()
endfunction()

include(CTest)
if(BUILD_TESTING)
    add_executable(solidify_image_tests
        tests/solidify_image_tests.cpp
        Solidify/src/imageops.cpp
    )
    solidify_configure_image_tool(solidify_image_tests)
    add_test(NAME solidify_image_tests COMMAND solidify_image_tests)

    add_executable(solidify_pushpull_tests
        tests/solidify_pushpull_tests.cpp
        Solidify/src/pushpull.cpp
    )
    solidify_configure_image_tool(solidify_pushpull_tests)
    add_test(NAME solidify_pushpull_tests COMMAND solidify_pushpull_tests)

    add_executable(solidify_settings_tests
        tests/solidify_settings_tests.cpp
        Solidify/src/settings.cpp
    )
    solidify_configure_image_tool(solidify_settings_tests)
    add_test(NAME solidify_settings_tests COMMAND solidify_settings_tests)

    add_executable(solidify_processing_tests
        tests/solidify_processing_tests.cpp
        Solidify/src/processing.cpp
        Solidify/src/settings.cpp
        Solidify/src/imageio.cpp
        Solidify/src/imageops.cpp
        Solidify/src/pushpull.cpp
        Solidify/src/solidify.cpp
    )
    solidify_configure_image_tool(solidify_processing_tests)
    add_test(NAME solidify_processing_tests COMMAND solidify_processing_tests)

    add_executable(solidify_pushpull_bench
        tests/solidify_pushpull_bench.cpp
        Solidify/src/pushpull.cpp
    )
    solidify_configure_image_tool(solidify_pushpull_bench)
endif()

add_custom_command(TARGET Solidify POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Solidify/src/sldf_config.toml"
            "$<TARGET_FILE_DIR:Solidify>/sldf_config.toml"
)

include(GNUInstallDirs)
install(TARGETS Solidify RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES Solidify/src/sldf_config.toml DESTINATION ${CMAKE_INSTALL_BINDIR})
