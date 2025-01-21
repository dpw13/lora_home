#ifndef __KEYS_H__
#define __KEYS_H__

#include <stdint.h>

int generate_keys(void);
uint8_t *get_join_eui(void);
uint8_t *get_dev_eui(void);
uint8_t *get_app_key(void);
uint8_t *get_nwk_key(void);

#endif