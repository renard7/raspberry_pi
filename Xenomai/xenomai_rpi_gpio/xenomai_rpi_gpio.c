/* 
 * Xenomai version of SQUARE example, POSIX skin
 */

#include <sys/mman.h>
#include <sys/time.h>
#include <sys/io.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>

#define BCM2708_PERI_BASE    0x20000000
#define GPIO_BASE            (BCM2708_PERI_BASE + 0x200000) /* GPIO controler */
#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

// I/O access
volatile unsigned *gpio;

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) 
// or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0

int  mem_fd;
char *gpio_map;

pthread_t thid_square;

#define PERIOD          50000000 // 50 ms
int nibl = 0;
int gpio_nr = 25; // default is GPIO #25

unsigned long period_ns = 0;
int loop_prt = 100;             /* print every 100 loops: 5 s */ 
unsigned int test_loops = 0;    /* outer loop count */

//
// Set up a memory regions to access GPIO
//
void setup_io()
{
  /* open /dev/mem */
  if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
    printf("can't open /dev/mem \n");
    exit(-1);
  }

  /* mmap GPIO */
  gpio_map = (char *)mmap(
			  NULL,             //Any adddress in our space will do
			  BLOCK_SIZE,       //Map length
			  PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
			  MAP_SHARED,       //Shared with other processes
			  mem_fd,           //File to map
			  GPIO_BASE         //Offset to GPIO peripheral
			  );

  close(mem_fd); //No need to keep mem_fd open after mmap

  if (gpio_map == MAP_FAILED) {
    printf("mmap error %d\n", (int)gpio_map);
    exit(-1);
  }

  // Always use volatile pointer!
  gpio = (volatile unsigned *)gpio_map;
}


/* Thread function*/
void *thread_square (void *dummy)
{
  int err;
  struct timespec start, period, t, told;

  /* Start a periodic task in 1s */
  clock_gettime(CLOCK_REALTIME, &start);
  start.tv_sec += 1;
  period.tv_sec = 0;
  period.tv_nsec = period_ns;
    
  // Make task periodic
  err = pthread_make_periodic_np(pthread_self(), &start, &period);

  if (err)
    {
      fprintf(stderr,"square: failed to set periodic, code %d\n",err);
      exit(EXIT_FAILURE);
    }

  /* Main loop */
  for (;;)
    {
      unsigned long overruns = 0;
	  
      test_loops++;

      /* Wait scheduler */
      err = pthread_wait_np(&overruns);

      if (err || overruns) {
	fprintf(stderr,"wait_period failed: err %d, overruns: %lu\n", err, overruns);
	exit(EXIT_FAILURE);
      }

      /* Write to GPIO */
      if (test_loops % 2)
	GPIO_SET = 1 << gpio_nr;
      else
	GPIO_CLR = 1 << gpio_nr;

      /* old_time <-- current_time */
      told.tv_sec = t.tv_sec;
      told.tv_nsec = t.tv_nsec;

      /* Get current time */
      clock_gettime (CLOCK_REALTIME, &t);

      /* Print if necessary */
      if ((test_loops % loop_prt) == 0)
	rt_printf ("Loop= %d dt= %d %d (%d ns)\n", test_loops, t.tv_sec - told.tv_sec, t.tv_nsec - told.tv_nsec, t.tv_nsec - told.tv_nsec - period_ns);
    }
}

void cleanup_upon_sig(int sig __attribute__((unused)))
{
  pthread_cancel (thid_square);
  pthread_join (thid_square, NULL);
  exit(0);
}

void usage (char *s)
{
  fprintf (stderr, "Usage: %s [-p period (ns)] [-g gpio#]\n", s);
  exit (1);
}

int main (int ac, char **av)
{
  int err;
  char *cp, *progname = (char*)basename(av[0]);
  struct sched_param param_square = {.sched_priority = 99 };
  pthread_attr_t thattr_square;

  period_ns = PERIOD; /* ns */

  signal(SIGINT, cleanup_upon_sig);
  signal(SIGTERM, cleanup_upon_sig);
  signal(SIGHUP, cleanup_upon_sig);
  signal(SIGALRM, cleanup_upon_sig);

  while (--ac) {
    if ((cp = *++av) == NULL)
      break;
    if (*cp == '-' && *++cp) {
      switch(*cp) {
      case 'g' :
	gpio_nr = atoi(*++av);
	break;

      case 'p' :
	period_ns = (unsigned long)atoi(*++av); break;

      default: 
	usage(progname);
	break;
      }
    }
    else
      break;
  }

  // Display every 2 sec
  loop_prt = 2000000000 / period_ns;
  
  printf ("Using GPIO %d and period %ld ns\n", gpio_nr, period_ns);

  // Avoid paging: MANDATORY for RT !!
  mlockall(MCL_CURRENT|MCL_FUTURE);

  // Init rt_printf() system
  rt_print_auto_init(1);

  // Set up gpi pointer for direct register access
  setup_io();

  // Set GPIO  as output
  //    INP_GPIO(gpio_nr); // must use INP_GPIO before we can use OUT_GPIO (?)
  OUT_GPIO(gpio_nr);

  // Thread attributes
  pthread_attr_init(&thattr_square);

  // Joinable 
  pthread_attr_setdetachstate(&thattr_square, PTHREAD_CREATE_JOINABLE);

  // Priority, set priority to 99
  pthread_attr_setinheritsched(&thattr_square, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&thattr_square, SCHED_FIFO);
  pthread_attr_setschedparam(&thattr_square, &param_square);

  // Create thread 
  err = pthread_create(&thid_square, &thattr_square, &thread_square, NULL);

  if (err)
    {
      fprintf(stderr,"square: failed to create square thread, code %d\n",err);
      return 0;
    }

  pause();

  return 0;
}
