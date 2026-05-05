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

#include "pushpull.h"

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/half.h>
#include <OpenImageIO/imageio.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>

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

static bool readSpec(const fs::path& path, OIIO::ImageSpec* spec)
{
    std::unique_ptr<OIIO::ImageInput> input = OIIO::ImageInput::open(path.string());
    if (!input) {
        std::cerr << "Could not open " << path << '\n';
        return false;
    }
    *spec = input->spec();
    input->close();
    return true;
}

static void testSampleSpecs()
{
    const fs::path dataDir = fs::path(SOLIDIFY_TEST_DATA_DIR);
    const fs::path rgbPath = dataDir / "001_00_D_3282930003039904.TIF";
    const fs::path maskPath = dataDir / "001_00_D_3282930003039904_mask.png";
    const fs::path grayPath = dataDir / "001_00_D_3282930003039904_gray.tif";

    if (!fs::exists(rgbPath) || !fs::exists(maskPath) || !fs::exists(grayPath)) {
        std::cout << "Push-pull sample image triad not present, skipping sample spec checks.\n";
        return;
    }

    OIIO::ImageSpec rgbSpec;
    OIIO::ImageSpec maskSpec;
    OIIO::ImageSpec graySpec;
    EXPECT_TRUE(readSpec(rgbPath, &rgbSpec));
    EXPECT_TRUE(readSpec(maskPath, &maskSpec));
    EXPECT_TRUE(readSpec(grayPath, &graySpec));

    EXPECT_TRUE(rgbSpec.width == maskSpec.width && rgbSpec.height == maskSpec.height);
    EXPECT_TRUE(rgbSpec.width == graySpec.width && rgbSpec.height == graySpec.height);
    EXPECT_TRUE(rgbSpec.nchannels == 3);
    EXPECT_TRUE(maskSpec.nchannels == 1);
    EXPECT_TRUE(graySpec.nchannels == 1);
    EXPECT_TRUE(rgbSpec.format == OIIO::TypeDesc::UINT16);
    EXPECT_TRUE(maskSpec.format == OIIO::TypeDesc::UINT8);
    EXPECT_TRUE(graySpec.format == OIIO::TypeDesc::UINT16);

    const fs::path rgbHalfPath = dataDir / "001_00_D_3282930003039904_half.exr";
    const fs::path rgbFloatPath = dataDir / "001_00_D_3282930003039904_float.exr";
    const fs::path grayHalfPath = dataDir / "001_00_D_3282930003039904_gray_half.exr";
    const fs::path grayFloatPath = dataDir / "001_00_D_3282930003039904_gray_float.exr";
    if (fs::exists(rgbHalfPath) && fs::exists(rgbFloatPath) && fs::exists(grayHalfPath) && fs::exists(grayFloatPath)) {
        OIIO::ImageSpec rgbHalfSpec;
        OIIO::ImageSpec rgbFloatSpec;
        OIIO::ImageSpec grayHalfSpec;
        OIIO::ImageSpec grayFloatSpec;
        EXPECT_TRUE(readSpec(rgbHalfPath, &rgbHalfSpec));
        EXPECT_TRUE(readSpec(rgbFloatPath, &rgbFloatSpec));
        EXPECT_TRUE(readSpec(grayHalfPath, &grayHalfSpec));
        EXPECT_TRUE(readSpec(grayFloatPath, &grayFloatSpec));
        EXPECT_TRUE(rgbHalfSpec.nchannels == 3 && rgbHalfSpec.format == OIIO::TypeDesc::HALF);
        EXPECT_TRUE(rgbFloatSpec.nchannels == 3 && rgbFloatSpec.format == OIIO::TypeDesc::FLOAT);
        EXPECT_TRUE(grayHalfSpec.nchannels == 1 && grayHalfSpec.format == OIIO::TypeDesc::HALF);
        EXPECT_TRUE(grayFloatSpec.nchannels == 1 && grayFloatSpec.format == OIIO::TypeDesc::FLOAT);
    }
}

