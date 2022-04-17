/**
 * @file   ds1620.c
 * @author Alborz Jafari
 * @date   2 April 2020
 * @brief  A kernel module for controlling DS1620 using  GPIO.
 * The sysfs entry appears at /sys/sensors/ds1620
*/

#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kobject.h>

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Alborz Jafari");
MODULE_DESCRIPTION("A Linux driver LKM for the DS1620");
MODULE_VERSION("0.1");

static unsigned int clk_pin = 48;
static unsigned int dq_pin = 49;
static unsigned int rstb_pin = 115;
static char* temperature = "0.0"; //  -55 .. +125

static unsigned int Tclk = 1;     // CLK period in ms

module_param(clk_pin, uint, S_IRUGO);
module_param(dq_pin, uint, S_IRUGO);
module_param(rstb_pin, uint, S_IRUGO);
module_param(temperature, charp, S_IRUGO);

MODULE_PARM_DESC(clk_pin, " GPIO number assigned for CLK pin");
MODULE_PARM_DESC(dq_pin, " GPIO number assigned for DQ pin");
MODULE_PARM_DESC(rstb_pin, " GPIO number assigned for RST_bar pin");
MODULE_PARM_DESC(temperature, " Temperature");

static char ds1620_name[7] = "ds1620";

static void send_command(char command) {
  int i;

  // Data valid at rising edge
  // Set DQ pin as output and turn it on
  gpio_direction_output(dq_pin, true);

  for(i = 0; i < 8 ; ++i) {
    gpio_set_value(clk_pin, false);
    msleep(Tclk);
    gpio_set_value(dq_pin, command & 0x1);  // LSB First
    gpio_set_value(clk_pin, true);
    msleep(Tclk);

    command >>= 1;
  }
}

static void receive_data(char* data, unsigned int size) {
  int i;

  // Data valid at falling edge
  // Set DQ pin as input
  gpio_direction_input(dq_pin);

  *data = 0;
  for(i = size; i > 0; --i) {
    gpio_set_value(clk_pin, true);
    msleep(Tclk);
    gpio_set_value(clk_pin, false);
    msleep(Tclk);
    *data |= gpio_get_value(dq_pin) * (0x80>>(i-1));  // LSB first
  }
}

/** @brief A callback function to display the clk pin */
static ssize_t clk_pin_show(struct kobject *kobj, struct kobj_attribute *attr,
    char *buf) {
   return sprintf(buf, "%d\n", clk_pin);
}

/** @brief A callback function to display the dq pin */
static ssize_t dq_pin_show(struct kobject *kobj, struct kobj_attribute *attr,
    char *buf) {
   return sprintf(buf, "%d\n", dq_pin);
}

/** @brief A callback function to display the rstb pin */
static ssize_t rstb_pin_show(struct kobject *kobj, struct kobj_attribute *attr,
    char *buf) {
   return sprintf(buf, "%d\n", rstb_pin);
}

/** @brief A callback function to display the temperature */
static ssize_t temperature_show(struct kobject *kobj,
    struct kobj_attribute *attr, char *buf) {
  u8 sign = 0;
  s8 tempr_value = 0;
  u8 half_deg = 0;

  gpio_set_value(rstb_pin, true);
  send_command(0xaa);

  receive_data(&tempr_value, 8);
  receive_data(&sign, 1);

  half_deg = 5 * (tempr_value & 0x01);  // Half degree
  
  tempr_value >>= 1;

  if (sign > 0)
    tempr_value *= -1;

  gpio_set_value(rstb_pin, false);

  return sprintf(buf, "%d.%d\n", tempr_value, half_deg);
}

/** @brief A callback function to store the clk_pin value */
static ssize_t clk_pin_store(struct kobject *kobj, struct kobj_attribute *attr,
    const char *buf, size_t count) {
   sscanf(buf, "%du", &clk_pin);             // Read in the period as an unsigned int
   return clk_pin;
}

/** @brief A callback function to store the dq_pin value */
static ssize_t dq_pin_store(struct kobject *kobj, struct kobj_attribute *attr,
    const char *buf, size_t count) {
   sscanf(buf, "%du", &dq_pin);
   return dq_pin;
}

/** @brief A callback function to store the rstb_pin value */
static ssize_t rstb_pin_store(struct kobject *kobj, struct kobj_attribute *attr,
    const char *buf, size_t count) {
   sscanf(buf, "%du", &rstb_pin);
   return rstb_pin;
}

#undef VERIFY_OCTAL_PERMISSIONS
#define VERIFY_OCTAL_PERMISSIONS(perms) (perms)

static struct kobj_attribute clk_pin_attr =
    __ATTR(clk_pin, 0666, clk_pin_show, clk_pin_store);
static struct kobj_attribute dq_pin_attr =
    __ATTR(dq_pin, 0666, dq_pin_show, dq_pin_store);
static struct kobj_attribute rstb_pin_attr =
    __ATTR(rstb_pin, 0666, rstb_pin_show, rstb_pin_store);
static struct kobj_attribute temperature_attr =
    __ATTR(temperature, 0444, temperature_show, NULL);

static struct attribute *ds1620_attrs[] = {
   &clk_pin_attr.attr,
   &dq_pin_attr.attr,
   &rstb_pin_attr.attr,
   &temperature_attr.attr,
   NULL,
};

static struct attribute_group attr_group = {
   .name  = ds1620_name,
   .attrs = ds1620_attrs,
};

static struct kobject *ds1620_kobj;

static int __init ds1620_init(void) {
   int result = 0;

  ds1620_kobj = kobject_create_and_add("sensors", kernel_kobj->parent);
   if (!ds1620_kobj) {
      printk(KERN_ALERT "DS1620 Driver: failed to create kobject\n");
      return -ENOMEM;
   }

  result = sysfs_create_group(ds1620_kobj, &attr_group);
  if (result) {
     printk(KERN_ALERT "DS1620 Driver: failed to create sysfs group\n");
     kobject_put(ds1620_kobj);
     return result;
  }

  gpio_request(rstb_pin, "sysfs");
  
  // Set the rstb_pin in output mode and turn on
  gpio_direction_output(rstb_pin, true);
  
  gpio_request(clk_pin, "sysfs");
  gpio_direction_output(clk_pin, true);

  gpio_request(dq_pin, "sysfs");

  // Reset communication
  gpio_set_value(rstb_pin, false);    // RST_bar => Low
  gpio_set_value(clk_pin, true);    // RST_bar => High
  msleep(3);

  gpio_set_value(rstb_pin, true);
  msleep(3);
  send_command(0x0c);
  send_command(0x02);

  gpio_set_value(rstb_pin, false);
  msleep(1);
  gpio_set_value(rstb_pin, true);
  msleep(3);

  send_command(0xee);

  gpio_set_value(rstb_pin, false);
  gpio_set_value(clk_pin, false);
  msleep(1);

  return result;
}

static void __exit ds1620_exit(void) {
   kobject_put(ds1620_kobj);

   gpio_set_value(clk_pin, false);
   gpio_free(clk_pin);

   gpio_set_value(dq_pin, false);
   gpio_free(dq_pin);

   gpio_set_value(rstb_pin, false);
   gpio_free(rstb_pin);

   printk(KERN_INFO "DS1620 Driver: Unloading\n");
}

module_init(ds1620_init);
module_exit(ds1620_exit);
