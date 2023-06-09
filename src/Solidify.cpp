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

#include <iostream>
#include <iomanip>
#include <string>
#include <math.h>

#include "Timer.h"

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include "solidify.h"
#include "imageio.h"
#include "settings.h"
#include "processing.h"

using namespace OIIO;

bool solidify_main(const std::string& inputFileName, const std::string& outputFileName, std::pair<ImageBuf, ImageBuf> mask_pair, 
    QProgressBar* progressBar, MainWindow* mainWindow) {
    Timer g_timer;
    // Create an ImageBuf object for the input file
    ImageSpec config;
    config["raw:user_flip"] = settings.rawRot;

    ImageBuf input_buf(inputFileName, 0, 0, nullptr, &config, nullptr);

    //ImageBuf::ImageBuf(config, input_buf);

    bool external_alpha = false;
    bool grayscale = false;

    if (mask_pair.first.initialized() && mask_pair.second.initialized()) {
		external_alpha = true;
	}

    // Read the image with a progress callback

    std::cout << "Reading " << inputFileName << std::endl;
    bool load_ok = img_load(input_buf, inputFileName, external_alpha, progressBar, mainWindow);
    if (!load_ok) {
		std::cerr << "Error reading " << inputFileName << std::endl;
        mainWindow->emitUpdateTextSignal("Error! Check console for details");
		return false;
	}

    // Get the image's spec
    const ImageSpec& ispec = input_buf.spec();

    // Get the format (bit depth and type)
    TypeDesc format = ispec.format;

    format2console(format);

    // get the image size
    int width = input_buf.spec().width;
    int height = input_buf.spec().height;
    std::cout << "Image size: " << width << "x" << height << std::endl;
    std::cout << "Channels: " << input_buf.nchannels() << " Alpha channel: " << input_buf.spec().alpha_channel << std::endl;

    bool isValid = true;
    int inputCh = input_buf.nchannels();
    //
    // Valid with and without external alpha:
    // RGBA - 4 channels
    // Grayscale with alpha - 2 channels
    // RGB with alpha - 4 channels
    // 
    // Valid only with external alpha:
    // Grayscale - 1 channel
    // RGB - 3 channels 
    //
    switch (inputCh)
    {
    case 1:
        isValid = external_alpha ? true : false;
        grayscale = true;
        break;
    case 2:
        isValid = true;
        input_buf.specmod().channelnames[0] = "Y";
        input_buf.specmod().channelnames[0] = "A";
        input_buf.specmod().alpha_channel = 1;
        grayscale = true;
        break;
    case 3:
        isValid = external_alpha ? true : false;
        grayscale = false;
		break;
    case 4:
        isValid = true;
        input_buf.specmod().channelnames[3] = "A";
        input_buf.specmod().alpha_channel = 3;
        grayscale = false;
		break;
    default:
        isValid = false;
        std::cerr << "Error: Only Grayscale, RGB and RGBA images are supported" << std::endl;
        std::cerr << "Grayscale and RGB images must have an external alpha channel or use external alpha file" << std::endl;
        return false;
        break;
    }

    // Create an ImageBuf object to store the result
    ImageBuf result_buf, rgba_buf, bit_alpha_buf;

    ImageSpec& rspec = result_buf.specmod();
    //rspec.format = getTypeDesc(settings.bitDepth);

    if (settings.isSolidify && isValid) {

        std::cout << "Filling holes in process...\n";

        Timer pushpull_timer;

        if (external_alpha) {

            ImageBuf* alpha_buf_ptr = &mask_pair.first;

            if (format != TypeDesc::FLOAT) {
                bit_alpha_buf = mask_pair.first.copy(format);
                alpha_buf_ptr = &bit_alpha_buf;
            }

            bool ok = ImageBufAlgo::mul(input_buf, input_buf, grayscale ? mask_pair.first : mask_pair.second);
            ok = ok && ImageBufAlgo::channel_append(rgba_buf, input_buf, *alpha_buf_ptr);
            if (!ok) {
                std::cerr << "Error: " << rgba_buf.geterror() << std::endl;
                mainWindow->emitUpdateTextSignal("Error! Check console for details");
                return false;
            }
            // rename last channel to alpha and set alpha channel
            rgba_buf.specmod().channelnames[rgba_buf.nchannels() - 1] = "A";
            rgba_buf.specmod().alpha_channel = rgba_buf.nchannels() - 1;
        }

        ImageBuf* input_buf_ptr = external_alpha ? &rgba_buf : &input_buf; // Use the multiplied RGBA buffer if have an external alpha

        // Call fillholes_pushpull
        bool ok = ImageBufAlgo::fillholes_pushpull(result_buf, *input_buf_ptr);

        if (!ok) {
            std::cerr << "Error: " << result_buf.geterror() << std::endl;
            mainWindow->emitUpdateTextSignal("Error! Check console for details");
            return false;
        }

        std::cout << "Push-Pull time : " << pushpull_timer.nowText() << std::endl;
    }
    else {
		result_buf = input_buf;
        std::cout << "Filling holes skipped\n";
	}
    // check if filename have any of settings.normNames as substring, case insensitive
    bool isNormName = false;
    std::string lowName = toLower(inputFileName);

    for (auto& name : settings.normNames) {
        if (lowName.find(name, 0) != std::string::npos) {
			isNormName = true;
			break;
		}
	}

    bool doNormalize = false;

    if ((settings.normMode == 2) || (isNormName && settings.normMode != 0)) {
        doNormalize = true;
    }

    if (doNormalize) {
        Timer normalize_timer;
        bool success = true;

        ROI roi(0, result_buf.spec().width, 0, result_buf.spec().height, 0, 1, 0, result_buf.spec().nchannels);
        int nthreads = 0; // debug time 1 thread, for release use 0
        bool fullRange = settings.rngMode;
        success = normalize(result_buf, result_buf, fullRange, roi, nthreads);

        if (!success) {
            std::cerr << "Error: Could not normalize image" << std::endl;
            mainWindow->emitUpdateTextSignal("Error! Check console for details");
            return false;
        }

        std::cout << "Normalize time : " << normalize_timer.nowText() << std::endl;
    }
    else {
		std::cout << "Normalize skipped\n";
	}

    rspec = result_buf.specmod();
    if (settings.expAlpha) {
        rspec.nchannels = grayscale ? 2 : 4; // Only write RGB channels
    }
    else {
		rspec.nchannels = grayscale ? 1 : 3; // Write RGB and alpha channels
	}

    rspec.alpha_channel = -1; // No alpha channel
    //int bits = settings.bitDepth != -1 ? settings.getBitDepth() : 2;

    //rspec.attribute("oiio:BitsPerSample", bits);
    rspec.attribute("pnm:binary", 1);
    rspec.attribute("oiio:UnassociatedAlpha", 1);
    rspec.attribute("jpeg:subsampling", "4:4:4");
    rspec.attribute("Compression", "jpeg:100");
    rspec.attribute("png:compressionLevel", 4);
    //rspec.attribute("tiff:bigtiff", 1);
    //rspec.set_format(TypeDesc::FLOAT); // temporary
    //std::cout << "Channels: " << rspec.nchannels << " Alpha channel: " << rspec.alpha_channel << std::endl;

    format2console(rspec.format);

    auto out = ImageOutput::create(outputFileName);
    if (!out) {
        std::cerr << "Could not create output file: " << outputFileName << std::endl;
        mainWindow->emitUpdateTextSignal("Error! Check console for details");
        return false;
    }    
    out->open(outputFileName, rspec, ImageOutput::Create);

    QMetaObject::invokeMethod(progressBar, "setValue", Qt::QueuedConnection, Q_ARG(int, 0));

    std::cout << "Writing " << outputFileName << std::endl;

    out->write_image(result_buf.spec().format, result_buf.localpixels(), 
        result_buf.pixel_stride(), result_buf.scanline_stride(), result_buf.z_stride(), 
        *progress_callback, progressBar);
    out->close();

    std::cout << std::endl << "Total processing time : " << g_timer.nowText() << std::endl;
    
    pbar_color_rand(progressBar);
    QMetaObject::invokeMethod(progressBar, "setValue", Qt::QueuedConnection, Q_ARG(int, 0));

    //QString DebugText = QString("Total processing time : %1").arg(QString::fromStdString(g_timer.nowText()));
    //mainWindow->emitUpdateTextSignal(DebugText);

    return true;
}