static OIIO::ImageBuf makeRgbaFloatHole()
{
    constexpr int width = 16;
    constexpr int height = 16;
    OIIO::ImageSpec spec(width, height, 4, OIIO::TypeDesc::FLOAT);
    spec.alpha_channel = 3;
    spec.channelnames[3] = "A";
    OIIO::ImageBuf image(spec);
    float* pixels = static_cast<float*>(image.localpixels());
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool hole = x >= 6 && x <= 9 && y >= 6 && y <= 9;
            const size_t base = (static_cast<size_t>(y) * width + static_cast<size_t>(x)) * 4u;
            const float alpha = hole ? 0.0f : 1.0f;
            pixels[base + 0] = 0.25f * alpha;
            pixels[base + 1] = 0.50f * alpha;
            pixels[base + 2] = 0.75f * alpha;
            pixels[base + 3] = alpha;
        }
    }
    return image;
}

static OIIO::ImageBuf makeCanonicalRgbaFloatHole()
{
    constexpr int width = 65;
    constexpr int height = 97;
    OIIO::ImageSpec spec(width, height, 4, OIIO::TypeDesc::FLOAT);
    spec.alpha_channel = 3;
    spec.channelnames[3] = "A";
    OIIO::ImageBuf image(spec);
    float* pixels = static_cast<float*>(image.localpixels());
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool hole = x >= 16 && x <= 50 && y >= 22 && y <= 74;
            const float alpha = hole ? 0.0f : 1.0f;
            const float r = 0.15f + 0.70f * static_cast<float>(x) / static_cast<float>(width - 1);
            const float g = 0.20f + 0.55f * static_cast<float>(y) / static_cast<float>(height - 1);
            const float b = 0.10f + 0.45f * static_cast<float>((x * 3 + y * 5) % 31) / 30.0f;
            const size_t base = (static_cast<size_t>(y) * width + static_cast<size_t>(x)) * 4u;
            pixels[base + 0] = r * alpha;
            pixels[base + 1] = g * alpha;
            pixels[base + 2] = b * alpha;
            pixels[base + 3] = alpha;
        }
    }
    return image;
}

static OIIO::ImageBuf makeGrayFloatHole()
{
    constexpr int width = 16;
    constexpr int height = 16;
    OIIO::ImageSpec spec(width, height, 2, OIIO::TypeDesc::FLOAT);
    spec.channelnames[0] = "Y";
    spec.channelnames[1] = "A";
    spec.alpha_channel = 1;
    OIIO::ImageBuf image(spec);
    float* pixels = static_cast<float*>(image.localpixels());
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool hole = x >= 6 && x <= 9 && y >= 6 && y <= 9;
            const size_t base = (static_cast<size_t>(y) * width + static_cast<size_t>(x)) * 2u;
            const float alpha = hole ? 0.0f : 1.0f;
            pixels[base + 0] = 0.60f * alpha;
            pixels[base + 1] = alpha;
        }
    }
    return image;
}

static bool readFloatPixels(const OIIO::ImageBuf& image, std::vector<float>* pixels)
{
    const OIIO::ImageSpec& spec = image.spec();
    pixels->assign(static_cast<size_t>(spec.width) * static_cast<size_t>(spec.height)
                       * static_cast<size_t>(spec.nchannels),
                   0.0f);
    const OIIO::ROI roi(image.xbegin(), image.xend(), image.ybegin(), image.yend(), image.zbegin(), image.zend(), 0,
                        spec.nchannels);
    return image.get_pixels(roi, OIIO::TypeDesc::FLOAT, pixels->data());
}

