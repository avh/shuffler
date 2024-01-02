#include "WiFi.h"

#define MAX_HANDLERS   20

int status = WL_IDLE_STATUS;
WiFiServer server(80);

struct {
  const char *prefix;
  void (*handler)(WiFiClient&, String&);
} handlers[MAX_HANDLERS];
int nhandlers = 0;

void wifi_scan()
{
   int n = WiFi.scanNetworks();
   dprintf("found %d networks", n);
   for (int i = 0 ; i < n ; i++) {
    const char *ssid = WiFi.SSID(i);
    if (strlen(ssid) > 0) {
      dprintf("%3d: %s, %d dBm, %d", i, WiFi.SSID(i), WiFi.RSSI(i), WiFi.encryptionType(i));
     }
   }
}

void wifi_404(WiFiClient& client)
{
  client.println("HTTP/1.1 404 Not Found");
  client.println("Connection: close");
  client.println();
}

void wifi_response(WiFiClient &client, const char *fmt, ...)
{
  char buf[64];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);

  client.println("HTTP/1.1 200 File Follows");
  client.println("Connection: close");
  client.println();
  client.println(buf);
  client.println();
}

void www_handler(WiFiClient &client, String &path)
{
  int buflen = 10*1024;
  char *buf = (char *)malloc(buflen);
  if (buf == NULL) {
    wifi_404(client);
    return;
  }

  snprintf(buf, buflen, "/user/%s", path.c_str());
  FILE *fp = fopen(buf, "r");
  if (fp == NULL) {
    free(buf);
    wifi_404(client);
    return;
  }
  fseek(fp, 0, SEEK_END);
  size_t content_length = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  const char * content_type = "application/octet-stream";
  if (path.endsWith(".html")) {
    content_type = "text/html";
  } else if (path.endsWith(".txt")) {
    content_type = "text/plain";
  } else if (path.endsWith(".png")) {
    content_type = "image/png";
  }

  dprintf("%s: sending %d bytes of %s", path.c_str(), content_length, content_type);

  client.println("HTTP/1.1 200 File Follows");
  snprintf(buf, buflen, "Content-Length: %d", content_length);
  client.println(buf);
  snprintf(buf, buflen, "Content-Type: %s", content_type);
  client.println(buf);
  client.println("Connection: close");
  client.println();

  for (int off = 0 ; off < content_length ;) {
    int n = fread(buf, 1, min(buflen, content_length-off), fp);
    if (n <= 0) {
      break;
    }
    client.write(buf, n);
    off += n;
  }
  fclose(fp);
  free(buf);
}

void wifi_handler(const char *prefix, void (*handler)(WiFiClient&, String &)) 
{
  if (nhandlers < MAX_HANDLERS) {
    handlers[nhandlers].prefix = prefix;
    handlers[nhandlers].handler = handler;
    nhandlers++;
  } else {
    dprintf("ERROR: too many wifi handlers (max=%d)", MAX_HANDLERS);
  }
}

void wifi_check() 
{
  WiFiClient client = server.available();
  if (client) {
    String currentLine = "";
    String path = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        //Serial.write(c);
        if (c == '\n') {
          if (currentLine.length() == 0) {
            //dprintf("PATH='%s'", path.c_str());
            for (int i = 0 ; i < nhandlers ; i++) {
              //dprintf("compare '%s' and '%s' is %d", path.c_str(), handlers[i].prefix, path.equals(handlers[i].prefix));
              if (path.startsWith(handlers[i].prefix)) {
                handlers[i].handler(client, path);
                client.stop();
                return;
              }
            }
            wifi_404(client);
            client.stop();
            break;
          } else {
            // header line
            if (currentLine.startsWith("GET ")) {
              path = currentLine.substring(4, currentLine.indexOf(' ', 4));
              //dprintf("GOT %s -> %s", currentLine.c_str(), path.c_str());
            }
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
   client.stop();
 }
}

void wifi_init(const char *ssid, const char *pass)
{
  // check WiFi firmware
  if (WiFi.status() == WL_NO_SHIELD) {
    dprintf("WiFi shield not present");
    while (true);
  }
  const char *v = "v1.94.0";
  String fv = WiFi.firmwareVersion();
  if (fv != v) {
    dprintf("Please upgrade the firmware %s != %s", fv.c_str(), v);
  }

  // connect to Wifi network
  for (int i = 0 ; i < 2 && status != WL_CONNECTED ; i++) {
    status = WiFi.begin(ssid, pass);
    if (status != WL_CONNECTED) {
      dprintf("%2d: connect to %s failed, status=%d", i, ssid, status);
      delay(500);
      wifi_scan();
    }
  }
  if (status != WL_CONNECTED) {
    dprintf("error: failed to connect to %s", ssid);
    while (true) delay(1000);
  }
  server.begin();

  IPAddress ip = WiFi.localIP();
  dprintf("SSID: %s, IP: %d.%d.%d.%d, %d dBm", WiFi.SSID(), ip[0], ip[1], ip[2], ip[3], WiFi.RSSI());
  //wifi_scan();
  wifi_handler("/www", www_handler);
}