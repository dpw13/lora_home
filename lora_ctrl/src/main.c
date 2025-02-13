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
#include <zephyr/usb/usb_device.h>
#include <zephyr/pm/policy.h>
#include "app_protocol.h"
#include "adc.h"
#include "fuota.h"
#include "pwm.h"
#include "buttons.h"


LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

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
		pwm_set2(i, 0);
	}
}

K_WORK_DELAYABLE_DEFINE(leds_off, leds_off_handler);

void on_error(uint8_t i) {
	pwm_behavior_off();
	leds_off_handler(NULL);
	pwm_set2(RED_CHAN(i), DUTY_PCT(50));
	k_work_schedule(&leds_off, K_SECONDS(2));
}

/**
 * Estimate the SoC of the battery based on its current voltage (assumed to
 * be approximately the open-circuit voltage given the low current draw of
 * this device). Input is in mV, output is 4.12 unsigned FXP in the range
 * [0, 1]. May exceed 1 while charging.
 *
 * The OCV to SoC curve here is a very rough estimate. In general the curve
 * is lightly nonlinear, so we estimate SoC using a piecewise linear mapping.
 */
inline static int16_t mv_to_soc(uint16_t lvl) {
	if (lvl > 3900) {
		/* 4200 -> 0x1000 */
		/* 3900 -> 0x0400 */
		return ((lvl * 82) - 311608) >> 3;
	} else {
		/* 3900 -> 0x0400 */
		/* 3400 -> 0x0000 */
		return ((lvl * 16) - 54208) >> 3;
	}
}

#define BAR_VARIANCE	0x200

static void render_battery_lvl(uint16_t lvl) {
	int16_t soc = mv_to_soc(lvl);
	uint8_t bar_max = (uint8_t)(MIN(soc + BAR_VARIANCE/2, (0xFFF)) >> 4);
	uint8_t bar_min = (uint8_t)(MAX(soc - BAR_VARIANCE/2, 0) >> 4);

	LOG_DBG("Battery %d mV, soc %03x, bar %02x:%02x", lvl, soc, bar_min, bar_max);
	display_bar(300, bar_min, bar_max);
}

int button_action(uint8_t i, uint8_t action) {
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
		pwm_set2(RED_CHAN(i), 5000000);
	} else if (ret > 0) {
		pwm_set2(GREEN_CHAN(i), 5000000);
		LOG_DBG("Received response type %02x: %02x", downlink.hdr.type, downlink.payload);
	}
	/* Schedule LEDs off regardless of what we set */
	k_work_schedule(&leds_off, K_SECONDS(2));

	return 0;
}

/* This is a hack to balance the PM state locks, since USB will be reset multiple times
 * but only disconnected once.
 */
inline static void pm_policy_state_lock_get_if_needed(enum pm_state state, uint8_t substate_id) {
	if (!pm_policy_state_lock_is_active(state, substate_id)) {
		pm_policy_state_lock_get(state, substate_id);
	}
}

void app_usb_status_callback(enum usb_dc_status_code cb_status, const uint8_t *param) {
	switch (cb_status) {
		case USB_DC_RESET:
			/* Disable power management when USB connects */
			LOG_INF("Disabling idle and standby states");
			pm_policy_state_lock_get_if_needed(PM_STATE_STANDBY, PM_ALL_SUBSTATES);
			pm_policy_state_lock_get_if_needed(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
			pm_policy_state_lock_get_if_needed(PM_STATE_RUNTIME_IDLE, PM_ALL_SUBSTATES);
			break;
		case USB_DC_DISCONNECTED:
			/* Re-enable power management */
			LOG_INF("Enabling idle and standby states");
			pm_policy_state_lock_put(PM_STATE_RUNTIME_IDLE, PM_ALL_SUBSTATES);
			pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
			pm_policy_state_lock_put(PM_STATE_STANDBY, PM_ALL_SUBSTATES);
			break;
		default:
			/* Nothing */
	}
}

int main(void)
{
	int ret;

	usb_enable(app_usb_status_callback);

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

	ret = adc_init();
	ret = pwm_init();
	ret = button_init();
	ret = lora_init();

	LOG_INF("Running");

	while (1) {
		/* For some unknown reason, the LoRa functions return an error
		 * (tx timeout) if lora_send() is not sent from the main thread.
		 */
		struct action_t act;
		k_msgq_get(&action_queue, &act, K_FOREVER);
		button_action(act.btn_id, act.action);
	}

	//fuota_run();

	return 0;
}
