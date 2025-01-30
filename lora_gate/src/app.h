#ifndef __APP_H__
#define __APP_H__

#include <stdint.h>

extern uint8_t charger_present;

int charger_xmit_cfg(void);
int charger_xmit_cur(void);
int fuota_run(void);

#endif /* __APP_H__ */