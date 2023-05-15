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

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

using namespace OIIO;

bool progress_callback(void* opaque_data, float portion_done)
{
    const int width = 40;
    int dashes = width * portion_done;
    std::cout << "\r" << std::flush;
    std::cout << '|' << std::left << std::setw(width) << std::string(dashes, '#') << '|' << std::setw(3);
    return (portion_done >= 1.f);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " INPUT OUTPUT\n";
        return 1;
    }

    const char* input_filename = argv[1];
    const char* output_filename = argv[2];

    // Create an ImageBuf object for the input file
    ImageBuf input_buf(input_filename);

    // Read the image with a progress callback
    std::cout << "Reading " << input_filename << std::endl;
    bool read_ok = input_buf.read(0, 0, 0, -1, true, TypeUnknown, *progress_callback, nullptr);
    if (!read_ok) {
        std::cerr << "Error: Could not read input image\n";
        return 1;
    }
    std::cout << std::endl;

    // Create an ImageBuf object to store the result
    ImageBuf result_buf;

    // Call fillholes_pushpull
    bool ok = ImageBufAlgo::fillholes_pushpull(result_buf, input_buf);

    if (!ok) {
        std::cerr << "Error: " << result_buf.geterror() << std::endl;
        return 1;
    }

    auto out = ImageOutput::create(output_filename);
    if (!out) {
        std::cerr << "Could not create output file: " << output_filename << std::endl;
        return 1;
    }

    ImageSpec spec = result_buf.spec();
    spec.nchannels = 3; // Only write RGB channels
    spec.alpha_channel = -1; // No alpha channel

    out->open(output_filename, spec, ImageOutput::Create);

    std::cout << "Writing " << output_filename << std::endl;

    out->write_image(result_buf.spec().format, result_buf.localpixels(), 
        result_buf.pixel_stride(), result_buf.scanline_stride(), result_buf.z_stride(), 
        *progress_callback, nullptr);
    out->close();

    std::cout << std::endl;

    return 0;
}
