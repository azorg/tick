/*
 * Простой генератор тактовых импульсов на выходном порте GPIO
 * Версия: 0.1a
 * Файл: "tick.c"
 * Кодировка: UTF-8
 */

//-----------------------------------------------------------------------------
//#include <math.h>
#include <stdlib.h>    // exit(), EXIT_SUCCESS, EXIT_FAILURE, atoi()
#include <string.h>    // strcmp()
//#include <unistd.h>  //
#include <stdint.h>    // `uint32_t`
#include <stdio.h>     // fprintf(), printf(), perror()
#include <string.h>    // memset()
#include <signal.h>    // sigaction(),  sigemptyset(), sigprocmask()
#include <time.h>      // clock_gettime(), clock_getres(), time_t, ...
#include <sched.h>     // sched_setscheduler(), SCHED_FIFO, ...
//-----------------------------------------------------------------------------
#include "sgpio.h"
//-----------------------------------------------------------------------------
// используемый таймер
// CLOCK_REALTIME CLOCK_MONOTONIC
// CLOCK_PROCESS_CPUTIME_ID CLOCK_THREAD_CPUTIME_ID (since Linux 2.6.12)
// CLOCK_REALTIME_HR CLOCK_MONOTONIC_HR (MontaVista)
#define CLOCKID CLOCK_REALTIME
//-----------------------------------------------------------------------------
// сигнал таймера
#define SIG SIGRTMIN
//-----------------------------------------------------------------------------
#define err_exit(msg) \
  do { perror("Error: " msg); exit(EXIT_FAILURE); } while (0)
