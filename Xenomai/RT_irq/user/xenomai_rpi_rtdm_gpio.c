/* 
 * Xenomai version of SQUARE example, POSIX skin, RTDM + IRQ 
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

pthread_t thid_square, thid_irq;

#define PERIOD          50000000 // 50 ms

unsigned long period_ns = 0;
int loop_prt = 100;             /* print every 100 loops: 5 s */ 
unsigned int test_loops = 0;    /* outer loop count */
int fd;
int clic = 0;

/* IRQ thread */
void *thread_irq (void *dummy)
{
  // Send ioctl command 2 to driver
  while (1) {
    if (rt_dev_ioctl (fd, 2, 0) < 0)
      fprintf (stderr, "rt_dev_ioctl error!\n");
    else {
      clic++;
      rt_printf ("*** got IRQ from driver!\n");
    }
  }
}

/* Periodic thread */
void *thread_square (void *dummy)
{
  int err, cmd, m = 1;
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
      fprintf(stderr,"failed to set periodic, code %d\n",err);
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
      cmd = (test_loops % (clic % 2 ? 4 : 2) ? 0 : 1);
	  
      if (rt_dev_ioctl(fd, cmd, 0) < 0)
	fprintf (stderr, "rt_dev_ioctl error!\n");

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
  rt_dev_close (fd);

  exit(0);
}

void usage (char *s)
{
  fprintf (stderr, "Usage: %s [-p period (ns)] [-r rtdm_driver_name]\n", s);
  exit (1);
}

int main (int ac, char **av)
{
  int err;
  char *cp, *progname = (char*)basename(av[0]), *rtdm_driver;
  struct sched_param param_square = {.sched_priority = 99 };
  struct sched_param param_irq = {.sched_priority = 98 };
  pthread_attr_t thattr_square, thattr_irq;

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
      case 'p' :
	period_ns = (unsigned long)atoi(*++av); break;

      case 'r' :
	rtdm_driver = *++av;
	break;

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
  
  printf ("Using driver \"%s\" and period %d ns\n", rtdm_driver, period_ns);

  // Avoid paging: MANDATORY for RT !!
  mlockall(MCL_CURRENT|MCL_FUTURE);

  // Init rt_printf() system
  rt_print_auto_init(1);

  // Open RTDM driver
  if ((fd = rt_dev_open(rtdm_driver, 0)) < 0) {
    perror("rt_open");
    exit(EXIT_FAILURE);
  }

  // Thread attributes
  pthread_attr_init(&thattr_square);

  // Joinable 
  pthread_attr_setdetachstate(&thattr_square, PTHREAD_CREATE_JOINABLE);

  // Priority, set priority to 99
  pthread_attr_setinheritsched(&thattr_square, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&thattr_square, SCHED_FIFO);
  pthread_attr_setschedparam(&thattr_square, &param_square);

  // Create thread(s)
  err = pthread_create(&thid_square, &thattr_square, &thread_square, NULL);

  if (err)
    {
      fprintf(stderr,"square: failed to create square thread, code %d\n",err);
      return 0;
    }

  // Thread attributes
  pthread_attr_init(&thattr_irq);

  // Priority, set priority to 98
  pthread_attr_setinheritsched(&thattr_irq, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&thattr_irq, SCHED_FIFO);
  pthread_attr_setschedparam(&thattr_irq, &param_irq);

  err = pthread_create(&thid_irq, &thattr_irq, &thread_irq, NULL);

  if (err)
    {
      fprintf(stderr,"square: failed to create IRQ thread, code %d\n",err);
      return 0;
    }

  pause();

  return 0;
}
