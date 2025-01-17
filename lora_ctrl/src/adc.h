#ifndef __ADC_H__
#define __ADC_H__

#include <stdint.h>

int adc_init(void);
uint16_t adc_read_battery(void);
uint16_t adc_read_temp(void);

#endif /* __ADC_H__ */