#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/lorawan/lorawan.h>

#include "renogy.h"
#include "relay.h"

LOG_MODULE_REGISTER(app, CONFIG_LORAWAN_SERVICES_LOG_LEVEL);

uint8_t charger_present;

/* TODO: refactor as periodic lorawan service */
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
