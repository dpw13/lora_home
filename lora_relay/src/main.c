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

LOG_MODULE_REGISTER(main, CONFIG_LORAWAN_SERVICES_LOG_LEVEL);

static const struct device *lora_dev = DEVICE_DT_GET(DT_ALIAS(lora0));
static const struct lora_modem_config lora_cfg = {
	.frequency = 915000000,
	.bandwidth = BW_125_KHZ,
	.datarate = SF_6,
	.coding_rate = CR_4_8,
	.public_network = 0,
	.preamble_len = 100,
	.tx_power = 20,
};

int lora_init(void) {
	if (!device_is_ready(lora_dev)) {
		LOG_ERR("%s: device not ready.", lora_dev->name);
		return -ENODEV;
	}

        return 0;
}

int fuota_run(void);

uint8_t remote_msg[8] = {0};

int main(void)
{
	int ret;

	ret = lora_init();
	if (ret != 0) {
		LOG_ERR("Failed to initialize LoRa");
		goto err;
	}

	ret = lora_config(lora_dev, &lora_cfg);
	if (ret != 0) {
		LOG_ERR("Failed to configure LoRa");
		goto err;
	}

	if (ret == 0) {
		fuota_run();
	}

err:
	return 0;
}
