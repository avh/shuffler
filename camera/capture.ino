// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
#include <camera.h>
#include <gc2145.h>
#include "image.h"

Image frame;
int frame_nr = 0;;

static FrameBuffer fb;
static GC2145 galaxyCore;
static Camera cam(galaxyCore);

#define r565(v)     ((v) & 0x00F8)
#define g565(v)     ((((v) & 0x0007) << 5) | (((v) & 0xE000) >> 11))
#define b565(v)     (((v) & 0x1F00) >> 5)

#define convert565(v) (b565(v) + g565(v))

// unpack 565, convert from little endian to grayscale, rotate, scale down
static void unpack_565(unsigned short *src, int src_stride, Image &dst)
{
  pixel *dstp = dst.data;
  for (int c = 0 ; c < dst.width ; c++, src += 2*src_stride, dstp += 1) {
    unsigned short *sp = src;
    unsigned char *dp = dstp;
    for (int r = 0 ; r < dst.height ; r++, sp += 2, dp += dst.stride) {
      *dp = (unsigned char)(((convert565(sp[0]) + convert565(sp[1]) + convert565(sp[src_stride+0]) + convert565(sp[src_stride+1])) >> 3) & 0xFF);
    }
  }
}

void capture_init()
{
  if (!cam.begin(CAMERA_R160x120, CAMERA_RGB565, 30)) {
    dprintf("camera failed");
  }
  if (!frame.init(120/2, 160/2)) {
    dprintf("frame alloc failed");
  }
}

int capture_frame()
{
  int result = cam.grabFrame(fb, 60);
  if (result != 0) {
    return result;
  }
  unpack_565((unsigned short *)fb.getBuffer(), 160, frame);
  frame_nr = frame_nr + 1;
  return 0;
}