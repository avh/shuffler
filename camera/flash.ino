// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
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

void handle_listdir(HTTP &http, const char *path) 
{
  char buf[64];
  DIR *d = opendir(path);
  if (d != NULL) {
    for (struct dirent *p = readdir(d) ; p != NULL ; p = readdir(d)) {
      if (p->d_name[0] == '.') {
        continue;
      }
      if (p->d_type == DT_DIR) {
        http.printf("%s/%s/\n", path, p->d_name);

        snprintf(buf, sizeof(buf), "%s/%s", path, p->d_name);
        handle_listdir(http, buf);
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
        http.printf(buf);
        for (int i = 32 - strlen(buf) ; i-- > 0 ;) {
          http.printf(" ");
        }
        http.printf( "%10d\n", bytes);
      }
    }
    closedir(d);
  } else {
    http.printf("failed to open directory\n");
  }
}

void handle_list(HTTP &http) 
{
  http.begin(200, "List Follows");
  http.end();
  http.printf("Listing\n");
  
  http.printf("--\n");
  handle_listdir(http, "/user");
  http.printf("--\n");
}

void flash_init() 
{
  user_fs.mount(&user_data);
  HTTP::add("/list", handle_list);
}

void flash_check() 
{
}
