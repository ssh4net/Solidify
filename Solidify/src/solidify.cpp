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

#include "pch.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <math.h>

#include "Timer.h"

//#include <OpenImageIO/imageio.h>
//#include <OpenImageIO/imagebuf.h>
//#include <OpenImageIO/imagebufalgo.h>

//#include "Log.h"
#include "solidify.h"
#include "imageio.h"
#include "imageops.h"
#include "settings.h"
#include "processing.h"

using namespace OIIO;

static void reportProgress(const SolidifyProgressCallback& progressCallback, float progress, const std::string& status)
{
    if (progressCallback) {
        progressCallback(progress, status);
    }
}

void* getTypedPointer(OIIO::ImageBuf& buf, const OIIO::TypeDesc& type) {
    switch (type.basetype) {
        case OIIO::TypeDesc::UINT8:
		    return (uint8_t*)buf.localpixels();
        case OIIO::TypeDesc::UINT16:
            return (uint16_t*)buf.localpixels();
        case OIIO::TypeDesc::UINT32:
			return (uint32_t*)buf.localpixels();
        case OIIO::TypeDesc::UINT64:
            return (uint64_t*)buf.localpixels();
        case OIIO::TypeDesc::HALF:
			return (half*)buf.localpixels();
        case OIIO::TypeDesc::FLOAT:
			return (float*)buf.localpixels();
        case OIIO::TypeDesc::DOUBLE:
            return (double*)buf.localpixels();
        default:
			return nullptr;
    }
}

