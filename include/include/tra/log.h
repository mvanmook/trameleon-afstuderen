#ifndef TRA_LOG_H
#define TRA_LOG_H

/*

  ┌─────────────────────────────────────────────────────────────────────────────────────┐
  │                                                                                     │
  │   ████████╗██████╗  █████╗ ███╗   ███╗███████╗██╗     ███████╗ ██████╗ ███╗   ██╗   │
  │   ╚══██╔══╝██╔══██╗██╔══██╗████╗ ████║██╔════╝██║     ██╔════╝██╔═══██╗████╗  ██║   │
  │      ██║   ██████╔╝███████║██╔████╔██║█████╗  ██║     █████╗  ██║   ██║██╔██╗ ██║   │
  │      ██║   ██╔══██╗██╔══██║██║╚██╔╝██║██╔══╝  ██║     ██╔══╝  ██║   ██║██║╚██╗██║   │
  │      ██║   ██║  ██║██║  ██║██║ ╚═╝ ██║███████╗███████╗███████╗╚██████╔╝██║ ╚████║   │
  │      ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝     ╚═╝╚══════╝╚══════╝╚══════╝ ╚═════╝ ╚═╝  ╚═══╝   │
  │                                                                 www.trameleon.org   │
  └─────────────────────────────────────────────────────────────────────────────────────┘

  LOGGING
  =======

  GENERAL INFO:

    We declare several log macros that you should use in your
    modules. We try to be verbose and help an user of this
    library as much as possible with good feedback. A rule of
    thumb is to log something if it helps the user to implement
    this library; it's better to log more info than not enough.

  USAGE:

    You can use the following `printf()` style macros:

      ```
      TRA_TRACE("This is a trace message.");
      TRA_DEBUG("This is a debug message.");
      TRA_INFO("This is an info message.");
      TRA_WARN("This is a warn message.");
      TRA_ERROR("This is an error message.");
      TRA_FATAL("This is an fatal message.");
      ```

    We also provide shorter macros:

      ```
      TRAT("This is a trace message.");
      TRAD("This is a debug message.");
      TRAI("This is an info message.");
      TRAW("This is a warn message.");
      TRAE("This is an error message.");
      TRAF("This is an fatal message.");
      ```
 
  CUSTOM CALLBACK:
  
     You can call `tra_log_start()` and pass it an instance
     of `tra_log_settings` which holds a pointer to your
     log callback. For example:

       ```
       // The callback that we will call
       static int log_callback(tra_log_message* msg, const char* fmt, ...);

       // Create the settings and set the custom callback.
       tra_log_settings log_cfg = { 0 };
       log_cfg.callback = log_callback;
       tra_log_start(&log_cfg);

       // ... your code ...

       // Cleanup
       tra_log_stop();

       ```

  LEVELS:

    We use the log levels: FATAL, ERROR, WARN, INFO, DEBUG and
    TRACE. These levels are based on the levels from
    syslog. Syslog defines 8 different level which seems a bit
    too much for this library.
    
    As a general rule you should use use the level based on the
    severity of the message (ERROR, WARN), during development and
    debugging (INFO, DEBUG) or how often the message occurs
    (DEBUG, TRACE). In general we use INFO to indicate function
    call flow.

    Don't hesitate to be verbose with your logging. When logging
    some info might help the user of this library to more easily
    spot an issue let them know. Our goal is to help developers
    who use this library.
    
      |-------+------------------------------------------------------------|
      | FATAL | Something seriously went wrong and we can't continue       |
      |       | normal operation anymore. We need to exit the application. |
      |       | You use this when e.g. you're validating user input        |
      |       | and some requirements are not met, when you can't          |
      |       | open an decoder because the device failed, the user        |
      |       | disconnected some hardware that is required, etc.          |
      |-------+------------------------------------------------------------|
      | ERROR | This is what you use when validating user input to         |
      |       | functions which might be really important but not that     |
      |       | important that you have to exit the application.           |
      |-------+------------------------------------------------------------|
      | WARN  | Something unexpected happened; e.g. you expected to        |
      |       | receive a keyframe but didn't. Your application can still  |
      |       | continue to work but you should have a look why            |
      |       | this happened.                                             |
      |-------+------------------------------------------------------------|
      | INFO  | An event happened that you want to log just to see the     |
      |       | general flow of your application. For example, you         |
      |       | connected to a remote server, you got an ip address,       |
      |       | you figured out what kind or router you're using, etc.     |
      |       |                                                            |
      |       | Use this when you don't print state values but only flow.  |
      |       |                                                            |
      |-------+------------------------------------------------------------|
      | DEBUG | Use this during debugging; e.g. while figuring out some    |
      |       | issue and you want to print some information. After        |
      |       | you've fixed the issue, you should remove the debug.       |
      |       | messages.                                                  |
      |       |                                                            |
      |       | Use this when you want to print state values sometimes.    |
      |       |                                                            |
      |-------+------------------------------------------------------------|
      | TRACE | You use trace messages to extract as much information      |
      |       | out of the application as possible. You use this when      |
      |       | trying to get an understanding of what events and data     |
      |       | flows through your app.                                    |
      |       |                                                            |
      |       | Use this when you want to print state values regularly.    |
      |       |                                                            |
      |-------+------------------------------------------------------------|

*/

/* ------------------------------------------------------- */

#include <stdarg.h>
#include <stdint.h>
#include <tra/api.h>

/* ------------------------------------------------------- */

