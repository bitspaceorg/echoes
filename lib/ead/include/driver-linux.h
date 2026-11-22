#ifndef DRIVER_LINUX_H
#define DRIVER_LINUX_H

#include "driver.h"
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <stdio.h>

typedef struct {
  DriverIMPL base;
  snd_pcm_t *pcm;
  audio_mixer mixer;
  int audio_initialized;
  pthread_t audio_thread;
  int thread_running;
  char *driver_name;
} DriverLinuxIMPL;

DriverIMPL *init_driver_linux_impl(const char *);

int write_sound_linux_impl(DriverIMPL *, sample);
char *get_driver_name_linux_impl(DriverIMPL *);
void sound_check_linux_impl();
void dispose_linux_impl(DriverIMPL *);

#endif /* DRIVER_LINUX_H */
