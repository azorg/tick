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
// global variable
sgpio_t gpio;
stimer_t timer;
options_t o;
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
    "   -h|--help          - show this help\n"
    "   -v|--verbose       - verbose output\n"
    "  -vv|--more-verbose  - more verbose output (or use -v twice)\n"
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
static void parse_options(int argc, const char *argv[], options_t *o)
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
        help();
      }
      else if (!strcmp(argv[i], "-v") ||
               !strcmp(argv[i], "--verbose"))
      { // verbose level 1
        o->verbose++;
        o->data = 0;
      }
      else if (!strcmp(argv[i], "-vv") ||
               !strcmp(argv[i], "--more-verbose"))
      { // verbode level 2
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
        if (++i >= argc) usage();
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
        if (++i >= argc) usage();
        o->tau = atoi(argv[i]);
        if (o->tau < 0) o->tau = 0;
      }
      else if (!strcmp(argv[i], "-r") ||
               !strcmp(argv[i], "--real-time"))
      { // real time mode
        o->realtime = 1;
      }
      else
        usage();
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
static void sigint_handler(int signo)
{
  stimer_stop(&timer);
  fprintf(stderr, "\nCtrl-C pressed\n");
} 
//-----------------------------------------------------------------------------
static int timer_handler(void *context)
{
  // дергать ножку GPIO по прерыванию от таймера
  unsigned *counter = (unsigned*) context;
 
  printf("counter = %u\n", ++(*counter)); //!!! 
  
  // выводить статистику
  if (o.data)
  { // #
    //printf("");
  }

/*
  //uint32_t up_time;   // время активации сигнала
  //uint32_t down_time; // время деактивации сигнала


  // up GPIO pin
  if (!fake)
    sgpio_set(&gpio, !negative);

  // tau
  //...

  // down GPIO pin
  if (!fake)
    sgpio_set(&gpio, negative);

  if (0)
  {
    fprintf(stderr, "Error: ...; exit\n");
    exit(EXIT_FAILURE);
  }
*/

  return 0;
}
//-----------------------------------------------------------------------------
int main(int argc, const char *argv[])
{
  int retv;
  unsigned counter = 0; // interrupt counter
  uint32_t daytime = stimer_daytime();

  // разобрать опции командной строки
  parse_options(argc, argv, &o);

  // инициализировать GPIO
  sgpio_init(&gpio, o.gpio_num);

  if (!o.fake && 1) // unexport
  {
    retv = sgpio_unexport(o.gpio_num);
    if (o.verbose >= 3)
      printf(">>> sgpio_unexport(%d): '%s'\n",
             o.gpio_num, sgpio_error_str(retv));
  }

  if (!o.fake && 1) // export
  {
    retv = sgpio_export(o.gpio_num);
    if (o.verbose >= 3)
      printf(">>> sgpio_export(%d): '%s'\n",
             o.gpio_num, sgpio_error_str(retv));
  }
  
  if (!o.fake)
  { // устаовить режим вывода порта GPIO
    retv = sgpio_mode(&gpio, SGPIO_DIR_OUT, SGPIO_EDGE_NONE);
    if (o.verbose >= 3)
      printf(">>> sgpio_mode(%d,%d,%d): '%s'\n",
             sgpio_num(&gpio), SGPIO_DIR_OUT, SGPIO_EDGE_NONE,
             sgpio_error_str(retv));
   
    // установить начальный уровень сигнала на порте GPIO
    retv = sgpio_set(&gpio, o.negative);
    if (retv != SGPIO_ERR_NONE && o.verbose >= 3)
      printf(">>> sgpio_set(%i,%i): '%s'\n",
             sgpio_num(&gpio), o.negative,
             sgpio_error_str(retv));
  }

  // зарегистрировать обработчик сигнала SIGINT (CTRL+C)
  if (1)
  {
    struct sigaction sa;
    memset((void*) &sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    //sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
  }
  else
    signal(SIGINT, sigint_handler); // old school ;-)
  
  if (o.verbose >= 3)
    printf(">>> establishing handler for SIGINT (CTRL+C)\n");

  // вывести на консоль начальное время
  if (o.verbose >= 3)
  {
    printf(">>> local day time is ");
    stimer_fprint_daytime(stdout, daytime);
    printf("\n");
  }

  if (o.verbose >= 1)
  {
    printf("--> TICK run with next parameters:\n");
    printf("-->   interval [ms] = %i\n", o.interval);
    printf("-->   verbose level = %i\n", o.verbose);
    printf("-->   negative      = %i\n", o.negative);
    printf("-->   tau           = %i\n", o.tau);
    printf("-->   real time     = %s\n", o.realtime ? "yes" : "no");
  }

  // установить "real-time" приоритет
  if (o.realtime)
    stimer_realtime();

  // инициализировать таймер
  if (o.verbose >= 2)
    printf("->> call stimer_init()\n");
  retv = stimer_init(&timer, timer_handler, (void*) &counter);
  if (retv != 0)
  {
    perror("error: stimer_init() return fail; exit");
    exit(EXIT_FAILURE);
  }

  // запустить таймер
  if (o.verbose >= 2)
    printf("->> call stimer_start(%i)\n", o.interval);
  retv = stimer_start(&timer, (double) o.interval);
  if (retv != 0)
  {
    perror("error: stimer_start() return fail; exit");
    exit(EXIT_FAILURE);
  }

  // ждать сигнала и вызывать обработчик "вечно"
  retv = stimer_loop(&timer);
  if (o.verbose >= 2)
    printf("stimer_loop() return %i\n", retv);

  if (!o.fake && 1) // set to input (more safe mode)
  {
    retv = sgpio_mode(&gpio, SGPIO_DIR_IN, SGPIO_EDGE_NONE);
    printf(">>> sgpio_set_dir(%d,%d,%d): '%s'\n",
           sgpio_num(&gpio), SGPIO_DIR_IN, SGPIO_EDGE_NONE,
           sgpio_error_str(retv));
  }

  if (!o.fake && 1) // unexport
  {
    retv = sgpio_unexport(o.gpio_num);
    printf(">>> sgpio_unexport(%d): '%s'\n",
           o.gpio_num, sgpio_error_str(retv));
  }

  sgpio_free(&gpio);

  return EXIT_SUCCESS;
}
//-----------------------------------------------------------------------------

/*** end of "tick.c" ***/

