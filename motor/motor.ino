#include <Wire.h>
#include "defs.h"

#define BUTTON1_PIN       3
#define BUTTON2_PIN       4

volatile int next_cmd = 0;
volatile int next_result = 0;

void handle_recv(int n) {
  //dprintf("handle_recv %d", n);
  int cmd = Wire.read();
  switch (cmd) {
    case MOTOR_CMD_EJECT:
      next_cmd = MOTOR_CMD_EJECT;
      break;
  }
}

void handle_req(int n) {
  //dprintf("handle_recv %d", n);
  Wire.write(next_result);
  next_result = 0;
}

void setup() {
  util_init("motor");
  hopper_init();

  Wire.begin(MOTOR_ADDR);
  //Wire.onRequest(handle_req);
  Wire.onReceive(handle_recv);
  Wire.onRequest(handle_req);
}

void loop() {
  // put your main code here, to run repeatedly:
  if (digitalRead(BUTTON1_PIN) == HIGH) {
    int result = eject_card();
    dprintf("eject_card=%d", result);
    while (digitalRead(BUTTON1_PIN) == HIGH);
  }
  if (digitalRead(BUTTON2_PIN) == HIGH) {
    dump_events();
     while (digitalRead(BUTTON2_PIN) == HIGH);
  }
  if (next_cmd == MOTOR_CMD_EJECT) {
    dprintf("got eject cmd");
    int result = eject_card();
    dprintf("eject result=%d", result);
    next_result = result;
    next_cmd = 0;
  }

  hopper_check();
}
