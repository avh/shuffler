// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
#include "image.h"
#include "http.h"

int bmp_content_length(Image &img)
{
  int w = img.width + (4 - (img.width % 4)) % 4;
  return 54 + 4*256 + w * img.height;
}

void bmp_http_write(HTTP &http, Image &img)
{
  int filesize = bmp_content_length(img);
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
  img.debug("BMP");
  dprintf("FILESIZE=%d", filesize);
}