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

int main(int argc, char* argv[]) {
    // create console
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    QApplication app(argc, argv);

    MainWindow window;
    window.show();

    //DropArea dropArea;
    //dropArea.show();

    return app.exec();
}