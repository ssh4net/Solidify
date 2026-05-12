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

#include "settings.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

namespace fs = std::filesystem;

static int g_failures = 0;

static void expectTrue(bool value, const char* expr, const char* file, int line)
{
    if (!value) {
        std::cerr << file << ':' << line << ": expected " << expr << '\n';
        ++g_failures;
    }
}

#define EXPECT_TRUE(expr) expectTrue((expr), #expr, __FILE__, __LINE__)

static void writeTextFile(const fs::path& path, const char* text)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        std::cerr << "Could not write " << path << '\n';
        ++g_failures;
        return;
    }
    file << text;
}

static void expectConfigA(const Settings& value)
{
    EXPECT_TRUE(value.isSolidify == false);
    EXPECT_TRUE(value.alphaMode == 2);
    EXPECT_TRUE(value.conEnable == false);
    EXPECT_TRUE(value.numThreads == 8);
    EXPECT_TRUE(value.queueLimit == 5);
    EXPECT_TRUE(value.verbosity == 5);
    EXPECT_TRUE(value.mask_substr.size() == 1 && value.mask_substr[0] == "_maskA");
    EXPECT_TRUE(value.normMode == 2);
    EXPECT_TRUE(value.normNames.size() == 1 && value.normNames[0] == "normalA");
    EXPECT_TRUE(value.rangeMode == 3);
    EXPECT_TRUE(value.swapBasis == 5);
    EXPECT_TRUE(value.swapInvertMask == 7);
    EXPECT_TRUE(value.grayscaleMode == 7);
    EXPECT_TRUE(value.grayscaleWeights[0] == 0.1f);
    EXPECT_TRUE(value.grayscaleWeights[1] == 0.2f);
    EXPECT_TRUE(value.grayscaleWeights[2] == 0.7f);
    EXPECT_TRUE(value.defFormat == 7);
    EXPECT_TRUE(value.fileFormat == 6);
    EXPECT_TRUE(value.defBDepth == 5);
    EXPECT_TRUE(value.bitDepth == 4);
    EXPECT_TRUE(value.tiffCompression == TiffCompression_Lzw);
    EXPECT_TRUE(value.tiffZipLevel == 9);
    EXPECT_TRUE(value.exrCompression == ExrCompression_Piz);
    EXPECT_TRUE(value.exrZipLevel == 8);
    EXPECT_TRUE(value.exrDwaLevel == 75);
    EXPECT_TRUE(value.pngStrategy == PngCompression_Rle);
    EXPECT_TRUE(value.pngCompressionLevel == 3);
    EXPECT_TRUE(value.jpegQuality == 77);
    EXPECT_TRUE(value.jpegSubsampling == JpegSubsampling_422);
    EXPECT_TRUE(value.jpeg2000QStep == 0.5f);
    EXPECT_TRUE(value.heicQuality == 66);
    EXPECT_TRUE(value.jpegxlQuality == 55);
    EXPECT_TRUE(value.jpegxlEffort == 4);
    EXPECT_TRUE(value.jpegxlSpeed == 2);
    EXPECT_TRUE(value.rawRot == 6);
}

static void expectConfigB(const Settings& value)
{
    EXPECT_TRUE(value.isSolidify == true);
    EXPECT_TRUE(value.alphaMode == 1);
    EXPECT_TRUE(value.conEnable == true);
    EXPECT_TRUE(value.numThreads == 2);
    EXPECT_TRUE(value.queueLimit == 1);
    EXPECT_TRUE(value.verbosity == 1);
    EXPECT_TRUE(value.mask_substr.size() == 2 && value.mask_substr[0] == "_maskB" && value.mask_substr[1] == "_alphaB");
    EXPECT_TRUE(value.normMode == 0);
    EXPECT_TRUE(value.normNames.size() == 2 && value.normNames[0] == "normalB" && value.normNames[1] == "worldB");
    EXPECT_TRUE(value.rangeMode == 1);
    EXPECT_TRUE(value.swapBasis == 1);
    EXPECT_TRUE(value.swapInvertMask == 2);
    EXPECT_TRUE(value.grayscaleMode == 5);
    EXPECT_TRUE(value.grayscaleWeights[0] == 0.3f);
    EXPECT_TRUE(value.grayscaleWeights[1] == 0.4f);
    EXPECT_TRUE(value.grayscaleWeights[2] == 0.3f);
    EXPECT_TRUE(value.defFormat == 2);
    EXPECT_TRUE(value.fileFormat == -1);
    EXPECT_TRUE(value.defBDepth == 1);
    EXPECT_TRUE(value.bitDepth == -1);
    EXPECT_TRUE(value.tiffCompression == TiffCompression_PackBits);
    EXPECT_TRUE(value.tiffZipLevel == 1);
    EXPECT_TRUE(value.exrCompression == ExrCompression_Dwab);
    EXPECT_TRUE(value.exrZipLevel == 2);
    EXPECT_TRUE(value.exrDwaLevel == 12);
    EXPECT_TRUE(value.pngStrategy == PngCompression_Huffman);
    EXPECT_TRUE(value.pngCompressionLevel == 9);
    EXPECT_TRUE(value.jpegQuality == 88);
    EXPECT_TRUE(value.jpegSubsampling == JpegSubsampling_420);
    EXPECT_TRUE(value.jpeg2000QStep == -1.0f);
    EXPECT_TRUE(value.heicQuality == 44);
    EXPECT_TRUE(value.jpegxlQuality == 33);
    EXPECT_TRUE(value.jpegxlEffort == 9);
    EXPECT_TRUE(value.jpegxlSpeed == 4);
    EXPECT_TRUE(value.rawRot == 3);
}

