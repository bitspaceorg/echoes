#include "driver-linux.h"
#include "driver.h"
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define CHANNELS 1
#define BUFFER_FRAMES 1024
#define IDLE_SLEEP_US 1000

static DriverLinuxIMPL *current_linux_driver = NULL;

/* Mirrors the mac AudioUnit render callback: mixes all active samples
   and pushes one buffer of frames to the sound card. Runs in a dedicated
   thread because ALSA has no callback-style audio API. */
void *_render_tone(void *inRefCon) {
  DriverLinuxIMPL *driver = (DriverLinuxIMPL *)inRefCon;
  float buffer[BUFFER_FRAMES];
  short out_buffer[BUFFER_FRAMES * CHANNELS];

  while (driver->thread_running) {
    memset(buffer, 0, sizeof(buffer));

    int any_active = 0;

    for (int i = 0; i < driver->mixer.count; i++) {
      sample *instance = &driver->mixer.instances[i];

      if (instance->completed)
        continue;

      any_active = 1;

      for (int j = 0; j < BUFFER_FRAMES; j++) {
        if (instance->current_position < instance->sample_count) {
          buffer[j] += instance->sample[instance->current_position++] * 0.7f;
          if (buffer[j] > 1.0f)
            buffer[j] = 1.0f;
          if (buffer[j] < -1.0f)
            buffer[j] = -1.0f;
        } else {
          instance->completed = 1;
          break;
        }
      }
    }

    if (!any_active) {
      usleep(IDLE_SLEEP_US);
    }

    for (int j = 0; j < BUFFER_FRAMES * CHANNELS; j++) {
      int value = (int)(buffer[j / CHANNELS] * 32767.0f);
      if (value > 32767)
        value = 32767;
      if (value < -32768)
        value = -32768;
      out_buffer[j] = (short)value;
    }

    snd_pcm_sframes_t written =
        snd_pcm_writei(driver->pcm, out_buffer, BUFFER_FRAMES);
    if (written < 0) {
      written = snd_pcm_recover(driver->pcm, (int)written, 0);
    }
    if (written < 0) {
      printf("Err: Failed to write to PCM device: %s\n",
             snd_strerror((int)written));
      break;
    }
  }

  return NULL;
}

void _init_audio_mixer(DriverLinuxIMPL *driver, int max_instances) {
  driver->mixer.max_instances = max_instances;
  driver->mixer.count = 0;
  driver->mixer.instances = (sample *)calloc(max_instances, sizeof(sample));
}

int add_sound_to_mixer(audio_mixer *mixer, sample sample_data) {
  int slot = -1;

  for (int i = 0; i < mixer->count; i++) {
    if (mixer->instances[i].completed) {
      slot = i;
      break;
    }
  }

  if (slot == -1) {
    if (mixer->count < mixer->max_instances) {
      slot = mixer->count++;
    } else {
      slot = 0;
      printf("Warning: Maximum concurrent sounds reached, replacing oldest "
             "sound\n");
    }
  }

  sample *instance = &mixer->instances[slot];

  /* The mixer owns these buffers; the sample API exposes them as const. */
  free((void *)instance->sample);

  float *buffer = (float *)malloc(sample_data.sample_count * sizeof(float));
  if (!buffer) {
    printf("Error: Failed to allocate memory for sample buffer\n");
    return -1;
  }

  memcpy(buffer, sample_data.sample,
         sample_data.sample_count * sizeof(float));
  instance->sample = buffer;
  instance->sample_count = sample_data.sample_count;
  instance->current_position = 0;
  instance->completed = 0;

  return slot;
}

