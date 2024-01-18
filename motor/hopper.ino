// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
#include <AccelStepper.h>
#include "defs.h"

#define CARD_PIN        2
#define DECK_PIN        5
#define MOTOR_STEP_PIN  6
#define MOTOR_DIR_PIN   7
#define MOTOR_ENABLE_PIN 8
#define FAN_PIN         12

AccelStepper stepper(1, MOTOR_STEP_PIN, MOTOR_DIR_PIN);

int speed = 100;
int fanSpeed = 0;
unsigned long fanOffMs = 0;

enum eject_state {
  WAITING, LOADING, EJECTING, RETRACTING, DONE
};
volatile unsigned long state_ms = 0;
volatile eject_state state = WAITING;

void set_state(int s) 
{
  add_event("set_state", s);
  state = (eject_state)s;
  state_ms = millis();
}

void fan_speed(int speed)
{
  if (speed != fanSpeed) {
    digitalWrite(FAN_PIN, speed == 0 ? LOW : HIGH);
    fanSpeed = speed;
    add_event("fan", speed);
  }
}

int deck_detected()
{
  int v = digitalRead(DECK_PIN) == HIGH;
  add_event("deck_detected", v);
  return v;
}

int card_detected() 
{
  int v = digitalRead(CARD_PIN) == HIGH;
  add_event("card_detected", v);
  return v;
}

void card_interrupt() {
  if (digitalRead(CARD_PIN) == HIGH) {
   add_event("card_interrupt", state);
    // card detected
    switch (state) {
        case LOADING:
          //stepper.moveTo(stepper.currentPosition() + 200);
          stepper.setMaxSpeed(5000);
          stepper.setAcceleration(10000);
          set_state(EJECTING);
          break;
      }
  } else {
    add_event("void_interrupt", state);
    // no card detected
    switch (state) {
      case EJECTING:
        stepper.setCurrentPosition(0);
        stepper.setMaxSpeed(2000);
        stepper.moveTo(-50);
        set_state(RETRACTING);
        break;
    }
  }
}

void stop_everything()
{
    stepper.disableOutputs();
    stepper.setSpeed(0);
    fan_speed(0);
    fanOffMs = 0;
}

int retract_card() {
  add_event("rectact_card", 0);

  if (fanSpeed == 0) {
    fan_speed(1);
    delay(1000);
  }
  stepper.enableOutputs();
  stepper.setCurrentPosition(0);
  stepper.setMaxSpeed(2000);
  stepper.setAcceleration(1000);
  stepper.moveTo(-500);
  add_event("dist", stepper.distanceToGo());

  set_state(RETRACTING);
  while (state == RETRACTING && stepper.distanceToGo() != 0 && millis() < state_ms + 500) {
    stepper.run();
  }
  add_event("dist", stepper.distanceToGo());
  if (card_detected() == 0) {
    add_event("retract card success", 1);
    stepper.setSpeed(0);
    stepper.disableOutputs();
    return 1;
  }
  add_event("retract card failed", 0);
  stop_everything();
  return 0;
}


int eject_card() {
  reset_events();
  add_event("eject_card", 0);
  if (card_detected() && !retract_card()) {
    stop_everything();
    return HOPPER_STUCK;
  }
  if (!deck_detected()) {
    add_event("hopper_empty", 0);
    stop_everything();
    return HOPPER_EMPTY;
  }
  if (fanSpeed == 0) {
    fan_speed(1);
    delay(1000);
  }
  fanOffMs = millis() + 5*1000;

  set_state(LOADING);
  stepper.enableOutputs();
  stepper.setCurrentPosition(0);
  stepper.setMaxSpeed(2000);
  stepper.setAcceleration(1000);
  stepper.moveTo(1000);
  while (1) {
    unsigned long now = millis();
    if (now > state_ms + 500) {
      set_state(DONE);
      add_event("next event timed out", state);
      stop_everything();
      return HOPPER_STUCK;
    }
    switch (state) {
      case LOADING:
        if (now > state_ms + 400) {
          set_state(DONE);
          add_event("eject card failed (hopper empty)", HOPPER_EMPTY);
          stop_everything();
          return HOPPER_EMPTY;
        }
        break;
      case RETRACTING:
        if (now < state_ms + 200 || card_detected()) {
          break;
        }
        add_event("retract abort", now - state_ms);
        set_state(DONE);
        // fall through
      case DONE:
        delay(10);
        if (card_detected()) {
          add_event("eject card failed (card still detected)", 0);
          stop_everything();
          return HOPPER_STUCK;
        }
        add_event("eject card success", 0);
        stepper.setSpeed(0);
        stepper.disableOutputs();
        return EJECT_OK;
    }
    stepper.run();
  }
  add_event("cant happen", 0);
}

/*
int eject_cards(int n) {
  add_event("eject cards", n);
  if (card_detected() && !retract_card()) {
    add_event("retract failed", 0);
    return HOPPER_STUCK;
  }
  for (int i = 0 ; i < n ; i++) {
    int result = eject_card();
    if (result != 1) {
      dprintf("failed ejecting %d/%d, state=%d, result=%d", i+1, n, state, result);
      add_event("eject cards failed", i);
      return i;
    }
    delay(1);
  }
  add_event("eject cards success", n);
  return n;
}

int count_cards(int delay_ms = 0) {
  add_event("count cards", 0);
  if (card_detected() && !retract_card()) {
    add_event("retract failed", 0);
    return HOPPER_STUCK;
  }
  unsigned long tm = millis();
  int count = 0;
  while (1) {
    int result = eject_card();
    switch (result) {
    case 1:
      count += 1;
      delay(delay_ms);
      break;
    case HOPPER_EMPTY:
      if (count > 0) {
        unsigned long ms = millis() - tm;
        dprintf("counted %d cards in %dms, %d ms/card, %d cards/sec", count, ms / count, count * 1000 / ms);
      }
      return count;
    default:
      dprintf("count fail result=%d", result);
      return result;
    }
  }
}
*/

void hopper_init()
{
  stepper.setEnablePin(MOTOR_ENABLE_PIN);
  stepper.setMaxSpeed(10000);
  stepper.setAcceleration(5000);
  pinMode(DECK_PIN, INPUT);
  pinMode(CARD_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CARD_PIN), card_interrupt, CHANGE);
  pinMode(FAN_PIN, OUTPUT);
  stepper.disableOutputs();
}

void hopper_check()
{
  if (fanOffMs != 0 && millis() > fanOffMs) {
    fan_speed(0);
    fanOffMs = 0;
  }
  stepper.run();
}
