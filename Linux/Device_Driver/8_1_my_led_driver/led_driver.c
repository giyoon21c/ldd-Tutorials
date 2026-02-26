#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/gpio/consumer.h> // Modern gpiod API
#include <linux/gpio.h>          // For legacy to descriptor conversion


#define GPIO_PIN 23

static dev_t dev_num;
static struct cdev led_cdev;
static struct class *led_class;
static struct gpio_desc *led_desc;

// file operations
static int led_open(struct inode *inode, struct file *file) {
  pr_info("LED Driver: Device opened\n");
  return 0;
}


static ssize_t led_write(struct file *file, 
		         const char __user *buf, 
			 size_t len,
			 loff_t *off) {

  char k_buf;

  if (copy_from_user(&k_buf, buf, 1)) {
    return -EFAULT;
  }

  if (k_buf == '1') {
    gpiod_set_value(led_desc, 1); // turn on
    pr_info("LED Driver: LED ON\n");
  } else if (k_buf == '0') {
    gpiod_set_value(led_desc, 0); // turn off
    pr_info("LED Driver: LED OFF\n");
  }

  return len;
}


static int led_release(struct inode *inode, struct file *file) {
  pr_info("LED Driver: Device closed\n");
  return 0;
}

static struct file_operations fops = {
  .owner = THIS_MODULE,
  .open = led_open,
  .write = led_write,
  .release = led_release,
};


static struct gpio_desc *led_desc;

// --- Init & Exit ---

static int __init led_init(void) {

  int ret;

  // 1. Allocate Major/Minor
  ret = alloc_chrdev_region(&dev_num, 0, 1, "my_led_driver");
  if (ret < 0) return ret;

  // 2. Setup cdev
  cdev_init(&led_cdev, &fops);
  if ((ret = cdev_add(&led_cdev, dev_num, 1)) < 0) goto fail_cdev;

  // 3. Create class 
  led_class = class_create("led_class"); // was failing
  if (IS_ERR(led_class)) {
    ret = PTR_ERR(led_class);
    goto fail_class;
  }

  // 4. Create device
  if (IS_ERR(device_create(led_class, NULL, dev_num, NULL, "my_led"))) {
    goto fail_device;
  }

  // 5. Request GPIO (The part that caused your "No such device" error)
  led_desc = gpio_to_desc(23);
  if (!led_desc) {
    pr_err("LED Driver: Failed to get descriptor for 23. Is it busy?\n");
    return -ENODEV;
  }

  // "Request" the GPIO to ensure the kernel marks it as OURS
  if (gpiod_direction_output(led_desc, 0)) {
    pr_err("LED Driver: Could not claim GPIO 23 as output\n");
    return -EBUSY; // Pin is busy!
  }

  return 0; // Success!

// --- ROLLBACK LABELS ---
//fail_gpio:
//    device_destroy(led_class, dev_num);
//fail_device:
//    class_destroy(led_class); // This cleans up the duplicate filename issue!
fail_class:
    cdev_del(&led_cdev);
fail_cdev:
    unregister_chrdev_region(dev_num, 1);
    return ret;
}

static void __exit led_exit(void) {
  gpiod_set_value(led_desc, 0);
  device_destroy(led_class, dev_num);
  class_destroy(led_class);
  cdev_del(&led_cdev);
  unregister_chrdev_region(dev_num, 1);
  pr_info("LED Driver: Exited\n");
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Senior Tester");
MODULE_DESCRIPTION("Simple RPi LED Driver");