int write_sound_linux_impl(DriverIMPL *base, sample sample_data) {
  DriverLinuxIMPL *parent_linux_impl = GET_PARENT_IMPL(DriverLinuxIMPL, base);

  if (!parent_linux_impl->audio_initialized) {
    int err = snd_pcm_prepare(parent_linux_impl->pcm);
    if (err < 0) {
      printf("Err: Failed to prepare PCM device: %s\n", snd_strerror(err));
      return 1;
    }

    _init_audio_mixer(parent_linux_impl, 16);

    parent_linux_impl->thread_running = 1;
    err = pthread_create(&parent_linux_impl->audio_thread, NULL,
                         _render_tone, parent_linux_impl);
    if (err != 0) {
      printf("Err: Failed to start render thread: %s\n", strerror(err));
      parent_linux_impl->thread_running = 0;
      return 1;
    }

    parent_linux_impl->audio_initialized = 1;
  }

  int sound_slot = add_sound_to_mixer(&parent_linux_impl->mixer, sample_data);
  if (sound_slot < 0) {
    return 1;
  }

  printf("Playing sound in slot %d\n", sound_slot);
  return 0;
}

void dispose_linux_impl(DriverIMPL *base) {
  DriverLinuxIMPL *parent_linux_impl = GET_PARENT_IMPL(DriverLinuxIMPL, base);

  if (parent_linux_impl->audio_initialized) {
    parent_linux_impl->thread_running = 0;
    pthread_join(parent_linux_impl->audio_thread, NULL);
    snd_pcm_drain(parent_linux_impl->pcm);
  }

  snd_pcm_close(parent_linux_impl->pcm);

  for (int i = 0; i < parent_linux_impl->mixer.count; i++) {
    free((void *)parent_linux_impl->mixer.instances[i].sample);
  }

  if (parent_linux_impl->mixer.instances) {
    free(parent_linux_impl->mixer.instances);
  }
}

char *get_driver_name_linux_impl(DriverIMPL *base) {
  return GET_PARENT_IMPL(DriverLinuxIMPL, base)->driver_name;
}

/* Linux has no system-sound API like AudioServicesPlaySystemSound, so the
   check tone is a short 440Hz beep through our own pipeline. */
void sound_check_linux_impl() {
  if (current_linux_driver == NULL) {
    printf("Err: No audio driver initialized\n");
    return;
  }

  size_t frame_count = SAMPLE_RATE / 10;
  float *tone = (float *)malloc(frame_count * sizeof(float));
  if (tone == NULL) {
    printf("Err: Memory allocation failed\n");
    return;
  }

  for (size_t i = 0; i < frame_count; i++) {
    tone[i] = 0.5f * sinf(2.0f * (float)M_PI * 440.0f * (float)i /
                          (float)SAMPLE_RATE);
  }

  sample tone_sample = {
      .sample = tone,
      .sample_count = frame_count,
      .current_position = 0,
      .completed = false,
  };

  write_sound_linux_impl(&current_linux_driver->base, tone_sample);
  free(tone);
}

DriverIMPL *init_driver_linux_impl(const char *driver_name) {
  DriverLinuxIMPL *new_linux_driver =
      (DriverLinuxIMPL *)malloc(sizeof(DriverLinuxIMPL));
  if (new_linux_driver == NULL) {
    printf("Err: Memory allocation failed\n");
    return NULL;
  }

  new_linux_driver->driver_name = (char *)driver_name;
  new_linux_driver->base.get_driver_name = get_driver_name_linux_impl;
  new_linux_driver->base.sound_check = sound_check_linux_impl;
  new_linux_driver->base.dispose = dispose_linux_impl;
  new_linux_driver->audio_initialized = 0;

  /* Open the default playback device */
  int err = snd_pcm_open(&new_linux_driver->pcm, "default",
                         SND_PCM_STREAM_PLAYBACK, 0);
  if (err < 0) {
    printf("Err: Failed to open default PCM device: %s\n", snd_strerror(err));
    free(new_linux_driver);
    return NULL;
  }

  err = snd_pcm_set_params(new_linux_driver->pcm, SND_PCM_FORMAT_S16_LE,
                           SND_PCM_ACCESS_RW_INTERLEAVED, CHANNELS,
                           SAMPLE_RATE, 1, 100000);
  if (err < 0) {
    printf("Err: Failed to set PCM device params: %s\n", snd_strerror(err));
    snd_pcm_close(new_linux_driver->pcm);
    free(new_linux_driver);
    return NULL;
  }

  new_linux_driver->base.write = write_sound_linux_impl;
  current_linux_driver = new_linux_driver;
  return (DriverIMPL *)new_linux_driver;
}
