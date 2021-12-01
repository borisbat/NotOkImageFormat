# NotOkImageFormat
Lossy fixed-rate GPU-friendly image compression\decompression ( roughly 8:5 to 1 )

I finally got to implement this really old idea of mine, of combining a quantizer with Hadamard transform.
This is whats going on

To compress

1. RGB->YUV color conversion, 4:1:1
2. 4x4 HDT
3. summ weight (0,0) is stored as is. there are 4 bits there which can be used for soemthing
4. 'corners' of size 3, 5, and 7 of the 4x4 matrix are quantized with k-mean quantizer - down to 256 means
5. as the result we have 5 byte blocks - 2 bytes for weight, and 3 quantization indices
6. index pallete is stored as 256 entries of 15 2-byte koefficients

To decompress

1. original blocks are restored from the 3 palette
2. 4x4 iHDT
3. YUV->RGB

NOI is really fast to decompress, even on the CPU. GPU is probably fast enough to decompress as it textures.

Compression can be speed-up siginficantly with better k-means implementation. However I would not want to waste any time on it. This really ought to be a shader. GPU implementation of k-means would be crazy fast and completely parallel.

