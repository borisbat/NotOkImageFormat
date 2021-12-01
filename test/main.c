#include <noi_image.h>
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

#include <time.h>
#include <math.h>

void print_use ( void ) {
  printf(
    "NotOk image compression\n"
    "  noi -c source_image dest_noi     compress\n"
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
  if ( strcmp(argv[1],"-c")==0 || strcmp(argv[1],"-cp")==0 ) {
    int w, h;
    uint8_t * pixels = stbi_load(argv[2], &w, &h, NULL, 4);
    if ( !pixels ) {
      printf("can't load image from %s\n", argv[1]);
      return -2;
    }
    if ( (w&15) || (h&15) ) {
      printf("image dimensions need to be fully dividable by 16, and not %ix%i\n", w, h );
      return -3;
    }
    printf("noi_compress %i x %i\n", w, h);
    int csize;
    void * cbytes = noi_compress(pixels, w, h, &csize);
    if ( !cbytes ) {
      printf("compression failed\n");
      return -4;
    }
    if ( strcmp(argv[1],"-cp")==0 ) {
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
    fread(cbytes, 1, csize, f);
    fclose(f);
    int w, h;
    if ( strcmp(argv[1],"-pd")==0 ) {
      int nTimes = 100;
      printf("running noi_decompressing %i times, %i bytes\n", nTimes, csize);
      noi_image_size(cbytes, &w, &h);
      uint8_t * pixels = (uint8_t *) malloc(w*h*4);
      clock_t t0 = clock();
      for ( int t=0; t!=nTimes; ++t )
        noi_decompress(cbytes, &w, &h, pixels);
      clock_t t1 = clock();
      double sec = ((double)(t1-t0))/CLOCKS_PER_SEC;
      double mb = ((double)(w*h*3))*nTimes/1024./1024.;
      printf("%i mb in %.2f sec, %.1fmb/sec\n", ((int)mb), sec, mb/sec );
      if ( !pixels ) {
        printf("can't decompress noi\n");
        return -7;
      }
      stbi_write_png(argv[3], w, h, 4, pixels, w*4);
    } else {
      printf("noi_decompress %i bytes\n", csize);
      clock_t t0 = clock();
      uint8_t * pixels = noi_decompress(cbytes, &w, &h, NULL);
      clock_t t1 = clock();
      printf("%i bytes in %.2f sec\n", w*h*3,((double)(t1-t0))/CLOCKS_PER_SEC);
      if ( !pixels ) {
        printf("can't decompress noi\n");
        return -7;
      }
      stbi_write_png(argv[3], w, h, 4, pixels, w*4);
    }
    return 0;
  }
}