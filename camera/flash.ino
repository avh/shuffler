#include <BlockDevice.h>
#include <MBRBlockDevice.h>
#include <FATFileSystem.h>
#include <PluggableUSBMSD.h>
#include <QSPIFBlockDevice.h>
#include "defs.h"

//USBMSD __attribute__ ((init_priority (1))) MassStorage(mbed::BlockDevice::get_default_instance());

static mbed::BlockDevice *root = mbed::BlockDevice::get_default_instance();
static mbed::MBRBlockDevice user_data(root, 2);
static mbed::FATFileSystem user_fs("user");

void handle_listdir(WiFiClient &client, const char *path) 
{
  char buf[64];
  DIR *d = opendir(path);
  if (d != NULL) {
    for (struct dirent *p = readdir(d) ; p != NULL ; p = readdir(d)) {
      if (p->d_name[0] == '.') {
        continue;
      }
      if (p->d_type == DT_DIR) {
        snprintf(buf, sizeof(buf), "%s/%s/", path, p->d_name);
        client.println(buf);
        snprintf(buf, sizeof(buf), "%s/%s", path, p->d_name);
        handle_listdir(client, buf);
      } else {
        snprintf(buf, sizeof(buf), "%s/%s", path, p->d_name);
        FILE *fp = fopen(buf, "r");
        size_t bytes = 0;
        if (fp != NULL) {
          fseek(fp, 0, SEEK_END);
          bytes = ftell(fp);
          fclose(fp);
        }
        snprintf(buf, sizeof(buf), "%s/%s", path, p->d_name);
        client.print(buf);
        for (int i = 32 - strlen(buf) ; i-- > 0 ;) {
          client.print(" ");
        }
        snprintf(buf, sizeof(buf), "%10d", bytes);
        client.println(buf);
      }
    }
    closedir(d);
  } else {
    client.println("failed to open directory");
  }
}

void handle_list(WiFiClient &client, String &path) 
{
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/plain");
  client.println("Connection: close");
  client.println();
  client.print("Listing");
  client.println();

  // listing
  //client.println("--");
  //handle_listdir(client, "/wifi");
  client.println("--");
  handle_listdir(client, "/user");
  client.println("--");
}

void flash_init() 
{
  user_fs.mount(&user_data);
  wifi_handler("/list", handle_list);
}

void flash_check() 
{
}
