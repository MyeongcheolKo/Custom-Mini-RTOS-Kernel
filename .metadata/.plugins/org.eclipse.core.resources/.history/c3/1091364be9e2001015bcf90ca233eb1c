/*
 * GPIO_driver.h
 *
 *  Created on: Dec 26, 2025
 *      Author: krisko
 */

#ifndef DRIVERS_GPIO_DRIVER_H_
#define DRIVERS_GPIO_DRIVER_H_

#include "STM32F446xx.h"

/*
 * GPIO pin configuration structure
 */
typedef struct{
	uint8_t GPIO_pin_num;
	uint8_t GPIO_pin_mode;
	uint8_t GPIO_pin_speed;
	uint8_t GPIO_pin_pupd_control;
	uint8_t GPIO_pin_out_type;
	uint8_t GPIO_pin_alt_fcn_mode;
}GPIO_pin_config_t;

/*
 * Handle structure for a GPIO pin
 */
typedef struct{
	GPIO_reg_t *p_GPIOx; 			// the specific GPIO port base address specified by the user
	GPIO_pin_config_t GPIO_config; 	// the GPIO configuration settings specified by user
}GPIO_Handle_t;


/*                     API                   */


//clock setup
void GPIO_clock_control(GPIO_reg_t *p_GPIOx, uint8_t enable);

//initialize and disable
void GPIO_init(GPIO_Handle_t *p_GPIO_Handle);
void GPIO_disable(GPIO_reg_t *p_GPIOx); // only need to set reset register

//read and write
uint8_t GPIO_read_input_pin(GPIO_reg_t *p_GPIOx, uint8_t pin_num);
uint16_t GPIO_read_input_port(GPIO_reg_t *p_GPIOx);
void GPIO_write_output_pin(GPIO_reg_t *p_GPIOx, uint8_t pin_num, uint8_t val);
void GPIO_write_output_port(GPIO_reg_t *p_GPIOx, uint16_t val);
void GPIO_toggle_output_pin(GPIO_reg_t *p_GPIOx, uint8_t pin_num);

//IQR configuration and handling
void GPIO_IRQ_config(uint8_t IRQ_num, uint8_t IRQ_priority, uint8_t enable);
void GPIO_IRQ_handler(uint8_t pin_num);


#endif /* DRIVERS_GPIO_DRIVER_H_ */
