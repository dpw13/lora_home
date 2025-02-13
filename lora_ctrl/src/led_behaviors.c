#include "pwm.h"
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_DECLARE(pwm, LOG_LEVEL_INF);

/* TODO: the first offset is always zero and the last width array
 * always matches the first. Reduce the number of vectors stored.
 */
const struct led_behavior_t beh_cylon = {
        .n_keys = 5,
        .keys = {
                { .offset_ms =    0, .width_ms = { DUTY_PCT(50), 0, 0, 0, 0, 0 } },
                { .offset_ms =  500, .width_ms = { 0, 0, DUTY_PCT(50), 0, 0, 0 } },
                { .offset_ms = 1000, .width_ms = { 0, 0, 0, 0, DUTY_PCT(50), 0 } },
                { .offset_ms = 1500, .width_ms = { 0, 0, DUTY_PCT(50), 0, 0, 0 } },
                { .offset_ms = 2000, .width_ms = { DUTY_PCT(50), 0, 0, 0, 0, 0 } },
        }
};

const struct led_behavior_t beh_colors = {
        .n_keys = 4,
        .keys = {
                { .offset_ms =    0, .width_ms = { DUTY_PCT(50), 0, 0, 0, 0, DUTY_PCT(50) } },
                { .offset_ms =  500, .width_ms = { 0, DUTY_PCT(50), DUTY_PCT(50), 0, 0, 0 } },
                { .offset_ms = 1000, .width_ms = { 0, 0, 0, DUTY_PCT(50), DUTY_PCT(50), 0 } },
                { .offset_ms = 1500, .width_ms = { DUTY_PCT(50), 0, 0, 0, 0, DUTY_PCT(50) } },
        }
};

struct led_behavior_t beh_bar = {
        .n_keys = 5,
        .keys = {
                { .offset_ms =    0, .width_ms = { 0, 0, 0, 0, 0, 0 } },
                { .offset_ms =  500, .width_ms = { 0, 0, 0, 0, 0, 0 } },
                { .offset_ms = 1000, .width_ms = { 0, 0, 0, 0, 0, 0 } },
                { .offset_ms = 1500, .width_ms = { 0, 0, 0, 0, 0, 0 } },
                { .offset_ms = 2000, .width_ms = { 0, 0, 0, 0, 0, 0 } },
        }
};

void set_bar_value(int idx, uint8_t val) {
        /* Number of LEDs lit in 8.8 FXP format (range 1 to N_LEDS/2) */
        uint16_t n_leds = (1 << 8) + (uint16_t)val * (N_LEDS/2 - 1);
        uint8_t n_leds_int = (uint8_t)(n_leds >> 8);
        uint8_t n_leds_frac = (uint8_t)(n_leds & 0xFF);
        uint32_t duty_g = DUTY_ONE_255 * (uint32_t)val;
        uint32_t duty_r = DUTY_ONE_255 * (255 - (uint32_t)val);
        uint32_t *width_vec = &beh_bar.keys[idx].width_ms[0];

        LOG_DBG("n_leds %x.%02x g %d r %d", n_leds_int, n_leds_frac, duty_g, duty_r);

        for (int i=0; i < N_LEDS/2; i++) {
                if (i < n_leds_int) {
                        LOG_DBG("LED %d intensity 255", i);
                        /* Lower LEDs are fully on in the specified color */
                        width_vec[GREEN_CHAN(i)] = duty_g;
                        width_vec[RED_CHAN(i)] = duty_r;
                } else if (i == n_leds_int) {
                        /* Top LED intensity varies */
                        LOG_DBG("LED %d intensity %d", i, n_leds_frac);
                        width_vec[GREEN_CHAN(i)] = (duty_g * n_leds_frac) >> 8;
                        width_vec[RED_CHAN(i)] = (duty_r * n_leds_frac) >> 8;
                } else {
                        /* Upper LEDs are off */
                        LOG_DBG("LED %d intensity 0", i);
                        width_vec[GREEN_CHAN(i)] = 0;
                        width_vec[RED_CHAN(i)] = 0;
                }
        }
}

/**
 * Renders a flashing bar that goes from the min to max value. Zero renders as
 * one solid red LED, 255 renders as three solid green LEDs.
 */
void display_bar(uint32_t period, uint8_t min, uint8_t max) {
        set_bar_value(0, min);
        set_bar_value(1, max);
        memcpy(&beh_bar.keys[2].width_ms[0], &beh_bar.keys[0].width_ms[0], N_LEDS*sizeof(uint32_t));
        beh_bar.n_keys = 3;
        for (int i=0; i < beh_bar.n_keys; i++) {
                beh_bar.keys[i].offset_ms = period * i;
        }
        pwm_set_behavior(&beh_bar);
}

void test_bar(uint32_t period) {
        set_bar_value(0, 0);
        set_bar_value(1, 128);
        set_bar_value(2, 255);
        set_bar_value(3, 128);
        memcpy(&beh_bar.keys[4].width_ms[0], &beh_bar.keys[0].width_ms[0], N_LEDS*sizeof(uint32_t));
        beh_bar.n_keys = 5;
        for (int i=0; i < beh_bar.n_keys; i++) {
                beh_bar.keys[i].offset_ms = period * i;
        }
        pwm_set_behavior(&beh_bar);
}