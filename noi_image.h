// NotOkImageFormat  revision  0
//    NOI_THREADS               - when defined, compession will use posix (or windows) threads
//    NOI_IMAGE_IMPLEMENTATION  - when defined, this also becomes implementation

#ifndef NOI_IMAGE_H
#define NOI_IMAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>

#define NOI_KMEANS_NPASS  32      // this controls compression quality. 100 is excessive? 8 goes down, 32 is excellent
#define NOI_MAX_THREADS   32      // when NOI_THREADS is defined, this is max number of threads
#define NOI_MAGIC   0xBAD0

#define NOI_YUV_16_1_1      18        // YUV 16:1:1
#define NOI_YUV_4_1_1       6         // YUV 4:1:1
#define NOI_YUV_2_1_1       4         // YUV 2:1:1
#define NOI_RGB_1_1_1       3         // RGB 1:1:1
#define NOI_Y_1_0_0         1         // Y   1:0:0    greyscale

#define NOI_K3    1024
#define NOI_K5    512
#define NOI_K7    512

#ifdef __cplusplus
extern "C" {
#endif

typedef struct noi_header_s {     // NOI file header
  uint16_t magic;
  uint16_t profile;
  uint16_t width;
  uint16_t height;
} noi_header_t;

void * noi_compress ( uint8_t * pixels, int w, int h, int * bytes, int profile ); // profile is NOI_YUV_16_1_1, etc
uint8_t * noi_decompress ( void * bytes, int * W, int * H, uint8_t * pixels );  // if pixels are NULL, they are allocated
void noi_image_size ( void * bytes, int * W, int * H );
const char * noi_profile_name ( int profile );
void noi_profile_block_size ( int profile, int * bx, int * by );

#ifdef __cplusplus
}
#endif

#ifdef NOI_IMAGE_IMPLEMENTATION

void noi_hdt2x2 ( int * a, int * b, int * c, int * d ) {
  int A = *a; int B = *b; int C = *c; int D = *d;
  int aab = A + B;  int sab = A - B;  int acd = C + D;  int scd = C - D;
  *a = aab + acd;  *b = sab + scd;  *c = aab - acd;  *d = sab - scd;
}

