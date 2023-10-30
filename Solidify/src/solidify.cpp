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

#include <iostream>
#include <iomanip>
#include <string>
#include <math.h>

#include "Timer.h"

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include "Log.h"
#include "solidify.h"
#include "imageio.h"
#include "settings.h"
#include "processing.h"

using namespace OIIO;

bool solidify_main(const std::string& inputFileName, const std::string& outputFileName, std::pair<ImageBuf, ImageBuf> mask_pair, 
    QProgressBar* progressBar, MainWindow* mainWindow) {
    Timer g_timer;

//
    // Generate a random delay
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distr(1, 1000); // delay range in milliseconds

    // Sleep for the random delay
    std::this_thread::sleep_for(std::chrono::milliseconds(distr(gen)));
//
    TypeDesc out_format;
    
    ImageSpec config;
    config["raw:user_flip"] = settings.rawRot;
    
    config["oiio:UnassociatedAlpha"] = 0;
    config["tiff:UnassociatedAlpha"] = 0;
    config["oiio:ColorSpace"] = "Linear";

    ImageBuf input_buf(inputFileName, 0, 0, nullptr, &config, nullptr);

    if (!input_buf.init_spec(inputFileName, 0, 0)){
		LOG(error) << "Error reading " << inputFileName << std::endl;
        LOG(error) << input_buf.geterror() << std::endl;
		mainWindow->emitUpdateTextSignal("Error! Check console for details");
		return false;
	}

    TypeDesc orig_format = input_buf.spec().format;

    LOG(info) << "File bith depth: " << formatText(orig_format) << std::endl;

    //ImageBuf::ImageBuf(config, input_buf);

    bool external_alpha = false;
    bool grayscale = false;

    if (mask_pair.first.initialized() && mask_pair.second.initialized()) {
		external_alpha = true;
	}

    // Read the image with a progress callback

    LOG(info) << "Reading " << inputFileName << std::endl;
    bool load_ok = img_load(input_buf, inputFileName, external_alpha, progressBar, mainWindow);
    if (!load_ok) {
		LOG(error) << "Error reading " << inputFileName << std::endl;
        mainWindow->emitUpdateTextSignal("Error! Check console for details");
		return false;
	}

    // Get the image's spec
    const ImageSpec& ispec = input_buf.spec();

    // Get the format (bit depth and type)
    TypeDesc load_format = ispec.format;
    out_format = load_format; // copy latest buffer format as an output format

    LOG(info) << "File loaded bit depth: " << formatText(load_format) << std::endl;

    // get the image size
    int width = input_buf.spec().width;
    int height = input_buf.spec().height;
    LOG(info) << "Image size: " << width << "x" << height << std::endl;
    LOG(info) << "Channels: " << input_buf.nchannels() << " Alpha channel index: " << input_buf.spec().alpha_channel << std::endl;

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
        LOG(error) << "Error: Only Grayscale, RGB and RGBA images are supported" << std::endl;
        LOG(error) << "Grayscale and RGB images must have an external alpha channel or use external alpha file" << std::endl;
        return false;
        break;
    }

    // Create an ImageBuf object to store the result

    ImageBuf result_buf, out_buf, rgba_buf, original_alpha, bit_alpha_buf;

    // check if filename have any of settings.normNames as substring, case insensitive
    bool isNormName = false;
    bool doNormalize = false;
    std::string lowName = toLower(inputFileName);

    for (auto& name : settings.normNames) {
        if (lowName.find(name, 0) != std::string::npos) {
            isNormName = true;
            break;
        }
    }
    //rspec.format = TypeDesc::FLOAT;
    //rspec.format = getTypeDesc(settings.bitDepth);

    if (settings.isSolidify && isValid) {
        LOG(info) << "Filling holes in process...\n";
        Timer pushpull_timer;

        if (external_alpha) {
            ImageBuf* alpha_buf_ptr = &mask_pair.first;

            if (load_format != TypeDesc::FLOAT) {
                bit_alpha_buf = mask_pair.first.copy(load_format);
                alpha_buf_ptr = &bit_alpha_buf;
            }
            else {
                bit_alpha_buf = mask_pair.first;
			}

            bool ok = ImageBufAlgo::mul(input_buf, input_buf, grayscale ? mask_pair.first : mask_pair.second);
            if (!ok) {
                LOG(error) << "multiplication error: " << rgba_buf.geterror() << std::endl;
                mainWindow->emitUpdateTextSignal("Error! Check console for details");
                return false;
            }
            ok = ok && ImageBufAlgo::channel_append(rgba_buf, input_buf, *alpha_buf_ptr);
            if (!ok) {
                LOG(error) << "channel_append error: " << rgba_buf.geterror() << std::endl;
                mainWindow->emitUpdateTextSignal("Error! Check console for details");
                return false;
            }
            // rename last channel to alpha and set alpha channel
            rgba_buf.specmod().channelnames[rgba_buf.nchannels() - 1] = "A";
            rgba_buf.specmod().alpha_channel = rgba_buf.nchannels() - 1;
        }
        else if (settings.expAlpha) { // if Export alpha channel is enabled, copy the original alpha channel to a separate buffer
            original_alpha = ImageBufAlgo::channels(input_buf, 1, 3);
        }

        ImageBuf* input_buf_ptr = external_alpha ? &rgba_buf : &input_buf; // Use the multiplied RGBA buffer if have an external alpha

        // Call fillholes_pushpull
        bool ok = ImageBufAlgo::fillholes_pushpull(result_buf, *input_buf_ptr);

        if (!ok) {
            LOG(error) << "fillholes_pushpull error: " << result_buf.geterror() << std::endl;
            mainWindow->emitUpdateTextSignal("Error! Check console for details");
            return false;
        }

        if (settings.expAlpha) {
            ImageBufAlgo::paste(result_buf, 0, 0, 0, result_buf.spec().alpha_channel, external_alpha ? bit_alpha_buf : original_alpha);
            if (result_buf.has_error()) {
                LOG(error) << "paste error: " << result_buf.geterror() << std::endl;
                mainWindow->emitUpdateTextSignal("Error! Check console for details");
                return false;
            }
        }
        // reset unused buffers
        rgba_buf.clear();
        external_alpha ? bit_alpha_buf.clear() : original_alpha.clear();
        
#if 0
        debugImageBufWrite(result_buf, "d:/result_buf.tif");
#endif

        out_format = result_buf.spec().format; // copy latest buffer format as an output format
        LOG(info) << "Push-Pull format: " << formatText(out_format) << std::endl;
        LOG(info) << "Push-Pull time : " << pushpull_timer.nowText() << std::endl;
    }
    else {
		result_buf = input_buf;
        LOG(info) << "Filling holes skipped\n";
	}

    if ((settings.normMode == 2) || (isNormName && settings.normMode != 0)) {
        doNormalize = true;
    }

    if (settings.repairMode > 0) {
        Timer normalize_timer;
        bool success = true;
        int sign = 1;

        LOG(info) << "Repairing normals in process...\n";

        uint channel = settings.repairMode - 1;

        if (settings.repairMode > 4) {
			sign = -1;
            channel = settings.repairMode - 4;
		}

        ROI roi(0, result_buf.spec().width, 0, result_buf.spec().height, 0, 1, 0, result_buf.spec().nchannels);
        int nthreads = 0; // debug time 1 thread, for release use 0
        float inCenter = 0.5f, outCenter = 0.5f, outScale = 0.5f;

        switch (settings.rangeMode) {
        case 0:
            inCenter = 0.5f;
            outCenter = 0.5f;
            outScale = 0.5f;
            break;
        case 1:
            inCenter = 0.0f;
            outCenter = 0.0f;
            outScale = 1.0f;
            break;
        case 2:
            inCenter = 0.0f;
            outCenter = 0.5f;
            outScale = 0.5f;
            break;
        case 3:
            inCenter = 0.5f;
            outCenter = 0.0f;
            outScale = 1.0f;
            break;
        default:
            break;
        }

        success = recalc_normal(out_buf, result_buf, channel, sign, inCenter, outCenter, outScale, roi, nthreads);
        doNormalize = false;
    }

    if (doNormalize) {
        Timer normalize_timer;
        bool success = true;

        ROI roi(0, result_buf.spec().width, 0, result_buf.spec().height, 0, 1, 0, result_buf.spec().nchannels);
        int nthreads = 0; // debug time 1 thread, for release use 0
        float inCenter = 0.5f, outCenter = 0.5f, outScale = 0.5f;
        switch (settings.rangeMode) {
        case 0:
			inCenter = 0.5f;
			outCenter = 0.5f;
			outScale = 0.5f;
			break;
        case 1:
            inCenter = 0.0f;
            outCenter = 0.0f;
            outScale = 1.0f;
            break;
        case 2:
			inCenter = 0.0f;
			outCenter = 0.5f;
			outScale = 0.5f;
			break;
        case 3:
            inCenter = 0.5f;
            outCenter = 0.0f;
            outScale = 1.0f;
            break;
        default:
			break;
        }

        success = ImageBufAlgo::normalize(out_buf, result_buf, inCenter, outCenter, outScale, roi, nthreads);
        if (!success) {
            LOG(error) << "Error: Could not normalize image" << std::endl;
            mainWindow->emitUpdateTextSignal("Error! Check console for details");
            return false;
        }
        out_format = out_buf.spec().format; // copy latest buffer format as an output format
        LOG(info) << "Normalized format: " << formatText(out_format) << std::endl;
        LOG(info) << "Normalize time : " << normalize_timer.nowText() << std::endl;
    }
    else if (settings.repairMode == 0) {
        out_buf = result_buf;
		LOG(info) << "Normalize skipped\n";
	}

    ImageSpec& ospec = out_buf.specmod();
    if (settings.expAlpha) {
        ospec.nchannels = grayscale ? 2 : 4; // Only write RGB channels
    }
    else {
		ospec.nchannels = grayscale ? 1 : 3; // Write RGB and alpha channels
	}
    
    ospec.erase_attribute("Exif:LensSpecification");
    LOG(info) << "OIIO Libtiff EXIF fix deleting: " << "Exif:LensSpecification" << std::endl;

