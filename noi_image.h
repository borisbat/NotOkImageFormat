#ifndef NOI_IMAGE_H
#define NOI_IMAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>

#define NOI_KMEANS_NPASS  16

#define NOI_MAGIC   0xBAD0DAB0

#ifdef __cplusplus
extern "C" {
#endif

typedef struct noi_header_s {
  uint32_t magic;
  uint16_t width;
  uint16_t height;
} noi_header_t;

void * noi_compress ( uint8_t * pixels, int w, int h, int * bytes );

// if pixels are NULL, they are allocated
uint8_t * noi_decompress ( void * bytes, int * W, int * H, uint8_t * pixels );

void noi_image_size ( void * bytes, int * W, int * H );

#ifdef __cplusplus
}
#endif

#ifdef NOI_IMAGE_IMPLEMENTATION

void noi_hdt2x2 ( int * a, int * b, int * c, int * d ) {
  int A = *a; int B = *b; int C = *c; int D = *d;
  int aab = A + B;  int sab = A - B;  int acd = C + D;  int scd = C - D;
  *a = aab + acd;  *b = sab + scd;  *c = aab - acd;  *d = sab - scd;
}

void noi_hdt2x2s4 ( int * a, int * b, int * c, int * d ) {
  int A = *a; int B = *b; int C = *c; int D = *d;
  int aab = A + B;  int sab = A - B;  int acd = C + D;  int scd = C - D;
  *a = (aab + acd) >> 4;  *b = (sab + scd) >> 4;  *c = (aab - acd) >> 4;  *d = (sab - scd) >> 4;
}

void noi_hdt4x4 ( int * m ) {
  noi_hdt2x2(m+ 0,m+ 1,m+ 4,m+ 5);  noi_hdt2x2(m+ 2,m+ 3,m+ 6,m+ 7);  noi_hdt2x2(m+ 8,m+ 9,m+12,m+13);  noi_hdt2x2(m+10,m+11,m+14,m+15);
  noi_hdt2x2(m+ 0,m+ 2,m+ 8,m+10);  noi_hdt2x2(m+ 1,m+ 3,m+ 9,m+11);  noi_hdt2x2(m+ 4,m+ 6,m+12,m+14);  noi_hdt2x2(m+ 5,m+ 7,m+13,m+15);
}

void noi_ihdt4x4 ( int * m ) {
  noi_hdt2x2(m+ 0,m+ 1,m+ 4,m+ 5);  noi_hdt2x2(m+ 2,m+ 3,m+ 6,m+ 7);  noi_hdt2x2(m+ 8,m+ 9,m+12,m+13);  noi_hdt2x2(m+10,m+11,m+14,m+15);
  noi_hdt2x2s4(m+ 0,m+ 2,m+ 8,m+10);  noi_hdt2x2s4(m+ 1,m+ 3,m+ 9,m+11);  noi_hdt2x2s4(m+ 4,m+ 6,m+12,m+14);  noi_hdt2x2s4(m+ 5,m+ 7,m+13,m+15);
}

void noi_rgb2yuv ( int r, int g, int b, int * y, int * u, int * v) {
  *y = (( 66*r + 129*g +  25*b + 128)>>8) + 16;
  *u = ((-38*r -  74*g + 112*b + 128)>>8) + 128;
  *v = ((112*r -  94*g -  18*b + 128)>>8) + 128;
}

void noi_yuv2rgb ( int y, int u, int v, int * r, int * g, int * b ) {
  int c = y - 16;  int d = u - 128;  int e = v - 128;
  *r = (298*c + 409*e + 128) >> 8;
  *g = (298*c - 100*d - 209*e + 128) >> 8;
  *b = (298*c + 516*d + 128) >> 8;
}

uint8_t noi_saturate ( int t ) {
  return (t<0) ? 0 : (t>255 ? 255 : t);
}

int noi_dist3 ( int * a, int * b ) {
  int d0 = a[3]  - b[3];  int d1 = a[12] - b[12]; int d2 = a[15] - b[15];
  return d0*d0 + d1*d1 + d2*d2;
}

int noi_dist5 ( int * a, int * b ) {
  int d0 = a[1]  - b[1];  int d1 = a[4]  - b[4];  int d2 = a[5] -  b[5];
  int d3 = a[7] -  b[7];  int d4 = a[13] - b[13];
  return d0*d0 + d1*d1 + d2*d2 + d3*d3 + d4*d4;
}

int noi_dist7 ( int * a, int * b ) {
  int d0 = a[2]  - b[2];  int d1 = a[6]  - b[6];  int d2 = a[8] -  b[8];  int d3 = a[9] -  b[9];
  int d4 = a[10] - b[10]; int d5 = a[11] - b[11]; int d6 = a[14] - b[14];
  return d0*d0 + d1*d1 + d2*d2 + d3*d3 + d4*d4 + d5*d5 + d6*d6;
}

void noi_norm3 ( int * a, int numK ) {
  a[3] /= numK; a[12] /= numK;  a[15] /= numK;
}

