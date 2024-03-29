// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
#include "image.h"
#include "http.h"

int bmp_file_size(Image &img)
{
  int w = img.width + (4 - (img.width % 4)) % 4;
  return 54 + 4*256 + w * img.height;
}

int bmp_load(const char *fname, Image &img)
{
  unsigned char bmpfileheader[14];
  unsigned char bmpinfoheader[40];

  FILE *fp = fopen(fname, "r");
  if (fp == NULL) {
    return -1;
  }
  if (fread(bmpfileheader, 1, sizeof(bmpfileheader), fp) != sizeof(bmpfileheader)) {
    fclose(fp);
    return -1;
  }
  if (fread(bmpinfoheader, 1, sizeof(bmpinfoheader), fp) != sizeof(bmpinfoheader)) {
    fclose(fp);
    return -1;
  }
  if (bmpfileheader[0] != 'B' || bmpfileheader[1] != 'M') {
    fclose(fp);
    return -1;
  }
  int width = bmpinfoheader[4] + (bmpinfoheader[5] << 8) + (bmpinfoheader[6] << 16) + (bmpinfoheader[7] << 24);
  int height = bmpinfoheader[8] + (bmpinfoheader[9] << 8) + (bmpinfoheader[10] << 16) + (bmpinfoheader[11] << 24);
  if (!img.init(width, height)) {
    fclose(fp);
    return -1;
  }
  fseek(fp, 4*256, SEEK_CUR);
  int padlen = (4 - (img.width % 4)) % 4;
  for (int y = height-1 ; y-- > 0 ;) {
    if (fread(img.addr(0, y), 1, img.width, fp) != img.width) {
      fclose(fp);
      return -1;
    }
    fseek(fp, padlen, SEEK_CUR);
  }
  fclose(fp);
  return 0;
}

void bmp_save(const char *fname, Image &img)
{
  int ms = millis();
  int filesize = bmp_file_size(img);
  unsigned char bmpfileheader[14] = {'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0};
  unsigned char bmpinfoheader[40] = {40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 8,0};
  unsigned char bmppad[3] = {0,0,0};
  unsigned char pallet[4*356];
  int w = img.width;
  int h = img.height;

  for (int i = 0 ; i < 256 ; i++) {
    pallet[i*4 + 0] = i;
    pallet[i*4 + 1] = i;
    pallet[i*4 + 2] = i;
    pallet[i*4 + 3] = 0;
  }

  bmpfileheader[ 2] = (unsigned char)(filesize    );
  bmpfileheader[ 3] = (unsigned char)(filesize>> 8);
  bmpfileheader[ 4] = (unsigned char)(filesize>>16);
  bmpfileheader[ 5] = (unsigned char)(filesize>>24);

  bmpinfoheader[ 4] = (unsigned char)(       w    );
  bmpinfoheader[ 5] = (unsigned char)(       w>> 8);
  bmpinfoheader[ 6] = (unsigned char)(       w>>16);
  bmpinfoheader[ 7] = (unsigned char)(       w>>24);
  bmpinfoheader[ 8] = (unsigned char)(       h    );
  bmpinfoheader[ 9] = (unsigned char)(       h>> 8);
  bmpinfoheader[10] = (unsigned char)(       h>>16);
  bmpinfoheader[11] = (unsigned char)(       h>>24);

  FILE *fp = fopen(fname, "w");
  int padlen = (4 - (img.width % 4)) % 4;
  if (fp != NULL) {
    fwrite(bmpfileheader, 1, 14, fp);
    fwrite(bmpinfoheader, 1, 40, fp);
    fwrite(pallet, 1, 4*256, fp);
    for (int y = img.height ; y-- > 0 ;) {
      fwrite(img.addr(0, y), 1, img.width, fp);
      fwrite(bmppad, 1, padlen, fp);
    }
    fclose(fp);
  }
  dprintf("saved %s in %dms", fname, millis() - ms);
}

void bmp_http_write(HTTP &http, Image &img)
{
  int filesize = bmp_file_size(img);
  unsigned char bmpfileheader[14] = {'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0};
  unsigned char bmpinfoheader[40] = {40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 8,0};
  unsigned char bmppad[3] = {0,0,0};
  unsigned char pallet[4*356];
  int w = img.width;
  int h = img.height;

  for (int i = 0 ; i < 256 ; i++) {
    pallet[i*4 + 0] = i;
    pallet[i*4 + 1] = i;
    pallet[i*4 + 2] = i;
    pallet[i*4 + 3] = 0;
  }

  bmpfileheader[ 2] = (unsigned char)(filesize    );
  bmpfileheader[ 3] = (unsigned char)(filesize>> 8);
  bmpfileheader[ 4] = (unsigned char)(filesize>>16);
  bmpfileheader[ 5] = (unsigned char)(filesize>>24);

  bmpinfoheader[ 4] = (unsigned char)(       w    );
  bmpinfoheader[ 5] = (unsigned char)(       w>> 8);
  bmpinfoheader[ 6] = (unsigned char)(       w>>16);
  bmpinfoheader[ 7] = (unsigned char)(       w>>24);
  bmpinfoheader[ 8] = (unsigned char)(       h    );
  bmpinfoheader[ 9] = (unsigned char)(       h>> 8);
  bmpinfoheader[10] = (unsigned char)(       h>>16);
  bmpinfoheader[11] = (unsigned char)(       h>>24);

  http.write(bmpfileheader, 14);
  http.write(bmpinfoheader, 40);
  http.write(pallet, 4*256);
  for (int y = img.height ; y-- > 0 ;) {
    http.write(img.addr(0, y), img.width);
    http.write(bmppad, (4 - (img.width % 4)) % 4);
  }
}