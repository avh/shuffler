// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
#include <WiFi.h>
#include <Wire.h>   
#include "defs.h"
#include "http.h"
#include "image.h"

#define LIGHT_PIN         3

unsigned long flash_on = 0;

extern Image frame;
extern int frame_nr;

int last_frame_nr = 0;
int last_card_nr = 0;

Image tmp;
Image cardsuit;
int cardsuit_row = 0;
int cardsuit_col = 0;

int eject_card()
{
  Wire.beginTransmission(MOTOR_ADDR);
  Wire.write(MOTOR_CMD_EJECT);
  Wire.endTransmission();
  while (1) { 
    if (Wire.requestFrom(MOTOR_ADDR, 1) != 1) {
      return CMD_TIMEOUT;
    }
    //while (!Wire.available());
    int eject_result = Wire.read();
    if (eject_result != 0) {
      return eject_result;
    }
    delay(10);
  }
}

void handle_frame_bmp(HTTP& http)
{
  http.begin(200, "File Follows");
  http.printf("Content-Length: %d\n", bmp_file_size(frame));
  http.printf("Content-Type: image/bmp\n");
  http.end();
  bmp_http_write(http, frame);
}

void handle_frame(HTTP &http)
{
  if (frame_nr == last_frame_nr) {
    http.begin(404, "No New Frame Available");
    http.end();
  } else {
    last_frame_nr = frame_nr;
    handle_frame_bmp(http);
  }
}

void handle_cardsuit_bmp(HTTP &http) 
{
  if (cardsuit.data == NULL) {
    http.begin(404, "No Data Allocated");
    http.end();
    return;
  }
  http.begin(200, "File Follows");
  http.printf("Content-Type: image/bmp\n");
  http.printf("Content-Length: %d\n", bmp_file_size(cardsuit));
  http.end();
  bmp_http_write(http, cardsuit);
}

void handle_cardsuit(HTTP& http)
{
  if (false && frame_nr == last_card_nr) {
    http.begin(404, "No New Card Available");
    http.end();
  } else {
    last_card_nr = frame_nr;
    handle_cardsuit_bmp(http);
  }
}

void handle_capture(HTTP &http) 
{
  if (flash_on == 0) {
    digitalWrite(LIGHT_PIN, HIGH);
    delay(100);
  }
  flash_on = millis();
  int result = capture_frame();
  http.begin(200, "Capture OK");
  http.end();
  http.printf("captured frame_nr=%d, %dx%d, stride=%d, cardsuit=%d,%d, result=%d\n", 
    frame_nr, frame.width, frame.height, frame.stride, cardsuit_row, cardsuit_col, result);

  if (result == 0 && cardsuit.data != NULL) {
    cardsuit.debug("cardsuit");
    Image card, suit;
    if (frame.locate(tmp, card, suit)) {
      cardsuit.copy(cardsuit_col * CARDSUIT_WIDTH, cardsuit_row * CARDSUIT_HEIGHT, card);
      cardsuit.copy(cardsuit_col * CARDSUIT_WIDTH, cardsuit_row * CARDSUIT_HEIGHT + card.height + 2, suit);
    }
    dprintf("done");
  }
}

void handle_eject(HTTP &http) 
{
  int eject_result = eject_card();
  http.begin(200, "Eject OK");
  http.end();
  http.printf("eject result=%d\n", eject_result);

  if (eject_result == HOPPER_EMPTY) {
    cardsuit_row = 3;
    cardsuit_col = 13;
  } else if (eject_result == EJECT_OK) {
    cardsuit_col += 1;
    if (cardsuit_col >= 13) {
      cardsuit_col = 0;
      cardsuit_row = (cardsuit_row + 1) % 4;
    }
  }
}

void handle_calibrate(HTTP &http) 
{
  if (cardsuit.data == NULL) {
    cardsuit.init(CARDSUIT_NCOLS * CARDSUIT_WIDTH, CARDSUIT_NROWS * CARDSUIT_HEIGHT);
  }
  if (cardsuit.data == NULL) {
    http.begin(404, "Could not allocate Calibration Data");
    http.end();
    return;
  }

  if (flash_on == 0) {
    digitalWrite(LIGHT_PIN, HIGH);
    delay(100);
  }

  // reset
  cardsuit_col = 0;
  cardsuit_row = 0;
  cardsuit.clear();

  int count = 0;
  for (int i = 0 ; i < 53 ; i++) {
    unsigned long ms = millis();
    // capture
    int capture_result = capture_frame();
    dprintf("%3d: capture %dms, result=%d", i, millis() - ms, capture_result);
    if (capture_result == 0) {
      count++;
      Image card, suit;
      if (frame.locate(tmp, card, suit)) {
        dprintf("CARD %d, %d", cardsuit_row, cardsuit_col);
        cardsuit.copy(cardsuit_col * CARDSUIT_WIDTH, cardsuit_row * CARDSUIT_HEIGHT, card);
        cardsuit.copy(cardsuit_col * CARDSUIT_WIDTH, cardsuit_row * CARDSUIT_HEIGHT + card.height + 2, suit);
      }
    }
    ms = millis();

    // eject
    int eject_result = eject_card();
    dprintf("%3d: eject %dms, result=%d", i, millis() - ms, eject_result);
    if (eject_result != EJECT_OK) {
      break;
    }
    cardsuit_col += 1;
    if ((cardsuit_col == 13 && cardsuit_row < 3) || (cardsuit_col > 13)) {
      cardsuit_col = 0;
      cardsuit_row = (cardsuit_row + 1) % 4;
    }
  }
  http.begin(200, "Calibrate OK");
  http.end();
  http.printf("calibrate count=%d\n", count);
  flash_on = millis();
  frame_nr++;
}

void setup() {
  util_init("camera");
  flash_init();
  Wire.begin();
  Wire.setTimeout(1000);
  capture_init();
  image_init();
  http_init();

  if (true) {
    wifi_init("Vliegveld", "AB12CD34EF56G");
    HTTP::add("/capture", handle_capture);
    HTTP::add("/frame.bmp", handle_frame_bmp);
    HTTP::add("/eject", handle_eject);
    HTTP::add("/cardsuit.bmp", handle_cardsuit_bmp);
    HTTP::add("/calibrate", handle_calibrate);
  }

  cardsuit.init(CARDSUIT_NCOLS * CARDSUIT_WIDTH, CARDSUIT_NROWS * CARDSUIT_HEIGHT);
  cardsuit.clear();

  pinMode(LIGHT_PIN, OUTPUT);
  digitalWrite(LIGHT_PIN, LOW);
}

int cnt = 0;

void loop() {
  if (cnt++ == 0) {
    dprintf("in looop");
  }
  wifi_check();
  //flash_check();
  if (flash_on != 0 && millis() > flash_on + 10*1000) {
    digitalWrite(LIGHT_PIN, LOW);
    flash_on = 0;
  }
}
