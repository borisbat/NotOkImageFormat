#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <noi_image.h>
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

#include <math.h>

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
int64_t ref_time_ticks () {
    LARGE_INTEGER  t0;
    QueryPerformanceCounter(&t0);
    return t0.QuadPart;
}
int get_time_usec ( int64_t reft ) {
    int64_t t0 = ref_time_ticks();
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return (int)(((t0-reft)*1000000) / freq.QuadPart);
}
#elif __linux__
#include <time.h>
const uint64_t NSEC_IN_SEC = 1000000000LL;
int64_t ref_time_ticks () {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * NSEC_IN_SEC + ts.tv_nsec;
}
int get_time_usec ( int64_t reft ) {
    return (int) ((ref_time_ticks() - reft) / (NSEC_IN_SEC/1000000));
}
#else // osx
#include <mach/mach.h>
#include <mach/mach_time.h>
int64_t ref_time_ticks() {
    return mach_absolute_time();
}
int get_time_usec ( int64_t reft ) {
    int64_t relt = ref_time_ticks() - reft;
    mach_timebase_info_data_t s_timebase_info;
    mach_timebase_info(&s_timebase_info);
    return relt * s_timebase_info.numer/s_timebase_info.denom/1000;
}
#endif

void print_use ( void ) {
  printf(
    "NotOk image compression\n"
    "  noi -b path_to_images/* dest_folder        batch compress and output PSNR\n"
    "  noi -c source_image dest_noi {profile}     compress\n"
    "  noi -pc source_image dest_noi {profile}    compress and output stats\n"
    "  noi -d source_noi dest_png                 decompress\n"
    "  noi -pd source_noi dest_png                profile decompression\n"
    "  noi -stbjpg source_image dest_jpg          profile stbimage jpeg decoder\n"
  );
}

void rgb2yuv ( double R, double G, double B, double * Y, double * U, double * V ) {
  *Y =  0.257 * R + 0.504 * G + 0.098 * B +  16;
  *U = -0.148 * R - 0.291 * G + 0.439 * B + 128;
  *V =  0.439 * R - 0.368 * G - 0.071 * B + 128;
}

void psnr ( uint8_t * oi, uint8_t * ci, int npixels, double * PSNR, double * PSNR_YUV ) {
  double mse = 0.;
  double mse_yuv = 0.;
  double npix = npixels;
  while ( npixels-- ) {
    double dr = ((double)oi[0]) - ((double)ci[0]);
    double dg = ((double)oi[1]) - ((double)ci[1]);
    double db = ((double)oi[2]) - ((double)ci[2]);
    mse += ( dr*dr + dg*dg + db*db ) / 3.;
    double Y1,U1,V1,Y2,U2,V2;
    rgb2yuv(oi[0],oi[1],oi[2],&Y1,&U1,&V1);
    rgb2yuv(ci[0],ci[1],ci[2],&Y2,&U2,&V2);
    double dy = Y1 - Y2;
    double du = U1 - U2;
    double dv = V1 - V2;
    mse_yuv += ( dy*dy + du*du + dv*dv ) / 3.;
    oi += 4;
    ci += 4;
  }
  mse /= npix;
  mse_yuv /= npix;
  double D = 255.;
	*PSNR = (10 * log10((D*D) / mse));
  *PSNR_YUV = (10 * log10((D*D) / mse_yuv));
}

void * load_file ( const char * fileName, int * fsize ) {
  FILE * f = fopen(fileName,"rb");
  if ( !f ) return NULL;
  fseek(f,0,SEEK_END);
  int csize = ftell(f);
  fseek(f,0,SEEK_SET);
  void * cbytes = malloc ( csize );
  int cdsize = fread(cbytes, 1, csize, f);
  if ( csize != cdsize ) {
    free(cbytes);
    return NULL;
  }
  fclose(f);
  if ( fsize ) *fsize = csize;
  return cbytes;
}

