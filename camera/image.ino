// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
#include "defs.h"

Image::Image() : data(NULL), width(0), height(0), stride(0), owned(false)
{
}

Image::Image(const Image &other) : data(other.data), width(other.width), height(other.height), stride(other.stride), owned(false)
{
}

Image::Image(int width, int height) : width(width), height(height), stride(width), owned(true)
{
  data = (pixel *)malloc(width * height);
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
    ::free(data);
  }
  this->data = (pixel *)malloc(width * height * sizeof(pixel));
  if (data == NULL) {
    dprintf("WARNING: failed to allocate %d bytes for %dx%d image", width * height * sizeof(pixel), width, height);
  }
  this->width = width;
  this->height = height;
  this->stride = width;
  this->owned = true;
  //dprintf("allocated %p -> %dx%d", this->data, this->width, this->height);
  return data != NULL;
}

Image Image::crop(int x, int y, int w, int h)
{
  x = max(0, min(x, width));
  y = max(0, min(y, height));
  w = min(w, width - x);
  h = min(h, height - y);
  return Image(addr(x, y), w, h, stride);
}

void Image::copy(int x, int y, const Image &src)
{
  x = max(0, min(x, width));
  y = max(0, min(y, height));
  int w = min(src.width, width - x);
  int h = min(src.height, height - y);

  const pixel *sp = src.data;
  pixel *dp = addr(x, y);
  for (int r = 0 ; r < h ; r++, sp += src.stride, dp += stride) {
    bcopy(sp, dp, w);
  }
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

  // adjust brightness per line into tmp
  tmp.init(CARDSUIT_WIDTH, height);
  for (int r = 0 ; r < tmp.height ; r++) {
    pixel *src = data + x + r*stride;
    pixel *dst = tmp.data + r*tmp.stride;
    //int pmax = 250 - (r * 17)/10;
    //int pmin = 150 - (r * 125)/100;
    //int offset = pmin;
    //int scale = pmax - offset;
    int offset = 0;
    int scale = 255;
    //dprintf("%d: pmin=%d, pmax=%d, offset=%d, scale=%d", r, pmin, pmax, offset, scale);
    for (int c = 0 ; c < CARDSUIT_WIDTH ; c++) {
      dst[c] = max(0, min((src[c] - offset) * 255 / scale, 255));
    }
  }

  // determine vertical location
  int ycard = tmp.vlocate(4, 25);
  int ysuit = tmp.vlocate(ycard + SUIT_OFFSET - 1, ycard + SUIT_OFFSET + 10);
  //dprintf("locate X=%d, YC=%d, YS=%d", x, ycard, ysuit);
  card = tmp.crop(0, ycard, CARD_WIDTH, CARD_HEIGHT);
  suit = tmp.crop(0, ysuit, SUIT_WIDTH, SUIT_HEIGHT);
  return true;
}

int Image::vlocate(int ymin, int ymax)
{
  ymin = max(0, min(ymin, height));
  ymax = max(0, min(ymax, height));
  for (int y = ymin ; y < ymax ; y++) {
    pixel *src = addr(0, y);
    int pmin = *src++;
    int pmax = pmin;
    for (int x = 1 ; x < width ; x++, src++) {
      pmin = min(pmin, *src);
      pmax = max(pmax, *src);
    }
    if (pmax - pmin > 60) {
      return y;
    }
  }
  return ymin;
}

int Image::match(const Image &img)
{
  int n = width / img.width;
  int bestd = 999999999;
  int bestm = -1;
  for (int i = 0 ; i < n ; i++) {
    int dist = 0;
    for (int r = 0 ; r < img.height ; r++) {
      const pixel *p1 = addr(i*img.width, r);
      const pixel *p2 = img.addr(0, r);
      for (int c = 0 ; c < img.width ; c++, p1++, p2++) {
        int d = *p1 - *p2;
        dist += d*d;
      }
    }
    dprintf("match %d = %d", i, dist);
    if (bestd > dist) {
      bestd = dist;
      bestm = i;
    }
  }
  dprintf("bestm=%d, bestd=%d", bestm, bestd);
  return bestm;
}

void Image::free()
{
  if (owned) {
    ::free(data);
    data = NULL;
  }
}

Image::~Image()
{
  Image::free();
}

void image_init()
{
}