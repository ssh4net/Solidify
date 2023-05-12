// 
//

#include <iostream>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

using namespace OIIO;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " INPUT OUTPUT\n";
        return 1;
    }

    const char* input_filename = argv[1];
    const char* output_filename = argv[2];

    // Create an ImageBuf object for the input file
    ImageBuf input_buf(input_filename); 
    // Create an ImageBuf object to store the result
    ImageBuf result_buf;

    // Call fillholes_pushpull
    bool ok = ImageBufAlgo::fillholes_pushpull(result_buf, input_buf);

    if (!ok) {
        std::cerr << "Error: " << result_buf.geterror() << std::endl;
        return 1;
    }

    // Write the result to the output file
    result_buf.set_write_format(input_buf.spec().format); // Preserve original bit depth
    result_buf.write(output_filename, "tiff"); // Write out as TIFF

    return 0;
}