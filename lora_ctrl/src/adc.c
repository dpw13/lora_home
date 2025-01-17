#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "adc.h"

LOG_MODULE_REGISTER(adc, LOG_LEVEL_INF);

#define DT_SPEC_AND_COMMA(node_id, prop, idx) \
	ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

/* Data of ADC io-channels specified in devicetree. */
static const struct adc_dt_spec adc_channels[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels,
			     DT_SPEC_AND_COMMA)
};

int adc_init(void)
{
	int err;

	/* Configure channels individually prior to sampling. */
	for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++) {
		if (!adc_is_ready_dt(&adc_channels[i])) {
			LOG_ERR("ADC controller device %s not ready\n", adc_channels[i].dev->name);
			return 0;
		}

		LOG_INF("ADC %s:%d: %d bits, vref %d, %d ticks", adc_channels[i].dev->name,
			adc_channels[i].channel_id, adc_channels[i].resolution, adc_channels[i].vref_mv,
			adc_channels[i].channel_cfg.acquisition_time);

		err = adc_channel_setup_dt(&adc_channels[i]);
		if (err < 0) {
			LOG_ERR("Could not setup channel #%d (%d)\n", i, err);
			return 0;
		}
	}

	return 0;
}

uint16_t adc_sample(uint16_t chan_id) {
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

	(void)adc_sequence_init_dt(&adc_channels[chan_id], &sequence);

	err = adc_read_dt(&adc_channels[chan_id], &sequence);
	if (err < 0) {
		LOG_ERR("Could not read (%d)", err);
		return 0;
	}

	return buf;
}

uint16_t adc_read_battery(void) {
	return adc_sample(0);
}

uint16_t adc_read_temp(void) {
	return adc_sample(1);
}
