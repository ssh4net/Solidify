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

#include "imageops.h"

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>

namespace {

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

static const uint16_t* u16Pixels(const OIIO::ImageBuf& image)
{
    return static_cast<const uint16_t*>(image.localpixels());
}

static const float* f32Pixels(const OIIO::ImageBuf& image)
{
    return static_cast<const float*>(image.localpixels());
}

static void testSwapInvertU16()
{
    OIIO::ImageSpec spec(2, 1, 3, OIIO::TypeDesc::UINT16);
    OIIO::ImageBuf src(spec);
    EXPECT_TRUE(OIIO::ImageBufAlgo::zero(src));

    uint16_t* pixels = static_cast<uint16_t*>(src.localpixels());
    pixels[0] = 10;
    pixels[1] = 20;
    pixels[2] = 30;
    pixels[3] = 1000;
    pixels[4] = 2000;
    pixels[5] = 3000;

    OIIO::ImageBuf dst;
    EXPECT_TRUE(applyChannelSwapInvert(dst, src, 3, 2, false, 1));
    const uint16_t* out = u16Pixels(dst);

    EXPECT_TRUE(out[0] == 20);
    EXPECT_TRUE(out[1] == static_cast<uint16_t>(std::numeric_limits<uint16_t>::max() - 30));
    EXPECT_TRUE(out[2] == 10);
    EXPECT_TRUE(out[3] == 2000);
    EXPECT_TRUE(out[4] == static_cast<uint16_t>(std::numeric_limits<uint16_t>::max() - 3000));
    EXPECT_TRUE(out[5] == 1000);
}

static void testSwapInvertSignedFloat()
{
    OIIO::ImageSpec spec(1, 1, 3, OIIO::TypeDesc::FLOAT);
    OIIO::ImageBuf src(spec);
    EXPECT_TRUE(OIIO::ImageBufAlgo::zero(src));
    float* pixels = static_cast<float*>(src.localpixels());
    pixels[0] = -0.25f;
    pixels[1] = 0.5f;
    pixels[2] = 0.75f;

    OIIO::ImageBuf dst;
    EXPECT_TRUE(applyChannelSwapInvert(dst, src, 0, 5, true, 1));
    const float* out = f32Pixels(dst);
    EXPECT_NEAR_VALUE(out[0], 0.25f, 0.00001f, "signed inverted X");
    EXPECT_NEAR_VALUE(out[1], 0.5f, 0.00001f, "signed unchanged Y");
    EXPECT_NEAR_VALUE(out[2], -0.75f, 0.00001f, "signed inverted Z");
}

static uint16_t luminosityU16(uint16_t r, uint16_t g, uint16_t b)
{
    const uint64_t sum = static_cast<uint64_t>(r) * 13933u + static_cast<uint64_t>(g) * 46871u
                       + static_cast<uint64_t>(b) * 4732u + 32768u;
    const uint64_t value = sum >> 16;
    return static_cast<uint16_t>(std::min<uint64_t>(value, std::numeric_limits<uint16_t>::max()));
}

static void testGrayscaleU16()
{
    OIIO::ImageSpec spec(2, 1, 4, OIIO::TypeDesc::UINT16);
    OIIO::ImageBuf src(spec);
    EXPECT_TRUE(OIIO::ImageBufAlgo::zero(src));
    src.specmod().alpha_channel = 3;

    uint16_t* pixels = static_cast<uint16_t*>(src.localpixels());
    pixels[0] = 10000;
    pixels[1] = 20000;
    pixels[2] = 30000;
    pixels[3] = 40000;
    pixels[4] = 65535;
    pixels[5] = 65535;
    pixels[6] = 65535;
    pixels[7] = 12345;

    const float weights[3] = { 10.0f, 10.0f, 10.0f };

    OIIO::ImageBuf luma;
    EXPECT_TRUE(applyGrayscale(luma, src, 5, weights, true, 1));
    EXPECT_TRUE(luma.nchannels() == 2);
    const uint16_t* lumaPixels = u16Pixels(luma);
    EXPECT_TRUE(lumaPixels[0] == luminosityU16(10000, 20000, 30000));
    EXPECT_TRUE(lumaPixels[1] == 40000);

    OIIO::ImageBuf alpha;
    EXPECT_TRUE(applyGrayscale(alpha, src, 4, weights, true, 1));
    EXPECT_TRUE(alpha.nchannels() == 1);
    const uint16_t* alphaPixels = u16Pixels(alpha);
    EXPECT_TRUE(alphaPixels[0] == 40000);

    OIIO::ImageBuf arbitrary;
    EXPECT_TRUE(applyGrayscale(arbitrary, src, 7, weights, false, 1));
    EXPECT_TRUE(arbitrary.nchannels() == 1);
    const uint16_t* arbitraryPixels = u16Pixels(arbitrary);
    EXPECT_TRUE(arbitraryPixels[1] == std::numeric_limits<uint16_t>::max());
}

}  // namespace

int main()
{
    testSwapInvertU16();
    testSwapInvertSignedFloat();
    testGrayscaleU16();

    if (g_failures != 0) {
        std::cerr << g_failures << " test expectation(s) failed.\n";
        return 1;
    }

    std::cout << "Solidify image tests passed.\n";
    return 0;
}
