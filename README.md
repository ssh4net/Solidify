![Solidify](https://github.com/ssh4net/Solidify/assets/3924000/c3d297bd-24e7-4de2-93c7-5c8d74c9767d)

# Solidify
Small GUI utility on top of OpenImageIO and QT5 to Solidify (fill, push-pull) empty areas in textures.

Usage
------------

Run Solidify.exe and drag&drop images into open window.
Check console to progress information.

![Screenshot](https://github.com/ssh4net/Solidify/assets/3924000/3b8562f6-ca73-49f6-a3b1-b9e1f4cbc8ac)

Tool expecting RGBA image file. Output files will be write in same folder as a source files with a name **Source_file_name_fill.ext** as RGB without alpha channel.

From v1.1 possible to use external alpha channel as a single channel file with **_mask.ext** or **_alpha.ext** in name (case insencitive).
Tool will read external alpha channel file only once per batch.

![Solidify 1 1](https://github.com/ssh4net/Solidify/assets/3924000/24dc9382-e554-44d0-8ed1-2465752a4752)

Dependencies
------------

### Required dependencies
* OpenImageIO
* QT5

Changelog
---------
* 1.1 - External alpha/mask channel support. Drag&Drop window always on top.
* 1.0 - Initial release

License
-------

Copyright © 2023 Erium Vladlen.

Solidify is licensed under the GNU General Public License, Version 3.
Individual files may have a different, but compatible license.
