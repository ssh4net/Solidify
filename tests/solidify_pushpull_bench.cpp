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

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>

namespace {

namespace fs = std::filesystem;

struct BenchOptions {
    fs::path dataDir = fs::path(SOLIDIFY_TEST_DATA_DIR);
    int repeats = 1;
    bool runRgb = true;
    bool runGray = true;
    bool runOiio = false;
    bool useUint16 = false;
    bool useHalf = false;
    bool useHalfFiles = false;
    bool useFloatFiles = false;
    bool writeOnly = false;
    fs::path outputDir;
};

static bool readFloatImage(OIIO::ImageBuf* image, const fs::path& path, const int channels)
{
    *image = OIIO::ImageBuf(path.string());
    const bool ok = image->read(0, 0, 0, channels, true, OIIO::TypeDesc::FLOAT);
    if (!ok) {
        std::cerr << "read failed: " << path << ": " << image->geterror() << '\n';
        return false;
    }
    return true;
}

static bool loadMask(OIIO::ImageBuf* mask, const fs::path& path)
{
    if (!readFloatImage(mask, path, 1)) {
        return false;
    }
    mask->specmod().channelnames[0] = "A";
    mask->specmod().alpha_channel = 0;
    return true;
}

static bool makeRgbaSample(OIIO::ImageBuf* rgba, const fs::path& rgbPath, const fs::path& maskPath)
{
    OIIO::ImageBuf rgb;
    OIIO::ImageBuf mask;
    if (!readFloatImage(&rgb, rgbPath, 3) || !loadMask(&mask, maskPath)) {
        return false;
    }

    int channelOrder[3] = { 0, 0, 0 };
    float channelValues[3] = { 1.0f, 1.0f, 1.0f };
    OIIO::ImageBuf rgbMask = OIIO::ImageBufAlgo::channels(mask, 3, channelOrder, channelValues);
    OIIO::ImageBuf premultiplied;
    if (!OIIO::ImageBufAlgo::mul(premultiplied, rgb, rgbMask)) {
        std::cerr << "RGB premultiply failed: " << premultiplied.geterror() << '\n';
        return false;
    }
    if (!OIIO::ImageBufAlgo::channel_append(*rgba, premultiplied, mask)) {
        std::cerr << "RGB alpha append failed: " << rgba->geterror() << '\n';
        return false;
    }
    rgba->specmod().channelnames[3] = "A";
    rgba->specmod().alpha_channel = 3;
    return true;
}

static bool makeGraySample(OIIO::ImageBuf* grayAlpha, const fs::path& grayPath, const fs::path& maskPath)
{
    OIIO::ImageBuf gray;
    OIIO::ImageBuf mask;
    if (!readFloatImage(&gray, grayPath, 1) || !loadMask(&mask, maskPath)) {
        return false;
    }

    OIIO::ImageBuf premultiplied;
    if (!OIIO::ImageBufAlgo::mul(premultiplied, gray, mask)) {
        std::cerr << "gray premultiply failed: " << premultiplied.geterror() << '\n';
        return false;
    }
    if (!OIIO::ImageBufAlgo::channel_append(*grayAlpha, premultiplied, mask)) {
        std::cerr << "gray alpha append failed: " << grayAlpha->geterror() << '\n';
        return false;
    }
    grayAlpha->specmod().channelnames[0] = "Y";
    grayAlpha->specmod().channelnames[1] = "A";
    grayAlpha->specmod().alpha_channel = 1;
    return true;
}

static bool convertSample(OIIO::ImageBuf* image, const OIIO::TypeDesc format, const char* label)
{
    OIIO::ImageBuf converted;
    if (!converted.copy(*image, format)) {
        std::cerr << label << " conversion failed: " << converted.geterror() << '\n';
        return false;
    }
    *image = std::move(converted);
    return true;
}

static const char* sampleTypeLabel(const BenchOptions& options)
{
    if (options.useUint16) {
        return "uint16";
    }
    if (options.useHalf) {
        return "half";
    }
    return "float";
}

static OIIO::TypeDesc outputFormat(const BenchOptions& options)
{
    if (options.useUint16) {
        return OIIO::TypeDesc::UINT16;
    }
    if (options.useHalf) {
        return OIIO::TypeDesc::HALF;
    }
    return OIIO::TypeDesc::FLOAT;
}

static const char* outputExtension(const BenchOptions& options)
{
    return options.useUint16 ? ".tif" : ".exr";
}

static bool writeImage(const OIIO::ImageBuf& image, const OIIO::TypeDesc format, const fs::path& path)
{
    OIIO::ImageBuf output;
    if (!output.copy(image, format)) {
        std::cerr << "copy for write failed: " << output.geterror() << '\n';
        return false;
    }
    if (!output.write(path.string())) {
        std::cerr << "write failed: " << path << ": " << output.geterror() << '\n';
        return false;
    }
    return true;
}

static bool writeComparison(const char* label, const OIIO::ImageBuf& src, const BenchOptions& options)
{
    std::error_code error;
    fs::create_directories(options.outputDir, error);
    if (error) {
        std::cerr << "could not create output directory " << options.outputDir << ": " << error.message() << '\n';
        return false;
    }

    OIIO::ImageBuf native;
    if (!applyPushPullFill(native, src, 0)) {
        std::cerr << label << " native failed: " << native.geterror() << '\n';
        return false;
    }
    OIIO::ImageBuf oiio;
    if (!OIIO::ImageBufAlgo::fillholes_pushpull(oiio, src)) {
        std::cerr << label << " OIIO failed: " << oiio.geterror() << '\n';
        return false;
    }

    const std::string type = sampleTypeLabel(options);
    const std::string extension = outputExtension(options);
    const OIIO::TypeDesc format = outputFormat(options);
    const fs::path nativePath = options.outputDir / (std::string(label) + "_" + type + "_native" + extension);
    const fs::path oiioPath = options.outputDir / (std::string(label) + "_" + type + "_oiio" + extension);
    if (!writeImage(native, format, nativePath) || !writeImage(oiio, format, oiioPath)) {
        return false;
    }
    std::cout << "Wrote " << nativePath << '\n';
    std::cout << "Wrote " << oiioPath << '\n';
    return true;
}

static double secondsSince(std::chrono::steady_clock::time_point start)
{
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
    return elapsed.count();
}

static bool benchNative(const char* label, const OIIO::ImageBuf& src, const int repeats)
{
    for (int i = 0; i < repeats; ++i) {
        OIIO::ImageBuf dst;
        const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        if (!applyPushPullFill(dst, src, 0)) {
            std::cerr << label << " native failed: " << dst.geterror() << '\n';
            return false;
        }
        std::cout << label << " native pass " << (i + 1) << ": " << secondsSince(start) << " s\n";
    }
    return true;
}

static bool benchOiio(const char* label, const OIIO::ImageBuf& src, const int repeats)
{
    for (int i = 0; i < repeats; ++i) {
        OIIO::ImageBuf dst;
        const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        if (!OIIO::ImageBufAlgo::fillholes_pushpull(dst, src)) {
            std::cerr << label << " OIIO failed: " << dst.geterror() << '\n';
            return false;
        }
        std::cout << label << " OIIO pass " << (i + 1) << ": " << secondsSince(start) << " s\n";
    }
    return true;
}

static BenchOptions parseOptions(const int argc, char** argv)
{
    BenchOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--data" && i + 1 < argc) {
            options.dataDir = fs::path(argv[++i]);
        } else if (arg == "--repeats" && i + 1 < argc) {
            options.repeats = std::max(1, std::atoi(argv[++i]));
        } else if (arg == "--rgb-only") {
            options.runRgb = true;
            options.runGray = false;
        } else if (arg == "--gray-only") {
            options.runRgb = false;
            options.runGray = true;
        } else if (arg == "--oiio") {
            options.runOiio = true;
        } else if (arg == "--uint16") {
            options.useUint16 = true;
            options.useHalf = false;
        } else if (arg == "--half") {
            options.useHalf = true;
            options.useUint16 = false;
            options.useHalfFiles = true;
        } else if (arg == "--half-files") {
            options.useHalfFiles = true;
            options.useFloatFiles = false;
        } else if (arg == "--float-files") {
            options.useFloatFiles = true;
            options.useHalfFiles = false;
        } else if (arg == "--write-results" && i + 1 < argc) {
            options.outputDir = fs::path(argv[++i]);
        } else if (arg == "--write-only") {
            options.writeOnly = true;
        } else if (arg == "--help") {
            std::cout << "Usage: solidify_pushpull_bench [--data DIR] [--repeats N] [--rgb-only|--gray-only] [--oiio] [--uint16|--half] [--half-files|--float-files] [--write-results DIR] [--write-only]\n";
        }
    }
    return options;
}

}  // namespace

