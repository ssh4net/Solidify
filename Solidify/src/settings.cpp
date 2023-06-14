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

#include <toml.hpp>
#include "settings.h"
#include "Log.h"

Settings settings;

bool loadSettings(Settings& settings, const std::string& filename) {
    try {
        auto parsed = toml::parse(filename);

        if (!parsed.contains("Global") || parsed["Global"].as_table().empty()) {
            LOG(error) << "Error parsing settings file: [Global] section not found or empty." << std::endl;
            return false;
        }
        if (!parsed.contains("Normalize") || parsed["Normalize"].as_table().empty()) {
            LOG(error) << "Error parsing settings file: [Normalize] section not found or empty." << std::endl;
            return false;
        }
        if (!parsed.contains("Range") || parsed["Range"].as_table().empty()) {
            LOG(error) << "Error parsing settings file: [Range] section not found or empty." << std::endl;
            return false;
        }
        if (!parsed.contains("Transform") || parsed["Transform"].as_table().empty()) {
            LOG(error) << "Error parsing settings file: [Transform] section not found or empty." << std::endl;
            return false;
        }
        if (!parsed.contains("Export") || parsed["Export"].as_table().empty()) {
            LOG(error) << "Error parsing settings file: [Export] section not found or empty." << std::endl;
            return false;
        }
        if (!parsed.contains("CameraRaw") || parsed["CameraRaw"].as_table().empty()) {
            LOG(error) << "Error parsing settings file: [CameraRaw] section not found or empty." << std::endl;
            return false;
        }

        auto check = [&parsed](const std::string& section, const std::string& key) {
            auto ts = parsed[section].as_table().find(key);
            if (ts == parsed[section].as_table().end()) {
                LOG(error) << "Warning: [" << section.c_str() << "] section does not contain \"" << key.c_str() << "\" key." << std::endl;
                return false;
            }
            return true;
        };
        // Global
        if (!check("Global", "Solidify")) return false;
        settings.isSolidify = parsed["Global"]["Solidify"].as_boolean();

        if (!check("Global", "ExportAlpha")) return false;
        settings.expAlpha = parsed["Global"]["ExportAlpha"].as_boolean();

        settings.mask_substr.clear();
        auto it = parsed["Global"]["MaskNames"].as_array();
        auto begin = it.begin();
        for (int i = 0; i < it.size(); i++) {
            std::string t = begin[i].as_string();
            if (t.empty()) continue;
            settings.mask_substr.push_back(t);
        }

        if (!check("Global", "Console")) return false;
        settings.conEnable = parsed["Global"]["Console"].as_boolean();

        if (!check("Global", "Threads")) return false;
        settings.numThreads = parsed["Global"]["Threads"].as_integer();

        // Normals
        if (!check("Normalize", "NormalizeMode")) return false;
        settings.normMode = parsed["Normalize"]["NormalizeMode"].as_integer();
        if (settings.normMode < 0 || settings.normMode > 2) {
            LOG(error) << "Error parsing settings file: [Normalize] section: \"NormalizeMode\" key value is out of range." << std::endl;
            return false;
        }

        settings.normNames.clear();
        it = parsed["Normalize"]["NormalsNames"].as_array();
        begin = it.begin();
        for (int i = 0; i < it.size(); i++) {
            std::string t = begin[i].as_string();
            if (t.empty()) continue;
            settings.normNames.push_back(t);
        }
        // Range
        if (!check("Range", "RangeMode")) return false;
        settings.rangeMode = parsed["Range"]["RangeMode"].as_integer();
        if (settings.rangeMode < 0 || settings.rangeMode > 3) {
            LOG(error) << "Error parsing settings file: [Range] section: \"RangeMode\" key value is out of range." << std::endl;
            return false;
        }
        // Transform


        // Export
        if (!check("Export", "DefaultFormat")) return false;
        settings.defFormat = parsed["Export"]["DefaultFormat"].as_integer();
        if (settings.defFormat < 0 || settings.defFormat > 5) {
            LOG(error) << "Error parsing settings file: [Export] section: \"DefaultFormat\" key value is out of range." << std::endl;
            return false;
        }
        if (!check("Export", "FileFormat")) return false;
        settings.fileFormat = parsed["Export"]["FileFormat"].as_integer();
        if (settings.fileFormat < -1 || settings.fileFormat > 5) {
            LOG(error) << "Error parsing settings file: [Export] section: \"FileFormat\" key value is out of range." << std::endl;
            return false;
        }
        if (!check("Export", "DefaultBit")) return false;
        settings.defBDepth = parsed["Export"]["DefaultBit"].as_integer();
        if (settings.defBDepth < 0 || settings.defBDepth > 6) {
            LOG(error) << "Error parsing settings file: [Export] section: \"DefaultBit\" key value is out of range." << std::endl;
            return false;
        }
        if (!check("Export", "BitDepth")) return false;
        settings.bitDepth = parsed["Export"]["BitDepth"].as_integer();
        if (settings.bitDepth < -1 || settings.bitDepth > 6) {
            LOG(error) << "Error parsing settings file: [Export] section: \"BitDepth\" key value is out of range." << std::endl;
            return false;
        }
        // CameraRaw
        if (!check("CameraRaw", "RawRotation")) return false;
        settings.rawRot = parsed["CameraRaw"]["RawRotation"].as_integer();
        if (settings.rawRot < -1 || settings.rawRot > 6) {
            LOG(error) << "Error parsing settings file: [CameraRaw] section: \"RawRotation\" key value is out of range." << std::endl;
            return false;
        }

        return true;
    }
    catch (const toml::syntax_error& err) {
        LOG(error) << "Error parsing settings file: " << err.what();
        return false;
    }
    catch (const toml::type_error& err) {
        LOG(error) << "Type Error: " << err.what();
        return false;
    }
    catch (const std::exception& ex) {
        LOG(error) << "Error loading settings file: " << ex.what();
        return false;
    }
}