/*
    // Debug. Output all EXIF tags to console
    // Initialize a vector to hold names of attributes to be removed
    std::vector<std::string> attrs_to_remove;

    for (auto& attr : ospec.extra_attribs) {
        std::string name = attr.name().string();

        // Check if the attribute name starts with the prefixes you want to remove
        if ( name.rfind("Exif:LensSpecification", 0) == 0) {
            LOG(info) << "OIIO Libtiff EXIF fix deleting: " << name << " : " << attr.get_string() << std::endl;
            attrs_to_remove.push_back(name);
        }
        else
        {
            //LOG(info) << name << " : " << attr.get_string() << std::endl;
        }
    }
    
    // Remove the selected attributes
    for (const auto& name : attrs_to_remove) {
        ospec.erase_attribute(name);
    }
*/
///////
/*
    // Initialize a vector to hold pairs of old and new attribute names
    std::vector<std::pair<std::string, std::string>> attrs_to_rename;

    // Loop through all metadata attributes in the ImageSpec
    for (const auto& attr : ospec.extra_attribs) {
        std::string name = attr.name().string();

        // Check if the attribute name starts with "Exif:"
        if (name.rfind("Exif:", 0) == 0) {
            // Generate new attribute name with lowercase "exif:"
            std::string new_name = "exif:" + name.substr(5);
            attrs_to_rename.emplace_back(name, new_name);
        }
    }

    // Rename the selected attributes
    for (const auto& name_pair : attrs_to_rename) {
        // Get old and new names
        const std::string& old_name = name_pair.first;
        const std::string& new_name = name_pair.second;

        // Retrieve the value of the old attribute
        const OIIO::ParamValue* param = ospec.find_attribute(old_name);
        OIIO::TypeDesc type = param->type();
        const void* value = param->data();

        // Create a copy of the attribute data
        void* non_const_value = malloc(type.size());
        memcpy(non_const_value, value, type.size());

        // Remove old attribute and insert new one
        ospec.erase_attribute(old_name);
        ospec.attribute(new_name, type, non_const_value);

        // Free the allocated memory
        free(non_const_value);
    }
*/
////////////////////



    ospec.alpha_channel = -1; // No alpha channel
    //int bits = settings.bitDepth != -1 ? settings.getBitDepth() : 2;

    //rspec.attribute("oiio:BitsPerSample", bits);
    ospec.attribute("pnm:binary", 1);
    ospec.attribute("oiio:UnassociatedAlpha", 1);
    ospec.attribute("jpeg:subsampling", "4:4:4");
    ospec.attribute("Compression", "jpeg:100");
    ospec.attribute("png:compressionLevel", 4);
    //ospec.attribute("tiff:write_exif", 0);
    //rspec.attribute("tiff:bigtiff", 1);
    //rspec.set_format(TypeDesc::FLOAT); // temporary
    //LOG(info) << "Channels: " << rspec.nchannels << " Alpha channel: " << rspec.alpha_channel << std::endl;
