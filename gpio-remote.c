/**
 * Code based on pwm from Justin Griggs and Randy Simons
 * Code under the GPL-V2 license
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <asm/io.h>

#include "gpio-remote.h"

const unsigned long max_timing_offset = 50;

/* Default pin */
static int pin = 134;

/* Counter to keep track of open files. */
int Device_Open = 0;

/* Major number assigned to our device driver */
int Major = 0;

/* Dev name as it appears in /proc/devices   */
#define DEVICE_NAME "remotedev"


// do some kernel module documentation
MODULE_AUTHOR       ("Qball Cow <qball@qballcow.nl>");
MODULE_DESCRIPTION  ("OMAP GPIO Remote control");
MODULE_LICENSE      ("GPL");

/* Allow you to set GPIO pin */
module_param(pin, int, 0);
MODULE_PARM_DESC(pin, "Set GPIO pin used to send out signal");


// setup a GPIO pin for use
static int gpio_remote_setup_pin(uint32_t gpio_number) {

	int err;

	// see if that pin is available to use
	if (gpio_is_valid(gpio_number)) {

		printk("gpio_remote module: setting up gpio pin %i...",gpio_number);
		// allocate the GPIO pin
		err = gpio_request(gpio_number,"gpio_remoteIRQ");
		//error check
		if(err) {
			printk("gpio_remote module: failed to request GPIO %i\n",gpio_number);
			return -1;
		}

		// set as output
		err = gpio_direction_output(gpio_number,0);

		//error check
		if(err) {
			printk("gpio_remote module: failed to set GPIO to ouput\n");
			return -1;
		}

	}
	else
	{
		printk("gpio_remote module: requested GPIO is not valid\n");
		// return failure
		return -1;
	}

	// return success
	printk("DONE\n");
	return 0;
}



/*
 * Called when a process tries to open the device file, like
 * "cat /dev/mycharfile"
 */
static int device_open(struct inode *inode, struct file *file)
{
	if (Device_Open)
		return -EBUSY;

	Device_Open++;
	printk(KERN_ALERT "Opening file.\n");

	return 0;
}


/*
 * Called when a process closes the device file.
 */
static int device_release(struct inode *inode, struct file *file)
{
	Device_Open--;		/* We're now ready for our next caller */

	/*
	 * Decrement the usage count, or else once you opened the file, you'll
	 * never get get rid of the module.
	 */
	printk(KERN_ALERT "Closing file.\n");

	return 0;
}

/**
 * This loop is re-used with permission from Randy Simons.
 */
void sendTelegram(unsigned long data, unsigned short pin)
{
	unsigned int periodusec   = (unsigned long)data >> 23;
	unsigned int periodusecl  = periodusec-max_timing_offset;
	unsigned int periodusech  = periodusec+max_timing_offset;
    unsigned int periodusec3l = 3*periodusecl;
    unsigned int periodusec3h = 3*periodusech;

	unsigned short repeats    = 1 << (((unsigned long)data >> 20) & 7);
	unsigned short i;
	unsigned short j;

	//Convert the base3-code to base4, to avoid lengthy calculations when transmitting.. Messes op timings.
	unsigned long dataBase4 = 0;

	//truncate to 20 bit
	data = data & 0xFFFFF;


	for (i=0; i<12; i++) {
		dataBase4<<=2;
		dataBase4|=(data%3);
		data/=3;
	}

	for (j=0;j<repeats;j++) {
		//Sent one telegram
		//Use data-var as working var
		data=dataBase4;
		for (i=0; i<12; i++) {
			switch (data & 3) {
				case 0:
					gpio_set_value(pin, 1);
                    usleep_range(periodusecl, periodusech);

					gpio_set_value(pin, 0);
                    usleep_range(periodusec3l, periodusec3h);

					gpio_set_value(pin, 1);
                    usleep_range(periodusecl, periodusech);

					gpio_set_value(pin, 0);
                    usleep_range(periodusec3l, periodusec3h);
					break;
				case 1:
					gpio_set_value(pin, 1);
                    usleep_range(periodusec3l, periodusec3h);

					gpio_set_value(pin, 0);
                    usleep_range(periodusecl, periodusech);

					gpio_set_value(pin, 1);
                    usleep_range(periodusec3l, periodusec3h);

					gpio_set_value(pin, 0);
                    usleep_range(periodusecl, periodusech);
					break;
				case 2: //AKA: X or float
					gpio_set_value(pin, 1);
                    usleep_range(periodusecl, periodusech);

					gpio_set_value(pin, 0);
                    usleep_range(periodusec3l, periodusec3h);

					gpio_set_value(pin, 1);
                    usleep_range(periodusec3l, periodusec3h);

					gpio_set_value(pin, 0);
                    usleep_range(periodusecl, periodusech);
					break;
			}
			//Next trit
			data>>=2;
		}

		//Send termination/synchronisation-signal. Total length: 32 periods
		gpio_set_value(pin, 1);
        usleep_range(periodusecl, periodusech);
		gpio_set_value(pin, 0);
        usleep_range(periodusec*31-max_timing_offset, periodusec*31+max_timing_offset);
	}
}


/*
 * Called when a process writes to dev file: echo "hi" > /dev/hello
 */
static ssize_t
device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	int data;

	if(len != 4) return -1;

	data = *((int*)(buff));

	printk(KERN_INFO "Sending data: %u\n", data);

	sendTelegram(data, pin);

	return 4;
}

/*
 * Called when a process, which already opened the dev file, attempts to
 * read from it.
 */
static ssize_t device_read(struct file *filp,	/* see include/linux/fs.h   */
			   char *buffer,	/* buffer to fill with data */
			   size_t length,	/* length of the buffer     */
			   loff_t * offset)
{
    printk(KERN_ALERT "Reading from this device node is not supported.");
	return 0;
}

struct file_operations fops = {
	.read    = device_read,
	.write   = device_write,
	.open    = device_open,
	.release = device_release
};


static int __init gpio_remote_start(void)
{
        Major = register_chrdev(0, DEVICE_NAME, &fops);

	if (Major < 0) {
	  printk(KERN_ALERT "Registering char device failed with %d\n", Major);
	  return Major;
	}

	printk(KERN_INFO "Loading PWM Module... Major: %i\n", Major);

	// setup a GPIO
	gpio_remote_setup_pin(pin);

	gpio_set_value(pin, 0);
	// return success
	return 0;
}

static void __exit gpio_remote_end(void)
{
	printk(KERN_INFO "Exiting PWM Module. \n");


	// release GPIO
	gpio_free(pin);

	/*
	 * Unregister the device
	 */
	unregister_chrdev(Major, DEVICE_NAME);
}

// entry and exit points
module_init(gpio_remote_start);
module_exit(gpio_remote_end);
