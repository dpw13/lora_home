#ifndef __PWM_H__
#define __PWM_H__

#include <stdint.h>

#define GREEN_CHAN(btn) (2*btn+0)
#define RED_CHAN(btn)   (2*btn+1)

int pwm_init(void);
int pwm_set2(uint16_t channel, uint32_t pulse_width);

#endif /* __PWM_H__ */