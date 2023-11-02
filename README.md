![Solidify](https://github.com/ssh4net/Solidify/assets/3924000/c3d297bd-24e7-4de2-93c7-5c8d74c9767d)

# Solidify
Small GUI utility on top of OpenImageIO and QT5 to Solidify (fill, push-pull) empty areas in textures.
Can be used to Normalize normal maps and export alpha/mask channels as single-channel Grayscale images.
Possible to use to unpack/repair the third channel for packed two-channel tangent space normals.

Usage
------------
On start, set desired app settings would be used in the current batch.

![Solidify138_1](https://github.com/ssh4net/Solidify/assets/3924000/33f41b89-0a97-48fd-93f9-a3e9113e7887) ![Solidify138_2](https://github.com/ssh4net/Solidify/assets/3924000/716fcf13-2ae9-451e-a0f4-afd221326ca8)

Run Solidify.exe and drag and drop images into the open window.
Check the console for progress information.

![Screenshot](https://github.com/ssh4net/Solidify/assets/3924000/3b8562f6-ca73-49f6-a3b1-b9e1f4cbc8ac)

Tool expecting RGBA or Grayscale with the alpha channel image file. Output files will be written in the same folder as source files with the name **Source_file_name_fill.ext** as RGB without an alpha channel.

From v1.2 possible to use the external alpha channel as a single channel file with **_mask.ext** or **_alpha.ext** in name (case insencitive).

![Solidify 1 1](https://github.com/ssh4net/Solidify/assets/3924000/24dc9382-e554-44d0-8ed1-2465752a4752)

Supporting for RGB and Grayscale images only with external alpha/mask image files.

![Solidify 1 2](https://github.com/ssh4net/Solidify/assets/3924000/7405f944-59f5-452c-ba9c-aafd7f96c2d7)

To optimize performance Solidify app reads external alpha channel file once per batch.

Dependencies
------------

### Required dependencies
* OpenImageIO
* QT5

Changelog
---------
* 1.38 - Temporary fix for Libtiff crash on Exif:LensInformation
*      - Export Alpha/Mask channel
*      - Repair or Recompute packed normals (usually B or Z channel in RGB (XYZ) )
* 1.30 - Now Solidify is a GUI app by default
* 1.21 - Bit depth when using external alpha/mask fix
* 1.2  - Grayscale textures fix
* 1.1  - External alpha/mask channel support. Drag&Drop window always on top
* 1.0  - Initial release

License
-------

Copyright © 2023 Erium Vladlen.

Solidify is licensed under the GNU General Public License, Version 3.
Individual files may have a different, but compatible license.