void noi_hdt4x4 ( int * m ) {
  noi_hdt2x2(m+ 0,m+ 1,m+ 4,m+ 5);  noi_hdt2x2(m+ 2,m+ 3,m+ 6,m+ 7);  noi_hdt2x2(m+ 8,m+ 9,m+12,m+13);  noi_hdt2x2(m+10,m+11,m+14,m+15);
  noi_hdt2x2(m+ 0,m+ 2,m+ 8,m+10);  noi_hdt2x2(m+ 1,m+ 3,m+ 9,m+11);  noi_hdt2x2(m+ 4,m+ 6,m+12,m+14);  noi_hdt2x2(m+ 5,m+ 7,m+13,m+15);
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

// 0, 1, 2, 3 ->    0   2   1   3

/*
   0   2   1   3
   8  10   9  11
   4   6   5   7
  12  14  13  15
*/

#define NOI_B3_0    2
#define NOI_B3_1    8
#define NOI_B3_2    10

#define NOI_B5_0    1
#define NOI_B5_1    4
#define NOI_B5_2    5
#define NOI_B5_3    6
#define NOI_B5_4    9

#define NOI_B7_0    3
#define NOI_B7_1    7
#define NOI_B7_2    11
#define NOI_B7_3    12
#define NOI_B7_4    13
#define NOI_B7_5    14
#define NOI_B7_6    15

int noi_dist3 ( int * a, int * b ) {
  int d0 = a[NOI_B3_0] - b[NOI_B3_0];  int d1 = a[NOI_B3_1] - b[NOI_B3_1];  int d2 = a[NOI_B3_2] - b[NOI_B3_2];
  return d0*d0 + d1*d1 + d2*d2;
}

int noi_dist5 ( int * a, int * b ) {
  int d0 = a[NOI_B5_0] - b[NOI_B5_0];  int d1 = a[NOI_B5_1] - b[NOI_B5_1];  int d2 = a[NOI_B5_2] - b[NOI_B5_2];
  int d3 = a[NOI_B5_3] - b[NOI_B5_3];  int d4 = a[NOI_B5_4] - b[NOI_B5_4];
  return d0*d0 + d1*d1 + d2*d2 + d3*d3 + d4*d4;
}

int noi_dist7 ( int * a, int * b ) {
  int d0 = a[NOI_B7_0] - b[NOI_B7_0];  int d1 = a[NOI_B7_1] - b[NOI_B7_1];  int d2 = a[NOI_B7_2] - b[NOI_B7_2];
  int d3 = a[NOI_B7_3] - b[NOI_B7_3];  int d4 = a[NOI_B7_4] - b[NOI_B7_4];  int d5 = a[NOI_B7_5] - b[NOI_B7_5];
  int d6 = a[NOI_B7_6] - b[NOI_B7_6];
  return d0*d0 + d1*d1 + d2*d2 + d3*d3 + d4*d4 + d5*d5 + d6*d6;
}

typedef struct noi_kmeans_s {
  int * index;
  int * center;
} noi_kmeans_t;

void noi_free_means ( noi_kmeans_t * km ) {
  free(km->index);
  free(km->center);
}

typedef struct noi_kmeans_thread_s {
  int * blocks;
  int   nblocks;
  int * index;
  int * center;
  int   K;
  int   ( *dist) ( int *, int * );
} noi_kmeans_thread_t;

void noi_kmeans_single_thread ( noi_kmeans_thread_t * nkt ) {
  int * blocks = nkt->blocks;
  int nblocks = nkt->nblocks;
  int ( *dist ) ( int *, int * ) = nkt->dist;
  int * index = nkt->index;
  int * center = nkt->center;
  int K = nkt->K;
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
}

#ifdef NOI_THREADS
  #ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    DWORD WINAPI noi_kmeans_thread ( void * _nkt ) {
      noi_kmeans_thread_t * nkt = (noi_kmeans_thread_t *) _nkt;
      noi_kmeans_single_thread(nkt);
      return 0;
    }
  #else
    #include <unistd.h>
    #include <pthread.h>
    void * noi_kmeans_thread ( void * _nkt ) {
      noi_kmeans_thread_t * nkt = (noi_kmeans_thread_t *) _nkt;
      noi_kmeans_single_thread(nkt);
      return NULL;
    }
  #endif
#endif

void noi_kmeans ( noi_kmeans_t * res, int * blocks, int nblocks, int K, int  (* dist) (int * o, int * c) ) {
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
      if ( dim_min[i] != dim_max[i] ) {
        center[k*16+i] = ((int)(seed>>2)) % ( dim_max[i] - dim_min[i] ) + dim_min[i];
      } else {
        center[k*16+i] = dim_min[i];
      }
    }
  }
  memset(index, 0, nblocks*sizeof(int));
  #ifdef NOI_THREADS
    #if _WIN32
      SYSTEM_INFO sysinfo;
      GetSystemInfo(&sysinfo);
      int nth = sysinfo.dwNumberOfProcessors - 2;
    #else
      int nth = sysconf(_SC_NPROCESSORS_ONLN) - 2;
    #endif
    nth = nth<=0 ? 1 : (nth>NOI_MAX_THREADS ? NOI_MAX_THREADS : nth);
    noi_kmeans_thread_t tinfo[NOI_MAX_THREADS];
    int bpt = nblocks / nth;
    for ( int t=0; t!=nth; ++t ) {
      tinfo[t] = (noi_kmeans_thread_t ) { blocks+16*t*bpt,bpt,index+t*bpt,center,K,dist };
    }
    tinfo[nth-1].nblocks = nblocks - (nth-1)*bpt;
  #endif
  for ( int iter=0; iter!=NOI_KMEANS_NPASS; ++iter ) {
    #ifdef NOI_THREADS
      #ifdef _WIN32
        HANDLE hth[NOI_MAX_THREADS];
        for ( int t=1; t!=nth; ++t ) {
          hth[t] = CreateThread(NULL, 0, noi_kmeans_thread, tinfo + t, 0, NULL);
        }
        noi_kmeans_single_thread(tinfo);
        WaitForMultipleObjects(nth-1,hth+1,TRUE,INFINITE);
      #else
        pthread_t hth[NOI_MAX_THREADS];
        for ( int t=1; t!=nth; ++t ) {
          pthread_create(&hth[t], NULL, noi_kmeans_thread, tinfo + t);
        }
        noi_kmeans_single_thread(tinfo);
        for ( int t=1; t!=nth; ++t ) {
          pthread_join(hth[t], NULL);
        }
      #endif
    #else
      noi_kmeans_thread_t nkt = (noi_kmeans_thread_t ) { blocks,nblocks,index,center,K,dist };
      noi_kmeans_single_thread(&nkt);
    #endif
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
        for ( int i=1; i!=16; ++i ) {
          center[k*16 + i] /= numK;
        }
      }
    }
  }
  res->index = index;
  res->center = center;
  free(num);
}