int main(int argc, char** argv)
{
    const BenchOptions options = parseOptions(argc, argv);
    fs::path rgbPath = options.dataDir / "001_00_D_3282930003039904.TIF";
    const fs::path maskPath = options.dataDir / "001_00_D_3282930003039904_mask.png";
    fs::path grayPath = options.dataDir / "001_00_D_3282930003039904_gray.tif";
    if (options.useHalfFiles) {
        rgbPath = options.dataDir / "001_00_D_3282930003039904_half.exr";
        grayPath = options.dataDir / "001_00_D_3282930003039904_gray_half.exr";
    } else if (options.useFloatFiles) {
        rgbPath = options.dataDir / "001_00_D_3282930003039904_float.exr";
        grayPath = options.dataDir / "001_00_D_3282930003039904_gray_float.exr";
    }

    if (!fs::exists(maskPath)) {
        std::cerr << "mask sample not found: " << maskPath << '\n';
        return 1;
    }

    if (options.runRgb) {
        OIIO::ImageBuf rgba;
        if (!fs::exists(rgbPath) || !makeRgbaSample(&rgba, rgbPath, maskPath)) {
            return 1;
        }
        if (options.useUint16 && !convertSample(&rgba, OIIO::TypeDesc::UINT16, "uint16")) {
            return 1;
        }
        if (options.useHalf && !convertSample(&rgba, OIIO::TypeDesc::HALF, "half")) {
            return 1;
        }
        std::cout << "RGB sample: " << rgba.spec().width << 'x' << rgba.spec().height
                  << " RGBA " << sampleTypeLabel(options) << '\n';
        if (!options.outputDir.empty() && !writeComparison("rgb", rgba, options)) {
            return 1;
        }
        if (!options.writeOnly) {
            if (!benchNative("RGB", rgba, options.repeats)) {
                return 1;
            }
            if (options.runOiio && !benchOiio("RGB", rgba, options.repeats)) {
                return 1;
            }
        }
    }

    if (options.runGray) {
        OIIO::ImageBuf grayAlpha;
        if (!fs::exists(grayPath) || !makeGraySample(&grayAlpha, grayPath, maskPath)) {
            return 1;
        }
        if (options.useUint16 && !convertSample(&grayAlpha, OIIO::TypeDesc::UINT16, "uint16")) {
            return 1;
        }
        if (options.useHalf && !convertSample(&grayAlpha, OIIO::TypeDesc::HALF, "half")) {
            return 1;
        }
        std::cout << "Gray sample: " << grayAlpha.spec().width << 'x' << grayAlpha.spec().height
                  << " YA " << sampleTypeLabel(options) << '\n';
        if (!options.outputDir.empty() && !writeComparison("gray", grayAlpha, options)) {
            return 1;
        }
        if (!options.writeOnly) {
            if (!benchNative("Gray", grayAlpha, options.repeats)) {
                return 1;
            }
            if (options.runOiio && !benchOiio("Gray", grayAlpha, options.repeats)) {
                return 1;
            }
        }
    }

    return 0;
}
