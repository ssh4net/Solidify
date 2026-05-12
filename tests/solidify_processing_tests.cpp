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

#include "processing.h"
#include "settings.h"

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imageio.h>

#include <cstdint>
#include <filesystem>
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

static bool writeRgbaPng(const fs::path& path)
{
    constexpr int width = 8;
    constexpr int height = 8;
    OIIO::ImageSpec spec(width, height, 4, OIIO::TypeDesc::UINT8);
    spec.channelnames[3] = "A";
    spec.alpha_channel   = 3;

    OIIO::ImageBuf image(spec);
    uint8_t* pixels = static_cast<uint8_t*>(image.localpixels());
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool hole = x >= 3 && x <= 4 && y >= 3 && y <= 4;
            const size_t base = (static_cast<size_t>(y) * width + static_cast<size_t>(x)) * 4u;
            const uint8_t alpha = hole ? 0u : 255u;
            pixels[base + 0]    = hole ? 0u : 90u;
            pixels[base + 1]    = hole ? 0u : 120u;
            pixels[base + 2]    = hole ? 0u : 160u;
            pixels[base + 3]    = alpha;
        }
    }

    return image.write(path.string());
}

static void configureProcessing(bool useAlpha)
{
    settings.reSettings();
    settingsDefaults          = settings;
    settings.isSolidify       = true;
    settings.useAlpha         = useAlpha;
    settings.alphaMode        = 0;
    settings.normMode         = 0;
    settings.repairMode       = 0;
    settings.rangeMode        = 0;
    settings.swapBasis        = 0;
    settings.swapInvertMask   = 0;
    settings.grayscaleMode    = 0;
    settings.fileFormat       = 2;
    settings.bitDepth         = -1;
    settings.numThreads       = 1;
    settings.queueLimit       = 1;
    settings.pngStrategy      = PngCompression_Default;
    settings.pngCompressionLevel = 4;
}

static void testUseAlphaProcessesTextureNamedMask()
{
    const fs::path testDir = fs::temp_directory_path() / "solidify_processing_tests";
    std::error_code ec;
    fs::remove_all(testDir, ec);
    ec.clear();
    fs::create_directories(testDir, ec);
    EXPECT_TRUE(!ec);

    const fs::path albedoPath = testDir / "cloth_albedo.png";
    const fs::path maskPath   = testDir / "cloth_mask.png";
    EXPECT_TRUE(writeRgbaPng(albedoPath));
    EXPECT_TRUE(writeRgbaPng(maskPath));

    configureProcessing(true);
    EXPECT_TRUE(doProcessing({ testDir.string() }, nullptr));
    EXPECT_TRUE(fs::exists(testDir / "cloth_albedo_fill.png"));
    EXPECT_TRUE(fs::exists(testDir / "cloth_mask_fill.png"));

    fs::remove(testDir / "cloth_albedo_fill.png", ec);
    ec.clear();
    fs::remove(testDir / "cloth_mask_fill.png", ec);
    ec.clear();

    configureProcessing(false);
    EXPECT_TRUE(doProcessing({ testDir.string() }, nullptr));
    EXPECT_TRUE(fs::exists(testDir / "cloth_albedo_fill.png"));
    EXPECT_TRUE(!fs::exists(testDir / "cloth_mask_fill.png"));

    fs::remove_all(testDir, ec);
}

}  // namespace

int main()
{
    testUseAlphaProcessesTextureNamedMask();

    if (g_failures != 0) {
        std::cerr << g_failures << " processing test expectation(s) failed.\n";
        return 1;
    }

    std::cout << "Solidify processing tests passed.\n";
    return 0;
}