int * noi_compress_yuv ( int * nblocks, uint8_t * pixels, int w, int h, int profile ) {
  int psx, pxshift, psy, pyshift;
  switch ( profile ) {
    case NOI_YUV_16_1_1:  psx = psy = 4; pxshift = pyshift = 2; break;
    case NOI_YUV_4_1_1:   psx = psy = 2; pxshift = pyshift = 1; break;
    case NOI_YUV_2_1_1:   psx = 2; psy = 1; pxshift = 1; pyshift = 0; break;
  }
  int bsx = psx * 4;
  int bsy = psy * 4;
  int bw = w / bsx;
  int bh = h / bsy;
  int stride = w * 4;
  int numBlocks = bw*bh*profile; *nblocks = numBlocks;
  int * blocks = (int *) malloc ( numBlocks*16*sizeof(int) );
  int * block = blocks;
  for ( int by=0; by!=bh; ++by ) {
    for ( int bx=0; bx!=bw; ++bx ) {
      int * ublock = block; block += 16;
      int * vblock = block; block += 16;
      memset(ublock, 0, 16*sizeof(int));
      memset(vblock, 0, 16*sizeof(int));
      for ( int ty=0; ty!=psy; ty++ ) {
        for ( int tx=0; tx!=psx; tx++ ) {
          int * yblock = block; block += 16;
          for ( int y=0; y!=4; y++ ) {
            for ( int x=0; x!=4; x++ ) {
              int uvy = ty*4+y;
              int uvx = tx*4+x;
              int ofs = (by*bsy+uvy)*stride + (bx*bsx+uvx)*4;
              int r = pixels[ofs+0];
              int g = pixels[ofs+1];
              int b = pixels[ofs+2];
              int Y, U, V;
              noi_rgb2yuv(r,g,b,&Y,&U,&V);
              yblock[y*4+x] = Y;
              int t = (uvy>>pyshift)*4 + (uvx>>pxshift);
              ublock[t] += U;
              vblock[t] += V;
            }
          }
          noi_hdt4x4(yblock);
        }
      }
      for ( int t=0; t!=16; ++t ) {
          ublock[t] /= (psy*psx);
          vblock[t] /= (psy*psx);
      }
      noi_hdt4x4(ublock);
      noi_hdt4x4(vblock);
    }
  }
  return blocks;
}

int * noi_compress_rgb ( int * nblocks, uint8_t * pixels, int w, int h, int profile ) {
  int bw = w / 4;
  int bh = h / 4;
  int stride = w * 4;
  int numBlocks = bw*bh*profile; *nblocks = numBlocks;
  int * blocks = (int *) malloc ( numBlocks*16*sizeof(int) );
  int * block = blocks;
  for ( int by=0; by!=bh; ++by ) {
    for ( int bx=0; bx!=bw; ++bx ) {
      int * rblock = block; block += 16;
      int * gblock = block; block += 16;
      int * bblock = block; block += 16;
      for ( int y=0; y!=4; y++ ) {
        for ( int x=0; x!=4; x++ ) {
            int ofs = (by*4+y)*stride + (bx*4+x)*4;
            int t = y*4 + x;
            rblock[t] = pixels[ofs+0];
            gblock[t] = pixels[ofs+1];
            bblock[t] = pixels[ofs+2];
        }
      }
      noi_hdt4x4(rblock);
      noi_hdt4x4(gblock);
      noi_hdt4x4(bblock);
    }
  }
  return blocks;
}

