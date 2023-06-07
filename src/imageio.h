/*
 * Solidify (Push Pull) algorithm implementation using OpenImageIO
 * Copyright (c) 2022 Erium Vladlen.
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

#include <iostream>
#include <iomanip>
#include <string>
#include <math.h>

#include "Timer.h"
#include "ui.h"

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

using namespace OIIO;

void pbar_color_rand(QProgressBar* progressBar);
bool progress_callback(void* opaque_data, float portion_done);

std::pair<ImageBuf, ImageBuf> mask_load(const std::string& mask_file, MainWindow* mainWindow);
bool img_load(ImageBuf& outBuf, const std::string& inputFileName, bool external_alpha, QProgressBar* progressBar, MainWindow* mainWindow);

std::pair<ImageBuf, ImageBuf> mask_load(const std::string& mask_file, MainWindow* mainWindow);