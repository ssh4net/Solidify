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

#if defined(SOLIDIFY_PUSHPULL_HWY_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef SOLIDIFY_PUSHPULL_HWY_INL_H_
#undef SOLIDIFY_PUSHPULL_HWY_INL_H_
#else
#define SOLIDIFY_PUSHPULL_HWY_INL_H_
#endif

#include <hwy/highway.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

HWY_BEFORE_NAMESPACE();
namespace solidify_pushpull_hwy {
namespace HWY_NAMESPACE {
namespace {

namespace hn = hwy::HWY_NAMESPACE;

static constexpr float kPushPullAlphaEpsilon = 1.0e-6f;

HWY_ATTR int clampIndex(const int value, const int upper)
{
    if (value < 0) {
        return 0;
    }
    if (value > upper) {
        return upper;
    }
    return value;
}

HWY_ATTR uint16_t floatToU16(const float value)
{
    if (!(value > 0.0f)) {
        return 0u;
    }
    if (value >= 1.0f) {
        return 65535u;
    }
    return static_cast<uint16_t>(value * 65535.0f + 0.5f);
}

HWY_ATTR half floatToHalf(float value)
{
    if (!std::isfinite(value)) {
        if (value > 0.0f) {
            value = 65504.0f;
        } else if (value < 0.0f) {
            value = -65504.0f;
        } else {
            value = 0.0f;
        }
    } else if (value > 65504.0f) {
        value = 65504.0f;
    } else if (value < -65504.0f) {
        value = -65504.0f;
    }
    return half(value);
}

template<int Channels>
HWY_ATTR hn::VFromD<hn::FixedTag<float, Channels>> normalizePulledPixelFixed(
    const hn::FixedTag<float, Channels> d, const hn::VFromD<hn::FixedTag<float, Channels>> v)
{
    float tmp[Channels];
    hn::Store(v, d, tmp);
    const float alpha = tmp[Channels - 1];
    if (alpha != 0.0f) {
        const float invAlpha = 1.0f / alpha;
        for (int c = 0; c < Channels; ++c) {
            tmp[c] *= invAlpha;
        }
        return hn::Load(d, tmp);
    }
    return v;
}

HWY_ATTR int localDownDim(const int localSize, const int globalSize)
{
    if (globalSize == 1 && localSize > 0) {
        return 1;
    }
    return localSize / 2;
}

template<int Channels>
HWY_ATTR void pullRowsFixed(const PushPullPullView* view)
{
    const hn::FixedTag<float, Channels> d;
    using V = hn::VFromD<decltype(d)>;
    const V zero = hn::Zero(d);

    for (int y = view->yBegin; y < view->yEnd; ++y) {
        float* dstRow = view->dst + static_cast<size_t>(y) * static_cast<size_t>(view->dstWidth)
                        * static_cast<size_t>(Channels);
        const PushPullTriangleWeights& yw = view->yWeights[y];
        for (int x = 0; x < view->dstWidth; ++x) {
            const PushPullTriangleWeights& xw = view->xWeights[x];
            V sum = zero;
            for (int dy = 0; dy < yw.taps; ++dy) {
                const float wy = yw.weights[dy];
                if (wy == 0.0f) {
                    continue;
                }
                for (int dx = 0; dx < xw.taps; ++dx) {
                    const float weight = wy * xw.weights[dx];
                    if (weight == 0.0f) {
                        continue;
                    }
                    const float* srcPixel = view->src
                                            + (static_cast<size_t>(yw.indices[dy]) * static_cast<size_t>(view->srcWidth)
                                               + static_cast<size_t>(xw.indices[dx]))
                                                  * static_cast<size_t>(Channels);
                    sum = hn::MulAdd(hn::Load(d, srcPixel), hn::Set(d, weight), sum);
                }
            }
            const V out = normalizePulledPixelFixed<Channels>(d, sum);
            hn::Store(out, d, dstRow + static_cast<size_t>(x) * Channels);
        }
    }
}

template<int Channels>
HWY_ATTR hn::VFromD<hn::FixedTag<float, Channels>> loadExact2xRowFixed(
    const hn::FixedTag<float, Channels> d, const float* src, const int srcWidth, const int y, const int x0,
    const int x1, const int x2, const int x3)
{
    using V = hn::VFromD<hn::FixedTag<float, Channels>>;
    const V three = hn::Set(d, 3.0f);
    const float* p0 = src + (static_cast<size_t>(y) * static_cast<size_t>(srcWidth) + static_cast<size_t>(x0))
                                * static_cast<size_t>(Channels);
    const float* p1 = src + (static_cast<size_t>(y) * static_cast<size_t>(srcWidth) + static_cast<size_t>(x1))
                                * static_cast<size_t>(Channels);
    const float* p2 = src + (static_cast<size_t>(y) * static_cast<size_t>(srcWidth) + static_cast<size_t>(x2))
                                * static_cast<size_t>(Channels);
    const float* p3 = src + (static_cast<size_t>(y) * static_cast<size_t>(srcWidth) + static_cast<size_t>(x3))
                                * static_cast<size_t>(Channels);
    V sum = hn::Load(d, p0);
    sum = hn::MulAdd(hn::Load(d, p1), three, sum);
    sum = hn::MulAdd(hn::Load(d, p2), three, sum);
    return hn::Add(sum, hn::Load(d, p3));
}

template<int Channels>
HWY_ATTR hn::VFromD<hn::FixedTag<float, Channels>> sampleExact2xFixed(
    const PushPullPullView* view, const hn::FixedTag<float, Channels> d, const int sy0, const int sy1, const int sy2,
    const int sy3, const int sx0, const int sx1, const int sx2, const int sx3)
{
    using V = hn::VFromD<hn::FixedTag<float, Channels>>;
    const V three = hn::Set(d, 3.0f);
    const V inv64 = hn::Set(d, 1.0f / 64.0f);
    const V row0 = loadExact2xRowFixed<Channels>(d, view->src, view->srcWidth, sy0, sx0, sx1, sx2, sx3);
    const V row1 = loadExact2xRowFixed<Channels>(d, view->src, view->srcWidth, sy1, sx0, sx1, sx2, sx3);
    const V row2 = loadExact2xRowFixed<Channels>(d, view->src, view->srcWidth, sy2, sx0, sx1, sx2, sx3);
    const V row3 = loadExact2xRowFixed<Channels>(d, view->src, view->srcWidth, sy3, sx0, sx1, sx2, sx3);
    V sum = row0;
    sum = hn::MulAdd(row1, three, sum);
    sum = hn::MulAdd(row2, three, sum);
    sum = hn::Add(sum, row3);
    return normalizePulledPixelFixed<Channels>(d, hn::Mul(sum, inv64));
}

template<int Channels>
HWY_ATTR void storeExact2xPixelFixed(const PushPullPullView* view, const hn::FixedTag<float, Channels> d,
                                     float* dstRow, const int x, const int sy0, const int sy1, const int sy2,
                                     const int sy3, const int sx0, const int sx1, const int sx2, const int sx3)
{
    const hn::VFromD<hn::FixedTag<float, Channels>> out
        = sampleExact2xFixed<Channels>(view, d, sy0, sy1, sy2, sy3, sx0, sx1, sx2, sx3);
    hn::Store(out, d, dstRow + static_cast<size_t>(x) * static_cast<size_t>(Channels));
}

template<int Channels>
HWY_ATTR void pullRowsExact2xFixed(const PushPullPullView* view)
{
    const hn::FixedTag<float, Channels> d;
    const int srcXMax = view->srcWidth - 1;
    const int srcYMax = view->srcHeight - 1;

    for (int y = view->yBegin; y < view->yEnd; ++y) {
        float* dstRow = view->dst + static_cast<size_t>(y) * static_cast<size_t>(view->dstWidth)
                        * static_cast<size_t>(Channels);
        const int srcY = y * 2;
        const int sy0 = clampIndex(srcY - 1, srcYMax);
        const int sy1 = srcY;
        const int sy2 = srcY + 1;
        const int sy3 = clampIndex(srcY + 2, srcYMax);

        if (view->dstWidth == 1) {
            storeExact2xPixelFixed<Channels>(view, d, dstRow, 0, sy0, sy1, sy2, sy3, 0, 0, srcXMax, srcXMax);
            continue;
        }

        storeExact2xPixelFixed<Channels>(view, d, dstRow, 0, sy0, sy1, sy2, sy3, 0, 0, 1, 2);
        for (int x = 1; x + 1 < view->dstWidth; ++x) {
            const int sx0 = x * 2 - 1;
            storeExact2xPixelFixed<Channels>(view, d, dstRow, x, sy0, sy1, sy2, sy3, sx0, sx0 + 1, sx0 + 2,
                                             sx0 + 3);
        }
        const int lastX = view->dstWidth - 1;
        const int srcX = lastX * 2;
        storeExact2xPixelFixed<Channels>(view, d, dstRow, lastX, sy0, sy1, sy2, sy3, srcX - 1, srcX, srcX + 1,
                                         srcXMax);
    }
}

template<int Channels>
HWY_ATTR void storeFixedPixel(const hn::FixedTag<float, Channels> d, const hn::VFromD<hn::FixedTag<float, Channels>> v,
                              float* dst)
{
    hn::Store(v, d, dst);
}

template<int Channels>
HWY_ATTR hn::VFromD<hn::FixedTag<float, Channels>> loadU16PixelFixed(const hn::FixedTag<float, Channels> d,
                                                                     const uint16_t* src)
{
    float tmp[Channels];
    for (int c = 0; c < Channels; ++c) {
        tmp[c] = static_cast<float>(src[c]) * (1.0f / 65535.0f);
    }
    return hn::Load(d, tmp);
}

template<int Channels>
HWY_ATTR hn::VFromD<hn::FixedTag<float, Channels>> loadHalfPixelFixed(const hn::FixedTag<float, Channels> d,
                                                                      const half* src)
{
    float tmp[Channels];
    for (int c = 0; c < Channels; ++c) {
        tmp[c] = static_cast<float>(src[c]);
    }
    return hn::Load(d, tmp);
}

template<int Channels>
HWY_ATTR void pullGlobalToLocalAndGlobalFixed(const PushPullPullTiledView* view, float* localDst,
                                              const int localDstWidth, const int localDstHeight, const int tileX,
                                              const int tileY)
{
    const hn::FixedTag<float, Channels> d;
    using V = hn::VFromD<decltype(d)>;
    const V zero = hn::Zero(d);
    const int dstX0 = tileX / 2;
    const int dstY0 = tileY / 2;

    for (int y = 0; y < localDstHeight; ++y) {
        for (int x = 0; x < localDstWidth; ++x) {
            const int srcX = tileX + x * 2;
            const int srcY = tileY + y * 2;
            V sum = zero;
            int count = 0;
            for (int yy = 0; yy < 2; ++yy) {
                const int sy = srcY + yy;
                if (sy >= view->baseHeight) {
                    continue;
                }
                for (int xx = 0; xx < 2; ++xx) {
                    const int sx = srcX + xx;
                    if (sx >= view->baseWidth) {
                        continue;
                    }
                    const float* pixel = view->base
                                         + (static_cast<size_t>(sy) * static_cast<size_t>(view->baseWidth)
                                            + static_cast<size_t>(sx))
                                               * static_cast<size_t>(Channels);
                    sum = hn::Add(sum, hn::Load(d, pixel));
                    ++count;
                }
            }
            const float scale = count > 0 ? 1.0f / static_cast<float>(count) : 0.0f;
            const V out = normalizePulledPixelFixed<Channels>(d, hn::Mul(sum, hn::Set(d, scale)));
            float* local = localDst + (static_cast<size_t>(y) * static_cast<size_t>(localDstWidth)
                                       + static_cast<size_t>(x))
                                          * static_cast<size_t>(Channels);
            float* global = view->level1
                            + (static_cast<size_t>(dstY0 + y) * static_cast<size_t>(view->level1Width)
                               + static_cast<size_t>(dstX0 + x))
                                  * static_cast<size_t>(Channels);
            storeFixedPixel<Channels>(d, out, local);
            storeFixedPixel<Channels>(d, out, global);
        }
    }
}

template<int Channels>
HWY_ATTR void pullGlobalU16ToLocalAndGlobalFixed(const PushPullPullU16TiledView* view, float* localDst,
                                                 const int localDstWidth, const int localDstHeight, const int tileX,
                                                 const int tileY)
{
    const hn::FixedTag<float, Channels> d;
    using V = hn::VFromD<decltype(d)>;
    const V zero = hn::Zero(d);
    const int dstX0 = tileX / 2;
    const int dstY0 = tileY / 2;

    for (int y = 0; y < localDstHeight; ++y) {
        for (int x = 0; x < localDstWidth; ++x) {
            const int srcX = tileX + x * 2;
            const int srcY = tileY + y * 2;
            V sum = zero;
            int count = 0;
            for (int yy = 0; yy < 2; ++yy) {
                const int sy = srcY + yy;
                if (sy >= view->baseHeight) {
                    continue;
                }
                for (int xx = 0; xx < 2; ++xx) {
                    const int sx = srcX + xx;
                    if (sx >= view->baseWidth) {
                        continue;
                    }
                    const uint16_t* pixel = view->base
                                            + (static_cast<size_t>(sy) * static_cast<size_t>(view->baseWidth)
                                               + static_cast<size_t>(sx))
                                                  * static_cast<size_t>(Channels);
                    sum = hn::Add(sum, loadU16PixelFixed<Channels>(d, pixel));
                    ++count;
                }
            }
            const float scale = count > 0 ? 1.0f / static_cast<float>(count) : 0.0f;
            const V out = normalizePulledPixelFixed<Channels>(d, hn::Mul(sum, hn::Set(d, scale)));
            float* local = localDst + (static_cast<size_t>(y) * static_cast<size_t>(localDstWidth)
                                       + static_cast<size_t>(x))
                                          * static_cast<size_t>(Channels);
            float* global = view->level1
                            + (static_cast<size_t>(dstY0 + y) * static_cast<size_t>(view->level1Width)
                               + static_cast<size_t>(dstX0 + x))
                                  * static_cast<size_t>(Channels);
            storeFixedPixel<Channels>(d, out, local);
            storeFixedPixel<Channels>(d, out, global);
        }
    }
}

template<int Channels>
HWY_ATTR void pullGlobalHalfToLocalAndGlobalFixed(const PushPullPullHalfTiledView* view, float* localDst,
                                                  const int localDstWidth, const int localDstHeight, const int tileX,
                                                  const int tileY)
{
    const hn::FixedTag<float, Channels> d;
    using V = hn::VFromD<decltype(d)>;
    const V zero = hn::Zero(d);
    const int dstX0 = tileX / 2;
    const int dstY0 = tileY / 2;

    for (int y = 0; y < localDstHeight; ++y) {
        for (int x = 0; x < localDstWidth; ++x) {
            const int srcX = tileX + x * 2;
            const int srcY = tileY + y * 2;
            V sum = zero;
            int count = 0;
            for (int yy = 0; yy < 2; ++yy) {
                const int sy = srcY + yy;
                if (sy >= view->baseHeight) {
                    continue;
                }
                for (int xx = 0; xx < 2; ++xx) {
                    const int sx = srcX + xx;
                    if (sx >= view->baseWidth) {
                        continue;
                    }
                    const half* pixel = view->base
                                        + (static_cast<size_t>(sy) * static_cast<size_t>(view->baseWidth)
                                           + static_cast<size_t>(sx))
                                              * static_cast<size_t>(Channels);
                    sum = hn::Add(sum, loadHalfPixelFixed<Channels>(d, pixel));
                    ++count;
                }
            }
            const float scale = count > 0 ? 1.0f / static_cast<float>(count) : 0.0f;
            const V out = normalizePulledPixelFixed<Channels>(d, hn::Mul(sum, hn::Set(d, scale)));
            float* local = localDst + (static_cast<size_t>(y) * static_cast<size_t>(localDstWidth)
                                       + static_cast<size_t>(x))
                                          * static_cast<size_t>(Channels);
            float* global = view->level1
                            + (static_cast<size_t>(dstY0 + y) * static_cast<size_t>(view->level1Width)
                               + static_cast<size_t>(dstX0 + x))
                                  * static_cast<size_t>(Channels);
            storeFixedPixel<Channels>(d, out, local);
            storeFixedPixel<Channels>(d, out, global);
        }
    }
}

template<int Channels>
HWY_ATTR void pullLocalToLocalAndGlobalFixed(const float* localSrc, const int localSrcWidth,
                                             const int localSrcHeight, float* globalDst,
                                             const int globalDstWidth, float* localDst,
                                             const int localDstWidth, const int localDstHeight, const int dstX0,
                                             const int dstY0)
{
    const hn::FixedTag<float, Channels> d;
    using V = hn::VFromD<decltype(d)>;
    const V zero = hn::Zero(d);

    for (int y = 0; y < localDstHeight; ++y) {
        for (int x = 0; x < localDstWidth; ++x) {
            const int srcX = x * 2;
            const int srcY = y * 2;
            V sum = zero;
            int count = 0;
            for (int yy = 0; yy < 2; ++yy) {
                const int sy = srcY + yy;
                if (sy >= localSrcHeight) {
                    continue;
                }
                for (int xx = 0; xx < 2; ++xx) {
                    const int sx = srcX + xx;
                    if (sx >= localSrcWidth) {
                        continue;
                    }
                    const float* pixel = localSrc
                                         + (static_cast<size_t>(sy) * static_cast<size_t>(localSrcWidth)
                                            + static_cast<size_t>(sx))
                                               * static_cast<size_t>(Channels);
                    sum = hn::Add(sum, hn::Load(d, pixel));
                    ++count;
                }
            }
            const float scale = count > 0 ? 1.0f / static_cast<float>(count) : 0.0f;
            const V out = normalizePulledPixelFixed<Channels>(d, hn::Mul(sum, hn::Set(d, scale)));
            float* local = localDst + (static_cast<size_t>(y) * static_cast<size_t>(localDstWidth)
                                       + static_cast<size_t>(x))
                                          * static_cast<size_t>(Channels);
            float* global = globalDst
                            + (static_cast<size_t>(dstY0 + y) * static_cast<size_t>(globalDstWidth)
                               + static_cast<size_t>(dstX0 + x))
                                  * static_cast<size_t>(Channels);
            storeFixedPixel<Channels>(d, out, local);
            storeFixedPixel<Channels>(d, out, global);
        }
    }
}

template<int Channels>
HWY_ATTR void pullTile4Fixed(const PushPullPullTiledView* view, const int tileX, const int tileY)
{
    const int tileWidth = std::min(16, view->baseWidth - tileX);
    const int tileHeight = std::min(16, view->baseHeight - tileY);
    const int level1Height = std::max(1, view->baseHeight / 2);
    const int level2Height = std::max(1, level1Height / 2);
    const int level3Height = std::max(1, level2Height / 2);
    const int l1w = localDownDim(tileWidth, view->baseWidth);
    const int l1h = localDownDim(tileHeight, view->baseHeight);
    const int l2w = localDownDim(l1w, view->level1Width);
    const int l2h = localDownDim(l1h, level1Height);
    const int l3w = localDownDim(l2w, view->level2Width);
    const int l3h = localDownDim(l2h, level2Height);
    const int l4w = localDownDim(l3w, view->level3Width);
    const int l4h = localDownDim(l3h, level3Height);
    float l1[8 * 8 * Channels] = {};
    float l2[4 * 4 * Channels] = {};
    float l3[2 * 2 * Channels] = {};
    float l4[1 * 1 * Channels] = {};

    if (view->levelCount >= 1) {
        pullGlobalToLocalAndGlobalFixed<Channels>(view, l1, l1w, l1h, tileX, tileY);
    }
    if (view->levelCount >= 2) {
        pullLocalToLocalAndGlobalFixed<Channels>(l1, l1w, l1h, view->level2, view->level2Width, l2, l2w, l2h,
                                                 tileX / 4, tileY / 4);
    }
    if (view->levelCount >= 3) {
        pullLocalToLocalAndGlobalFixed<Channels>(l2, l2w, l2h, view->level3, view->level3Width, l3, l3w, l3h,
                                                 tileX / 8, tileY / 8);
    }
    if (view->levelCount >= 4) {
        pullLocalToLocalAndGlobalFixed<Channels>(l3, l3w, l3h, view->level4, view->level4Width, l4, l4w, l4h,
                                                 tileX / 16, tileY / 16);
    }
}

template<int Channels>
HWY_ATTR void pullTile4U16Fixed(const PushPullPullU16TiledView* view, const int tileX, const int tileY)
{
    const int tileWidth = std::min(16, view->baseWidth - tileX);
    const int tileHeight = std::min(16, view->baseHeight - tileY);
    const int level1Height = std::max(1, view->baseHeight / 2);
    const int level2Height = std::max(1, level1Height / 2);
    const int level3Height = std::max(1, level2Height / 2);
    const int l1w = localDownDim(tileWidth, view->baseWidth);
    const int l1h = localDownDim(tileHeight, view->baseHeight);
    const int l2w = localDownDim(l1w, view->level1Width);
    const int l2h = localDownDim(l1h, level1Height);
    const int l3w = localDownDim(l2w, view->level2Width);
    const int l3h = localDownDim(l2h, level2Height);
    const int l4w = localDownDim(l3w, view->level3Width);
    const int l4h = localDownDim(l3h, level3Height);
    float l1[8 * 8 * Channels] = {};
    float l2[4 * 4 * Channels] = {};
    float l3[2 * 2 * Channels] = {};
    float l4[1 * 1 * Channels] = {};

    if (view->levelCount >= 1) {
        pullGlobalU16ToLocalAndGlobalFixed<Channels>(view, l1, l1w, l1h, tileX, tileY);
    }
    if (view->levelCount >= 2) {
        pullLocalToLocalAndGlobalFixed<Channels>(l1, l1w, l1h, view->level2, view->level2Width, l2, l2w, l2h,
                                                 tileX / 4, tileY / 4);
    }
    if (view->levelCount >= 3) {
        pullLocalToLocalAndGlobalFixed<Channels>(l2, l2w, l2h, view->level3, view->level3Width, l3, l3w, l3h,
                                                 tileX / 8, tileY / 8);
    }
    if (view->levelCount >= 4) {
        pullLocalToLocalAndGlobalFixed<Channels>(l3, l3w, l3h, view->level4, view->level4Width, l4, l4w, l4h,
                                                 tileX / 16, tileY / 16);
    }
}

template<int Channels>
HWY_ATTR void pullTile4HalfFixed(const PushPullPullHalfTiledView* view, const int tileX, const int tileY)
{
    const int tileWidth = std::min(16, view->baseWidth - tileX);
    const int tileHeight = std::min(16, view->baseHeight - tileY);
    const int level1Height = std::max(1, view->baseHeight / 2);
    const int level2Height = std::max(1, level1Height / 2);
    const int level3Height = std::max(1, level2Height / 2);
    const int l1w = localDownDim(tileWidth, view->baseWidth);
    const int l1h = localDownDim(tileHeight, view->baseHeight);
    const int l2w = localDownDim(l1w, view->level1Width);
    const int l2h = localDownDim(l1h, level1Height);
    const int l3w = localDownDim(l2w, view->level2Width);
    const int l3h = localDownDim(l2h, level2Height);
    const int l4w = localDownDim(l3w, view->level3Width);
    const int l4h = localDownDim(l3h, level3Height);
    float l1[8 * 8 * Channels] = {};
    float l2[4 * 4 * Channels] = {};
    float l3[2 * 2 * Channels] = {};
    float l4[1 * 1 * Channels] = {};

    if (view->levelCount >= 1) {
        pullGlobalHalfToLocalAndGlobalFixed<Channels>(view, l1, l1w, l1h, tileX, tileY);
    }
    if (view->levelCount >= 2) {
        pullLocalToLocalAndGlobalFixed<Channels>(l1, l1w, l1h, view->level2, view->level2Width, l2, l2w, l2h,
                                                 tileX / 4, tileY / 4);
    }
    if (view->levelCount >= 3) {
        pullLocalToLocalAndGlobalFixed<Channels>(l2, l2w, l2h, view->level3, view->level3Width, l3, l3w, l3h,
                                                 tileX / 8, tileY / 8);
    }
    if (view->levelCount >= 4) {
        pullLocalToLocalAndGlobalFixed<Channels>(l3, l3w, l3h, view->level4, view->level4Width, l4, l4w, l4h,
                                                 tileX / 16, tileY / 16);
    }
}

template<int Channels>
HWY_ATTR void pullTiledFixed(const PushPullPullTiledView* view)
{
    for (int ty = view->tileYBegin; ty < view->tileYEnd; ++ty) {
        const int tileY = ty * 16;
        for (int tx = view->tileXBegin; tx < view->tileXEnd; ++tx) {
            const int tileX = tx * 16;
            pullTile4Fixed<Channels>(view, tileX, tileY);
        }
    }
}

template<int Channels>
HWY_ATTR void pullU16TiledFixed(const PushPullPullU16TiledView* view)
{
    for (int ty = view->tileYBegin; ty < view->tileYEnd; ++ty) {
        const int tileY = ty * 16;
        for (int tx = view->tileXBegin; tx < view->tileXEnd; ++tx) {
            const int tileX = tx * 16;
            pullTile4U16Fixed<Channels>(view, tileX, tileY);
        }
    }
}

template<int Channels>
HWY_ATTR void pullHalfTiledFixed(const PushPullPullHalfTiledView* view)
{
    for (int ty = view->tileYBegin; ty < view->tileYEnd; ++ty) {
        const int tileY = ty * 16;
        for (int tx = view->tileXBegin; tx < view->tileXEnd; ++tx) {
            const int tileX = tx * 16;
            pullTile4HalfFixed<Channels>(view, tileX, tileY);
        }
    }
}

template<int Channels>
HWY_ATTR hn::VFromD<hn::FixedTag<float, Channels>> sampleBilinear(const PushPullPushView* view,
                                                                  const PushPullBilinearWeights& xw,
                                                                  const PushPullBilinearWeights& yw)
{
    const hn::FixedTag<float, Channels> d;
    using V = hn::VFromD<decltype(d)>;

    const float* p00 = view->coarse + (static_cast<size_t>(yw.index0) * static_cast<size_t>(view->coarseWidth)
                                       + static_cast<size_t>(xw.index0))
                                          * static_cast<size_t>(Channels);
    const float* p10 = view->coarse + (static_cast<size_t>(yw.index0) * static_cast<size_t>(view->coarseWidth)
                                       + static_cast<size_t>(xw.index1))
                                          * static_cast<size_t>(Channels);
    const float* p01 = view->coarse + (static_cast<size_t>(yw.index1) * static_cast<size_t>(view->coarseWidth)
                                       + static_cast<size_t>(xw.index0))
                                          * static_cast<size_t>(Channels);
    const float* p11 = view->coarse + (static_cast<size_t>(yw.index1) * static_cast<size_t>(view->coarseWidth)
                                       + static_cast<size_t>(xw.index1))
                                          * static_cast<size_t>(Channels);

    const V v00 = hn::Load(d, p00);
    const V v10 = hn::Load(d, p10);
    const V v01 = hn::Load(d, p01);
    const V v11 = hn::Load(d, p11);
    const V txv = hn::Set(d, xw.t);
    const V tyv = hn::Set(d, yw.t);
    const V top = hn::MulAdd(hn::Sub(v10, v00), txv, v00);
    const V bottom = hn::MulAdd(hn::Sub(v11, v01), txv, v01);
    return hn::MulAdd(hn::Sub(bottom, top), tyv, top);
}

template<int Channels>
HWY_ATTR void pushRowsFixed(const PushPullPushView* view)
{
    const hn::FixedTag<float, Channels> d;
    using V = hn::VFromD<decltype(d)>;

    for (int y = view->yBegin; y < view->yEnd; ++y) {
        const PushPullBilinearWeights& yw = view->yWeights[y];
        for (int x = view->xBegin; x < view->xEnd; ++x) {
            const size_t base = (static_cast<size_t>(y) * static_cast<size_t>(view->fineWidth)
                                 + static_cast<size_t>(x))
                                * static_cast<size_t>(Channels);
            const float* finePixel = view->fine + base;
            float* dstPixel = view->dst + base;
            const float alpha = finePixel[Channels - 1];
            if (alpha >= 1.0f - kPushPullAlphaEpsilon) {
                const V fine = hn::Load(d, finePixel);
                hn::Store(fine, d, dstPixel);
                continue;
            }

            const V fine = hn::Load(d, finePixel);
            const V coarse = sampleBilinear<Channels>(view, view->xWeights[x], yw);
            float missing = 1.0f - alpha;
            if (missing < 0.0f) {
                missing = 0.0f;
            } else if (missing > 1.0f) {
                missing = 1.0f;
            }
            const V out = hn::MulAdd(coarse, hn::Set(d, missing), fine);
            hn::Store(out, d, dstPixel);
        }
    }
}

template<int Channels>
HWY_ATTR void finalRowsFixed(const PushPullFinalView* view)
{
    const hn::FixedTag<float, Channels> d;
    using V = hn::VFromD<decltype(d)>;

    PushPullPushView coarseView;
    coarseView.coarse = view->coarse;
    coarseView.xWeights = view->xWeights;
    coarseView.yWeights = view->yWeights;
    coarseView.coarseWidth = view->coarseWidth;
    coarseView.coarseHeight = view->coarseHeight;
    coarseView.channels = view->channels;

    for (int y = view->yBegin; y < view->yEnd; ++y) {
        const PushPullBilinearWeights& yw = view->yWeights[y];
        for (int x = view->xBegin; x < view->xEnd; ++x) {
            const size_t base = (static_cast<size_t>(y) * static_cast<size_t>(view->fineWidth)
                                 + static_cast<size_t>(x))
                                * static_cast<size_t>(Channels);
            const float* finePixel = view->fine + base;
            float* dstPixel = view->dst + base;
            const float alpha = finePixel[Channels - 1];
            if (alpha >= 1.0f - kPushPullAlphaEpsilon) {
                for (int c = 0; c < Channels - 1; ++c) {
                    dstPixel[c] = finePixel[c];
                }
                dstPixel[Channels - 1] = 1.0f;
                continue;
            }

            const V fine = hn::Load(d, finePixel);
            const V coarse = sampleBilinear<Channels>(&coarseView, view->xWeights[x], yw);
            float missing = 1.0f - alpha;
            if (missing < 0.0f) {
                missing = 0.0f;
            } else if (missing > 1.0f) {
                missing = 1.0f;
            }

            const V filled = hn::MulAdd(coarse, hn::Set(d, missing), fine);
            float tmp[Channels];
            hn::Store(filled, d, tmp);
            const float outAlpha = tmp[Channels - 1];
            const float invAlpha = outAlpha > kPushPullAlphaEpsilon ? 1.0f / outAlpha : 0.0f;
            for (int c = 0; c < Channels - 1; ++c) {
                dstPixel[c] = tmp[c] * invAlpha;
            }
            dstPixel[Channels - 1] = outAlpha > kPushPullAlphaEpsilon ? 1.0f : 0.0f;
        }
    }
}

template<int Channels>
HWY_ATTR void finalRowsU16Fixed(const PushPullFinalU16View* view)
{
    const hn::FixedTag<float, Channels> d;
    using V = hn::VFromD<decltype(d)>;

    PushPullPushView coarseView;
    coarseView.coarse = view->coarse;
    coarseView.xWeights = view->xWeights;
    coarseView.yWeights = view->yWeights;
    coarseView.coarseWidth = view->coarseWidth;
    coarseView.coarseHeight = view->coarseHeight;
    coarseView.channels = view->channels;

    for (int y = view->yBegin; y < view->yEnd; ++y) {
        const PushPullBilinearWeights& yw = view->yWeights[y];
        for (int x = view->xBegin; x < view->xEnd; ++x) {
            const size_t base = (static_cast<size_t>(y) * static_cast<size_t>(view->fineWidth)
                                 + static_cast<size_t>(x))
                                * static_cast<size_t>(Channels);
            const float* finePixel = view->fine + base;
            uint16_t* dstPixel = view->dst + base;
            const float alpha = finePixel[Channels - 1];
            if (alpha >= 1.0f - kPushPullAlphaEpsilon) {
                for (int c = 0; c < Channels - 1; ++c) {
                    dstPixel[c] = floatToU16(finePixel[c]);
                }
                dstPixel[Channels - 1] = 65535u;
                continue;
            }

            const V fine = hn::Load(d, finePixel);
            const V coarse = sampleBilinear<Channels>(&coarseView, view->xWeights[x], yw);
            float missing = 1.0f - alpha;
            if (missing < 0.0f) {
                missing = 0.0f;
            } else if (missing > 1.0f) {
                missing = 1.0f;
            }

            const V filled = hn::MulAdd(coarse, hn::Set(d, missing), fine);
            float tmp[Channels];
            hn::Store(filled, d, tmp);
            const float outAlpha = tmp[Channels - 1];
            const float invAlpha = outAlpha > kPushPullAlphaEpsilon ? 1.0f / outAlpha : 0.0f;
            for (int c = 0; c < Channels - 1; ++c) {
                dstPixel[c] = floatToU16(tmp[c] * invAlpha);
            }
            dstPixel[Channels - 1] = outAlpha > kPushPullAlphaEpsilon ? 65535u : 0u;
        }
    }
}

template<int Channels>
HWY_ATTR void finalRowsHalfFixed(const PushPullFinalHalfView* view)
{
    const hn::FixedTag<float, Channels> d;
    using V = hn::VFromD<decltype(d)>;

    PushPullPushView coarseView;
    coarseView.coarse = view->coarse;
    coarseView.xWeights = view->xWeights;
    coarseView.yWeights = view->yWeights;
    coarseView.coarseWidth = view->coarseWidth;
    coarseView.coarseHeight = view->coarseHeight;
    coarseView.channels = view->channels;

    for (int y = view->yBegin; y < view->yEnd; ++y) {
        const PushPullBilinearWeights& yw = view->yWeights[y];
        for (int x = view->xBegin; x < view->xEnd; ++x) {
            const size_t base = (static_cast<size_t>(y) * static_cast<size_t>(view->fineWidth)
                                 + static_cast<size_t>(x))
                                * static_cast<size_t>(Channels);
            const float* finePixel = view->fine + base;
            half* dstPixel = view->dst + base;
            const float alpha = finePixel[Channels - 1];
            if (alpha >= 1.0f - kPushPullAlphaEpsilon) {
                for (int c = 0; c < Channels - 1; ++c) {
                    dstPixel[c] = floatToHalf(finePixel[c]);
                }
                dstPixel[Channels - 1] = half(1.0f);
                continue;
            }

            const V fine = hn::Load(d, finePixel);
            const V coarse = sampleBilinear<Channels>(&coarseView, view->xWeights[x], yw);
            float missing = 1.0f - alpha;
            if (missing < 0.0f) {
                missing = 0.0f;
            } else if (missing > 1.0f) {
                missing = 1.0f;
            }

            const V filled = hn::MulAdd(coarse, hn::Set(d, missing), fine);
            float tmp[Channels];
            hn::Store(filled, d, tmp);
            const float outAlpha = tmp[Channels - 1];
            const float invAlpha = outAlpha > kPushPullAlphaEpsilon ? 1.0f / outAlpha : 0.0f;
            for (int c = 0; c < Channels - 1; ++c) {
                dstPixel[c] = floatToHalf(tmp[c] * invAlpha);
            }
            dstPixel[Channels - 1] = outAlpha > kPushPullAlphaEpsilon ? half(1.0f) : half(0.0f);
        }
    }
}

template<int Channels>
HWY_ATTR void finalRowsFromU16Fixed(const PushPullFinalFromU16View* view)
{
    const hn::FixedTag<float, Channels> d;
    using V = hn::VFromD<decltype(d)>;

    PushPullPushView coarseView;
    coarseView.coarse = view->coarse;
    coarseView.xWeights = view->xWeights;
    coarseView.yWeights = view->yWeights;
    coarseView.coarseWidth = view->coarseWidth;
    coarseView.coarseHeight = view->coarseHeight;
    coarseView.channels = view->channels;

    for (int y = view->yBegin; y < view->yEnd; ++y) {
        const PushPullBilinearWeights& yw = view->yWeights[y];
        for (int x = view->xBegin; x < view->xEnd; ++x) {
            const size_t base = (static_cast<size_t>(y) * static_cast<size_t>(view->fineWidth)
                                 + static_cast<size_t>(x))
                                * static_cast<size_t>(Channels);
            const uint16_t* finePixel = view->fine + base;
            uint16_t* dstPixel = view->dst + base;
            const uint16_t alphaRaw = finePixel[Channels - 1];
            if (alphaRaw == 65535u) {
                for (int c = 0; c < Channels - 1; ++c) {
                    dstPixel[c] = finePixel[c];
                }
                dstPixel[Channels - 1] = 65535u;
                continue;
            }

            const float alpha = static_cast<float>(alphaRaw) * (1.0f / 65535.0f);
            const V fine = loadU16PixelFixed<Channels>(d, finePixel);
            const V coarse = sampleBilinear<Channels>(&coarseView, view->xWeights[x], yw);
            float missing = 1.0f - alpha;
            if (missing < 0.0f) {
                missing = 0.0f;
            } else if (missing > 1.0f) {
                missing = 1.0f;
            }

            const V filled = hn::MulAdd(coarse, hn::Set(d, missing), fine);
            float tmp[Channels];
            hn::Store(filled, d, tmp);
            const float outAlpha = tmp[Channels - 1];
            const float invAlpha = outAlpha > kPushPullAlphaEpsilon ? 1.0f / outAlpha : 0.0f;
            for (int c = 0; c < Channels - 1; ++c) {
                dstPixel[c] = floatToU16(tmp[c] * invAlpha);
            }
            dstPixel[Channels - 1] = outAlpha > kPushPullAlphaEpsilon ? 65535u : 0u;
        }
    }
}

template<int Channels>
HWY_ATTR void finalRowsFromHalfFixed(const PushPullFinalFromHalfView* view)
{
    const hn::FixedTag<float, Channels> d;
    using V = hn::VFromD<decltype(d)>;

    PushPullPushView coarseView;
    coarseView.coarse = view->coarse;
    coarseView.xWeights = view->xWeights;
    coarseView.yWeights = view->yWeights;
    coarseView.coarseWidth = view->coarseWidth;
    coarseView.coarseHeight = view->coarseHeight;
    coarseView.channels = view->channels;

    for (int y = view->yBegin; y < view->yEnd; ++y) {
        const PushPullBilinearWeights& yw = view->yWeights[y];
        for (int x = view->xBegin; x < view->xEnd; ++x) {
            const size_t base = (static_cast<size_t>(y) * static_cast<size_t>(view->fineWidth)
                                 + static_cast<size_t>(x))
                                * static_cast<size_t>(Channels);
            const half* finePixel = view->fine + base;
            half* dstPixel = view->dst + base;
            const float alpha = static_cast<float>(finePixel[Channels - 1]);
            if (alpha >= 1.0f - kPushPullAlphaEpsilon) {
                for (int c = 0; c < Channels - 1; ++c) {
                    dstPixel[c] = finePixel[c];
                }
                dstPixel[Channels - 1] = half(1.0f);
                continue;
            }

            const V fine = loadHalfPixelFixed<Channels>(d, finePixel);
            const V coarse = sampleBilinear<Channels>(&coarseView, view->xWeights[x], yw);
            float missing = 1.0f - alpha;
            if (missing < 0.0f) {
                missing = 0.0f;
            } else if (missing > 1.0f) {
                missing = 1.0f;
            }

            const V filled = hn::MulAdd(coarse, hn::Set(d, missing), fine);
            float tmp[Channels];
            hn::Store(filled, d, tmp);
            const float outAlpha = tmp[Channels - 1];
            const float invAlpha = outAlpha > kPushPullAlphaEpsilon ? 1.0f / outAlpha : 0.0f;
            for (int c = 0; c < Channels - 1; ++c) {
                dstPixel[c] = floatToHalf(tmp[c] * invAlpha);
            }
            dstPixel[Channels - 1] = outAlpha > kPushPullAlphaEpsilon ? half(1.0f) : half(0.0f);
        }
    }
}

template<int Channels>
HWY_ATTR void normalizeRowsScalarTail(const PushPullNormalizeView* view, const int y, const int xBegin)
{
    for (int x = xBegin; x < view->width; ++x) {
        const size_t base = (static_cast<size_t>(y) * static_cast<size_t>(view->width) + static_cast<size_t>(x))
                            * static_cast<size_t>(Channels);
        const float* src = view->src + base;
        float* dst = view->dst + base;
        const float alpha = src[Channels - 1];
        const float invAlpha = alpha > kPushPullAlphaEpsilon ? 1.0f / alpha : 0.0f;
        for (int c = 0; c < Channels - 1; ++c) {
            dst[c] = src[c] * invAlpha;
        }
        dst[Channels - 1] = alpha > kPushPullAlphaEpsilon ? 1.0f : 0.0f;
    }
}

template<int Channels>
HWY_ATTR void normalizeRowsInterleaved(const PushPullNormalizeView* view)
{
    const hn::ScalableTag<float> d;
    using V = hn::VFromD<decltype(d)>;
    const int lanes = static_cast<int>(hn::Lanes(d));
    const V zero = hn::Zero(d);
    const V one = hn::Set(d, 1.0f);
    const V eps = hn::Set(d, kPushPullAlphaEpsilon);

    for (int y = view->yBegin; y < view->yEnd; ++y) {
        const float* src = view->src + static_cast<size_t>(y) * static_cast<size_t>(view->width)
                                           * static_cast<size_t>(Channels);
        float* dst = view->dst + static_cast<size_t>(y) * static_cast<size_t>(view->width)
                                     * static_cast<size_t>(Channels);
        int x = 0;
        if constexpr (Channels == 4) {
            for (; x + lanes <= view->width; x += lanes) {
                V r, g, b, a;
                hn::LoadInterleaved4(d, src + static_cast<size_t>(x) * 4u, r, g, b, a);
                const auto valid = hn::Gt(a, eps);
                const V invAlpha = hn::IfThenElse(valid, hn::Div(one, a), zero);
                const V outAlpha = hn::IfThenElse(valid, one, zero);
                r = hn::Mul(r, invAlpha);
                g = hn::Mul(g, invAlpha);
                b = hn::Mul(b, invAlpha);
                hn::StoreInterleaved4(r, g, b, outAlpha, d, dst + static_cast<size_t>(x) * 4u);
            }
        } else {
            for (; x + lanes <= view->width; x += lanes) {
                V yv, a;
                hn::LoadInterleaved2(d, src + static_cast<size_t>(x) * 2u, yv, a);
                const auto valid = hn::Gt(a, eps);
                const V invAlpha = hn::IfThenElse(valid, hn::Div(one, a), zero);
                const V outAlpha = hn::IfThenElse(valid, one, zero);
                yv = hn::Mul(yv, invAlpha);
                hn::StoreInterleaved2(yv, outAlpha, d, dst + static_cast<size_t>(x) * 2u);
            }
        }
        normalizeRowsScalarTail<Channels>(view, y, x);
    }
}

template<int Channels>
HWY_ATTR void normalizeRowsU16Fixed(const PushPullNormalizeU16View* view)
{
    for (int y = view->yBegin; y < view->yEnd; ++y) {
        for (int x = view->xBegin; x < view->xEnd; ++x) {
            const size_t base = (static_cast<size_t>(y) * static_cast<size_t>(view->width) + static_cast<size_t>(x))
                                * static_cast<size_t>(Channels);
            const float* src = view->src + base;
            uint16_t* dst = view->dst + base;
            const float alpha = src[Channels - 1];
            const float invAlpha = alpha > kPushPullAlphaEpsilon ? 1.0f / alpha : 0.0f;
            for (int c = 0; c < Channels - 1; ++c) {
                dst[c] = floatToU16(src[c] * invAlpha);
            }
            dst[Channels - 1] = alpha > kPushPullAlphaEpsilon ? 65535u : 0u;
        }
    }
}

template<int Channels>
HWY_ATTR void normalizeRowsHalfFixed(const PushPullNormalizeHalfView* view)
{
    for (int y = view->yBegin; y < view->yEnd; ++y) {
        for (int x = view->xBegin; x < view->xEnd; ++x) {
            const size_t base = (static_cast<size_t>(y) * static_cast<size_t>(view->width) + static_cast<size_t>(x))
                                * static_cast<size_t>(Channels);
            const float* src = view->src + base;
            half* dst = view->dst + base;
            const float alpha = src[Channels - 1];
            const float invAlpha = alpha > kPushPullAlphaEpsilon ? 1.0f / alpha : 0.0f;
            for (int c = 0; c < Channels - 1; ++c) {
                dst[c] = floatToHalf(src[c] * invAlpha);
            }
            dst[Channels - 1] = alpha > kPushPullAlphaEpsilon ? half(1.0f) : half(0.0f);
        }
    }
}

template<int Channels>
HWY_ATTR void normalizeRowsFromU16Fixed(const PushPullNormalizeFromU16View* view)
{
    for (int y = view->yBegin; y < view->yEnd; ++y) {
        for (int x = view->xBegin; x < view->xEnd; ++x) {
            const size_t base = (static_cast<size_t>(y) * static_cast<size_t>(view->width) + static_cast<size_t>(x))
                                * static_cast<size_t>(Channels);
            const uint16_t* src = view->src + base;
            uint16_t* dst = view->dst + base;
            const float alpha = static_cast<float>(src[Channels - 1]) * (1.0f / 65535.0f);
            const float invAlpha = alpha > kPushPullAlphaEpsilon ? 1.0f / alpha : 0.0f;
            for (int c = 0; c < Channels - 1; ++c) {
                dst[c] = floatToU16(static_cast<float>(src[c]) * (1.0f / 65535.0f) * invAlpha);
            }
            dst[Channels - 1] = alpha > kPushPullAlphaEpsilon ? 65535u : 0u;
        }
    }
}

template<int Channels>
HWY_ATTR void normalizeRowsFromHalfFixed(const PushPullNormalizeFromHalfView* view)
{
    for (int y = view->yBegin; y < view->yEnd; ++y) {
        for (int x = view->xBegin; x < view->xEnd; ++x) {
            const size_t base = (static_cast<size_t>(y) * static_cast<size_t>(view->width) + static_cast<size_t>(x))
                                * static_cast<size_t>(Channels);
            const half* src = view->src + base;
            half* dst = view->dst + base;
            const float alpha = static_cast<float>(src[Channels - 1]);
            const float invAlpha = alpha > kPushPullAlphaEpsilon ? 1.0f / alpha : 0.0f;
            for (int c = 0; c < Channels - 1; ++c) {
                dst[c] = floatToHalf(static_cast<float>(src[c]) * invAlpha);
            }
            dst[Channels - 1] = alpha > kPushPullAlphaEpsilon ? half(1.0f) : half(0.0f);
        }
    }
}

bool PushPullPullKernel(const PushPullPullView* view)
{
    if (view->channels == 4) {
        pullRowsFixed<4>(view);
        return true;
    }
    if (view->channels == 2) {
        pullRowsFixed<2>(view);
        return true;
    }
    return false;
}

bool PushPullPullExact2xKernel(const PushPullPullView* view)
{
    if (view->channels == 4) {
        pullRowsExact2xFixed<4>(view);
        return true;
    }
    if (view->channels == 2) {
        pullRowsExact2xFixed<2>(view);
        return true;
    }
    return false;
}

bool PushPullPullTiledKernel(const PushPullPullTiledView* view)
{
    if (view->channels == 4) {
        pullTiledFixed<4>(view);
        return true;
    }
    if (view->channels == 2) {
        pullTiledFixed<2>(view);
        return true;
    }
    return false;
}

bool PushPullPullU16TiledKernel(const PushPullPullU16TiledView* view)
{
    if (view->channels == 4) {
        pullU16TiledFixed<4>(view);
        return true;
    }
    if (view->channels == 2) {
        pullU16TiledFixed<2>(view);
        return true;
    }
    return false;
}

bool PushPullPullHalfTiledKernel(const PushPullPullHalfTiledView* view)
{
    if (view->channels == 4) {
        pullHalfTiledFixed<4>(view);
        return true;
    }
    if (view->channels == 2) {
        pullHalfTiledFixed<2>(view);
        return true;
    }
    return false;
}

bool PushPullPushKernel(const PushPullPushView* view)
{
    if (view->channels == 4) {
        pushRowsFixed<4>(view);
        return true;
    }
    if (view->channels == 2) {
        pushRowsFixed<2>(view);
        return true;
    }
    return false;
}

bool PushPullNormalizeKernel(const PushPullNormalizeView* view)
{
    if (view->channels == 4) {
        normalizeRowsInterleaved<4>(view);
        return true;
    }
    if (view->channels == 2) {
        normalizeRowsInterleaved<2>(view);
        return true;
    }
    return false;
}

bool PushPullFinalKernel(const PushPullFinalView* view)
{
    if (view->channels == 4) {
        finalRowsFixed<4>(view);
        return true;
    }
    if (view->channels == 2) {
        finalRowsFixed<2>(view);
        return true;
    }
    return false;
}

bool PushPullNormalizeU16Kernel(const PushPullNormalizeU16View* view)
{
    if (view->channels == 4) {
        normalizeRowsU16Fixed<4>(view);
        return true;
    }
    if (view->channels == 2) {
        normalizeRowsU16Fixed<2>(view);
        return true;
    }
    return false;
}

bool PushPullFinalU16Kernel(const PushPullFinalU16View* view)
{
    if (view->channels == 4) {
        finalRowsU16Fixed<4>(view);
        return true;
    }
    if (view->channels == 2) {
        finalRowsU16Fixed<2>(view);
        return true;
    }
    return false;
}

bool PushPullNormalizeHalfKernel(const PushPullNormalizeHalfView* view)
{
    if (view->channels == 4) {
        normalizeRowsHalfFixed<4>(view);
        return true;
    }
    if (view->channels == 2) {
        normalizeRowsHalfFixed<2>(view);
        return true;
    }
    return false;
}

bool PushPullFinalHalfKernel(const PushPullFinalHalfView* view)
{
    if (view->channels == 4) {
        finalRowsHalfFixed<4>(view);
        return true;
    }
    if (view->channels == 2) {
        finalRowsHalfFixed<2>(view);
        return true;
    }
    return false;
}

bool PushPullNormalizeFromU16Kernel(const PushPullNormalizeFromU16View* view)
{
    if (view->channels == 4) {
        normalizeRowsFromU16Fixed<4>(view);
        return true;
    }
    if (view->channels == 2) {
        normalizeRowsFromU16Fixed<2>(view);
        return true;
    }
    return false;
}

bool PushPullFinalFromU16Kernel(const PushPullFinalFromU16View* view)
{
    if (view->channels == 4) {
        finalRowsFromU16Fixed<4>(view);
        return true;
    }
    if (view->channels == 2) {
        finalRowsFromU16Fixed<2>(view);
        return true;
    }
    return false;
}

bool PushPullNormalizeFromHalfKernel(const PushPullNormalizeFromHalfView* view)
{
    if (view->channels == 4) {
        normalizeRowsFromHalfFixed<4>(view);
        return true;
    }
    if (view->channels == 2) {
        normalizeRowsFromHalfFixed<2>(view);
        return true;
    }
    return false;
}

bool PushPullFinalFromHalfKernel(const PushPullFinalFromHalfView* view)
{
    if (view->channels == 4) {
        finalRowsFromHalfFixed<4>(view);
        return true;
    }
    if (view->channels == 2) {
        finalRowsFromHalfFixed<2>(view);
        return true;
    }
    return false;
}

}  // namespace
}  // namespace HWY_NAMESPACE
}  // namespace solidify_pushpull_hwy
HWY_AFTER_NAMESPACE();

#endif
