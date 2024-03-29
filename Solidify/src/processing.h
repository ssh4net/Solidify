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

#pragma once
#include "ui.h"
#include "settings.h"

class MainWindow; // forward declaration
struct Settings;  // forward declaration

std::string toLower(const std::string& str);
void getWritableExt(QString* ext, Settings* settings);
QString getExtension(const QString& fileName, Settings* settings);
QString getOutName(const QString& fileName, Settings* settings);

bool doProcessing(QList<QUrl> URLs, QProgressBar* progressBar, MainWindow* mainWindow);

QString checkAlpha(std::vector<QString> fileNames);