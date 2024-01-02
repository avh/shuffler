#include <camera.h>
#include "gc2145.h"

int frame_nr = 0;
FrameBuffer fb;
GC2145 galaxyCore;
Camera cam(galaxyCore);
unsigned char *frame_data;
size_t frame_size;
int frame_width;
int frame_height;

void capture_init()
{
  if (!cam.begin(CAMERA_R160x120, CAMERA_RGB565, 30)) {
    dprintf("camera failed");
  }
}

int capture_frame()
{
  int result = cam.grabFrame(fb, 60);
  if (result != 0) {
    return result;
  }
  frame_data = fb.getBuffer();
  frame_size = cam.frameSize();
  frame_width = 160;
  frame_height = 120;
  frame_nr = frame_nr + 1;
  return 0;
}