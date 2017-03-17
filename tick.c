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
#include <stdio.h>     // fprintf(), printf(), perror()
//-----------------------------------------------------------------------------
#include "stimer.h"
#include "sgpio.h"
//-----------------------------------------------------------------------------
// command line options
typedef struct options_ {
  int interval; // ms
  int gpio_num; // >=0
  int verbose;  // {0,1,2,3}
  int data;     // 0|1
  int negative; // 0|1  
  int meandr;   // 0|1
  int fake;     // 0|1  
  int tau;      // >=0
  int realtime; // 0|1
} options_t;
//-----------------------------------------------------------------------------
typedef struct tick_ {
  options_t   options;
  sgpio_t     gpio;
  stimer_t    timer;
  int         state;
  unsigned    counter;
  double      daytime;
  double      dt_min;
  double      dt_max;
  long double dt_sum;
} tick_t;
//-----------------------------------------------------------------------------
static void tick_usage()
{
  fprintf(stderr,
    "This is simple GPIO/timer tester\n"
    "Usage: tick [-options] [interval-ms]\n"
    "       tick --help\n");
  exit(EXIT_FAILURE);
}
//-----------------------------------------------------------------------------
static void tick_help()
{
  printf(
    "This is simple GPIO/timer tester\n"
    "Run:  tick [-options] [interval-ms]\n"
    "Options:\n"
    "   -h|--help          - show this help\n"
    "   -v|--verbose       - verbose output\n"
    "  -vv|--more-verbose  - more verbose output (or use -v twice)\n"
    " -vvv|--much-verbose  - more verbose output (or use -v thrice)\n"
    "   -d|--data          - output statistic to stdout (no verbose)\n"
    "   -g|--gpio          - number of GPIO channel (1 by default)\n"
    "   -n|--negative      - negative output\n"
    "   -m|--meandr        - meandr 2*T mode\n"
    "   -f|--fake          - fake GPIO\n"
    "   -t|--tau           - impulse time in 'bogoticks'\n"
    "   -r|--real-time     - real time mode (root required)\n"
    "interval-ms           - timer interval in ms (100 by default)\n");
  exit(EXIT_SUCCESS);
}
//-----------------------------------------------------------------------------
// parse command options
static void tick_parse_options(int argc, const char *argv[], options_t *o)
{
  int i;
 
  // set options by default
  o->interval  = 100; // ms
  o->gpio_num  = 1;   // 1 by default
  o->verbose   = 0;   // verbose level {0,1,2,3}
  o->data      = 0;   // output statistic to stdout
  o->negative  = 0;   // 0|1  
  o->meandr    = 0;   // 0|1
  o->fake      = 0;   // 0|1  
  o->tau       = 0;   // >=0
  o->realtime  = 0;   // 0|1

  // pase options
  for (i = 1; i < argc; i++)
  {
    if (argv[i][0] == '-')
    { // parse options
      if (!strcmp(argv[i], "-h") ||
          !strcmp(argv[i], "--help"))
      { // print help
        tick_help();
      }
      else if (!strcmp(argv[i], "-v") ||
               !strcmp(argv[i], "--verbose"))
      { // verbose level 1 or more
        o->verbose++;
        o->data = 0;
      }
      else if (!strcmp(argv[i], "-vv") ||
               !strcmp(argv[i], "--more-verbose"))
      { // verbode level 2
        o->verbose = 2;
        o->data    = 0;
      }
      else if (!strcmp(argv[i], "-vvv") ||
               !strcmp(argv[i], "--much-verbose"))
      { // verbode level 3
        o->verbose = 3;
        o->data    = 0;
      }
      else if (!strcmp(argv[i], "-d") ||
               !strcmp(argv[i], "--data"))
      { // output packet statistic to stdout
        o->verbose = 0;
        o->data    = 1;
      }
      else if (!strcmp(argv[i], "-g") ||
               !strcmp(argv[i], "--gpio"))
      { // gpio number
        if (++i >= argc) tick_usage();
        o->gpio_num = atoi(argv[i]);
        if (o->gpio_num < 0) o->gpio_num = 0;
      }
      else if (!strcmp(argv[i], "-n") ||
               !strcmp(argv[i], "--negative"))
      { // negative
        o->negative = 1;
      }
      else if (!strcmp(argv[i], "-m") ||
               !strcmp(argv[i], "--meandr"))
      { // meandr
        o->meandr = 1;
      }
      else if (!strcmp(argv[i], "-f") ||
               !strcmp(argv[i], "--fake"))
      { // fake
        o->fake = 1;
      }
      else if (!strcmp(argv[i], "-t") ||
               !strcmp(argv[i], "--tau"))
      { // tau
        if (++i >= argc) tick_usage();
        o->tau = atoi(argv[i]);
        if (o->tau < 0) o->tau = 0;
      }
      else if (!strcmp(argv[i], "-r") ||
               !strcmp(argv[i], "--real-time"))
      { // real time mode
        o->realtime = 1;
      }
      else
        tick_usage();
    }
    else
    { // interavl
      o->interval = atoi(argv[i]);
      if (o->interval <= 0) o->interval = 1;
    }
  } // for
}
//-----------------------------------------------------------------------------
// SIGINT handler (Ctrl-C)
static void tick_sigint_handler(void *context)
{
  tick_t *tick = (tick_t*) context;
  stimer_stop(&tick->timer);
  fprintf(stderr, "\nCtrl-C pressed\n");
} 
//-----------------------------------------------------------------------------
static int tick_timer_handler(void *context)
{
  // дергать ножку GPIO по прерыванию от таймера
  tick_t *tick = (tick_t*) context;
  const options_t *o = &tick->options; 
  sgpio_t *gpio      = &tick->gpio;
  double daytime = stimer_daytime();
  double dt = 0.;

  if (tick->state > 0)
  {
    dt = daytime - tick->daytime;
    if (dt < -12.*60*60)
        dt += 24.*60.*60.;
    else if (dt > 12.*60.*60.)
        dt -= 24.*60.*60.;
    
    tick->dt_sum += dt;
  }

  tick->daytime = daytime;

  // накапливать статистику по периоду прерываний
  if (tick->state == 0)
  {
    tick->dt_min = tick->dt_max = 0.;
    tick->state++;
  }
  else if (tick->state == 1)
  {
    tick->dt_min = tick->dt_max = dt;
    tick->state++;
  }
  else // if (tick->state == 2) 
  {
    if (tick->dt_max < dt) tick->dt_max = dt;
    if (tick->dt_min > dt) tick->dt_min = dt;
  }

  // up GPIO pin
  if (!o->fake)
    sgpio_set(gpio, !o->negative);

  // tau
  //...

  // down GPIO pin
  if (!o->fake)
    sgpio_set(gpio, o->negative);

  if (0)
  {
    fprintf(stderr, "Error: ...; exit\n");
    exit(EXIT_FAILURE);
  }
  
  // выводить статистику
  if (tick->state > 1 && o->data)
  { // #counter #daytime #dt_min #dt_max #dt
    printf("%10u %12.3f %12.3f %12.3f %12.3f\n",
           tick->counter, daytime * 1e3,
           tick->dt_min * 1e3, tick->dt_max * 1e3, dt * 1e3);
  }

  // счетчик прерываний
  tick->counter++;
  
  return 0;
}
//-----------------------------------------------------------------------------
static void tick_init(tick_t *tick)
{
  tick->state   = 0;
  tick->counter = 0;
  tick->daytime = 0.; 
  tick->dt_min  = 0.;
  tick->dt_max  = 0.;
  tick->dt_sum  = 0.;
}
//-----------------------------------------------------------------------------
int main(int argc, const char *argv[])
{
  int retv;
  tick_t tick;
  options_t *o    = &tick.options;
  sgpio_t *gpio   = &tick.gpio;
  stimer_t *timer = &tick.timer;
  long double dt_mid;
  
  FILE *fout; // statstics output (stdout/stderr)

  // разобрать опции командной строки
  tick_parse_options(argc, argv, o);

  // обнулить статистику
  tick_init(&tick);

  // вывести параметры запуска
  if (o->verbose >= 1)
  {
    printf("--> TICK start with next parameters:\n");
    printf("-->   interval      = %i ms\n", o->interval);
    printf("-->   gpio_num      = %i\n",    o->gpio_num);
    printf("-->   verbose level = %i\n",    o->verbose);
    printf("-->   data          = %i\n",    o->data);
    printf("-->   negative      = %s\n",    o->negative ? "yes" : "no");
    printf("-->   meandr        = %s\n",    o->meandr   ? "yes" : "no");
    printf("-->   fake GPIO     = %s\n",    o->fake     ? "yes" : "no");
    printf("-->   tau           = %i\n",    o->tau);
    printf("-->   real time     = %s\n",    o->realtime ? "yes" : "no");
  }
  
  // вывести на консоль начальное время
  if (o->verbose >= 2)
  {
    printf("->> local day time is ");
    stimer_fprint_daytime(stdout, stimer_daytime());
    printf("\n");
  }

  // инициализировать GPIO
  if (!o->fake)
  {
    sgpio_init(gpio, o->gpio_num);
    if (o->verbose >= 3)
      printf(">>> sgpio_init(%d) finish\n", o->gpio_num);

    if (1) // unexport
    {
      retv = sgpio_unexport(o->gpio_num);
      if (o->verbose >= 3)
        printf(">>> sgpio_unexport(%d) return '%s'\n",
               o->gpio_num, sgpio_error_str(retv));
    }

    if (1) // export
    {
      retv = sgpio_export(o->gpio_num);
      if (o->verbose >= 3)
        printf(">>> sgpio_export(%d) return '%s'\n",
               o->gpio_num, sgpio_error_str(retv));
    }
  
    // устаовить режим вывода порта GPIO
    retv = sgpio_mode(gpio, SGPIO_DIR_OUT, SGPIO_EDGE_NONE);
    if (o->verbose >= 3)
      printf(">>> sgpio_mode(%d,%d,%d) return '%s'\n",
             sgpio_num(gpio), SGPIO_DIR_OUT, SGPIO_EDGE_NONE,
             sgpio_error_str(retv));
   
    // установить начальный уровень сигнала на порте GPIO
    retv = sgpio_set(gpio, o->negative);
    if (o->verbose >= 3)
      printf(">>> sgpio_set(%i,%i) return '%s'\n",
             sgpio_num(gpio), o->negative,
             sgpio_error_str(retv));
  } // if (!o->fake)

  // зарегистрировать обработчик сигнала SIGINT (CTRL+C)
  retv = stimer_sigint(tick_sigint_handler, (void*) &tick);
  if (o->verbose >= 3)
    printf(">>> stimer_sigint_handler() return %d\n", retv);

  // установить "real-time" приоритет
  if (o->realtime)
  {
    retv = stimer_realtime();
    if (o->verbose >= 3)
      printf(">>> stimer_realtime() return %d\n", retv);
  }

  // инициализировать таймер
  retv = stimer_init(timer, tick_timer_handler, (void*) &tick);
  if (o->verbose >= 3)
    printf(">>> stimer_init() return %d\n", retv);
  if (retv != 0)
  {
    perror("error: stimer_init() fail; exit");
    exit(EXIT_FAILURE);
  }

  // запустить таймер
  retv = stimer_start(timer, (double) o->interval);
  if (o->verbose >= 3)
    printf(">>> stimer_start(%d) return %d\n", o->interval, retv);
  if (retv != 0)
  {
    perror("error: stimer_start() fail; exit");
    exit(EXIT_FAILURE);
  }

  // ждать сигнала и вызывать обработчик "вечно"
  retv = stimer_loop(timer);
  if (o->verbose >= 3)
    printf(">>> stimer_loop() return %i\n", retv);
  if (retv < 0)
  {
    perror("error: stimer_loop() fail; exit");
    exit(EXIT_FAILURE);
  }

  if (!o->fake && 1) // set to input (more safe mode)
  {
    retv = sgpio_mode(gpio, SGPIO_DIR_IN, SGPIO_EDGE_NONE);
    if (o->verbose >= 3)
      printf(">>> sgpio_mode(%d,%d,%d) return '%s'\n",
             sgpio_num(gpio), SGPIO_DIR_IN, SGPIO_EDGE_NONE,
             sgpio_error_str(retv));
  }

  if (!o->fake && 1) // unexport
  {
    retv = sgpio_unexport(o->gpio_num);
    if (o->verbose >= 3)
      printf(">>> sgpio_unexport(%d) return '%s'\n",
             o->gpio_num, sgpio_error_str(retv));
  }

  sgpio_free(gpio);

  // вывести результаты накопленной статистики
  fout = o->data ? stderr : stdout;
  dt_mid = tick.dt_sum / ((long double) tick.counter - 1);
  fprintf(fout, "--- TICK statistics ---\n");
  fprintf(fout, "=> counter         = %u\n",   tick.counter);
  fprintf(fout, "=> dt_min          = %.9f\n", tick.dt_min);
  fprintf(fout, "=> dt_max          = %.9f\n", tick.dt_max);
  fprintf(fout, "=> dt_max - dt_min = %.9f\n", tick.dt_max - tick.dt_min);
  fprintf(fout, "=> dt_mid          = %.9f\n", (double) dt_mid);

  return EXIT_SUCCESS;
}
//-----------------------------------------------------------------------------

/*** end of "tick.c" ***/

