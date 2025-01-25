#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/modbus/modbus.h>

#include <zephyr/logging/log.h>

#include "renogy.h"

LOG_MODULE_REGISTER(renogy, LOG_LEVEL_INF);

static int client_iface;
static uint8_t client_addr;

const static struct modbus_iface_param client_param = {
	.mode = MODBUS_MODE_RTU,
	.rx_timeout = 100000,
	.serial = {
		.baud = 9600,
		.parity = UART_CFG_PARITY_NONE,
		.stop_bits_client = UART_CFG_STOP_BITS_1,
	},
};

#define MODBUS_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(zephyr_modbus_serial)

static int find_client(void) {
        int ret;
        uint16_t buf;
        uint8_t i=1;

        ret = modbus_read_holding_regs(client_iface, i, REN_SYS_DEV_ADDR, &buf, 1);
        LOG_INF("Addr %02x ret %d", i, ret);

        if (ret == 0) {
                LOG_INF("Device responded at %02x with addr %04x", i, buf);
                return i;
        }

        /* Device not found */
        return -ENOENT;
}

int charger_get_system(struct renogy_sys_t *buf) {
        return modbus_read_holding_regs(client_iface, client_addr, REN_SYS_CHARGE_RATING, (uint16_t *)buf, sizeof(struct renogy_sys_t)/2);
}

int charger_get_cur_stats(struct renogy_dyn_stat_t *buf) {
        return modbus_read_holding_regs(client_iface, client_addr, REN_DYN_STAT_SOC, (uint16_t *)buf, sizeof(struct renogy_dyn_stat_t)/2);
}

int charger_get_daily_stats(struct renogy_dyn_daily_t *buf) {
        return modbus_read_holding_regs(client_iface, client_addr, REN_DYN_DAILY_MIN_V, (uint16_t *)buf, sizeof(struct renogy_dyn_daily_t)/2);
}

int charger_get_hist_stats(struct renogy_dyn_hist_t *buf) {
        return modbus_read_holding_regs(client_iface, client_addr, REN_DYN_HIST_TOTAL_OP_DAYS, (uint16_t *)buf, sizeof(struct renogy_dyn_hist_t)/2);
}

int charger_get_state(struct renogy_dyn_status_t *buf) {
        return modbus_read_holding_regs(client_iface, client_addr, REN_DYN_STATUS, (uint16_t *)buf, sizeof(struct renogy_dyn_status_t)/2);
}

int charger_get_bat_info(struct renogy_param_bat_t *buf) {
        return modbus_read_holding_regs(client_iface, client_addr, REN_PARAM_NOM_CAPACITY, (uint16_t *)buf, sizeof(struct renogy_param_bat_t)/2);
}

int init_charger(void) {
        int ret;
	const char *iface_name = DEVICE_DT_NAME(MODBUS_NODE);

	client_iface = modbus_iface_get_by_name(iface_name);

	ret = modbus_init_client(client_iface, client_param);
        if (ret < 0) {
                LOG_ERR("Could not init modbus client: %d\n", ret);
                return ret;
        }

        k_msleep(100);

        ret = find_client();
        if (ret < 0) {
                LOG_ERR("Could not detect device");
                return ret;
        }
        client_addr = (uint16_t)ret;

        return 0;
}