static void expectImageClose(const OIIO::ImageBuf& actual, const OIIO::ImageBuf& expected, const float eps,
                             const char* label)
{
    EXPECT_TRUE(actual.spec().width == expected.spec().width);
    EXPECT_TRUE(actual.spec().height == expected.spec().height);
    EXPECT_TRUE(actual.nchannels() == expected.nchannels());
    std::vector<float> actualPixels;
    std::vector<float> expectedPixels;
    EXPECT_TRUE(readFloatPixels(actual, &actualPixels));
    EXPECT_TRUE(readFloatPixels(expected, &expectedPixels));
    if (actualPixels.size() != expectedPixels.size()) {
        ++g_failures;
        std::cerr << label << " pixel buffers have different sizes\n";
        return;
    }
    float maxError = 0.0f;
    for (size_t i = 0; i < actualPixels.size(); ++i) {
        maxError = std::max(maxError, std::fabs(actualPixels[i] - expectedPixels[i]));
    }
    if (maxError > eps) {
        ++g_failures;
        std::cerr << label << " max error expected <= " << eps << ", got " << maxError << '\n';
    }
}

static OIIO::ImageBuf makeRgbaHalfHole()
{
    constexpr int width = 16;
    constexpr int height = 16;
    OIIO::ImageSpec spec(width, height, 4, OIIO::TypeDesc::HALF);
    spec.alpha_channel = 3;
    spec.channelnames[3] = "A";
    OIIO::ImageBuf image(spec);
    half* pixels = static_cast<half*>(image.localpixels());
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool hole = x >= 6 && x <= 9 && y >= 6 && y <= 9;
            const size_t base = (static_cast<size_t>(y) * width + static_cast<size_t>(x)) * 4u;
            const float alpha = hole ? 0.0f : 1.0f;
            pixels[base + 0] = half(0.25f * alpha);
            pixels[base + 1] = half(0.50f * alpha);
            pixels[base + 2] = half(0.75f * alpha);
            pixels[base + 3] = half(alpha);
        }
    }
    return image;
}

static OIIO::ImageBuf makeGrayHalfHole()
{
    constexpr int width = 16;
    constexpr int height = 16;
    OIIO::ImageSpec spec(width, height, 2, OIIO::TypeDesc::HALF);
    spec.channelnames[0] = "Y";
    spec.channelnames[1] = "A";
    spec.alpha_channel = 1;
    OIIO::ImageBuf image(spec);
    half* pixels = static_cast<half*>(image.localpixels());
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool hole = x >= 6 && x <= 9 && y >= 6 && y <= 9;
            const size_t base = (static_cast<size_t>(y) * width + static_cast<size_t>(x)) * 2u;
            const float alpha = hole ? 0.0f : 1.0f;
            pixels[base + 0] = half(0.60f * alpha);
            pixels[base + 1] = half(alpha);
        }
    }
    return image;
}

static void testRgbaFloatPushPull()
{
    OIIO::ImageBuf src = makeRgbaFloatHole();
    OIIO::ImageBuf filled;
    EXPECT_TRUE(applyPushPullFill(filled, src, 1));
    EXPECT_TRUE(filled.nchannels() == 4);
    EXPECT_TRUE(filled.spec().format == OIIO::TypeDesc::FLOAT);

    float center[4] = {};
    filled.getpixel(8, 8, center, 4);
    EXPECT_NEAR_VALUE(center[0], 0.25f, 0.01f, "filled R");
    EXPECT_NEAR_VALUE(center[1], 0.50f, 0.01f, "filled G");
    EXPECT_NEAR_VALUE(center[2], 0.75f, 0.01f, "filled B");
    EXPECT_TRUE(center[3] > 0.99f);
}

static void testGrayFloatPushPull()
{
    OIIO::ImageBuf src = makeGrayFloatHole();
    OIIO::ImageBuf filled;
    EXPECT_TRUE(applyPushPullFill(filled, src, 1));
    EXPECT_TRUE(filled.nchannels() == 2);
    EXPECT_TRUE(filled.spec().format == OIIO::TypeDesc::FLOAT);

    float center[2] = {};
    filled.getpixel(8, 8, center, 2);
    EXPECT_NEAR_VALUE(center[0], 0.60f, 0.01f, "filled Y");
    EXPECT_TRUE(center[1] > 0.99f);
}

