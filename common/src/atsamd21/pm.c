#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/kernel_structs.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/pm/policy.h>

LOG_MODULE_DECLARE(pm, LOG_LEVEL_INF);

static bool lp_enabled;

#define DOUBLE_TAP_MAGIC 0xf01669ef

static struct _timeout sysclock_lock;
static atomic_t lock_count;

/* These functions aren't exported by the kernel headers */
void z_add_timeout(struct _timeout *to, _timeout_func_t fn,
        k_timeout_t timeout);

int z_abort_timeout(struct _timeout *to);

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
        memset(&sysclock_lock, 0, sizeof(struct _timeout));

        return 0;
}

static void pm_sysclock_fn(struct _timeout *t) {
        /* Nothing; this should never actually get called */
}

void pm_sysclock_force_active(void) {
        /* Add a low-level kernel timeout to prevent sysclock from
         * being deactivated */
        if (atomic_inc(&lock_count) == 0) {
                z_add_timeout(&sysclock_lock, pm_sysclock_fn, Z_TIMEOUT_TICKS(INT_MAX));
        }
}

void pm_sysclock_allow_idle(void) {
        if (atomic_dec(&lock_count) == 1) {
                z_abort_timeout(&sysclock_lock);
        }
}

void pm_disable_lp(void) {
        if (lp_enabled) {
                pm_sysclock_force_active();
		LOG_INF("Locking PM states");
                pm_policy_state_lock_get(PM_STATE_STANDBY, PM_ALL_SUBSTATES);
                pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
                //pm_policy_state_lock_get(PM_STATE_RUNTIME_IDLE, PM_ALL_SUBSTATES);

                lp_enabled = 1;
        }
}

void pm_enable_lp(void) {

        if (!lp_enabled) {
                pm_sysclock_allow_idle();

                LOG_INF("Unlocking PM states");
                pm_policy_state_lock_put(PM_STATE_STANDBY, PM_ALL_SUBSTATES);
                pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
                //pm_policy_state_lock_put(PM_STATE_RUNTIME_IDLE, PM_ALL_SUBSTATES);

                lp_enabled = 0;
        }
}