void noi_norm5 ( int * a, int numK ) {
  a[1] /= numK; a[4]  /= numK;  a[5] /= numK;  a[7] /= numK; a[13] /= numK;
}

void noi_norm7 ( int * a, int numK ) {
  a[2] /= numK; a[6] /= numK; a[8] /= numK; a[9] /= numK; a[10] /= numK;  a[11] /= numK;  a[14] /= numK;
}
typedef struct noi_kmeans_s {
  int * index;
  int * center;
} noi_kmeans_t;

void noi_free_means ( noi_kmeans_t * km ) {
  free(km->index);
  free(km->center);
}

void noi_kmeans ( noi_kmeans_t * res, int * blocks, int nblocks, int K,
  int  (* dist) (int * o, int * c),
  void (* norm) (int * o, int numK) ) {
  int * index     = (int *) malloc(nblocks*sizeof(int));
  int * center = (int *) malloc(K*16*sizeof(int));
  int * num = (int *) malloc(K*sizeof(int));
  int dim_min[16], dim_max[16];
  for ( int i=1; i!=16; ++i ) {
    dim_min[i] =  INT_MAX;
    dim_max[i] = -INT_MAX;
  }
  for ( int o=0; o!=nblocks; ++o ) {
    for ( int i=1; i!=16; ++i ) {
      int d = blocks[o*16 + i];
      if ( dim_min[i]>d ) dim_min[i] = d;
      if ( dim_max[i]<d ) dim_max[i] = d;
    }
  }
  uint32_t seed = 13;
  for ( int k=0; k!=K; ++k ) {
    for ( int i=1; i!=16; i++ ) {
      seed = 214013*seed+2531011;
      center[k*16+i] = ((int)(seed>>2)) % ( dim_max[i] - dim_min[i] ) + dim_min[i];
    }
  }
  memset(index, 0, nblocks*sizeof(int));
  for ( int iter=0; iter!=NOI_KMEANS_NPASS; ++iter ) {
    for ( int o=0; o!=nblocks; ++o ) {
      int cur_dist = (*dist) ( blocks + o*16, center );
      int cur_index = 0;
      for ( int k=1; k!=K; ++k ) {
        int d = (*dist) ( blocks + o*16, center + k*16 );
        if ( d < cur_dist ) {
          cur_index = k;
          cur_dist = d;
        }
      }
      index[o] = cur_index;
    }
    memset(center, 0, K*16*sizeof(int) );
    memset(num, 0, K*sizeof(int));
    for ( int o=0; o!=nblocks; ++o ) {
      int k = index[o];
      for ( int i=0; i!=16; ++i ) {
        center[k*16 + i] += blocks[o*16 + i];
      }
      num[k] ++;
    }
    for ( int k=0; k!=K; ++k ) {
      int numK = num[k];
      if ( numK ) {
        (*norm) ( center+k*16, numK );
      }
    }
  }
  res->index = index;
  res->center = center;
  free(num);
}

void * noi_compress ( uint8_t * pixels, int w, int h, int * bytes ) {
  int bw = w / 16;
  int bh = h / 16;
  int stride = w * 4;
  int numBlocks = bw*bh*18;
  int * blocks = (int *) malloc ( numBlocks*16*sizeof(int) );
  int * block = blocks;
  for ( int by=0; by!=bh; ++by ) {
    for ( int bx=0; bx!=bw; ++bx ) {
      int * ublock = block; block += 16;
      int * vblock = block; block += 16;
      memset(ublock, 0, 16*sizeof(int));
      memset(vblock, 0, 16*sizeof(int));
      for ( int ty=0; ty!=4; ty++ ) {
        for ( int tx=0; tx!=4; tx++ ) {
          int * yblock = block; block += 16;
          int t = ty*4 + tx;
          for ( int y=0; y!=4; y++ ) {
            for ( int x=0; x!=4; x++ ) {
              int ofs = (by*16+ty*4+y)*stride + (bx*16+tx*4+x)*4;
              int r = pixels[ofs+0];
              int g = pixels[ofs+1];
              int b = pixels[ofs+2];
              int Y, U, V;
              noi_rgb2yuv(r,g,b,&Y,&U,&V);
              yblock[y*4+x] = Y;
              ublock[t] += U;
              vblock[t] += V;
            }
          }
          noi_hdt4x4(yblock);
          ublock[t] /= 16;
          vblock[t] /= 16;
        }
      }
      noi_hdt4x4(ublock);
      noi_hdt4x4(vblock);
    }
  }
  int K = 256;
  noi_kmeans_t res3;
  noi_kmeans(&res3, blocks, numBlocks, K, noi_dist3, noi_norm3);
  noi_kmeans_t res5;
  noi_kmeans(&res5, blocks, numBlocks, K, noi_dist5, noi_norm5);
  noi_kmeans_t res7;
  noi_kmeans(&res7, blocks, numBlocks, K, noi_dist7, noi_norm7);
  int * center = (int *) malloc(16 * K * sizeof(int));
  for ( int k=0; k!=K; ++k ) {
    int * c = center + k*16;
    int * c3 = res3.center + k*16;
    int * c5 = res5.center + k*16;
    int * c7 = res7.center + k*16;
    c[0]  = 0;      c[1]  = c5[1];  c[2]  = c7[2];  c[3]  = c3[3];
    c[4]  = c5[4];  c[5]  = c5[5];  c[6]  = c7[6];  c[7]  = c5[7];
    c[8]  = c7[8];  c[9]  = c7[9];  c[10] = c7[10]; c[11] = c7[11];
    c[12] = c3[12]; c[13] = c5[13]; c[14] = c7[14]; c[15] = c3[15];
  }
  int osize = numBlocks*5 + 256*15*sizeof(int16_t) + sizeof(noi_header_t);
  if ( bytes ) *bytes = osize;
  uint8_t * outbytes = (uint8_t *) malloc(osize);
  uint8_t * out = outbytes;
  noi_header_t * header = (noi_header_t *) out; out += sizeof(noi_header_t);
  header->magic = NOI_MAGIC;
  header->width = w;
  header->height = h;
  for ( int o=0; o!=numBlocks; ++o ) {
    *((uint16_t *)out) = blocks[o*16+0];
    out[2] = res3.index[o];
    out[3] = res5.index[o];
    out[4] = res7.index[o];
    out += 5;
  }
  for ( int k=0; k!=256; ++k ) {
    int16_t * cout = (int16_t *) out; out += 15*sizeof(int16_t);
    for ( int j=1; j!=16; ++j ) {
      cout[j-1] = center[k*16 + j];
    }
  }
  free(center);
  free(blocks);
  noi_free_means(&res3);
  noi_free_means(&res5);
  noi_free_means(&res7);
  return outbytes;
}

