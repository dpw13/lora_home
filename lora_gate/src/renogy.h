#ifndef __RENOGY_H__
#define __RENOGY_H__

#include "renogy_internal.h"

int init_charger(void);

int charger_get_system(struct renogy_sys_t *buf);
int charger_get_cur_stats(struct renogy_dyn_stat_t *buf);
int charger_get_daily_stats(struct renogy_dyn_daily_t *buf);
int charger_get_hist_stats(struct renogy_dyn_hist_t *buf);
int charger_get_state(struct renogy_dyn_status_t *buf);
int charger_get_bat_info(struct renogy_param_bat_t *buf);

#endif /* __RENOGY_H__ */