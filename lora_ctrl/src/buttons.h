#ifndef __BUTTONS_H__
#define __BUTTONS_H__

#include <stdint.h>

int button_init(void);
uint8_t button_poll(void);

#endif