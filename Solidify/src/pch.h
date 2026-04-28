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
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cctype>
#include <cmath>
#include <condition_variable>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef _WIN32
#    define WIN32_LEAN_AND_MEAN
#    define NOMINMAX
#    include <windows.h>
#    include <tlhelp32.h>
#    include <process.h>
#else
#    include <signal.h>
#    include <sys/types.h>
#    include <unistd.h>
#endif

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#ifndef GL_CLAMP_TO_EDGE
#    define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_RGBA16F
#    define GL_RGBA16F 0x881A
#endif
#ifndef GL_HALF_FLOAT
#    define GL_HALF_FLOAT 0x140B
#endif

#include <dnd_glfw.h>

#include <OpenImageIO/color.h>
#include <OpenImageIO/half.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/span.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>

#include <spdlog/fmt/ostr.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <toml.hpp>
