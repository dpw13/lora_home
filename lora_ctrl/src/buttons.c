#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <inttypes.h>

LOG_MODULE_REGISTER(buttons, LOG_LEVEL_INF);

static const struct gpio_dt_spec buttons[] = {
        GPIO_DT_SPEC_GET(DT_ALIAS(btn_gate), gpios),
        GPIO_DT_SPEC_GET(DT_ALIAS(btn_garage0), gpios),
        GPIO_DT_SPEC_GET(DT_ALIAS(btn_garage1), gpios),
};

int button_init(void)
{
	int ret;

        for (int i=0; i < ARRAY_SIZE(buttons); i++) {
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
