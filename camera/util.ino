// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
#include <stdarg.h>

void dprintf(char *fmt, ...)
{
  char buf[64];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  Serial.write(buf);
  Serial.write("\n");
}

#define MAX_EVENTS   100

struct event {
  int tm;
  const char *cmd;
  int arg;
};
static struct event *evts = NULL;
static int nevts = 0;

void reset_events()
{
  nevts = 0;
}

void add_event(char *cmd, int arg) 
{
  if (evts == NULL) {
    evts = (struct event *)malloc(MAX_EVENTS * sizeof(struct event));
  }
  if (evts != NULL && nevts < MAX_EVENTS) {
    evts[nevts].tm = millis();
    evts[nevts].cmd = cmd;
    evts[nevts].arg = arg;
    nevts++;
  }
}

void dump_events()
{
  for (int i = 0 ; i < nevts ; i++) {
      dprintf("%4d: %4ld %s %d", i, evts[i].tm - evts[0].tm, evts[i].cmd, evts[i].arg);
  }
  nevts = 0;
}

void util_init(const char *msg)
{
  Serial.begin(SERIAL_BAUDRATE);
  for (int tm = millis() ; millis() < tm + 10000 && !Serial ; delay(1));
  dprintf("== %s ==\n", msg);
}