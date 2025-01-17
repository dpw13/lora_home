#ifndef __PWM_H__
#define __PWM_H__

#include <stdint.h>

int pwm_init(void);
int pwm_set2(uint16_t channel, uint32_t pulse_width);

#endif /* __PWM_H__ */