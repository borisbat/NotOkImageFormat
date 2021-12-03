# NotOkImageFormat
Lossy fixed-rate GPU-friendly image compression\decompression ( roughly 8:5 to 1, ~2.8bpp )

Tested on Windows (Windows 10, MSVC 2019 and Clang 12), Mac OSX (12.0 Monterey, Apple Clang 13),
Linux (Ubuntu 20.04LTS, GCC-9).

Some work in progress numbers from my M1 Max 2021 apple laptop:

    bash-3.2$ ./test_lenna.sh
    noi_compress 512 x 512
    0 mb in 0.25 sec, 3.033mb/sec
    PSNR = -31.6
    running noi_decompressing 100 times, 110600 bytes
    75 mb in 0.05 sec, 1444.3mb/sec

I finally got to implement this really old idea of mine, of combining a quantizer with Hadamard transform.

This is how compression works:

1. RGB->YUV color conversion, 4:1:1
2. 4x4 HDT
3. combined weight (0,0) is stored as is. there are 4 bits there which can be used for something
4. 'corners' of size 3, 5, and 7 of the 4x4 matrix are quantized with k-mean quantizer - down to 256 means
5. as the result we have 5 byte blocks - 2 bytes for weight, and 3 quantization indices
6. index pallete is stored as 256 entries of 15 2-byte coefficients

This is what happens during decompression:

1. original blocks are restored from the 3 palette
2. 4x4 iHDT
3. YUV->RGB

NOI is really fast to decompress, even on the CPU. GPU is probably fast enough to decompress as it textures.

Compression can be speed-up significantly with better k-means implementation. However I would not want to waste any time on it. This really ought to be a shader. GPU implementation of k-means would be crazy fast and completely parallel.

Future work (in no particular order)

* GPU implementation
* better PSNR by interpolating U, V - what's currently there is a nearest filter, which is horrible
* expose number of passes for minor improvement in quality. At around 8 passes PSNR goes down 0.1db

![imgonline-com-ua-twotoone-yWptqOdrL5MhWSdx](https://user-images.githubusercontent.com/272689/144541848-c6ffc010-7475-4ca7-8fb5-300776a08e68.png)
