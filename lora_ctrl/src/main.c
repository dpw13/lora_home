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
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/pm/policy.h>
#include "app_protocol.h"
#include "adc.h"
#include "fuota.h"
#include "pwm.h"
#include "buttons.h"
#include "pm.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#undef DEBUG

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

void leds_off_handler(struct k_work *work) {
	for (int i=0; i < 6; i++) {
		led_set_intensity(i, 0);
	}
}

K_WORK_DELAYABLE_DEFINE(leds_off, leds_off_handler);

void on_error(uint8_t i) {
	pwm_behavior_off();
	leds_off_handler(NULL);
	led_set_intensity(RED_CHAN(i), 50);
	k_work_schedule(&leds_off, K_SECONDS(2));
}

/**
 * Estimate the SoC of the battery based on its current voltage (assumed to
 * be approximately the open-circuit voltage given the low current draw of
 * this device). Input is in mV, output is 8.8 unsigned FXP in the range
 * [0, 100]. May exceed 1 while charging.
 *
 * The OCV to SoC curve here is a very rough estimate. In general the curve
 * is lightly nonlinear, so we estimate SoC using a piecewise linear mapping.
 */
inline static int16_t mv_to_soc(uint16_t bat_mv) {
	int16_t ret;

	if (bat_mv > 3900) {
		/* 4200 -> 25600 */
		/* 3900 -> 6400 */
		ret = ((bat_mv * 64) - 243200);
	} else {
		/* 3900 -> 6400 */
		/* 3400 -> 0 */
		ret = ((bat_mv * 13) - 44300);
	}

	return MIN(MAX(ret, 0), 25600);
}

#define BAR_VARIANCE	0x200

static void render_battery_lvl(uint16_t bat_mv) {
	int16_t soc = mv_to_soc(bat_mv);
	uint8_t bar_max = (uint8_t)(MIN(soc + BAR_VARIANCE/2, 25600) >> 8);
	uint8_t bar_min = (uint8_t)(MAX(soc - BAR_VARIANCE/2, 0) >> 8);

	LOG_DBG("Battery %d mV, soc %03x, bar %02x:%02x", bat_mv, soc, bar_min, bar_max);
	display_bar(300, bar_min, bar_max);
}

static int custom_local_action(uint8_t i, uint8_t action) {
	uint16_t both = ((uint16_t)i << 8) | action;

	switch (both) {
		case 0x022A:
			/* Triple click button 2: enable USB */
			pwm_set_behavior(&beh_colors);
			pm_disable_lp();
			usb_enable(NULL);
			return 1;
		case 0x022B:
			/* Button 2, short short long: reset into bootloader */
			reset_into_bootloader();
			/* no return */
	}

	return 0;
}

static int button_action(uint8_t i, uint8_t action) {
	int ret;
	int16_t rssi;
	int8_t snr;

	/* Set MHDR to proprietary, LoRa major version 1 */
	struct lora_remote_uplink_t uplink;
	uplink.hdr.mhdr = LORA_MHDR_PROPRIETARY;
	uplink.hdr.type = LORA_PROP_TYPE_REMOTE;

	uplink.hdr.battery_lvl = adc_read_battery();
	render_battery_lvl(uplink.hdr.battery_lvl);

	int16_t temp = adc_read_temp();
	int16_t temp_int = temp / 10;
	int16_t temp_frac = temp - (10 * temp_int);
	LOG_INF("Battery: %d mV Temp: %d.%d C Btn: %d", uplink.hdr.battery_lvl, temp_int, temp_frac, i);

	uplink.btn = i;
	uplink.action = action;
	ret = lora_config(lora_dev, &lora_tx_cfg);
	if (ret < 0) {
		LOG_ERR("Failed to configure TX: %d", ret);
		on_error(i);
		return ret;
	}
	ret = lora_send(lora_dev, (uint8_t *)&uplink, sizeof(uplink));
	if (ret < 0) {
		LOG_ERR("Failed to transmit: %d", ret);
		on_error(i);
		return ret;
	}

	ret = lora_config(lora_dev, &lora_rx_cfg);
	if (ret < 0) {
		LOG_ERR("Failed to configure RX: %d", ret);
		on_error(i);
		return ret;
	}

	/* Receive window is tx time + 1s +/- 250 ms */
	k_msleep(750);

	struct lora_remote_downlink_t downlink;
	ret = lora_recv(lora_dev, (uint8_t *)&downlink, sizeof(downlink), K_MSEC(500), &rssi, &snr);
	pwm_behavior_off();
	leds_off_handler(NULL);
	if (ret < 0) {
		LOG_ERR("Failed to receive: %d", ret);
		led_set_intensity(RED_CHAN(i), 100);
	} else if (ret > 0) {
		led_set_intensity(GREEN_CHAN(i), 100);
		LOG_DBG("Received response type %02x: %02x", downlink.hdr.type, downlink.payload);
	}
	/* Schedule LEDs off regardless of what we set */
	k_work_schedule(&leds_off, K_SECONDS(2));

	return 0;
}

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

	ret = gpio_pin_set_dt(&led, 0);
	if (ret < 0) {
		LOG_ERR("Failed to turn off primary LED");
	}

	ret = pm_init();

	ret = adc_init();
	ret = pwm_init();
	ret = button_init();
	ret = lora_init();

	LOG_INF("Running");

#ifdef DEBUG
	pm_disable_lp();
	k_msleep(200);
	usb_enable(NULL);
#endif

	while (1) {
		/* For some unknown reason, the LoRa functions return an error
		 * (tx timeout) if lora_send() is not sent from the main thread.
		 */
		struct action_t act;
		k_msgq_get(&action_queue, &act, K_FOREVER);
		/* See if there's some local action we should take first */
		if (!custom_local_action(act.btn_id, act.action)) {
			/* If not, send the action over LoRa */
			button_action(act.btn_id, act.action);
		}
	}

	//fuota_run();

	return 0;
}
