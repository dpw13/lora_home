#ifndef __PWM_H__
#define __PWM_H__

#include <stdint.h>

#define N_LEDS	6

#define GREEN_CHAN(btn) (2*(btn)+0)
#define RED_CHAN(btn)   (2*(btn)+1)

/* Change this if base PWM period changes */
#define PWM_PERIOD_NS	5000000

#define DUTY_PCT(pct)   ((uint16_t)(pct) << 8)

#define PWM_UPDATE_INTERVAL_US  50000

#define MAX_BEHAVIOR_KEYS	8

/* Preset behaviors */
extern const struct led_behavior_t beh_cylon;
extern const struct led_behavior_t beh_colors;

struct led_behavior_key_t {
	uint32_t offset_ms;
	uint16_t width_pct[N_LEDS];
};

struct led_behavior_t {
	int64_t start_time;
	struct led_behavior_key_t keys[MAX_BEHAVIOR_KEYS];
	int n_keys;
};

void display_bar(uint32_t period, uint8_t min, uint8_t max);
void test_bar(uint32_t period);

int pwm_behavior_off(void);
int pwm_set_behavior(const struct led_behavior_t *beh);

int pwm_init(void);
int led_set_intensity(uint16_t channel, uint8_t pct);

#endif /* __PWM_H__ */