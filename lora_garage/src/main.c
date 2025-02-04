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
#include <zephyr/drivers/lora.h>
#include <zephyr/lorawan/lorawan.h>

#include "fuota.h"
#include "relay.h"

LOG_MODULE_REGISTER(main, CONFIG_LORAWAN_SERVICES_LOG_LEVEL);

static const struct device *lora_dev = DEVICE_DT_GET(DT_ALIAS(lora0));

int lora_init(void) {
	if (!device_is_ready(lora_dev)) {
		LOG_ERR("%s: device not ready.", lora_dev->name);
		return -ENODEV;
	}

        return 0;
}

uint8_t remote_msg[8] = {0};

int main(void)
{
	int ret;

	ret = lora_init();
	if (ret != 0) {
		LOG_ERR("Failed to initialize LoRa");
		goto err;
	}

	fuota_run();

	/* Wait for datarate change*/
	wait_for_datarate(LORAWAN_DR_3);

	lorawan_relay_run();

	while (1) {
		k_sleep(K_MINUTES(5));
	}

err:
	return 0;
}