bool solidify_main(const std::string& inputFileName, const std::string& outputFileName, std::pair<ImageBuf, ImageBuf> mask_pair,
    const SolidifyProgressCallback& progressCallback) {
    VTimer g_timer;

    TypeDesc out_format;
    
    ImageSpec config;
    config["raw:user_flip"] = settings.rawRot;
    //config["oiio:reorient"] = 0;

    config["oiio:UnassociatedAlpha"] = 0;
    config["tiff:UnassociatedAlpha"] = 0;
    config["oiio:ColorSpace"] = "Linear";

    ImageBuf input_buf(inputFileName, 0, 0, nullptr, &config, nullptr);

    if (!input_buf.init_spec(inputFileName, 0, 0)){
		spdlog::error("Error reading {}", inputFileName);
        spdlog::error("{}", input_buf.geterror());
        reportProgress(progressCallback, 0.0f, "Error! Check console for details");
		return false;
	}

    TypeDesc orig_format = input_buf.spec().format;

 //   // check EXIF rotation
 //   int orientation = 1;
 //   auto& tspec = input_buf.spec();
 //   auto& extspec = tspec.extra_attribs;
 //   for (auto& attr : extspec) {
	//	std::string name = attr.name().string();
	//	std::string value = attr.get_string();
	//	spdlog::info) << name << " : " << value << std::endl;
	//}

    spdlog::info("File bith depth: {}", formatText(orig_format));

    //ImageBuf::ImageBuf(config, input_buf);

    bool external_alpha = false;
    bool grayscale = false;

    if (mask_pair.first.initialized() && mask_pair.second.initialized()) {
		external_alpha = true;
	}

    // Read the image with a progress callback

    spdlog::info("Reading {}", inputFileName);
    bool load_ok = img_load(input_buf, inputFileName, external_alpha, progressCallback);
    if (!load_ok) {
		spdlog::error("Error reading {}", inputFileName);
        reportProgress(progressCallback, 0.0f, "Error! Check console for details");
		return false;
	}

    // Get the image's spec
    const ImageSpec& ispec = input_buf.spec();

    // Get the format (bit depth and type)
    TypeDesc load_format = ispec.format;
    out_format = load_format; // copy latest buffer format as an output format

    spdlog::info("File loaded bit depth: {}", formatText(load_format));

    // get the image size
    int width = input_buf.spec().width;
    int height = input_buf.spec().height;
    spdlog::info("Image size: {}x{}", width, height);
    spdlog::info("Channels: {} Alpha channel index: {}", input_buf.nchannels(), input_buf.spec().alpha_channel);

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
        spdlog::error("Error: Only Grayscale, RGB and RGBA images are supported");
        spdlog::error("Grayscale and RGB images must have an external alpha channel or use external alpha file");
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
        spdlog::info("Filling holes in process...\n");
        VTimer pushpull_timer;

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
                spdlog::error("multiplication error: {}", input_buf.geterror());
                reportProgress(progressCallback, 0.0f, "Error! Check console for details");
                return false;
            }
            ok = ok && ImageBufAlgo::channel_append(rgba_buf, input_buf, *alpha_buf_ptr);
            if (!ok) {
                spdlog::error("channel_append error: {}", rgba_buf.geterror());
                reportProgress(progressCallback, 0.0f, "Error! Check console for details");
                return false;
            }
            // rename last channel to alpha and set alpha channel
            rgba_buf.specmod().channelnames[rgba_buf.nchannels() - 1] = "A";
            rgba_buf.specmod().alpha_channel = rgba_buf.nchannels() - 1;
        }
        else if (settings.alphaMode == 1) { // if Export alpha channel is enabled, copy the original alpha channel to a separate buffer
            original_alpha = ImageBufAlgo::channels(input_buf, 1, 3);
        }

        ImageBuf* input_buf_ptr = external_alpha ? &rgba_buf : &input_buf; // Use the multiplied RGBA buffer if have an external alpha

        // Call fillholes_pushpull
        bool ok = ImageBufAlgo::fillholes_pushpull(result_buf, *input_buf_ptr);

        if (!ok) {
            spdlog::error("fillholes_pushpull error: {}", result_buf.geterror());
            reportProgress(progressCallback, 0.0f, "Error! Check console for details");
            return false;
        }

        if (settings.alphaMode == 1) {
            ImageBufAlgo::paste(result_buf, 0, 0, 0, result_buf.spec().alpha_channel, external_alpha ? bit_alpha_buf : original_alpha);
            if (result_buf.has_error()) {
                spdlog::error("paste error: {}", result_buf.geterror());
                reportProgress(progressCallback, 0.0f, "Error! Check console for details");
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
        spdlog::info("Push-Pull format: {}", formatText(out_format));
        spdlog::info("Push-Pull time : {}", pushpull_timer.nowText());
    }
    else {
		result_buf = input_buf;
        spdlog::info("Filling holes skipped\n");
	}

    if ((settings.normMode == 2) || (isNormName && settings.normMode != 0)) {
        doNormalize = true;
    }

    if (settings.repairMode > 0) {
        VTimer normalize_timer;
        bool success = true;
        int sign = 1;

        spdlog::info("Repairing normals in process...\n");

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
        VTimer normalize_timer;
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
            spdlog::error("Error: Could not normalize image");
            reportProgress(progressCallback, 0.0f, "Error! Check console for details");
            return false;
        }
        out_format = out_buf.spec().format; // copy latest buffer format as an output format
        spdlog::info("Normalized format: {}", formatText(out_format));
        spdlog::info("Normalize time : {}", normalize_timer.nowText());
    }
    else if (settings.repairMode == 0) {
        out_buf = result_buf;
		spdlog::info("Normalize skipped\n");
	}

    // if not normalize and not repair and range conversion is needed
    if (settings.repairMode == 0 && !doNormalize && settings.rangeMode > 1) {
        switch (settings.rangeMode) {
        case 2: // 2 - unsigned -> signed
            out_buf = ImageBufAlgo::mad(out_buf, 2.0f, -1.0f);
            break;
        case 3: // 3 - signed -> unsigned
            out_buf = ImageBufAlgo::mad(out_buf, 0.5f, 0.5f);
			break;
        }
    }

    if (settings.swapBasis != 0 || settings.swapInvertMask != 0) {
        VTimer transform_timer;
        ImageBuf transformed_buf;
        const bool signedOutputRange = settings.rangeMode == 1 || settings.rangeMode == 3;
        spdlog::info("Applying channel swap/invert...\n");
        const bool success = applyChannelSwapInvert(transformed_buf, out_buf, settings.swapBasis,
                                                    settings.swapInvertMask, signedOutputRange, 0);
        if (!success) {
            spdlog::error("Error: Could not apply channel swap/invert");
            spdlog::error("{}", transformed_buf.geterror());
            reportProgress(progressCallback, 0.0f, "Error! Check console for details");
            return false;
        }
        out_buf = std::move(transformed_buf);
        out_format = out_buf.spec().format;
        grayscale = out_buf.nchannels() <= 2;
        spdlog::info("Channel swap/invert time : {}", transform_timer.nowText());
    }

    if (settings.grayscaleMode != 0) {
        VTimer grayscale_timer;
        ImageBuf grayscale_buf;
        spdlog::info("Applying grayscale conversion...\n");
        const bool success = applyGrayscale(grayscale_buf, out_buf, settings.grayscaleMode, settings.grayscaleWeights,
                                            settings.alphaMode == 1, 0);
        if (!success) {
            spdlog::error("Error: Could not apply grayscale conversion");
            spdlog::error("{}", grayscale_buf.geterror());
            reportProgress(progressCallback, 0.0f, "Error! Check console for details");
            return false;
        }
        out_buf = std::move(grayscale_buf);
        out_format = out_buf.spec().format;
        grayscale = true;
        spdlog::info("Grayscale conversion time : {}", grayscale_timer.nowText());
    }

    const int outputBufferChannels = out_buf.nchannels();
    const int outputAlphaChannel = out_buf.spec().alpha_channel >= 0 ? out_buf.spec().alpha_channel :
                                   (out_buf.nchannels() == 4 ? 3 : (out_buf.nchannels() == 2 ? 1 : -1));

    ImageSpec& ospec = out_buf.specmod();
    if (settings.alphaMode == 1 && outputAlphaChannel >= 0) {
        ospec.nchannels = grayscale ? std::min(2, outputBufferChannels) : std::min(4, outputBufferChannels); // Write RGB and alpha channels
    }
    else if (settings.alphaMode == 0) {
		ospec.nchannels = grayscale ? 1 : std::min(3, outputBufferChannels); // Only write RGB channels
    }
    else {
        ospec.nchannels = 1; // Only write alpha channel
    }
    
    ospec.erase_attribute("Exif:LensSpecification");
    spdlog::info("OIIO Libtiff EXIF fix deleting: Exif:LensSpecification");