int get_profile ( const char * arg ) {
  int profile = NOI_YUV_16_1_1;
        if ( strcmp(arg,"4_1_1")==0 ) profile = NOI_YUV_4_1_1;
  else  if ( strcmp(arg,"2_1_1")==0 ) profile = NOI_YUV_2_1_1;
  else  if ( strcmp(arg,"16_1_1")==0 ) profile = NOI_YUV_16_1_1;
  else  if ( strcmp(arg,"1_1_1")==0 ) profile = NOI_RGB_1_1_1;
  else  if ( strcmp(arg,"1_0_0")==0 ) profile = NOI_Y_1_0_0;
  else {
    printf("unsupported profile %s\n", arg);
    exit(-9);
  }
  return profile;
}

#ifdef _WIN32
#include "dirent/dirent.h"
#else
#include <dirent.h>
#endif

int main(int argc, char** argv) {
  if ( !(argc==4 || argc==5) ) {
    print_use();
    return -1;
  }
  if ( strcmp(argv[1],"-b")==0 ) {
    int profile = argc==5 ? get_profile(argv[4]) : NOI_YUV_16_1_1;
    struct dirent **namelist;
    int n = scandir(argv[2], &namelist, 0, alphasort);
    if ( n>=0 ) {
        printf("IMAGE, PSNR, PSNR_YUV, PROFILE, W, H\n");
        while( n-- ) {
            size_t sl = strlen(namelist[n]->d_name);
            if ( sl>4 && strcmp(namelist[n]->d_name+sl-4,".png")==0 ) {
              char img_name[1024], test_name[1024];
              snprintf(img_name, sizeof(img_name), "%s\\%s", argv[2], namelist[n]->d_name);
              snprintf(test_name, sizeof(test_name), "%s\\%s", argv[3], namelist[n]->d_name);
              int w, h;
              uint8_t * pixels = stbi_load(img_name, &w, &h, NULL, 4);
              if ( pixels!=NULL ) {
                if ( w&15 || h&15 ) {
                  printf("\"%s\",\"skipped\",\"skipped\",%i,%i\n",  namelist[n]->d_name, w, h );
                } else {
                  int csize = 0;
                  void * cbytes = noi_compress(pixels, w, h, &csize, profile);
                  uint8_t * cpixels = noi_decompress(cbytes, NULL, NULL, NULL);
                  double PSNR, PSNR_YUV;
                  psnr(pixels, cpixels, w*h, &PSNR, &PSNR_YUV);
                  printf("\"%s\", -%.1f, -%.1f, \"%s\", %i, %i\n",  namelist[n]->d_name, PSNR, PSNR_YUV, noi_profile_name(profile), w, h );
                  stbi_write_png(test_name, w, h, 4, cpixels, w*4);
                  free(cpixels);
                  free(cbytes);
                }
                stbi_image_free(pixels);
              }
            }
            free(namelist[n]);
        }
    }
    free(namelist);
    return 0;
  } else if ( strcmp(argv[1],"-c")==0 || strcmp(argv[1],"-pc")==0 ) {
    int profile = get_profile(argv[4]);
    int w, h;
    uint8_t * pixels = stbi_load(argv[2], &w, &h, NULL, 4);
    if ( !pixels ) {
      printf("can't load image from %s\n", argv[2]);
      return -2;
    }
    if ( (w&15) || (h&15) ) {
      printf("image dimensions need to be fully dividable by 16, and not %ix%i\n", w, h );
      return -3;
    }
    printf("noi_compress %i x %i profile %s\n", w, h, noi_profile_name(profile));
    int csize;
    uint64_t t0 = ref_time_ticks();
    void * cbytes = noi_compress(pixels, w, h, &csize, profile);
    double sec = get_time_usec(t0) / 1000000.0;
    if ( !cbytes ) {
      printf("compression failed\n");
      return -4;
    }
    double mb = ((double)(w*h*3))/1024./1024.;
    printf("%i mb in %.2f sec, %.3fmb/sec\n", ((int)mb), sec, mb/sec );
    if ( strcmp(argv[1],"-pc")==0 ) {
      uint8_t * cpixels = noi_decompress(cbytes, NULL, NULL, NULL);
      double PSNR, PSNR_YUV;
      psnr(pixels, cpixels, w*h, &PSNR, &PSNR_YUV);
      printf("PSNR = -%.1f   PSNR(YUV) = -%.1f\n", PSNR, PSNR_YUV);
      free(cpixels);
    }
    stbi_image_free(pixels);
    FILE * f = fopen(argv[3], "wb");
    if ( !f ) {
      printf("can't save noi to %s\n", argv[2]);
      return -5;
    }
    fwrite(cbytes, 1, csize, f);
    fclose(f);
    free(cbytes);
    return 0;
  } else if ( strcmp(argv[1],"-d")==0 || strcmp(argv[1],"-pd")==0 ) {
    int csize;
    void * cbytes = load_file(argv[2], &csize);
    if ( !cbytes ) {
      printf("can't load NOI %s\n", argv[2]);
      return -6;
    }
    int w, h;
    if ( strcmp(argv[1],"-pd")==0 ) {
      int nTimes = 100;
      printf("running noi_decompressing %i times, %i bytes\n", nTimes, csize);
      noi_image_size(cbytes, &w, &h);
      uint8_t * pixels = (uint8_t *) malloc(w*h*4);
      double minsec = 100500.;
      for ( int t=0; t!=nTimes; ++t ) {
        uint64_t t0 = ref_time_ticks();
        noi_decompress(cbytes, &w, &h, pixels);
        double sec = get_time_usec(t0) / 1000000.0;
        if ( sec<minsec ) minsec = sec;
      }
      double mb = ((double)(w*h*3))/1024./1024.;
      printf("decompression speed %.1fmb/sec\n", mb/minsec );
      if ( !pixels ) {
        printf("can't decompress noi\n");
        return -7;
      }
      stbi_write_png(argv[3], w, h, 4, pixels, w*4);
    } else {
      printf("noi_decompress %i bytes\n", csize);
      uint64_t t0 = ref_time_ticks();
      uint8_t * pixels = noi_decompress(cbytes, &w, &h, NULL);
      double sec = get_time_usec(t0) / 1000000.0;
      printf("%i bytes in %.2f sec\n", w*h*3, sec);
      if ( !pixels ) {
        printf("can't decompress noi\n");
        return -7;
      }
      stbi_write_png(argv[3], w, h, 4, pixels, w*4);
      return 0;
    }
  } else if ( strcmp(argv[1],"-stbjpg")==0 ) {
    int w, h;
    uint8_t * pixels = stbi_load(argv[2], &w, &h, NULL, 4);
    if ( !pixels ) {
      printf("can't load image from %s\n", argv[2]);
      return -2;
    }
    stbi_write_jpg(argv[3], w, h, 4, pixels, 90);
    int csize;
    void * cbytes = load_file(argv[3], &csize);
    if ( !cbytes ) {
      printf("can't load JPG %s\n", argv[3]);
      return -6;
    }
    int nTimes = 100;
    printf("running stbi_load_from_memory %i times, %i bytes\n", nTimes, csize);
    double minsec = 100500;
    for ( int t=0; t!=nTimes; ++t ) {
      uint64_t t0 = ref_time_ticks();
      void * pixels = stbi_load_from_memory(cbytes, csize, &w, &h, NULL, 4);
      double sec = get_time_usec(t0) / 1000000.0;
      if ( sec<minsec ) minsec = sec;
      free(pixels);
    }
    double mb = ((double)(w*h*3))/1024./1024.;
    printf("decompression speed %.1fmb/sec\n", mb/minsec );
  } else {
    print_use();
    return -8;
  }
}