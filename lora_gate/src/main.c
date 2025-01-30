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
#include "relay.h"

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

uint8_t charger_present;

int charger_xmit_cfg(void) {
	int ret;
	uint8_t buf[256];

	if (!charger_present) {
		return 0;
	}

	ret = charger_get_system((struct renogy_sys_t *)buf);
	if (ret != 0) {
		LOG_ERR("Failed to get charger system data");
		return ret;
	} else {
		ret = lorawan_send(0x10, buf, sizeof(struct renogy_sys_t), LORAWAN_MSG_UNCONFIRMED);
		if (ret < 0) {
			LOG_ERR("lorawan_send failure: %d", ret);
			return ret;
		}
	}

	ret = charger_get_bat_info((struct renogy_param_bat_t *)buf);
	if (ret != 0) {
		LOG_ERR("Failed to get charger battery data");
		return ret;
	} else {
		ret = lorawan_send(0x11, buf, sizeof(struct renogy_param_bat_t), LORAWAN_MSG_UNCONFIRMED);
		if (ret < 0) {
			LOG_ERR("lorawan_send failure: %d", ret);
			return ret;
		}
	}

	return 0;
}

int charger_xmit_cur(void) {
	int ret;
	uint8_t buf[256];

	if (!charger_present) {
		return 0;
	}

	ret = charger_get_state((struct renogy_dyn_status_t *)buf);
	if (ret != 0) {
		LOG_ERR("Failed to get charger state");
		return ret;
	} else {
		LOG_INF("Transmitting dyn status");
		ret = lorawan_send(0x12, buf, sizeof(struct renogy_dyn_status_t), LORAWAN_MSG_UNCONFIRMED);
		if (ret < 0) {
			LOG_ERR("lorawan_send failure: %d", ret);
			return ret;
		}
	}

	ret = charger_get_cur_stats((struct renogy_dyn_stat_t *)buf);
	if (ret != 0) {
		LOG_ERR("Failed to get charger stats");
		return ret;
	} else {
		LOG_INF("Transmitting charger stats");
		ret = lorawan_send(0x13, buf, sizeof(struct renogy_dyn_stat_t), LORAWAN_MSG_UNCONFIRMED);
		if (ret < 0) {
			LOG_ERR("lorawan_send failure: %d", ret);
			return ret;
		}
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

	LOG_INF("Looking for charger");
	k_msleep(500);

	/* TODO: save address in settings */
	charger_present = 0;
	ret = -ENOENT; //init_charger();
	if (ret == 0) {
		charger_present = 1;
	} else {
		LOG_ERR("Failed to find charger");
	}

	fuota_run();

	k_msleep(5000);
	lorawan_relay_run();

	/* TODO: move to separate file and invoke as period lorawan service */
	charger_xmit_cfg();

	while (1) {
		charger_xmit_cur();
		k_sleep(K_MINUTES(5));
	}

err:
	return 0;
}
