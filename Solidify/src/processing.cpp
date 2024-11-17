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

//#include "ui.h"
#include "solidify.h"
#include "stdafx.h"
#include "imageio.h"
#include "processing.h"
#include "settings.h"
#include "Log.h"

#include <algorithm>
#include <cctype>
#include <string>

#include <thread>
#include <mutex>
#include <condition_variable>
#include "threadpool.h"

std::string toLower(const std::string& str) {
    std::string strCopy = str;
    std::transform(strCopy.begin(), strCopy.end(), strCopy.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return strCopy;
}

QString checkAlpha(std::vector<QString> fileNames) {
    std::vector<QString>::iterator it;

    for (std::string ss : settings.mask_substr) {
		it = std::find_if(fileNames.begin(), fileNames.end(), [&ss](const QString& str) { 
            return str.contains(QString::fromStdString(ss), Qt::CaseInsensitive); });
        if (it != fileNames.end()) {
			return *it;
		}
	}
    // If we reach this point, we did not find a match.
    return "";
}

void getWritableExt(QString* ext, Settings* settings) {
    std::unique_ptr<ImageOutput> probe;
    QString fn = "probename." + *ext;
    probe = ImageOutput::create(fn.toStdString());
    if (probe) {
        LOG(info) << ext->toStdString() << " is writable" << std::endl;
    }
    else {
        LOG(info) << ext->toStdString() << " is readonly" << std::endl;
        LOG(info) << "Output format changed to " << settings->out_formats[settings->defFormat] << std::endl;
        *ext = "." + QString::fromStdString(settings->out_formats[settings->defFormat]);
    }
    probe.reset();
}

QString getExtension(const QString& fileName, Settings* settings) {
    QFileInfo fileInfo(fileName);
	QString extension = "." + fileInfo.completeSuffix();
    extension = extension.toLower();
    switch (settings->fileFormat) {
    //-1 - original, 0 - TIFF, 1 - OpenEXR, 2 - PNG, 3 - JPEG, 4 - JPEG-2000, 5 - HEIC,6 - PPM
    case 0:
        return ".tif";
    case 1:
        return ".exr";
    case 2:
        return ".png";
    case 3:
        return ".jpg";
    case 4:
        return ".jp2";
    case 5:
        return ".heic";
    case 6:
        return ".ppm";
    }
    getWritableExt(&extension, settings);

    return extension;
}

QString getOutName(const QString& fileName, Settings* settings) {
	QFileInfo fileInfo(fileName);
	QString baseName = fileInfo.baseName();
	QString path = fileInfo.absolutePath();
    QString proc_sfx = "";
    if (settings->isSolidify) {
		proc_sfx += "_fill";
	}
    else if (settings->normMode > 0) 
    {
        if (settings->normMode == 2) {
			proc_sfx += "_norm";
		}
        else {
            // check if filename have any of settings.normNames as substring, case insensitive
            std::string lowName = toLower(baseName.toStdString());

            for (auto& name : settings->normNames) {
                if (lowName.find(name, 0) != std::string::npos) {
                    proc_sfx += "_norm";
                    break;
                }
            }
        }
    }
    else if (settings->repairMode > 0)
    {
        if (settings->repairMode == 2) {
            proc_sfx += "_rep";
		}
		else {
			// check if filename have any of settings.normNames as substring, case insensitive
			std::string lowName = toLower(baseName.toStdString());

            for (auto& name : settings->normNames) {
                if (lowName.find(name, 0) != std::string::npos) {
					proc_sfx += "_rep";
					break;
				}
			}
        }
    }
    else if (settings->alphaMode == 2) {
        proc_sfx += "_mask";
	}
    else {
        proc_sfx += "_conv";
    }
	QString outName = path + "/" + baseName + proc_sfx + getExtension(fileName, settings);
	return outName;
}

bool doProcessing(QList<QUrl> urls, QProgressBar* progressBar, MainWindow* mainWindow) {
    std::vector<QString> fileNames; // This will hold the names of all files
    VTimer f_timer;

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
        mask_pair = mask_load(mask_file.toStdString(), mainWindow);
	}

    std::vector< std::future<bool> > results;
    // Create a pool with 5 threads
    ThreadPool pool(settings.numThreads > 0 ? settings.numThreads : 1);
    //for (auto& thr_file : fileNames) {
    for (int i = 0; i < fileNames.size(); i++) {
        //qDebug() << "File name: " << fileName;
        if (fileNames[i] == mask_file) {
			continue;
		}

        QString outName = getOutName(fileNames[i], &settings);
        //QFileInfo fileInfo(fileNames[i]);
        //QString baseName = fileInfo.baseName();
        //QString path = fileInfo.absolutePath();
        //QString outName = path + "/" + baseName + "_fill." + fileInfo.completeSuffix();
        
        std::string infile = fileNames[i].toStdString();
        std::string outfile = outName.toStdString();

        QString DebugText = "Source: " + QFileInfo(fileNames[i]).fileName() +
                          "\nTarget: " + QFileInfo(outName).fileName() +
                          "\nMask:   " + QFileInfo(mask_file).fileName();

        mainWindow->emitUpdateTextSignal(DebugText);


        results.emplace_back(pool.enqueue(solidify_main, infile, outfile, mask_pair, progressBar, mainWindow));

        // Call the solidify_main function
        //bool result = solidify_main(infile, outfile, mask_pair, progressBar, mainWindow);
        //bool result_ok = result.get();

        //if (!result_ok) {
         //   system("pause");
         //   exit(-1);
        //};
    }
    for (auto&& result : results) {
        result.get();
    }

    mainWindow->emitUpdateTextSignal("Everything Done!");
    LOG(info) << "Total processing time : " << f_timer.nowText() << " for " << fileNames.size() << " files." << std::endl;
    return true;
}