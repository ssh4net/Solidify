# Repository Guidelines

## Project Structure & Module Organization

This repository contains a Windows C++ GUI utility for texture processing. The Visual Studio solution is `Solidify.sln`; the main project is `Solidify/Solidify.vcxproj`. Application source lives in `Solidify/src/`, with entry point `main.cpp`, UI code in `ui.*`, image loading/saving in `imageio.*`, processing logic in `processing.*`, and runtime settings in `settings.*` plus `sldf_config.toml`. Qt-generated files `moc_ui.cpp` and `qrc_gui.cpp` are checked in and produced from `ui.h` and `gui.qrc`. Icons and Windows resources are under `Solidify/` and `Solidify/src/`.

## Build, Test, and Development Commands

- `msbuild Solidify.sln /p:Configuration=Debug /p:Platform=x64` builds the debug executable.
- `msbuild Solidify.sln /p:Configuration=Release /p:Platform=x64` builds the optimized release executable.
- `devenv Solidify.sln` opens the project in Visual Studio for UI/resource editing and debugging.

The project targets MSVC v143, C++20, Qt 6, OpenImageIO, spdlog, toml11, and related image codec libraries. Local paths such as `C:\Qt\6.9.1\msvc2022_64`, `E:\DVS`, and `E:\GH` are currently embedded in the project file; adjust them locally before building if your dependency layout differs.

## Coding Style & Naming Conventions

Use C++20 and keep `pch.h` as the first project include in `.cpp` files. Match the existing brace style: function and control-block braces open on the same line. Prefer clear lower-camel-case function names such as `loadSettings` and `getOutName`, PascalCase for types such as `Settings` and `MainWindow`, and short file names grouped by feature. Keep comments useful and specific; avoid broad rewrites or formatting-only churn.

## Testing Guidelines

There is no dedicated automated test suite in this repository. For behavior changes, build both `Debug|x64` and `Release|x64`, run `Solidify.exe`, and validate drag-and-drop processing with representative RGBA, grayscale, normal-map, and external `_mask` or `_alpha` files. Confirm outputs use the expected suffixes such as `_fill`, `_norm`, `_rep`, `_mask`, or `_conv`.

## Commit & Pull Request Guidelines

Recent commits use short, imperative or descriptive subjects, for example `Add JPEG XL file format support` and `Refactor to use precompiled headers and spdlog`. Keep commit subjects concise and focused on one change. Pull requests should describe the user-visible behavior, list dependency or project-file changes, mention manual test images/formats used, and include screenshots when the UI changes.

## Agent-Specific Instructions

Do not overwrite generated Qt files unless updating the corresponding source (`ui.h`, `gui.qrc`, or `gui.ui`) and rebuilding generation outputs intentionally. Preserve local Visual Studio and dependency-path assumptions unless the task is specifically to make the build more portable.
