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
#include <zephyr/logging/log.h>
#include "pwm.h"
#include "buttons.h"


LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void)
{
	int ret;

	ret = pwm_init();
	ret = button_init();

	LOG_INF("Running");

	//while (1) {
	//	k_msleep(1000);
	//	LOG_INF("Wake");
	//}

	return 0;
}
