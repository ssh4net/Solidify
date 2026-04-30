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

#include <OpenImageIO/imagebuf.h>

bool applyChannelSwapInvert(OIIO::ImageBuf& dst, const OIIO::ImageBuf& src, unsigned int basis,
                            unsigned int invertMask, bool signedRange, int nthreads = 0);

bool applyGrayscale(OIIO::ImageBuf& dst, const OIIO::ImageBuf& src, unsigned int mode, const float weights[3],
                    bool preserveAlpha, int nthreads = 0);

