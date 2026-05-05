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

#include "pch.h"

#include "imageops.h"

namespace solidify_hwy {

enum SolidifyHwyPixelType {
    SolidifyHwyPixelType_Unsupported = 0,
    SolidifyHwyPixelType_U8,
    SolidifyHwyPixelType_U16,
    SolidifyHwyPixelType_U32,
    SolidifyHwyPixelType_U64,
    SolidifyHwyPixelType_F32,
    SolidifyHwyPixelType_F64,
};

struct SolidifyHwyImageView {
    const void* src = nullptr;
    void* dst = nullptr;
    int width = 0;
    int height = 0;
    int srcChannels = 0;
    int dstChannels = 0;
    ptrdiff_t srcRowStride = 0;
    ptrdiff_t dstRowStride = 0;
    int pixelType = SolidifyHwyPixelType_Unsupported;
};

struct SolidifyHwySwapOp {
    uint8_t order[3] = { 0, 1, 2 };
    uint8_t invertMask = 0;
    uint8_t signedRange = 0;
};

struct SolidifyHwyGrayscaleOp {
    uint8_t mode = 0;
    float weights[3] = { 0.2126f, 0.7152f, 0.0722f };
    uint32_t fixedWeights[3] = { 13933u, 46871u, 4732u };
};

}  // namespace solidify_hwy

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "imageops_hwy.inl"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>
#include "imageops_hwy.inl"

#if HWY_ONCE
namespace solidify_hwy {

HWY_EXPORT(SwapInvertKernel);
HWY_EXPORT(GrayscaleKernel);

static bool runSwapInvertHwy(const SolidifyHwyImageView* view, const SolidifyHwySwapOp* op)
{
    return HWY_DYNAMIC_DISPATCH(SwapInvertKernel)(view, op);
}

static bool runGrayscaleHwy(const SolidifyHwyImageView* view, const SolidifyHwyGrayscaleOp* op)
{
    return HWY_DYNAMIC_DISPATCH(GrayscaleKernel)(view, op);
}

}  // namespace solidify_hwy
#endif

