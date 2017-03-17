/*
 * Simple Linux timer wrapper
 * File: "stimer.c"
 */
//----------------------------------------------------------------------------
#include "stimer.h" // `stimer_t`
#include <string.h> // memset()
#include <stdio.h>  // perror()
#include <unistd.h> // pause()
//-----------------------------------------------------------------------------
// set the process to real-time privs via call sched_setscheduler()
int stimer_realtime()
{
  int retv;
  struct sched_param schp;
  memset(&schp, 0, sizeof(schp));
  schp.sched_priority = sched_get_priority_max(SCHED_FIFO);

  retv = sched_setscheduler(0, SCHED_FIFO, &schp);
  if (retv)
    perror("error: sched_setscheduler(SCHED_FIFO)");

  return retv;
}
//-----------------------------------------------------------------------------
// get day time (LSB is 24*60*60/2**32 seconds)
uint32_t stimer_daytime()
{
  struct timespec ts;
  struct tm tm;
  time_t time;
  double t;

  //clock_gettime(CLOCK_REALTIME, &ts);
  clock_gettime(CLOCK_MONOTONIC, &ts);

  time = (time_t) ts.tv_sec;
  localtime_r(&time, &tm);
  time = tm.tm_sec + tm.tm_min * 60 + tm.tm_hour * 3600;

  t  = ((double) ts.tv_nsec) * 1e-9;
  t += ((double) time);
  t *= (4294967296. / (24.*60.*60.));

  return (uint32_t) t;
}
//----------------------------------------------------------------------------
#ifdef STIMER_EXTRA
// convert day timer to seconds [0..24h]
double stimer_daytime_to_sec(uint32_t daytime)
{
  double t = (double) daytime;
  return t * ((24.*60.*60.) / 4294967296.);
}
//-----------------------------------------------------------------------------
// convert day time delta to seconds [-12h...12h]
double stimer_deltatime_to_sec(int32_t delta_daytime)
{
  double t = (double) delta_daytime;
  return t * ((24.*60.*60.) / 4294967296.);
}
#endif // STIMER_EXTRA
//-----------------------------------------------------------------------------
// convert time in seconds to `struct timespec`
struct timespec stimer_double_to_ts(double t)
{
  struct timespec ts;
  ts.tv_sec  = (time_t) t;
  ts.tv_nsec = (long) ((t - (double) ts.tv_sec) * 1e9);
  return ts;
}
//----------------------------------------------------------------------------
// print day time to file in next format: HH:MM:SS.mmmuuu
void stimer_fprint_daytime(FILE *stream, uint32_t daytime)
{
  unsigned h, m, s, us;
  double t = (double) daytime;
  t *= ((24.*60.*60.) / 4294967296.); // to seconds
  s = (unsigned) t;
  h =  s / 3600;     // hours
  m = (s / 60) % 60; // minutes
  s =  s       % 60; // seconds
  t -= (double) (h * 3600 + m * 60 + s);
  us = (unsigned) (t * 1e6);
  fprintf(stream, "%02u:%02u:%02u.%06u", h, m, s, us);
}
//-----------------------------------------------------------------------------
// timer handler
static void stimer_handler(int signo, siginfo_t *si, void *ucontext)
{
  if (si->si_code == SI_TIMER)
  {
    stimer_t *timer = (stimer_t*) si->si_value.sival_ptr;
    int retv = timer_getoverrun(timer->timerid);
    if (retv < 0)
    {
      perror("timer_getoverrun() failed");
    }
    else
      timer->overrun += retv;
  }
}
//----------------------------------------------------------------------------
// init timer
int stimer_init(stimer_t *self, int (*fn)(void *context), void *context)
{
  // save user callback funcion
  self->fn = fn;

  // save context
  self->context = context;

  // reset stop flag
  self->stop = 0;

  // reset overrrun counter
  self->overrun = 0;

  // establishing handler for signal STIMER_SIG
  memset((void*) &self->sa, 0, sizeof(self->sa));
  self->sa.sa_flags = SA_SIGINFO | SA_RESTART;
  self->sa.sa_sigaction = stimer_handler;
  sigemptyset(&self->sa.sa_mask);
  if (sigaction(STIMER_SIG, &self->sa, NULL) < 0)
  {
    perror("error in stimer_init(): sigaction() failed; return -1");
    return -1;
  }

  // block signal STIMER_SIG temporarily
  sigemptyset(&self->mask);
  sigaddset(&self->mask, STIMER_SIG);
  if (sigprocmask(SIG_SETMASK, &self->mask, NULL) < 0)
  {
    perror("error in stimer_init(): sigprocmask(SIG_SETMASK) failed; return -2");
    return -2;
  }

  // create timer with ID = timerid
  memset((void*) &self->sev, 0, sizeof(self->sev));
  self->sev.sigev_notify = SIGEV_SIGNAL; // SIGEV_NONE SIGEV_SIGNAL SIGEV_THREAD...
  self->sev.sigev_signo  = STIMER_SIG;
#if 1
  self->sev.sigev_value.sival_ptr = (void*) self; // very important!
#else
  self->sev.sigev_value.sival_int = 1; // use sival_ptr instead!
#endif
  //self->sev.sigev_notify_function     = ...; // for SIGEV_THREAD
  //self->sev.sigev_notify_attributes   = ...; // for SIGEV_THREAD
  //self->sigev.sigev_notify_thread_id  = ...; // for SIGEV_THREAD_ID
  if (timer_create(STIMER_CLOCKID, &self->sev, &self->timerid) < 0)
  {
    perror("error in stimer_init(): timer_create() failed; return -3");
    return -3;
  }

  return 0;
}
//----------------------------------------------------------------------------
// start timer
int stimer_start(stimer_t *self, double interval_ms)
{
  // unblock signal STIMER_SIG
  sigemptyset(&self->mask);
  sigaddset(&self->mask, STIMER_SIG);
  if (sigprocmask(SIG_UNBLOCK, &self->mask, NULL) < 0)
  {
    perror("error in stimer_start(): sigprocmask(SIG_UNBLOCK) failed; return -1");
    return -1;
  }

  // start time
  self->ival.it_value    = stimer_double_to_ts(((double) interval_ms) * 1e-3);
  self->ival.it_interval = self->ival.it_value;
  if (timer_settime(self->timerid, 0, &self->ival, NULL) < 0)
  {
    perror("error in stimer_start(): timer_settime() failed; return -2");
    return -2;
  }

  return 0;
}
//----------------------------------------------------------------------------
// stop timer
void stimer_stop(stimer_t *self)
{
  self->stop = 1;
}
//----------------------------------------------------------------------------
// timer main loop
int stimer_loop(stimer_t *self)
{
  for (;;)
  {
    if (self->stop) return 0;

    // unlock timer signal
    sigemptyset(&self->mask);
    sigaddset(&self->mask, STIMER_SIG);
    if (sigprocmask(SIG_UNBLOCK, &self->mask, NULL) < 0)
    {
      perror("error in stimer_main_loop(): sigprocmask() failed #1; exit");
      return -1;
    }

    // sleep and wait signal
    pause();

    // lock timer signal for normal select() work
    if (sigprocmask(SIG_BLOCK, &self->mask, NULL) < 0)
    {
      perror("error in stimer_main_loop(): sigprocmask() failed #2; exit");
      return -1;
    }

    if (self->fn != (int (*)(void*)) NULL)
    { // callback user function
      int retv = self->fn(self->context);
      if (retv)
        return retv;
    }
  } // for(;;)
}
//----------------------------------------------------------------------------
/*** end of "stimer.c" file ***/

