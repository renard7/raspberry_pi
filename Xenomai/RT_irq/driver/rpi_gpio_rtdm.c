/*
 * GPIO RTDM driver example for Raspberry Pi + IRQ handling
 */
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/types.h>
#include <rtdm/rtdm_driver.h>

#define RTDM_SUBCLASS_RPI_GPIO       0
#define DEVICE_NAME                 "rpi_gpio"

MODULE_DESCRIPTION("RTDM driver for RPI GPIO");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pierre Ficheux");

#define BCM2708_PERI_BASE    0x20000000
#define GPIO_BASE            (BCM2708_PERI_BASE + 0x200000) /* GPIO controler */

// GPIO access macros
#define GPIO_SET(gpio) *((gpio)+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR(gpio) *((gpio)+10) // clears bits which are 1 ignores bits which are 0

unsigned long *virt_addr;

static int gpio_nr = 25;
static int gpio_irq_nr = 24;
static rtdm_irq_t irq_handle;
static rtdm_event_t gpio_event;

module_param(gpio_nr, int, 0644);
module_param(gpio_irq_nr, int, 0644);

struct rpi_gpio_context {
  unsigned long *gpio_addr;
  int gpio_nr;
  int gpio_irq_nr;
};

int irq_handler(rtdm_irq_t *irq_handle)
{
  // We got IRQ !
  rtdm_event_signal (&gpio_event);

  return RTDM_IRQ_HANDLED;
}

int rpi_gpio_open(struct rtdm_dev_context *context, rtdm_user_info_t *user_info, int oflags)
{
  struct rpi_gpio_context *ctx;

  ctx = (struct rpi_gpio_context *) context->dev_private;
  ctx->gpio_addr = virt_addr;
  ctx->gpio_nr = gpio_nr;
  ctx->gpio_irq_nr = gpio_irq_nr;

  return 0;
}

int rpi_gpio_close(struct rtdm_dev_context *context, rtdm_user_info_t *user_info)
{
  return 0;
}

static ssize_t rpi_gpio_ioctl_rt(struct rtdm_dev_context* context, rtdm_user_info_t* user_info, unsigned int request, void __user* arg)
{
  struct rpi_gpio_context *ctx = (struct rpi_gpio_context *) context->dev_private;
  int err;
  
  switch (request) {
  case 0 :
    GPIO_SET(ctx->gpio_addr) = (1 << ctx->gpio_nr);
    break;

  case 1 :
    GPIO_CLR(ctx->gpio_addr) = (1 << ctx->gpio_nr);
    break;

  case 2 : // IRQ
    // Wait IRQ
    if ((err = rtdm_event_wait (&gpio_event)) < 0)
      return err;
    else
      break;

  default:
    rtdm_printk ("ioctl: invalid value %d\n", request);
    return -1;
  }

  return 0;
}

static struct rtdm_device device = {
 struct_version:         RTDM_DEVICE_STRUCT_VER,
 
 device_flags:           RTDM_NAMED_DEVICE,
 context_size:           sizeof(struct rpi_gpio_context),
 device_name:            DEVICE_NAME,

 open_rt:                NULL,
 open_nrt:               rpi_gpio_open,

 ops:{
  close_rt:       NULL,
  close_nrt:      rpi_gpio_close,
  
  ioctl_rt:       rpi_gpio_ioctl_rt,
  ioctl_nrt:      NULL,
  
  read_rt:        NULL,
  read_nrt:       NULL,

  write_rt:       NULL,
  write_nrt:      NULL,   
  },

 device_class:           RTDM_CLASS_EXPERIMENTAL,
 device_sub_class:       RTDM_SUBCLASS_RPI_GPIO,
 driver_name:            "rpi_gpio_rtdm",
 driver_version:         RTDM_DRIVER_VER(0, 0, 0),
 peripheral_name:        "RPI_GPIO RTDM",
 provider_name:          "PF",
 proc_name:              device.device_name,
};


int __init rpi_gpio_init(void)
{
  int err;

  rtdm_printk("RPI_GPIO RTDM, loading\n");

  // Map GPIO addr
  if ((virt_addr = ioremap (GPIO_BASE, PAGE_SIZE)) == NULL) {
    printk(KERN_ERR "Can't map GPIO addr !\n");
    return -1;
  }
  else
    printk(KERN_INFO "GPIO mapped to 0x%08x\n", (unsigned int)virt_addr);

  if ((err = gpio_request(gpio_irq_nr, THIS_MODULE->name)) != 0)
    return err;

  if ((err = gpio_direction_input(gpio_irq_nr)) != 0) {
    gpio_free(gpio_irq_nr);
    return err;
  }

  // led (#16) is already used => free it !
  if (gpio_nr == 16)
    gpio_free(gpio_nr);

  if ((err = gpio_request(gpio_nr, THIS_MODULE->name)) != 0) {
    rtdm_printk ("gpio_request error %d\n", err);
    return err;
  }

  if ((err = gpio_direction_output(gpio_nr, 0)) != 0) {
    gpio_free(gpio_nr);
    rtdm_printk ("gpio_direction_output error %d\n", err);
    return err;
  }

  // Set IRQ
  irq_set_irq_type(gpio_to_irq(gpio_irq_nr), IRQF_TRIGGER_RISING);

  err = rtdm_irq_request(&irq_handle, gpio_to_irq(gpio_irq_nr), irq_handler, RTDM_IRQTYPE_EDGE, "myirq", THIS_MODULE->name);
  if (err < 0)
    printk(KERN_WARNING "unable to register irq handler\n");
  else
    printk(KERN_WARNING "irq handler installed\n");

  rtdm_irq_enable(& irq_handle);

  rtdm_event_init(&gpio_event, 0);

  return rtdm_dev_register (&device); 
}

void __exit rpi_gpio_exit(void)
{
  rtdm_printk("RPI_GPIO RTDM, unloading\n");

  rtdm_irq_disable(& irq_handle);

  rtdm_irq_free(&irq_handle);

  gpio_free(gpio_nr);
  gpio_free(gpio_irq_nr);

  // Unmap addr
  iounmap (virt_addr);
  
  rtdm_dev_unregister (&device, 1000);
}

module_init(rpi_gpio_init);
module_exit(rpi_gpio_exit); 

