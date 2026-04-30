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

#include "settings.h"

Settings settings;
Settings settingsDefaults;

template<typename T>
static void get_value(const toml::value& data, const std::string& section, const std::string& key, T& var)
{
    if (data.contains(section) && data.at(section).contains(key)) {
        var = toml::find<T>(data, section, key);
    }
}

bool loadSettings(Settings& outSettings, const std::string& filename)
{
    try {
        const auto data = toml::parse(filename);
        Settings loaded;

        get_value(data, "Global", "Solidify", loaded.isSolidify);
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
            std::vector<std::string> values =
                toml::find<std::vector<std::string>>(data, "Normalize", "NormalsNames");
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

        get_value(data, "CameraRaw", "RawRotation", loaded.rawRot);

        loaded.alphaMode  = std::clamp<uint>(loaded.alphaMode, 0, 2);
        loaded.normMode   = std::clamp<uint>(loaded.normMode, 0, 2);
        loaded.repairMode = std::clamp<uint>(loaded.repairMode, 0, 6);
        loaded.rangeMode  = std::clamp<uint>(loaded.rangeMode, 0, 3);
        loaded.swapBasis = std::clamp<uint>(loaded.swapBasis, 0, 5);
        loaded.swapInvertMask = std::clamp<uint>(loaded.swapInvertMask, 0, 7);
        loaded.grayscaleMode = std::clamp<uint>(loaded.grayscaleMode, 0, 7);
        loaded.defFormat  = std::clamp(loaded.defFormat, 0, 7);
        loaded.fileFormat = std::clamp(loaded.fileFormat, -1, 7);
        loaded.defBDepth  = std::clamp(loaded.defBDepth, 0, 6);
        loaded.bitDepth   = std::clamp(loaded.bitDepth, -1, 6);
        loaded.verbosity  = std::clamp<uint>(loaded.verbosity, 0, 5);

        outSettings = loaded;
        return true;
    } catch (const std::exception& ex) {
        spdlog::error("Error loading settings file: {}", ex.what());
        return false;
    }
}

bool loadSettingsDefaults(const std::string& filename)
{
    Settings loaded;
    if (!loadSettings(loaded, filename)) {
        return false;
    }

    settingsDefaults = loaded;
    settings         = loaded;
    return true;
}

void resetSettingsToDefaults()
{
    settings = settingsDefaults;
}

static const char* formatName(int fileFormat)
{
    switch (fileFormat) {
    case 0: return "TIFF";
    case 1: return "OpenEXR";
    case 2: return "PNG";
    case 3: return "JPEG";
    case 4: return "JPEG-2000";
    case 5: return "HEIC";
    case 6: return "JPEG XL";
    case 7: return "PPM";
    default: return "Same as input";
    }
}

static const char* bitDepthName(int bitDepth)
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

void printSettings(Settings& settings)
{
    spdlog::info("--- Current Settings ---");
    spdlog::info("Solidify: {}", settings.isSolidify ? "Enabled" : "Disabled");
    spdlog::info("Alpha/Mask: {}", settings.alphaMode == 0 ? "Remove Alpha" :
                                      (settings.alphaMode == 1 ? "Preserve Alpha" : "Export Alpha only"));
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
    spdlog::info("Raw Rotation: {}", settings.rawRot);
    spdlog::info("Verbosity: {}", settings.verbosity);
    spdlog::info("------------------------");
}
