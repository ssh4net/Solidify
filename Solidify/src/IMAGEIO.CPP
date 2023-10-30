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
#pragma once

#include "imageio.h"
#include "settings.h"
#include "Log.h"

int hue = 186;
//void pbar_color_rand(QProgressBar* progressBar) {
void pbar_color_rand(MainWindow* mainWindow) {
    hue = (hue + 45) % 360;
    int saturation = 250;  // Set saturation value
    int value = 205;       // Set value

    QColor color;
    color.setHsv(hue, saturation, value);

    //setPBarColor(progressBar, color.name());
    emit mainWindow->changeProgressBarColorSignal(color);
}

bool m_progress_callback(void* opaque_data, float portion_done) {
    // Cast the opaque_data back to a QProgressBar
    QProgressBar* progressBar = static_cast<QProgressBar*>(opaque_data);

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

std::string formatText(TypeDesc format) {
    switch (format.basetype) {
    case TypeDesc::UINT8:
        return "8bit integer";
    case TypeDesc::UINT16:
        return "16bit integer";
    case TypeDesc::UINT32:
        return "32bit integer";
    case TypeDesc::UINT64:
        return "64bit integer";
    case TypeDesc::INT8:
        return "8bit integer (signed)";
    case TypeDesc::INT16:
        return "16bit integer (signed)";
    case TypeDesc::INT32:
        return "32bit integer (signed)";
    case TypeDesc::INT64:
        return "64bit integer (signed)";
    case TypeDesc::HALF:
        return "16bit (half) float";
    case TypeDesc::FLOAT:
        return "32bit float";
    case TypeDesc::DOUBLE:
        return "64bit (double) float";
    default:
        return "UNKNOWN bit depth";
    }
}

void formatFromBuff(ImageBuf& buf) {
	std::string format = buf.file_format_name();
    LOG(info) << "Format: " << format << std::endl;
}

void debugImageBufWrite(const ImageBuf& buf, const std::string& filename) {
	bool write_ok = buf.write(filename);
    if (!write_ok) {
		LOG(error) << "Error: Could not write to " << filename << std::endl;
		LOG(error) << "Error: " << buf.geterror() << std::endl;
	}
}

std::pair<ImageBuf, ImageBuf> mask_load(const std::string& mask_file, MainWindow* mainWindow) {
    ImageBuf alpha_buf(mask_file);
    LOG(info) << "Reading " << mask_file << std::endl;
    bool read_ok = alpha_buf.read(0, 0, 0, 1, true, TypeDesc::FLOAT, nullptr, nullptr);
    if (!read_ok) {
        LOG(error) << "Error: Could not read mask image\n";
        mainWindow->emitUpdateTextSignal("Error! Check console for details");
        system("pause");
        exit(-1);
    }

    int width = alpha_buf.spec().width;
    int height = alpha_buf.spec().height;
    LOG(info) << "Mask size: " << width << "x" << height << std::endl;

    // rename channel to alpha and set it as an alpha channel
    alpha_buf.specmod().channelnames[0] = "A";
    alpha_buf.specmod().alpha_channel = 0;

    ImageBuf rgb_alpha;// , temp_buff;

    int channelorder[] = { 0, 0, 0 };
    float channelvalues[] = { 1.0f, 1.0f, 1.0f };
    rgb_alpha = ImageBufAlgo::channels(alpha_buf, 3, channelorder, channelvalues);
    
    if (rgb_alpha.has_error()) {
		LOG(error) << "Error: Could not append channels\n";
		mainWindow->emitUpdateTextSignal("Error! Check console for details");
		system("pause");
		exit(-1);
	}

#if 0
    debugImageBufWrite(rgb_alpha, "d:/rgb_alpha.tif");
    debugImageBufWrite(alpha_buf, "d:/alpha.tif");
#endif

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
//    outBuf.specmod().attribute("raw:user_flip", settings.rawRot);
//    outBuf.specmod().extra_attribs["raw:user_flip"] = settings.rawRot;

//    LOG(info) << "CameraRaw Rotations: " << settings.rawRot << std::endl;
    
    TypeDesc o_format = spec.format; //getTypeDesc(settings.bitDepth);

    switch (o_format.basetype) {
        case TypeDesc::UINT8:
            o_format = TypeDesc::UINT16;
			break;
        case TypeDesc::HALF:
            o_format = TypeDesc::FLOAT;
            break;
		default:
			break;
    } 

    // = TypeDesc::FLOAT;// getTypeDesc(settings.bitDepth);//TypeUnknown; //TypeDesc::FLOAT

    bool read_ok = outBuf.read(0, 0, 0, last_channel, true, o_format, m_progress_callback, progressBar);
    if (!read_ok) {
        LOG(error) << "Error: Could not read input image\n";
        mainWindow->emitUpdateTextSignal("Error! Check console for details");
        return false;
    }

    // fix for pushpull bug or premultiplied alpha
    if (!external_alpha && settings.isSolidify) {
        ImageBuf alphs_rgba;
        if (nchannels == 4) {
            int channelorder[] = { 3, 3, 3 };
            float channelvalues[] = { 1.0f, 1.0f, 1.0f };
            alphs_rgba = ImageBufAlgo::channels(outBuf, 3, channelorder, channelvalues);
        }
        else if (nchannels == 2) {
            int channelorder[] = { 1 };
            float channelvalues[] = { 1.0f };
            alphs_rgba = ImageBufAlgo::channels(outBuf, 3, channelorder, channelvalues);
        }
        read_ok = ImageBufAlgo::mul(outBuf, outBuf, alphs_rgba);
        if (!read_ok) {
            LOG(error) << "Error: Could not multiply alpha\n";
            mainWindow->emitUpdateTextSignal("Error! Check console for details");
            return false;
        }
#if 0
        debugImageBufWrite(outBuf, "d:/premul_Buf.tif");
        debugImageBufWrite(alphs_rgba, "d:/premul_alphs_rgba.tif");
#endif
        alphs_rgba.clear();
    }
    //
    return true;
}

template<class Rtype>
static bool recalc_normal_impl(ImageBuf& R, const ImageBuf& A, uint channel, int sign, float inCenter, float outCenter, float scale, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::ConstIterator<Rtype> a(A, roi);
        for (ImageBuf::Iterator<Rtype> r(R, roi); !r.done(); ++r, ++a)
        {

            // axis = sqrt(1 - (x^2 + y^2))
            // axs = sqrt(1 - (x*x + y*y + z*z))

            float t[3];

            t[0] = channel == 0 ? 0.0f : (a[0] - inCenter) / scale;
            t[1] = channel == 1 ? 0.0f : (a[1] - inCenter) / scale;
            t[2] = channel == 2 ? 0.0f : (a[2] - inCenter) / scale;

            bool isZero = (abs(t[0]) + abs(t[1]) + abs(t[2])) == 0.0f;

            float recomp = 0.0f;

            if (!isZero) {
                recomp = sqrt(1.0f - (t[0] * t[0] + t[1] * t[1] + t[2] * t[2]));
			}

            t[channel] = sign * recomp;

            float length = sqrt(t[0] * t[0] + t[1] * t[1] + t[2] * t[2]);

            float s = (length > 0.0f) ? scale / length : scale;

            r[0] = t[0] * s + outCenter;
            r[1] = t[1] * s + outCenter;
            r[2] = t[2] * s + outCenter;
            
            if (A.spec().nchannels == 4) {
                r[3] = a[3];
            }
        }
    });
    return true;
}

bool recalc_normal(ImageBuf& dst, const ImageBuf& A, uint channel, int sign, float inCenter, float outCenter, float scale, ROI roi, int nthreads)
{
    if (!ImageBufAlgo::IBAprep(roi, &dst, &A))
        return false;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES(ok, "recalc_normal", recalc_normal_impl, dst.spec().format, dst, A,
                              channel, sign, inCenter, outCenter, scale, roi, nthreads);
    return ok;
}

ImageBuf recalc_normal(const ImageBuf& A, uint channel, int sign, float inCenter, float outCenter, float scale, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = recalc_normal(result, A, channel, sign, inCenter, outCenter, scale, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("recalculation error");
    return result;
}