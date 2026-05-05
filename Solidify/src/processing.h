/*
 * Solidify - texture push-pull processing utility
 * Copyright (c) 2023 Erium Vladlen.
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

#include "settings.h"

#include <functional>
#include <string>
#include <vector>

using SolidifyProgressCallback = std::function<void(float, std::string)>;

std::string toLower(const std::string& str);
void getWritableExt(std::string* ext, Settings* settings);
std::string getExtension(const std::string& fileName, Settings* settings);
std::string getOutName(const std::string& fileName, Settings* settings);

bool doProcessing(const std::vector<std::string>& filePaths, SolidifyProgressCallback progressCallback = nullptr);

std::string checkAlpha(const std::vector<std::string>& fileNames);
