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
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log.h>
#include <zephyr/lorawan/lorawan.h>
#include "../lorawan/eui.h"

LOG_MODULE_REGISTER(lorawan_fuota, CONFIG_LORAWAN_SERVICES_LOG_LEVEL);

#define DELAY K_SECONDS(180)

static enum lorawan_datarate dr_wait;
static enum lorawan_datarate dr_current;

/* Initialize semaphore for waiting for datarate increase */
K_SEM_DEFINE(dr_sem, 1, 1);

static void datarate_changed(enum lorawan_datarate dr)
{
	uint8_t unused, max_size;

	lorawan_get_payload_sizes(&unused, &max_size);
	LOG_INF("New Datarate: DR %d, Max Payload %d", dr, max_size);
	dr_current = dr;
	if (dr >= dr_wait) {
		k_sem_give(&dr_sem);
	}
}

void wait_for_datarate(enum lorawan_datarate dr)
{
	if (dr > dr_current) {
		dr_wait = dr;
		k_sem_take(&dr_sem, K_FOREVER);
	}
}

static void fuota_finished(void)
{
	LOG_INF("FUOTA finished. Resetting device to apply firmware upgrade.");
	/* Wait for log to flush */
	k_msleep(100);
	sys_reboot(SYS_REBOOT_WARM);
}

int fuota_run(void) {
	struct lorawan_join_config join_cfg = {0};
	int ret;

	/* Keys restored from NVM */
	ret = lorawan_start();
	if (ret < 0) {
		LOG_ERR("lorawan_start failed: %d", ret);
		return ret;
	}

	ret = lorawan_init_eui();
	if (ret < 0) {
		LOG_ERR("failed to initialize EUI: %d", ret);
		return ret;
	}

	lorawan_register_dr_changed_callback(datarate_changed);

	join_cfg.mode = LORAWAN_ACT_OTAA;
	/*
	 * By not setting EUIs and keys we rely on the restored NVM values
	 * and those generated in lorawan_init_eui()
	 */

	do {
		LOG_INF("Joining network over OTAA");
		ret = lorawan_join(&join_cfg);
		if (ret < 0) {
			LOG_ERR("lorawan_join_network failed: %d", ret);
			//return ret;
			k_msleep(5000);
		}
	} while (ret < 0);

	lorawan_enable_adr(true);

	/*
	 * Clock synchronization is required to schedule the multicast session
	 * in class C mode. It can also be used independent of FUOTA.
	 */
	lorawan_clock_sync_run();

	/*
	 * The multicast session setup service is automatically started in the
	 * background. It is also responsible for switching to class C at a
	 * specified time.
	 */

	/*
	 * The fragmented data transport transfers the actual firmware image.
	 * It could also be used in a class A session, but would take very long
	 * in that case.
	 */
	lorawan_frag_transport_run(fuota_finished);

	return 0;
}
