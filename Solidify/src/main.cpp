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
#include <Windows.h>
#include <QtCore/QResource>

Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)

int main(int argc, char* argv[]) {
    // Allocate console and redirect std output
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);

    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("%^[%l]%$<%t> %v");

    time_t timestamp;
    time(&timestamp);
    std::cout << std::fixed << std::setprecision(8);

    spdlog::info("Solidify {}.{}.{}", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    spdlog::info("Build from: {} {}", __DATE__, __TIME__);
    spdlog::info("Log started at: {}", ctime(&timestamp), "%Y-%m-%d %H:%M:%S");

    if (!loadSettings(settings, "sldf_config.toml")) {
        spdlog::error("Can not load [app_config.toml] Using default settings.");
        settings.reSettings();
    }

    ShowWindow(GetConsoleWindow(), (settings.conEnable) ? SW_SHOW : SW_HIDE);
    printSettings(settings);


    QApplication app(argc, argv);
    Q_INIT_RESOURCE(gui);
    app.setWindowIcon(QIcon(":/MainWindow/sldf.ico"));

    MainWindow window;
    window.show();

    //DropArea dropArea;
    //dropArea.show();

    return app.exec();
}