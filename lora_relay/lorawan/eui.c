#include <zephyr/kernel.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/logging/log.h>

#include <LoRaMac.h>
#include <secure-element.h>
#include <lw_priv.h>
#include "../src/keys.h"

LOG_MODULE_REGISTER(eui, CONFIG_LORAWAN_SERVICES_LOG_LEVEL);

/**
 * Read and decrypt lorawan secrets
 */
int lorawan_init_eui(void) {
	LoRaMacStatus_t status;

	MibRequestConfirm_t mib_req;
	int ret;

	ret = generate_keys();
	if (ret != 0) {
		LOG_ERR("Failed to generate keys: %d", ret);
		return ret;
	}

	mib_req.Type = MIB_DEV_EUI;
	mib_req.Param.DevEui = get_dev_eui();
	status = LoRaMacMibSetRequestConfirm(&mib_req);
	if (status != LORAMAC_STATUS_OK) {
		LOG_ERR("Failed to set dev_eui: %s (%d)", 
			lorawan_status2str(status), lorawan_status2errno(status));
	}

	mib_req.Type = MIB_JOIN_EUI;
	mib_req.Param.JoinEui = get_join_eui();
	status = LoRaMacMibSetRequestConfirm(&mib_req);
	if (status != LORAMAC_STATUS_OK) {
		LOG_ERR("Failed to set join_eui: %s (%d)", 
			lorawan_status2str(status), lorawan_status2errno(status));
	}

	mib_req.Type = MIB_NWK_KEY;
	mib_req.Param.NwkKey = get_nwk_key();
	status = LoRaMacMibSetRequestConfirm(&mib_req);
	if (status != LORAMAC_STATUS_OK) {
		LOG_ERR("Failed to set nwk_key: %s (%d)", 
			lorawan_status2str(status), lorawan_status2errno(status));
	}

	mib_req.Type = MIB_APP_KEY;
	mib_req.Param.AppKey = get_app_key();
	status = LoRaMacMibSetRequestConfirm(&mib_req);
	if (status != LORAMAC_STATUS_OK) {
		LOG_ERR("Failed to set app_key: %s (%d)", 
			lorawan_status2str(status), lorawan_status2errno(status));
	}

	return 0;
}
