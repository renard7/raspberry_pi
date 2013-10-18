/*
 * Rate Monotonic Scheduling example for RPi
 *
 * pierre.ficheux@gmail.com
 *
 */
#include "system.h"
#include <stdio.h>
#include <stdlib.h>
#include <rtems/error.h>

#define BCM2708_PERI_BASE    0x20000000
#define GPIO_BASE            (BCM2708_PERI_BASE + 0x200000) /* GPIO controler */

// GPIO setup macros
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))

#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0
#define GPIO_READ(g) *(gpio+13) &= (1<<(g))

#define GPIO_NR 25

volatile unsigned int *gpio = (unsigned int *)GPIO_BASE;

// Periods for the various tasks
#define PERIOD_TASK_RATE_MONOTONIC     2000

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
  OUT_GPIO(GPIO_NR);

  printf ("Period interval: %d tick(s)\n", (int)period_interval);

  // Init RMS
  my_period_name = rtems_build_name( 'P', 'E', 'R', '1' );
  status = rtems_rate_monotonic_create( my_period_name, &RM_period );
  if( RTEMS_SUCCESSFUL != status ) {
    printf("RM failed with status: %d\n", status);
    exit(1);
  }
  
  while( 1 ) {
    if (count % 2)
      GPIO_SET = 1 << GPIO_NR;
    else
      GPIO_CLR = 1 << GPIO_NR;

    count++;

    // Block until RM period has expired
    status = rtems_rate_monotonic_period (RM_period, period_interval);

    // Overrun ?
    if (status == RTEMS_TIMEOUT) {
      printf ("RM missed period !\n");
    }
  }
}
