#ifndef __KEYS_H__
#define __KEYS_H__

#include <stdint.h>

/* Not all of these are secrets but to simplify the provisioning process 
 * just include everything.
 */
struct provision_t {
    uint8_t dev_eui[8];
    uint8_t join_eui[8];
    uint8_t app_key[16];
    uint8_t nwk_key[16];
};

struct crypt_block_t {
	/* Read a full SHA256_BLOCK_SZ of data for key derivation */
	uint8_t entropy[64];
	struct provision_t p;
};

int generate_keys(void);
uint8_t *get_join_eui(void);
uint8_t *get_dev_eui(void);
uint8_t *get_app_key(void);
uint8_t *get_nwk_key(void);

#endif