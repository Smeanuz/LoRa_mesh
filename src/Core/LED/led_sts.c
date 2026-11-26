

#include "led_sts.h"
#include "zephyr/drivers/gpio.h"

#include <sys/errno.h>
#include <zephyr/kernel.h>


#define LED_STS_NODE DT_ALIAS(led0)

const struct gpio_dt_spec led_sts_dev = GPIO_DT_SPEC_GET(LED_STS_NODE, gpios);

int led_sts_init(void)
{
    if (!gpio_is_ready_dt(&led_sts_dev)) {
        return -ENODEV;
    }

    return gpio_pin_configure_dt(&led_sts_dev, GPIO_OUTPUT_INACTIVE);
}

int led_sts_toggle(void)
{
    return gpio_pin_toggle_dt(&led_sts_dev);
}

int led_sts_set(bool state)
{
    return gpio_pin_set_dt(&led_sts_dev, state);
}


