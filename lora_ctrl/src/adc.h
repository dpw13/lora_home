#ifndef __ADC_H__
#define __ADC_H__

#include <stdint.h>

int adc_init(void);
/* Returns the battery voltage in mV. */
uint16_t adc_read_battery(void);
/* Returns the measured temperature in units of 0.1 deg C */
int16_t adc_read_temp(void);

#endif /* __ADC_H__ */