#include <linux/module.h>
#include <rtdm/rtdm_driver.h>

MODULE_LICENSE("GPL");

#define HT_PERIOD	1000000000 /* 1000 ms */

static rtdm_task_t ht_task;
static int end = 0;

/* Generic part: A simple periodic RTDM kernel space task */
void hello_task (void *cookie)
{
  static int n = 0;

  while (!end) {
    rtdm_task_wait_period();
    rtdm_printk ("hello_task %d\n", n++);
  }
}

int __init init_hello(void)
{
  return rtdm_task_init(&ht_task, "hello_task", hello_task, NULL, 99, HT_PERIOD);
}

void __exit cleanup_hello(void)
{
  end = 1;
  rtdm_task_join_nrt(&ht_task, 100);
}

module_init(init_hello);
module_exit(cleanup_hello);
