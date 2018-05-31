#ifndef _CUSTOMER_H_
#define _CUSTOMER_H_

#include <linux/kernel.h>
extern int sec_scl;
extern int sec_sda;
#define SCL_H   do { \
					_gpio_set_data(sec_scl, 1);\
				} while(0)
#define SCL_L   do { \
					_gpio_set_data(sec_scl, 0); \
				}while(0)
#define SDA_H   do { \
					_gpio_get_data(sec_sda);\
				}while(0)				
#define SDA_L   do { \
					_gpio_set_data(sec_sda, 0);\
				}while(0)
#define SDA_R   _gpio_get_data(sec_sda)

extern unsigned char MTZ_test(unsigned char mtz1, unsigned char mtz2);
extern unsigned char cw0881_demo(void);
#endif