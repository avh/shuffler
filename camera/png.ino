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
      rc = png.encodeBegin(frame_width, frame_height, PNG_PIXEL_TRUECOLOR, 24, NULL, 3);
      if (rc == PNG_SUCCESS) {
        for (int r = 0 ; r < height && rc == PNG_SUCCESS ; r++, src += stride) {
          unsigned char *dst = linebuf.get();
          for (int c = 0 ; c < width ; c++) {
            unsigned short v = src[c];
            *dst++ = r565(v);
            *dst++ = g565(v);
            *dst++ = b565(v);
          }
          rc = png.addLine(linebuf.get());
        }
        content_length = png.close();
      }
    }
  }
  //free(linebuf);
  return rc == PNG_SUCCESS ? content_length : -rc;
}

int png_encode_gray(unsigned char *buf, int buflen, unsigned char *src, int width, int height, int stride)
{
  int rc = PNG_NO_BUFFER;
  int content_length = -1;
  if (buf != NULL) {
    rc = png.open(buf, buflen);
    if (rc == PNG_SUCCESS) {
      rc = png.encodeBegin(frame_width, frame_height, PNG_PIXEL_GRAYSCALE, 8, NULL, 3);
      if (rc == PNG_SUCCESS) {
        for (int r = 0 ; r < height && rc == PNG_SUCCESS ; r++, src += stride) {
          rc = png.addLine(src);
        }
        content_length = png.close();
      }
    }
  }
  return rc == PNG_SUCCESS ? content_length : -rc;
}