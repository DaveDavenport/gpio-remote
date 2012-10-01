/*
 * gpio_remote.h
 *
 */


#ifndef PWM_H
#define PWN_H

// struct for gpio_remote object
struct gpio_remote_data {
	uint32_t pin;
};


static int gpio_remote_setup_pin(uint32_t gpio_number);

#endif
