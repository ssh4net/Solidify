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

#include "pch.h"

#include "pushpull.h"

#include <cmath>
#include <cstdint>

namespace solidify_pushpull_hwy {

struct PushPullTriangleWeights {
    int indices[8]   = {};
    float weights[8] = {};
    int taps         = 0;
};

struct PushPullBilinearWeights {
    int index0 = 0;
    int index1 = 0;
    float t    = 0.0f;
};

struct PushPullPullView {
    const float* src                        = nullptr;
    float* dst                              = nullptr;
    const PushPullTriangleWeights* xWeights = nullptr;
    const PushPullTriangleWeights* yWeights = nullptr;
    int srcWidth                            = 0;
    int srcHeight                           = 0;
    int dstWidth                            = 0;
    int dstHeight                           = 0;
    int channels                            = 0;
    int yBegin                              = 0;
    int yEnd                                = 0;
};

struct PushPullPullTiledView {
    const float* base = nullptr;
    float* level1     = nullptr;
    float* level2     = nullptr;
    float* level3     = nullptr;
    float* level4     = nullptr;
    int baseWidth     = 0;
    int baseHeight    = 0;
    int level1Width   = 0;
    int level2Width   = 0;
    int level3Width   = 0;
    int level4Width   = 0;
    int channels      = 0;
    int levelCount    = 0;
    int tileXBegin    = 0;
    int tileXEnd      = 0;
    int tileYBegin    = 0;
    int tileYEnd      = 0;
};

struct PushPullPullU16TiledView {
    const uint16_t* base = nullptr;
    float* level1        = nullptr;
    float* level2        = nullptr;
    float* level3        = nullptr;
    float* level4        = nullptr;
    int baseWidth        = 0;
    int baseHeight       = 0;
    int level1Width      = 0;
    int level2Width      = 0;
    int level3Width      = 0;
    int level4Width      = 0;
    int channels         = 0;
    int levelCount       = 0;
    int tileXBegin       = 0;
    int tileXEnd         = 0;
    int tileYBegin       = 0;
    int tileYEnd         = 0;
};

struct PushPullPullHalfTiledView {
    const half* base = nullptr;
    float* level1    = nullptr;
    float* level2    = nullptr;
    float* level3    = nullptr;
    float* level4    = nullptr;
    int baseWidth    = 0;
    int baseHeight   = 0;
    int level1Width  = 0;
    int level2Width  = 0;
    int level3Width  = 0;
    int level4Width  = 0;
    int channels     = 0;
    int levelCount   = 0;
    int tileXBegin   = 0;
    int tileXEnd     = 0;
    int tileYBegin   = 0;
    int tileYEnd     = 0;
};

struct PushPullPushView {
    const float* fine                       = nullptr;
    const float* coarse                     = nullptr;
    float* dst                              = nullptr;
    const PushPullBilinearWeights* xWeights = nullptr;
    const PushPullBilinearWeights* yWeights = nullptr;
    int fineWidth                           = 0;
    int fineHeight                          = 0;
    int coarseWidth                         = 0;
    int coarseHeight                        = 0;
    int channels                            = 0;
    int xBegin                              = 0;
    int xEnd                                = 0;
    int yBegin                              = 0;
    int yEnd                                = 0;
};

struct PushPullNormalizeView {
    const float* src = nullptr;
    float* dst       = nullptr;
    int width        = 0;
    int channels     = 0;
    int yBegin       = 0;
    int yEnd         = 0;
};

struct PushPullFinalView {
    const float* fine                       = nullptr;
    const float* coarse                     = nullptr;
    float* dst                              = nullptr;
    const PushPullBilinearWeights* xWeights = nullptr;
    const PushPullBilinearWeights* yWeights = nullptr;
    int fineWidth                           = 0;
    int fineHeight                          = 0;
    int coarseWidth                         = 0;
    int coarseHeight                        = 0;
    int channels                            = 0;
    int xBegin                              = 0;
    int xEnd                                = 0;
    int yBegin                              = 0;
    int yEnd                                = 0;
};

struct PushPullNormalizeU16View {
    const float* src = nullptr;
    uint16_t* dst    = nullptr;
    int width        = 0;
    int channels     = 0;
    int xBegin       = 0;
    int xEnd         = 0;
    int yBegin       = 0;
    int yEnd         = 0;
};

struct PushPullFinalU16View {
    const float* fine                       = nullptr;
    const float* coarse                     = nullptr;
    uint16_t* dst                           = nullptr;
    const PushPullBilinearWeights* xWeights = nullptr;
    const PushPullBilinearWeights* yWeights = nullptr;
    int fineWidth                           = 0;
    int fineHeight                          = 0;
    int coarseWidth                         = 0;
    int coarseHeight                        = 0;
    int channels                            = 0;
    int xBegin                              = 0;
    int xEnd                                = 0;
    int yBegin                              = 0;
    int yEnd                                = 0;
};

struct PushPullNormalizeHalfView {
    const float* src = nullptr;
    half* dst        = nullptr;
    int width        = 0;
    int channels     = 0;
    int xBegin       = 0;
    int xEnd         = 0;
    int yBegin       = 0;
    int yEnd         = 0;
};

struct PushPullFinalHalfView {
    const float* fine                       = nullptr;
    const float* coarse                     = nullptr;
    half* dst                               = nullptr;
    const PushPullBilinearWeights* xWeights = nullptr;
    const PushPullBilinearWeights* yWeights = nullptr;
    int fineWidth                           = 0;
    int fineHeight                          = 0;
    int coarseWidth                         = 0;
    int coarseHeight                        = 0;
    int channels                            = 0;
    int xBegin                              = 0;
    int xEnd                                = 0;
    int yBegin                              = 0;
    int yEnd                                = 0;
};

struct PushPullNormalizeFromU16View {
    const uint16_t* src = nullptr;
    uint16_t* dst       = nullptr;
    int width           = 0;
    int channels        = 0;
    int xBegin          = 0;
    int xEnd            = 0;
    int yBegin          = 0;
    int yEnd            = 0;
};

struct PushPullNormalizeFromHalfView {
    const half* src = nullptr;
    half* dst       = nullptr;
    int width       = 0;
    int channels    = 0;
    int xBegin      = 0;
    int xEnd        = 0;
    int yBegin      = 0;
    int yEnd        = 0;
};

struct PushPullFinalFromU16View {
    const uint16_t* fine                    = nullptr;
    const float* coarse                     = nullptr;
    uint16_t* dst                           = nullptr;
    const PushPullBilinearWeights* xWeights = nullptr;
    const PushPullBilinearWeights* yWeights = nullptr;
    int fineWidth                           = 0;
    int fineHeight                          = 0;
    int coarseWidth                         = 0;
    int coarseHeight                        = 0;
    int channels                            = 0;
    int xBegin                              = 0;
    int xEnd                                = 0;
    int yBegin                              = 0;
    int yEnd                                = 0;
};

struct PushPullFinalFromHalfView {
    const half* fine                        = nullptr;
    const float* coarse                     = nullptr;
    half* dst                               = nullptr;
    const PushPullBilinearWeights* xWeights = nullptr;
    const PushPullBilinearWeights* yWeights = nullptr;
    int fineWidth                           = 0;
    int fineHeight                          = 0;
    int coarseWidth                         = 0;
    int coarseHeight                        = 0;
    int channels                            = 0;
    int xBegin                              = 0;
    int xEnd                                = 0;
    int yBegin                              = 0;
    int yEnd                                = 0;
};

}  // namespace solidify_pushpull_hwy

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "pushpull_hwy.inl"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>
#include "pushpull_hwy.inl"

