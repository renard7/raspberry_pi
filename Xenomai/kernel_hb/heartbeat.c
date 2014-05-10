/*
 * Simple RTDM demo that generates a running light on your PC keyboard
 * or on a BF537-STAMP board.
 * Copyright (C) 2005-2007 Jan Kiszka <jan.kiszka@web.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with rtaisec-reloaded; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <linux/module.h>
#include <rtdm/rtdm_driver.h>

MODULE_LICENSE("GPL");

#define HEARTBEAT_PERIOD	100000000 /* 100 ms */

static rtdm_task_t heartbeat_task;
static int end = 0;


/* Architecture specific code follows... */
#ifdef CONFIG_X86

static void leds_set(int state)
{
	const int led_state[] = { 0x00, 0x01, 0x05, 0x07, 0x06, 0x02, 0x00 };

	while (inb(0x64) & 2);
	outb(0xED, 0x60);
	while (!(inb(0x64) & 1));
	inb(0x60);
	while (inb(0x64) & 2);
	outb(led_state[state % 7], 0x60);
	while (!(inb(0x64) & 1));
	inb(0x60);
}

static int leds_init(void)
{
	return 0;
}

static void leds_cleanup(void)
{
	leds_set(0);
}

#elif defined(CONFIG_BFIN537_STAMP)

#include <asm/gpio.h>

static void leds_set(int state)
{
	state = state % 13;

	/* Note: The I-pipe patch for blackfin ensures that gpio_set_value
	 * (among other services) can safely be called from RT context. */
	if (state <= 5)
		gpio_set_value(GPIO_PF6 + state, 1);
	else if (state <= 11)
		gpio_set_value(GPIO_PF6 + state-6, 0);
}

static int leds_init(void)
{
	int i;

	for (i = GPIO_PF6; i <= GPIO_PF11; i++) {
		if (gpio_request(i, "heartbeat") < 0) {
			while (--i > GPIO_PF6)
				gpio_free(i);
			return -EBUSY;
		}
		gpio_direction_output(i);
	}
	return 0;
}

static void leds_cleanup(void)
{
	int i;

	for (i = GPIO_PF6; i <= GPIO_PF11; i++)
		gpio_free(i);
}

#elif defined(CONFIG_RPI)

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
static int gpio_nr = 16; /* RPi led */

module_param(debug, int, 0644);
module_param(gpio_nr, int, 0644);

static inline void leds_set(int state) 
{ 
  if (state % 2)
    GPIO_SET(virt_addr) = (1 << gpio_nr);
  else
    GPIO_CLR(virt_addr) = (1 << gpio_nr);
}

static inline int leds_init(void) 
{ 
  // Map GPIO addr
  if ((virt_addr = ioremap (GPIO_BASE, PAGE_SIZE)) == NULL) {
    printk(KERN_ERR "Can't map GPIO addr !\n");
    return -1;
  }
  else
    printk(KERN_INFO "GPIO mapped to 0x%08x\n", (unsigned int)virt_addr);

  OUT_GPIO(virt_addr, gpio_nr);

  return 0; 
}

static inline void leds_cleanup(void) 
{ 
  // Unmap addr
  iounmap (virt_addr);
}

#else

#warning Sorry, no lights on this hardware :(

static inline void leds_set(int state) { }
static inline int leds_init(void) { return 0; }
static inline void leds_cleanup(void) { }

#endif

/* Generic part: A simple periodic RTDM kernel space task */
void heartbeat(void *cookie)
{
	int state = 0;

	while (!end) {
		rtdm_task_wait_period();

		leds_set(state++);
	}
}

int __init init_heartbeat(void)
{
	int err;

	err = leds_init();
	if (err)
		return err;

	return rtdm_task_init(&heartbeat_task, "heartbeat", heartbeat, NULL,
			      99, HEARTBEAT_PERIOD);
}

void __exit cleanup_heartbeat(void)
{
	end = 1;
	rtdm_task_join_nrt(&heartbeat_task, 100);
	leds_cleanup();
}

module_init(init_heartbeat);
module_exit(cleanup_heartbeat);
