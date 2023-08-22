/* ------------------------------------------------------- */

#include <stdlib.h>
#include <stdio.h>

#if defined(__linux)
#  include <time.h>
#  include <sys/time.h>
#endif

#include <tra/time.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

#if defined(_WIN32)

#  if !defined(WIN32_LEAN_AND_MEAN)
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>

#  define CONSOLE_COLOR_BLACK        0
#  define CONSOLE_COLOR_DARK_BLUE    FOREGROUND_BLUE
#  define CONSOLE_COLOR_DARK_GREEN   FOREGROUND_GREEN
#  define CONSOLE_COLOR_DARK_CYAN    FOREGROUND_GREEN | FOREGROUND_BLUE
#  define CONSOLE_COLOR_DARK_RED     FOREGROUND_RED
#  define CONSOLE_COLOR_DARK_MAGENTA FOREGROUND_RED | FOREGROUND_BLUE
#  define CONSOLE_COLOR_DARK_YELLOW  FOREGROUND_RED | FOREGROUND_GREEN
#  define CONSOLE_COLOR_DARK_GRAY    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE
#  define CONSOLE_COLOR_GRAY         FOREGROUND_INTENSITY
#  define CONSOLE_COLOR_BLUE         FOREGROUND_INTENSITY | FOREGROUND_BLUE
#  define CONSOLE_COLOR_GREEN        FOREGROUND_INTENSITY | FOREGROUND_GREEN
#  define CONSOLE_COLOR_CYAN         FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE
#  define CONSOLE_COLOR_RED          FOREGROUND_INTENSITY | FOREGROUND_RED
#  define CONSOLE_COLOR_MAGENTA      FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_BLUE
#  define CONSOLE_COLOR_YELLOW       FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN
#  define CONSOLE_COLOR_WHITE        FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE

#endif /* _WIN32 */

/* ------------------------------------------------------- */

static tra_log_settings g_log_settings = { 0 };

/* ------------------------------------------------------- */

static int log_message(tra_log_settings* cfg, int level, uint16_t line, const char* filename, const char* fmt, va_list args); /* When a callback has been set via `tra_log_start()` this will call that function; otherwise this will call our default stdout function. */
static int log_message_stdout(int level, uint16_t line, const char* filename, const char* fmt, va_list args); /* The default log callback. */

/* ------------------------------------------------------- */

int tra_log_start(tra_log_settings* cfg) {

  if (NULL == cfg) {
    printf("Cannot start the log, given `tra_log_settings` is NULL.\n");
    return -1;
  }

  if (NULL == cfg->callback) {
    printf("Cannot start the log, given `tra_log_settings::callback` is NULL.\n");
    return -2;
  }

  g_log_settings = *cfg;
  
  return 0;
}

int tra_log_stop() {
  return 0;
}

/* ------------------------------------------------------- */

static int log_message(
  tra_log_settings* cfg,
  int level,
  uint16_t line,
  const char* filename,
  const char* fmt,
  va_list args
)
{
  struct tra_log_message msg = { 0 };
  int r = 0;

  if (NULL == cfg) {
    printf("Cannot log the message as the given `tra_log_settings*` is NULL.");
    return -1;
  }

  if (NULL == fmt) {
    printf("Cannot log the message as the given `fmt` is NULL.");
    return -2;
  }

  /* When callback function hasn't been set use our own logging function (...) */
  if (NULL == cfg->callback) {
    return log_message_stdout(level, line, filename, fmt, args);
  }

  /* ... otherwise pass the message to the callback. */
  msg.line = line;
  msg.level = level;
  msg.filename = filename;
  
  r = cfg->callback(&msg, fmt, args);
  if (r < 0) {
    printf("Failed to log a message from %s:%u.", filename, line);
    return -4;
  }

  return 0;
}

/* ------------------------------------------------------- */

#if defined(_WIN32)

/*
  Windows stdout logging function; this function has a limitation
  that the log message cannot exceed 1024 characters.
 */
