#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 300

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

#define BUTTON_NODE DT_ALIAS(sw0)

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);

void main(void)
{
    printk("Hello World! %s\n", CONFIG_BOARD);

	int ret;

	if (!gpio_is_ready_dt(&led))
	{
		return;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0)
	{
		return;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT | GPIO_PULL_UP);
	if (ret < 0)
	{
		return;
	}

	while (1)
	{
		int button_value = gpio_pin_get_dt(&button);
		printk("Button value: %d\n", button_value);

		if (button_value != 0) {
			// ret = gpio_pin_toggle_dt(&led);
			ret = gpio_pin_set_dt(&led, 1);
		} else {
			ret = gpio_pin_set_dt(&led, 0);
		}
		if (ret < 0)
		{
			return;
		}
		// k_msleep(SLEEP_TIME_MS);
	}
}