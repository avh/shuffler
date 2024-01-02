// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
#include <camera.h>
#include "gc2145.h"

static FrameBuffer fb;
static GC2145 galaxyCore;
static Camera cam(galaxyCore);

unsigned char *frame_data = NULL;
int frame_nr = 0;
size_t frame_size = 0;
int frame_width = 0;
int frame_height = 0;

void capture_init()
{
  if (!cam.begin(CAMERA_R160x120, CAMERA_RGB565, 30)) {
    dprintf("camera failed");
  }
  frame_width = 160;
  frame_height = 120;
}

int capture_frame()
{
  int result = cam.grabFrame(fb, 60);
  if (result != 0) {
    return result;
  }
  frame_data = fb.getBuffer();
  frame_size = cam.frameSize();
  frame_nr = frame_nr + 1;
  return 0;
}