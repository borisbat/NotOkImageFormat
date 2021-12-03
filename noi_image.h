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

#define NOI_KMEANS_NPASS  16      // this controls compression quality. 100 is excessive? 8 goes down, 32 is excellent
#define NOI_MAX_THREADS   32      // when NOI_THREADS is defined, this is max number of threads
#define NOI_MAGIC   0xBAD0DAB0

#define NOI_K3    1024
#define NOI_K5    512
#define NOI_K7    512

#ifdef __cplusplus
extern "C" {
#endif

typedef struct noi_header_s {     // NOI file header
  uint32_t magic;
  uint16_t width;
  uint16_t height;
} noi_header_t;

void * noi_compress ( uint8_t * pixels, int w, int h, int * bytes );
uint8_t * noi_decompress ( void * bytes, int * W, int * H, uint8_t * pixels );  // if pixels are NULL, they are allocated
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
      center[k*16+i] = ((int)(seed>>2)) % ( dim_max[i] - dim_min[i] ) + dim_min[i];
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
  *header = (noi_header_t) { NOI_MAGIC, w, h };
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
    B[0] = c3[3]; B[1] = c3[12]; B[2] = c3[15];
  }
  for ( int k=0; k!=NOI_K5; ++k ) {
    int16_t * B = (int16_t *) out; out += 5*sizeof(int16_t);
    int * c5 = res5.center + k*16;
    B[0] = c5[1]; B[1] = c5[4]; B[2] = c5[5]; B[3] = c5[7]; B[4] = c5[13];
  }
  for ( int k=0; k!=NOI_K7; ++k ) {
    int16_t * B = (int16_t *) out; out += 7*sizeof(int16_t);
    int * c7 = res7.center + k*16;
    B[0] = c7[2]; B[1] = c7[6]; B[2] = c7[8]; B[3] = c7[9]; B[4] = c7[10]; B[5] = c7[11]; B[6] = c7[14];
  }
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

void noi_decompress_block ( uint8_t * in, uint8_t * pixels, int stride, int16_t * center3, int16_t * center5, int16_t * center7 ) {
  int blocks[16*18];
  int * fb = blocks;
  for ( int o=0; o!=18; ++o ) {
    int in0 = *((uint16_t *)in);
    int in3 = in[2] + ((in0 & 0xC000)>>6);
    int in5 = in[3] + ((in0 & 0x2000)>>5);
    int in7 = in[4] + ((in0 & 0x1000)>>4);
    int16_t * c3 = center3 + in3*3;
    int16_t * c5 = center5 + in5*5;
    int16_t * c7 = center7 + in7*7;
    fb[0] = in0 & 4095;
    fb[1] = c5[0];  fb[2] = c7[0];  fb[3] = c3[0];
    fb[4] = c5[1];  fb[5] = c5[2];  fb[6] = c7[1];  fb[7] = c5[3];
    fb[8] = c7[2];  fb[9] = c7[3];  fb[10] = c7[4]; fb[11] = c7[5];
    fb[12] = c3[1]; fb[13] = c5[4]; fb[14] = c7[6]; fb[15] = c3[2];
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
  int16_t * c3 = (int16_t *)(in + numBlocks * 5);
  int16_t * c5 = c3 + NOI_K3*3;
  int16_t * c7 = c5 + NOI_K5*5;
  for ( int by=0; by!=bh; ++by ) {
    for ( int bx=0; bx!=bw; ++bx ) {
      noi_decompress_block(in, pixels + (bx*16*4) + (by*16)*stride, stride, c3, c5, c7 );
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
