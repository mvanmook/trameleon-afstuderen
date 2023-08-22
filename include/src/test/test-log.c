/* ------------------------------------------------------- */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <rxtx/log.h>
#include <tra/time.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

static int log_callback(tra_log_message* msg, const char* fmt, ...);

/* ------------------------------------------------------- */

int main(int argc, const char* argv[]) {

#if !defined(_WIN32)  
  tra_log_settings log_cfg = { 0 };
  log_cfg.callback = log_callback;
  tra_log_start(&log_cfg);
  rx_log_start("log", 0, 0, argc, argv);
#endif

  tra_time_init();
  
  TRAT("This is a trace message.");
  tra_sleep_millis(100);
  TRAD("This is a debug message.");
  tra_sleep_millis(25);
  TRAI("This is an info message.");
  tra_sleep_millis(25);
  TRAW("This is a warning message.");
  tra_sleep_millis(25);
  TRAE("This is an error message.");
  tra_sleep_millis(100);
  TRAF("This is a fatal message.");

 error:
  
#if !defined(_WIN32)
  tra_log_stop();
  rx_log_stop();
#endif
  
  return EXIT_SUCCESS;
}

/* ------------------------------------------------------- */

#if !defined(_WIN32)
static int log_callback(tra_log_message* msg, const char* fmt, ...) { 

  va_list args;

  if (NULL == msg) {
    printf("Cannot log the given message as `msg` is NULL.");
    return -1;
  }

  va_start(args, fmt);
  
  switch (msg->level) {
    case TRA_LOG_LEVEL_TRACE: {
      rx_vlog_trace(RX_COL_GRAY, msg->line, msg->filename, fmt, args);
      break;
    }
    case TRA_LOG_LEVEL_DEBUG: {
      rx_vlog_debug(RX_COL_BLUE, msg->line, msg->filename, fmt, args);
      break;
    }
    case TRA_LOG_LEVEL_INFO: {
      rx_vlog_info(RX_COL_GREEN, msg->line, msg->filename, fmt, args);
      break;
    }
    case TRA_LOG_LEVEL_WARN: {
      rx_vlog_warn(RX_COL_YELLOW, msg->line, msg->filename, fmt, args);
      break;
    }
    case TRA_LOG_LEVEL_ERROR: {
      rx_vlog_error(RX_COL_RED, msg->line, msg->filename, fmt, args);
      break;
    }
    case TRA_LOG_LEVEL_FATAL: {
      rx_vlog_fatal(RX_COL_MAGENTA, msg->line, msg->filename, fmt, args);
      break;
    }
  }
  
  va_end(args);

  return 0;
}
#endif

/* ------------------------------------------------------- */
