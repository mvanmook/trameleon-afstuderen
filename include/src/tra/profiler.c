/* ------------------------------------------------------- */

#include <tra/profiler.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

static tra_profiler_settings g_profiler_settings = { 0 };

/* ------------------------------------------------------- */

int tra_profiler_start(tra_profiler_settings* cfg) {

  int r = 0;

  if (NULL == cfg) {
    TRAE("Cannot start the profiler as the given `tra_profiler_settings*` is NULL.");
    r = -10;
    goto error;
  }

  g_profiler_settings = *cfg;

 error:
  return r;
}

/* ------------------------------------------------------- */

int tra_profiler_stop() {

  g_profiler_settings.set_thread_name = NULL;
  g_profiler_settings.frame_begin = NULL;
  g_profiler_settings.frame_end = NULL;
  g_profiler_settings.timer_begin = NULL;
  g_profiler_settings.timer_end = NULL;
  g_profiler_settings.output_filename = NULL;

  return 0;
}

/* ------------------------------------------------------- */

void tra_profiler_set_thread_name(const char* threadName) {
  if (NULL != g_profiler_settings.set_thread_name) {
    g_profiler_settings.set_thread_name(threadName);
  }
}

/* ------------------------------------------------------- */

void tra_profiler_timer_begin(const char* timerTitle) {
  if (NULL != g_profiler_settings.timer_begin) {
    g_profiler_settings.timer_begin(timerTitle);
  }
}

/* ------------------------------------------------------- */

void tra_profiler_timer_end(const char* timerTitle) {
  if (NULL != g_profiler_settings.timer_end) {
    g_profiler_settings.timer_end(timerTitle);
  }
}

/* ------------------------------------------------------- */

void tra_profiler_frame_begin(const char* frameTitle) {
  if (NULL != g_profiler_settings.frame_begin) {
    g_profiler_settings.frame_begin(frameTitle);
  }
}

/* ------------------------------------------------------- */

void tra_profiler_frame_end(const char* frameTitle) {
  if (NULL != g_profiler_settings.frame_end) {
    g_profiler_settings.frame_end(frameTitle);
  }
}

/* ------------------------------------------------------- */

