#include <noi_image.h>
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

void print_use ( void ) {
  printf(
    "NotOk image compression\n"
    "  noi -c source_image dest_noi\n"
    "  noi -d source_noi dest_png\n"
  );
}

int main(int argc, char** argv) {
  if ( argc!=4 ) {
    print_use();
    return -1;
  }
  if ( strcmp(argv[1],"-c")==0 ) {
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
    int csize;
    void * cbytes = noi_compress(pixels, w, h, &csize);
    if ( !cbytes ) {
      printf("compression failed\n");
      return -4;
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
  } else if ( strcmp(argv[1],"-d")==0 ) {
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
    uint8_t * pixels = noi_decompress(cbytes, &w, &h);
    if ( !pixels ) {
      printf("can't decompress noi\n");
      return -7;
    }
    stbi_write_png(argv[3], w, h, 4, pixels, w*4);
    return 0;
  }
}