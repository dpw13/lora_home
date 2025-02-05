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
#include <zephyr/drivers/gpio.h>
#include "app_protocol.h"
#include "adc.h"
#include "fuota.h"
#include "pwm.h"
#include "buttons.h"


LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static const struct device *lora_dev = DEVICE_DT_GET(DT_ALIAS(lora0));
static const struct lora_modem_config lora_tx_cfg = {
	/* Channel 7 in US915 */
	.frequency = 903700000,
	/* DR_0 (uplink) corresponds to SF_10 on a 125 kHz channel for region US915 */
	.bandwidth = BW_125_KHZ,
	.datarate = SF_10,
	.coding_rate = CR_4_5,
	.public_network = 1, // call it public for now to match LoRaWAN config
	.preamble_len = 8,
	.tx_power = 20,
	.tx = true,
};

static const struct lora_modem_config lora_rx_cfg = {
	/* Channel 64 in US915 */
	.frequency = 923300000,
	/* DR_8 (downlink) corresponds to SF_12 on a 500 kHz channel for region US915 */
	.bandwidth = BW_500_KHZ,
	.datarate = SF_12,
	.coding_rate = CR_4_5,
	.public_network = 1, // call it public for now to match LoRaWAN config
	.preamble_len = 8,
	.iq_inverted = true, // Seems to be set by default for downlinks
};

int lora_init(void) {
	if (!device_is_ready(lora_dev)) {
		LOG_ERR("%s: device not ready.", lora_dev->name);
		return -ENODEV;
	}

        return 0;
}

struct lora_remote_uplink_t uplink;

int main(void)
{
	int ret;

	if (!gpio_is_ready_dt(&led)) {
		LOG_ERR("LED not ready??");
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure LED pin");
	}

	ret = adc_init();
	ret = pwm_init();
	ret = button_init();
	ret = lora_init();

	/* Set MHDR to proprietary, LoRa major version 1 */
	uplink.hdr.mhdr = LORA_MHDR_PROPRIETARY;
	uplink.hdr.type = LORA_PROP_TYPE_REMOTE;

	while (1) {
		uint8_t btns = button_poll();
		uint16_t bat = adc_read_battery();
		uplink.hdr.battery_lvl = bat;
		LOG_INF("Battery: %04x Btns: %02x", bat, btns);

		pwm_set2(0, bat);

		if (btns == 0x07) {
			fuota_run();
		} else {
			gpio_pin_set_dt(&led, 1);
			for (int i=0; i < 3; i++) {
				if (btns & 0x01) {
					int16_t rssi;
					int8_t snr;

					uplink.cmd = i;
					ret = lora_config(lora_dev, &lora_tx_cfg);
					lora_send(lora_dev, (uint8_t *)&uplink, sizeof(uplink));

					ret = lora_config(lora_dev, &lora_rx_cfg);
					ret = lora_recv(lora_dev, (uint8_t *)&uplink, sizeof(uplink), K_MSEC(1000), &rssi, &snr);
					if (ret > 0) {
						LOG_HEXDUMP_INF(&uplink, sizeof(uplink), "Received response:");
					}
				}
				btns >>= 1;
			}
			gpio_pin_set_dt(&led, 0);
		}
		k_msleep(500);
	}

	return 0;
}