void noi_yuv2rgb_block ( int * yblock, int U, int V, uint8_t * pixels, int stride ) {
  for ( int y=0; y!=4; y++ ) {
    for ( int x=0; x!=4; x++ ) {
      int Y = yblock[y*4+x];
      int R, G, B;
      noi_yuv2rgb(Y, U, V, &R, &G, &B);
      pixels[x*4+0] = noi_saturate(R);
      pixels[x*4+1] = noi_saturate(G);
      pixels[x*4+2] = noi_saturate(B);
      pixels[x*4+3] = 255;
    }
    pixels += stride;
  }
}

void noi_decompress_block ( uint8_t * in, uint8_t * pixels, int stride, int16_t * center ) {
  int blocks[16*18];
  int * fb = blocks;
  for ( int o=0; o!=18; ++o ) {
    int16_t * c3 = center + in[2]*15 - 1;
    int16_t * c5 = center + in[3]*15 - 1;
    int16_t * c7 = center + in[4]*15 - 1;
    fb[0] = *((uint16_t *)in);  fb[1] = c5[1];  fb[2] = c7[2];  fb[3] = c3[3];
    fb[4] = c5[4];  fb[5] = c5[5];  fb[6] = c7[6];  fb[7] = c5[7];
    fb[8] = c7[8];  fb[9] = c7[9];  fb[10] = c7[10];  fb[11] = c7[11];
    fb[12] = c3[12];  fb[13] = c5[13];  fb[14] = c7[14];  fb[15] = c3[15];
    noi_ihdt4x4(fb);
    fb += 16;
    in += 5;
  }
  int * ublock = blocks;
  int * vblock = blocks + 16;
  for ( int ty=0; ty!=4; ty++ ) {
    for ( int tx=0; tx!=4; tx++ ) {
      int t = ty*4 + tx;
      int * yblock = blocks + 2*16 + t*16;
      int U = ublock[t];
      int V = vblock[t];
      int ofs = ty*4*stride + tx*4*4;
      noi_yuv2rgb_block(yblock, U, V, pixels + ofs, stride );
    }
  }
}

uint8_t * noi_decompress ( void * bytes, int * W, int * H, uint8_t * pixels ) {
  uint8_t * in = (uint8_t *) bytes;
  noi_header_t * header = (noi_header_t *) in; in += sizeof(noi_header_t);
  if ( header->magic != NOI_MAGIC ) return NULL;
  int w = header->width; if ( W ) *W = w;
  int h = header->height; if ( H ) *H = h;
  int bw = w / 16;
  int bh = h / 16;
  int numBlocks = bw*bh*18;
  if ( !pixels ) pixels = (uint8_t *) malloc(w*h*4);
  int stride = w * 4;
  int16_t * center = (int16_t *)(in + numBlocks * 5);
  for ( int by=0; by!=bh; ++by ) {
    for ( int bx=0; bx!=bw; ++bx ) {
      noi_decompress_block(in, pixels + (bx*16*4) + (by*16)*stride, stride, center );
      in += 18*5;
    }
  }
  return pixels;
}

void noi_image_size ( void * bytes, int * W, int * H ) {
  noi_header_t * header = (noi_header_t *) bytes;
  if ( header->magic != NOI_MAGIC ) return;
  int w = header->width; if ( W ) *W = w;
  int h = header->height; if ( H ) *H = h;
}

#endif

#endif