#if HWY_ONCE
namespace solidify_pushpull_hwy {

HWY_EXPORT(PushPullPullKernel);
HWY_EXPORT(PushPullPullExact2xKernel);
HWY_EXPORT(PushPullPullTiledKernel);
HWY_EXPORT(PushPullPullU16TiledKernel);
HWY_EXPORT(PushPullPullHalfTiledKernel);
HWY_EXPORT(PushPullPushKernel);
HWY_EXPORT(PushPullNormalizeKernel);
HWY_EXPORT(PushPullFinalKernel);
HWY_EXPORT(PushPullNormalizeU16Kernel);
HWY_EXPORT(PushPullFinalU16Kernel);
HWY_EXPORT(PushPullNormalizeFromU16Kernel);
HWY_EXPORT(PushPullFinalFromU16Kernel);
HWY_EXPORT(PushPullNormalizeHalfKernel);
HWY_EXPORT(PushPullFinalHalfKernel);
HWY_EXPORT(PushPullNormalizeFromHalfKernel);
HWY_EXPORT(PushPullFinalFromHalfKernel);

static bool
runPullHwy(const PushPullPullView* view)
{
    return HWY_DYNAMIC_DISPATCH(PushPullPullKernel)(view);
}

static bool
runPullExact2xHwy(const PushPullPullView* view)
{
    return HWY_DYNAMIC_DISPATCH(PushPullPullExact2xKernel)(view);
}

static bool
runPullTiledHwy(const PushPullPullTiledView* view)
{
    return HWY_DYNAMIC_DISPATCH(PushPullPullTiledKernel)(view);
}

static bool
runPullU16TiledHwy(const PushPullPullU16TiledView* view)
{
    return HWY_DYNAMIC_DISPATCH(PushPullPullU16TiledKernel)(view);
}

static bool
runPullHalfTiledHwy(const PushPullPullHalfTiledView* view)
{
    return HWY_DYNAMIC_DISPATCH(PushPullPullHalfTiledKernel)(view);
}

static bool
runPushHwy(const PushPullPushView* view)
{
    return HWY_DYNAMIC_DISPATCH(PushPullPushKernel)(view);
}

static bool
runNormalizeHwy(const PushPullNormalizeView* view)
{
    return HWY_DYNAMIC_DISPATCH(PushPullNormalizeKernel)(view);
}

static bool
runFinalHwy(const PushPullFinalView* view)
{
    return HWY_DYNAMIC_DISPATCH(PushPullFinalKernel)(view);
}

static bool
runNormalizeU16Hwy(const PushPullNormalizeU16View* view)
{
    return HWY_DYNAMIC_DISPATCH(PushPullNormalizeU16Kernel)(view);
}

static bool
runFinalU16Hwy(const PushPullFinalU16View* view)
{
    return HWY_DYNAMIC_DISPATCH(PushPullFinalU16Kernel)(view);
}

static bool
runNormalizeFromU16Hwy(const PushPullNormalizeFromU16View* view)
{
    return HWY_DYNAMIC_DISPATCH(PushPullNormalizeFromU16Kernel)(view);
}

static bool
runFinalFromU16Hwy(const PushPullFinalFromU16View* view)
{
    return HWY_DYNAMIC_DISPATCH(PushPullFinalFromU16Kernel)(view);
}

static bool
runNormalizeHalfHwy(const PushPullNormalizeHalfView* view)
{
    return HWY_DYNAMIC_DISPATCH(PushPullNormalizeHalfKernel)(view);
}

static bool
runFinalHalfHwy(const PushPullFinalHalfView* view)
{
    return HWY_DYNAMIC_DISPATCH(PushPullFinalHalfKernel)(view);
}

static bool
runNormalizeFromHalfHwy(const PushPullNormalizeFromHalfView* view)
{
    return HWY_DYNAMIC_DISPATCH(PushPullNormalizeFromHalfKernel)(view);
}

static bool
runFinalFromHalfHwy(const PushPullFinalFromHalfView* view)
{
    return HWY_DYNAMIC_DISPATCH(PushPullFinalFromHalfKernel)(view);
}

}  // namespace solidify_pushpull_hwy
#endif

