# NotOkImageFormat
Lossy fixed-rate GPU-friendly image compression\decompression.
Supported profiles

    16:1:1     2.8125 bpp       yuv
    4:1:1      3.75 bpp         yuv
    2:1:1      5.0 bpp          yuv
    1:1:1      7.5 bpp          rgb

Tested on Windows (Windows 10, MSVC 2019 and Clang 12), Mac OSX (12.0 Monterey, Apple Clang 13),
Linux (Ubuntu 20.04LTS, GCC-9).

Currently 3.5-4.5 times faster decompression than STBI JPEG implementation, with a lot of potential to optimize.

Some work in progress numbers from my M1 Max 2021 apple laptop:

    noi_compress 512 x 512 profile YUV_16_1_1
    0 mb in 0.28 sec, 2.697mb/sec
    PSNR = -31.6   PSNR(YUV) = -37.3
    running noi_decompressing 100 times, 110600 bytes
    decompression speed 1609.4mb/sec

    noi_compress 512 x 512 profile YUV_4_1_1
    0 mb in 0.34 sec, 2.196mb/sec
    PSNR = -32.9   PSNR(YUV) = -38.6
    running noi_decompressing 100 times, 141320 bytes
    decompression speed 1148.5mb/sec

    noi_compress 512 x 512 profile YUV_2_1_1
    0 mb in 0.44 sec, 1.686mb/sec
    PSNR = -33.4   PSNR(YUV) = -39.2
    running noi_decompressing 100 times, 182280 bytes
    decompression speed 1116.1mb/sec

    noi_compress 512 x 512 profile RGB_1_1_1
    0 mb in 0.64 sec, 1.175mb/sec
    PSNR = -35.8   PSNR(YUV) = -40.9
    running noi_decompressing 100 times, 264200 bytes
    decompression speed 1239.7mb/sec

    bash-3.2$ ../bin/noi -stbjpg lenna.png lenna.jpg
    running stbi_load_from_memory 100 times, 68593 bytes
    decompression speed 354.3mb/sec

I finally got to implement this really old idea of mine, of combining a quantizer with Hadamard transform.

This is how compression works:

1. RGB->YUV color conversion for the YUV profiles
2. 4x4 HDT
3. combined weight (0,0) is stored as is. there are 4 bits there which can be used for something
4. 'corners' of size 3, 5, and 7 of the 4x4 matrix are quantized with k-mean quantizer - down to 256 means
5. as the result we have 5 byte blocks - 2 bytes for weight, and 3 quantization indices
6. index pallet is stored as 256 entries of 15 2-byte coefficients

This is what happens during decompression:

1. original blocks are restored from the 3 palette
2. 4x4 iHDT
3. YUV->RGB for the YUV profiles

NOI is really fast to decompress, even on the CPU. GPU is probably fast enough to decompress as it textures.

Compression can be speed-up significantly with better k-means implementation. However I would not want to waste any time on it. This really ought to be a shader. GPU implementation of k-means would be crazy fast and completely parallel.

Future work (in no particular order)

* GPU implementation
* better PSNR by interpolating U, V - what's currently there is a nearest filter, which is horrible
* expose number of passes for minor improvement in quality. At around 8 passes PSNR goes down 0.1db

[Kodak dataset numbers](https://docs.google.com/spreadsheets/d/e/2PACX-1vROIuXdb9BQB0Gem7Pn0q9Y4heimPg6y8xvhhnJ1Cgaqr1qaJ4LmQsBXUk4pBaG7HcME4SPS2JNNUb2/pubhtml?gid=1381620930&single=true)

        YUV_16_1_1  PSNR=-31.6db	PSNR(YUV)=-37.6db
        YUV_4_1_1   PSNR=-32.5db	PSNR(YUV)=-38.5db
        YUV_2_1_1   PSNR=-32.8db	PSNR(YUV)=-38.9db
        RGB_1_1_1   PSNR=-33.8db    PSNR(YUV)=-39.5db

top left - original, top right - 1:1:1, bottom left 4:1:1, bottom right - 16:1:1
![lenna_tlorig_tr111_bl411_br1611](https://user-images.githubusercontent.com/272689/144662522-33efda8d-6ede-45ae-960c-f0ee93a3fa2b.png)
