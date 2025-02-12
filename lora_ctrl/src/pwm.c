#include <zephyr/kernel.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>

#include "pwm.h"

LOG_MODULE_REGISTER(pwm, LOG_LEVEL_INF);

/* Data of ADC io-channels specified in devicetree. */
static const struct pwm_dt_spec pwm_channels[N_LEDS] = {
	PWM_DT_SPEC_GET(DT_ALIAS(pwm_led0g)),
	PWM_DT_SPEC_GET(DT_ALIAS(pwm_led0a)),
	PWM_DT_SPEC_GET(DT_ALIAS(pwm_led1g)),
	PWM_DT_SPEC_GET(DT_ALIAS(pwm_led1a)),
	PWM_DT_SPEC_GET(DT_ALIAS(pwm_led2g)),
	PWM_DT_SPEC_GET(DT_ALIAS(pwm_led2a)),
};

static const struct device *timer = DEVICE_DT_GET(DT_ALIAS(tc_4));

static struct led_behavior_t behavior;
/* This is a separate array because it's computed from the keyframes */
static uint32_t inv_period[MAX_BEHAVIOR_KEYS];

/**
 * This function assumes that the total number of fractional bits
 * between a and b is 32.
 */
static inline int32_t mult_and_shift(int32_t a, uint32_t b) {
	return (int32_t)(((int64_t)a * (int64_t)b) >> 32);
}

/**
 * Linearly interpolates between a (alpha = 0) and b (alpha = 1) where
 * alpha is a q31 (0.31 fixed-point).
 */
static inline uint32_t interp(uint32_t a, uint32_t b, uint32_t alpha) {
	/* a*(1-alpha) + b*alpha = a - a*alpha + b*alpha = alpha*(b-a) + a */
	uint32_t x = mult_and_shift(b - a, alpha);
	//LOG_DBG("interp: a %08x b %08x x %08x ret %08x", a, b, x, a + x);
	return a + x;
}

void timer_callback(const struct device *dev, void *unused) {
	int i;
	int ret;
	int64_t now = k_uptime_get();

	if (behavior.start_time == 0 || behavior.start_time > now) {
		/* Set all to zero */
		for (i=0; i < N_LEDS; i++) {
			ret = pwm_set_pulse_dt(&pwm_channels[i], 0);
			__ASSERT(ret, "Failed to set PWM pulse width");
		}
		counter_stop(timer);
	}

	uint32_t delta = (uint32_t)(now - behavior.start_time);
	struct led_behavior_key_t *last_key = &behavior.keys[behavior.n_keys - 1];
	if (delta >= last_key->offset_ms) {
		/* We're off the end of the sequence. Repeat forward. */
		uint32_t count = (delta / last_key->offset_ms);
		uint32_t offset = count * last_key->offset_ms;
		LOG_DBG("Advancing sequence forward %d cycles (%d ms)", count, offset);
		/* Increment start time for the next update */
		behavior.start_time += offset;
		delta -= offset;
	}

	int start_idx = -1;
	/* Find the first index in the past */
	for (i=1; i < behavior.n_keys; i++) {
		if (delta < behavior.keys[i].offset_ms) {
			start_idx = i - 1;
			break;
		}
	}

	if (start_idx < 0) {
		/* The first offset should always be zero so we should *always* be
		 * after the first key frame
		 */
		LOG_ERR("Could not determine start index for delta %d ms idx %d, %d keys", delta, start_idx, behavior.n_keys);
		counter_stop(timer);
	}
	__ASSERT(start_idx < behavior.n_keys, "Start key should never be last key frame");

	/* TODO: intentionally not optimizing any of this yet */
	/* Probably need to move this to a delayed work item as this callback is in an ISR */
	uint32_t alpha = (delta - behavior.keys[start_idx].offset_ms) * inv_period[start_idx];

	LOG_DBG("start %d delta %08x alpha %08x inv %08x", start_idx, delta, alpha, inv_period[start_idx]);

	/* For each channel, interpolate between the past and future key frames */
	struct led_behavior_key_t *a = &behavior.keys[start_idx];
	struct led_behavior_key_t *b = &behavior.keys[start_idx + 1];

	for (i=0; i < N_LEDS; i++) {
		uint32_t val = interp(a->width_ms[i], b->width_ms[i], alpha);
		if (i == 0) {
			LOG_DBG("a %08x b %08x val %08x", a->width_ms[i], b->width_ms[i], val);
		}
		ret = pwm_set_pulse_dt(&pwm_channels[i], val);
		if (ret < 0) {
			LOG_ERR("Failed to set channel %d pulse width %d: %d", i, val, ret);
			counter_stop(timer);
		}
	}
}

int pwm_set_behavior(const struct led_behavior_t *beh) {
	__ASSERT(beh->keys[0].offset_ms == 0, "First key must start at zero");
	memcpy(&behavior, beh, sizeof(struct led_behavior_t));

	uint32_t last_offset_ms = 0;
	for (int i=1; i < behavior.n_keys; i++) {
		/* Pre-calculate inverse period */
		uint32_t this_offset_ms = behavior.keys[i].offset_ms;
		uint32_t period = this_offset_ms - last_offset_ms;
		inv_period[i-1] = (uint32_t)((1ULL << 32) / period);
		last_offset_ms = this_offset_ms;
	}

	behavior.start_time = k_uptime_get();
	int ret = counter_start(timer);
	if (ret < 0) {
		LOG_ERR("Failed to start timer: %d", ret);
		return ret;
	}

	return 0;
}

int pwm_behavior_off(void) {
	counter_stop(timer);
	behavior.start_time = 0;
	behavior.n_keys = 0;

	return 0;
}

static struct counter_top_cfg timer_cfg = {
	.callback = timer_callback,
	.flags = COUNTER_TOP_CFG_RESET_WHEN_LATE,
	.user_data = NULL,
};

int pwm_init(void) {
	if (!device_is_ready(timer)) {
		LOG_ERR("Timer device %s is not ready", timer->name);
	}

	timer_cfg.ticks = counter_us_to_ticks(timer, PWM_UPDATE_INTERVAL_US);
	LOG_DBG("counter set for %d ticks", timer_cfg.ticks);
	int ret = counter_set_top_value(timer, &timer_cfg);
	if (ret != 0) {
		LOG_ERR("Could not set counter top: %d", ret);
	}
	pwm_behavior_off();

	for (size_t i = 0U; i < N_LEDS; i++) {
		if (!pwm_is_ready_dt(&pwm_channels[i])) {
			LOG_ERR("PWM device %d %s is not ready: %d\n",
				i, pwm_channels[i].dev->name, ret);
			return -1;
		}

		ret = pwm_set_pulse_dt(&pwm_channels[i], 0);
		if (ret) {
			LOG_ERR("failed to clear channel %d: %d\n", i, ret);
			return ret;
		}
	}

	return 0;
}

int pwm_set2(uint16_t channel, uint32_t pulse_width) {
	if (channel >= N_LEDS) {
		return -EINVAL;
	}

	int ret = pwm_set_pulse_dt(&pwm_channels[channel], pulse_width);
	if (ret) {
		LOG_ERR("failed to set pulse width %d ns on channel %d: %d\n", pulse_width, channel, ret);
		return ret;
	}

	return 0;
}