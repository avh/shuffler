// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
#include <WiFi.h>
#include <Wire.h>   
#include "defs.h"
#include "http.h"
#include "image.h"

extern Image frame;
extern int frame_nr;

int last_frame_nr = 0;
int last_card_nr = 0;

Image tmp;
Image last_card;
Image last_suit;
Image cardsuit;
Image card_master;
Image suit_master;
int cardsuit_row = 0;
int cardsuit_col = 0;

int card_match(Image &card, Image &suit)
{
  if (card_master.data == NULL || suit_master.data == NULL) {
    return CARD_MATCH_NONE;
  }
  int c = card_master.match(card);
  if (c >= 0) {
    int s = suit_master.match(suit);
    if (c == 13 || s == 4) {
      return CARD_MATCH_EMPTY;
    }
    return s * 13 + c;
  }
  return CARD_MATCH_NONE;
}

const char *card_name(int c)
{
  if (c <= CARD_MATCH_NONE) {
    return "NONE";
  }
  if (c >= CARD_MATCH_EMPTY) {
    return "EMPTY";
  }
  static char *s_name[] = {"Clubs", "Diamonds", "Hearts", "Spades"};
  static char *c_name[] = {"2", "3", "4", "5", "6", "7", "8", "9", "10", "Jack", "Queen", "King", "Ace"};
  static char buf[64];
  snprintf(buf, sizeof(buf), "%s of %s", c_name[c%13], s_name[c/13]);
  return buf;
}

void handle_bmp(HTTP& http, Image &img, const char *msg = "File Follows")
{
  if (img.data == NULL) {
    http.begin(404, "Not Allocated");
    return;
  }
  http.begin(200, msg);
  http.printf("Content-Length: %d\n", bmp_file_size(img));
  http.printf("Content-Type: image/bmp\n");
  http.end();
  bmp_http_write(http, img);
}
void handle_frame_bmp(HTTP& http)
{
  handle_bmp(http, frame, "Frame Follows");
}
void handle_card_bmp(HTTP& http)
{
  handle_bmp(http, last_card, "Card Follows");
}
void handle_suit_bmp(HTTP& http)
{
  handle_bmp(http, last_suit, "Suit Follows");
}
void handle_cardsuit_bmp(HTTP& http)
{
  handle_bmp(http, cardsuit, "CardSuit Follows");
}

void ready_card()
{
  Wire.beginTransmission(MOTOR_ADDR);
  Wire.write(MOTOR_CMD_READY);
  Wire.endTransmission();
}

int eject_card()
{
  Wire.beginTransmission(MOTOR_ADDR);
  Wire.write(MOTOR_CMD_EJECT);
  Wire.endTransmission();
  while (1) { 
    int r = Wire.requestFrom(MOTOR_ADDR, 1);
    if (r != 1) {
      dprintf("CMD_TIMEOUT = %d", r);
      return CMD_TIMEOUT;
    }
    //while (!Wire.available());
    int eject_result = Wire.read();
    if (eject_result == HOPPER_EMPTY) {
      capture_light_off();
    }
    if (eject_result != 0) {
      return eject_result;
    }
    delay(10);
  }
}

void handle_cardsuit(HTTP& http)
{
  if (frame_nr == last_card_nr) {
    http.begin(404, "No New Card Available");
    http.end();
  } else {
    last_card_nr = frame_nr;
    handle_cardsuit_bmp(http);
  }
}