namespace {

struct PushPullLevel {
    int width    = 0;
    int height   = 0;
    int channels = 0;
    std::vector<float> pixels;
};

static float
triangleFilter(const float x)
{
    const float ax = std::fabs(x);
    return ax < 1.0f ? 1.0f - ax : 0.0f;
}

static void
computeTriangleResizeWeights(solidify_pushpull_hwy::PushPullTriangleWeights* dst, const int dstCoord, const int srcSize,
                             const int dstSize)
{
    const float ratio    = static_cast<float>(dstSize) / static_cast<float>(srcSize);
    const int radius     = static_cast<int>(std::ceil(1.0f / ratio));
    const int taps       = radius * 2 + 1;
    const float srcCoord = (static_cast<float>(dstCoord) + 0.5f) * static_cast<float>(srcSize)
                           / static_cast<float>(dstSize);
    const int srcBase = static_cast<int>(std::floor(srcCoord));
    const float frac  = srcCoord - static_cast<float>(srcBase);
    float total       = 0.0f;

    dst->taps = 0;
    for (int i = 0; i < taps; ++i) {
        const float w = triangleFilter(ratio * (static_cast<float>(i - radius) - (frac - 0.5f)));
        if (w == 0.0f) {
            continue;
        }
        const int out     = dst->taps++;
        dst->weights[out] = w;
        total += w;
        dst->indices[out] = std::clamp(srcBase - radius + i, 0, srcSize - 1);
    }
    if (total != 0.0f) {
        const float invTotal = 1.0f / total;
        for (int i = 0; i < dst->taps; ++i) {
            dst->weights[i] *= invTotal;
        }
    }
}

static void
computeBilinearResizeWeight(solidify_pushpull_hwy::PushPullBilinearWeights* dst, const int fineCoord,
                            const int fineSize, const int coarseSize)
{
    const float scale = static_cast<float>(coarseSize) / static_cast<float>(fineSize);
    const float coord = (static_cast<float>(fineCoord) + 0.5f) * scale - 0.5f;
    const int raw     = static_cast<int>(std::floor(coord));
    dst->index0       = std::clamp(raw, 0, coarseSize - 1);
    dst->index1       = std::clamp(raw + 1, 0, coarseSize - 1);
    dst->t            = std::clamp(coord - static_cast<float>(raw), 0.0f, 1.0f);
}

static void
prepareBilinearResizeWeights(std::vector<solidify_pushpull_hwy::PushPullBilinearWeights>* xWeights,
                             std::vector<solidify_pushpull_hwy::PushPullBilinearWeights>* yWeights, const int fineWidth,
                             const int fineHeight, const int coarseWidth, const int coarseHeight)
{
    xWeights->resize(static_cast<size_t>(fineWidth));
    yWeights->resize(static_cast<size_t>(fineHeight));
    for (int x = 0; x < fineWidth; ++x) {
        computeBilinearResizeWeight(&(*xWeights)[static_cast<size_t>(x)], x, fineWidth, coarseWidth);
    }
    for (int y = 0; y < fineHeight; ++y) {
        computeBilinearResizeWeight(&(*yWeights)[static_cast<size_t>(y)], y, fineHeight, coarseHeight);
    }
}

static int
findAlphaChannel(const OIIO::ImageBuf& src)
{
    const int alpha = src.spec().alpha_channel;
    if (alpha >= 0 && alpha < src.nchannels()) {
        return alpha;
    }
    if (src.nchannels() == 4) {
        return 3;
    }
    if (src.nchannels() == 2) {
        return 1;
    }
    return -1;
}

static bool
validatePushPullSource(OIIO::ImageBuf& dst, const OIIO::ImageBuf& src)
{
    if (!src.initialized()) {
        dst.errorfmt("push-pull source image is not initialized");
        return false;
    }
    if (src.spec().depth != 1) {
        dst.errorfmt("push-pull does not support volume images");
        return false;
    }
    if (src.nchannels() != 2 && src.nchannels() != 4) {
        dst.errorfmt("push-pull requires grayscale+alpha or RGBA input");
        return false;
    }
    const int alpha = findAlphaChannel(src);
    if (alpha != src.nchannels() - 1) {
        dst.errorfmt("push-pull requires alpha in the last channel");
        return false;
    }
    return true;
}

static void
resetLevel(PushPullLevel* level, const int width, const int height, const int channels)
{
    level->width    = width;
    level->height   = height;
    level->channels = channels;
    level->pixels.assign(static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(channels),
                         0.0f);
}

static bool
readTopLevel(PushPullLevel* level, OIIO::ImageBuf& dst, const OIIO::ImageBuf& src)
{
    const OIIO::ImageSpec& spec = src.spec();
    resetLevel(level, spec.width, spec.height, spec.nchannels);
    const OIIO::ROI roi(src.xbegin(), src.xend(), src.ybegin(), src.yend(), src.zbegin(), src.zend(), 0,
                        spec.nchannels);
    if (!src.get_pixels(roi, OIIO::TypeDesc::FLOAT, level->pixels.data())) {
        dst.errorfmt("push-pull could not read source pixels as float");
        return false;
    }
    return true;
}

static bool
runPullLevel(PushPullLevel* dst, const PushPullLevel& src, const int nthreads)
{
    const int dstWidth  = std::max(1, src.width / 2);
    const int dstHeight = std::max(1, src.height / 2);
    resetLevel(dst, dstWidth, dstHeight, src.channels);

    const bool exact2x = src.width == dstWidth * 2 && src.height == dstHeight * 2;
    if (exact2x) {
        std::atomic<bool> ok = true;
        OIIO::ROI roi(0, dstWidth, 0, dstHeight, 0, 1, 0, src.channels);
        OIIO::ImageBufAlgo::parallel_image(roi, nthreads, [&](OIIO::ROI chunk) {
            solidify_pushpull_hwy::PushPullPullView view;
            view.src       = src.pixels.data();
            view.dst       = dst->pixels.data();
            view.srcWidth  = src.width;
            view.srcHeight = src.height;
            view.dstWidth  = dst->width;
            view.dstHeight = dst->height;
            view.channels  = src.channels;
            view.yBegin    = chunk.ybegin;
            view.yEnd      = chunk.yend;
            if (!solidify_pushpull_hwy::runPullExact2xHwy(&view)) {
                ok = false;
            }
        });
        return ok.load();
    }

    std::vector<solidify_pushpull_hwy::PushPullTriangleWeights> xWeights(static_cast<size_t>(dstWidth));
    std::vector<solidify_pushpull_hwy::PushPullTriangleWeights> yWeights(static_cast<size_t>(dstHeight));
    for (int x = 0; x < dstWidth; ++x) {
        computeTriangleResizeWeights(&xWeights[static_cast<size_t>(x)], x, src.width, dstWidth);
    }
    for (int y = 0; y < dstHeight; ++y) {
        computeTriangleResizeWeights(&yWeights[static_cast<size_t>(y)], y, src.height, dstHeight);
    }

    std::atomic<bool> ok = true;
    OIIO::ROI roi(0, dstWidth, 0, dstHeight, 0, 1, 0, src.channels);
    OIIO::ImageBufAlgo::parallel_image(roi, nthreads, [&](OIIO::ROI chunk) {
        solidify_pushpull_hwy::PushPullPullView view;
        view.src       = src.pixels.data();
        view.dst       = dst->pixels.data();
        view.xWeights  = xWeights.data();
        view.yWeights  = yWeights.data();
        view.srcWidth  = src.width;
        view.srcHeight = src.height;
        view.dstWidth  = dst->width;
        view.dstHeight = dst->height;
        view.channels  = src.channels;
        view.yBegin    = chunk.ybegin;
        view.yEnd      = chunk.yend;
        if (!solidify_pushpull_hwy::runPullHwy(&view)) {
            ok = false;
        }
    });
    return ok.load();
}

static int
appendPullLevelsFromDimensions(std::vector<PushPullLevel>* pyramid, const int baseWidth, const int baseHeight,
                               const int channels)
{
    int width      = baseWidth;
    int height     = baseHeight;
    int levelCount = 0;
    while (levelCount < 4 && (width > 1 || height > 1)) {
        width  = std::max(1, width / 2);
        height = std::max(1, height / 2);
        PushPullLevel level;
        resetLevel(&level, width, height, channels);
        pyramid->push_back(std::move(level));
        ++levelCount;
    }
    return levelCount;
}

static int
appendPullLevels(std::vector<PushPullLevel>* pyramid)
{
    const PushPullLevel& base = pyramid->back();
    return appendPullLevelsFromDimensions(pyramid, base.width, base.height, base.channels);
}

static bool
runPullGroupTiled(std::vector<PushPullLevel>* pyramid, const int nthreads)
{
    const size_t baseIndex = pyramid->size() - 1u;
    const int levelCount   = appendPullLevels(pyramid);
    if (levelCount == 0) {
        return true;
    }

    const PushPullLevel& base = (*pyramid)[baseIndex];
    PushPullLevel* level1     = &(*pyramid)[baseIndex + 1u];
    PushPullLevel* level2     = levelCount >= 2 ? &(*pyramid)[baseIndex + 2u] : nullptr;
    PushPullLevel* level3     = levelCount >= 3 ? &(*pyramid)[baseIndex + 3u] : nullptr;
    PushPullLevel* level4     = levelCount >= 4 ? &(*pyramid)[baseIndex + 4u] : nullptr;
    const int tileCols        = (base.width + 15) / 16;
    const int tileRows        = (base.height + 15) / 16;

    OIIO::ROI tileRoi(0, tileCols, 0, tileRows, 0, 1, 0, base.channels);
    std::atomic<bool> ok = true;
    OIIO::ImageBufAlgo::parallel_image(tileRoi, nthreads, [&](OIIO::ROI chunk) {
        solidify_pushpull_hwy::PushPullPullTiledView view;
        view.base        = base.pixels.data();
        view.level1      = level1->pixels.data();
        view.level2      = level2 != nullptr ? level2->pixels.data() : nullptr;
        view.level3      = level3 != nullptr ? level3->pixels.data() : nullptr;
        view.level4      = level4 != nullptr ? level4->pixels.data() : nullptr;
        view.baseWidth   = base.width;
        view.baseHeight  = base.height;
        view.level1Width = level1->width;
        view.level2Width = level2 != nullptr ? level2->width : 0;
        view.level3Width = level3 != nullptr ? level3->width : 0;
        view.level4Width = level4 != nullptr ? level4->width : 0;
        view.channels    = base.channels;
        view.levelCount  = levelCount;
        view.tileXBegin  = chunk.xbegin;
        view.tileXEnd    = chunk.xend;
        view.tileYBegin  = chunk.ybegin;
        view.tileYEnd    = chunk.yend;
        if (!solidify_pushpull_hwy::runPullTiledHwy(&view)) {
            ok = false;
        }
    });
    if (!ok.load()) {
        return false;
    }

    return true;
}

static bool
runPullGroupTiledFromU16(std::vector<PushPullLevel>* pyramid, const uint16_t* basePixels, const int baseWidth,
                         const int baseHeight, const int channels, const int nthreads)
{
    const size_t baseIndex = pyramid->size();
    const int levelCount   = appendPullLevelsFromDimensions(pyramid, baseWidth, baseHeight, channels);
    if (levelCount == 0) {
        return true;
    }

    PushPullLevel* level1 = &(*pyramid)[baseIndex];
    PushPullLevel* level2 = levelCount >= 2 ? &(*pyramid)[baseIndex + 1u] : nullptr;
    PushPullLevel* level3 = levelCount >= 3 ? &(*pyramid)[baseIndex + 2u] : nullptr;
    PushPullLevel* level4 = levelCount >= 4 ? &(*pyramid)[baseIndex + 3u] : nullptr;
    const int tileCols    = (baseWidth + 15) / 16;
    const int tileRows    = (baseHeight + 15) / 16;

    OIIO::ROI tileRoi(0, tileCols, 0, tileRows, 0, 1, 0, channels);
    std::atomic<bool> ok = true;
    OIIO::ImageBufAlgo::parallel_image(tileRoi, nthreads, [&](OIIO::ROI chunk) {
        solidify_pushpull_hwy::PushPullPullU16TiledView view;
        view.base        = basePixels;
        view.level1      = level1->pixels.data();
        view.level2      = level2 != nullptr ? level2->pixels.data() : nullptr;
        view.level3      = level3 != nullptr ? level3->pixels.data() : nullptr;
        view.level4      = level4 != nullptr ? level4->pixels.data() : nullptr;
        view.baseWidth   = baseWidth;
        view.baseHeight  = baseHeight;
        view.level1Width = level1->width;
        view.level2Width = level2 != nullptr ? level2->width : 0;
        view.level3Width = level3 != nullptr ? level3->width : 0;
        view.level4Width = level4 != nullptr ? level4->width : 0;
        view.channels    = channels;
        view.levelCount  = levelCount;
        view.tileXBegin  = chunk.xbegin;
        view.tileXEnd    = chunk.xend;
        view.tileYBegin  = chunk.ybegin;
        view.tileYEnd    = chunk.yend;
        if (!solidify_pushpull_hwy::runPullU16TiledHwy(&view)) {
            ok = false;
        }
    });
    if (!ok.load()) {
        return false;
    }

    return true;
}

static bool
runPullGroupTiledFromHalf(std::vector<PushPullLevel>* pyramid, const half* basePixels, const int baseWidth,
                          const int baseHeight, const int channels, const int nthreads)
{
    const size_t baseIndex = pyramid->size();
    const int levelCount   = appendPullLevelsFromDimensions(pyramid, baseWidth, baseHeight, channels);
    if (levelCount == 0) {
        return true;
    }

    PushPullLevel* level1 = &(*pyramid)[baseIndex];
    PushPullLevel* level2 = levelCount >= 2 ? &(*pyramid)[baseIndex + 1u] : nullptr;
    PushPullLevel* level3 = levelCount >= 3 ? &(*pyramid)[baseIndex + 2u] : nullptr;
    PushPullLevel* level4 = levelCount >= 4 ? &(*pyramid)[baseIndex + 3u] : nullptr;
    const int tileCols    = (baseWidth + 15) / 16;
    const int tileRows    = (baseHeight + 15) / 16;

    OIIO::ROI tileRoi(0, tileCols, 0, tileRows, 0, 1, 0, channels);
    std::atomic<bool> ok = true;
    OIIO::ImageBufAlgo::parallel_image(tileRoi, nthreads, [&](OIIO::ROI chunk) {
        solidify_pushpull_hwy::PushPullPullHalfTiledView view;
        view.base        = basePixels;
        view.level1      = level1->pixels.data();
        view.level2      = level2 != nullptr ? level2->pixels.data() : nullptr;
        view.level3      = level3 != nullptr ? level3->pixels.data() : nullptr;
        view.level4      = level4 != nullptr ? level4->pixels.data() : nullptr;
        view.baseWidth   = baseWidth;
        view.baseHeight  = baseHeight;
        view.level1Width = level1->width;
        view.level2Width = level2 != nullptr ? level2->width : 0;
        view.level3Width = level3 != nullptr ? level3->width : 0;
        view.level4Width = level4 != nullptr ? level4->width : 0;
        view.channels    = channels;
        view.levelCount  = levelCount;
        view.tileXBegin  = chunk.xbegin;
        view.tileXEnd    = chunk.xend;
        view.tileYBegin  = chunk.ybegin;
        view.tileYEnd    = chunk.yend;
        if (!solidify_pushpull_hwy::runPullHalfTiledHwy(&view)) {
            ok = false;
        }
    });
    if (!ok.load()) {
        return false;
    }

    return true;
}

static bool
runPushLevel(PushPullLevel* dst, const PushPullLevel& fine, const PushPullLevel& coarse, const int nthreads)
{
    resetLevel(dst, fine.width, fine.height, fine.channels);

    std::vector<solidify_pushpull_hwy::PushPullBilinearWeights> xWeights;
    std::vector<solidify_pushpull_hwy::PushPullBilinearWeights> yWeights;
    prepareBilinearResizeWeights(&xWeights, &yWeights, fine.width, fine.height, coarse.width, coarse.height);

    std::atomic<bool> ok = true;
    OIIO::ROI roi(0, fine.width, 0, fine.height, 0, 1, 0, fine.channels);
    OIIO::ImageBufAlgo::parallel_image(roi, nthreads, [&](OIIO::ROI chunk) {
        solidify_pushpull_hwy::PushPullPushView view;
        view.fine         = fine.pixels.data();
        view.coarse       = coarse.pixels.data();
        view.dst          = dst->pixels.data();
        view.xWeights     = xWeights.data();
        view.yWeights     = yWeights.data();
        view.fineWidth    = fine.width;
        view.fineHeight   = fine.height;
        view.coarseWidth  = coarse.width;
        view.coarseHeight = coarse.height;
        view.channels     = fine.channels;
        view.xBegin       = chunk.xbegin;
        view.xEnd         = chunk.xend;
        view.yBegin       = chunk.ybegin;
        view.yEnd         = chunk.yend;
        if (!solidify_pushpull_hwy::runPushHwy(&view)) {
            ok = false;
        }
    });
    return ok.load();
}

static bool
runNormalizeLevelToBuffer(float* dst, const PushPullLevel& src, const int nthreads)
{
    std::atomic<bool> ok = true;
    OIIO::ROI roi(0, src.width, 0, src.height, 0, 1, 0, src.channels);
    OIIO::ImageBufAlgo::parallel_image(roi, nthreads, [&](OIIO::ROI chunk) {
        solidify_pushpull_hwy::PushPullNormalizeView view;
        view.src      = src.pixels.data();
        view.dst      = dst;
        view.width    = src.width;
        view.channels = src.channels;
        view.yBegin   = chunk.ybegin;
        view.yEnd     = chunk.yend;
        if (!solidify_pushpull_hwy::runNormalizeHwy(&view)) {
            ok = false;
        }
    });
    return ok.load();
}

static bool
runNormalizeLevelToU16Buffer(uint16_t* dst, const PushPullLevel& src, const int nthreads)
{
    std::atomic<bool> ok = true;
    OIIO::ROI roi(0, src.width, 0, src.height, 0, 1, 0, src.channels);
    OIIO::ImageBufAlgo::parallel_image(roi, nthreads, [&](OIIO::ROI chunk) {
        solidify_pushpull_hwy::PushPullNormalizeU16View view;
        view.src      = src.pixels.data();
        view.dst      = dst;
        view.width    = src.width;
        view.channels = src.channels;
        view.xBegin   = chunk.xbegin;
        view.xEnd     = chunk.xend;
        view.yBegin   = chunk.ybegin;
        view.yEnd     = chunk.yend;
        if (!solidify_pushpull_hwy::runNormalizeU16Hwy(&view)) {
            ok = false;
        }
    });
    return ok.load();
}

static bool
runNormalizeLevelToHalfBuffer(half* dst, const PushPullLevel& src, const int nthreads)
{
    std::atomic<bool> ok = true;
    OIIO::ROI roi(0, src.width, 0, src.height, 0, 1, 0, src.channels);
    OIIO::ImageBufAlgo::parallel_image(roi, nthreads, [&](OIIO::ROI chunk) {
        solidify_pushpull_hwy::PushPullNormalizeHalfView view;
        view.src      = src.pixels.data();
        view.dst      = dst;
        view.width    = src.width;
        view.channels = src.channels;
        view.xBegin   = chunk.xbegin;
        view.xEnd     = chunk.xend;
        view.yBegin   = chunk.ybegin;
        view.yEnd     = chunk.yend;
        if (!solidify_pushpull_hwy::runNormalizeHalfHwy(&view)) {
            ok = false;
        }
    });
    return ok.load();
}

static bool
runNormalizeU16SourceToBuffer(uint16_t* dst, const uint16_t* srcPixels, const int width, const int height,
                              const int channels, const int nthreads)
{
    std::atomic<bool> ok = true;
    OIIO::ROI roi(0, width, 0, height, 0, 1, 0, channels);
    OIIO::ImageBufAlgo::parallel_image(roi, nthreads, [&](OIIO::ROI chunk) {
        solidify_pushpull_hwy::PushPullNormalizeFromU16View view;
        view.src      = srcPixels;
        view.dst      = dst;
        view.width    = width;
        view.channels = channels;
        view.xBegin   = chunk.xbegin;
        view.xEnd     = chunk.xend;
        view.yBegin   = chunk.ybegin;
        view.yEnd     = chunk.yend;
        if (!solidify_pushpull_hwy::runNormalizeFromU16Hwy(&view)) {
            ok = false;
        }
    });
    return ok.load();
}

static bool
runNormalizeHalfSourceToBuffer(half* dst, const half* srcPixels, const int width, const int height, const int channels,
                               const int nthreads)
{
    std::atomic<bool> ok = true;
    OIIO::ROI roi(0, width, 0, height, 0, 1, 0, channels);
    OIIO::ImageBufAlgo::parallel_image(roi, nthreads, [&](OIIO::ROI chunk) {
        solidify_pushpull_hwy::PushPullNormalizeFromHalfView view;
        view.src      = srcPixels;
        view.dst      = dst;
        view.width    = width;
        view.channels = channels;
        view.xBegin   = chunk.xbegin;
        view.xEnd     = chunk.xend;
        view.yBegin   = chunk.ybegin;
        view.yEnd     = chunk.yend;
        if (!solidify_pushpull_hwy::runNormalizeFromHalfHwy(&view)) {
            ok = false;
        }
    });
    return ok.load();
}

static bool
runNormalizeLevel(std::vector<float>* dst, const PushPullLevel& src, const int nthreads)
{
    dst->assign(src.pixels.size(), 0.0f);
    return runNormalizeLevelToBuffer(dst->data(), src, nthreads);
}

static bool
runFinalLevelToBuffer(float* dst, const PushPullLevel& fine, const PushPullLevel& coarse, const int nthreads)
{
    std::vector<solidify_pushpull_hwy::PushPullBilinearWeights> xWeights;
    std::vector<solidify_pushpull_hwy::PushPullBilinearWeights> yWeights;
    prepareBilinearResizeWeights(&xWeights, &yWeights, fine.width, fine.height, coarse.width, coarse.height);

    std::atomic<bool> ok = true;
    OIIO::ROI roi(0, fine.width, 0, fine.height, 0, 1, 0, fine.channels);
    OIIO::ImageBufAlgo::parallel_image(roi, nthreads, [&](OIIO::ROI chunk) {
        solidify_pushpull_hwy::PushPullFinalView view;
        view.fine         = fine.pixels.data();
        view.coarse       = coarse.pixels.data();
        view.dst          = dst;
        view.xWeights     = xWeights.data();
        view.yWeights     = yWeights.data();
        view.fineWidth    = fine.width;
        view.fineHeight   = fine.height;
        view.coarseWidth  = coarse.width;
        view.coarseHeight = coarse.height;
        view.channels     = fine.channels;
        view.xBegin       = chunk.xbegin;
        view.xEnd         = chunk.xend;
        view.yBegin       = chunk.ybegin;
        view.yEnd         = chunk.yend;
        if (!solidify_pushpull_hwy::runFinalHwy(&view)) {
            ok = false;
        }
    });
    return ok.load();
}

static bool
runFinalLevelToU16Buffer(uint16_t* dst, const PushPullLevel& fine, const PushPullLevel& coarse, const int nthreads)
{
    std::vector<solidify_pushpull_hwy::PushPullBilinearWeights> xWeights;
    std::vector<solidify_pushpull_hwy::PushPullBilinearWeights> yWeights;
    prepareBilinearResizeWeights(&xWeights, &yWeights, fine.width, fine.height, coarse.width, coarse.height);

    std::atomic<bool> ok = true;
    OIIO::ROI roi(0, fine.width, 0, fine.height, 0, 1, 0, fine.channels);
    OIIO::ImageBufAlgo::parallel_image(roi, nthreads, [&](OIIO::ROI chunk) {
        solidify_pushpull_hwy::PushPullFinalU16View view;
        view.fine         = fine.pixels.data();
        view.coarse       = coarse.pixels.data();
        view.dst          = dst;
        view.xWeights     = xWeights.data();
        view.yWeights     = yWeights.data();
        view.fineWidth    = fine.width;
        view.fineHeight   = fine.height;
        view.coarseWidth  = coarse.width;
        view.coarseHeight = coarse.height;
        view.channels     = fine.channels;
        view.xBegin       = chunk.xbegin;
        view.xEnd         = chunk.xend;
        view.yBegin       = chunk.ybegin;
        view.yEnd         = chunk.yend;
        if (!solidify_pushpull_hwy::runFinalU16Hwy(&view)) {
            ok = false;
        }
    });
    return ok.load();
}

static bool
runFinalLevelToHalfBuffer(half* dst, const PushPullLevel& fine, const PushPullLevel& coarse, const int nthreads)
{
    std::vector<solidify_pushpull_hwy::PushPullBilinearWeights> xWeights;
    std::vector<solidify_pushpull_hwy::PushPullBilinearWeights> yWeights;
    prepareBilinearResizeWeights(&xWeights, &yWeights, fine.width, fine.height, coarse.width, coarse.height);

    std::atomic<bool> ok = true;
    OIIO::ROI roi(0, fine.width, 0, fine.height, 0, 1, 0, fine.channels);
    OIIO::ImageBufAlgo::parallel_image(roi, nthreads, [&](OIIO::ROI chunk) {
        solidify_pushpull_hwy::PushPullFinalHalfView view;
        view.fine         = fine.pixels.data();
        view.coarse       = coarse.pixels.data();
        view.dst          = dst;
        view.xWeights     = xWeights.data();
        view.yWeights     = yWeights.data();
        view.fineWidth    = fine.width;
        view.fineHeight   = fine.height;
        view.coarseWidth  = coarse.width;
        view.coarseHeight = coarse.height;
        view.channels     = fine.channels;
        view.xBegin       = chunk.xbegin;
        view.xEnd         = chunk.xend;
        view.yBegin       = chunk.ybegin;
        view.yEnd         = chunk.yend;
        if (!solidify_pushpull_hwy::runFinalHalfHwy(&view)) {
            ok = false;
        }
    });
    return ok.load();
}

static bool
runFinalLevelFromU16ToU16Buffer(uint16_t* dst, const uint16_t* finePixels, const int fineWidth, const int fineHeight,
                                const PushPullLevel& coarse, const int nthreads)
{
    std::vector<solidify_pushpull_hwy::PushPullBilinearWeights> xWeights;
    std::vector<solidify_pushpull_hwy::PushPullBilinearWeights> yWeights;
    prepareBilinearResizeWeights(&xWeights, &yWeights, fineWidth, fineHeight, coarse.width, coarse.height);

    std::atomic<bool> ok = true;
    OIIO::ROI roi(0, fineWidth, 0, fineHeight, 0, 1, 0, coarse.channels);
    OIIO::ImageBufAlgo::parallel_image(roi, nthreads, [&](OIIO::ROI chunk) {
        solidify_pushpull_hwy::PushPullFinalFromU16View view;
        view.fine         = finePixels;
        view.coarse       = coarse.pixels.data();
        view.dst          = dst;
        view.xWeights     = xWeights.data();
        view.yWeights     = yWeights.data();
        view.fineWidth    = fineWidth;
        view.fineHeight   = fineHeight;
        view.coarseWidth  = coarse.width;
        view.coarseHeight = coarse.height;
        view.channels     = coarse.channels;
        view.xBegin       = chunk.xbegin;
        view.xEnd         = chunk.xend;
        view.yBegin       = chunk.ybegin;
        view.yEnd         = chunk.yend;
        if (!solidify_pushpull_hwy::runFinalFromU16Hwy(&view)) {
            ok = false;
        }
    });
    return ok.load();
}

static bool
runFinalLevelFromHalfToHalfBuffer(half* dst, const half* finePixels, const int fineWidth, const int fineHeight,
                                  const PushPullLevel& coarse, const int nthreads)
{
    std::vector<solidify_pushpull_hwy::PushPullBilinearWeights> xWeights;
    std::vector<solidify_pushpull_hwy::PushPullBilinearWeights> yWeights;
    prepareBilinearResizeWeights(&xWeights, &yWeights, fineWidth, fineHeight, coarse.width, coarse.height);

    std::atomic<bool> ok = true;
    OIIO::ROI roi(0, fineWidth, 0, fineHeight, 0, 1, 0, coarse.channels);
    OIIO::ImageBufAlgo::parallel_image(roi, nthreads, [&](OIIO::ROI chunk) {
        solidify_pushpull_hwy::PushPullFinalFromHalfView view;
        view.fine         = finePixels;
        view.coarse       = coarse.pixels.data();
        view.dst          = dst;
        view.xWeights     = xWeights.data();
        view.yWeights     = yWeights.data();
        view.fineWidth    = fineWidth;
        view.fineHeight   = fineHeight;
        view.coarseWidth  = coarse.width;
        view.coarseHeight = coarse.height;
        view.channels     = coarse.channels;
        view.xBegin       = chunk.xbegin;
        view.xEnd         = chunk.xend;
        view.yBegin       = chunk.ybegin;
        view.yEnd         = chunk.yend;
        if (!solidify_pushpull_hwy::runFinalFromHalfHwy(&view)) {
            ok = false;
        }
    });
    return ok.load();
}

static bool
runFinalLevel(std::vector<float>* dst, const PushPullLevel& fine, const PushPullLevel& coarse, const int nthreads)
{
    dst->assign(fine.pixels.size(), 0.0f);
    return runFinalLevelToBuffer(dst->data(), fine, coarse, nthreads);
}

static bool
resetLocalResult(OIIO::ImageBuf& dst, const OIIO::ImageBuf& src)
{
    OIIO::ImageSpec spec = src.spec();
    spec.channelformats.clear();
    dst.reset(spec);
    if (dst.localpixels() == nullptr) {
        dst.errorfmt("push-pull could not allocate result pixels");
        return false;
    }
    return true;
}

static bool
applyPushPullFillLocalU16(OIIO::ImageBuf& dst, const OIIO::ImageBuf& src, const int nthreads)
{
    const OIIO::ImageSpec& spec = src.spec();
    const uint16_t* srcPixels   = static_cast<const uint16_t*>(src.localpixels());
    if (srcPixels == nullptr) {
        return false;
    }

    std::vector<PushPullLevel> pyramid;
    pyramid.reserve(31);

    if (!runPullGroupTiledFromU16(&pyramid, srcPixels, spec.width, spec.height, spec.nchannels, nthreads)) {
        dst.errorfmt("push-pull uint16 tiled pull failed");
        return false;
    }
    while (!pyramid.empty() && (pyramid.back().width > 1 || pyramid.back().height > 1)) {
        if (!runPullGroupTiled(&pyramid, nthreads)) {
            dst.errorfmt("push-pull tiled pull failed");
            return false;
        }
        if (pyramid.empty()) {
            dst.errorfmt("push-pull pyramid construction failed");
            return false;
        }
    }
    for (int i = static_cast<int>(pyramid.size()) - 2; i >= 0; --i) {
        PushPullLevel filled;
        if (!runPushLevel(&filled, pyramid[static_cast<size_t>(i)], pyramid[static_cast<size_t>(i + 1)], nthreads)) {
            dst.errorfmt("push-pull push kernel failed");
            return false;
        }
        pyramid[static_cast<size_t>(i)].pixels.swap(filled.pixels);
    }

    if (!resetLocalResult(dst, src)) {
        return false;
    }
    uint16_t* dstPixels = static_cast<uint16_t*>(dst.localpixels());
    if (!pyramid.empty()) {
        if (!runFinalLevelFromU16ToU16Buffer(dstPixels, srcPixels, spec.width, spec.height, pyramid[0], nthreads)) {
            dst.errorfmt("push-pull final uint16 source kernel failed");
            return false;
        }
    } else {
        if (!runNormalizeU16SourceToBuffer(dstPixels, srcPixels, spec.width, spec.height, spec.nchannels, nthreads)) {
            dst.errorfmt("push-pull normalize uint16 source kernel failed");
            return false;
        }
    }
    return true;
}

static bool
applyPushPullFillLocalHalf(OIIO::ImageBuf& dst, const OIIO::ImageBuf& src, const int nthreads)
{
    const OIIO::ImageSpec& spec = src.spec();
    const half* srcPixels       = static_cast<const half*>(src.localpixels());
    if (srcPixels == nullptr) {
        return false;
    }

    std::vector<PushPullLevel> pyramid;
    pyramid.reserve(31);

    if (!runPullGroupTiledFromHalf(&pyramid, srcPixels, spec.width, spec.height, spec.nchannels, nthreads)) {
        dst.errorfmt("push-pull half tiled pull failed");
        return false;
    }
    while (!pyramid.empty() && (pyramid.back().width > 1 || pyramid.back().height > 1)) {
        if (!runPullGroupTiled(&pyramid, nthreads)) {
            dst.errorfmt("push-pull tiled pull failed");
            return false;
        }
        if (pyramid.empty()) {
            dst.errorfmt("push-pull pyramid construction failed");
            return false;
        }
    }
    for (int i = static_cast<int>(pyramid.size()) - 2; i >= 0; --i) {
        PushPullLevel filled;
        if (!runPushLevel(&filled, pyramid[static_cast<size_t>(i)], pyramid[static_cast<size_t>(i + 1)], nthreads)) {
            dst.errorfmt("push-pull push kernel failed");
            return false;
        }
        pyramid[static_cast<size_t>(i)].pixels.swap(filled.pixels);
    }

    if (!resetLocalResult(dst, src)) {
        return false;
    }
    half* dstPixels = static_cast<half*>(dst.localpixels());
    if (!pyramid.empty()) {
        if (!runFinalLevelFromHalfToHalfBuffer(dstPixels, srcPixels, spec.width, spec.height, pyramid[0], nthreads)) {
            dst.errorfmt("push-pull final half source kernel failed");
            return false;
        }
    } else {
        if (!runNormalizeHalfSourceToBuffer(dstPixels, srcPixels, spec.width, spec.height, spec.nchannels, nthreads)) {
            dst.errorfmt("push-pull normalize half source kernel failed");
            return false;
        }
    }
    return true;
}

static bool
writeResult(OIIO::ImageBuf& dst, const OIIO::ImageBuf& src, const std::vector<float>& pixels)
{
    OIIO::ImageSpec spec = src.spec();
    spec.channelformats.clear();
    dst.reset(spec);

    const OIIO::ROI roi(src.xbegin(), src.xend(), src.ybegin(), src.yend(), src.zbegin(), src.zend(), 0,
                        src.nchannels());
    if (!dst.set_pixels(roi, OIIO::TypeDesc::FLOAT, pixels.data())) {
        dst.errorfmt("push-pull could not write result pixels");
        return false;
    }
    return true;
}

}  // namespace

