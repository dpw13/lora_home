#ifndef __BUTTONS_H__
#define __BUTTONS_H__

#include <stdint.h>

#define BTN_ACTION_SIZE 2
#define BTN_ACTION_VALID (1 << 1)
#define BTN_ACTION_SHORT (0 << 0)
#define BTN_ACTION_LONG  (1 << 0)

#define BTN_ACT_1_SHORT  (BTN_ACTION_VALID | BTN_ACTION_SHORT)
#define BTN_ACT_1_LONG   (BTN_ACTION_VALID | BTN_ACTION_LONG)

#define GLITCH_THRESH_MS          5
#define SHORT_PRESS_THRESH_MS   200
#define NEXT_PRESS_TIMEOUT_MS   1000

/* Button action queue */
struct action_t {
	uint8_t btn_id;
	uint8_t action;
};
extern struct k_msgq action_queue;

int button_init(void);
uint8_t button_poll(void);

#endif