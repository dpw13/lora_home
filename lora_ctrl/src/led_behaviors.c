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
                { .offset_ms =    0, .width_pct = { DUTY_PCT(50), 0, 0, 0, 0, 0 } },
                { .offset_ms =  500, .width_pct = { 0, 0, DUTY_PCT(50), 0, 0, 0 } },
                { .offset_ms = 1000, .width_pct = { 0, 0, 0, 0, DUTY_PCT(50), 0 } },
                { .offset_ms = 1500, .width_pct = { 0, 0, DUTY_PCT(50), 0, 0, 0 } },
                { .offset_ms = 2000, .width_pct = { DUTY_PCT(50), 0, 0, 0, 0, 0 } },
        }
};

const struct led_behavior_t beh_colors = {
        .n_keys = 4,
        .keys = {
                { .offset_ms =    0, .width_pct = { DUTY_PCT(50), 0, 0, 0, 0, DUTY_PCT(50) } },
                { .offset_ms =  500, .width_pct = { 0, DUTY_PCT(50), DUTY_PCT(50), 0, 0, 0 } },
                { .offset_ms = 1000, .width_pct = { 0, 0, 0, DUTY_PCT(50), DUTY_PCT(50), 0 } },
                { .offset_ms = 1500, .width_pct = { DUTY_PCT(50), 0, 0, 0, 0, DUTY_PCT(50) } },
        }
};

struct led_behavior_t beh_bar = {
        .n_keys = 5,
        .keys = {
                { .offset_ms =    0, .width_pct = { 0, 0, 0, 0, 0, 0 } },
                { .offset_ms =  500, .width_pct = { 0, 0, 0, 0, 0, 0 } },
                { .offset_ms = 1000, .width_pct = { 0, 0, 0, 0, 0, 0 } },
                { .offset_ms = 1500, .width_pct = { 0, 0, 0, 0, 0, 0 } },
                { .offset_ms = 2000, .width_pct = { 0, 0, 0, 0, 0, 0 } },
        }
};

#define LED_SCALE_CNT   (N_LEDS/2 - 1)
/* Scaler for LED count computation, 8.24 FXP */
#define LED_SCALE       ((LED_SCALE_CNT << 24)/100)
/**
 * Scale the percent (0-100 in 8.0 format) to the fractional number of
 * LEDs to illuminate (1.0-3.0 in 8.8 format).
 */
static inline uint16_t scale_pct_to_leds(uint8_t val) {
        /* 0 -> 0x0100 = 0x0100 + 0x00000
         * 100 -> 0x0300 = 0x0100 + 0x0200
         * tmp = 1 + val * led_channels / 100 (8.24 FXP)
         * return fxp8_8(tmp) // round and shift
         */
        uint32_t tmp = val * LED_SCALE + 0x01008000;
        return (uint16_t)(tmp >> 16);
}

static void set_bar_value(int idx, uint8_t val) {
        /* Number of LEDs lit in 8.8 FXP format (range 1 to N_LEDS/2) */
        uint16_t n_leds = scale_pct_to_leds(val);
        uint8_t n_leds_int = (uint8_t)(n_leds >> 8);
        uint8_t n_leds_frac = (uint8_t)(n_leds & 0xFF);
        uint16_t pct_g = DUTY_PCT(val);
        uint16_t pct_r = DUTY_PCT(100 - (uint32_t)val);
        uint16_t *width_vec = &beh_bar.keys[idx].width_pct[0];

        LOG_DBG("n_leds %x.%02x g %d r %d", n_leds_int, n_leds_frac, pct_g, pct_r);

        for (int i=0; i < N_LEDS/2; i++) {
                if (i < n_leds_int) {
                        LOG_DBG("LED %d intensity 255", i);
                        /* Lower LEDs are fully on in the specified color */
                        width_vec[GREEN_CHAN(i)] = pct_g;
                        width_vec[RED_CHAN(i)] = pct_r;
                } else if (i == n_leds_int) {
                        /* Top LED intensity varies */
                        LOG_DBG("LED %d intensity %d", i, n_leds_frac);
                        width_vec[GREEN_CHAN(i)] = (uint16_t)(((uint32_t)pct_g * n_leds_frac) >> 8);
                        width_vec[RED_CHAN(i)] = (uint16_t)(((uint32_t)pct_r * n_leds_frac) >> 8);
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
 * one solid red LED, 100 renders as three solid green LEDs.
 */
void display_bar(uint32_t period, uint8_t min, uint8_t max) {
        set_bar_value(0, min);
        set_bar_value(1, max);
        memcpy(&beh_bar.keys[2].width_pct[0], &beh_bar.keys[0].width_pct[0], N_LEDS*sizeof(uint32_t));
        beh_bar.n_keys = 3;
        for (int i=0; i < beh_bar.n_keys; i++) {
                beh_bar.keys[i].offset_ms = period * i;
        }
        pwm_set_behavior(&beh_bar);
}

void test_bar(uint32_t period) {
        set_bar_value(0, 0);
        set_bar_value(1, 50);
        set_bar_value(2, 100);
        set_bar_value(3, 50);
        memcpy(&beh_bar.keys[4].width_pct[0], &beh_bar.keys[0].width_pct[0], N_LEDS*sizeof(uint32_t));
        beh_bar.n_keys = 5;
        for (int i=0; i < beh_bar.n_keys; i++) {
                beh_bar.keys[i].offset_ms = period * i;
        }
        pwm_set_behavior(&beh_bar);
}