/*
 * Simple Linux timer wrapper
 * File: "stimer.h"
 */

#ifndef STIMER_H
#define STIMER_H
//-----------------------------------------------------------------------------
#include <time.h>   // clock_gettime(), clock_getres(), time_t, ...
#include <signal.h> // sigaction(),  sigemptyset(), sigprocmask()
#include <sched.h>  // sched_setscheduler(), SCHED_FIFO, ...
#include <stdint.h> // `uint32_t`
#include <stdio.h>  // `FILE`, fprintf()
//-----------------------------------------------------------------------------
// timer used
//   CLOCK_REALTIME CLOCK_MONOTONIC
//   CLOCK_PROCESS_CPUTIME_ID CLOCK_THREAD_CPUTIME_ID (since Linux 2.6.12)
//   CLOCK_REALTIME_HR CLOCK_MONOTONIC_HR (MontaVista)
#define STIMER_CLOCKID CLOCK_REALTIME

// timer signal
#define STIMER_SIG SIGRTMIN

// add some extra funcions
#define STIMER_EXTRA
//----------------------------------------------------------------------------
// `ti_t` type structure
typedef struct stimer_ {
  int stop;
  int (*fn)(void *context);
  void *context;
  unsigned overrun;
  sigset_t mask;
  struct sigevent sev;
  struct sigaction sa;
  struct itimerspec ival;
  timer_t timerid;
} stimer_t;
//----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus
//----------------------------------------------------------------------------
// set the process to real-time privs via call sched_setscheduler()
int stimer_realtime();
//----------------------------------------------------------------------------
// get day time (LSB is 24*60*60/2**32 seconds)
uint32_t stimer_daytime();
//----------------------------------------------------------------------------
#ifdef STIMER_EXTRA
// convert day timer to seconds [0..24h]
double stimer_daytime_to_sec(uint32_t daytime);
//-----------------------------------------------------------------------------
// convert day time delta to seconds [-12h...12h]
double stimer_deltatime_to_sec(int32_t delta_daytime);
#endif // STIMER_EXTRA
//----------------------------------------------------------------------------
// convert time in seconds to `struct timespec`
struct timespec stimer_double_to_ts(double t);
//----------------------------------------------------------------------------
// print day time to file in next format: HH:MM:SS.mmmuuu
void stimer_fprint_daytime(FILE *stream, uint32_t daytime);
//----------------------------------------------------------------------------
// init timer
int stimer_init(stimer_t *self, int (*fn)(void *context), void *context);
//----------------------------------------------------------------------------
// start timer
int stimer_start(stimer_t *self, double interval_ms);
//----------------------------------------------------------------------------
// stop timer
void stimer_stop(stimer_t *self);
//----------------------------------------------------------------------------
// timer main loop
int stimer_loop(stimer_t *self);
//----------------------------------------------------------------------------
#ifdef __cplusplus
}
#endif // __cplusplus
//----------------------------------------------------------------------------
#endif // STIMER_H

/*** end of "stimer.h" file ***/

