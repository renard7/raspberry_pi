/*
 * RTEMS GPIO driver for RPi
 */
#include <rtems.h>
#include "rpi_gpio.h"

#define BCM2708_PERI_BASE    0x20000000
#define GPIO_BASE            (BCM2708_PERI_BASE + 0x200000) /* GPIO controler */

// GPIO setup macros
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))

#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0
#define GPIO_READ(g) *(gpio+13) &= (1<<(g))

volatile unsigned int *gpio = (unsigned int *)GPIO_BASE;

static char initialized;

rtems_device_driver rpi_gpio_initialize(
  rtems_device_major_number major,
  rtems_device_minor_number minor,
  void *pargp
)
{
  rtems_device_driver status;

  if ( !initialized ) {
    initialized = 1;

    status = rtems_io_register_name(
      "/dev/rpi_gpio",
      major,
      (rtems_device_minor_number) 0
    );

    if (status != RTEMS_SUCCESSFUL)
      rtems_fatal_error_occurred(status);
  }

  return RTEMS_SUCCESSFUL;
}

rtems_device_driver rpi_gpio_open(
  rtems_device_major_number major,
  rtems_device_minor_number minor,
  void *pargp
)
{
  return RTEMS_SUCCESSFUL;
}

rtems_device_driver rpi_gpio_close(
  rtems_device_major_number major,
  rtems_device_minor_number minor,
  void *pargp
)
{
  return RTEMS_SUCCESSFUL;
}

rtems_device_driver rpi_gpio_control(
  rtems_device_major_number major,
  rtems_device_minor_number minor,
  void *pargp
)
{
  int n, cmd;
  rtems_libio_ioctl_args_t *args = pargp;

  n = (int)(args->buffer);
  cmd = (int)(args->command);
  switch (cmd) {
  case RPI_GPIO_SET : 
    GPIO_SET = 1 << n;
    break;

  case RPI_GPIO_CLR :
    GPIO_CLR = 1 << n;
    break;

  case RPI_GPIO_OUT :
    OUT_GPIO(n);
    break;

  case RPI_GPIO_IN :
    INP_GPIO(n);
    break;

  case RPI_GPIO_READ :
    args->ioctl_return = ((GPIO_READ(n) & (1 << n)) != 0);
    return RTEMS_SUCCESSFUL;

  default: 
    printk ("rpi_gpio_control: unknown cmd %x\n", cmd); 

    args->ioctl_return = -1;
    return RTEMS_UNSATISFIED;
  }
    
  args->ioctl_return = 0;

  return RTEMS_SUCCESSFUL;
}
