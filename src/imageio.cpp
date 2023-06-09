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

// settings.bitDepth to OIIO::TypeDesc
TypeDesc getTypeDesc(int bit_depth) {
    switch (bit_depth) {
    /*
    UNKNOWN,            ///< unknown type
    NONE,               ///< void/no type
    UINT8,              ///< 8-bit unsigned int values ranging from 0..255,
                        ///<   (C/C++ `unsigned char`).
    UCHAR=UINT8,
    INT8,               ///< 8-bit int values ranging from -128..127,
                        ///<   (C/C++ `char`).
    CHAR=INT8,
    UINT16,             ///< 16-bit int values ranging from 0..65535,
                        ///<   (C/C++ `unsigned short`).
    USHORT=UINT16,
    INT16,              ///< 16-bit int values ranging from -32768..32767,
                        ///<   (C/C++ `short`).
    SHORT=INT16,
    UINT32,             ///< 32-bit unsigned int values (C/C++ `unsigned int`).
    UINT=UINT32,
    INT32,              ///< signed 32-bit int values (C/C++ `int`).
    INT=INT32,
    UINT64,             ///< 64-bit unsigned int values (C/C++
                        ///<   `unsigned long long` on most architectures).
    ULONGLONG=UINT64,
    INT64,              ///< signed 64-bit int values (C/C++ `long long`
                        ///<   on most architectures).
    LONGLONG=INT64,
    HALF,               ///< 16-bit IEEE floating point values (OpenEXR `half`).
    FLOAT,              ///< 32-bit IEEE floating point values, (C/C++ `float`).
    DOUBLE,             ///< 64-bit IEEE floating point values, (C/C++ `double`).
    STRING,             ///< Character string.
    PTR,                ///< A pointer value.
    LASTBASE
    */
    case 0:
		return OIIO::TypeDesc::UINT8;
    case 1:
        return OIIO::TypeDesc::UINT16;
    case 2:
        return OIIO::TypeDesc::UINT32;
    case 3:
        return OIIO::TypeDesc::UINT64;
	case 4:
	    return OIIO::TypeDesc::HALF;
	case 5:
		return OIIO::TypeDesc::FLOAT;
	case 6:
		return OIIO::TypeDesc::DOUBLE;
	default:
		return OIIO::TypeDesc::UNKNOWN;
	}
}

void format2console(TypeDesc format) {
    if (format == TypeDesc::UINT8) {
        std::cout << "8-bit unsigned int" << std::endl;
    }
    else if (format == TypeDesc::UINT16) {
        std::cout << "16-bit unsigned int" << std::endl;
    }
    else if (format == TypeDesc::UINT32) {
        std::cout << "32-bit unsigned int" << std::endl;
    }
    else if (format == TypeDesc::UINT64) {
        std::cout << "64-bit unsigned int" << std::endl;
    }
    else if (format == TypeDesc::HALF) {
        std::cout << "16-bit float" << std::endl;
    }
    else if (format == TypeDesc::FLOAT) {
        std::cout << "32-bit float" << std::endl;
    }
    else if (format == TypeDesc::DOUBLE) {
        std::cout << "64-bit float" << std::endl;
    }
    else {
        std::cout << "Unknown or unsupported format" << std::endl;
    }
}

void formatFromBuff(ImageBuf& buf) {
	std::string format = buf.file_format_name();
    std::cout << "Format: " << format << std::endl;
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
    outBuf.specmod().attribute("oiio:ColorSpace", "Linear");
//    outBuf.specmod().attribute("raw:user_flip", settings.rawRot);
//    outBuf.specmod().extra_attribs["raw:user_flip"] = settings.rawRot;

    std::cout << "Rotations: " << settings.rawRot << std::endl;

    TypeDesc o_format = getTypeDesc(settings.bitDepth);//TypeUnknown; //TypeDesc::FLOAT

    bool read_ok = outBuf.read(0, 0, 0, last_channel, true, o_format, progress_callback, progressBar);
    if (!read_ok) {
        std::cerr << "Error: Could not read input image\n";
        mainWindow->emitUpdateTextSignal("Error! Check console for details");
        return false;
    }

    // fix for pushpull bug or premultiplied alpha
    if (!external_alpha && settings.isSolidify) {
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

template<class Rtype>
static bool normalize_impl(ImageBuf& R, const ImageBuf& A, bool fullRange, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::ConstIterator<Rtype> a(A, roi);
        for (ImageBuf::Iterator<Rtype> r(R, roi); !r.done(); ++r, ++a)
        {
            float x = a[0];
            float y = a[1];
            float z = a[2];

            x = !fullRange ? 2.0f * x - 1.0f : x;
            y = !fullRange ? 2.0f * y - 1.0f : y;
            z = !fullRange ? 2.0f * z - 1.0f : z;

            float length = std::hypot(x, y, z);

            if (length > 0) {
                x /= length;
                y /= length;
                z /= length;
            }

            r[0] = !fullRange ? x * 0.5f + 0.5f : x;
            r[1] = !fullRange ? y * 0.5f + 0.5f : y;
            r[2] = !fullRange ? z * 0.5f + 0.5f : z;
            
            if (A.spec().nchannels == 4) {
                r[3] = a[3];
            }
        }
        });
    return true;
}

bool normalize(ImageBuf& dst, const ImageBuf& A, bool fullRange, ROI roi, int nthreads)
{
    if (!ImageBufAlgo::IBAprep(roi, &dst, &A))
        return false;
    bool ok;
    //OIIO_DISPATCH_COMMON_TYPES(ok, "normalize", normalize_impl, dst.spec().format, dst, A, fullRange, roi, nthreads);
    switch (dst.spec().format.basetype) {
        case TypeDesc::FLOAT: 
            ok = normalize_impl<float>(dst, A, fullRange, roi, nthreads); 
            break; 
        case TypeDesc::UINT8: 
            ok = normalize_impl<unsigned char>(dst, A, fullRange, roi, nthreads); 
            break; 
        case TypeDesc::HALF: // should be half but it give me an error
            ok = normalize_impl<float>(dst, A, fullRange, roi, nthreads);
            break; 
        case TypeDesc::UINT16: 
            ok = normalize_impl<unsigned short>(dst, A, fullRange, roi, nthreads); 
            break; 
        default: 
            { 
            ImageBuf Rtmp; 
            if ((dst).initialized()) Rtmp.copy(dst, TypeDesc::FLOAT); 
                ok = normalize_impl<float>(Rtmp, A, fullRange, roi, nthreads); 
            if (ok) (dst).copy(Rtmp); 
                else (dst).errorfmt("{}", Rtmp.geterror()); 
            }
        };
    return ok;
}


ImageBuf normalize(const ImageBuf& A, bool fullRange, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = normalize(result, A, fullRange, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("normalize error");
    return result;
}