#include <zephyr/kernel.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>

#include <esp_private/periph_ctrl.h>
#include <hal/aes_hal.h>
#include <hal/hmac_hal.h>
#include <hal/hmac_ll.h>
#include <esp_efuse.h>
#include "keys.h"

LOG_MODULE_REGISTER(keys, LOG_LEVEL_INF);

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
	/* Read a full block of data for key derivation */
	uint8_t entropy[SHA256_BLOCK_SZ];
	struct provision_t p;
};

static struct provision_t provisioning = { 0 };

#define STORAGE_ID	FIXED_PARTITION_ID(storage_partition)
#define STORAGE_SIZE	FIXED_PARTITION_SIZE(storage_partition)
#define BLOCK_SIZE	4096

int generate_keys(void) {
	const struct flash_area *storage_area;
	uint8_t key[SHA256_DIGEST_SZ];
	struct crypt_block_t block;

	int err = flash_area_open(STORAGE_ID, &storage_area);
	if (err != 0) {
		LOG_ERR("Failed to open flash area: %d", err);
		return err;
	}

	/* Encrypted block stored at end of flash */
	err = flash_area_read(storage_area, STORAGE_SIZE - BLOCK_SIZE, &block, sizeof(struct crypt_block_t));
	if (err != 0) {
		LOG_ERR("Failed to read flash area: %d", err);
		return err;
	}
	//memset(provisioning.entropy, 0, SHA256_BLOCK_SZ);
	//buffer[0] = 0x80;

	LOG_HEXDUMP_DBG(&block, sizeof(struct crypt_block_t), "Crypt block:");
	//LOG_HEXDUMP_DBG(block.entropy, SHA256_BLOCK_SZ, "Entropy buffer:");

	esp_efuse_block_t key_block;
	if (!esp_efuse_find_purpose(ESP_EFUSE_KEY_PURPOSE_HMAC_UP, &key_block)) {
		uint8_t dev_id[8];

		LOG_ERR("Could not find HMAC_UP key in eFuse");
		/* Print device ID */
		err = hwinfo_get_device_id(dev_id, sizeof(dev_id));
		if (err < 0) {
			LOG_ERR("hwinfo failed: %d", err);
		} else {
			LOG_HEXDUMP_INF(dev_id, sizeof(dev_id), "Device ID");
		}

		return -ENOENT;
	}

	LOG_INF("Using key index %d for HMAC_UP", key_block);

	// We also enable SHA and DS here. SHA is used by HMAC, DS will otherwise hold SHA in reset state.
	periph_module_enable(PERIPH_HMAC_MODULE);
	periph_module_enable(PERIPH_SHA_MODULE);
	periph_module_enable(PERIPH_DS_MODULE);

	/* Derive encryption key from fixed flash buffer */
	hmac_hal_start();
	/* hmac_hal_configure takes the key ID, not the key block id */
	err = hmac_hal_configure(HMAC_OUTPUT_USER, key_block - EFUSE_BLK_KEY0);
	if (err) {
		LOG_ERR("Error configuring HMAC: %d", err);
		return -EINVAL;
	}
	hmac_hal_write_block_512(block.entropy);
	hmac_ll_wait_idle();
	hmac_ll_msg_end();

	hmac_hal_read_result_256(key);
	hmac_hal_clean();

	periph_module_disable(PERIPH_DS_MODULE);
	periph_module_disable(PERIPH_SHA_MODULE);
	periph_module_disable(PERIPH_HMAC_MODULE);

	LOG_HEXDUMP_DBG(key, SHA256_DIGEST_SZ, "Derived key:");

	/* Enable AES hardware */
	periph_module_enable(PERIPH_AES_MODULE);

	aes_hal_setkey(key, SHA256_DIGEST_SZ, ESP_AES_DECRYPT);
	aes_ll_dma_enable(0);

	uint8_t *src = (uint8_t *)&block.p;
	uint8_t *dst = (uint8_t *)&provisioning;
	for (int i=0; i < sizeof(struct provision_t)/AES_BLOCK_BYTES; i++) {
		aes_hal_transform_block(src, dst);
		LOG_HEXDUMP_DBG(src, AES_BLOCK_BYTES, "block ->");
		LOG_HEXDUMP_DBG(dst, AES_BLOCK_BYTES, "block <-");
		src += AES_BLOCK_BYTES;
		dst += AES_BLOCK_BYTES;
	}

	periph_module_disable(PERIPH_AES_MODULE);

	LOG_HEXDUMP_DBG(provisioning.dev_eui, 8, "Dev EUI:");
	LOG_HEXDUMP_DBG(provisioning.app_key, 16, "App key:");
	LOG_HEXDUMP_DBG(provisioning.nwk_key, 16, "Nwk key:");

	return 0;
}

uint8_t *get_join_eui() {
    return &provisioning.join_eui[0];
}

uint8_t *get_dev_eui() {
    return &provisioning.dev_eui[0];
}

uint8_t *get_app_key() {
    return &provisioning.app_key[0];
}

uint8_t *get_nwk_key() {
    return &provisioning.nwk_key[0];
}