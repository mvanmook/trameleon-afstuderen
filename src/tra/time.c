/* ------------------------------------------------------- */

#include <tra/time.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

/* Linux */
#if defined(__linux)
#  define HAVE_POSIX_TIMER
#  include <time.h>
#  include <unistd.h>
#  if !defined(__ARM_ARCH) && !defined(__arm__)
#    include <x86intrin.h> /* for rdtsc, cycle count. */
#  endif
#  ifdef CLOCK_MONOTONIC
#     define CLOCKID CLOCK_MONOTONIC
#  else
#     define CLOCKID CLOCK_REALTIME
#  endif
#endif

/* Apple */
#if defined(__APPLE__)
#  define HAVE_MACH_TIMER
#  include <mach/mach_time.h>
#  if !defined(__ARM_ARCH) && !defined(__aarch64__) && !defined(__arm__)
#    include <x86intrin.h>
#  endif
#endif

/* Windows */
#if defined(_WIN32) 
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

/* ------------------------------------------------------- */

/* Clock time includes */
#if defined(_WIN32)
#  include <time.h>
#  include <Winsock2.h>
#elif defined(__linux)
#  include <sys/time.h>
#else
#  include <sys/time.h>
#endif

/* ------------------------------------------------------- */

#if defined(_WIN32)
static LARGE_INTEGER win_freq;
#endif

int tra_time_init() {

#if defined(_WIN32)
  if (FALSE == QueryPerformanceFrequency(&win_freq)) {
    TRAE("Failed to query the Query Performance Counter Frequency.");
    return -1;
  }
#endif
  
  return 0;
}

/* ------------------------------------------------------- */

uint64_t tra_nanos() {
  
#if defined(__APPLE__)
    uint64_t now;
    now = mach_absolute_time();
    now *= info.numer;
    now /= info.denom;
    return now;
#elif defined(__linux)
    uint64_t now;
    struct timespec spec;
    clock_gettime(CLOCKID, &spec);
    now = spec.tv_sec * 1.0e9 + spec.tv_nsec;
    return now;
#elif defined(_WIN32)
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (uint64_t) ((1e9 * now.QuadPart)  / win_freq.QuadPart);
#endif
}

uint64_t tra_micros() {
  return tra_nanos() / 1e3;
}

uint64_t tra_millis() {
  return tra_nanos() / 1e6;
}

uint64_t tra_seconds() {
  return tra_nanos() / 1e9;
}

/* ------------------------------------------------------- */

#if defined(_WIN32)

uint64_t tra_get_clock_millis() {
  SYSTEMTIME system_time = { 0 } ;
  GetSystemTime(&system_time);
  return system_time.wMilliseconds; 
}

uint64_t tra_get_clock_micros() {
  SYSTEMTIME system_time = { 0 } ;
  GetSystemTime(&system_time);
  return system_time.wMilliseconds * 1000;  
}

#else

uint64_t tra_get_clock_millis() {
  struct timeval miltime;
  gettimeofday(&miltime, NULL);
  int milli = miltime.tv_usec / 1000;
  return milli;
}

uint64_t tra_get_clock_micros() {
  struct timeval miltime;
  gettimeofday(&miltime, NULL);
  int milli = miltime.tv_usec / 100;
  return milli;
}

#endif

/*
  `strftime()` returns the number of bytes written on success or
  0 when it couldn't write everything.
 
  @todo add error checking to the call to `time()`. 

*/
int tra_strftime(char* dst, uint32_t nbytes, const char* fmt, uint32_t* nwritten) {

  int r = 0;
  time_t t;
  struct tm* info = NULL;

  if (NULL == fmt) {
    TRAE("Given `fmt` is NULL, cannot `tra_strftime()`.");
    return -1;
  }
  
  if (NULL == dst) {
    TRAE("Given `dst` is NULL cannot get time string.");
    return -2;
  }

  if (0 == nbytes) {
    TRAE("Given `nbytes` is 0, cannot write into that.");
    return -3;
  }

  t = time(NULL);
  info = localtime(&t);

  r = strftime(dst, nbytes, fmt, info);
  if (0 == r) {
    return -4;
  }

  if (NULL != nwritten) {
    /*
      `snprintf()` returns the number of bytes it wrote excluding
      the null character, or < 0 when an error occurs. We add the 
      `+ 1` here to include the null char. 
    */
    *nwritten = r + 1;
  }
  
  return 0;
}


/* ------------------------------------------------------- */

#if defined(_WIN32)

void tra_sleep_millis(uint32_t num) {
  Sleep(num);
}

#else

void tra_sleep_millis(uint32_t num) {
  usleep(num * 1e3);
}

#endif

/* ------------------------------------------------------- */
