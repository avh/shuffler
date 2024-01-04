// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
#include <PNGenc.h>

static PNG png; 

int png_encode_565(unsigned char *buf, int buflen, unsigned short *src, int width, int height, int stride)
{
  int rc = PNG_NO_BUFFER;
  int content_length = -1;
  auto linebuf = std::shared_ptr<uint8_t[]>(new uint8_t[width*3]);
  if (buf != NULL && linebuf != NULL) {
    rc = png.open(buf, buflen);
    if (rc == PNG_SUCCESS) {
      rc = png.encodeBegin(width, height, PNG_PIXEL_TRUECOLOR, 24, NULL, 3);
      if (rc == PNG_SUCCESS) {
        for (int r = 0 ; r < height && rc == PNG_SUCCESS ; r++, src += stride) {
          rc = png.addRGB565Line(src, linebuf.get());
        }
        content_length = png.close();
      }
    }
  }
  return rc == PNG_SUCCESS ? content_length : -rc;
}

int png_encode(unsigned char *buf, int buflen, Image &src)
{
  int rc = PNG_NO_BUFFER;
  int content_length = -1;
  if (buf != NULL) {
    rc = png.open(buf, buflen);
    if (rc == PNG_SUCCESS) {
      rc = png.encodeBegin(src.width, src.height, PNG_PIXEL_GRAYSCALE, 8, NULL, 3);
      if (rc == PNG_SUCCESS) {
        unsigned char *p = src.data;
        for (int r = 0 ; r < src.height && rc == PNG_SUCCESS ; r++, p += src.stride) {
          rc = png.addLine(p);
        }
        content_length = png.close();
      }
    }
  }
  return rc == PNG_SUCCESS ? content_length : -rc;
}



