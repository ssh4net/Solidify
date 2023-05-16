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

#include "Timer.h"

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

using namespace OIIO;

bool progress_callback(void* opaque_data, float portion_done)
{
    const int width = 40;
    int dashes = width * portion_done;
    std::cout << "\r" << std::flush;
    std::cout << '|' << std::left << std::setw(width) << std::string(dashes, '#') << '|' << std::setw(1);
    return (portion_done >= 1.f);
}

std::pair<ImageBuf, ImageBuf> mask_load(const std::string& mask_file) {
    ImageBuf alpha_buf(mask_file);
    std::cout << "Reading " << mask_file << std::endl;
    bool read_ok = alpha_buf.read(0, 0, 0, 1, true, TypeDesc::FLOAT, nullptr, nullptr);
    if (!read_ok) {
		std::cerr << "Error: Could not read mask image\n";
        system("pause");
		exit(-1);
	}

    // rename channel to alpha and set it as an alpha channel
    alpha_buf.specmod().channelnames[0] = "A";
    alpha_buf.specmod().alpha_channel = 0;

    ImageBuf rgb_alpha, temp_buff;

    bool ok = ImageBufAlgo::channel_append(temp_buff, alpha_buf, alpha_buf);
    ok = ok && ImageBufAlgo::channel_append(rgb_alpha, temp_buff, alpha_buf);
    if (!ok) {
		std::cerr << "Error: Could not append channels\n";
        system("pause");
		exit(-1);
	}

    temp_buff.clear();

    return { alpha_buf, rgb_alpha };
}

bool solidify_main(const std::string& inputFileName, const std::string& outputFileName, std::pair<ImageBuf, ImageBuf> mask_pair) {
    Timer g_timer;

    // Create an ImageBuf object for the input file
    ImageBuf input_buf(inputFileName);

    bool external_alpha = false;
    bool grayscale = false;

    if (mask_pair.first.initialized() && mask_pair.second.initialized()) {
		external_alpha = true;
	}

    // Read the image with a progress callback

    int last_channel = (external_alpha) ? 3 : -1; // If we have an external alpha, read only 3 channels

    std::cout << "Reading " << inputFileName << std::endl;

    bool read_ok = input_buf.read(0, 0, 0, last_channel, true, TypeUnknown, *progress_callback, nullptr);
    if (!read_ok) {
        std::cerr << "Error: Could not read input image\n";
        return false;
    }

    std::cout << std::endl;
    std::cout << "channels: " << input_buf.nchannels() << std::endl;

    int alpha_channel = input_buf.spec().alpha_channel;
    std::cout << "alpha channel: " << alpha_channel << std::endl;

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
    ImageBuf result_buf, rgba_buf;

    std::cout << "Filling holes in process...\n";

    Timer pushpull_timer;

    if (external_alpha) {
        bool ok = ImageBufAlgo::mul(input_buf, input_buf, grayscale ? mask_pair.first : mask_pair.second);
        ok = ok && ImageBufAlgo::channel_append(rgba_buf, input_buf, mask_pair.first);
        if (!ok) {
			std::cerr << "Error: " << rgba_buf.geterror() << std::endl;
			return false;
		}
        // rename last channel to alpha and set alpha channel
        rgba_buf.specmod().channelnames[rgba_buf.nchannels() - 1] = "A";
        rgba_buf.specmod().alpha_channel = rgba_buf.nchannels() - 1;
    }

    ImageBuf* input_buf_ptr = external_alpha ? &rgba_buf : &input_buf; // Use the multiplied RGBA buffer if have an external alpha

#if 0
    std::cout << "channels: " << input_buf_ptr->nchannels() << std::endl;
    //std::vector<std::string> chnames = input_buf_ptr->spec().channelnames;
    //for (int i = 0; i < input_buf_ptr->nchannels(); ++i)
    //    std::cout << "channel " << i << " : " << chnames[i] << std::endl;

    std::cout << "alpha channel: " << input_buf_ptr->spec().alpha_channel << std::endl;
#endif

    // Call fillholes_pushpull
    bool ok = ImageBufAlgo::fillholes_pushpull(result_buf, *input_buf_ptr);

    if (!ok) {
        std::cerr << "Error: " << result_buf.geterror() << std::endl;
        return false;
    }

    std::cout << "Push-Pull time : " << pushpull_timer.nowText() << std::endl;

    auto out = ImageOutput::create(outputFileName);
    if (!out) {
        std::cerr << "Could not create output file: " << outputFileName << std::endl;
        return false;
    }

    ImageSpec spec = result_buf.spec();
    spec.nchannels = grayscale ? 1 : 3; // Only write RGB channels
    spec.alpha_channel = -1; // No alpha channel

    out->open(outputFileName, spec, ImageOutput::Create);

    std::cout << "Writing " << outputFileName << std::endl;

    out->write_image(result_buf.spec().format, result_buf.localpixels(), 
        result_buf.pixel_stride(), result_buf.scanline_stride(), result_buf.z_stride(), 
        *progress_callback, nullptr);
    out->close();

    std::cout << std::endl << "Total processing time : " << g_timer.nowText() << std::endl;

    return true;
}
