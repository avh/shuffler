
#include "defs.h"
#define r565(v)     ((v) & 0x00F8)
#define g565(v)     ((((v) & 0x0007) << 5) | (((v) & 0xE000) >> 11))
#define b565(v)     (((v) & 0x1F00) >> 5)
#define convert565(v) (b565(v) + g565(v))

// unpack 565, convert to grayscale, rotate, scale down
void unpack_565(unsigned short *src, int src_stride, unsigned char *dst, int dst_width, int dst_height, int dst_stride)
{
  for (int c = 0 ; c < dst_width ; c++, src += 2*src_stride, dst += 1) {
    unsigned short *sp = src;
    unsigned char *dp = dst;
    for (int r = 0 ; r < dst_height ; r++, sp += 2, dp += dst_stride) {
      *dp = (unsigned char)(((convert565(sp[0]) + convert565(sp[1]) + convert565(sp[src_stride+0]) + convert565(sp[src_stride+1])) >> 3) & 0xFF);
    }
  }
}

void unpack_cardsuit(unsigned short *src, int src_stride, unsigned char *dst, int dst_stride)
{
  //unpack_565(src + CARD_COL + CARD_ROW*src_stride, src_stride, dst, CARD_WIDTH, CARD_HEIGHT, dst_stride);
  //unpack_565(src + SUIT_COL + SUIT_ROW*src_stride, src_stride, dst + CARD_HEIGHT*dst_stride, SUIT_WIDTH, SUIT_HEIGHT, dst_stride);
  unpack_565(src + CARDSUIT_COL + CARDSUIT_ROW*src_stride, src_stride, dst, CARDSUIT_WIDTH, CARDSUIT_HEIGHT, dst_stride);
}

void image_init()
{
}