#ifndef TRA_PROFILER_H
#define TRA_PROFILER_H
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

  PROFILING
  =========

  GENERAL INFO:

    One of the goals of the Trameleon library is to be able to
    get a good understanding of call flow and where potential
    bottlenecks occur. To get a high level understanding of this,
    we annotate the functions of the modules and core
    API. Profiling can be enabled and disabled during compile
    time by defining `TRA_PROFILER_ENABLED`. When disabled, none
    of the profiling macros will be set.

    
  DEVELOPMENT:

    This paragraph is important for developers of modules for
    Trameleon. Whenever you want to profile your code you have
    to annotate it with the following macros.

    TRAP_SET_THREAD_NAME(threadName):

      Sets the name of the thread. Most profiler GUIs will group
      the `TRAP_TIMER_{BEGIN,END}` blocks per thread.  This
      allows you to define the name of the thread.

    TRAP_FRAME_BEGIN(varName, frameTitle):

      See [this][0] image from the [Tracy][2] profiler which
      shows the visual result of a frame. This macro is used to
      define a block/area that groups high level work. In a
      graphics app or game this could be used to mark the
      beginning of the event loop, render loop, etc. The
      `varName` might be used by some profilers to create a
      variable that holds some values; therefore it must be
      unique per scope block.

    TRAP_FRAME_END(varName, frameTitle):

      The counter part of `TRAP_FRAME_END`, this marks the end of
      a block/area. Make sure that the values you use for `varName`
      and `frameTitle` are the same.

    TRAP_TIMER_BEGIN(varName, timerTitle):

      See [this][1] image from teh [Tracy][2] profiler which
      shows the visual result of a timed block. The most obvious
      use of the timer macros is to measure the time of a
      function call and stack them in a flame graph. Similar to
      the frame macros, you have to make sure that the `varName`
      is unique per scope.

    TRAP_TIMER_END(varName, timerTitle):

      This marks the end of a timed region. Make sure to use the
      same variable name and title that you used with
      `TRAP_TIMER_BEGIN()`. 

      
  USAGE:

    To enable profiling you have to either implement the
    profiling API or define custom macros in a file called
    `tra-profiler-macros.h`.

    GENERAL:

      To enable profiling make sure to add the
      `TRA_PROFILER_ENABLED` command line definition to your
      compiler. With CMake you can use the example below. Once
      enabled, you either implement the tracing API (1) or define
      the custom macros for your own profiler (2).

        add_definitions(-DTRA_PROFILER_ENABLED)
      
    1. IMPLEMENT THE API:

      When you want to create your own profiling functions, you
      can implement the callbacks that are used for
      profiling. When you don't define custom macros these
      functions will be called when profiling is enabled.  This
      works similar as our logging implementation where you can
      set a callback that will take care of logging.

      When you want to use this approach, you create an instance
      of `tra_profiler_settings` and set the required callbacks.
      Then you enable the profiling functions by calling
      `tra_profiler_start(&cfg)` and passing your settings
      object. When you're ready, you call `tra_profiler_stop()`.

    2. DEFINE CUSTOM MACROS:

      You can also specify your own profiling functions or
      macros. To use this, you have to create a header file
      called `tra-profiler-macros.h`. Make sure the path to the
      header is in your header search paths. In that file you can
      define the following functions:

      TRA_PROFILER_FUNC_SET_THREAD_NAME(threadName)
      TRA_PROFILER_FUNC_FRAME_BEGIN(varName, frameTitle)
      TRA_PROFILER_FUNC_FRAME_END(varName, frameTitle)
      TRA_PROFILER_FUNC_TIMER_BEGIN(varName, timerTitle)
      TRA_PROFILER_FUNC_TIMER_END(varName, timerTitle)

      Next you have to add a command line compiler definition to
      tell Trameleon you want to use these custom macros. With
      CMake you can use:

          add_definitions(-DTRA_PROFILER_USE_CUSTOM_MACROS)

      See [this][4] header which generates something like
      [this][3] which use the Tracy profiler.


  REFERENCES:

    [0]: https://imgur.com/a/XBVWQfZ "How `TRAP_FRAME_*` macros are visualised when supported by the profiler."
    [1]: https://imgur.com/a/nSZqDBa "How `TRAP_TIMER_*` macros are visualised when supported by the profiler."
    [2]: https://github.com/wolfpld/tracy "Tracy profiler"
    [3]: https://imgur.com/a/aOZ5bzy "Using Tracy as a profiler"
    [4]: https://gist.github.com/roxlu/c137467c13c7ac320f6405f8986722dc "Profiling macros for Tracy"
    
 */