#if defined(__cplusplus)
  extern "C" {
#endif

/* ------------------------------------------------------- */

#define TRA_LOG_LEVEL_NONE   0
#define TRA_LOG_LEVEL_FATAL  1
#define TRA_LOG_LEVEL_ERROR  2
#define TRA_LOG_LEVEL_WARN   3
#define TRA_LOG_LEVEL_INFO   4
#define TRA_LOG_LEVEL_DEBUG  5
#define TRA_LOG_LEVEL_TRACE  6

/* ------------------------------------------------------- */

#if defined(_WIN32)
#  if !defined(WIN32_LEAN_AND_MEAN)
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
#  include <string.h>
#  define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

/* ------------------------------------------------------- */

#if !defined(TRA_LOG_LEVEL)
#  define TRA_LOG_LEVEL TRA_LOG_LEVEL_TRACE
#endif

/* ------------------------------------------------------- */

#if TRA_LOG_LEVEL >= TRA_LOG_LEVEL_TRACE
#  define TRA_TRACE(fmt, ...)  { tra_log_trace(__LINE__, __FILENAME__, fmt, ##__VA_ARGS__); }
#  define TRAT(fmt, ...)       { tra_log_trace(__LINE__, __FILENAME__, fmt, ##__VA_ARGS__); }
#else
#  define TRA_TRACE(fmt, ...)  {}
#  define TRAT(fmt, ...)       {}
#endif

/* ------------------------------------------------------- */

#if TRA_LOG_LEVEL >= TRA_LOG_LEVEL_DEBUG
#  define TRA_DEBUG(fmt, ...)  { tra_log_debug(__LINE__, __FILENAME__, fmt, ##__VA_ARGS__); }
#  define TRAD(fmt, ...)       { tra_log_debug(__LINE__, __FILENAME__, fmt, ##__VA_ARGS__); }
#else
#  define TRA_DEBUG(fmt, ...)  {}
#  define TRAD(fmt, ...)       {}
#endif

/* ------------------------------------------------------- */

#if TRA_LOG_LEVEL >= TRA_LOG_LEVEL_INFO
#  define TRA_INFO(fmt, ...)  { tra_log_info(__LINE__, __FILENAME__, fmt, ##__VA_ARGS__); }
#  define TRAI(fmt, ...)      { tra_log_info(__LINE__, __FILENAME__, fmt, ##__VA_ARGS__); }
#else
#  define TRA_INFO(fmt, ...)  {}
#  define TRAI(fmt, ...)      {}
#endif

/* ------------------------------------------------------- */

#if TRA_LOG_LEVEL >= TRA_LOG_LEVEL_WARN
#  define TRA_WARN(fmt, ...)  { tra_log_warn(__LINE__, __FILENAME__, fmt, ##__VA_ARGS__); }
#  define TRAW(fmt, ...)      { tra_log_warn(__LINE__, __FILENAME__, fmt, ##__VA_ARGS__); }
#else
#  define TRA_WARN(fmt, ...)  {}
#  define TRAW(fmt, ...)      {}
#endif

/* ------------------------------------------------------- */

#if TRA_LOG_LEVEL >= TRA_LOG_LEVEL_ERROR
#  define TRA_ERROR(fmt, ...) { tra_log_error(__LINE__, __FILENAME__, fmt, ##__VA_ARGS__); }
#  define TRAE(fmt, ...)      { tra_log_error(__LINE__, __FILENAME__, fmt, ##__VA_ARGS__); }
#else
#  define TRA_ERROR(fmt, ...) {}
#  define TRAE(fmt, ...)      {}
#endif

/* ------------------------------------------------------- */

#if TRA_LOG_LEVEL >= TRA_LOG_LEVEL_FATAL
#  define TRA_FATAL(fmt, ...) { tra_log_fatal(__LINE__, __FILENAME__, fmt, ##__VA_ARGS__); }
#  define TRAF(fmt, ...)      { tra_log_fatal(__LINE__, __FILENAME__, fmt, ##__VA_ARGS__); }
#else
#  define TRA_FATAL(fmt, ...) {}
#  define TRAF(fmt, ...)      {}
#endif

/* ------------------------------------------------------- */

typedef struct tra_log_settings tra_log_settings;
typedef struct tra_log_message tra_log_message;
typedef int (*tra_log_callback)(tra_log_message* msg, const char* fmt, ...); 

/* ------------------------------------------------------- */

struct tra_log_message {
  uint16_t line;
  uint16_t level;
  const char* filename;
};

/* ------------------------------------------------------- */

struct tra_log_settings {
  tra_log_callback callback;                                             /* The callback function that we call with the logging info. */
};

/* ------------------------------------------------------- */

TRA_LIB_DLL int tra_log_start(tra_log_settings* cfg);                    /* Call this to setup a custom logging function. Do this before calling any other functions of the library. */
TRA_LIB_DLL int tra_log_stop();                                          /* Call this before exiting your application. */

/* ------------------------------------------------------- */

TRA_LIB_DLL void tra_log_trace(int line, const char* filename, const char* fmt, ...);
TRA_LIB_DLL void tra_log_debug(int line, const char* filename, const char* fmt, ...);
TRA_LIB_DLL void tra_log_info(int line, const char* filename, const char* fmt, ...);
TRA_LIB_DLL void tra_log_warn(int line, const char* filename, const char* fmt, ...);
TRA_LIB_DLL void tra_log_error(int line, const char* filename, const char* fmt, ...);
TRA_LIB_DLL void tra_log_fatal(int line, const char* filename, const char* fmt, ...);

/* ------------------------------------------------------- */

#if defined(__cplusplus)
  }
#endif

/* ------------------------------------------------------- */

#endif