void handle_capture(HTTP &http) 
{
  int result = capture_frame();
  http.begin(200, "Capture OK");
  http.end();
  http.printf("captured frame_nr=%d, %dx%d, stride=%d, cardsuit=%d,%d, result=%d\n", 
    frame_nr, frame.width, frame.height, frame.stride, cardsuit_row, cardsuit_col, result);

  if (result == 0) {
    cardsuit.debug("cardsuit");
    if (frame.locate(tmp, last_card, last_suit)) {
      int c = card_match(last_card, last_suit);
      if (c != CARD_MATCH_NONE) {
        dprintf("MATCHED: %d = %s", c, card_name(c));
      }

      if (cardsuit.data != NULL) {
        cardsuit.copy(cardsuit_col * CARDSUIT_WIDTH, cardsuit_row * CARDSUIT_HEIGHT, last_card);
        cardsuit.copy(cardsuit_col * CARDSUIT_WIDTH, cardsuit_row * CARDSUIT_HEIGHT + CARD_HEIGHT + 2, last_suit);
       }
    }
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

void handle_train(HTTP &http) 
{
  if (cardsuit.data == NULL) {
    cardsuit.init(CARDSUIT_NCOLS * CARDSUIT_WIDTH, CARDSUIT_NROWS * CARDSUIT_HEIGHT);
  }
  if (cardsuit.data == NULL) {
    http.begin(404, "Could not allocate Training Data");
    http.end();
    return;
  }
  
  // reset
  cardsuit_col = 0;
  cardsuit_row = 0;
  cardsuit.clear();
  ready_card();

  int count = 0;
  for (int i = 0 ; i < 53 ; i++) {
    unsigned long ms = millis();
    // capture
    int capture_result = capture_frame();
    dprintf("%3d: capture %dms, result=%d", i, millis() - ms, capture_result);
    if (capture_result == 0) {
      count++;
      if (frame.locate(tmp, last_card, last_suit)) {
        cardsuit.copy(cardsuit_col * CARDSUIT_WIDTH, cardsuit_row * CARDSUIT_HEIGHT, last_card);
        cardsuit.copy(cardsuit_col * CARDSUIT_WIDTH, cardsuit_row * CARDSUIT_HEIGHT + CARD_HEIGHT + 2, last_suit);
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
  http.begin(200, "Train OK");
  http.end();
  http.printf("training sample count=%d\n", count);
  frame_nr++;

  if (cardsuit_row == 3 && cardsuit_col == 13) {
    card_master.init(CARD_WIDTH * 14, CARD_HEIGHT);
    suit_master.init(SUIT_WIDTH * 5, SUIT_HEIGHT);
    if (card_master.data == NULL || suit_master.data == NULL) {
      dprintf("failed to allocate master data");
    } else {
      // gather all the card data
      for (int card = 0 ; card < 13 ; card++) {
        int off = card*CARDSUIT_WIDTH;
        for (int r = 0 ; r < CARD_HEIGHT ; r++) {
          for (int c = 0 ; c < CARD_WIDTH ; c++) {
            int sum = 0;
            for (int i = 0 ; i < 4 ; i++) {
              sum += *cardsuit.addr(off + c, i * CARDSUIT_HEIGHT + r);
            }
            *card_master.addr(card*CARD_WIDTH + c, r) = sum/4;
          }
        }
      }
      card_master.copy(13 * CARD_WIDTH, 0, cardsuit.crop(CARD_WIDTH*13, CARDSUIT_HEIGHT*3, CARD_WIDTH, CARD_HEIGHT));

      // gather all the suit data
      for (int suit = 0 ; suit < 4 ; suit++) {
        int off = suit * CARDSUIT_HEIGHT + CARD_HEIGHT + 2;
        for (int r = 0 ; r < SUIT_HEIGHT ; r++) {
          for (int c = 0 ; c < SUIT_WIDTH ; c++) {
            int sum = 0;
            for (int i = 0 ; i < 13 ; i++) {
              sum += *cardsuit.addr(c + i * SUIT_WIDTH, off + r);
            }
            *suit_master.addr(suit*SUIT_WIDTH + c, r) = sum/13;
          }
        }
      }
      suit_master.copy(4 * SUIT_WIDTH, 0, cardsuit.crop(CARD_WIDTH*13, CARDSUIT_HEIGHT*3 + CARD_HEIGHT + 2, SUIT_WIDTH, SUIT_HEIGHT));
      bmp_save("/user/www/training.bmp", cardsuit);
      bmp_save("/user/www/cards.bmp", card_master);
      bmp_save("/user/www/suits.bmp", suit_master);
    }
  } else {
    dprintf("training data incomplete: %d,%d", cardsuit_row, cardsuit_col);
  }
}

void handle_check(HTTP &http) 
{
  if (cardsuit.data == NULL) {
    cardsuit.init(CARDSUIT_NCOLS * CARDSUIT_WIDTH, CARDSUIT_NROWS * CARDSUIT_HEIGHT);
  }
  if (cardsuit.data == NULL) {
    http.begin(404, "Could not allocate Training Data");
    http.end();
    return;
  }

  // reset
  cardsuit_col = 0;
  cardsuit_row = 0;
  cardsuit.clear();
  ready_card();

  int count = 0;
  for (int i = 0 ; i < 53 ; i++) {
    unsigned long ms = millis();
    // capture
    int capture_result = capture_frame();
    dprintf("%3d: capture %dms, result=%d", i, millis() - ms, capture_result);
    if (capture_result == 0) {
      count++;
      if (frame.locate(tmp, last_card, last_suit)) {
        int c = card_match(last_card, last_suit);
        if (cardsuit_col == 0 && cardsuit_row == 0 && c > CARD_MATCH_NONE && c < CARD_MATCH_EMPTY) {
          cardsuit_col = c % 13;
          cardsuit_row = c / 13;
          dprintf("starting at %s", card_name(c));
        }

        cardsuit.copy(cardsuit_col * CARDSUIT_WIDTH, cardsuit_row * CARDSUIT_HEIGHT, last_card);
        cardsuit.copy(cardsuit_col * CARDSUIT_WIDTH, cardsuit_row * CARDSUIT_HEIGHT + CARD_HEIGHT + 2, last_suit);

        dprintf("MATCHED: %d = %s", c, card_name(c));
        if (c == CARD_MATCH_NONE || (c != cardsuit_row*13 + cardsuit_col)) {
          dprintf("ABORT");
          break;
        }
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
  if (cardsuit_row == 3 && cardsuit_col == 13) {
    dprintf("all sorted");
  }
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
    HTTP::add("/eject", handle_eject);
    HTTP::add("/frame.bmp", handle_frame_bmp);
    HTTP::add("/cardsuit.bmp", handle_cardsuit_bmp);
    HTTP::add("/card.bmp", handle_card_bmp);
    HTTP::add("/suit.bmp", handle_suit_bmp);
    HTTP::add("/train", handle_train);
    HTTP::add("/check", handle_check);
 }

  //cardsuit.init(CARDSUIT_NCOLS * CARDSUIT_WIDTH, CARDSUIT_NROWS * CARDSUIT_HEIGHT);
  //cardsuit.clear();
  if (bmp_load("/user/www/cards.bmp", card_master) != 0 || bmp_load("/user/www/suits.bmp", suit_master) != 0) {
    dprintf("note: failed to load card/suit masters");
  }

  dprintf("ready...");
}

void loop() {
  wifi_check();
  capture_check();
}
