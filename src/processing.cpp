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
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDropEvent>
#include <QtCore/QMimeData>
#include <QtCore/QDebug>
#include <QtCore/QFileInfo>

#include "solidify.h"

std::vector<QString> fileNames; // This will hold the names of all files

bool doProcessing(QList<QUrl> urls) {
    for (const QUrl& url : urls) {
        QString fileName = url.toLocalFile();
        if (!fileName.isEmpty()) {
            fileNames.push_back(fileName);
        }
    }

    for (int i = 0; i < fileNames.size(); i++) {
        //qDebug() << "File name: " << fileName;
        QFileInfo fileInfo(fileNames[i]);
        QString baseName = fileInfo.baseName();
        QString path = fileInfo.absolutePath();
        QString outName = path + "/" + baseName + "_fill." + fileInfo.completeSuffix();

        // Call the solidify_main function
        if (solidify_main(fileNames[i].toStdString(), outName.toStdString())) {
            exit(-1);
        };
    }

    return true;
}