namespace {

static constexpr int kSwapOrders[6][3] = {
    { 0, 1, 2 },
    { 0, 2, 1 },
    { 1, 0, 2 },
    { 1, 2, 0 },
    { 2, 0, 1 },
    { 2, 1, 0 },
};

static int pixelTypeFromFormat(const OIIO::TypeDesc format)
{
    switch (format.basetype) {
    case OIIO::TypeDesc::UINT8: return solidify_hwy::SolidifyHwyPixelType_U8;
    case OIIO::TypeDesc::UINT16: return solidify_hwy::SolidifyHwyPixelType_U16;
    case OIIO::TypeDesc::UINT32: return solidify_hwy::SolidifyHwyPixelType_U32;
    case OIIO::TypeDesc::UINT64: return solidify_hwy::SolidifyHwyPixelType_U64;
    case OIIO::TypeDesc::FLOAT: return solidify_hwy::SolidifyHwyPixelType_F32;
    case OIIO::TypeDesc::DOUBLE: return solidify_hwy::SolidifyHwyPixelType_F64;
    default: return solidify_hwy::SolidifyHwyPixelType_Unsupported;
    }
}

static bool canUsePackedHwy(const OIIO::ImageBuf& src, const OIIO::ImageBuf& dst)
{
    const OIIO::ImageSpec& sspec = src.spec();
    const OIIO::ImageSpec& dspec = dst.spec();
    const ptrdiff_t srcPixel = static_cast<ptrdiff_t>(sspec.nchannels * sspec.format.size());
    const ptrdiff_t dstPixel = static_cast<ptrdiff_t>(dspec.nchannels * dspec.format.size());
    return src.localpixels() != nullptr && dst.localpixels() != nullptr && sspec.depth == 1 && dspec.depth == 1
           && sspec.format == dspec.format && pixelTypeFromFormat(sspec.format) != solidify_hwy::SolidifyHwyPixelType_Unsupported
           && src.pixel_stride() == srcPixel && dst.pixel_stride() == dstPixel;
}

static int findAlphaChannel(const OIIO::ImageBuf& src)
{
    int alpha = src.spec().alpha_channel;
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

static bool runSwapInvertHwyParallel(OIIO::ImageBuf& dst, const OIIO::ImageBuf& src,
                                     const solidify_hwy::SolidifyHwySwapOp& op, const int nthreads)
{
    if (!canUsePackedHwy(src, dst)) {
        return false;
    }

    std::atomic<bool> ok = true;
    const int pixelType = pixelTypeFromFormat(src.spec().format);
    OIIO::ROI roi(src.xbegin(), src.xend(), src.ybegin(), src.yend(), src.zbegin(), src.zend(), 0,
                  src.nchannels());
    OIIO::ImageBufAlgo::parallel_image(roi, nthreads, [&](OIIO::ROI chunk) {
        const ptrdiff_t srcPixelStride = src.pixel_stride();
        const ptrdiff_t dstPixelStride = dst.pixel_stride();
        const uint8_t* srcBase = static_cast<const uint8_t*>(src.localpixels());
        uint8_t* dstBase = static_cast<uint8_t*>(dst.localpixels());
        solidify_hwy::SolidifyHwyImageView view;
        view.src = srcBase + static_cast<size_t>(chunk.ybegin - src.ybegin()) * src.scanline_stride()
                   + static_cast<size_t>(chunk.xbegin - src.xbegin()) * srcPixelStride;
        view.dst = dstBase + static_cast<size_t>(chunk.ybegin - dst.ybegin()) * dst.scanline_stride()
                   + static_cast<size_t>(chunk.xbegin - dst.xbegin()) * dstPixelStride;
        view.width = chunk.width();
        view.height = chunk.height();
        view.srcChannels = src.nchannels();
        view.dstChannels = dst.nchannels();
        view.srcRowStride = src.scanline_stride();
        view.dstRowStride = dst.scanline_stride();
        view.pixelType = pixelType;
        if (!solidify_hwy::runSwapInvertHwy(&view, &op)) {
            ok = false;
        }
    });
    return ok.load();
}

static bool runGrayscaleHwyParallel(OIIO::ImageBuf& dst, const OIIO::ImageBuf& src,
                                    const solidify_hwy::SolidifyHwyGrayscaleOp& op, const int nthreads)
{
    if (!canUsePackedHwy(src, dst)) {
        return false;
    }

    std::atomic<bool> ok = true;
    const int pixelType = pixelTypeFromFormat(src.spec().format);
    OIIO::ROI roi(src.xbegin(), src.xend(), src.ybegin(), src.yend(), src.zbegin(), src.zend(), 0,
                  src.nchannels());
    OIIO::ImageBufAlgo::parallel_image(roi, nthreads, [&](OIIO::ROI chunk) {
        const ptrdiff_t srcPixelStride = src.pixel_stride();
        const ptrdiff_t dstPixelStride = dst.pixel_stride();
        const uint8_t* srcBase = static_cast<const uint8_t*>(src.localpixels());
        uint8_t* dstBase = static_cast<uint8_t*>(dst.localpixels());
        solidify_hwy::SolidifyHwyImageView view;
        view.src = srcBase + static_cast<size_t>(chunk.ybegin - src.ybegin()) * src.scanline_stride()
                   + static_cast<size_t>(chunk.xbegin - src.xbegin()) * srcPixelStride;
        view.dst = dstBase + static_cast<size_t>(chunk.ybegin - dst.ybegin()) * dst.scanline_stride()
                   + static_cast<size_t>(chunk.xbegin - dst.xbegin()) * dstPixelStride;
        view.width = chunk.width();
        view.height = chunk.height();
        view.srcChannels = src.nchannels();
        view.dstChannels = dst.nchannels();
        view.srcRowStride = src.scanline_stride();
        view.dstRowStride = dst.scanline_stride();
        view.pixelType = pixelType;
        if (!solidify_hwy::runGrayscaleHwy(&view, &op)) {
            ok = false;
        }
    });
    return ok.load();
}

static void setFixedWeights(solidify_hwy::SolidifyHwyGrayscaleOp* op)
{
    if (op->mode == 6) {
        op->weights[0] = 1.0f / 3.0f;
        op->weights[1] = 1.0f / 3.0f;
        op->weights[2] = 1.0f / 3.0f;
        op->fixedWeights[0] = 21845u;
        op->fixedWeights[1] = 21845u;
        op->fixedWeights[2] = 21846u;
    } else if (op->mode == 5) {
        op->weights[0] = 0.2126f;
        op->weights[1] = 0.7152f;
        op->weights[2] = 0.0722f;
        op->fixedWeights[0] = 13933u;
        op->fixedWeights[1] = 46871u;
        op->fixedWeights[2] = 4732u;
    } else {
        static constexpr uint64_t kMaxFixedTotal = 4294901760ull;
        uint64_t fixed[3] = { 0u, 0u, 0u };
        uint64_t total = 0u;
        for (int i = 0; i < 3; ++i) {
            const float rawWeight = op->weights[i];
            const double weight = std::isfinite(rawWeight) && rawWeight > 0.0f ? static_cast<double>(rawWeight) : 0.0;
            fixed[i] = static_cast<uint64_t>(std::min(4294967295.0, weight * 65536.0 + 0.5));
            total += fixed[i];
        }
        if (total > kMaxFixedTotal) {
            for (int i = 0; i < 3; ++i) {
                op->fixedWeights[i] = static_cast<uint32_t>((fixed[i] * kMaxFixedTotal) / total);
            }
        } else {
            for (int i = 0; i < 3; ++i) {
                op->fixedWeights[i] = static_cast<uint32_t>(fixed[i]);
            }
        }
    }
}

static void setGraySpec(OIIO::ImageSpec* spec, const int outChannels)
{
    spec->nchannels = outChannels;
    spec->channelnames.clear();
    spec->default_channel_names();
    spec->channelnames[0] = "Y";
    spec->alpha_channel = -1;
    spec->z_channel = -1;
    spec->channelformats.clear();
    if (outChannels == 2) {
        spec->channelnames[1] = "A";
        spec->alpha_channel = 1;
    }
}

}  // namespace

bool applyChannelSwapInvert(OIIO::ImageBuf& dst, const OIIO::ImageBuf& src, unsigned int basis,
                            unsigned int invertMask, const bool signedRange, const int nthreads)
{
    if (!src.initialized()) {
        dst.errorfmt("swap/invert source image is not initialized");
        return false;
    }
    if (src.nchannels() < 3 || src.nchannels() > 4) {
        dst.errorfmt("swap/invert requires RGB or RGBA images");
        return false;
    }

    basis = std::clamp<unsigned int>(basis, 0, 5);
    invertMask = std::clamp<unsigned int>(invertMask, 0, 7);
    if (basis == 0 && invertMask == 0) {
        return dst.copy(src);
    }

    if (&dst == &src) {
        OIIO::ImageBuf tmp;
        const bool ok = applyChannelSwapInvert(tmp, src, basis, invertMask, signedRange, nthreads);
        dst = std::move(tmp);
        return ok;
    }

    OIIO::ImageSpec spec = src.spec();
    spec.channelformats.clear();
    dst.reset(spec);

    solidify_hwy::SolidifyHwySwapOp op;
    op.order[0] = static_cast<uint8_t>(kSwapOrders[basis][0]);
    op.order[1] = static_cast<uint8_t>(kSwapOrders[basis][1]);
    op.order[2] = static_cast<uint8_t>(kSwapOrders[basis][2]);
    op.invertMask = static_cast<uint8_t>(invertMask);
    op.signedRange = signedRange ? 1 : 0;

    if (!runSwapInvertHwyParallel(dst, src, op, nthreads)) {
        dst.errorfmt("swap/invert requires packed RGB/RGBA uint or float image data");
        return false;
    }
    return true;
}

bool applyGrayscale(OIIO::ImageBuf& dst, const OIIO::ImageBuf& src, unsigned int mode, const float weights[3],
                    const bool preserveAlpha, const int nthreads)
{
    if (!src.initialized()) {
        dst.errorfmt("grayscale source image is not initialized");
        return false;
    }

    mode = std::clamp<unsigned int>(mode, 0, 7);
    if (mode == 0) {
        return dst.copy(src);
    }

    const int srcChannels = src.nchannels();
    const int alphaChannel = findAlphaChannel(src);
    if ((mode == 2 && srcChannels < 2) || (mode == 3 && srcChannels < 3)
        || ((mode == 5 || mode == 6 || mode == 7) && srcChannels < 3)) {
        dst.errorfmt("selected grayscale mode requires RGB input");
        return false;
    }
    if (mode == 4 && alphaChannel < 0) {
        dst.errorfmt("selected grayscale mask mode requires an alpha channel");
        return false;
    }

    if (&dst == &src) {
        OIIO::ImageBuf tmp;
        const bool ok = applyGrayscale(tmp, src, mode, weights, preserveAlpha, nthreads);
        dst = std::move(tmp);
        return ok;
    }

    const int outChannels = preserveAlpha && alphaChannel >= 0 && mode != 4 ? 2 : 1;
    OIIO::ImageSpec spec = src.spec();
    setGraySpec(&spec, outChannels);
    dst.reset(spec);

    solidify_hwy::SolidifyHwyGrayscaleOp op;
    op.mode = static_cast<uint8_t>(mode);
    op.weights[0] = weights != nullptr ? weights[0] : 0.2126f;
    op.weights[1] = weights != nullptr ? weights[1] : 0.7152f;
    op.weights[2] = weights != nullptr ? weights[2] : 0.0722f;
    setFixedWeights(&op);

    if (!runGrayscaleHwyParallel(dst, src, op, nthreads)) {
        dst.errorfmt("grayscale requires packed RGB/RGBA uint or float image data");
        return false;
    }
    return true;
}