/*
    // Debug. Output all EXIF tags to console
    // Initialize a vector to hold names of attributes to be removed
    std::vector<std::string> attrs_to_remove;

    for (auto& attr : ospec.extra_attribs) {
        std::string name = attr.name().string();

        // Check if the attribute name starts with the prefixes you want to remove
        if ( name.rfind("Exif:LensSpecification", 0) == 0) {
            spdlog::info) << "OIIO Libtiff EXIF fix deleting: " << name << " : " << attr.get_string() << std::endl;
            attrs_to_remove.push_back(name);
        }
        else
        {
            //spdlog::info) << name << " : " << attr.get_string() << std::endl;
        }
    }
    
    // Remove the selected attributes
    for (const auto& name : attrs_to_remove) {
        ospec.erase_attribute(name);
    }
*/
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
    if (settings.fileFormat == 5)
        ospec.attribute("Compression", "heic:100");
    if (settings.fileFormat == 6)
		ospec.attribute("Compression", "jpegxl:100");

    ospec.attribute("png:compressionLevel", 4);
    //ospec.attribute("tiff:write_exif", 0);
    //rspec.attribute("tiff:bigtiff", 1);
    //rspec.set_format(TypeDesc::FLOAT); // temporary
    //spdlog::info) << "Channels: " << rspec.nchannels << " Alpha channel: " << rspec.alpha_channel << std::endl;
//
    //ospec.format = getTypeDesc(settings.bitDepth);
    if (getTypeDesc(settings.bitDepth) == TypeDesc::UNKNOWN) {
        ospec.set_format(orig_format);
	} else {
		ospec.set_format(getTypeDesc(settings.bitDepth));
    }

    spdlog::info("Output file format: {}", formatText(ospec.format));

    auto out = ImageOutput::create(outputFileName);
    if (!out) {
        spdlog::error("Could not create output file: {}", outputFileName);
        reportProgress(progressCallback, 0.0f, "Error! Check console for details");
        return false;
    }
    out->open(outputFileName, ospec, ImageOutput::Create);
    if (out->has_error()) {
		spdlog::error("Error opening {}", outputFileName);
		spdlog::error("{}", out->geterror());
        reportProgress(progressCallback, 0.0f, "Error! Check console for details");
        out->close();
		return false;
	}

    spdlog::info("Writing {}", outputFileName);

    bool ok = false;
    OIIOProgressContext writeCtx;
    writeCtx.callback = &progressCallback;
    writeCtx.status   = "Writing: " + std::filesystem::path(outputFileName).filename().string();
    writeCtx.base     = 0.65f;
    writeCtx.scale    = 0.35f;
    void* writeProgressData = progressCallback ? &writeCtx : nullptr;

    if (settings.alphaMode != 2) {
        ok = out->write_image(out_format, out_buf.localpixels(), out_buf.pixel_stride(), out_buf.scanline_stride(), out_buf.z_stride(), m_progress_callback, writeProgressData);
    }
    else {
        int channel_to_extract = outputAlphaChannel >= 0 ? outputAlphaChannel : 0;  // for Alpha channel from RGBA/YA
        int channels = outputBufferChannels;
        int bytes = out_format.size(); //

        ok = out->write_image(out_format,
            (char*)out_buf.localpixels() + channel_to_extract * bytes, // pointer to the first pixel to write
            channels * bytes,      // x stride
            out_buf.scanline_stride(),      // y stride
            out_buf.z_stride(), 		    // z stride  
            m_progress_callback, writeProgressData);
    }

    if (!ok) {
		spdlog::error("Error writing {}", outputFileName);
		spdlog::error("{}", out->geterror());
        reportProgress(progressCallback, 0.0f, "Error! Check console for details");
		return false;
	}
    out->close();

    spdlog::info("File processing time : {}", g_timer.nowText());
    
    reportProgress(progressCallback, 1.0f, "Written: " + std::filesystem::path(outputFileName).filename().string());

    return true;
}
