// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
#include <WiFi.h>
#include <Wire.h>   
#include "defs.h"
#include "http.h"

#define LIGHT_PIN         3

unsigned long flash_on = 0;

int last_frame_nr = 0;
int last_card_nr = 0;
extern int frame_nr;
extern unsigned char *frame_data;
extern size_t frame_size;
extern int frame_width;
extern int frame_height;

unsigned char *cardsuit_data = NULL;
int cardsuit_stride = 0;
int cardsuit_size = 0;
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

void send_frame_png(HTTP& http)
{
  int pnglen = 3*frame_width*frame_height;
  auto pngbuf = std::shared_ptr<uint8_t[]>(new uint8_t[pnglen]);
  int content_length = png_encode_565(pngbuf.get(), pnglen, (unsigned short *)frame_data, frame_width, frame_height, frame_width);
  if (content_length >= 0) {
    http.begin(200, "File Follows");
    http.printf("Content-Length: %d\n", content_length);
    http.printf("Content-Type: image/png\n");
    http.end();
    http.write(pngbuf.get(), content_length);
  } else {
    http.begin(404, "PNG failed");
    http.end();
  }
}

void send_frame(HTTP &http) 
{
  struct {
    unsigned short frame_nr;
    unsigned short width;
    unsigned short height;
  } hdr = {
    frame_nr, frame_width, frame_height
  };

  http.begin(200, "OK");
  http.printf("Content-type: application/octet-stream\n");
  http.printf("Content-Length: %d\n", sizeof(hdr) + frame_size);
  http.end();
  http.write((unsigned char *)&hdr, sizeof(hdr));
  http.write(frame_data, frame_size);
}

void handle_frame(HTTP &http)
{
  if (frame_nr == last_frame_nr) {
    http.begin(404, "No New Frame Available");
    http.end();
  } else {
    last_frame_nr = frame_nr;
    send_frame(http);
  }
}

void send_cardsuit(HTTP &http) 
{
  if (cardsuit_data == NULL) {
    http.begin(404, "No Data Allocated");
    http.end();
    return;
  }

  struct {
    unsigned short frame_nr;
    unsigned short width;
    unsigned short height;
  } hdr = {
    frame_nr, CARDSUIT_WIDTH*14, CARDSUIT_HEIGHT*4
  };

  http.begin(200, "OK");
  http.printf("Content-type: application/octet-stream\n");
  http.printf("Content-Length: %d\n", sizeof(hdr) + cardsuit_size);
  http.end();
  http.write((unsigned char *)&hdr, sizeof(hdr));
  http.write(cardsuit_data, cardsuit_size);
}

void handle_cardsuit(HTTP& http)
{
  if (frame_nr == last_card_nr) {
    http.begin(404, "No New Card Available");
    http.end();
  } else {
    last_card_nr = frame_nr;
    send_cardsuit(http);
  }
}

void handle_snapshot(HTTP &http)
{
  if (flash_on == 0) {
    digitalWrite(LIGHT_PIN, HIGH);
    delay(100);
  }
  capture_frame();
  send_frame_png(http);
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
  http.printf("captured frame_nr=%d, %dx%d, bytes=%u, cardsuit=%d,%d, result=%d\n", 
    frame_nr, frame_width, frame_height, frame_size, cardsuit_row, cardsuit_col, result);

  if (result == 0 && cardsuit_data != NULL) {
    unsigned char *dst = cardsuit_data + cardsuit_col * CARDSUIT_WIDTH + cardsuit_row * CARDSUIT_HEIGHT * cardsuit_stride;
    unpack_cardsuit((unsigned short *)frame_data, frame_width, dst, cardsuit_stride);
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
  if (cardsuit_data == NULL) {
    cardsuit_stride = CARDSUIT_NCOLS * CARDSUIT_WIDTH;
    cardsuit_size = CARDSUIT_NROWS * CARDSUIT_HEIGHT * cardsuit_stride;
    cardsuit_data = (unsigned char *)malloc(cardsuit_size);
    if (cardsuit_data != NULL) {
      dprintf("allocated %d bytes, no problem", cardsuit_size);
      bzero(cardsuit_data, cardsuit_size);
    } else {
      dprintf("failed to allocate %d bytes, oops", cardsuit_size);
    }
  }
  if (cardsuit_data == NULL) {
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
  bzero(cardsuit_data, cardsuit_size);

  int count = 0;
  for (int i = 0 ; i < 53 ; i++) {
    unsigned long ms = millis();
    // capture
    int capture_result = capture_frame();
    dprintf("%3d: capture %dms, result=%d", i, millis() - ms, capture_result);
    if (capture_result == 0) {
      count++;
      unsigned char *dst = cardsuit_data + cardsuit_col * CARDSUIT_WIDTH + cardsuit_row * CARDSUIT_HEIGHT * cardsuit_stride;
      unpack_cardsuit((unsigned short *)frame_data, frame_width, dst, cardsuit_stride);
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

  dprintf("=== camera module ===");
  Wire.begin();
  Wire.setTimeout(1000);
  capture_init();
  image_init();
  http_init();

  if (true) {
    wifi_init("Vliegveld", "AB12CD34EF56G");
    HTTP::add("/capture", handle_capture);
    HTTP::add("/snapshot.png", handle_snapshot);
    HTTP::add("/frame", handle_frame);
    HTTP::add("/eject", handle_eject);
    HTTP::add("/cardsuit", handle_cardsuit);
    HTTP::add("/calibrate", handle_calibrate);
  }

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