//-----------------------------------------------------------------------------
static void usage()
{
  fprintf(stderr,
    "This is simple GPIO/timer tester\n"
    "Usage: tick [-options] [interval-ms]\n"
    "       tick --help\n");
  exit(EXIT_FAILURE);
}
//-----------------------------------------------------------------------------
static void help()
{
  printf(
    "This is simple GPIO/timer tester\n"
    "Run:  tick [-options] [interval-ms]\n"
    "Options:\n"
    "   -h|--help              show this help\n"
    "   -v|--verbose           verbose output\n"
    "  -vv|--more-verbose      more verbose output (or use -v twice)\n"
    "   -g|--gpio              number of GPIO channel\n"
    "   -n|--negative          negative output\n"
    "   -f|--fake              fake GPIO\n"
    "   -t|--tau               impulse time in 'bogoticks'\n"
    "   -r|--real-time         real time mode (root required)\n"
    "By default interval-ms is 10 ms, gpio is 1\n");
  exit(EXIT_SUCCESS);
}
//-----------------------------------------------------------------------------
// options
static int interval  = 10; // ms
static int gpio_num  = 1;
static int verbose   = 1;  // verbose level {0,1,2,3}
static int negative  = 0;  // 0|1  
static int fake      = 0;  // 0|1  
static int tau       = 0;  // >=0
static int realtime  = 0;  // 0|1
//-----------------------------------------------------------------------------
// global variable
unsigned overrun = 0; // FIXME
int    stop_flag = 0; // set to 1 if Ctrl-C pressed
uint32_t counter = 1; // interrupt counter
sgpio_t gpio;
//-----------------------------------------------------------------------------
// parse command options
static void parse_options(int argc, const char *argv[])
{
  int i;
  for (i = 1; i < argc; i++)
  {
    if (argv[i][0] == '-')
    { // parse options
      if (!strcmp(argv[i], "-h") ||
          !strcmp(argv[i], "--help"))
      { // print help
        help();
      }
      else if (!strcmp(argv[i], "-v") ||
               !strcmp(argv[i], "--verbose"))
      { // verbose level 1
        verbose++;
      }
      else if (!strcmp(argv[i], "-vv") ||
               !strcmp(argv[i], "--more-verbose"))
      { // verbode level 2
        verbose = 3;
      }
      else if (!strcmp(argv[i], "-g") ||
               !strcmp(argv[i], "--gpio"))
      { // gpio
        if (++i >= argc) usage();
        gpio_num = atoi(argv[i]);
        if (gpio_num < 0) gpio_num = 0;
      }
      else if (!strcmp(argv[i], "-n") ||
               !strcmp(argv[i], "--negative"))
      { // negative
        negative = 1;
      }
      else if (!strcmp(argv[i], "-f") ||
               !strcmp(argv[i], "--fake"))
      { // fake
        fake = 1;
      }
      else if (!strcmp(argv[i], "-t") ||
               !strcmp(argv[i], "--tau"))
      { // tau
        if (++i >= argc) usage();
        tau = atoi(argv[i]);
        if (tau < 0) tau = 0;
      }
      else if (!strcmp(argv[i], "-r") ||
               !strcmp(argv[i], "--real-time"))
      { // real time mode
        realtime = 1;
      }
      else
        usage();
    }
    else
    { // interavl
      interval = atoi(argv[i]);
      if (interval <= 0) interval = 1;
    }
  } // for
}
//-----------------------------------------------------------------------------
// set the process to real-time privs
static void set_realtime_priority()
{
  struct sched_param schp;
  memset(&schp, 0, sizeof(schp));
  schp.sched_priority = sched_get_priority_max(SCHED_FIFO);

  if (sched_setscheduler(0, SCHED_FIFO, &schp) != 0)
    err_exit("sched_setscheduler(SCHED_FIFO)");
}
//-----------------------------------------------------------------------------
// вернуть время суток (цена младшего разряда 24*60*60/2**32)
static uint32_t get_daytime()
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
//-----------------------------------------------------------------------------
// перевести суточное время в секунды [0..24h]
#if 0
static double daytime_to_sec(uint32_t daytime)
{
  double t = (double) daytime;
  return t * ((24.*60.*60.) / 4294967296.);
}
#endif
//-----------------------------------------------------------------------------
// перевести разницу суточного времени в секунды [12...12h]
#if 0
static double deltatime_to_sec(int32_t delta_daytime)
{
  double t = (double) delta_daytime;
  return t * ((24.*60.*60.) / 4294967296.);
}
#endif
//-----------------------------------------------------------------------------
// перевести разницу суточного времени в мс
//static int deltatime_to_ms(int32_t delta_daytime)
//{
//double t = (double) delta_daytime;
//  return (int) (t * ((24.*60.*60.*1000.) / 4294967296.));
//}
//-----------------------------------------------------------------------------
// преобразовать время из double [sec] в `struct timespec`
static struct timespec double_to_ts(double t)
{
  struct timespec ts;
  ts.tv_sec  = (time_t) t;
  ts.tv_nsec = (long) ((t - (double) ts.tv_sec) * 1e9);
  return ts;
}
//-----------------------------------------------------------------------------
// распечатать суточное время в формате HH:MM:SS.mmmuuu
static void print_daytime(uint32_t daytime)
{
  unsigned h, m, s, us;
  double t = (double) daytime;
  t *= ((24.*60.*60.) / 4294967296.); // to seconds
  s = (unsigned) t;
  h =  s / 3600;     // часы
  m = (s / 60) % 60; // минуты
  s =  s       % 60; // секунды
  t -= (double) (h * 3600 + m * 60 + s);
  us = (unsigned) (t * 1e6);
  printf("%02u:%02u:%02u.%06u", h, m, s, us);
}
//-----------------------------------------------------------------------------
// обработчик сигнала таймера
static void timer_handler(int signo, siginfo_t *si, void *context)
{
  if (si->si_code == SI_TIMER)
  {
    int retv;
    timer_t *tidp = si->si_value.sival_ptr;

    retv = timer_getoverrun(*tidp);
    if (retv == -1)
      err_exit("timer_getoverrun() failed; exit");
    else
      overrun += retv;
  }
}
//-----------------------------------------------------------------------------
// обработчик сигнала SIGINT (Ctrl-C)
static void sigint_handler(int signo)
{
  stop_flag = 1;
}
//-----------------------------------------------------------------------------
// основная функция взвода таймера и ожидания прерываний
static void tick()
{
  //int retv;

  sigset_t mask;
  struct sigevent sigev;
  struct sigaction sa;
  struct itimerspec ival;
  timer_t timerid;

  if (verbose >= 1)
  {
    printf("TICK run with next parameters:\n");
    printf("  interval [ms] = %i\n", interval);
    printf("  verbose level = %i\n", verbose);
    printf("  negative      = %i\n", negative);
    printf("  tau           = %i\n", tau);
    printf("  real time     = %s\n", realtime ? "yes" : "no");
  }

  // зарегистрировать обработчик сигнала таймера
  if (verbose >= 3)
    printf("Establishing handler for signal %d\n", SIG);
  memset((void*) &sa, 0, sizeof(sa));
  sa.sa_sigaction = timer_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO | SA_RESTART;
  if (sigaction(SIG, &sa, NULL) == -1)
    err_exit("sigaction() failed; exit");

  // разблокировать сигнал (хотя он и так по умолчанию не блокирован)
  if (verbose >= 3)
    printf("Unblocking signal %d\n", SIG);
  sigemptyset(&mask);
  sigaddset(&mask, SIG);
  if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1)
    err_exit("sigprocmask() failed; exit");

  // создать таймер
  memset((void*) &sigev, 0, sizeof(sigev));
  sigev.sigev_notify = SIGEV_SIGNAL; // SIGEV_NONE SIGEV_SIGNAL SIGEV_THREAD...
  sigev.sigev_signo = SIG;
  sigev.sigev_value.sival_ptr = (void*) &timerid;
  //sigev.sigev_value.sival_int = 1; // use sival_ptr instead!
  //sigev.sigev_notify_function   = ...; // for SIGEV_THREAD
  //sigev.sigev_notify_attributes = ...; // for SIGEV_THREAD
  //sigev.sigev_notify_thread_id  = ...; // for SIGEV_THREAD_ID
  if (timer_create(CLOCKID, &sigev, &timerid) == -1)
    err_exit("timer_create() failed; exit");
  if (verbose >= 3)
    printf("Create timer ID = 0x%lx\n", (long) timerid);

  // запустить таймер
  ival.it_value    = double_to_ts(((double) interval) * 1e-3);
  ival.it_interval = ival.it_value;
  if (timer_settime(timerid, 0, &ival, NULL) == -1)
    err_exit("timer_settime() failed; exit");

  // дергать ножку GPIO по прерыванию от таймера
  for (;; counter++)
  {
    //uint32_t up_time;   // время активации сигнала
    //uint32_t down_time; // время деактивации сигнала

    if (stop_flag)
    { // Ctrl-C pressed
      fprintf(stderr, "\nCtrl-C pressed; exit\n");
      counter--;
      break;
    }

    // разблокировать сигнал таймера
    sigemptyset(&mask);
    sigaddset(&mask, SIG);
    if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1)
      err_exit("sigprocmask() failed; exit");

    pause(); // заснуть до прихода сигнала от таймера

    // заблокировать сигнал, чтобы он не прерывал select()
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
      err_exit("sigprocmask() failed; exit");

    if (verbose >= 2)
    {
      uint32_t daytime = get_daytime();
      printf("Tick: time=");
      print_daytime(daytime);
      printf("\n");
    }

    // up GPIO
    if (!fake)
      sgpio_set(&gpio, !negative);

    // tau
    //...

    // down GPIO
    if (!fake)
      sgpio_set(&gpio, negative);
  
    if (0)
    {
      fprintf(stderr, "Error: ...; exit\n");
      exit(EXIT_FAILURE);
    }

  } // for(;; counter++)
}
//-----------------------------------------------------------------------------
int main(int argc, const char *argv[])
{
  uint32_t daytime = get_daytime();
  int retv;

  // разобрать опции командной строки
  parse_options(argc, argv);

  // установить "real-time" приоритет
  if (realtime)
    set_realtime_priority();

  // инициализировать GPIO
  sgpio_init(&gpio, gpio_num);

  if (!fake && 1) // unexport
  {
    retv = sgpio_unexport(gpio_num);
    if (verbose >= 3)
      printf(">>> sgpio_unexport(%d): '%s'\n",
             gpio_num, sgpio_error_str(retv));
  }

  if (!fake && 1) // export
  {
    retv = sgpio_export(gpio_num);
    if (verbose >= 3)
      printf(">>> sgpio_export(%d): '%s'\n",
             gpio_num, sgpio_error_str(retv));
  }
  
  if (!fake)
  { // устаовить режим вывода порта GPIO
    retv = sgpio_mode(&gpio, SGPIO_DIR_OUT, SGPIO_EDGE_NONE);
    if (verbose >= 3)
      printf(">>> sgpio_mode(%d,%d,%d): '%s'\n",
             sgpio_num(&gpio), SGPIO_DIR_OUT, SGPIO_EDGE_NONE,
             sgpio_error_str(retv));
   
    // установить начальный уровень сигнала на порте GPIO
    retv = sgpio_set(&gpio, negative);
    if (retv != SGPIO_ERR_NONE && verbose >= 3)
      printf(">>> sgpio_set(%i,%i): '%s'\n",
             sgpio_num(&gpio), negative,
             sgpio_error_str(retv));
  }

  // зарегистрировать обработчик сигнала SIGINT
  if (verbose >= 3)
    printf("Establishing handler for signal %d\n", SIGINT);
#if 0
  memset((void*) &sa, 0, sizeof(sa));
  sa.sa_handler = sigint_handler;
  sigemptyset(&sa.sa_mask);
  //sa.sa_flags = 0;
  if (sigaction(SIGINT, &sa, NULL) == -1)
    err_exit("sigaction() failed; exit");
#else
  signal(SIGINT, sigint_handler); // old school ;-)
#endif

  // вывести на консоль начальное время
  if (verbose >= 2)
  {
    printf("Local day time is ");
    print_daytime(daytime);
    printf("\n");
  }

  // запустить основную функцию
  tick();

  if (!fake && 1) // set to input (more safe mode)
  {
    retv = sgpio_mode(&gpio, SGPIO_DIR_IN, SGPIO_EDGE_NONE);
    printf(">>> sgpio_set_dir(%d,%d,%d): '%s'\n",
           sgpio_num(&gpio), SGPIO_DIR_IN, SGPIO_EDGE_NONE,
           sgpio_error_str(retv));
  }

  if (!fake && 1) // unexport
  {
    retv = sgpio_unexport(gpio_num);
    printf(">>> sgpio_unexport(%d): '%s'\n",
           gpio_num, sgpio_error_str(retv));
  }

  sgpio_free(&gpio);

  return EXIT_SUCCESS;
}
//-----------------------------------------------------------------------------

/*** end of "tick.c" ***/

