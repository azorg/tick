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
    "Run:  tick [-options] [interval]\n"
    "Options:\n"
    "   -h|--help              show this help\n"
    "   -v|--verbose           verbose output\n"
    "  -vv|--more-verbose      more verbose output (or use -v twice)\n"
    "   -g|--gpio              number of GPIO channel\n"
    "   -n|--negative          negative output\n"
    "   -m|--meandr            meandr 2*T mode\n"
    "   -f|--fake              fake GPIO\n"
    "   -t|--tau               impulse time in 'bogoticks'\n"
    "   -r|--real-time         real time mode (root required)\n"
    "interval - timer interval in ms;\n"
    "By default interval is 100 ms, gpio is 1\n");
  exit(EXIT_SUCCESS);
}
//-----------------------------------------------------------------------------
// options
static int interval  = 100; // ms
static int gpio_num  = 1;   // 1 by default
static int verbose   = 1;   // verbose level {0,1,2,3}
static int negative  = 0;   // 0|1  
static int meandr    = 0;   // 0|1
static int fake      = 0;   // 0|1  
static int tau       = 0;   // >=0
static int realtime  = 0;   // 0|1
//-----------------------------------------------------------------------------
// global variable
int    stop_flag = 0; // set to 1 if Ctrl-C pressed
unsigned counter = 0; // interrupt counter
sgpio_t gpio;
stimer_t timer;
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
      else if (!strcmp(argv[i], "-m") ||
               !strcmp(argv[i], "--meandr"))
      { // meandr
        meandr = 1;
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
// SIGINT handler (Ctrl-C)
static void sigint_handler(int signo)
{
  stimer_stop(&timer);
} 
//-----------------------------------------------------------------------------
static int timer_handler(void *context)
{
  // дергать ножку GPIO по прерыванию от таймера
  unsigned *counter = (unsigned*) context;
 
  printf("counter = %u\n", *counter); //!!! 

/*
  //uint32_t up_time;   // время активации сигнала
  //uint32_t down_time; // время деактивации сигнала


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
*/

  (*counter)++;
  return 0;
}
//-----------------------------------------------------------------------------
int main(int argc, const char *argv[])
{
  int retv;
  uint32_t daytime = stimer_daytime();

  // разобрать опции командной строки
  parse_options(argc, argv);

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
    stimer_fprint_daytime(stdout, daytime);
    printf("\n");
  }

  if (verbose >= 1)
  {
    printf("TICK run with next parameters:\n");
    printf("  interval [ms] = %i\n", interval);
    printf("  verbose level = %i\n", verbose);
    printf("  negative      = %i\n", negative);
    printf("  tau           = %i\n", tau);
    printf("  real time     = %s\n", realtime ? "yes" : "no");
  }

  // установить "real-time" приоритет
  if (realtime)
    stimer_realtime();

  // инициализировать таймер
  if (verbose >= 2)
    printf("call stimer_init()\n");
  retv = stimer_init(&timer, timer_handler, (void*) &counter);
  if (retv != 0)
  {
    perror("error: stimer_init() return fail; exit");
    exit(EXIT_FAILURE);
  }

  // запустить таймер
  if (verbose >= 2)
    printf("call stimer_start(%i)\n", interval);
  retv = stimer_start(&timer, (double) interval);
  if (retv != 0)
  {
    perror("error: stimer_start() return fail; exit");
    exit(EXIT_FAILURE);
  }

  // ждать сигнала и вызывать обработчик "вечно"
  retv = stimer_loop(&timer);
  if (verbose >= 2)
    printf("stimer_loop() return %i\n", retv);

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

