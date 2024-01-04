// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
#include "defs.h"

Image::Image() : data(NULL), width(0), height(0), stride(0), owned(false)
{
}

Image::Image(int width, int height) : width(width), height(height), stride(width), owned(true)
{
  data = (unsigned char *)malloc(width * height);
}

Image::Image(pixel *data, int width, int height, int stride) : data(data), width(width), height(height), stride(stride), owned(false)
{
}

bool Image::init(int width, int height)
{
  if (owned && data != NULL) {
    if (this->width == width && this->height == height) {
      return true;
    }
    free(data);
  }
  this->data = (pixel *)malloc(width * height * sizeof(pixel));
  this->width = width;
  this->height = height;
  this->stride = width;
  this->owned = true;
  return data != NULL;
}

Image Image::crop(int x, int y, int w, int h)
{
  return Image(data + x + y*stride, w, h, stride);
}

void Image::copy(int x, int y, Image &src)
{
  src.debug("copy from");
  this->debug("copy to");

  pixel *sp = src.data;
  pixel *dp = data + x + y*stride;
  for (int r = 0 ; r < src.height ; r++, sp += src.stride, dp += stride) {
    bcopy(sp, dp, src.width);
  }
}

bool Image::locate(Image &tmp, Image &card, Image &suit)
{
  int n = width;
  std::unique_ptr<int[]> sums(new int[n]);
  bzero((void *)sums.get(), n*sizeof(int));

  // determine horizontal position
  for (int c = 0 ; c < width ; c++) {
    int sum = 0;
    pixel *p = data + c;
    for (int i = 0 ; i < height ; i++, p += stride) {
      sum += *p;
    }
    sums[c] = sum;
  }
  // smooth and locate horizontally
  int bestx = 0;
  int x = 0;
  int *p = sums.get();
  for (int c = 1 ; c < width - CARDSUIT_WIDTH - 1; c++, p++) {
    int sum = p[0] + p[1] + p[2] + p[CARDSUIT_WIDTH + 0] + p[CARDSUIT_WIDTH + 1] + p[CARDSUIT_WIDTH + 2];
    if (bestx < sum) {
      bestx = sum;
      x = c;
    }
  }

  // threshold into tmp
  tmp.init(CARDSUIT_WIDTH, height);
  for (int r = 0 ; r < height ; r++) {
    pixel *src = data + x + r*stride;
    pixel *dst = tmp.data + r*tmp.stride;
    int scale = src[0];
    for (int c = 0 ; c < CARDSUIT_WIDTH ; c++) {
      dst[c] = min(src[c] * 255 / scale, 255);
    }
  }

  // determine vertical position, take into account the brightness ramp
  int besty = 0;
  int y = 0;
  for (int r = 4 ; r < 25 ; r ++) {
    pixel *p1 = tmp.data + r*tmp.stride;
    pixel *p2 = p1 + tmp.stride;
    int sum = 0;
    for (int c = 0 ; c < CARDSUIT_WIDTH ; c++, p1++, p2++) {
      sum += *p1 - *p2;
    }
    if (besty < sum) {
      besty = sum;
      y = r;
    }
    //dprintf("%d: %4d, %4d", r, s1, sum);
  }
  dprintf("locate X=%d, Y=%d", x, y);
  card = tmp.crop(0, y, CARD_WIDTH, CARD_HEIGHT);
  suit = tmp.crop(0, y+SUIT_OFFSET, SUIT_WIDTH, SUIT_HEIGHT);
  tmp.debug("tmp");
  card.debug("card");
  suit.debug("suit");
  return true;
}

void Image::clear()
{
  for (int r = 0 ; r < height ; r++) {
    memset(data + r*stride, 0, width);
  }
}
void Image::debug(const char *msg)
{
  dprintf("%s[%p,%dx%d,%d%s]",msg, data, width, height, stride, owned ? ",owned" : "");
}

Image::~Image()
{
  if (owned) {
    free(data);
    data = NULL;
  }
}

void image_init()
{
}