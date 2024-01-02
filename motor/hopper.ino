#include "defs.h"

#define DETECT_PIN        2
#define MOTOR_PWM_PIN     9
#define MOTOR_D1_PIN      10
#define MOTOR_D2_PIN      11

int speed = 255;

enum eject_state {
  WAITING, LOADING, EJECTING, RETRACTING, DONE
};
volatile unsigned long state_ms = 0;
volatile eject_state state = WAITING;

void set_state(int s) {
  add_event("set_state", s);
  state = (eject_state)s;
  state_ms = millis();
}

void motor_move(int fwd = 1) 
{
  analogWrite(MOTOR_PWM_PIN, speed);
  digitalWrite(MOTOR_D1_PIN, fwd > 0 ? LOW : HIGH);
  digitalWrite(MOTOR_D2_PIN, fwd > 0 ? HIGH : LOW);
  add_event("move", fwd);
}

void motor_brake() 
{
  analogWrite(MOTOR_PWM_PIN, 0);
  digitalWrite(MOTOR_D1_PIN, HIGH);
  digitalWrite(MOTOR_D2_PIN, HIGH);
  add_event("brake", state);
}

void motor_relax() 
{
  analogWrite(MOTOR_PWM_PIN, 0);
  digitalWrite(MOTOR_D1_PIN, LOW);
  digitalWrite(MOTOR_D2_PIN, LOW);
  add_event("relax", state);
}

int card_detected() {
  int v = digitalRead(DETECT_PIN) == HIGH;
  add_event("card_detected", v);
  return v;
}

void card_interrupt() {
  if (digitalRead(DETECT_PIN) == HIGH) {
   add_event("card_interrupt", state);
    // card detected
    switch (state) {
        case LOADING:
          set_state(EJECTING);
          break;
      }
  } else {
    add_event("void_interrupt", state);
    // no card detected
    switch (state) {
      case EJECTING:
        set_state(RETRACTING);
        motor_move(-1);
        break;
    }
  }
}

int retract_card() {
  add_event("rectact_card", 0);
  set_state(RETRACTING);
  motor_move(-1);
  while (state != DONE) {
    unsigned long now = millis();
    if (now > state_ms + 500 && card_detected()) {
      set_state(DONE);
      motor_relax();
      return 0;
    }
    delayMicroseconds(1);
  }
  return 1;
}

int eject_card() {
  reset_events();
  add_event("eject_card", 0);
  set_state(LOADING);
  motor_move(1);
  while (1) {
    unsigned long now = millis();
    if (now > state_ms + 250) {
      set_state(DONE);
      add_event("next event timed out", state);
      motor_relax();
      return HOPPER_STUCK;
    }
    switch (state) {
      case LOADING:
        if (now > state_ms + 200) {
          set_state(DONE);
          add_event("eject card failed (hopper empty)", HOPPER_EMPTY);
          motor_relax();
          return HOPPER_EMPTY;
        }
        break;
      case RETRACTING:
        if (now < state_ms + 100 || card_detected()) {
          break;
        }
        add_event("retract abort", now - state_ms);
        motor_relax();
        set_state(DONE);
        // fall through
      case DONE:
        delay(10);
        if (card_detected()) {
          add_event("eject card failed (card still detected)", 0);
          motor_relax();
          return HOPPER_STUCK;
        }
        add_event("eject card success", 0);
        motor_relax();
        return EJECT_OK;
    }
    delayMicroseconds(1);
  }
  add_event("cant happen", 0);
}

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

void init_hopper()
{
  pinMode(DETECT_PIN, INPUT_PULLUP);
  pinMode(MOTOR_PWM_PIN, OUTPUT);
  pinMode(MOTOR_D1_PIN, OUTPUT);
  pinMode(MOTOR_D2_PIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(DETECT_PIN), card_interrupt, CHANGE);
}
