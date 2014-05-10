/*
 * GPIO RTDM driver example for Raspberry Pi
 */
#include <linux/module.h>
#include <rtdm/rtdm_driver.h>
#include <linux/types.h>

#define RTDM_SUBCLASS_RPI_GPIO       0
#define DEVICE_NAME                 "rpi_gpio"

MODULE_DESCRIPTION("RTDM driver for RPI GPIO");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pierre Ficheux");

#define BCM2708_PERI_BASE        0x20000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controler */

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) 
// or SET_GPIO_ALT(x,y)
#define INP_GPIO(addr,g) *((addr)+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(addr,g) *((addr)+((g)/10)) |=  (1<<(((g)%10)*3))

#define GPIO_SET(gpio) *((gpio)+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR(gpio) *((gpio)+10) // clears bits which are 1 ignores bits which are 0

unsigned long *virt_addr;

static int debug;
static int gpio_nr = 25;

module_param(debug, int, 0644);
module_param(gpio_nr, int, 0644);

struct rpi_gpio_context {
  unsigned long *gpio_addr;
  int gpio_nr;
};

int rpi_gpio_open(struct rtdm_dev_context *context, rtdm_user_info_t *user_info, int oflags)
{
  struct rpi_gpio_context *ctx;
  
  ctx = (struct rpi_gpio_context *) context->dev_private;
  ctx->gpio_addr = virt_addr;
  ctx->gpio_nr = gpio_nr;

  OUT_GPIO(ctx->gpio_addr, ctx->gpio_nr);

  return 0;
}

int rpi_gpio_close(struct rtdm_dev_context *context, rtdm_user_info_t *user_info)
{
  return 0;
}

static ssize_t rpi_gpio_ioctl_rt(struct rtdm_dev_context* context, rtdm_user_info_t* user_info, unsigned int request, void __user* arg)
{
  struct rpi_gpio_context *ctx = (struct rpi_gpio_context *) context->dev_private;
  
  // Set or Clear
  if (request == 0)
    GPIO_SET(ctx->gpio_addr) = (1 << ctx->gpio_nr);
  else
    GPIO_CLR(ctx->gpio_addr) = (1 << ctx->gpio_nr);

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
  rtdm_printk("RPI_GPIO RTDM, loading\n");

  // Map GPIO addr
  if ((virt_addr = ioremap (GPIO_BASE, PAGE_SIZE)) == NULL) {
    printk(KERN_ERR "Can't map GPIO addr !\n");
    return -1;
  }
  else
    printk(KERN_INFO "GPIO mapped to 0x%08x\n", (unsigned int)virt_addr);

  return rtdm_dev_register (&device); 
}

void __exit rpi_gpio_exit(void)
{
  rtdm_printk("RPI_GPIO RTDM, unloading\n");

  // Unmap addr
  iounmap (virt_addr);
  
  rtdm_dev_unregister (&device, 1000);
}

module_init(rpi_gpio_init);
module_exit(rpi_gpio_exit); 

