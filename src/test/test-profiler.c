/* ------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <tra/profiler.h>
#include <tra/log.h>
#include <rxtx/trace.h>
#include <rxtx/log.h>

/* ------------------------------------------------------- */

int main(int argc, const char* argv[]) {

  const char* frame = "frame";
  tra_profiler_settings prof_cfg = { 0 };
  int r = 0;
  int i = 0;

  /* Initialize the log (which is used by the tracer) and initalize the tracer itself. */
  rx_log_start("profiler", 1024, 1024 * 1024, argc, argv);
  rx_trace_start("profiler.json", 1024 * 1024);

  TRAI("Profiler Test");

  TRAP_SET_THREAD_NAME("encoder-thread");

  /*
    Use the `rx_trace_*` functions when profiling has been
    enabled and the `TRA_PROFILER_USE_CUSTOM_MACROS` compiler cli
    define hasn't been set. When it has been set it will use the
    macros that are defined in a custom file, see `profiler.h`.
  */
  prof_cfg.timer_begin = rx_trace_timer_begin;
  prof_cfg.timer_end = rx_trace_timer_end;

  tra_profiler_start(&prof_cfg);

  while(i < 200) {

    /* Mark begin */
    TRAP_FRAME_BEGIN(pf, "loop");
    TRAP_TIMER_BEGIN(app, "frame");

    /* Fake resize */
    TRAP_TIMER_BEGIN(rz, "resize");
    usleep(10e3);
    TRAP_TIMER_END(rz, "resize");

    /* Fake encode. */
    TRAP_TIMER_BEGIN(enc, "encode");
    usleep(100e3);
    TRAP_TIMER_END(enc, "encode");

    /* Mark end */
    usleep(20e3);
    TRAP_TIMER_END(app, "frame");
    TRAP_FRAME_END(pf, "loop");

    ++i;
  }

  tra_profiler_stop();
  rx_log_stop();
  rx_trace_stop();
  
 error:
  
  if (r < 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

/* ------------------------------------------------------- */
