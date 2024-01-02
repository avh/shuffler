#include <PNGenc.h>

PNG png; 

int png_encode_565(unsigned char *buf, int buflen, unsigned short *src, int width, int height, int stride)
{
  if (buf == NULL) {
    return -1;
  }
  uint8_t *linebuf = (uint8_t *)malloc(width*3);
  if (linebuf == NULL) {
    return -1;
  }
  
  int content_length = -1;
  int rc = png.open(buf, buflen);
  if (rc == PNG_SUCCESS) {
    rc = png.encodeBegin(frame_width, frame_height, PNG_PIXEL_TRUECOLOR, 24, NULL, 3);
    if (rc == PNG_SUCCESS) {
      for (int r = 0 ; r < height && rc == PNG_SUCCESS ; r++, src += stride) {
        unsigned char *dst = linebuf;
        for (int c = 0 ; c < width ; c++) {
          unsigned short v = src[c];
          *dst++ = r565(v);
          *dst++ = g565(v);
          *dst++ = b565(v);
        }
        rc = png.addLine(linebuf);
      }
      content_length = png.close();
      dprintf("content_length=%d", content_length);
    }
  }
  free(linebuf);
  return rc == PNG_SUCCESS ? content_length : -1;
}
