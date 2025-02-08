#include "pwm.h"

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
                { .offset_ms =  500, .width_ms = { 0, DUTY_PCT(50), DUTY_PCT(50), 0, 0 } },
                { .offset_ms = 1000, .width_ms = { 0, 0, 0, DUTY_PCT(50), DUTY_PCT(50), 0 } },
                { .offset_ms = 1500, .width_ms = { DUTY_PCT(50), 0, 0, 0, 0, DUTY_PCT(50) } },
        }
};
