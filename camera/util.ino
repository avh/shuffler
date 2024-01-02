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

#define NEVTS   100
unsigned long tms[NEVTS];
char *cmds[NEVTS];
int args[NEVTS];
int nevents = 0;

void reset_events()
{
  nevents = 0;
}

void add_event(char *cmd, int arg) {
  if (nevents < NEVTS) {
    tms[nevents] = millis();
    cmds[nevents] = cmd;
    args[nevents] = arg;
    nevents++;
  }
}

void dump_events()
{
  unsigned long tm = tms[0];
  for (int i = 0 ; i < nevents ; i++) {
      dprintf("%4d: %4ld %s %d", i, tms[i] - tm, cmds[i], args[i]);
  }
  nevents = 0;
}