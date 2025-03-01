#ifndef __PM_H__
#define __PM_H__

void reset_into_bootloader(void);

int pm_init(void);

void pm_sysclock_force_active(void);
void pm_sysclock_allow_idle(void);

void pm_enable_lp(void);
void pm_disable_lp(void);

#endif