bool
applyPushPullFill(OIIO::ImageBuf& dst, const OIIO::ImageBuf& src, const int nthreads)
{
    if (!validatePushPullSource(dst, src)) {
        return false;
    }
    if (&dst == &src) {
        OIIO::ImageBuf tmp;
        const bool ok = applyPushPullFill(tmp, src, nthreads);
        dst           = std::move(tmp);
        return ok;
    }
    std::vector<PushPullLevel> pyramid;
    pyramid.reserve(32);
    pyramid.emplace_back();
    if (!readTopLevel(&pyramid.back(), dst, src)) {
        return false;
    }

    while (pyramid.back().width > 1 || pyramid.back().height > 1) {
        PushPullLevel level;
        if (!runPullLevel(&level, pyramid.back(), nthreads)) {
            dst.errorfmt("push-pull pull kernel failed");
            return false;
        }
        pyramid.push_back(std::move(level));
    }
    for (int i = static_cast<int>(pyramid.size()) - 2; i >= 1; --i) {
        PushPullLevel filled;
        if (!runPushLevel(&filled, pyramid[static_cast<size_t>(i)], pyramid[static_cast<size_t>(i + 1)], nthreads)) {
            dst.errorfmt("push-pull push kernel failed");
            return false;
        }
        pyramid[static_cast<size_t>(i)].pixels.swap(filled.pixels);
    }

    if (src.spec().format == OIIO::TypeDesc::FLOAT) {
        if (!resetLocalResult(dst, src)) {
            return false;
        }
        float* dstPixels = static_cast<float*>(dst.localpixels());
        if (pyramid.size() >= 2u) {
            if (!runFinalLevelToBuffer(dstPixels, pyramid[0], pyramid[1], nthreads)) {
                dst.errorfmt("push-pull final kernel failed");
                return false;
            }
        } else {
            if (!runNormalizeLevelToBuffer(dstPixels, pyramid.front(), nthreads)) {
                dst.errorfmt("push-pull normalize kernel failed");
                return false;
            }
        }
        return true;
    } else if (src.spec().format == OIIO::TypeDesc::UINT16) {
        if (!resetLocalResult(dst, src)) {
            return false;
        }
        uint16_t* dstPixels = static_cast<uint16_t*>(dst.localpixels());
        if (pyramid.size() >= 2u) {
            if (!runFinalLevelToU16Buffer(dstPixels, pyramid[0], pyramid[1], nthreads)) {
                dst.errorfmt("push-pull final uint16 kernel failed");
                return false;
            }
        } else {
            if (!runNormalizeLevelToU16Buffer(dstPixels, pyramid.front(), nthreads)) {
                dst.errorfmt("push-pull normalize uint16 kernel failed");
                return false;
            }
        }
        return true;
    } else if (src.spec().format == OIIO::TypeDesc::HALF) {
        if (!resetLocalResult(dst, src)) {
            return false;
        }
        half* dstPixels = static_cast<half*>(dst.localpixels());
        if (pyramid.size() >= 2u) {
            if (!runFinalLevelToHalfBuffer(dstPixels, pyramid[0], pyramid[1], nthreads)) {
                dst.errorfmt("push-pull final half kernel failed");
                return false;
            }
        } else {
            if (!runNormalizeLevelToHalfBuffer(dstPixels, pyramid.front(), nthreads)) {
                dst.errorfmt("push-pull normalize half kernel failed");
                return false;
            }
        }
        return true;
    } else {
        std::vector<float> normalized;
        if (pyramid.size() >= 2u) {
            if (!runFinalLevel(&normalized, pyramid[0], pyramid[1], nthreads)) {
                dst.errorfmt("push-pull final kernel failed");
                return false;
            }
        } else {
            if (!runNormalizeLevel(&normalized, pyramid.front(), nthreads)) {
                dst.errorfmt("push-pull normalize kernel failed");
                return false;
            }
        }
        return writeResult(dst, src, normalized);
    }
}
