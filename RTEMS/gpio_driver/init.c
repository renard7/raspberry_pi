/*
 * RPi GPIO test example
 * blink ACT led (GPIO 16) + receive input from GPIO 24
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#include "rpi_gpio.h"

int fd;
int gpio_input;

#define G_IN    24
#define G_OUT   25

void got_signal (int sig)
{
  static int n = 0;
  int status, input;

  if (n % 2 == 0)
    status = ioctl (fd, RPI_GPIO_SET, G_OUT);
  else
    status = ioctl (fd, RPI_GPIO_CLR, G_OUT);

  if ((input = ioctl (fd, RPI_GPIO_READ, G_IN)) != gpio_input) {
    printf ("input= %d\n", ioctl (fd, RPI_GPIO_READ, G_IN));
    gpio_input = input;
  }

  n++;

  if (status)
    fprintf (stderr, "status= %d errno= %d => %s\n", status, errno, strerror(errno));
}

void *POSIX_Init() 
{
  timer_t myTimer;
  struct sigaction sig;
  struct itimerspec ti, ti_old;
  struct sigevent event;
  sigset_t mask;

  puts( "\n\n*** RPi GPIO driver test ***" );

  if ((fd = open ("/dev/rpi_gpio", O_RDWR)) < 0) {
    fprintf (stderr, "open error => %d %s\n", errno, strerror(errno));
    exit (1);
  }

  printf ("fd = %d\n", fd);

  ioctl(fd, RPI_GPIO_IN, G_IN);
  ioctl(fd, RPI_GPIO_OUT, G_OUT);

  gpio_input = ioctl (fd, RPI_GPIO_READ, G_IN);

  // Set up signal
  sig.sa_flags = 0;
  sig.sa_handler = got_signal;
  sigemptyset (&sig.sa_mask);
  sigaction (SIGALRM, &sig, NULL);
  sigemptyset (&mask);
  sigaddset (&mask, SIGALRM);
  sigprocmask (SIG_UNBLOCK, &mask, NULL);

  event.sigev_notify = SIGEV_SIGNAL;
  event.sigev_value.sival_int = 0;
  event.sigev_signo = SIGALRM;

  // Start timer
  timer_create (CLOCK_REALTIME, &event, &myTimer);

  ti.it_value.tv_sec = 0;
  ti.it_value.tv_nsec = 5000000;
  ti.it_interval.tv_sec = 0;
  ti.it_interval.tv_nsec = 10000000;

  timer_settime(myTimer, 0, &ti, &ti_old);

  while (1)
    pause ();
}


/* configuration information */

#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_EXTRA_DRIVERS RPI_GPIO_DRIVER_TABLE_ENTRY

#define CONFIGURE_LIBIO_MAXIMUM_FILE_DESCRIPTORS 5

#define CONFIGURE_MAXIMUM_POSIX_TIMERS          1
#define CONFIGURE_MAXIMUM_POSIX_THREADS		1

#define CONFIGURE_EXTRA_TASK_STACKS         (6 * RTEMS_MINIMUM_STACK_SIZE)

#define CONFIGURE_POSIX_INIT_THREAD_TABLE

#define CONFIGURE_INIT
#include <rtems/confdefs.h>
/* end of file */