int * noi_compress_greyscale ( int * nblocks, uint8_t * pixels, int w, int h, int profile ) {
  int bw = w / 4;
  int bh = h / 4;
  int stride = w * 4;
  int numBlocks = bw*bh*profile; *nblocks = numBlocks;
  int * blocks = (int *) malloc ( numBlocks*16*sizeof(int) );
  int * block = blocks;
  for ( int by=0; by!=bh; ++by ) {
    for ( int bx=0; bx!=bw; ++bx ) {
      for ( int y=0; y!=4; y++ ) {
        for ( int x=0; x!=4; x++ ) {
            int ofs = (by*4+y)*stride + (bx*4+x)*4;
            int t = y*4 + x;
            int R = pixels[ofs+0];
            int G = pixels[ofs+1];
            int B = pixels[ofs+2];
            block[t] = (int)(0.299*R + 0.587*G + 0.114*B);
        }
      }
      noi_hdt4x4(block);
      block += 16;
    }
  }
  return blocks;
}

void * noi_compress ( uint8_t * pixels, int w, int h, int * bytes, int profile ) {
  int numBlocks; int * blocks = NULL;
  switch ( profile ) {
    case NOI_YUV_16_1_1: case NOI_YUV_4_1_1: case NOI_YUV_2_1_1:
      blocks = noi_compress_yuv(&numBlocks, pixels, w, h, profile); break;
    case NOI_RGB_1_1_1:
      blocks = noi_compress_rgb(&numBlocks, pixels, w, h, profile); break;
    case NOI_Y_1_0_0:
      blocks = noi_compress_greyscale(&numBlocks, pixels, w, h, profile); break;
    default:
      return NULL;
  }
  noi_kmeans_t res3;
  noi_kmeans(&res3, blocks, numBlocks, NOI_K3, noi_dist3);
  noi_kmeans_t res5;
  noi_kmeans(&res5, blocks, numBlocks, NOI_K5, noi_dist5);
  noi_kmeans_t res7;
  noi_kmeans(&res7, blocks, numBlocks, NOI_K7, noi_dist7);
  int osize = numBlocks*5 + NOI_K3*3*sizeof(int16_t) + NOI_K5*5*sizeof(int16_t) + NOI_K7*7*sizeof(int16_t) + sizeof(noi_header_t);
  if ( bytes ) *bytes = osize;
  uint8_t * outbytes = (uint8_t *) malloc(osize);
  uint8_t * out = outbytes;
  noi_header_t * header = (noi_header_t *) out; out += sizeof(noi_header_t);
  *header = (noi_header_t) { NOI_MAGIC, profile, w, h };
  for ( int o=0; o!=numBlocks; ++o ) {
    int i0 = blocks[o*16+0] & 4095;
    int i3 = res3.index[o];
    int i5 = res5.index[o];
    int i7 = res7.index[o];
    *((uint16_t *)out) = i0 | ((i7&0x100)<<4) | ((i5&0x100)<<5) | ((i3&0x300)<<6);
    out[2] = i3;
    out[3] = i5;
    out[4] = i7;
    out += 5;
  }
  for ( int k=0; k!=NOI_K3; ++k ) {
    int16_t * B = (int16_t *) out; out += 3*sizeof(int16_t);
    int * c3 = res3.center + k*16;
    B[0] = c3[NOI_B3_0]; B[1] = c3[NOI_B3_1]; B[2] = c3[NOI_B3_2];
  }
  for ( int k=0; k!=NOI_K5; ++k ) {
    int16_t * B = (int16_t *) out; out += 5*sizeof(int16_t);
    int * c5 = res5.center + k*16;
    B[0] = c5[NOI_B5_0]; B[1] = c5[NOI_B5_1]; B[2] = c5[NOI_B5_2]; B[3] = c5[NOI_B5_3]; B[4] = c5[NOI_B5_4];
  }
  for ( int k=0; k!=NOI_K7; ++k ) {
    int16_t * B = (int16_t *) out; out += 7*sizeof(int16_t);
    int * c7 = res7.center + k*16;
    B[0] = c7[NOI_B7_0]; B[1] = c7[NOI_B7_1]; B[2] = c7[NOI_B7_2]; B[3] = c7[NOI_B7_3]; B[4] = c7[NOI_B7_4]; B[5] = c7[NOI_B7_5]; B[6] = c7[NOI_B7_6];
  }
  free(blocks);
  noi_free_means(&res3);
  noi_free_means(&res5);
  noi_free_means(&res7);
  return outbytes;
}

