#include <IO.h>

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __APPLE__
#include "driver-mac.h"
#elif defined(__unix__)
#include "driver-linux.h"
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#include "driver-windows.h"
#endif

static DriverIMPL *driver_impl = NULL;

int init_audio_driver(const char *driver_name) {
  if (driver_impl != NULL) {
    return 0;
  }

#ifdef __APPLE__
  driver_impl = init_driver_mac_impl(driver_name);
#elif defined(__unix__)
  driver_impl = init_driver_linux_impl(driver_name);
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
  driver_impl = init_driver_window_impl(driver_name);
#endif

  if (driver_impl == NULL) {
    return 1;
  }
  return 0;
}

int write_audio(sample sample_data) {
  return driver_impl->write(driver_impl, sample_data);
}

void cleanup_audio_driver(void) {
  if (driver_impl != NULL) {
    driver_impl->dispose(driver_impl);
    free(driver_impl);
    driver_impl = NULL;
  }
}

DriverIMPL *get_driver_impl() { return driver_impl; }
