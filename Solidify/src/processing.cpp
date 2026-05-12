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

#include "processing.h"

#include "imageio.h"
#include "solidify.h"
#include "threadpool.h"

namespace fs = std::filesystem;

std::string
toLower(const std::string& str)
{
    std::string strCopy = str;
    std::transform(strCopy.begin(), strCopy.end(), strCopy.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return strCopy;
}

std::string
checkAlpha(const std::vector<std::string>& fileNames)
{
    if (settings.useAlpha) {
        return "";
    }

    for (const std::string& ss : settings.mask_substr) {
        const std::string lowMask = toLower(ss);
        for (const std::string& fileName : fileNames) {
            if (toLower(fileName).find(lowMask) != std::string::npos) {
                return fileName;
            }
        }
    }
    return "";
}

void
getWritableExt(std::string* ext, Settings* settings)
{
    std::unique_ptr<ImageOutput> probe;
    std::string fn = "probename" + *ext;
    probe          = ImageOutput::create(fn);
    if (probe) {
        spdlog::info("{} is writable", *ext);
    } else {
        spdlog::info("{} is readonly", *ext);
        spdlog::info("Output format changed to {}", settings->out_formats[settings->defFormat]);
        *ext = "." + settings->out_formats[settings->defFormat];
    }
    probe.reset();
}

std::string
getExtension(const std::string& fileName, Settings* settings)
{
    fs::path path(fileName);
    std::string extension = path.extension().string();
    extension             = toLower(extension);

    switch (settings->fileFormat) {
    case 0: return ".tif";
    case 1: return ".exr";
    case 2: return ".png";
    case 3: return ".jpg";
    case 4: return ".jp2";
    case 5: return ".jph";
    case 6: return ".heic";
    case 7: return ".jxl";
    case 8: return ".ppm";
    default: break;
    }

    if (extension.empty()) {
        extension = "." + settings->out_formats[settings->defFormat];
    }
    getWritableExt(&extension, settings);
    return extension;
}

std::string
getOutName(const std::string& fileName, Settings* settings)
{
    fs::path inPath(fileName);
    std::string baseName = inPath.stem().string();
    std::string proc_sfx;

    if (settings->isSolidify) {
        proc_sfx = "_fill";
    } else if (settings->normMode > 0) {
        if (settings->normMode == 2) {
            proc_sfx = "_norm";
        } else {
            const std::string lowName = toLower(baseName);
            for (const std::string& name : settings->normNames) {
                if (lowName.find(name) != std::string::npos) {
                    proc_sfx = "_norm";
                    break;
                }
            }
        }
    } else if (settings->repairMode > 0) {
        proc_sfx = "_rep";
    } else if (settings->grayscaleMode > 0) {
        proc_sfx = "_gray";
    } else if (settings->swapBasis != 0 || settings->swapInvertMask != 0) {
        proc_sfx = "_swap";
    } else if (settings->alphaMode == 2) {
        proc_sfx = "_mask";
    } else {
        proc_sfx = "_conv";
    }

    if (proc_sfx.empty()) {
        proc_sfx = "_conv";
    }

    fs::path outPath = inPath.parent_path() / (baseName + proc_sfx + getExtension(fileName, settings));
    return outPath.string();
}

static void
collectInputFiles(const std::vector<std::string>& inputs, std::vector<std::string>* fileNames)
{
    for (const std::string& input : inputs) {
        if (input.empty()) {
            continue;
        }

        std::error_code ec;
        fs::path p(input);
        if (fs::is_directory(p, ec)) {
            for (const fs::directory_entry& entry :
                 fs::recursive_directory_iterator(p, fs::directory_options::skip_permission_denied, ec)) {
                if (ec) {
                    spdlog::warn("Directory traversal warning for {}: {}", p.string(), ec.message());
                    ec.clear();
                    continue;
                }
                if (entry.is_regular_file(ec)) {
                    fileNames->push_back(entry.path().string());
                }
            }
        } else if (fs::is_regular_file(p, ec)) {
            fileNames->push_back(p.string());
        } else {
            spdlog::warn("Skipping non-file input: {}", input);
        }
    }
}

static std::string
fileNameOnly(const std::string& path)
{
    return fs::path(path).filename().string();
}

bool
doProcessing(const std::vector<std::string>& filePaths, SolidifyProgressCallback progressCallback)
{
    std::vector<std::string> fileNames;
    VTimer f_timer;

    collectInputFiles(filePaths, &fileNames);

    if (fileNames.empty()) {
        if (progressCallback) {
            progressCallback(0.0f, "No files found.");
        }
        spdlog::error("No files found.");
        return false;
    }

    std::string mask_file = checkAlpha(fileNames);

    std::pair<ImageBuf, ImageBuf> mask_pair;
    if (!mask_file.empty()) {
        spdlog::info("Mask file: {} will be used.", mask_file);
        mask_pair = mask_load(mask_file, progressCallback);
        if (!mask_pair.first.initialized() || !mask_pair.second.initialized()) {
            spdlog::error("Mask load failed: {}", mask_file);
            return false;
        }
    } else if (settings.useAlpha) {
        spdlog::info("Use Alpha enabled. External mask file discovery skipped.");
    }

    std::vector<std::string> processFiles;
    processFiles.reserve(fileNames.size());
    for (const std::string& fileName : fileNames) {
        if (fileName != mask_file) {
            processFiles.push_back(fileName);
        }
    }

    if (processFiles.empty()) {
        if (progressCallback) {
            progressCallback(0.0f, "No source files found after excluding mask files.");
        }
        return false;
    }

    const unsigned int hardwareThreads = std::max(1u, std::thread::hardware_concurrency());
    const size_t threadCount           = settings.numThreads > 0 ? settings.numThreads : hardwareThreads;
    const size_t queueLimit            = settings.queueLimit > 0 ? settings.queueLimit : threadCount;

    ThreadPool pool(threadCount, queueLimit);
    std::vector<std::future<bool>> results;
    results.reserve(processFiles.size());

    std::vector<std::atomic<float>> fileProgress(processFiles.size());
    for (std::atomic<float>& value : fileProgress) {
        value.store(0.0f);
    }
    std::mutex callbackMutex;

    auto updateProgress = [&](size_t fileIndex, float filePortion, std::string status) {
        fileProgress[fileIndex].store(std::clamp(filePortion, 0.0f, 1.0f), std::memory_order_relaxed);
        if (!progressCallback) {
            return;
        }

        float total = 0.0f;
        for (const std::atomic<float>& value : fileProgress) {
            total += value.load(std::memory_order_relaxed);
        }
        total /= static_cast<float>(fileProgress.size());

        std::lock_guard<std::mutex> lock(callbackMutex);
        progressCallback(total, std::move(status));
    };

    spdlog::info("Processing {} files with {} threads and queue limit {}.", processFiles.size(), threadCount,
                 queueLimit);

    for (size_t i = 0; i < processFiles.size(); ++i) {
        const std::string infile  = processFiles[i];
        const std::string outfile = getOutName(infile, &settings);

        results.emplace_back(pool.enqueue([&, i, infile, outfile]() mutable {
            const std::string debugText = "Source: " + fileNameOnly(infile) + "\nTarget: " + fileNameOnly(outfile)
                                          + "\nMask:   " + fileNameOnly(mask_file);
            updateProgress(i, 0.0f, debugText);

            SolidifyProgressCallback fileCallback = [&](float p, std::string s) {
                std::string status = s.empty() ? debugText : std::move(s);
                updateProgress(i, p, std::move(status));
            };

            const bool ok = solidify_main(infile, outfile, mask_pair, fileCallback);
            updateProgress(i, 1.0f, ok ? ("Done: " + fileNameOnly(outfile)) : ("Failed: " + fileNameOnly(infile)));
            return ok;
        }));
    }

    pool.waitForAllTasks();

    bool allOk = true;
    for (std::future<bool>& result : results) {
        try {
            allOk = result.get() && allOk;
        } catch (const std::exception& ex) {
            spdlog::error("Processing task failed: {}", ex.what());
            allOk = false;
        }
    }

    if (progressCallback) {
        progressCallback(allOk ? 1.0f : 0.0f, allOk ? "Everything Done!" : "Finished with errors.");
    }

    spdlog::info("Total processing time : {} for {} files.", f_timer.nowText(), processFiles.size());
    return allOk;
}
