

#ifndef _LED_STS_H_
#define _LED_STS_H_

#include <zephyr/drivers/gpio.h>

#include <sys/errno.h>

int led_sts_init(void);
int led_sts_toggle(void);
int led_sts_set(bool state);


#endif // _LED_STS_H_
