// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
#include <stdarg.h>
#include "http.h"

static struct {
  const char *path;
  void (*handler)(HTTP &);
} handlers[MAX_HANDLERS];
static int nhandlers = 0;

void HTTP::add(const char *path, void (*handler)(HTTP &))
{
  if (nhandlers < MAX_HANDLERS) {
    handlers[nhandlers].path = path;
    handlers[nhandlers].handler = handler;
    nhandlers++;
  } else {
    dprintf("ERROR: too many HTTP handlers (max=%d)", MAX_HANDLERS);
  }
}

HTTP::HTTP(WiFiClient &client, String &method, String &path) : client(client), method(method), path(path)
{
}

void HTTP::begin(int code, const char *msg)
{
  state = 1;
  printf("HTTP/1.1 %d %s\n", code, msg);
}

void HTTP::printf(const char *fmt, ...)
{
  char buf[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  client.print(buf);
}

void HTTP::end()
{
  if (state == 1) {
    client.println("Connection: close");
    client.println();
  }
  state = method == "HEAD" ? 4 : 2;
}

int HTTP::write(unsigned char *buf, int len)
{
  switch (state) {
    case 0:
      begin(200, "File Follows");
    case 1:
      end();
    case 2:
      state = 3;
    case 3:
      break;
    case 4:
      return len;
  }
  for (int off = 0 ; off < len ; ) {
    int n = client.write(buf + off, min(10*1024, len - off));
    if (n < 0) {
      state = 4;
      return off;
    }
    off += n;
  }
  return len;
}

void file_handler(HTTP &http)
{
  char fname[64];
  snprintf(fname, sizeof(fname), "/user/www/%s", http.path.c_str());
  FILE *fp = fopen(fname, "r");
  if (fp == NULL) {
    return;
  }

  int buflen = 10*1024;
  auto buf = std::shared_ptr<unsigned char[]>(new unsigned char[buflen]);
  if (buf == NULL) {
    http.begin(404, "Out of Memory");
    return;
  }

  fseek(fp, 0, SEEK_END);
  size_t content_length = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  const char * content_type = "application/octet-stream";
  if (http.path.endsWith(".html")) {
    content_type = "text/html";
  } else if (http.path.endsWith(".txt")) {
    content_type = "text/plain";
  } else if (http.path.endsWith(".png")) {
    content_type = "image/png";
  }

  dprintf("%s: sending %d bytes of %s", http.path.c_str(), content_length, content_type);

  http.begin(200, "File Follows");
  http.printf("Content-Length: %d\n", content_length);
  http.printf("Content-Type: %s\n", content_type);
  http.end();

  for (int off = 0 ; off < content_length ;) {
    int n = fread(buf.get(), 1, min(buflen, content_length-off), fp);
    if (n <= 0) {
      break;
    }
    http.write(buf.get(), n);
    off += n;
  }
  fclose(fp);
}

void HTTP::dispatch(WiFiClient &client, String &method, String &path)
{
  HTTP http(client, method, path);

  for (int i = 0 ; i < nhandlers ; i++) {
    if (path.equals(handlers[i].path)) {
      handlers[i].handler(http);
    }
  }
  if (http.state == 0) {
    file_handler(http);
  }
  if (http.state == 0) {
    http.begin(404, "File Not Found");
  }
  if (http.state == 1) {
    http.end();
  }
}

void HTTP::handle(WiFiClient &client)
{
  String currentLine = "";
  String method = "";
  String path = "";
  while (client.connected()) {
    if (client.available()) {
      char c = client.read();
      //Serial.write(c);
      if (c == '\n') {
        if (currentLine.length() == 0) {
          HTTP::dispatch(client, method, path);
          return;
        }
        // header line
        if (method == "") {
          int i = currentLine.indexOf(' ');
          method = currentLine.substring(0, i);
          path = currentLine.substring(i+1, currentLine.indexOf(' ', i+1));
        }
        currentLine = "";
      } else if (c != '\r') {
        currentLine += c;
      }
    }
  }
}

void http_init()
{
}