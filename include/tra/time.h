#ifndef TRA_TIME_H
#define TRA_TIME_H

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

  TIME
  ==== 

  
  GENERAL INFO:

    The `time.h` gives you several time related functions that
    you can use to e.g. get the time of the day, a high
    resolution time stamp, etc.  Getting the accurate time is
    tricky and there are many articles about this; for our
    purposes, where we mostly use time to measure relative
    events we seem to get away by using the [Time Stamp
    Counter][1].
  
  USAGE:

    Before you can use the timer functions you have to call
    `tra_time_init()`. This will set some initial variables which
    are required on certain platforms (Windows).

  TIME STAMP COUNTER:

    According to [wikipedia][1], the time stamp counter counts
    the number of cpu cycles since reset and it can be retrieved
    via the `rdtsc` instruction. Recent (?) Intel processors
    include a constant time stamp counter. You can use `cat
    /proc/cpuinfo` to check if the flag `contstant_tsc` is
    available. When this flag has been set, the TSC ticks at the
    CPU nominal frequency regardless of the actual CPU clock
    frequency.

    The monotonic time stamp counter can be retrieved using
    CLOCK_MONOTONIC on Linux and it will return a value that can
    be used to time events. This is what we use on Linux; on
    Windows we use the `QueryPerformanceCounter`.

  REFERENCES:

    [0]: https://docs.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps?redirectedfrom=MSDN "Acquiring high-resolution time stamps"
    [1]: https://en.wikipedia.org/wiki/Time_Stamp_Counter
    [2]: https://stackoverflow.com/a/6749766 "How to create a high resolution timer on Linux ..."
    [3]: https://www.softprayog.in/tutorials/alarm-sleep-and-high-resolution-timers "Great article on available timers on Linux."
    [4]: https://stackoverflow.com/a/29795430 "Getting high resolution time on Windows."

*/

#include <stdint.h>
#include <tra/api.h>

/* Initialize the time library (does some caching). */
TRA_LIB_DLL int tra_time_init();

/* Measuring events */
TRA_LIB_DLL uint64_t tra_nanos();
TRA_LIB_DLL uint64_t tra_micros();
TRA_LIB_DLL uint64_t tra_millis();
TRA_LIB_DLL uint64_t tra_seconds();

/* Wall Clock Measurements */
TRA_LIB_DLL uint64_t tra_get_clock_millis();
TRA_LIB_DLL uint64_t tra_get_clock_micros();
TRA_LIB_DLL int tra_strftime(char* dst, uint32_t nbytes, const char* fmt, uint32_t* nwritten); /* [IMPORTANT] Returns a string using the given format. Returns 0 when we've written the time string into the buffer; or < 0 when something went wrong (when the buffer is too small we will still return 0). When `nwritten` is not NULL, it is set to the total number of bytes written INCLUDING the null character. */

/* Utils */
TRA_LIB_DLL void tra_sleep_millis(uint32_t num);

#endif