void printSettings(Settings& settings) {
    QString mode;
    qDebug() << "Solidify: " << (settings.isSolidify ? "Enabled" : "Disabled");
    qDebug() << "Export with Alpha/Mask: " << (settings.expAlpha ? "Enabled" : "Disabled");
    qDebug() << "Parallel Threads: " << settings.numThreads;
    qDebug() << "Normalize Mode: " << (settings.normMode == 0 ? "Disabled" : (settings.normMode == 1 ? "Smart" : "Force"));
    switch (settings.rangeMode)
    {
    case 0:
        mode = "Unsigned";
        break;
    case 1:
		mode = "Signed";
		break;
    case 2:
        mode = "Signed -> Unsigned";
        break;
    case 3:
		mode = "Unsigned -> Signed";
		break;
    }
    qDebug() << qPrintable(QString("Range Mode: %1").arg(mode));

    auto getMode = [](int fileFormat) {
        switch (fileFormat)
        {
        case 0:
            return QString("TIFF");
        case 1:
            return QString("OpenEXR");
        case 2:
            return QString("PNG");
        case 3:
            return QString("JPEG");
        case 4:
            return QString("JPEG-2000");
        case 5:
            return QString("PPM");
        default:
            return QString("Same as input");
        }
    };
    qDebug() << qPrintable(QString("File Format: %1").arg(getMode(settings.fileFormat)));
    qDebug() << qPrintable(QString("Default File Format: %1").arg(getMode(settings.defFormat)));

    auto getBitDepth = [](int bitDepth) {
		switch (bitDepth)
		{
		case 0:
			return QString("uint8");
		case 1:
			return QString("uint16");
		case 2:
			return QString("uint32");
		case 3:
			return QString("uint64");
		case 4:
			return QString("16bit (half) float");
		case 5:
			return QString("32bit float");
		case 6:
			return QString("64bit (double) float");
		default:
            return QString("Same as input");
		}
	};
    qDebug() << qPrintable(QString("Export Bit Depth: %1").arg(getBitDepth(settings.bitDepth)));
    qDebug() << qPrintable(QString("Default Export Bit Depth: %1").arg(getBitDepth(settings.defBDepth)));
}
