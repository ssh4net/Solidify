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

#if defined(SOLIDIFY_IMAGEOPS_HWY_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef SOLIDIFY_IMAGEOPS_HWY_INL_H_
#undef SOLIDIFY_IMAGEOPS_HWY_INL_H_
#else
#define SOLIDIFY_IMAGEOPS_HWY_INL_H_
#endif

#include <hwy/highway.h>

#include <cstdint>
#include <limits>
#include <type_traits>

HWY_BEFORE_NAMESPACE();
namespace solidify_hwy {
namespace HWY_NAMESPACE {
namespace {

namespace hn = hwy::HWY_NAMESPACE;

template<class V>
HWY_ATTR V selectRgb(const V r, const V g, const V b, const uint8_t channel)
{
    switch (channel) {
    case 1: return g;
    case 2: return b;
    default: return r;
    }
}

template<typename T, class D>
HWY_ATTR hn::VFromD<D> invertVec(const D d, const hn::VFromD<D> v, const bool signedRange)
{
    if constexpr (std::is_floating_point_v<T>) {
        return signedRange ? hn::Neg(v) : hn::Sub(hn::Set(d, static_cast<T>(1)), v);
    } else {
        (void)signedRange;
        return hn::Sub(hn::Set(d, std::numeric_limits<T>::max()), v);
    }
}

template<typename T>
HWY_ATTR T invertScalar(const T v, const bool signedRange)
{
    if constexpr (std::is_floating_point_v<T>) {
        return signedRange ? -v : static_cast<T>(1) - v;
    } else {
        (void)signedRange;
        return static_cast<T>(std::numeric_limits<T>::max() - v);
    }
}

template<typename T>
HWY_ATTR void loadRgbAlpha(const T* HWY_RESTRICT src, const int nchannels, const hn::ScalableTag<T> d,
                           hn::VFromD<hn::ScalableTag<T>>& r, hn::VFromD<hn::ScalableTag<T>>& g,
                           hn::VFromD<hn::ScalableTag<T>>& b, hn::VFromD<hn::ScalableTag<T>>& a)
{
    if (nchannels == 4) {
        hn::LoadInterleaved4(d, src, r, g, b, a);
    } else {
        hn::LoadInterleaved3(d, src, r, g, b);
        a = hn::Set(d, std::numeric_limits<T>::max());
    }
}

template<>
HWY_ATTR void loadRgbAlpha<float>(const float* HWY_RESTRICT src, const int nchannels, const hn::ScalableTag<float> d,
                                  hn::VFromD<hn::ScalableTag<float>>& r,
                                  hn::VFromD<hn::ScalableTag<float>>& g,
                                  hn::VFromD<hn::ScalableTag<float>>& b,
                                  hn::VFromD<hn::ScalableTag<float>>& a)
{
    if (nchannels == 4) {
        hn::LoadInterleaved4(d, src, r, g, b, a);
    } else {
        hn::LoadInterleaved3(d, src, r, g, b);
        a = hn::Set(d, 1.0f);
    }
}

template<>
HWY_ATTR void loadRgbAlpha<double>(const double* HWY_RESTRICT src, const int nchannels,
                                   const hn::ScalableTag<double> d, hn::VFromD<hn::ScalableTag<double>>& r,
                                   hn::VFromD<hn::ScalableTag<double>>& g,
                                   hn::VFromD<hn::ScalableTag<double>>& b,
                                   hn::VFromD<hn::ScalableTag<double>>& a)
{
    if (nchannels == 4) {
        hn::LoadInterleaved4(d, src, r, g, b, a);
    } else {
        hn::LoadInterleaved3(d, src, r, g, b);
        a = hn::Set(d, 1.0);
    }
}

template<typename T>
HWY_ATTR bool swapInvertTyped(const SolidifyHwyImageView* view, const SolidifyHwySwapOp* op)
{
    const hn::ScalableTag<T> d;
    using V = hn::VFromD<decltype(d)>;
    const size_t lanes = hn::Lanes(d);

    for (int y = 0; y < view->height; ++y) {
        const uint8_t* srcBytes = static_cast<const uint8_t*>(view->src) + static_cast<size_t>(y) * view->srcRowStride;
        uint8_t* dstBytes = static_cast<uint8_t*>(view->dst) + static_cast<size_t>(y) * view->dstRowStride;
        const T* src = reinterpret_cast<const T*>(srcBytes);
        T* dst = reinterpret_cast<T*>(dstBytes);

        int x = 0;
        for (; x + static_cast<int>(lanes) <= view->width; x += static_cast<int>(lanes)) {
            V r, g, b, a;
            loadRgbAlpha<T>(src + static_cast<size_t>(x) * view->srcChannels, view->srcChannels, d, r, g, b, a);

            V out0 = selectRgb(r, g, b, op->order[0]);
            V out1 = selectRgb(r, g, b, op->order[1]);
            V out2 = selectRgb(r, g, b, op->order[2]);

            if ((op->invertMask & 1u) != 0) {
                out0 = invertVec<T>(d, out0, op->signedRange != 0);
            }
            if ((op->invertMask & 2u) != 0) {
                out1 = invertVec<T>(d, out1, op->signedRange != 0);
            }
            if ((op->invertMask & 4u) != 0) {
                out2 = invertVec<T>(d, out2, op->signedRange != 0);
            }

            T* out = dst + static_cast<size_t>(x) * view->dstChannels;
            if (view->dstChannels == 4) {
                hn::StoreInterleaved4(out0, out1, out2, a, d, out);
            } else {
                hn::StoreInterleaved3(out0, out1, out2, d, out);
            }
        }

        for (; x < view->width; ++x) {
            const T* s = src + static_cast<size_t>(x) * view->srcChannels;
            T* dptr = dst + static_cast<size_t>(x) * view->dstChannels;
            T v0 = s[op->order[0]];
            T v1 = s[op->order[1]];
            T v2 = s[op->order[2]];
            if ((op->invertMask & 1u) != 0) {
                v0 = invertScalar(v0, op->signedRange != 0);
            }
            if ((op->invertMask & 2u) != 0) {
                v1 = invertScalar(v1, op->signedRange != 0);
            }
            if ((op->invertMask & 4u) != 0) {
                v2 = invertScalar(v2, op->signedRange != 0);
            }
            dptr[0] = v0;
            dptr[1] = v1;
            dptr[2] = v2;
            if (view->dstChannels == 4) {
                dptr[3] = s[3];
            }
        }
    }
    return true;
}

template<typename T, typename WideT>
HWY_ATTR hn::VFromD<hn::ScalableTag<T>> grayFixed(const hn::ScalableTag<T> d, const hn::VFromD<hn::ScalableTag<T>> r,
                                                  const hn::VFromD<hn::ScalableTag<T>> g,
                                                  const hn::VFromD<hn::ScalableTag<T>> b,
                                                  const SolidifyHwyGrayscaleOp* op)
{
    const hn::Repartition<WideT, decltype(d)> dw;
    const auto wr = hn::Set(dw, static_cast<WideT>(op->fixedWeights[0]));
    const auto wg = hn::Set(dw, static_cast<WideT>(op->fixedWeights[1]));
    const auto wb = hn::Set(dw, static_cast<WideT>(op->fixedWeights[2]));
    const auto round = hn::Set(dw, static_cast<WideT>(32768));

    auto r0 = hn::PromoteLowerTo(dw, r);
    auto g0 = hn::PromoteLowerTo(dw, g);
    auto b0 = hn::PromoteLowerTo(dw, b);
    auto lo = hn::MulAdd(b0, wb, hn::MulAdd(g0, wg, hn::MulAdd(r0, wr, round)));
    lo = hn::ShiftRight<16>(lo);
    lo = hn::Min(lo, hn::Set(dw, static_cast<WideT>(std::numeric_limits<T>::max())));

    auto r1 = hn::PromoteUpperTo(dw, r);
    auto g1 = hn::PromoteUpperTo(dw, g);
    auto b1 = hn::PromoteUpperTo(dw, b);
    auto hi = hn::MulAdd(b1, wb, hn::MulAdd(g1, wg, hn::MulAdd(r1, wr, round)));
    hi = hn::ShiftRight<16>(hi);
    hi = hn::Min(hi, hn::Set(dw, static_cast<WideT>(std::numeric_limits<T>::max())));

    return hn::OrderedDemote2To(d, lo, hi);
}

template<typename T>
HWY_ATTR hn::VFromD<hn::ScalableTag<T>> grayFloat(const hn::ScalableTag<T> d,
                                                  const hn::VFromD<hn::ScalableTag<T>> r,
                                                  const hn::VFromD<hn::ScalableTag<T>> g,
                                                  const hn::VFromD<hn::ScalableTag<T>> b,
                                                  const SolidifyHwyGrayscaleOp* op)
{
    const auto wr = hn::Set(d, static_cast<T>(op->weights[0]));
    const auto wg = hn::Set(d, static_cast<T>(op->weights[1]));
    const auto wb = hn::Set(d, static_cast<T>(op->weights[2]));
    return hn::MulAdd(b, wb, hn::MulAdd(g, wg, hn::Mul(r, wr)));
}

template<typename T>
HWY_ATTR bool grayscaleTyped(const SolidifyHwyImageView* view, const SolidifyHwyGrayscaleOp* op)
{
    const hn::ScalableTag<T> d;
    using V = hn::VFromD<decltype(d)>;
    const size_t lanes = hn::Lanes(d);

    if constexpr (!std::is_floating_point_v<T>) {
        if (op->mode >= 5 && !(std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t>)) {
            return false;
        }
    }

    for (int y = 0; y < view->height; ++y) {
        const uint8_t* srcBytes = static_cast<const uint8_t*>(view->src) + static_cast<size_t>(y) * view->srcRowStride;
        uint8_t* dstBytes = static_cast<uint8_t*>(view->dst) + static_cast<size_t>(y) * view->dstRowStride;
        const T* src = reinterpret_cast<const T*>(srcBytes);
        T* dst = reinterpret_cast<T*>(dstBytes);

        int x = 0;
        for (; x + static_cast<int>(lanes) <= view->width; x += static_cast<int>(lanes)) {
            V r, g, b, a;
            loadRgbAlpha<T>(src + static_cast<size_t>(x) * view->srcChannels, view->srcChannels, d, r, g, b, a);

            V gray;
            switch (op->mode) {
            case 2: gray = g; break;
            case 3: gray = b; break;
            case 4: gray = a; break;
            case 5:
            case 6:
            case 7:
                if constexpr (std::is_floating_point_v<T>) {
                    gray = grayFloat<T>(d, r, g, b, op);
                } else if constexpr (std::is_same_v<T, uint16_t>) {
                    gray = grayFixed<T, uint32_t>(d, r, g, b, op);
                } else if constexpr (std::is_same_v<T, uint32_t>) {
                    gray = grayFixed<T, uint64_t>(d, r, g, b, op);
                } else {
                    return false;
                }
                break;
            case 1:
            default: gray = r; break;
            }

            T* out = dst + static_cast<size_t>(x) * view->dstChannels;
            if (view->dstChannels == 2) {
                hn::StoreInterleaved2(gray, a, d, out);
            } else {
                hn::Store(gray, d, out);
            }
        }

        for (; x < view->width; ++x) {
            const T* s = src + static_cast<size_t>(x) * view->srcChannels;
            T* dptr = dst + static_cast<size_t>(x) * view->dstChannels;
            T gray = s[0];
            switch (op->mode) {
            case 2: gray = s[1]; break;
            case 3: gray = s[2]; break;
            case 4: gray = s[view->srcChannels == 4 ? 3 : 0]; break;
            case 5:
            case 6:
            case 7:
                if constexpr (std::is_floating_point_v<T>) {
                    gray = static_cast<T>(static_cast<double>(s[0]) * op->weights[0]
                                          + static_cast<double>(s[1]) * op->weights[1]
                                          + static_cast<double>(s[2]) * op->weights[2]);
                } else {
                    const unsigned long long sum = static_cast<unsigned long long>(s[0]) * op->fixedWeights[0]
                                                 + static_cast<unsigned long long>(s[1]) * op->fixedWeights[1]
                                                 + static_cast<unsigned long long>(s[2]) * op->fixedWeights[2]
                                                 + 32768ull;
                    const unsigned long long value = sum >> 16;
                    const unsigned long long maxValue =
                        static_cast<unsigned long long>(std::numeric_limits<T>::max());
                    gray = static_cast<T>(value > maxValue ? maxValue : value);
                }
                break;
            case 1:
            default: break;
            }
            dptr[0] = gray;
            if (view->dstChannels == 2) {
                dptr[1] = s[view->srcChannels == 4 ? 3 : 0];
            }
        }
    }
    return true;
}

bool SwapInvertKernel(const SolidifyHwyImageView* view, const SolidifyHwySwapOp* op)
{
    if (view->srcChannels != view->dstChannels || (view->srcChannels != 3 && view->srcChannels != 4)) {
        return false;
    }

    switch (view->pixelType) {
    case SolidifyHwyPixelType_U8: return swapInvertTyped<uint8_t>(view, op);
    case SolidifyHwyPixelType_U16: return swapInvertTyped<uint16_t>(view, op);
    case SolidifyHwyPixelType_U32: return swapInvertTyped<uint32_t>(view, op);
    case SolidifyHwyPixelType_U64: return swapInvertTyped<uint64_t>(view, op);
    case SolidifyHwyPixelType_F32: return swapInvertTyped<float>(view, op);
    case SolidifyHwyPixelType_F64: return swapInvertTyped<double>(view, op);
    default: return false;
    }
}

bool GrayscaleKernel(const SolidifyHwyImageView* view, const SolidifyHwyGrayscaleOp* op)
{
    if ((view->srcChannels != 3 && view->srcChannels != 4) || (view->dstChannels != 1 && view->dstChannels != 2)) {
        return false;
    }

    switch (view->pixelType) {
    case SolidifyHwyPixelType_U8: return grayscaleTyped<uint8_t>(view, op);
    case SolidifyHwyPixelType_U16: return grayscaleTyped<uint16_t>(view, op);
    case SolidifyHwyPixelType_U32: return grayscaleTyped<uint32_t>(view, op);
    case SolidifyHwyPixelType_U64: return grayscaleTyped<uint64_t>(view, op);
    case SolidifyHwyPixelType_F32: return grayscaleTyped<float>(view, op);
    case SolidifyHwyPixelType_F64: return grayscaleTyped<double>(view, op);
    default: return false;
    }
}

}  // namespace
}  // namespace HWY_NAMESPACE
}  // namespace solidify_hwy
HWY_AFTER_NAMESPACE();

#endif
