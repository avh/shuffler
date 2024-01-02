#include <WiFi.h>
#include <Wire.h>   
#include "defs.h"

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

void send_frame_png(WiFiClient& client)
{
  int pnglen = 3*frame_width*frame_height;
  uint8_t *pngbuf = (uint8_t *)malloc(pnglen);
  int content_length = png_encode_565(pngbuf, pnglen, (unsigned short *)frame_data, frame_width, frame_height, frame_width);
  if (content_length >= 0) {
    char buf[64];
    client.println("HTTP/1.1 200 File Follows");
    client.println("Connection: close");
    snprintf(buf, sizeof(buf), "Content-Length: %d", content_length);
    client.println(buf);
    client.println("Content-Type: image/png");
    client.println();
    client.write(pngbuf, content_length);
  } else {
    client.println("HTTP/1.1 404 PNG failed");
    client.println("Connection: close");
    client.println();
  }
  delete(pngbuf);
}

void send_frame(WiFiClient& client, String& path) 
{
  struct {
    unsigned short frame_nr;
    unsigned short width;
    unsigned short height;
  } hdr = {
    frame_nr, frame_width, frame_height
  };

  char buf[64];

  client.println("HTTP/1.1 200 OK");
  client.println("Content-type: application/octet-stream");  //multipart/byteranges
  snprintf(buf, sizeof(buf), "Content-Length: %u", sizeof(hdr) + frame_size);
  client.println(buf);
  client.println("Connection: close");
  client.println();
  client.write((unsigned char *)&hdr, sizeof(hdr));
  client.write(frame_data, frame_size);
}

void handle_frame(WiFiClient& client, String& path)
{
  if (frame_nr == last_frame_nr) {
    client.println("HTTP/1.1 404 No New Frame Available");
    client.println("Connection: close");
    client.println();
    return;
  }
  last_frame_nr = frame_nr;
  send_frame(client, path);
}

void send_cardsuit(WiFiClient& client, String& path) 
{
  if (cardsuit_data == NULL) {
    client.println("HTTP/1.1 404 No Data Allocated");
    client.println("Connection: close");
    client.println();
    return;
  }

  struct {
    unsigned short frame_nr;
    unsigned short width;
    unsigned short height;
  } hdr = {
    frame_nr, CARDSUIT_WIDTH*14, CARDSUIT_HEIGHT*4
  };

  char buf[64];

  client.println("HTTP/1.1 200 OK");
  client.println("Content-type: application/octet-stream");  //multipart/byteranges
  snprintf(buf, sizeof(buf), "Content-Length: %u", sizeof(hdr) + cardsuit_size);
  client.println(buf);
  client.println("Connection: close");
  client.println();
  client.write((unsigned char *)&hdr, sizeof(hdr));
  client.write(cardsuit_data, cardsuit_size);
}

void handle_cardsuit(WiFiClient& client, String& path)
{
  if (frame_nr == last_card_nr) {
    client.println("HTTP/1.1 404 No New Card Available");
    client.println("Connection: close");
    client.println();
    return;
  }
  last_card_nr = frame_nr;
  send_cardsuit(client, path);
}

void handle_snapshot(WiFiClient& client, String& path)
{
  if (flash_on == 0) {
    digitalWrite(LIGHT_PIN, HIGH);
    delay(100);
  }
  capture_frame();
  send_frame_png(client);
}

void handle_capture(WiFiClient& client, String& path) 
{
  if (flash_on == 0) {
    digitalWrite(LIGHT_PIN, HIGH);
    delay(100);
  }
  flash_on = millis();
  int result = capture_frame();
  wifi_response(client, "captured frame_nr=%d, %dx%d, bytes=%u, cardsuit=%d,%d, result=%d", 
    frame_nr, frame_width, frame_height, frame_size, cardsuit_row, cardsuit_col, result);

  if (result == 0 && cardsuit_data != NULL) {
    unsigned char *dst = cardsuit_data + cardsuit_col * CARDSUIT_WIDTH + cardsuit_row * CARDSUIT_HEIGHT * cardsuit_stride;
    unpack_cardsuit((unsigned short *)frame_data, frame_width, dst, cardsuit_stride);
  }
}

void handle_eject(WiFiClient& client, String& path) 
{
  int eject_result = eject_card();
  wifi_response(client, "eject result=%d", eject_result);

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

void handle_calibrate(WiFiClient& client, String& path) 
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
    client.println("HTTP/1.1 404 Could not allocate Calibration Data");
    client.println("Connection: close");
    client.println();
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

  wifi_response(client, "calibrate count=%d", count);
  flash_on = millis();
  frame_nr++;
}

void setup() {
  Serial.begin(115200);
  while (!Serial);
  flash_init();

  dprintf("=== camera module ===");
  Wire.begin();
  Wire.setTimeout(1000);
  capture_init();
  image_init();

  if (true) {
    wifi_init("Vliegveld", "AB12CD34EF56G");
    wifi_handler("/capture", handle_capture);
    wifi_handler("/snapshot", handle_snapshot);
    wifi_handler("/frame", handle_frame);
    wifi_handler("/eject", handle_eject);
    wifi_handler("/cardsuit", handle_cardsuit);
    wifi_handler("/calibrate", handle_calibrate);
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
