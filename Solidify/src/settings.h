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

#ifndef SETTINGS_H
#    define SETTINGS_H

#    include <string>
#    include <vector>

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;

enum TiffCompressionMode : int {
    TiffCompression_Zip = 0,
    TiffCompression_Lzw,
    TiffCompression_PackBits,
    TiffCompression_None,
};

enum ExrCompressionMode : int {
    ExrCompression_Zip = 0,
    ExrCompression_Zips,
    ExrCompression_Piz,
    ExrCompression_Pxr24,
    ExrCompression_Rle,
    ExrCompression_B44,
    ExrCompression_B44A,
    ExrCompression_Dwaa,
    ExrCompression_Dwab,
    ExrCompression_Htj2k256,
    ExrCompression_Htj2k32,
    ExrCompression_None,
};

enum PngCompressionStrategy : int {
    PngCompression_Default = 0,
    PngCompression_Filtered,
    PngCompression_Huffman,
    PngCompression_Rle,
    PngCompression_Fixed,
    PngCompression_Fast,
    PngCompression_None,
};

enum JpegSubsamplingMode : int {
    JpegSubsampling_444 = 0,
    JpegSubsampling_422,
    JpegSubsampling_420,
    JpegSubsampling_411,
};

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
    int tiffCompression, tiffZipLevel;
    int exrCompression, exrZipLevel, exrDwaLevel;
    int pngStrategy, pngCompressionLevel;
    int jpegQuality, jpegSubsampling;
    float jpeg2000QStep;
    int heicQuality;
    int jpegxlQuality, jpegxlEffort, jpegxlSpeed;

    std::vector<std::string> normNames, mask_substr, out_formats;

    static constexpr int raw_rot[5]  = { -1, 0, 3, 5, 6 };
    static constexpr uint rngConv[4] = { 0, 1, 2, 3 };

    Settings() { reSettings(); }

    void reSettings()
    {
        conEnable  = true;
        isSolidify = true;

        alphaMode      = 0;
        numThreads     = 3;
        queueLimit     = 0;
        verbosity      = 3;
        normMode       = 1;
        repairMode     = 0;
        swapBasis      = 0;
        swapInvertMask = 0;
        grayscaleMode  = 0;

        rangeMode           = 0;
        fileFormat          = -1;
        defFormat           = 0;
        bitDepth            = -1;
        defBDepth           = 1;
        rawRot              = -1;
        grayscaleWeights[0] = 0.2126f;
        grayscaleWeights[1] = 0.7152f;
        grayscaleWeights[2] = 0.0722f;
        tiffCompression     = TiffCompression_Zip;
        tiffZipLevel        = 6;
        exrCompression      = ExrCompression_Zip;
        exrZipLevel         = 4;
        exrDwaLevel         = 45;
        pngStrategy         = PngCompression_Default;
        pngCompressionLevel = 4;
        jpegQuality         = 100;
        jpegSubsampling     = JpegSubsampling_444;
        jpeg2000QStep       = -1.0f;
        heicQuality         = 100;
        jpegxlQuality       = 100;
        jpegxlEffort        = 7;
        jpegxlSpeed         = 0;

        normNames   = { "normal", "tangent", "object", "world" };
        mask_substr = { "_mask.", "_mask_", "_alpha.", "_alpha_" };
        out_formats = { "tif", "exr", "png", "jpg", "jp2", "jph", "heic", "jxl", "ppm" };
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
extern Settings settingsDefaults;
extern std::string settingsConfigPath;

bool
loadSettings(Settings& settings, const std::string& filename);
bool
loadSettingsDefaults(const std::string& filename);
void
resetSettingsToDefaults();
void
resetEncoderSettingsToDefaults();
void
printSettings(Settings& settings);

#endif
