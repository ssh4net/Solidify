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

#pragma once

#include <filesystem>
#include <string>

inline std::filesystem::path
pathFromUtf8(const std::string& utf8Path)
{
#if defined(__cpp_char8_t)
    std::u8string text;
    text.reserve(utf8Path.size());
    for (unsigned char c : utf8Path) {
        text.push_back(static_cast<char8_t>(c));
    }
    return std::filesystem::path(text);
#else
    return std::filesystem::u8path(utf8Path);
#endif
}

inline std::string
pathToUtf8(const std::filesystem::path& path)
{
#if defined(__cpp_char8_t)
    const std::u8string text = path.u8string();
    std::string result;
    result.reserve(text.size());
    for (char8_t c : text) {
        result.push_back(static_cast<char>(c));
    }
    return result;
#else
    return path.u8string();
#endif
}

inline std::string
pathFilenameUtf8(const std::string& utf8Path)
{
    return pathToUtf8(pathFromUtf8(utf8Path).filename());
}
