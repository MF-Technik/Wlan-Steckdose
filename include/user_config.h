#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

//#define DEBUG

#define USER_PORT 		23

#define OUT_GPIO 			13
#define OUT_GPIO_FUNC 		FUNC_GPIO13
#define OUT_PERIPHS_MUX 	PERIPHS_IO_MUX_MTCK_U

#define LED_GPIO 			12
#define LED_GPIO_FUNC 		FUNC_GPIO12
#define LED_PERIPHS_MUX 	PERIPHS_IO_MUX_MTDI_U

#define TASTER_GPIO 		14
#define TASTER_GPIO_FUNC 	FUNC_GPIO14
#define TASTER_PERIPHS_MUX 	PERIPHS_IO_MUX_MTMS_U

uint8_t change_out(uint8_t status);

#endif
