#pragma once

#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;

struct Settings {
	bool isSolidify;
	bool expAlpha;
	uint normMode;
	uint rngMode;
	int fileFormat;
	int bitDepth;
	int rawRot;

	const std::string normNames[4] = {"normal", "tangent", "object", "world"};
	const std::string mask_substr[4] = { "_mask.", "_mask_", "_alpha.", "_alpha_" };
	const std::string out_formats[6] = {"tif", "exr", "png", "jpg", "jp2", "ppm"};
	const int raw_rot[5] = { -1, 0, 3, 5, 6 }; // -1 - Auto EXIF, 0 - Unrotated/Horisontal, 3 - 180 Horisontal, 5 - 90 CW Vertical, 6 - 90 CCW Vertical

	Settings() {
		isSolidify = true; // Solidify enabled/disabled
		expAlpha = false;   // Export alpha channel
		normMode = 1;      // Normalize mode: 0 - disabled, 1 - smart, 2 - force
		rngMode = 0;       // Float type: 0 - unsigned, 1 - signed
		fileFormat = -1;    // File format: -1 - original, 0 - TIFF, 1 - OpenEXR, 2 - PNG, 3 - JPEG, 4 - JPEG-2000, 5 - PPM
		bitDepth = -1;      // Bit depth: -1 - Original, 0 - uint8, 1 - uint16, 2 - uint32, 3 - uint64, 4 - half, 5 - float, 6 - double
		rawRot = -1;		// Raw rotation: -1 - Auto EXIF, 0 - Unrotated/Horisontal, 3 - 180 Horisontal, 5 - 90 CW Vertical, 6 - 90 CCW Vertical
	}
	// get bit depth in bytes
	int getBitDepth() {
		switch (bitDepth) {
		case 0:
			return 1;
		case 1:
			return 2;
		case 2:
			return 4;
		case 3:
			return 8;
		case 4:
			return 2;
		case 5:
			return 4;
		case 6:
			return 8;
		}
		return 0;
	}
};

extern Settings settings;

#endif