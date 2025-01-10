#include <zephyr/kernel.h>
#include <zephyr/drivers/hwinfo.h>
#include <psa/crypto.h>
#include <zephyr/logging/log.h>

#include "keys.h"

#define EUI_KEYS_ITER_COUNT 8
#define EUI_KEYS_ALG        PSA_ALG_PBKDF2_HMAC(PSA_ALG_SHA_256)

LOG_MODULE_REGISTER(keys, CONFIG_LORAWAN_SERVICES_LOG_LEVEL);

const uint8_t join_eui[] = LORAWAN_JOIN_EUI;

struct dev_keys_t {
    uint8_t dev[8];
    uint8_t app[8];
};

static struct dev_keys_t dev_keys = { 0 };

int generate_keys(void) {
	uint8_t dev_id[8];
	psa_status_t status = PSA_SUCCESS;
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_key_derivation_operation_t operation = PSA_KEY_DERIVATION_OPERATION_INIT;
	int ret;

	/* TODO: check settings to see if this was previously calculated */

	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DERIVE);
	psa_set_key_lifetime(&attributes, PSA_KEY_PERSISTENCE_READ_ONLY);
	psa_set_key_algorithm(&attributes, EUI_KEYS_ALG);
	psa_set_key_type(&attributes, PSA_KEY_TYPE_RAW_DATA);
	psa_set_key_bits(&attributes, PSA_BYTES_TO_BITS(sizeof(struct dev_keys_t)));

	status = psa_key_derivation_setup(&operation, EUI_KEYS_ALG);
	if (status != PSA_SUCCESS) {
		goto out;
	}

	status = psa_key_derivation_input_integer(&operation, PSA_KEY_DERIVATION_INPUT_COST,
						  EUI_KEYS_ITER_COUNT);
	if (status != PSA_SUCCESS) {
		goto out;
	}

    /* Salt with the join EUI */
	status = psa_key_derivation_input_bytes(&operation, PSA_KEY_DERIVATION_INPUT_SALT,
						join_eui, sizeof(join_eui));
	if (status != PSA_SUCCESS) {
		goto out;
	}

	/* Copy device ID into dev_id */
	ret = hwinfo_get_device_id(dev_id, sizeof(dev_id));
	if (ret < 0) {
		LOG_ERR("hwinfo failed: %d", ret);
		goto out;
	}

    /* Use the device ID as a low-entropy input */
	status = psa_key_derivation_input_bytes(&operation, PSA_KEY_DERIVATION_INPUT_PASSWORD,
						dev_id, sizeof(dev_id));
	if (status != PSA_SUCCESS) {
		goto out;
	}

	status = psa_key_derivation_output_bytes(&operation, (uint8_t *)&dev_keys, sizeof(struct dev_keys_t));
	if (status != PSA_SUCCESS) {
		goto out;
	}

out:
	psa_reset_key_attributes(&attributes);
	psa_key_derivation_abort(&operation);

	return 1;
}

uint8_t *get_dev_eui() {
    return &dev_keys.dev[0];
}

uint8_t *get_app_key() {
    return &dev_keys.app[0];
}