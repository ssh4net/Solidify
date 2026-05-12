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
#include "imageio.h"
#include "settings.h"

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imageio.h>

#include <cmath>
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

static void expectNear(float actual, float expected, float eps, const char* label, const char* file, int line)
{
    if (std::fabs(actual - expected) > eps) {
        std::cerr << file << ':' << line << ": " << label << " expected " << expected << ", got " << actual << '\n';
        ++g_failures;
    }
}

#define EXPECT_NEAR_VALUE(actual, expected, eps, label) expectNear((actual), (expected), (eps), (label), __FILE__, __LINE__)

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

static bool writeFloatMask(const fs::path& path, float alpha)
{
    OIIO::ImageSpec spec(1, 1, 1, OIIO::TypeDesc::FLOAT);
    spec.channelnames[0] = "A";
    spec.alpha_channel   = 0;

    OIIO::ImageBuf image(spec);
    float* pixels = static_cast<float*>(image.localpixels());
    pixels[0]     = alpha;
    return image.write(path.string());
}

static bool writeFloatRgba(const fs::path& path, float r, float g, float b, float a)
{
    OIIO::ImageSpec spec(1, 1, 4, OIIO::TypeDesc::FLOAT);
    spec.channelnames[3] = "A";
    spec.alpha_channel   = 3;

    OIIO::ImageBuf image(spec);
    float* pixels = static_cast<float*>(image.localpixels());
    pixels[0]     = r;
    pixels[1]     = g;
    pixels[2]     = b;
    pixels[3]     = a;
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

static void testMaskGammaUsesImageAppConventionAndPreservesOriginal()
{
    const fs::path testDir = fs::temp_directory_path() / "solidify_processing_gamma_tests";
    std::error_code ec;
    fs::remove_all(testDir, ec);
    ec.clear();
    fs::create_directories(testDir, ec);
    EXPECT_TRUE(!ec);

    const fs::path maskPath = testDir / "soft_mask.exr";
    EXPECT_TRUE(writeFloatMask(maskPath, 0.25f));

    settings.reSettings();
    settings.alphaMode  = 1;
    settings.alphaGamma = 2.0f;

    MaskBuffers mask = mask_load(maskPath.string(), nullptr);
    EXPECT_TRUE(mask.alpha.initialized());
    EXPECT_TRUE(mask.rgbAlpha.initialized());
    EXPECT_TRUE(mask.originalAlpha.initialized());

    float gammaAlpha[1] = {};
    float originalAlpha[1] = {};
    float rgbAlpha[3] = {};
    mask.alpha.getpixel(0, 0, gammaAlpha, 1);
    mask.originalAlpha.getpixel(0, 0, originalAlpha, 1);
    mask.rgbAlpha.getpixel(0, 0, rgbAlpha, 3);

    EXPECT_NEAR_VALUE(gammaAlpha[0], 0.5f, 0.001f, "gamma alpha");
    EXPECT_NEAR_VALUE(originalAlpha[0], 0.25f, 0.001f, "original alpha");
    EXPECT_NEAR_VALUE(rgbAlpha[0], 0.5f, 0.001f, "rgb alpha R");
    EXPECT_NEAR_VALUE(rgbAlpha[1], 0.5f, 0.001f, "rgb alpha G");
    EXPECT_NEAR_VALUE(rgbAlpha[2], 0.5f, 0.001f, "rgb alpha B");

    fs::remove_all(testDir, ec);
}

static void testEmbeddedAlphaGammaAndPremultiplySwitch()
{
    const fs::path testDir = fs::temp_directory_path() / "solidify_processing_premultiply_tests";
    std::error_code ec;
    fs::remove_all(testDir, ec);
    ec.clear();
    fs::create_directories(testDir, ec);
    EXPECT_TRUE(!ec);

    const fs::path imagePath = testDir / "embedded_alpha.exr";
    EXPECT_TRUE(writeFloatRgba(imagePath, 0.8f, 0.4f, 0.2f, 0.25f));

    settings.reSettings();
    settings.isSolidify       = true;
    settings.alphaGamma       = 2.0f;
    settings.premultiplyAlpha = false;

    OIIO::ImageBuf unpremultiplied(imagePath.string());
    EXPECT_TRUE(img_load(unpremultiplied, imagePath.string(), false, nullptr, nullptr));
    float unpremultipliedPixel[4] = {};
    unpremultiplied.getpixel(0, 0, unpremultipliedPixel, 4);
    EXPECT_NEAR_VALUE(unpremultipliedPixel[0], 0.8f, 0.001f, "unpremultiplied R");
    EXPECT_NEAR_VALUE(unpremultipliedPixel[1], 0.4f, 0.001f, "unpremultiplied G");
    EXPECT_NEAR_VALUE(unpremultipliedPixel[2], 0.2f, 0.001f, "unpremultiplied B");
    EXPECT_NEAR_VALUE(unpremultipliedPixel[3], 0.5f, 0.001f, "unpremultiplied A");

    settings.premultiplyAlpha = true;

    OIIO::ImageBuf premultiplied(imagePath.string());
    EXPECT_TRUE(img_load(premultiplied, imagePath.string(), false, nullptr, nullptr));
    float premultipliedPixel[4] = {};
    premultiplied.getpixel(0, 0, premultipliedPixel, 4);
    EXPECT_NEAR_VALUE(premultipliedPixel[0], 0.4f, 0.001f, "premultiplied R");
    EXPECT_NEAR_VALUE(premultipliedPixel[1], 0.2f, 0.001f, "premultiplied G");
    EXPECT_NEAR_VALUE(premultipliedPixel[2], 0.1f, 0.001f, "premultiplied B");
    EXPECT_NEAR_VALUE(premultipliedPixel[3], 0.5f, 0.001f, "premultiplied A");

    fs::remove_all(testDir, ec);
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
    testMaskGammaUsesImageAppConventionAndPreservesOriginal();
    testEmbeddedAlphaGammaAndPremultiplySwitch();
    testUseAlphaProcessesTextureNamedMask();

    if (g_failures != 0) {
        std::cerr << g_failures << " processing test expectation(s) failed.\n";
        return 1;
    }

    std::cout << "Solidify processing tests passed.\n";
    return 0;
}
