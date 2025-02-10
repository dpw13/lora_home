#ifndef __PWM_H__
#define __PWM_H__

#include <stdint.h>

#define N_LEDS	1

/* Change this if base PWM period changes */
#define DUTY_ONE_PCT    (5000000/100)
#define DUTY_PCT(pct)   (pct*DUTY_ONE_PCT)

int pwm_init(void);
int pwm_set2(uint16_t channel, uint32_t pulse_width);

#endif /* __PWM_H__ */