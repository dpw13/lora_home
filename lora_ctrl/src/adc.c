#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "adc.h"

LOG_MODULE_REGISTER(adc, CONFIG_ADC_LOG_LEVEL);

#define DT_SPEC(node_id, prop, idx) \
	ADC_DT_SPEC_GET_BY_IDX(node_id, idx)

/* Data of ADC io-channels specified in devicetree. */
static const struct adc_dt_spec adc_channels[] = {
	DT_FOREACH_PROP_ELEM_SEP(DT_PATH(zephyr_user), io_channels,
		DT_SPEC, (,))
};

int32_t adc_sample(uint16_t chan_id);

int adc_init(void)
{
	/* Configure channels individually prior to sampling. */
	for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++) {
		if (!adc_is_ready_dt(&adc_channels[i])) {
			LOG_ERR("ADC controller device %s chan %d not ready\n", adc_channels[i].dev->name, i);
			return 0;
		}

		LOG_DBG("ADC %s:%d: %d bits, vref %d, %d ticks", adc_channels[i].dev->name,
			adc_channels[i].channel_id, adc_channels[i].resolution, adc_channels[i].vref_mv,
			adc_channels[i].channel_cfg.acquisition_time);
	}

	return 0;
}

int32_t adc_sample(uint16_t chan_id) {
	int err;
	uint16_t buf;
	struct adc_sequence sequence = {
		.buffer = &buf,
		/* buffer size in bytes, not number of samples */
		.buffer_size = sizeof(buf),
	};

	if (chan_id >= ARRAY_SIZE(adc_channels)) {
		LOG_ERR("Invalid channel %d", chan_id);
		return 0;
	}

	/* The SAM0 ADC apparently needs to be set up each acquisition */
	err = adc_channel_setup_dt(&adc_channels[chan_id]);
	if (err < 0) {
		LOG_ERR("Could not setup channel %d (%d)\n", chan_id, err);
		return 0;
	}

	err = adc_sequence_init_dt(&adc_channels[chan_id], &sequence);
	if (err < 0) {
		LOG_ERR("Could not init sequence (%d)\n", err);
		return 0;
	}

	err = adc_read_dt(&adc_channels[chan_id], &sequence);
	if (err < 0) {
		LOG_ERR("Could not read ADC %s chan %d (%d)", adc_channels[chan_id].dev->name, chan_id, err);
		return 0;
	}

	LOG_DBG("Channel %d raw value %04x", chan_id, buf);
	int32_t sample = (int32_t)buf;
	err = adc_raw_to_millivolts_dt(&adc_channels[chan_id], &sample);
	if (err < 0) {
		LOG_ERR("Could not convert ADC sample: %d", err);
	}

	return sample;
}

uint16_t adc_read_battery(void) {
	return (uint16_t)adc_sample(0);
}

int16_t adc_read_temp(void) {
	return (int16_t)(adc_sample(1) - 2529);
}
