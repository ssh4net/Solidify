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
#include "solidify.h"

QString checkAlpha(std::vector<QString> fileNames) {
    std::vector<QString>::iterator it;

    // Find "_mask."
    it = std::find_if(fileNames.begin(), fileNames.end(), [](const QString& str) {
        return str.contains("_mask.", Qt::CaseInsensitive);
        });
    if (it != fileNames.end()) {
        //qDebug() << "Found _mask. in: " << it->toStdString().c_str();
        return *it;
    }

    // Find "_alpha."
    it = std::find_if(fileNames.begin(), fileNames.end(), [](const QString& str) {
        return str.contains("_alpha.", Qt::CaseInsensitive);
        });
    if (it != fileNames.end()) {
        //qDebug() << "Found _alpha. in: " << it->toStdString().c_str();
        return *it;
    }

    // If we reach this point, we did not find a match.
    return "";
}

bool doProcessing(QList<QUrl> urls) {
    std::vector<QString> fileNames; // This will hold the names of all files

    for (const QUrl& url : urls) {
        QString fileName = url.toLocalFile();
        if (!fileName.isEmpty()) {
            fileNames.push_back(fileName);
        }
    }

    // Check if there is an alpha channel files
    QString mask_file = checkAlpha(fileNames);

    std::pair<ImageBuf, ImageBuf> mask_pair;

    if (mask_file != "") {
        qDebug() << "Mask file: " << mask_file << " will be used.";
        mask_pair = mask_load(mask_file.toStdString());
	}

    for (int i = 0; i < fileNames.size(); i++) {
        //qDebug() << "File name: " << fileName;
        if (fileNames[i] == mask_file) {
			continue;
		}

        QFileInfo fileInfo(fileNames[i]);
        QString baseName = fileInfo.baseName();
        QString path = fileInfo.absolutePath();
        QString outName = path + "/" + baseName + "_fill." + fileInfo.completeSuffix();

        // Call the solidify_main function
        if (!solidify_main(fileNames[i].toStdString(), outName.toStdString(), mask_pair)) {
            system("pause");
            exit(-1);
        };
    }

    return true;
}