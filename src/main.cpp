/*
 * Solidify (Push Pull) algorithm implementation using OpenImageIO
 * Copyright (c) 2022 Erium Vladlen.
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

#include "ui.h"
#include "stdafx.h"
#include <toml++/toml.h>
#include "settings.h"

bool loadSettings(Settings& settings, const std::string& filename) {
    try {
        toml::table parsed = toml::parse_file(filename);
        
        if (!parsed.contains("Global") || parsed["Global"].as_table()->empty()) {
			qDebug() << qPrintable(QString("Error parsing settings file: [Global] section not found or empty."));
			return false;
		}
        if (!parsed.contains("Normalize") || parsed["Normalize"].as_table()->empty()) {
            qDebug() << qPrintable(QString("Error parsing settings file: [Normalize] section not found or empty."));
            return false;
        }
        if (!parsed.contains("Range") || parsed["Range"].as_table()->empty()) {
			qDebug() << qPrintable(QString("Error parsing settings file: [Range] section not found or empty."));
			return false;
		}
        if (!parsed.contains("Transform") || parsed["Transform"].as_table()->empty()) {
            qDebug() << qPrintable(QString("Error parsing settings file: [Transform] section not found or empty."));
			return false;
        }
        if (!parsed.contains("Export") || parsed["Export"].as_table()->empty()) {
            qDebug() << qPrintable(QString("Error parsing settings file: [Export] section not found or empty."));
			return false;
        }
        if (!parsed.contains("CameraRaw") || parsed["CameraRaw"].as_table()->empty()) {
            qDebug() << qPrintable(QString("Error parsing settings file: [CameraRaw] section not found or empty."));
			return false;
		}

        auto check = [&parsed](const std::string& section, const std::string& key) {
			auto ts = parsed[section].as_table()->find(key);
            if (ts == parsed[section].as_table()->end()) {
				qDebug() << qPrintable(QString("Warning: [%1] section does not contain \"%2\" key.").arg(section.c_str()).arg(key.c_str()));
				return false;
			}
			return true;
		};
// Global
        if (!check("Global", "Solidify")) return false;
        settings.isSolidify = parsed["Global"]["Solidify"].value_or(true);

        if (!check("Global", "ExportAlpha")) return false;
        settings.expAlpha = parsed["Global"]["ExportAlpha"].value_or(false);

        settings.mask_substr.clear();
        auto it = parsed["Global"]["MaskNames"].as_array();
        auto begin = it->begin();
        for (int i = 0; i < it->size(); i++) {
            std::string t = begin[i].value_or("");
            if (t.empty()) continue;
            settings.mask_substr.push_back(t);
        }

        if (!check("Global", "Console")) return false;
        settings.conEnable = parsed["Global"]["Console"].value_or(false);
// Normals
        if (!check("Normalize", "NormalizeMode")) return false;
        settings.normMode = parsed["Normalize"]["NormalizeMode"].value_or(1);

        settings.normNames.clear();
        it = parsed["Normalize"]["NormalsNames"].as_array();
        begin = it->begin();
        for (int i = 0; i < it->size(); i++) {
            std::string t = begin[i].value_or("");
            if (t.empty()) continue;
            settings.normNames.push_back(t);
		}
// Range
        if (!check("Range", "RangeMode")) return false;
        settings.rangeMode = parsed["Range"]["RangeMode"].value_or(0);
// Transform


// Export
        if (!check("Export", "DefaultFormat")) return false;
        settings.defFormat = parsed["Export"]["DefaultFormat"].value_or(0);
        if (!check("Export", "FileFormat")) return false;
        settings.fileFormat = parsed["Export"]["FileFormat"].value_or(-1);
        if (!check("Export", "DefaultBit")) return false;
        settings.defBDepth = parsed["Export"]["DefaultBit"].value_or(1);
        if (!check("Export", "BitDepth")) return false;
        settings.bitDepth = parsed["Export"]["BitDepth"].value_or(-1);
// CameraRaw
        if (!check("CameraRaw", "RawRotation")) return false;
        settings.rawRot = parsed["CameraRaw"]["RawRotation"].value_or(-1);

        return true;
    }
    catch (const toml::parse_error& err) {
        qDebug() << qPrintable(QString("Error parsing settings file: ")) << err.what();
        return false;
    }
    catch (const std::exception& ex) {
        qDebug() << qPrintable(QString("Error loading settings file: ")) << ex.what();
        return false;
    }
}

int main(int argc, char* argv[]) {
    // Allocate console and redirect std output
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);

    if (!loadSettings(settings, "app_config.toml")) {
        qDebug() << qPrintable(QString("Can not load [app_config.toml] Using default settings."));
        settings.reSettings();
    }

    ShowWindow(GetConsoleWindow(), (settings.conEnable) ? SW_SHOW : SW_HIDE);
    qDebug() << qPrintable(QString("Solidify %1.%2").arg(VERSION_MAJOR).arg(VERSION_MINOR)) << "Debug output:";

    QApplication app(argc, argv);

    MainWindow window;
    window.show();

    //DropArea dropArea;
    //dropArea.show();

    return app.exec();
}