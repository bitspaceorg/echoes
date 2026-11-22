#ifndef IO_H
#define IO_H

#include "driver.h"

int init_audio_driver(const char *);
int write_audio(sample);
void cleanup_audio_driver(void);
DriverIMPL *get_driver_impl();

#endif /* IO_H */
