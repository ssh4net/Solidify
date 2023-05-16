![Solidify](https://github.com/ssh4net/Solidify/assets/3924000/c3d297bd-24e7-4de2-93c7-5c8d74c9767d)

# Solidify
Small GUI utility on top of OpenImageIO and QT5 to Solidify (fill, push-pull) empty areas in textures.

Usage
------------

Run Solidify.exe and drag&drop images into open window.
Check console to progress information.

![Screenshot](https://github.com/ssh4net/Solidify/assets/3924000/3b8562f6-ca73-49f6-a3b1-b9e1f4cbc8ac)

Tool expecting RGBA or Grayscale with alpha channel image file. Output files will be write in same folder as a source files with a name **Source_file_name_fill.ext** as RGB without alpha channel.

From v1.2 possible to use external alpha channel as a single channel file with **_mask.ext** or **_alpha.ext** in name (case insencitive).

![Solidify 1 1](https://github.com/ssh4net/Solidify/assets/3924000/24dc9382-e554-44d0-8ed1-2465752a4752)

Supporting for RGB and Grayscale images only with external alpha/mask image file.

![Solidify 1 2](https://github.com/ssh4net/Solidify/assets/3924000/7405f944-59f5-452c-ba9c-aafd7f96c2d7)

Too optimize performance Solidify app reading external alpha channel file once per batch.

Dependencies
------------

### Required dependencies
* OpenImageIO
* QT5

Changelog
---------
* 1.2 - Grayscale textures fix
* 1.1 - External alpha/mask channel support. Drag&Drop window always on top
* 1.0 - Initial release

License
-------

Copyright © 2023 Erium Vladlen.

Solidify is licensed under the GNU General Public License, Version 3.
Individual files may have a different, but compatible license.
