// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
#pragma once
#include <WiFi.h>

#define MAX_HANDLERS   20

class HTTP
{
  public:
    WiFiClient &client;
    String &method;
    String &path;
    int state = 0;

  public:
    HTTP(WiFiClient &client, String &method, String &path);

    void begin(int code, const char *str);
    void printf(const char *fmt, ...);
    void end();
    int write(unsigned char *buf, int len);

public:
    static void add(const char *path, void (*handler)(HTTP &));
    static void dispatch(WiFiClient &client, String &method, String &path);
    static void handle(WiFiClient &client);
};
