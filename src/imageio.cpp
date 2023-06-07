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
#pragma once

#include "imageio.h"
#include "settings.h"

int hue = 186;
void pbar_color_rand(QProgressBar* progressBar) {
    //hue = QRandomGenerator::global()->bounded(360);
    hue = (hue + 45) % 360;
    int saturation = 250;  // Set saturation value
    int value = 205;       // Set value

    QColor color;
    color.setHsv(hue, saturation, value);

    setPBarColor(progressBar, color.name());
}

bool progress_callback(void* opaque_data, float portion_done) {
    // Cast the opaque_data back to a QProgressBar
    QProgressBar* progressBar = static_cast<QProgressBar*>(opaque_data);

    // Calculate the value to set on the progress bar.
    // The value is an integer between the minimum and maximum (default 0 and 100).
    int value = static_cast<int>(portion_done * 100);

    // You need to use QMetaObject::invokeMethod when you are interacting with the GUI thread from a non-GUI thread
    // Qt::QueuedConnection ensures the change will be made when control returns to the event loop of the GUI thread
    QMetaObject::invokeMethod(progressBar, "setValue", Qt::QueuedConnection, Q_ARG(int, value));

    return (portion_done >= 1.f);
}

std::pair<ImageBuf, ImageBuf> mask_load(const std::string& mask_file, MainWindow* mainWindow) {
    ImageBuf alpha_buf(mask_file);
    std::cout << "Reading " << mask_file << std::endl;
    bool read_ok = alpha_buf.read(0, 0, 0, 1, true, TypeDesc::FLOAT, nullptr, nullptr);
    if (!read_ok) {
        std::cerr << "Error: Could not read mask image\n";
        mainWindow->emitUpdateTextSignal("Error! Check console for details");
        system("pause");
        exit(-1);
    }

    int width = alpha_buf.spec().width;
    int height = alpha_buf.spec().height;
    std::cout << "Mask size: " << width << "x" << height << std::endl;
    // get bit depth of the image channels
    //int bit_depth = alpha_buf.spec().format.basesize * 8;

    // rename channel to alpha and set it as an alpha channel
    alpha_buf.specmod().channelnames[0] = "A";
    alpha_buf.specmod().alpha_channel = 0;

    ImageBuf rgb_alpha, temp_buff;

    bool ok = ImageBufAlgo::channel_append(temp_buff, alpha_buf, alpha_buf);
    ok = ok && ImageBufAlgo::channel_append(rgb_alpha, temp_buff, alpha_buf);
    if (!ok) {
        std::cerr << "Error: Could not append channels\n";
        mainWindow->emitUpdateTextSignal("Error! Check console for details");
        system("pause");
        exit(-1);
    }

    temp_buff.clear();

    return { alpha_buf, rgb_alpha };
}

bool img_load(ImageBuf& outBuf, const std::string& inputFileName, bool external_alpha, QProgressBar* progressBar, MainWindow* mainWindow) {
    int last_channel = -1;

    ImageSpec& spec = outBuf.specmod();
    int nchannels = spec.nchannels;
    if (!external_alpha) {
        if (nchannels == 4) {
            outBuf.specmod().alpha_channel = 3;
            outBuf.specmod().channelnames[3] = "A";
        }
        else if (nchannels == 2) {
            outBuf.specmod().alpha_channel = 1;
            outBuf.specmod().channelnames[1] = "A";
        }
        last_channel = -1;
    }
    else {
        if (nchannels == 4) {
            last_channel = 3;
        }
        else if (nchannels == 2) {
            last_channel = 1;
        }
    }
    outBuf.specmod().attribute("oiio:UnassociatedAlpha", 0);
    outBuf.specmod().attribute("tiff:UnassociatedAlpha", 0);
    TypeDesc o_format = TypeUnknown; //TypeDesc::FLOAT
    bool read_ok = outBuf.read(0, 0, 0, last_channel, true, o_format, progress_callback, progressBar);
    if (!read_ok) {
        std::cerr << "Error: Could not read input image\n";
        mainWindow->emitUpdateTextSignal("Error! Check console for details");
        return false;
    }

    // fix for pushpull bug or premultiplied alpha
    if (!external_alpha) {
        ImageBuf alphs_rgba;
        if (nchannels == 4) {
            int channelorder[] = { 3, 3, 3, 3 };
            float channelvalues[] = { 1.0f, 1.0f, 1.0f, 1.0f };
            alphs_rgba = ImageBufAlgo::channels(outBuf, 4, channelorder, channelvalues);
        }
        else if (nchannels == 2) {
            int channelorder[] = { 1, 1 };
            float channelvalues[] = { 1.0f, 1.0f };
            alphs_rgba = ImageBufAlgo::channels(outBuf, 2, channelorder, channelvalues);
        }
        read_ok = ImageBufAlgo::mul(outBuf, outBuf, alphs_rgba);
        if (!read_ok) {
            std::cerr << "Error: Could not multiply alpha\n";
            mainWindow->emitUpdateTextSignal("Error! Check console for details");
            return false;
        }
        alphs_rgba.clear();
    }
    //
    return true;
}