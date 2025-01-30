#include <zephyr/kernel.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/logging/log.h>

#include <LoRaMac.h>
#include <secure-element.h>
#include <lw_priv.h>
#include "../src/keys.h"

LOG_MODULE_REGISTER(eui, CONFIG_LORAWAN_LOG_LEVEL);

/**
 * Read the current dev EUI. If it is all zeros, regenerate the dev EUI
 * and keys. Must be called *after* lorawan_start() in order to ensure
 * existing data has been loaded from NVM.
 */
int lorawan_init_eui(void) {
	uint8_t zero[SE_EUI_SIZE] = { 0 };
	uint8_t *cur_dev_eui = SecureElementGetDevEui();
	LoRaMacStatus_t status;

	if (memcmp(cur_dev_eui, zero, SE_EUI_SIZE) == 0) {
		/* EUIs recovered from NVM are zero. Regenerate. */
		MibRequestConfirm_t mib_req;
		int ret;

		ret = generate_keys();
		if (ret != 0) {
			LOG_ERR("Failed to generate keys: %d", ret);
		}

		mib_req.Type = MIB_DEV_EUI;
		mib_req.Param.DevEui = get_dev_eui();
		status = LoRaMacMibSetRequestConfirm(&mib_req);
		if (status != LORAMAC_STATUS_OK) {
			LOG_ERR("Failed to set dev_eui: %s (%d)", 
				lorawan_status2str(status), lorawan_status2errno(status));
		}

		mib_req.Type = MIB_JOIN_EUI;
		mib_req.Param.JoinEui = (uint8_t *)&join_eui[0];
		status = LoRaMacMibSetRequestConfirm(&mib_req);
		if (status != LORAMAC_STATUS_OK) {
			LOG_ERR("Failed to set dev_eui: %s (%d)", 
				lorawan_status2str(status), lorawan_status2errno(status));
		}

		mib_req.Type = MIB_NWK_KEY;
		mib_req.Param.NwkKey = get_app_key();
		status = LoRaMacMibSetRequestConfirm(&mib_req);
		if (status != LORAMAC_STATUS_OK) {
			LOG_ERR("Failed to set dev_eui: %s (%d)", 
				lorawan_status2str(status), lorawan_status2errno(status));
		}

		mib_req.Type = MIB_APP_KEY;
		mib_req.Param.AppKey = get_app_key();
		status = LoRaMacMibSetRequestConfirm(&mib_req);
		if (status != LORAMAC_STATUS_OK) {
			LOG_ERR("Failed to set dev_eui: %s (%d)", 
				lorawan_status2str(status), lorawan_status2errno(status));
		}
	}

	return 0;
}
