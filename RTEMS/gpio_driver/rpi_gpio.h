#ifndef __RPI_GPIO_DRIVER_h
#define __RPI_GPIO_DRIVER_h

#include <rtems/libio.h>


/* Driver cmds */
#define RPI_GPIO_OUT     0
#define RPI_GPIO_IN      1
#define RPI_GPIO_SET     2
#define RPI_GPIO_CLR     3
#define RPI_GPIO_READ    4

#ifdef __cplusplus
extern "C" {
#endif

#define RPI_GPIO_DRIVER_TABLE_ENTRY \
  { rpi_gpio_initialize, rpi_gpio_open, rpi_gpio_close, NULL, \
    NULL, rpi_gpio_control }

rtems_device_driver rpi_gpio_initialize(
  rtems_device_major_number,
  rtems_device_minor_number,
  void *
);

rtems_device_driver rpi_gpio_open(
  rtems_device_major_number,
  rtems_device_minor_number,
  void *
);

rtems_device_driver rpi_gpio_close(
  rtems_device_major_number,
  rtems_device_minor_number,
  void *
);

rtems_device_driver rpi_gpio_control(
  rtems_device_major_number,
  rtems_device_minor_number,
  void *
);

#ifdef __cplusplus
}
#endif

#endif
/* end of include file */
