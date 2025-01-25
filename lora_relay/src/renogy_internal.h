#ifndef __RENOGY_INTERNAL_H__
#define __RENOGY_INTERNAL_H__

#include <stdint.h>

#define REN_SYS_CHARGE_RATING           0x0A
#define REN_SYS_DISCHARGE_RATING        0x0B
#define REN_SYS_MODEL                   0x0C
#define REN_SYS_SW_VER                  0x14
#define REN_SYS_HW_VER                  0x16
#define REN_SYS_SERIAL                  0x18
#define REN_SYS_DEV_ADDR                0x1A

struct renogy_sys_t {
        /* TODO: endianness */
        uint8_t nom_v;
        uint8_t max_charge_i;
        uint8_t max_discharge_i;
        uint8_t prod_type;
        char model[16];
        uint32_t sw_version;
        uint32_t hw_version;
        uint32_t serial;
        uint16_t addr;
};

#define REN_DYN_STAT_SOC                0x100

struct renogy_dyn_stat_t {
        uint16_t soc_pct;
        uint16_t bat_dV;
        uint16_t charge_cA;
        uint8_t controller_c;
        uint8_t bat_c;

        uint16_t load_dV;
        uint16_t load_cA;
        uint16_t load_w;

        uint16_t solar_dV;
        uint16_t solar_cA;
        uint16_t solar_w;
};

#define REN_DYN_DAILY_MIN_V             0x10B

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

/* Cumulative totals */
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
        uint8_t load_status;
        uint8_t charge_state;
};

#define REN_DYN_FAULT_STATE             0x121

#define REN_PARAM_NOM_CAPACITY          0xE002

enum renogy_battery_type_t {
        REN_BAT_OPEN,
        REN_BAT_SEALED,
        REN_BAT_GEL,
        REN_BAT_LITHIUM,
        REN_BAT_CUSTOM,
};

struct renogy_param_bat_t {
        uint16_t nom_capacity_Ah;
        uint8_t nom_bat_V;
        uint8_t act_bat_V;
        uint16_t bat_type;

        uint16_t overvolt_dV;
        uint16_t charge_lim_dV;
        uint16_t equalizing_dV;
        uint16_t boost_dV;
        uint16_t float_dV;
        uint16_t boost_recov_dV;
        uint16_t over_disch_recov_dV;
        uint16_t undervolt_warn_dV;
        uint16_t over_disch_dV;
        uint16_t disch_lim_dV;

        uint8_t end_charge_soc;
        uint8_t end_discharge_soc;

        uint16_t over_disch_delay_s;
        uint16_t eq_charge_s;
        uint16_t boost_charge_s;
        uint16_t eq_charge_interval_d;
        uint16_t temp_comp; /* mV/dev C/2V */
};

#endif /* __RENOGY_INTERNAL_H__ */