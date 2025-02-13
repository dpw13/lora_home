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

#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* This is a hack to balance the PM state locks, since USB will be reset multiple times
 * but only disconnected once.
 */
inline static void pm_policy_state_lock_get_if_needed(enum pm_state state, uint8_t substate_id) {
	if (!pm_policy_state_lock_is_active(state, substate_id)) {
		LOG_INF("Locking PM %d:%d", state, substate_id);
		pm_policy_state_lock_get(state, substate_id);
	}
}

#include <soc.h>

#define REGS ((Usb *)DT_REG_ADDR(DT_NODELABEL(usb0)))

struct k_timer usb_fsm_timer;
#define FSM_INTERVAL	K_MSEC(5000)

static void usb_fsm_work(struct k_timer *timer) {
	uint8_t usb_fsm_state = REGS->DEVICE.FSMSTATUS.bit.FSMSTATE;

	LOG_INF("USB FSM: %02x", usb_fsm_state);
	switch (usb_fsm_state) {
		case USB_FSMSTATUS_FSMSTATE_OFF:
		case USB_FSMSTATUS_FSMSTATE_SUSPEND:
			/* Disconnected: re-enable PM */
			pm_policy_state_lock_put(PM_STATE_RUNTIME_IDLE, PM_ALL_SUBSTATES);
			pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
			pm_policy_state_lock_put(PM_STATE_STANDBY, PM_ALL_SUBSTATES);
			/* Stop timer; the callback below will notify us of a reconnect */
			k_timer_stop(&usb_fsm_timer);
	}
}

void app_usb_status_callback(enum usb_dc_status_code cb_status, const uint8_t *param) {
	switch (cb_status) {
		case USB_DC_RESET:
			LOG_INF("USB Reset");
			/* Disable power management when USB connects */
			pm_policy_state_lock_get_if_needed(PM_STATE_STANDBY, PM_ALL_SUBSTATES);
			pm_policy_state_lock_get_if_needed(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
			pm_policy_state_lock_get_if_needed(PM_STATE_RUNTIME_IDLE, PM_ALL_SUBSTATES);
			/* Start timer to check USB state */
			k_timer_start(&usb_fsm_timer, FSM_INTERVAL, FSM_INTERVAL);

			break;
		case USB_DC_DISCONNECTED:
			/* Never called */
		default:
			/* Nothing */
			LOG_INF("cb: %d", cb_status);
	}
}

int main(void)
{
	int ret;

	usb_enable(app_usb_status_callback);
	k_timer_init(&usb_fsm_timer, usb_fsm_work, NULL);

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
		k_msleep(10000);
		LOG_INF("wakeup");
	}

	return 0;
}
