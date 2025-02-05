#include <zephyr/kernel.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/logging/log.h>
#include <psa/crypto.h>

#include "keys.h"

#define EUI_KEYS_ITER_COUNT 8
#define EUI_KEYS_ALG        PSA_ALG_PBKDF2_HMAC(PSA_ALG_SHA_256)

LOG_MODULE_REGISTER(keys, LOG_LEVEL_INF);

static struct provision_t provisioning = { 0 };

#define STORAGE_ID	FIXED_PARTITION_ID(storage_partition)
#define STORAGE_SIZE	FIXED_PARTITION_SIZE(storage_partition)
#define FLASH_BLOCK_SIZE	256

int generate_keys(void) {
	const struct flash_area *storage_area;
	struct crypt_block_t block;

	psa_status_t status = PSA_SUCCESS;
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;

	/* Unconditionally print device ID. We could check to see if
	 * the flash is empty, but we can't tell if the decrypt fails;
	 * if it does we'll simply get garbage out.
	 */
	uint8_t dev_id[8];
	int err = hwinfo_get_device_id(dev_id, sizeof(dev_id));
	if (err < 0) {
		LOG_ERR("hwinfo failed: %d", err);
	} else {
		LOG_HEXDUMP_INF(dev_id, sizeof(dev_id), "Device ID");
	}

	err = flash_area_open(STORAGE_ID, &storage_area);
	if (err != 0) {
		LOG_ERR("Failed to open flash area: %d", err);
		return err;
	}

	/* Encrypted block stored at end of flash */
	LOG_DBG("Reading flash from %lx", storage_area->fa_off + STORAGE_SIZE - FLASH_BLOCK_SIZE);
	err = flash_area_read(storage_area, STORAGE_SIZE - FLASH_BLOCK_SIZE, &block, sizeof(struct crypt_block_t));
	if (err != 0) {
		LOG_ERR("Failed to read flash area: %d", err);
		return err;
	}

	/* As this SoC has no read protection on keys, we simply store the derived key
	 * in plaintext on the flash. We still use that key to decrypt the secrets so
	 * the secrets themselves are not directly visible in flash. This is not really
	 * secure in any way, but this SoC provides little ability to do better.
	 */
	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DECRYPT);
	/* erase key from memory when we're done */
	psa_set_key_lifetime(&attributes, PSA_KEY_LIFETIME_VOLATILE);
	psa_set_key_algorithm(&attributes, PSA_ALG_ECB_NO_PADDING);
	psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);

	psa_key_id_t key_id;
	status = psa_import_key(&attributes, &block.entropy[0], 32, &key_id);
	if (status != PSA_SUCCESS) {
		LOG_ERR("Failed to import key: %d", status);
		goto out;
	}
	LOG_HEXDUMP_DBG(&block.entropy[0], 32, "Derived key");

	size_t bytes_decrypted;
	status = psa_cipher_decrypt(key_id, PSA_ALG_ECB_NO_PADDING,
		(const uint8_t *)&block.p, sizeof(struct provision_t), // input ptr and size
		(uint8_t *)&provisioning, sizeof(struct provision_t), &bytes_decrypted);

	if (status != PSA_SUCCESS) {
		LOG_ERR("Failed to decrypt provisioning secrets: %d", status);
		goto out;
	}
	if (bytes_decrypted != sizeof(struct provision_t)) {
		LOG_ERR("Unexpected decrypt length: %d", bytes_decrypted);
	}

	LOG_HEXDUMP_DBG(provisioning.dev_eui, 8, "Device EUI");
	LOG_HEXDUMP_DBG(provisioning.app_key, 8, "App key");

out:
	psa_destroy_key(key_id);
	psa_reset_key_attributes(&attributes);

	return status;
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