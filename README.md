# NotOkImageFormat
Lossy fixed-rate GPU-friendly image compression\decompression ( roughly 8:5 to 1, ~2.8bpp )

Tested on Windows (Windows 10, MSVC 2019 and Clang 12), Mac OSX (12.0 Monterey, Apple Clang 13),
Linux (Ubuntu 20.04LTS, GCC-9).

Some work in progress numbers from my M1 Max 2021 apple laptop:

    bash-3.2$ ./test.sh
    noi_compress 1024 x 1024
    3 mb in 0.20 sec, 15.120mb/sec
    PSNR = -30.1
    running noi_decompressing 100 times, 376328 bytes
    300 mb in 0.20 sec, 1516.2mb/sec

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
* Better PSNR by interpolating U, V - what's currently there is a nearest filter, which is horrible
* expose number of passes for minor improvement in quality. At around 8 passes PSNR goes down 0.1db
* try A:10:9:9 instead of xxxxA:8:8:8 to improve PSNR at the cost of minor speed reduction.

![imgonline-com-ua-twotoone-IcPxr5nwSyJZrzh](https://user-images.githubusercontent.com/272689/144298283-cecd62d5-c9e9-42c1-a7a7-e6b1589c8bb8.png)
