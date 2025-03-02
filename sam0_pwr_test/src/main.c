/*
 * LoRaWAN FUOTA sample application
 *
 * Copyright (c) 2022-2024 Libre Solar Technologies GmbH
 * Copyright (c) 2022-2024 tado GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/pm/policy.h>
#include <zephyr/logging/log.h>
#include "buttons.h"
#include "pm.h"

#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void)
{
	int ret;

	usb_enable(NULL);

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure LED pin");
	}

	ret = gpio_pin_set_dt(&led, 0);
	if (ret < 0) {
		LOG_ERR("Failed to turn off primary LED");
	}

	ret = button_init();

	LOG_INF("Running");

	while (1) {
		k_msleep(5000);
		LOG_INF("wakeup");

		pm_sysclock_force_active();
		k_msleep(5000);
		LOG_INF("wakeup2");

		pm_sysclock_allow_idle();
	}

	return 0;
}
