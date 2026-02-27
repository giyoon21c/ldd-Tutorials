#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/gpio/consumer.h> // Modern gpiod API
#include <linux/gpio.h>          // For legacy to descriptor conversion


#define GPIO_PIN (512+23)

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


static ssize_t led_read(struct file *file, char __user *buf, size_t len, loff_t *off) {
  int val = gpiod_get_value(led_desc); // Get current state (0 or 1)
  char status = val ? '1' : '0';

  // If the user has already read the data, return 0 (EOF)
  if (*off > 0) return 0;

  // Send the status character to user-space
  if (copy_to_user(buf, &status, 1)) {
    return -EFAULT;
  }

  *off += 1; // Update offset so next read returns 0
  return 1;
}


static int led_release(struct inode *inode, struct file *file) {
  pr_info("LED Driver: Device closed\n");
  return 0;
}

static struct file_operations fops = {
  .owner = THIS_MODULE,
  .open = led_open,
  .write = led_write,
  .read = led_read,   
  .release = led_release,
};


static struct gpio_desc *led_desc;

// --- Init & Exit ---
static int __init led_init(void) {
  int ret;

  pr_info("LED Driver: Starting initialization for GPIO 23\n");

  // 1. Allocate Major/Minor numbers
  ret = alloc_chrdev_region(&dev_num, 0, 1, "my_led_driver");
  if (ret < 0) {
    pr_err("LED Driver: Failed to allocate major number\n");
    return ret;
  }

  // 2. Initialize and add the Character Device (cdev)
  cdev_init(&led_cdev, &fops);
  ret = cdev_add(&led_cdev, dev_num, 1);
  if (ret < 0) {
    pr_err("LED Driver: Failed to add cdev\n");
    goto fail_cdev;
  }

  // 3. Create the Sysfs Class (Visible in /sys/class)
  led_class = class_create("led_class");
  if (IS_ERR(led_class)) {
    ret = PTR_ERR(led_class);
    pr_err("LED Driver: Failed to create class\n");
    goto fail_class;
  }

  // 4. Create the Device Node (Visible in /dev/my_led)
  if (IS_ERR(device_create(led_class, NULL, dev_num, NULL, "my_led"))) {
    ret = -1;
    pr_err("LED Driver: Failed to create device node\n");
    goto fail_device;
  }

  // 5. Request GPIO 23
  // gpio_to_desc is a safe way to get the descriptor for BCM 23
  led_desc = gpio_to_desc(GPIO_PIN);
  if (!led_desc) {
    pr_err("LED Driver: GPIO 23 not found or busy (ENODEV)\n");
    ret = -ENODEV;
    goto fail_gpio;
  }

  // Set direction to output and initial value to 0 (OFF)
  ret = gpiod_direction_output(led_desc, 0);
  if (ret < 0) {
    pr_err("LED Driver: Could not claim GPIO 23 as output\n");
    goto fail_gpio;
  }

  pr_info("LED Driver: Successfully initialized (Major: %d)\n", MAJOR(dev_num));
  return 0; // Success

// --- THE ROLLBACK (The "Secret Sauce") ---
fail_gpio:
  device_destroy(led_class, dev_num);
fail_device:
  class_destroy(led_class);
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
