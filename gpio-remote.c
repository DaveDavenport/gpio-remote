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
#include <asm/io.h>
#include <plat/dmtimer.h>
#include <linux/types.h>

#include "gpio-remote.h"


/* Default pin */
const int pin = 134;

/* pointer to data struct */
static struct gpio_remote_data gpio_remote_data_ptr;

/* Counter to keep track of open files. */
int Device_Open = 0;

/* Major number assigned to our device driver */
int Major = 0;

/* Dev name as it appears in /proc/devices   */
#define DEVICE_NAME "kakudev"


// do some kernel module documentation
MODULE_AUTHOR("Qball Cow <qball@qballcow.nl>");
MODULE_DESCRIPTION("OMAP KaKu GPIO");
MODULE_LICENSE("GPL");



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

		//add gpio data to struct
		gpio_remote_data_ptr.pin = gpio_number;
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


void sendTelegram(unsigned long data, unsigned short pin)
{
	unsigned int   periodusec = (unsigned long)data >> 23;
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
					udelay(periodusec);

					gpio_set_value(pin, 0);
					udelay(periodusec*3);

					gpio_set_value(pin, 1);
					udelay(periodusec);

					gpio_set_value(pin, 0);
					udelay(periodusec*3);
					break;
				case 1:
					gpio_set_value(pin, 1);
					udelay(periodusec*3);

					gpio_set_value(pin, 0);
					udelay(periodusec);

					gpio_set_value(pin, 1);
					udelay(periodusec*3);

					gpio_set_value(pin, 0);
					udelay(periodusec);
					break;
				case 2: //AKA: X or float
					gpio_set_value(pin, 1);
					udelay(periodusec);

					gpio_set_value(pin, 0);
					udelay(periodusec*3);

					gpio_set_value(pin, 1);
					udelay(periodusec*3);

					gpio_set_value(pin, 0);
					udelay(periodusec);
					break;
			}
			//Next trit
			data>>=2;
		}

		//Send termination/synchronisation-signal. Total length: 32 periods
		gpio_set_value(pin, 1);
		udelay(periodusec);
		gpio_set_value(pin, 0);
		udelay(periodusec*31);
	}
}


/*  
 * Called when a process writes to dev file: echo "hi" > /dev/hello 
 */
static ssize_t
device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	int value, data;
	printk(KERN_ALERT "Sorry, this operation isn't supported. %u\n",len);

	value = (int)buff;
	if(len != 4) return -1;
	
	data = *((int*)(buff));

	printk(KERN_ALERT "Fixing data: %u\n", data);
	
	//gpio_set_value(pin, (data != 0));
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
	
	gpio_remote_data_ptr.pin = pin;


	gpio_set_value(pin, 0);
	// return success
	return 0;
}

static void __exit gpio_remote_end(void)
{
	printk(KERN_INFO "Exiting PWM Module. \n");


	// release GPIO
	gpio_free(gpio_remote_data_ptr.pin);

	/* 
	 * Unregister the device 
	 */
	unregister_chrdev(Major, DEVICE_NAME);
}

// entry and exit points
module_init(gpio_remote_start);
module_exit(gpio_remote_end);
