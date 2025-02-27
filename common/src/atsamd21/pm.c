#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/pm/policy.h>

LOG_MODULE_DECLARE(pm, LOG_LEVEL_INF);

static bool lp_enabled;

#define DOUBLE_TAP_MAGIC 0xf01669ef

void reset_into_bootloader(void) {
	uint32_t *top;

        usb_dc_detach();

	top = (uint32_t *)(DT_REG_ADDR(DT_NODELABEL(sram0)) +
			   DT_REG_SIZE(DT_NODELABEL(sram0)));
	top[-1] = DOUBLE_TAP_MAGIC;
	NVIC_SystemReset();
}

int pm_init(void) {
        /* Start with low-power enabled */
        lp_enabled = 1;

        return 0;
}

void pm_disable_lp(void) {
        if (lp_enabled) {
		LOG_INF("Locking PM states");
                pm_policy_state_lock_get(PM_STATE_STANDBY, PM_ALL_SUBSTATES);
                pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
                //pm_policy_state_lock_get(PM_STATE_RUNTIME_IDLE, PM_ALL_SUBSTATES);

                lp_enabled = 1;
        }
}

void pm_enable_lp(void) {
        if (!lp_enabled) {
                LOG_INF("Unlocking PM states");
                pm_policy_state_lock_put(PM_STATE_STANDBY, PM_ALL_SUBSTATES);
                pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
                //pm_policy_state_lock_put(PM_STATE_RUNTIME_IDLE, PM_ALL_SUBSTATES);

                lp_enabled = 0;
        }
}