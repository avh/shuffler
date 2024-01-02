// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
#include <WiFi.h>

static WiFiServer server(80);

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

void wifi_init(const char *ssid, const char *pass)
{
  // check WiFi firmware
  int status = WiFi.status();
  if (status == WL_NO_SHIELD) {
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
}

void wifi_check() 
{
  WiFiClient client = server.available();
  if (client) {
    HTTP::handle(client);
    client.stop();
  }
}