//
    //ospec.format = getTypeDesc(settings.bitDepth);
    if (getTypeDesc(settings.bitDepth) == TypeDesc::UNKNOWN) {
        ospec.set_format(orig_format);
	} else {
		ospec.set_format(getTypeDesc(settings.bitDepth));
    }

    LOG(info) << "Output file format: " << formatText(ospec.format) << std::endl;

    auto out = ImageOutput::create(outputFileName);
    if (!out) {
        LOG(error) << "Could not create output file: " << outputFileName << std::endl;
        mainWindow->emitUpdateTextSignal("Error! Check console for details");
        return false;
    }
    out->open(outputFileName, ospec, ImageOutput::Create);

    QMetaObject::invokeMethod(progressBar, "setValue", Qt::QueuedConnection, Q_ARG(int, 0));

    LOG(info) << "Writing " << outputFileName << std::endl;

    auto ok = out->write_image(out_format, out_buf.localpixels(),out_buf.pixel_stride(), out_buf.scanline_stride(), out_buf.z_stride(), *m_progress_callback, progressBar);
    if (!ok) {
		LOG(error) << "Error writing " << outputFileName << std::endl;
		LOG(error) << out->geterror() << std::endl;
		mainWindow->emitUpdateTextSignal("Error! Check console for details");
		return false;
	}
    out->close();

    LOG(info) << "File processing time : " << g_timer.nowText() << std::endl;
    
    //pbar_color_rand(progressBar);
    pbar_color_rand(mainWindow);
    QMetaObject::invokeMethod(progressBar, "setValue", Qt::QueuedConnection, Q_ARG(int, 0));

    //QString DebugText = QString("Total processing time : %1").arg(QString::fromStdString(g_timer.nowText()));
    //mainWindow->emitUpdateTextSignal(DebugText);

    return true;
}