#define NOI_HDT2X2(a,b,c,d) \
  aab = a + b;  sab = a - b;  acd = c + d;  scd = c - d;  \
  a = aab + acd;  b = sab + scd; c = aab - acd; d = sab - scd;

#define NOI_HDT2X2S(a,b,c,d) \
  aab = a + b;  sab = a - b;  acd = c + d;  scd = c - d;  \
  a = (aab + acd)>>4;  b = (sab + scd)>>4; c = (aab - acd)>>4; d = (sab - scd)>>4;

void noi_decompress_5_16 ( uint8_t * in, int * blocks, int16_t * center3, int profile ) {
  int16_t * center5 = center3 + NOI_K3*3;
  int16_t * center7 = center5 + NOI_K5*5;
  int * fb = blocks;
  for ( int o=0; o!=profile; ++o ) {
    int in0 = *((uint16_t *)in);
    int in3 = in[2] + ((in0 & 0xC000)>>6);
    int in5 = in[3] + ((in0 & 0x2000)>>5);
    int in7 = in[4] + ((in0 & 0x1000)>>4);
    int16_t * c3 = center3 + in3*3;
    int16_t * c5 = center5 + in5*5;
    int16_t * c7 = center7 + in7*7;
    int a00 = in0 & 4095;
    int a02 = c3[0];  int a20 = c3[1];  int a22 = c3[2];
    int a01 = c5[0];  int a10 = c5[1];  int a11 = c5[2];  int a12 = c5[3];  int a21 = c5[4];
    int a03 = c7[0];  int a13 = c7[1];  int a23 = c7[2];  int a30 = c7[3];  int a31 = c7[4];  int a32 = c7[5];  int a33 = c7[6];
    int aab, sab, acd, scd;
    NOI_HDT2X2(a00,a01,a10,a11);    NOI_HDT2X2(a02,a03,a12,a13);    NOI_HDT2X2(a20,a21,a30,a31);    NOI_HDT2X2(a22,a23,a32,a33);
    NOI_HDT2X2S(a00,a02,a20,a22);   NOI_HDT2X2S(a01,a03,a21,a23);   NOI_HDT2X2S(a10,a12,a30,a32);   NOI_HDT2X2S(a11,a13,a31,a33);
    fb[0]  = a00; fb[1]  = a01;  fb[2]  = a02; fb[3]  = a03;
    fb[4]  = a10; fb[5]  = a11;  fb[6]  = a12; fb[7]  = a13;
    fb[8]  = a20; fb[9]  = a21;  fb[10] = a22; fb[11] = a23;
    fb[12] = a30; fb[13] = a31;  fb[14] = a32; fb[15] = a33;
    fb += 16;
    in += 5;
  }
}

#define NOI_YUV2RGB(ofsx,AA) \
      C = 298*(AA-16); \
      bpixels[ofsx*4+0] = noi_saturate((C+DR) >> 8); \
      bpixels[ofsx*4+1] = noi_saturate((C+DG) >> 8); \
      bpixels[ofsx*4+2] = noi_saturate((C+DB) >> 8); \
      bpixels[ofsx*4+3] = 255;

