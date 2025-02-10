#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <inttypes.h>

#include "buttons.h"

LOG_MODULE_REGISTER(buttons, LOG_LEVEL_INF);

static const struct gpio_dt_spec buttons[] = {
        GPIO_DT_SPEC_GET(DT_ALIAS(btn0), gpios),
};

static struct gpio_callback gpio_callback;

static void button_irq_callback(const struct device *dev,
				struct gpio_callback *cb, uint32_t pins)
{
	LOG_DBG("Got button press");
}

int button_init(void)
{
	int ret;
	int i;
	gpio_port_pins_t mask = 0;

        for (i=0; i < ARRAY_SIZE(buttons); i++) {
		if (i > 0 && buttons[i].port != buttons[0].port) {
			LOG_ERR("All buttons must be on the same port!");
			return -EINVAL;
		}

		if (!gpio_is_ready_dt(&buttons[i])) {
			LOG_ERR("button device %s is not ready",
			buttons[i].port->name);
			return -ENOENT;
		}

		ret = gpio_pin_configure_dt(&buttons[i], GPIO_INPUT);
		if (ret != 0) {
			LOG_ERR("failed to configure %s pin %d: %d",
			buttons[i].port->name, buttons[i].pin, ret);
			return ret;
		}

		ret = gpio_pin_interrupt_configure_dt(&buttons[i], GPIO_INT_EDGE_BOTH);
		if (ret != 0) {
			LOG_ERR("failed to configure interrupt on %s pin %d: %d",
			buttons[i].port->name, buttons[i].pin, ret);
			return ret;
		}

		mask |= BIT(buttons[i].pin);
	}

	LOG_DBG("Init GPIO callback with mask %08x", mask);
	gpio_init_callback(&gpio_callback,
			button_irq_callback,
			mask);

	ret = gpio_add_callback(buttons[0].port, &gpio_callback);
	if (ret < 0) {
		LOG_ERR("Could not set gpio callback.");
		return -EINVAL;
	}

	return 0;
}

uint8_t button_poll(void) {
	uint8_t val = 0;

	for (int i=0; i < ARRAY_SIZE(buttons); i++) {
		val = (val << 1) | (uint8_t)gpio_pin_get_dt(&buttons[i]);
	}

	return val;
}
