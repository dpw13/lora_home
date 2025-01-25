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

#include "renogy.h"

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

	/* TODO: save address in settings */
	ret = init_charger();
	if (ret != 0) {
		LOG_ERR("Failed to find charger");
		goto err;
	}

	fuota_run();

	uint8_t buf[256];
	ret = charger_get_system((struct renogy_sys_t *)buf);
	if (ret != 0) {
		LOG_ERR("Failed to get charger system data");
	} else {
		ret = lorawan_send(0x10, buf, sizeof(struct renogy_sys_t), LORAWAN_MSG_UNCONFIRMED);
		if (ret < 0) {
			LOG_ERR("lorawan_send failure: %d", ret);
		}
	}

	ret = charger_get_bat_info((struct renogy_param_bat_t *)buf);
	if (ret != 0) {
		LOG_ERR("Failed to get charger battery data");
	} else {
		ret = lorawan_send(0x11, buf, sizeof(struct renogy_param_bat_t), LORAWAN_MSG_UNCONFIRMED);
		if (ret < 0) {
			LOG_ERR("lorawan_send failure: %d", ret);
		}
	}

	while (1) {
		ret = charger_get_state((struct renogy_dyn_status_t *)buf);
		if (ret != 0) {
			LOG_ERR("Failed to get charger state");
		} else {
			ret = lorawan_send(0x12, buf, sizeof(struct renogy_dyn_status_t), LORAWAN_MSG_UNCONFIRMED);
			if (ret < 0) {
				LOG_ERR("lorawan_send failure: %d", ret);
			}
		}

		ret = charger_get_cur_stats((struct renogy_dyn_stat_t *)buf);
		if (ret != 0) {
			LOG_ERR("Failed to get charger stats");
		} else {
			ret = lorawan_send(0x13, buf, sizeof(struct renogy_dyn_stat_t), LORAWAN_MSG_UNCONFIRMED);
			if (ret < 0) {
				LOG_ERR("lorawan_send failure: %d", ret);
			}
		}

		k_sleep(K_MINUTES(5));
	}

err:
	return 0;
}