void noi_convert_colors_YUV_16_1_1 ( uint8_t * in, int16_t * center3, int profile, uint8_t * pixels, int stride ) {
  int blocks[16*2];
  noi_decompress_5_16(in, blocks, center3, 2 );
  int * ublock = blocks;
  int * vblock = blocks + 16;
  int16_t * center5 = center3 + NOI_K3*3;
  int16_t * center7 = center5 + NOI_K5*5;
  in += 10;
  for ( int ty=0; ty!=4; ty++ ) {
    for ( int tx=0; tx!=4; tx++ ) {
      int in0 = *((uint16_t *)in);
      int in3 = in[2] + ((in0 & 0xC000)>>6);
      int in5 = in[3] + ((in0 & 0x2000)>>5);
      int in7 = in[4] + ((in0 & 0x1000)>>4);
      int16_t * c3 = center3 + in3*3;
      int16_t * c5 = center5 + in5*5;
      int16_t * c7 = center7 + in7*7;
      int a00 = in0 & 4095;
      int a02 = c3[0];  int a20 = c3[1];  int a22 = c3[2];
      int a01 = c5[0];  int a10 = c5[1];  int a11 = c5[2];  int a12 = c5[3];  int a21 = c5[4];
      int a03 = c7[0];  int a13 = c7[1];  int a23 = c7[2];  int a30 = c7[3];  int a31 = c7[4];  int a32 = c7[5];  int a33 = c7[6];
      int aab, sab, acd, scd;
      NOI_HDT2X2(a00,a01,a10,a11);    NOI_HDT2X2(a02,a03,a12,a13);    NOI_HDT2X2(a20,a21,a30,a31);    NOI_HDT2X2(a22,a23,a32,a33);
      NOI_HDT2X2S(a00,a02,a20,a22);   NOI_HDT2X2S(a01,a03,a21,a23);   NOI_HDT2X2S(a10,a12,a30,a32);   NOI_HDT2X2S(a11,a13,a31,a33);
      // convert Y-block directly from HDT output
      uint8_t * bpixels = pixels + ty*4*stride + tx*4*4;  int C;
      int d = *ublock++ - 128;  int e = *vblock++ - 128;
      int DR = + 409*e + 128; int DG = - 100*d - 209*e + 128; int DB = + 516*d + 128;
      NOI_YUV2RGB(0,a00);  NOI_YUV2RGB(1,a01);  NOI_YUV2RGB(2,a02);  NOI_YUV2RGB(3,a03);  bpixels += stride;
      NOI_YUV2RGB(0,a10);  NOI_YUV2RGB(1,a11);  NOI_YUV2RGB(2,a12);  NOI_YUV2RGB(3,a13);  bpixels += stride;
      NOI_YUV2RGB(0,a20);  NOI_YUV2RGB(1,a21);  NOI_YUV2RGB(2,a22);  NOI_YUV2RGB(3,a23);  bpixels += stride;
      NOI_YUV2RGB(0,a30);  NOI_YUV2RGB(1,a31);  NOI_YUV2RGB(2,a32);  NOI_YUV2RGB(3,a33);
      in += 5;
    }
  }
}

#undef NOI_YUV2RGB

#define NOI_YUV2RGB(ofsx,AA) \
      bpixels[ofsx*4+0] = bpixels[ofsx*4+1] = bpixels[ofsx*4+2] = noi_saturate(AA); bpixels[ofsx*4+3] = 255;

