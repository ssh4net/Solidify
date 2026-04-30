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

#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include <vector>

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;

struct Settings {
    bool isSolidify, conEnable;
    uint normMode, rangeMode, repairMode, alphaMode;
    uint swapBasis, swapInvertMask, grayscaleMode;
    int fileFormat, defFormat;
    int bitDepth, defBDepth;
    int rawRot;
    uint numThreads;
    uint queueLimit;
    uint verbosity;
    float grayscaleWeights[3];

    std::vector<std::string> normNames, mask_substr, out_formats;

    const int raw_rot[5] = { -1, 0, 3, 5, 6 };
    const uint rngConv[4] = { 0, 1, 2, 3 };

    Settings()
    {
        reSettings();
    }

    void reSettings()
    {
        conEnable  = true;
        isSolidify = true;

        alphaMode  = 0;
        numThreads = 3;
        queueLimit = 0;
        verbosity  = 3;
        normMode   = 1;
        repairMode = 0;
        swapBasis = 0;
        swapInvertMask = 0;
        grayscaleMode = 0;

        rangeMode = 0;
        fileFormat = -1;
        defFormat  = 0;
        bitDepth   = -1;
        defBDepth  = 1;
        rawRot     = -1;
        grayscaleWeights[0] = 0.2126f;
        grayscaleWeights[1] = 0.7152f;
        grayscaleWeights[2] = 0.0722f;

        normNames   = { "normal", "tangent", "object", "world" };
        mask_substr = { "_mask.", "_mask_", "_alpha.", "_alpha_" };
        out_formats = { "tif", "exr", "png", "jpg", "jp2", "heic", "jxl", "ppm" };
    }

    int getBitDepth() const
    {
        switch (bitDepth) {
        case 0: return 1;
        case 1: return 2;
        case 2: return 4;
        case 3: return 8;
        case 4: return 2;
        case 5: return 4;
        case 6: return 8;
        default: return 0;
        }
    }
};

extern Settings settings;

bool loadSettings(Settings& settings, const std::string& filename);
void printSettings(Settings& settings);

#endif