static void testCanonicalRgbaMatchesOiio()
{
    OIIO::ImageBuf src = makeCanonicalRgbaFloatHole();
    OIIO::ImageBuf native;
    OIIO::ImageBuf oiio;
    EXPECT_TRUE(applyPushPullFill(native, src, 1));
    EXPECT_TRUE(OIIO::ImageBufAlgo::fillholes_pushpull(oiio, src, {}, 1));
    expectImageClose(native, oiio, 0.002f, "canonical RGBA push-pull");
}

static void testRgbaHalfPushPull()
{
    OIIO::ImageBuf src = makeRgbaHalfHole();
    OIIO::ImageBuf filled;
    EXPECT_TRUE(applyPushPullFill(filled, src, 1));
    EXPECT_TRUE(filled.nchannels() == 4);
    EXPECT_TRUE(filled.spec().format == OIIO::TypeDesc::HALF);

    float center[4] = {};
    filled.getpixel(8, 8, center, 4);
    EXPECT_NEAR_VALUE(center[0], 0.25f, 0.01f, "filled half R");
    EXPECT_NEAR_VALUE(center[1], 0.50f, 0.01f, "filled half G");
    EXPECT_NEAR_VALUE(center[2], 0.75f, 0.01f, "filled half B");
    EXPECT_TRUE(center[3] > 0.99f);
}

static void testGrayHalfPushPull()
{
    OIIO::ImageBuf src = makeGrayHalfHole();
    OIIO::ImageBuf filled;
    EXPECT_TRUE(applyPushPullFill(filled, src, 1));
    EXPECT_TRUE(filled.nchannels() == 2);
    EXPECT_TRUE(filled.spec().format == OIIO::TypeDesc::HALF);

    float center[2] = {};
    filled.getpixel(8, 8, center, 2);
    EXPECT_NEAR_VALUE(center[0], 0.60f, 0.01f, "filled half Y");
    EXPECT_TRUE(center[1] > 0.99f);
}

static void testUint16FormatPreserved()
{
    constexpr int width = 8;
    constexpr int height = 8;
    OIIO::ImageSpec spec(width, height, 4, OIIO::TypeDesc::UINT16);
    spec.alpha_channel = 3;
    spec.channelnames[3] = "A";
    OIIO::ImageBuf src(spec);
    uint16_t* pixels = static_cast<uint16_t*>(src.localpixels());
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool hole = x >= 3 && x <= 4 && y >= 3 && y <= 4;
            const size_t base = (static_cast<size_t>(y) * width + static_cast<size_t>(x)) * 4u;
            const uint16_t alpha = hole ? 0u : std::numeric_limits<uint16_t>::max();
            pixels[base + 0] = hole ? 0u : 10000u;
            pixels[base + 1] = hole ? 0u : 20000u;
            pixels[base + 2] = hole ? 0u : 30000u;
            pixels[base + 3] = alpha;
        }
    }

    OIIO::ImageBuf filled;
    EXPECT_TRUE(applyPushPullFill(filled, src, 1));
    EXPECT_TRUE(filled.spec().format == OIIO::TypeDesc::UINT16);
    const uint16_t* out = static_cast<const uint16_t*>(filled.localpixels());
    const size_t center = (static_cast<size_t>(4) * width + 4u) * 4u;
    EXPECT_TRUE(out[center + 0] > 9000u && out[center + 0] < 11000u);
    EXPECT_TRUE(out[center + 1] > 19000u && out[center + 1] < 21000u);
    EXPECT_TRUE(out[center + 2] > 29000u && out[center + 2] < 31000u);
}

}  // namespace

int main()
{
    testSampleSpecs();
    testRgbaFloatPushPull();
    testGrayFloatPushPull();
    testCanonicalRgbaMatchesOiio();
    testRgbaHalfPushPull();
    testGrayHalfPushPull();
    testUint16FormatPreserved();

    if (g_failures != 0) {
        std::cerr << g_failures << " push-pull test expectation(s) failed.\n";
        return 1;
    }

    std::cout << "Solidify push-pull tests passed.\n";
    return 0;
}
