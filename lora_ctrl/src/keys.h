#ifndef __KEYS_H__
#define __KEYS_H__

#include <stdint.h>

#define LORAWAN_JOIN_EUI	{ 0xEF, 0x4E, 0x6B, 0x27, 0x35, 0x14, 0xCA, 0x84 }

extern const uint8_t join_eui[];

int generate_keys(void);
uint8_t *get_dev_eui(void);
uint8_t *get_app_key(void);

#endif