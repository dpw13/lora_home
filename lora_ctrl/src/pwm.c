#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pwm, LOG_LEVEL_INF);

#define DT_SPEC_AND_COMMA(node_id, prop, idx) \
	ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

/* Data of ADC io-channels specified in devicetree. */
static const struct pwm_dt_spec pwm_channels[] = {
	PWM_DT_SPEC_GET(DT_ALIAS(pwm_led0g)),
	PWM_DT_SPEC_GET(DT_ALIAS(pwm_led0a)),
	PWM_DT_SPEC_GET(DT_ALIAS(pwm_led1g)),
	PWM_DT_SPEC_GET(DT_ALIAS(pwm_led1a)),
	PWM_DT_SPEC_GET(DT_ALIAS(pwm_led2g)),
	PWM_DT_SPEC_GET(DT_ALIAS(pwm_led2a)),
};

int pwm_init(void) {
	for (size_t i = 0U; i < ARRAY_SIZE(pwm_channels); i++) {
		if (!pwm_is_ready_dt(&pwm_channels[i])) {
			LOG_ERR("PWM device %d %s is not ready\n",
				i, pwm_channels[i].dev->name);
			return -1;
		}
	}

	return 0;
}

int pwm_set2(uint16_t channel, uint32_t pulse_width) {
	int ret = pwm_set_pulse_dt(&pwm_channels[channel], pulse_width);
	if (ret) {
		LOG_ERR("failed to set pulse width on channel %d: %d\n", channel, ret);
		return ret;
	}

	return 0;
}