void noi_convert_colors_greyscale ( uint8_t * in, int16_t * center3, int profile, uint8_t * pixels, int stride ) {
  int16_t * center5 = center3 + NOI_K3*3;
  int16_t * center7 = center5 + NOI_K5*5;
  int in0 = *((uint16_t *)in);
  int in3 = in[2] + ((in0 & 0xC000)>>6);
  int in5 = in[3] + ((in0 & 0x2000)>>5);
  int in7 = in[4] + ((in0 & 0x1000)>>4);
  int16_t * c3 = center3 + in3*3;
  int16_t * c5 = center5 + in5*5;
  int16_t * c7 = center7 + in7*7;
  int a00 = in0 & 4095;
  int a02 = c3[0];  int a20 = c3[1];  int a22 = c3[2];
  int a01 = c5[0];  int a10 = c5[1];  int a11 = c5[2];  int a12 = c5[3];  int a21 = c5[4];
  int a03 = c7[0];  int a13 = c7[1];  int a23 = c7[2];  int a30 = c7[3];  int a31 = c7[4];  int a32 = c7[5];  int a33 = c7[6];
  int aab, sab, acd, scd;
  NOI_HDT2X2(a00,a01,a10,a11);    NOI_HDT2X2(a02,a03,a12,a13);    NOI_HDT2X2(a20,a21,a30,a31);    NOI_HDT2X2(a22,a23,a32,a33);
  NOI_HDT2X2S(a00,a02,a20,a22);   NOI_HDT2X2S(a01,a03,a21,a23);   NOI_HDT2X2S(a10,a12,a30,a32);   NOI_HDT2X2S(a11,a13,a31,a33);
  // convert Y-block directly from HDT output
  uint8_t * bpixels = pixels;
  NOI_YUV2RGB(0,a00);  NOI_YUV2RGB(1,a01);  NOI_YUV2RGB(2,a02);  NOI_YUV2RGB(3,a03);  bpixels += stride;
  NOI_YUV2RGB(0,a10);  NOI_YUV2RGB(1,a11);  NOI_YUV2RGB(2,a12);  NOI_YUV2RGB(3,a13);  bpixels += stride;
  NOI_YUV2RGB(0,a20);  NOI_YUV2RGB(1,a21);  NOI_YUV2RGB(2,a22);  NOI_YUV2RGB(3,a23);  bpixels += stride;
  NOI_YUV2RGB(0,a30);  NOI_YUV2RGB(1,a31);  NOI_YUV2RGB(2,a32);  NOI_YUV2RGB(3,a33);
  in += 5;
}

#undef NOI_HDT2X2
#undef NOI_HDT2X2S

#undef NOI_YUV2RGB

void noi_convert_colors_YUV_4_1_1 ( uint8_t * in, int16_t * c3, int profile, uint8_t * pixels, int stride ) {
  int blocks[16*6];
  noi_decompress_5_16(in, blocks, c3, profile );
  int * ublock = blocks;
  int * vblock = blocks + 16;
  int * yblock = blocks + 32;
  for ( int ty=0; ty!=2; ty++ ) {
    for ( int tx=0; tx!=2; tx++ ) {
      uint8_t * bpixels = pixels + ty*4*stride + tx*4*4;
      for ( int y=0; y!=4; y++ ) {
        for ( int x=0; x!=4; x++ ) {
          int uvy = ty*4+y;
          int uvx = tx*4+x;
          int t = (uvy>>1)*4 + (uvx>>1);
          int U = ublock[t];  int V = vblock[t];  int Y = yblock[y*4+x];
          int R, G, B;  noi_yuv2rgb(Y, U, V, &R, &G, &B);
          bpixels[x*4+0] = noi_saturate(R); bpixels[x*4+1] = noi_saturate(G); bpixels[x*4+2] = noi_saturate(B); bpixels[x*4+3] = 255;
        }
        bpixels += stride;
      }
      yblock += 16;
    }
  }
}

void noi_convert_colors_YUV_2_1_1 ( uint8_t * in, int16_t * c3, int profile, uint8_t * pixels, int stride ) {
  int blocks[16*4];
  noi_decompress_5_16(in, blocks, c3, profile );
  int * ublock = blocks;
  int * vblock = blocks + 16;
  int * yblock = blocks + 32;
  for ( int tx=0; tx!=2; tx++ ) {
    uint8_t * bpixels = pixels + tx*4*4;
    for ( int y=0; y!=4; y++ ) {
      for ( int x=0; x!=4; x++ ) {
        int uvy = y;
        int uvx = tx*4+x;
        int t = uvy*4 + (uvx>>1);
        int U = ublock[t];  int V = vblock[t];  int Y = yblock[y*4+x];
        int R, G, B;  noi_yuv2rgb(Y, U, V, &R, &G, &B);
        bpixels[x*4+0] = noi_saturate(R); bpixels[x*4+1] = noi_saturate(G); bpixels[x*4+2] = noi_saturate(B); bpixels[x*4+3] = 255;
      }
      bpixels += stride;
    }
    yblock += 16;
  }
}

