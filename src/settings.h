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
	uint normMode;
	uint rngMode;
	int fileFormat;
	int bitDepth;

	const std::string normNames[4] = {"normal", "tangent", "object", "world"};

	Settings() {
		isSolidify = true; // Solidify enabled/disabled
		normMode = 1;      // Normalize mode: 0 - disabled, 1 - smart, 2 - force
		rngMode = 0;       // Float type: 0 - unsigned, 1 - signed
		fileFormat = 0;    // File format: -1 - original, 0 - TIFF, 1 - OpenEXR, 2 - PNG, 3 - JPEG, 4 - JPEG-2000
		bitDepth = 0;      // Bit depth: -1 - Original, 0 - uint8, 1 - uint16, 2 - uint32, 3 - uint64, 4 - half, 5 - float, 6 - double
	}
};

extern Settings settings;

#endif