#ifndef __RENOGY_INTERNAL_H__
#define __RENOGY_INTERNAL_H__

#include <stdint.h>

/* See e.g. https://www.going-flying.com/blog/files/141/ROVER_MODBUS.pdf */

#define REN_SYS_CHARGE_RATING           0x0A
#define REN_SYS_DISCHARGE_RATING        0x0B
#define REN_SYS_MODEL                   0x0C
#define REN_SYS_SW_VER                  0x14
#define REN_SYS_HW_VER                  0x16
#define REN_SYS_SERIAL                  0x18
#define REN_SYS_DEV_ADDR                0x1A

struct renogy_sys_t {
        /* TODO: endianness */
        uint8_t max_charge_i;
        uint8_t nom_v;
        uint8_t prod_type;
        uint8_t max_discharge_i;
        char model[16];
        uint32_t sw_version;
        uint32_t hw_version;
        uint32_t serial;
        uint16_t addr;
};

#define REN_DYN_STAT_SOC                0x100

struct renogy_dyn_statistics_t {
        uint16_t soc_pct;
        uint16_t bat_dV;
        uint16_t charge_cA;
        uint8_t bat_temp_C;
        uint8_t controller_temp_C;

        uint16_t load_dV;
        uint16_t load_cA;
        uint16_t load_W;

        uint16_t solar_dV;
        uint16_t solar_cA;
        uint16_t solar_W;
};

#define REN_DYN_DAILY_MIN_V             0x10B

/**
 * Daily totals
 */
struct renogy_dyn_daily_t {
        uint16_t min_bat_dV;
        uint16_t max_bat_dV;
        uint16_t max_charge_cA;
        uint16_t max_discharge_cA;
        uint16_t max_charge_W;
        uint16_t max_discharge_W;

        /* Total for current day */
        uint16_t charge_Ah;
        uint16_t discharge_Ah;
        uint16_t generation_dWh; /* kWh/10k */
        uint16_t consumption_dWh;
};

#define REN_DYN_HIST_TOTAL_OP_DAYS      0x115

/**
 * Cumulative totals
 */
struct renogy_dyn_hist_t {
        uint16_t op_days;
        uint16_t over_discharge_cnt;
        uint16_t full_charge_cnt;

        uint32_t charge_Ah;
        uint32_t discharge_Ah;
        uint32_t generation_dWh; /* kWh/10k */
        uint32_t consumption_dWh;
};

#define REN_DYN_STATUS                  0x120

enum renogy_charge_state_t {
        REN_CHARGE_DEACTIVATED = 0,
        REN_CHARGE_ACTIVATED,
        REN_CHARGE_MPPT,
        REN_CHARGE_EQUALIZING,
        REN_CHARGE_BOOST,
        REN_CHARGE_FLOAT,
        REN_CHARGE_OVERPOWER,
};

struct renogy_dyn_status_t {
        uint8_t charge_state;
        uint8_t load_status;
};

#define REN_DYN_FAULT_STATE             0x121

#define REN_PARAM_NOM_CAPACITY          0xE002

enum renogy_battery_type_t {
        REN_BAT_OPEN,
        REN_BAT_FLOODED,
        REN_BAT_SEALED,
        REN_BAT_GEL,
        REN_BAT_LITHIUM,
        REN_BAT_CUSTOM,
};

// Unused values seem to default to 14.2V
// 120

// Lithium?
// 1212 200  160  4
// 0c0c c800 a000 0400
//      solar_ov?,hv_disconnect,type
// 14.2 15.5 14.2 14.2 12.5 13.2 11.0 12.0
// 8e00 9b00 8e00 8e00 7d00 8400 6e00 7800
// boost v,ev reconn,float,eq v,lv reconn?,boost return,overdisch recover?,?
// 3264
// 10.8 120       30   120
// 6c00 7800 0500 1e00 7800
// disch lim,eq dur?,eq interval d,ov disch delay?,?

// Sealed:
// 0cff c800 a000 0200
// 14.6 15.5 13.8 14.6 12.6 13.2 11.1 12.0
// 9200 9b00 8a00 9200 7e00 8400 6f00 7800
// 3264
// 10.6 0    5    0    120
// 6a00 0000 0500 0000 7800

// Gel:
// 0cff c800 a000 0300
// 15.2 15.5 13.8 14.2 12.6 13.2 11.1 12.0
// 9800 9b00 8a00 8e00 7e00 8400 6f00 7800
// 3264
// 10.6 0    5    0    120
// 6a00 0000 0500 0000 7800

// Flooded:
// 0cff c800 a000 0100
// 14.8 15.5 13.8 14.6 12.6 13.2 11.1 12.0
// 9400 9b00 8a00 9200 7e00 8400 6f00 7800
// 3264
// 6a00 7800 0500 1e00 7800

/* This struct likely has errors */
struct renogy_param_bat_t {
        uint8_t nom_bat_V;
        uint8_t act_bat_V;

        uint16_t solar_overvolt_dV;
        uint16_t charge_lim_dV;

        uint16_t bat_type;

        /* The boost voltage shouldn't be higher than the
         * equalization voltage so something is likely wrong here.
         * This is more likely some battery limit disconnect voltage
         * but the boost voltage in the datasheet appears to be missing.
         */
        uint16_t boost_dV;
        uint16_t overvolt_recov_dV;
        uint16_t float_dV;
        uint16_t equalizing_dV;
        uint16_t over_disch_recov_dV;
        uint16_t boost_recov_dV;
        uint16_t over_disch_dV;
        /* "under-voltage threshold" */
        uint16_t undervolt_warn_dV;

        uint8_t end_discharge_soc;
        uint8_t end_charge_soc;

        uint16_t over_disch_lim_dV;
        uint16_t eq_charge_s;
        uint16_t over_disch_delay_s;
        uint16_t eq_charge_interval_d;
        uint16_t boost_charge_s;
};

#endif /* __RENOGY_INTERNAL_H__ */