void noi_convert_colors_RGB_1_1_1 ( uint8_t * in, int16_t * c3, int profile, uint8_t * pixels, int stride ) {
  int blocks[16*3];
  noi_decompress_5_16(in, blocks, c3, profile );
  int * rblock = blocks;
  int * gblock = blocks + 16;
  int * bblock = blocks + 32;
  uint8_t * bpixels = pixels;
  for ( int y=0; y!=4; y++ ) {
    for ( int x=0; x!=4; x++ ) {
      int t = y*4 + x;
      int R = rblock[t];  int G = gblock[t];  int B = bblock[t];
      bpixels[x*4+0] = noi_saturate(R); bpixels[x*4+1] = noi_saturate(G); bpixels[x*4+2] = noi_saturate(B); bpixels[x*4+3] = 255;
    }
    bpixels += stride;
  }
}

uint8_t * noi_decompress ( void * bytes, int * W, int * H, uint8_t * pixels ) {
  uint8_t * in = (uint8_t *) bytes;
  noi_header_t * header = (noi_header_t *) in; in += sizeof(noi_header_t);
  if ( header->magic != NOI_MAGIC ) return NULL;
  int w = header->width; if ( W ) *W = w;
  int h = header->height; if ( H ) *H = h;
  int profile = header->profile;
  int psx, psy;
  void ( *decmpress_block ) ( uint8_t * in, int16_t * c3, int profile, uint8_t * pixels, int stride );
  switch ( profile ) {
    case NOI_YUV_16_1_1:  psx = psy = 4; decmpress_block = noi_convert_colors_YUV_16_1_1; break;
    case NOI_YUV_4_1_1:   psx = psy =2; decmpress_block = noi_convert_colors_YUV_4_1_1;  break;
    case NOI_YUV_2_1_1:   psx = 2; psy = 1; decmpress_block = noi_convert_colors_YUV_2_1_1;  break;
    case NOI_RGB_1_1_1:   psx = psy = 1; decmpress_block = noi_convert_colors_RGB_1_1_1;  break;
    case NOI_Y_1_0_0:     psx = psy = 1; decmpress_block = noi_convert_colors_greyscale;  break;
  }
  int bsx = psx * 4;
  int bsy = psy * 4;
  int bw = w / bsx;
  int bh = h / bsy;
  int numBlocks = bw*bh*profile;
  if ( !pixels ) pixels = (uint8_t *) malloc(w*h*4);
  int stride = w * 4;
  int16_t * c3 = (int16_t *)(in + numBlocks * 5);
  for ( int by=0; by!=bh; ++by ) {
    for ( int bx=0; bx!=bw; ++bx ) {
      (*decmpress_block) (in, c3, profile, pixels + (bx*bsx*4) + (by*bsy)*stride, stride );
      in += profile*5;
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

const char * noi_profile_name ( int profile ) {
  switch ( profile ) {
    case NOI_YUV_16_1_1:  return "YUV_16_1_1";
    case NOI_YUV_4_1_1:   return "YUV_4_1_1";
    case NOI_YUV_2_1_1:   return "YUV_2_1_1";
    case NOI_RGB_1_1_1:   return "RGB_1_1_1";
    case NOI_Y_1_0_0:     return "Y_1_0_0";
    default:              return NULL;
  }
}

void noi_profile_block_size ( int profile, int * bx, int * by ) {
  switch ( profile ) {
    case NOI_YUV_16_1_1:  *bx=16; *by=16; break;
    case NOI_YUV_4_1_1:   *bx=8;  *by=8; break;
    case NOI_YUV_2_1_1:   *bx=8;  *by=4; break;
    case NOI_RGB_1_1_1:   *bx=4;  *by=4; break;
    case NOI_Y_1_0_0:     *bx=4;  *by=4; break;
  }
}


#endif
#endif
