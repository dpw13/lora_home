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
#include "adc.h"
#include "pwm.h"
#include "buttons.h"

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

	ret = adc_init();
	ret = pwm_init();
	ret = button_init();
	ret = lora_init();
	ret = lora_config(lora_dev, &lora_cfg);

	while (1) {
		uint8_t btns = button_poll() | 0x02;
		uint16_t bat = adc_read_battery();
		LOG_INF("Battery: %04x Btns: %02x", bat, btns);

		pwm_set2(0, bat);

		if (btns == 0x07) {
			fuota_run();
		} else {
			for (int i=0; i < 3; i++) {
				if (btns & 0x01) {
					int16_t rssi;
					int8_t snr;

					remote_msg[0] = i;
					lora_send(lora_dev, remote_msg, sizeof(remote_msg));
					lora_recv(lora_dev, remote_msg, sizeof(remote_msg), K_MSEC(200), &rssi, &snr);
				}
				btns >>= 1;
			}
		}
		k_msleep(500);
	}

	return 0;
}