/* ------------------------------------------------------- */

#include <tra/api.h>

/* ------------------------------------------------------- */

/*
  When you want to implement your own profiling macros you have
  to create a file called `tra-profiler-macros.h` and make sure
  that it's in your search paths. See `DEFINE CUSTOM MACROS`
  above.
 */
#if defined(TRA_PROFILER_USE_CUSTOM_MACROS)
#  include <tra-profiler-macros.h>
#endif

/* ------------------------------------------------------- */

#if !defined(TRA_PROFILER_FUNC_SET_THREAD_NAME)
#  define TRA_PROFILER_FUNC_SET_THREAD_NAME(threadName) { tra_profiler_set_thread_name(threadName); }
#endif

#if !defined(TRA_PROFILER_FUNC_FRAME_BEGIN)
#  define TRA_PROFILER_FUNC_FRAME_BEGIN(varName, frameTitle) { tra_profiler_frame_begin(frameTitle); }
#endif

#if !defined(TRA_PROFILER_FUNC_FRAME_END)
#  define TRA_PROFILER_FUNC_FRAME_END(varName, frameTitle) { tra_profiler_frame_end(frameTitle); }
#endif

#if !defined(TRA_PROFILER_FUNC_TIMER_BEGIN)
#  define TRA_PROFILER_FUNC_TIMER_BEGIN(varName, timerTitle) { tra_profiler_timer_begin(timerTitle); }
#endif

#if !defined(TRA_PROFILER_FUNC_TIMER_END)
#  define TRA_PROFILER_FUNC_TIMER_END(varName, timerTitle) { tra_profiler_timer_end(timerTitle); }
#endif

/* ------------------------------------------------------- */

#if defined(TRA_PROFILER_ENABLED)
#  define TRAP_SET_THREAD_NAME(threadName)      TRA_PROFILER_FUNC_SET_THREAD_NAME(threadName);
#  define TRAP_FRAME_BEGIN(varName, frameTitle) TRA_PROFILER_FUNC_FRAME_BEGIN(varName, frameTitle);
#  define TRAP_FRAME_END(varName, frameTitle)   TRA_PROFILER_FUNC_FRAME_END(varName, frameTitle);
#  define TRAP_TIMER_BEGIN(varName, timerTitle) TRA_PROFILER_FUNC_TIMER_BEGIN(varName, timerTitle); 
#  define TRAP_TIMER_END(varName, timerTitle)   TRA_PROFILER_FUNC_TIMER_END(varName, timerTitle);   
#else
#  define TRAP_SET_THREAD_NAME(threadName)      {}
#  define TRAP_FRAME_BEGIN(varName, frameTitle) {}
#  define TRAP_FRAME_END(varName, frameTitle)   {}
#  define TRAP_TIMER_BEGIN(varName, timerTitle) {}
#  define TRAP_TIMER_END(varName, timerTitle)   {}
#endif

/* ------------------------------------------------------- */

typedef struct tra_profiler_settings tra_profiler_settings;

/* ------------------------------------------------------- */

struct tra_profiler_settings {

  /* API the profiler needs to implement. */
  void(*set_thread_name)(const char* threadName);
  void(*frame_begin)(const char* frameTitle);
  void(*frame_end)(const char* frameTitle);
  void(*timer_begin)(const char* timerTitle);
  void(*timer_end)(const char* timerTitle);

  /* Profiler settings. */
  const char* output_filename; /* Filename where you want to store the profiled data (if applicable). */
};

/* ------------------------------------------------------- */

TRA_LIB_DLL int tra_profiler_start(tra_profiler_settings* cfg);
TRA_LIB_DLL int tra_profiler_stop();

/* ------------------------------------------------------- */

TRA_LIB_DLL void tra_profiler_set_thread_name(const char* threadName);
TRA_LIB_DLL void tra_profiler_frame_begin(const char* frameTitle);
TRA_LIB_DLL void tra_profiler_frame_end(const char* frameTitle);
TRA_LIB_DLL void tra_profiler_timer_begin(const char* timerTitle);
TRA_LIB_DLL void tra_profiler_timer_end(const char* timerTitle);

/* ------------------------------------------------------- */

#endif
