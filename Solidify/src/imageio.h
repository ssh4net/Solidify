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

#include "timer.h"
#include "processing.h"

#include <OpenImageIO/half.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/imageio.h>

using namespace OIIO;

struct OIIOProgressContext {
    const SolidifyProgressCallback* callback = nullptr;
    std::string status;
    float base  = 0.0f;
    float scale = 1.0f;
};

bool m_progress_callback(void* opaque_data, float portion_done);

TypeDesc getTypeDesc(int bit_depth);
std::string formatText(TypeDesc format);
void formatFromBuff(ImageBuf& buf);

std::pair<ImageBuf, ImageBuf> mask_load(const std::string& mask_file, const SolidifyProgressCallback& progressCallback);
bool img_load(ImageBuf& outBuf, const std::string& inputFileName, bool external_alpha,
              const SolidifyProgressCallback& progressCallback);

void debugImageBufWrite(const ImageBuf& buf, const std::string& filename);

bool recalc_normal(ImageBuf& dst, const ImageBuf& A, uint channel, int sign, float inCenter, float outCenter,
                   float scale, ROI roi, int nthreads);
ImageBuf recalc_normal(const ImageBuf& A, uint channel, int sign, float inCenter, float outCenter, float scale,
                       ROI roi, int nthreads);