static void testReloadUpdatesSettingsAndDefaults()
{
    static constexpr const char* kConfigA = R"toml(
[Global]
Solidify = false
ExportAlpha = 2
MaskNames = ["_maskA"]
Console = false
Threads = 8
QueueLimit = 5
Verbosity = 5

[Normalize]
NormalizeMode = 2
NormalsNames = ["normalA"]

[Range]
RangeMode = 3

[Transform]
SwapBasis = 5
SwapInvertMask = 7
GrayscaleMode = 7
GrayscaleWeights = [0.1, 0.2, 0.7]

[Export]
DefaultFormat = 7
FileFormat = 6
DefaultBit = 5
BitDepth = 4

[Encoding]
TiffCompression = "lzw"
TiffZipLevel = 9
OpenEXRCompression = "piz"
OpenEXRZipLevel = 8
OpenEXRDwaLevel = 75
PngStrategy = "rle"
PngLevel = 3
JpegQuality = 77
JpegSubsampling = "4:2:2"
Jpeg2000QStep = 0.5
HeicQuality = 66
JpegXLQuality = 55
JpegXLEffort = 4
JpegXLSpeed = 2

[CameraRaw]
RawRotation = 6
)toml";

    static constexpr const char* kConfigB = R"toml(
[Global]
Solidify = true
ExportAlpha = 1
MaskNames = ["_maskB", "_alphaB"]
Console = true
Threads = 2
QueueLimit = 1
Verbosity = 1

[Normalize]
NormalizeMode = 0
NormalsNames = ["normalB", "worldB"]

[Range]
RangeMode = 1

[Transform]
SwapBasis = 1
SwapInvertMask = 2
GrayscaleMode = 5
GrayscaleWeights = [0.3, 0.4, 0.3]

[Export]
DefaultFormat = 2
FileFormat = -1
DefaultBit = 1
BitDepth = -1

[Encoding]
TiffCompression = "packbits"
TiffZipLevel = 1
OpenEXRCompression = "dwab"
OpenEXRZipLevel = 2
OpenEXRDwaLevel = 12
PngStrategy = "huffman"
PngLevel = 9
JpegQuality = 88
JpegSubsampling = "4:2:0"
Jpeg2000QStep = -1.0
HeicQuality = 44
JpegXLQuality = 33
JpegXLEffort = 9
JpegXLSpeed = 4

[CameraRaw]
RawRotation = 3
)toml";

    const fs::path testDir = fs::temp_directory_path() / "solidify_settings_tests";
    std::error_code ec;
    fs::remove_all(testDir, ec);
    ec.clear();
    fs::create_directories(testDir, ec);
    EXPECT_TRUE(!ec);

    const fs::path configPath = testDir / "sldf_config.toml";
    writeTextFile(configPath, kConfigA);
    EXPECT_TRUE(loadSettingsDefaults(configPath.string()));
    expectConfigA(settings);
    expectConfigA(settingsDefaults);

    settings.isSolidify = true;
    settings.alphaMode  = 0;
    settings.normMode   = 0;
    settings.normNames  = { "runtime-change" };
    resetSettingsToDefaults();
    expectConfigA(settings);

    writeTextFile(configPath, kConfigB);
    EXPECT_TRUE(loadSettingsDefaults(configPath.string()));
    expectConfigB(settings);
    expectConfigB(settingsDefaults);

    settings.jpegQuality = 11;
    EXPECT_TRUE(!loadSettingsDefaults((testDir / "missing.toml").string()));
    EXPECT_TRUE(settings.jpegQuality == 11);
    expectConfigB(settingsDefaults);

    ec.clear();
    fs::remove_all(testDir, ec);
}

}  // namespace

int main()
{
    testReloadUpdatesSettingsAndDefaults();

    if (g_failures != 0) {
        std::cerr << g_failures << " test expectation(s) failed.\n";
        return 1;
    }

    std::cout << "Solidify settings tests passed.\n";
    return 0;
}
