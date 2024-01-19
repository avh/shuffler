// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
#include "defs.h"

#define CARD_PIN        2
#define DECK_PIN        5
#define MOTOR1_PINS     6
#define MOTOR2_PINS     9
#define FAN_PIN         12

class DCMotor {
  public:
    const char *name;
    int rpm6v;
    int pwm, mot1, mot2;
    int rpm, minRPM, maxRPM;
  public:
    DCMotor(const char *name, int rpm6v, int pins) : name(name), rpm6v(rpm6v), pwm(pins+0), mot1(pins+1), mot2(pins+2)
    {
      rpm = 0;
      minRPM = rpm6v / 6;
      maxRPM = rpm6v * 2; // 12v power supply (overdrive)
    }

    void init()
    {
      pinMode(pwm, OUTPUT);
      pinMode(mot1, OUTPUT);
      pinMode(mot2, OUTPUT);
    }
    void idle()
    {
      rpm = 0;
      analogWrite(pwm, 0);
      digitalWrite(mot1, LOW);
      digitalWrite(mot2, LOW);
    }
    void brake()
    {
      rpm = 0;
      analogWrite(pwm, 0);
      digitalWrite(mot1, HIGH);
      digitalWrite(mot2, HIGH);
    }
    void speed(int rpm)
    {
      if (rpm == 0) {
        brake();
        return;
      }
      rpm = max(-maxRPM, min(rpm, maxRPM));
      if (rpm > 0 && rpm < minRPM) {
        rpm = minRPM;
      } else if (rpm < 0 && rpm > -minRPM) {
        rpm = -minRPM;
      }
      int value = abs(rpm) * 1023L / maxRPM;
      value = min(value, 1023);
      analogWrite(pwm, value);
      digitalWrite(mot1, rpm < 0 ? LOW : HIGH);
      digitalWrite(mot2, rpm > 0 ? LOW : HIGH);
      this->rpm = rpm;
      add_event(name, value);
   }
};

DCMotor motor1("motor1", 130, MOTOR1_PINS);
DCMotor motor2("motor2", 2500, MOTOR2_PINS);

int feedRPM = 0;
int ejectRPM = 0;
unsigned long feedReverseMs = 0;
unsigned long cardDetectMs = 0;

int fanSpeed = 0;
unsigned long fanOffMs = 0;

enum eject_state {
  WAITING, FEEDING, EJECTING, RETRACTING, DONE
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
        case FEEDING:
          set_state(EJECTING);
          cardDetectMs = millis();
          break;
      }
  } else {
    add_event("void_interrupt", state);
    // no card detected
    switch (state) {
      case EJECTING:
        motor1.speed(-feedRPM);
        motor2.speed(-feedRPM);
        set_state(RETRACTING);
        break;
    }
  }
}

void stop_everything()
{
    motor1.idle();
    motor2.idle();
    fan_speed(0);
    fanOffMs = 0;
}

int retract_card() {
  add_event("rectact_card", 0);

  if (fanSpeed == 0) {
    fan_speed(1);
    delay(1000);
  }
  motor1.speed(-feedRPM);
  motor2.speed(-feedRPM);

  set_state(RETRACTING);
  while (state == RETRACTING && card_detected() && millis() < state_ms + 1000) {
    delayMicroseconds(1);
  }
  if (card_detected() == 0) {
    add_event("retract card success", 1);
    motor1.idle();
    motor2.idle();
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

  set_state(FEEDING);
  motor1.speed(feedRPM);
  motor2.speed(feedRPM);
  while (1) {
    unsigned long now = millis();
    if (now > state_ms + 1000) {
      set_state(DONE);
      add_event("next event timed out", state);
      stop_everything();
      return HOPPER_STUCK;
    }
    switch (state) {
      case FEEDING:
        if (now > state_ms + 500) {
          set_state(DONE);
          add_event("eject card failed (hopper empty)", HOPPER_EMPTY);
          stop_everything();
          return HOPPER_EMPTY;
        }
        break;
      case EJECTING:
        if (cardDetectMs != 0 && now > cardDetectMs + feedReverseMs) {
          motor1.speed(-feedRPM);
          motor2.speed(ejectRPM);
          cardDetectMs = 0;
        }
        break;
      case RETRACTING:
        if (now < state_ms + 20 || card_detected()) {
          break;
        }
        add_event("retract complete", now - state_ms);
        set_state(DONE);
        // fall through
      case DONE:
        delay(10);
        if (card_detected()) {
          add_event("eject card failed (card still detected)", 0);
          stop_everything();
          return HOPPER_STUCK;
        }
        if (!deck_detected()) {
          fan_speed(0);
          fanOffMs = 0;
        }
        add_event("eject card success", 0);
        motor1.idle();
        motor2.idle();
        return EJECT_OK;
    }
  }
  add_event("cant happen", 0);
}

int set_eject_speed(int cardsPerSecond)
{
  float wheelRadius = 23;
  float wheelCircumference = 2*3.1415*wheelRadius;
  float cardLength = 90;
  float cardDisengageLength = 60;

  float desiredRPM = 60 * cardsPerSecond * cardLength / wheelCircumference;

  ejectRPM = max(motor2.minRPM, min(desiredRPM, motor2.maxRPM));
  feedRPM = max(motor1.minRPM, min(ejectRPM, motor1.maxRPM));
  feedReverseMs = (1000 * 60.0 * cardDisengageLength) / (feedRPM * wheelCircumference); 
  dprintf("ejectRPM=%d, feedRPM=%d, feedReverseMs=%lu", ejectRPM, feedRPM, feedReverseMs);
}

void hopper_init()
{
  motor1.init();
  motor2.init();

  pinMode(DECK_PIN, INPUT);
  pinMode(CARD_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CARD_PIN), card_interrupt, CHANGE);
  pinMode(FAN_PIN, OUTPUT);

  set_eject_speed(5);
}

void hopper_check()
{
  if (fanOffMs != 0 && millis() > fanOffMs) {
    fan_speed(0);
    fanOffMs = 0;
  }
}
