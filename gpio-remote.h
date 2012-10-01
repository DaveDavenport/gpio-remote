/*
 * pwm.h
 *
 */


#ifndef PWM_H
#define PWN_H

// struct for pwm object
struct pwm_data {
	uint32_t pin;
};


static int pwm_setup_pin(uint32_t gpio_number);

#endif