static int log_message_stdout(
  int level,
  uint16_t line,
  const char* filename,
  const char* fmt,
  va_list args
)
{
  char time_str[64] = { 0 };
  char msg_str[1024] = { 0 };
  size_t time_size = 0;
  HANDLE handle = NULL;
  
  uint32_t colors[] = {
    CONSOLE_COLOR_BLACK,   /* none:  reset   */
    CONSOLE_COLOR_MAGENTA, /* fatal: magenta */
    CONSOLE_COLOR_RED,     /* error: red     */
    CONSOLE_COLOR_YELLOW,  /* warn:  yellow  */
    CONSOLE_COLOR_GREEN,   /* info:  green   */
    CONSOLE_COLOR_BLUE,    /* debug: blue    */
    CONSOLE_COLOR_GRAY,    /* trace: gray    */
  };

  const char* names[] = {
    "none",
    "fatal",
    "error",
    "warn",
    "info",
    "debug",
    "trace",
  };

  uint8_t color = 0;
  int r = 0;

  handle = GetStdHandle(STD_OUTPUT_HANDLE);
  if (NULL == handle) {
    printf("Cannot handle the log entry, we failed to retrieve the std output handle.\n");
    r = -10;
    goto error;
  }

  r = tra_strftime(
    time_str,
    sizeof(time_str),
    "%Y-%m-%d %H:%M:%S",
    &time_size
  );

  if (r < 0) {
    printf("Cannot log a message as we failed to get the time string.\n");
    r = -20;
    goto error;
  }
  
  r = vsnprintf(msg_str, sizeof(msg_str), fmt, args);
  if (r < 0) {
    printf("Cannot log a message as the call to `vsnprintf()` failed.\n");
    r = -30;
    goto error;
  }

  SetConsoleTextAttribute(handle, colors[level]);

  printf(
    "%s.%03llu %s:%u %s: %s\n",
    time_str,
    tra_get_clock_millis(),
    filename,
    line,
    names[level],
    msg_str
  );
  
 error:

  if (NULL != handle) {
    /* Reset */
    SetConsoleTextAttribute(handle, CONSOLE_COLOR_WHITE);
  }
  
  return r;
}

#endif /* _WIN32 */

/* ------------------------------------------------------- */

#if defined(__linux) || defined(__APPLE__)

static int log_message_stdout(
  int level,
  uint16_t line,
  const char* filename,
  const char* fmt,
  va_list args
)
{
  const char* level_names[] = { "none", "fatal", "error", "warn", "info", "debug", "trace" };
  struct timeval time_millis = { 0 };
  struct tm* tm_now = { 0 };
  time_t time_now = { 0 };
  char time_str[64] = {};
  uint32_t millis = 0;
  int r = 0;

  const char* colors[] = {
    "\e[0m",    /* none:    reset      */
    "\e[95m",   /* fatal:   magenta    */
    "\e[91m",   /* error:   red        */
    "\e[93m",   /* warn:    yellow     */
    "\e[92m",   /* info:    green      */
    "\e[94m",   /* debug:   blue       */
    "\e[90m",   /* trace:   gray       */
  };

  /* Get the current time. */
  time_now = time(NULL);
  tm_now = localtime(&time_now);
  strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_now);

  /* We also like to get the millis. */
  r = gettimeofday(&time_millis, NULL);
  if (0 != r) {
    r = 0;
    millis = 0;
  }
  else {
    millis = time_millis.tv_usec / 1000;
  }

  printf(
    "%s %s.%03u %s:%u %s: ",
    colors[level],
    time_str,
    millis,
    filename,
    line,
    level_names[level]
  );

  vprintf(fmt, args);

  printf("\e[0m\n");

  return 0;
}
#endif /* __linux || __APPLE__ */

/* ------------------------------------------------------- */

void tra_log_trace(int line, const char* filename, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log_message(&g_log_settings, TRA_LOG_LEVEL_TRACE, line, filename, fmt, args);
  va_end(args);
}

void tra_log_debug(int line, const char* filename, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log_message(&g_log_settings, TRA_LOG_LEVEL_DEBUG, line, filename, fmt, args);
  va_end(args);
}

void tra_log_info(int line, const char* filename, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log_message(&g_log_settings, TRA_LOG_LEVEL_INFO, line, filename, fmt, args);
  va_end(args);
}

void tra_log_warn(int line, const char* filename, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log_message(&g_log_settings, TRA_LOG_LEVEL_WARN, line, filename, fmt, args);
  va_end(args);
}

void tra_log_error(int line, const char* filename, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log_message(&g_log_settings, TRA_LOG_LEVEL_ERROR, line, filename, fmt, args);
  va_end(args);
}

void tra_log_fatal(int line, const char* filename, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log_message(&g_log_settings, TRA_LOG_LEVEL_FATAL, line, filename, fmt, args);
  va_end(args);
}

/* ------------------------------------------------------- */
