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

#include "settings.h"

Settings settings;
Settings settingsDefaults;
std::string settingsConfigPath = "sldf_config.toml";

template<typename T>
static void
get_value(const toml::value& data, const std::string& section, const std::string& key, T& var)
{
    if (data.contains(section) && data.at(section).contains(key)) {
        var = toml::find<T>(data, section, key);
    }
}

static std::string
settingsToken(const std::string& value)
{
    std::string result;
    result.reserve(value.size());
    for (char c : value) {
        if (c != ' ' && c != '-' && c != '_' && c != '/') {
            result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }
    return result;
}

static int
tiffCompressionFromString(const std::string& value, int fallback)
{
    const std::string token = settingsToken(value);
    if (token == "zip" || token == "deflate" || token == "zipdeflate") {
        return TiffCompression_Zip;
    }
    if (token == "lzw") {
        return TiffCompression_Lzw;
    }
    if (token == "packbits") {
        return TiffCompression_PackBits;
    }
    if (token == "none") {
        return TiffCompression_None;
    }
    return fallback;
}

static int
exrCompressionFromString(const std::string& value, int fallback)
{
    const std::string token = settingsToken(value);
    if (token == "zip" || token == "deflate" || token == "zipdeflate") {
        return ExrCompression_Zip;
    }
    if (token == "zips") {
        return ExrCompression_Zips;
    }
    if (token == "piz") {
        return ExrCompression_Piz;
    }
    if (token == "pxr24") {
        return ExrCompression_Pxr24;
    }
    if (token == "rle") {
        return ExrCompression_Rle;
    }
    if (token == "b44") {
        return ExrCompression_B44;
    }
    if (token == "b44a") {
        return ExrCompression_B44A;
    }
    if (token == "dwaa") {
        return ExrCompression_Dwaa;
    }
    if (token == "dwab") {
        return ExrCompression_Dwab;
    }
    if (token == "htj2k256") {
        return ExrCompression_Htj2k256;
    }
    if (token == "htj2k32") {
        return ExrCompression_Htj2k32;
    }
    if (token == "none") {
        return ExrCompression_None;
    }
    return fallback;
}

static int
pngStrategyFromString(const std::string& value, int fallback)
{
    const std::string token = settingsToken(value);
    if (token == "default") {
        return PngCompression_Default;
    }
    if (token == "filtered") {
        return PngCompression_Filtered;
    }
    if (token == "huffman") {
        return PngCompression_Huffman;
    }
    if (token == "rle") {
        return PngCompression_Rle;
    }
    if (token == "fixed") {
        return PngCompression_Fixed;
    }
    if (token == "fast" || token == "pngfast") {
        return PngCompression_Fast;
    }
    if (token == "none") {
        return PngCompression_None;
    }
    return fallback;
}

static int
jpegSubsamplingFromString(const std::string& value, int fallback)
{
    const std::string token = settingsToken(value);
    if (token == "4:4:4" || token == "444") {
        return JpegSubsampling_444;
    }
    if (token == "4:2:2" || token == "422") {
        return JpegSubsampling_422;
    }
    if (token == "4:2:0" || token == "420") {
        return JpegSubsampling_420;
    }
    if (token == "4:1:1" || token == "411") {
        return JpegSubsampling_411;
    }
    return fallback;
}

static void
get_codec_value(const toml::value& data, const std::string& section, const std::string& key, int& var,
                int (*decode)(const std::string&, int))
{
    if (data.contains(section) && data.at(section).contains(key)) {
        const std::string value = toml::find<std::string>(data, section, key);
        var                     = decode(value, var);
    }
}

bool
loadSettings(Settings& outSettings, const std::string& filename)
{
    try {
        const auto data = toml::parse(filename);
        Settings loaded;

        get_value(data, "Global", "Solidify", loaded.isSolidify);
        get_value(data, "Global", "UseAlpha", loaded.useAlpha);
        get_value(data, "Global", "ExportAlpha", loaded.alphaMode);
        get_value(data, "Global", "Console", loaded.conEnable);
        get_value(data, "Global", "Threads", loaded.numThreads);
        get_value(data, "Global", "QueueLimit", loaded.queueLimit);
        get_value(data, "Global", "Verbosity", loaded.verbosity);

        if (data.contains("Global") && data.at("Global").contains("MaskNames")) {
            std::vector<std::string> values = toml::find<std::vector<std::string>>(data, "Global", "MaskNames");
            if (!values.empty()) {
                loaded.mask_substr = std::move(values);
            }
        }

        get_value(data, "Normalize", "NormalizeMode", loaded.normMode);
        if (data.contains("Normalize") && data.at("Normalize").contains("NormalsNames")) {
            std::vector<std::string> values = toml::find<std::vector<std::string>>(data, "Normalize", "NormalsNames");
            if (!values.empty()) {
                loaded.normNames = std::move(values);
            }
        }

        get_value(data, "Range", "RangeMode", loaded.rangeMode);

        get_value(data, "Transform", "SwapBasis", loaded.swapBasis);
        get_value(data, "Transform", "SwapInvertMask", loaded.swapInvertMask);
        get_value(data, "Transform", "GrayscaleMode", loaded.grayscaleMode);
        if (data.contains("Transform") && data.at("Transform").contains("GrayscaleWeights")) {
            std::vector<float> values = toml::find<std::vector<float>>(data, "Transform", "GrayscaleWeights");
            if (values.size() >= 3) {
                loaded.grayscaleWeights[0] = values[0];
                loaded.grayscaleWeights[1] = values[1];
                loaded.grayscaleWeights[2] = values[2];
            }
        }

        get_value(data, "Export", "DefaultFormat", loaded.defFormat);
        get_value(data, "Export", "FileFormat", loaded.fileFormat);
        get_value(data, "Export", "DefaultBit", loaded.defBDepth);
        get_value(data, "Export", "BitDepth", loaded.bitDepth);

        get_codec_value(data, "Encoding", "TiffCompression", loaded.tiffCompression, tiffCompressionFromString);
        get_value(data, "Encoding", "TiffZipLevel", loaded.tiffZipLevel);
        get_codec_value(data, "Encoding", "OpenEXRCompression", loaded.exrCompression, exrCompressionFromString);
        get_value(data, "Encoding", "OpenEXRZipLevel", loaded.exrZipLevel);
        get_value(data, "Encoding", "OpenEXRDwaLevel", loaded.exrDwaLevel);
        get_codec_value(data, "Encoding", "PngStrategy", loaded.pngStrategy, pngStrategyFromString);
        get_value(data, "Encoding", "PngLevel", loaded.pngCompressionLevel);
        get_value(data, "Encoding", "JpegQuality", loaded.jpegQuality);
        get_codec_value(data, "Encoding", "JpegSubsampling", loaded.jpegSubsampling, jpegSubsamplingFromString);
        get_value(data, "Encoding", "Jpeg2000QStep", loaded.jpeg2000QStep);
        get_value(data, "Encoding", "HeicQuality", loaded.heicQuality);
        get_value(data, "Encoding", "JpegXLQuality", loaded.jpegxlQuality);
        get_value(data, "Encoding", "JpegXLEffort", loaded.jpegxlEffort);
        get_value(data, "Encoding", "JpegXLSpeed", loaded.jpegxlSpeed);

        get_value(data, "CameraRaw", "RawRotation", loaded.rawRot);

        loaded.alphaMode           = std::clamp<uint>(loaded.alphaMode, 0, 2);
        loaded.normMode            = std::clamp<uint>(loaded.normMode, 0, 2);
        loaded.repairMode          = std::clamp<uint>(loaded.repairMode, 0, 6);
        loaded.rangeMode           = std::clamp<uint>(loaded.rangeMode, 0, 3);
        loaded.swapBasis           = std::clamp<uint>(loaded.swapBasis, 0, 5);
        loaded.swapInvertMask      = std::clamp<uint>(loaded.swapInvertMask, 0, 7);
        loaded.grayscaleMode       = std::clamp<uint>(loaded.grayscaleMode, 0, 7);
        loaded.defFormat           = std::clamp(loaded.defFormat, 0, 8);
        loaded.fileFormat          = std::clamp(loaded.fileFormat, -1, 8);
        loaded.defBDepth           = std::clamp(loaded.defBDepth, 0, 6);
        loaded.bitDepth            = std::clamp(loaded.bitDepth, -1, 6);
        loaded.verbosity           = std::clamp<uint>(loaded.verbosity, 0, 5);
        loaded.tiffCompression     = std::clamp(loaded.tiffCompression, static_cast<int>(TiffCompression_Zip),
                                                static_cast<int>(TiffCompression_None));
        loaded.tiffZipLevel        = std::clamp(loaded.tiffZipLevel, 1, 9);
        loaded.exrCompression      = std::clamp(loaded.exrCompression, static_cast<int>(ExrCompression_Zip),
                                                static_cast<int>(ExrCompression_None));
        loaded.exrZipLevel         = std::clamp(loaded.exrZipLevel, 1, 9);
        loaded.exrDwaLevel         = std::clamp(loaded.exrDwaLevel, 1, 100);
        loaded.pngStrategy         = std::clamp(loaded.pngStrategy, static_cast<int>(PngCompression_Default),
                                                static_cast<int>(PngCompression_None));
        loaded.pngCompressionLevel = std::clamp(loaded.pngCompressionLevel, 0, 9);
        loaded.jpegQuality         = std::clamp(loaded.jpegQuality, 1, 100);
        loaded.jpegSubsampling     = std::clamp(loaded.jpegSubsampling, static_cast<int>(JpegSubsampling_444),
                                                static_cast<int>(JpegSubsampling_411));
        loaded.jpeg2000QStep       = std::clamp(loaded.jpeg2000QStep, -1.0f, 10.0f);
        loaded.heicQuality         = std::clamp(loaded.heicQuality, 1, 100);
        loaded.jpegxlQuality       = std::clamp(loaded.jpegxlQuality, 1, 100);
        loaded.jpegxlEffort        = std::clamp(loaded.jpegxlEffort, 1, 9);
        loaded.jpegxlSpeed         = std::clamp(loaded.jpegxlSpeed, 0, 4);

        outSettings = loaded;
        return true;
    } catch (const std::exception& ex) {
        spdlog::error("Error loading settings file: {}", ex.what());
        return false;
    }
}

bool
loadSettingsDefaults(const std::string& filename)
{
    Settings loaded;
    if (!loadSettings(loaded, filename)) {
        return false;
    }

    settingsDefaults = loaded;
    settings         = loaded;
    return true;
}

void
resetSettingsToDefaults()
{
    settings = settingsDefaults;
}

void
resetEncoderSettingsToDefaults()
{
    settings.tiffCompression     = settingsDefaults.tiffCompression;
    settings.tiffZipLevel        = settingsDefaults.tiffZipLevel;
    settings.exrCompression      = settingsDefaults.exrCompression;
    settings.exrZipLevel         = settingsDefaults.exrZipLevel;
    settings.exrDwaLevel         = settingsDefaults.exrDwaLevel;
    settings.pngStrategy         = settingsDefaults.pngStrategy;
    settings.pngCompressionLevel = settingsDefaults.pngCompressionLevel;
    settings.jpegQuality         = settingsDefaults.jpegQuality;
    settings.jpegSubsampling     = settingsDefaults.jpegSubsampling;
    settings.jpeg2000QStep       = settingsDefaults.jpeg2000QStep;
    settings.heicQuality         = settingsDefaults.heicQuality;
    settings.jpegxlQuality       = settingsDefaults.jpegxlQuality;
    settings.jpegxlEffort        = settingsDefaults.jpegxlEffort;
    settings.jpegxlSpeed         = settingsDefaults.jpegxlSpeed;
}

static const char*
formatName(int fileFormat)
{
    switch (fileFormat) {
    case 0: return "TIFF";
    case 1: return "OpenEXR";
    case 2: return "PNG";
    case 3: return "JPEG";
    case 4: return "JPEG-2000";
    case 5: return "HTJ2K";
    case 6: return "HEIC";
    case 7: return "JPEG XL";
    case 8: return "PPM";
    default: return "Same as input";
    }
}

static const char*
bitDepthName(int bitDepth)
{
    switch (bitDepth) {
    case 0: return "uint8";
    case 1: return "uint16";
    case 2: return "uint32";
    case 3: return "uint64";
    case 4: return "16bit half float";
    case 5: return "32bit float";
    case 6: return "64bit double";
    default: return "Same as input";
    }
}

static const char*
tiffCompressionName(int compression)
{
    switch (compression) {
    case TiffCompression_Zip: return "ZIP/Deflate";
    case TiffCompression_Lzw: return "LZW";
    case TiffCompression_PackBits: return "PackBits";
    case TiffCompression_None: return "None";
    default: return "Unknown";
    }
}

static const char*
exrCompressionName(int compression)
{
    switch (compression) {
    case ExrCompression_Zip: return "ZIP";
    case ExrCompression_Zips: return "ZIPS";
    case ExrCompression_Piz: return "PIZ";
    case ExrCompression_Pxr24: return "PXR24";
    case ExrCompression_Rle: return "RLE";
    case ExrCompression_B44: return "B44";
    case ExrCompression_B44A: return "B44A";
    case ExrCompression_Dwaa: return "DWAA";
    case ExrCompression_Dwab: return "DWAB";
    case ExrCompression_Htj2k256: return "HTJ2K256";
    case ExrCompression_Htj2k32: return "HTJ2K32";
    case ExrCompression_None: return "None";
    default: return "Unknown";
    }
}

static const char*
pngStrategyName(int strategy)
{
    switch (strategy) {
    case PngCompression_Default: return "Default";
    case PngCompression_Filtered: return "Filtered";
    case PngCompression_Huffman: return "Huffman";
    case PngCompression_Rle: return "RLE";
    case PngCompression_Fixed: return "Fixed";
    case PngCompression_Fast: return "PNG fast";
    case PngCompression_None: return "None";
    default: return "Unknown";
    }
}

static const char*
jpegSubsamplingName(int subsampling)
{
    switch (subsampling) {
    case JpegSubsampling_444: return "4:4:4";
    case JpegSubsampling_422: return "4:2:2";
    case JpegSubsampling_420: return "4:2:0";
    case JpegSubsampling_411: return "4:1:1";
    default: return "Unknown";
    }
}

void
printSettings(Settings& settings)
{
    spdlog::info("--- Current Settings ---");
    spdlog::info("Solidify: {}", settings.isSolidify ? "Enabled" : "Disabled");
    spdlog::info("Use Alpha: {}", settings.useAlpha ? "Embedded image alpha" : "External mask file");
    spdlog::info("Alpha/Mask: {}", settings.alphaMode == 0
                                       ? "Remove Alpha"
                                       : (settings.alphaMode == 1 ? "Preserve Alpha" : "Export Alpha only"));
    spdlog::info("Parallel Threads: {}", settings.numThreads);
    spdlog::info("Queue Limit: {}", settings.queueLimit);
    spdlog::info("Normalize Mode: {}", settings.normMode);
    spdlog::info("Repair Mode: {}", settings.repairMode);
    spdlog::info("Range Mode: {}", settings.rangeMode);
    spdlog::info("Swap Basis: {}", settings.swapBasis);
    spdlog::info("Swap Invert Mask: {}", settings.swapInvertMask);
    spdlog::info("Grayscale Mode: {}", settings.grayscaleMode);
    spdlog::info("Grayscale Weights: {}, {}, {}", settings.grayscaleWeights[0], settings.grayscaleWeights[1],
                 settings.grayscaleWeights[2]);
    spdlog::info("File Format: {}", formatName(settings.fileFormat));
    spdlog::info("Default File Format: {}", formatName(settings.defFormat));
    spdlog::info("Export Bit Depth: {}", bitDepthName(settings.bitDepth));
    spdlog::info("Default Export Bit Depth: {}", bitDepthName(settings.defBDepth));
    spdlog::info("TIFF Compression: {} level {}", tiffCompressionName(settings.tiffCompression), settings.tiffZipLevel);
    spdlog::info("OpenEXR Compression: {} zip level {} dwa level {}", exrCompressionName(settings.exrCompression),
                 settings.exrZipLevel, settings.exrDwaLevel);
    spdlog::info("PNG Compression: {} level {}", pngStrategyName(settings.pngStrategy), settings.pngCompressionLevel);
    spdlog::info("JPEG Quality: {} subsampling {}", settings.jpegQuality,
                 jpegSubsamplingName(settings.jpegSubsampling));
    spdlog::info("JPEG-2000 Codec: OpenJPEG");
    spdlog::info("HTJ2K QStep: {}", settings.jpeg2000QStep);
    spdlog::info("HEIC Quality: {}", settings.heicQuality);
    spdlog::info("JPEG XL Quality: {} effort {} speed {}", settings.jpegxlQuality, settings.jpegxlEffort,
                 settings.jpegxlSpeed);
    spdlog::info("Raw Rotation: {}", settings.rawRot);
    spdlog::info("Verbosity: {}", settings.verbosity);
    spdlog::info("------------------------");
}
