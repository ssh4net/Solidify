/*
 * Solidify (Push Pull) algorithm implementation using OpenImageIO
 * Copyright (c) 2023 Erium Vladlen.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "pch.h"

#include "imageio.h"

#include "settings.h"

bool m_progress_callback(void* opaque_data, float portion_done)
{
    OIIOProgressContext* ctx = static_cast<OIIOProgressContext*>(opaque_data);
    if (ctx != nullptr && ctx->callback != nullptr && *ctx->callback) {
        const float p = ctx->base + ctx->scale * std::clamp(portion_done, 0.0f, 1.0f);
        (*ctx->callback)(p, ctx->status);
    }
    return false;
}

TypeDesc getTypeDesc(int bit_depth)
{
    switch (bit_depth) {
    case 0: return OIIO::TypeDesc::UINT8;
    case 1: return OIIO::TypeDesc::UINT16;
    case 2: return OIIO::TypeDesc::UINT32;
    case 3: return OIIO::TypeDesc::UINT64;
    case 4: return OIIO::TypeDesc::HALF;
    case 5: return OIIO::TypeDesc::FLOAT;
    case 6: return OIIO::TypeDesc::DOUBLE;
    default: return OIIO::TypeDesc::UNKNOWN;
    }
}

std::string formatText(TypeDesc format)
{
    switch (format.basetype) {
    case TypeDesc::UINT8: return "8bit integer";
    case TypeDesc::UINT16: return "16bit integer";
    case TypeDesc::UINT32: return "32bit integer";
    case TypeDesc::UINT64: return "64bit integer";
    case TypeDesc::INT8: return "8bit integer (signed)";
    case TypeDesc::INT16: return "16bit integer (signed)";
    case TypeDesc::INT32: return "32bit integer (signed)";
    case TypeDesc::INT64: return "64bit integer (signed)";
    case TypeDesc::HALF: return "16bit (half) float";
    case TypeDesc::FLOAT: return "32bit float";
    case TypeDesc::DOUBLE: return "64bit (double) float";
    default: return "UNKNOWN bit depth";
    }
}

void formatFromBuff(ImageBuf& buf)
{
    spdlog::info("Format: {}", buf.file_format_name());
}

void debugImageBufWrite(const ImageBuf& buf, const std::string& filename)
{
    const bool write_ok = buf.write(filename);
    if (!write_ok) {
        spdlog::error("Error: Could not write to {}", filename);
        spdlog::error("Error: {}", buf.geterror());
    }
}

std::pair<ImageBuf, ImageBuf> mask_load(const std::string& mask_file, const SolidifyProgressCallback& progressCallback)
{
    ImageBuf alpha_buf(mask_file);
    spdlog::info("Reading {}", mask_file);

    OIIOProgressContext ctx;
    ctx.callback = &progressCallback;
    ctx.status   = "Reading mask: " + std::filesystem::path(mask_file).filename().string();
    ctx.base     = 0.0f;
    ctx.scale    = 1.0f;

    const bool read_ok = alpha_buf.read(0, 0, 0, 1, true, TypeDesc::FLOAT, m_progress_callback,
                                        progressCallback ? &ctx : nullptr);
    if (!read_ok) {
        spdlog::error("Error: Could not read mask image");
        spdlog::error("{}", alpha_buf.geterror());
        return {};
    }

    const int width  = alpha_buf.spec().width;
    const int height = alpha_buf.spec().height;
    spdlog::info("Mask size: {}x{}", width, height);

    alpha_buf.specmod().channelnames[0] = "A";
    alpha_buf.specmod().alpha_channel   = 0;

    int channelorder[]    = { 0, 0, 0 };
    float channelvalues[] = { 1.0f, 1.0f, 1.0f };
    ImageBuf rgb_alpha    = ImageBufAlgo::channels(alpha_buf, 3, channelorder, channelvalues);

    if (rgb_alpha.has_error()) {
        spdlog::error("Error: Could not expand mask channels");
        spdlog::error("{}", rgb_alpha.geterror());
        return {};
    }

    return { alpha_buf, rgb_alpha };
}

bool img_load(ImageBuf& outBuf, const std::string& inputFileName, bool external_alpha,
              const SolidifyProgressCallback& progressCallback)
{
    int last_channel = -1;

    ImageSpec& spec = outBuf.specmod();
    const int nchannels = spec.nchannels;
    if (!external_alpha) {
        if (nchannels == 4) {
            outBuf.specmod().alpha_channel   = 3;
            outBuf.specmod().channelnames[3] = "A";
        } else if (nchannels == 2) {
            outBuf.specmod().alpha_channel   = 1;
            outBuf.specmod().channelnames[1] = "A";
        }
        last_channel = -1;
    } else {
        if (nchannels == 4) {
            last_channel = 3;
        } else if (nchannels == 2) {
            last_channel = 1;
        }
    }

    TypeDesc o_format = spec.format;
    switch (o_format.basetype) {
    case TypeDesc::UINT8: o_format = TypeDesc::UINT16; break;
    case TypeDesc::HALF: o_format = TypeDesc::FLOAT; break;
    default: break;
    }

    OIIOProgressContext ctx;
    ctx.callback = &progressCallback;
    ctx.status   = "Reading: " + std::filesystem::path(inputFileName).filename().string();
    ctx.base     = 0.0f;
    ctx.scale    = 0.35f;

    bool read_ok = outBuf.read(0, 0, 0, last_channel, true, o_format, m_progress_callback,
                               progressCallback ? &ctx : nullptr);
    if (!read_ok) {
        spdlog::error("Error: Could not read input image");
        spdlog::error("{}", outBuf.geterror());
        return false;
    }

    if (!external_alpha && settings.isSolidify) {
        ImageBuf alphs_rgba;
        if (nchannels == 4) {
            int channelorder[]    = { 3, 3, 3 };
            float channelvalues[] = { 1.0f, 1.0f, 1.0f };
            alphs_rgba           = ImageBufAlgo::channels(outBuf, 3, channelorder, channelvalues);
        } else if (nchannels == 2) {
            int channelorder[]    = { 1 };
            float channelvalues[] = { 1.0f };
            alphs_rgba           = ImageBufAlgo::channels(outBuf, 3, channelorder, channelvalues);
        }

        if (alphs_rgba.initialized()) {
            read_ok = ImageBufAlgo::mul(outBuf, outBuf, alphs_rgba);
            if (!read_ok) {
                spdlog::error("Error: Could not multiply alpha");
                spdlog::error("{}", outBuf.geterror());
                return false;
            }
        }
    }

    return true;
}

template<class Rtype>
static bool recalc_normal_impl(ImageBuf& R, const ImageBuf& A, uint channel, int sign, float inCenter,
                               float outCenter, float scale, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::ConstIterator<Rtype> a(A, roi);
        for (ImageBuf::Iterator<Rtype> r(R, roi); !r.done(); ++r, ++a) {
            float t[3];

            t[0] = channel == 0 ? 0.0f : (a[0] - inCenter) / scale;
            t[1] = channel == 1 ? 0.0f : (a[1] - inCenter) / scale;
            t[2] = channel == 2 ? 0.0f : (a[2] - inCenter) / scale;

            const bool isZero = (std::abs(t[0]) + std::abs(t[1]) + std::abs(t[2])) == 0.0f;

            float recomp = 0.0f;
            if (!isZero) {
                recomp = std::sqrt(std::max(0.0f, 1.0f - (t[0] * t[0] + t[1] * t[1] + t[2] * t[2])));
            }

            t[channel] = sign * recomp;

            const float length = std::sqrt(t[0] * t[0] + t[1] * t[1] + t[2] * t[2]);
            const float s      = (length > 0.0f) ? scale / length : scale;

            r[0] = t[0] * s + outCenter;
            r[1] = t[1] * s + outCenter;
            r[2] = t[2] * s + outCenter;

            if (A.spec().nchannels == 4) {
                r[3] = a[3];
            }
        }
    });
    return true;
}

bool recalc_normal(ImageBuf& dst, const ImageBuf& A, uint channel, int sign, float inCenter, float outCenter,
                   float scale, ROI roi, int nthreads)
{
    if (!ImageBufAlgo::IBAprep(roi, &dst, &A)) {
        return false;
    }
    bool ok = false;
    OIIO_DISPATCH_COMMON_TYPES(ok, "recalc_normal", recalc_normal_impl, dst.spec().format, dst, A, channel, sign,
                               inCenter, outCenter, scale, roi, nthreads);
    return ok;
}

ImageBuf recalc_normal(const ImageBuf& A, uint channel, int sign, float inCenter, float outCenter, float scale,
                       ROI roi, int nthreads)
{
    ImageBuf result;
    const bool ok = recalc_normal(result, A, channel, sign, inCenter, outCenter, scale, roi, nthreads);
    if (!ok && !result.has_error()) {
        result.errorfmt("recalculation error");
    }
    return result;
}
