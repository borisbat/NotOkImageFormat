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
    "  noi -c source_image dest_noi     compress\n"
    "  noi -pc source_image dest_noi    compress and output stats\n"
    "  noi -d source_noi dest_png       decompress\n"
    "  noi -pd source_noi dest_png      profile decompression\n"
  );
}

void psnr ( uint8_t * oi, uint8_t * ci, int npixels ) {
  double mse = 0.;
  double npix = npixels;
  while ( npixels-- ) {
    double dr = ((double)oi[0]) - ((double)ci[0]);
    double dg = ((double)oi[1]) - ((double)ci[1]);
    double db = ((double)oi[2]) - ((double)ci[2]);
    mse += ( dr*dr + dg*dg + db*db ) / 3.;
    oi += 4;
    ci += 4;
  }
  mse /= npix;
  double D = 255.;
	double PSNR = (10 * log10((D*D) / mse));
  printf("PSNR = -%.1f\n", PSNR);
}

int main(int argc, char** argv) {
  if ( argc!=4 ) {
    print_use();
    return -1;
  }
  if ( strcmp(argv[1],"-c")==0 || strcmp(argv[1],"-pc")==0 ) {
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
    printf("noi_compress %i x %i\n", w, h);
    int csize;
    uint64_t t0 = ref_time_ticks();
    void * cbytes = noi_compress(pixels, w, h, &csize);
    double sec = get_time_usec(t0) / 1000000.0;
    if ( !cbytes ) {
      printf("compression failed\n");
      return -4;
    }
    double mb = ((double)(w*h*3))/1024./1024.;
    printf("%i mb in %.2f sec, %.3fmb/sec\n", ((int)mb), sec, mb/sec );
    if ( strcmp(argv[1],"-pc")==0 ) {
      uint8_t * cpixels = noi_decompress(cbytes, NULL, NULL, NULL);
      psnr(pixels, cpixels, w*h);
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
    FILE * f = fopen(argv[2],"rb");
    if ( !f ) {
      printf("can't load noi from %s\n", argv[2]);
      return -6;
    }
    fseek(f,0,SEEK_END);
    int csize = ftell(f);
    fseek(f,0,SEEK_SET);
    void * cbytes = malloc ( csize );
    int cdsize = fread(cbytes, 1, csize, f);
    if ( csize != cdsize ) {
      printf("read error, %i vs %i\n", csize, cdsize );
      return -66;
    }
    fclose(f);
    int w, h;
    if ( strcmp(argv[1],"-pd")==0 ) {
      int nTimes = 100;
      printf("running noi_decompressing %i times, %i bytes\n", nTimes, csize);
      noi_image_size(cbytes, &w, &h);
      uint8_t * pixels = (uint8_t *) malloc(w*h*4);
      uint64_t t0 = ref_time_ticks();
      for ( int t=0; t!=nTimes; ++t )
        noi_decompress(cbytes, &w, &h, pixels);
      double sec = get_time_usec(t0) / 1000000.0;
      double mb = ((double)(w*h*3))*nTimes/1024./1024.;
      printf("%i mb in %.2f sec, %.1fmb/sec\n", ((int)mb), sec, mb/sec );
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
  } else {
    print_use();
    return -8;
  }
}