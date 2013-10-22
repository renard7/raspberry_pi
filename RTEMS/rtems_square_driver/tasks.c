/*
 * Rate Monotonic Scheduling example for RPi
 *
 * pierre.ficheux@gmail.com
 *
 */
#include "system.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <rtems/error.h>
#include "rpi_gpio.h"

//volatile unsigned int *gpio = (unsigned int *)GPIO_BASE;
int fd;

// Periods for the various tasks
#define PERIOD_TASK_RATE_MONOTONIC     100

//
// Rate Monotonic Scheduling
//
rtems_task Task_Rate_Monotonic_Period (rtems_task_argument unused)
{
  rtems_status_code status;
  rtems_name        my_period_name; 
  rtems_id          RM_period;
  uint32_t          count;
  rtems_interval    period_interval;

  period_interval = rtems_clock_get_ticks_per_second() / PERIOD_TASK_RATE_MONOTONIC;
  count = 0;

  printf ("Period interval: %d tick(s)\n", (int)period_interval);

  if ((fd = open ("/dev/rpi_gpio", O_RDWR)) < 0) {
    fprintf (stderr, "open error => %d %s\n", errno, strerror(errno));
    exit (1);
  }

  ioctl(fd, RPI_GPIO_OUT, 16);

  // Init RMS
  my_period_name = rtems_build_name( 'P', 'E', 'R', '1' );
  status = rtems_rate_monotonic_create( my_period_name, &RM_period );
  if( RTEMS_SUCCESSFUL != status ) {
    printf("RM failed with status: %d\n", status);
    exit(1);
  }
  
  while( 1 ) {
    if (count % 2 == 0)
    	status = ioctl (fd, RPI_GPIO_SET, 16);
    else
     	status = ioctl (fd, RPI_GPIO_CLR, 16);

    count++;

    // Block until RM period has expired
    status = rtems_rate_monotonic_period (RM_period, period_interval);

    // Overrun ?
    if (status == RTEMS_TIMEOUT)
      printf ("RM missed period !\n");
  }
}
