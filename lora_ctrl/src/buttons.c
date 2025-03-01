#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <inttypes.h>

#include "pm.h"
#include "buttons.h"

LOG_MODULE_REGISTER(buttons, LOG_LEVEL_INF);

static const struct gpio_dt_spec buttons[] = {
        GPIO_DT_SPEC_GET(DT_ALIAS(btn_gate), gpios),
        GPIO_DT_SPEC_GET(DT_ALIAS(btn_garage0), gpios),
        GPIO_DT_SPEC_GET(DT_ALIAS(btn_garage1), gpios),
};

struct gpio_context_t {
	int64_t press_ticks;
	struct k_timer exp_timer;
	uint8_t action;
};

static struct gpio_context_t ctx[ARRAY_SIZE(buttons)] = {0};

K_MSGQ_DEFINE(action_queue, sizeof(struct action_t), 4, 1);

static void exp_handler(struct k_timer *timer) {
	struct gpio_context_t *pctx = k_timer_user_data_get(timer);
	/* Position of context tells us the button index without us having to store it */
	uint8_t i = pctx - &ctx[0];

	k_timer_stop(timer);
	pm_sysclock_allow_idle();

	/* No further action within the timeout period: submit actions */
	struct action_t act = {
		.btn_id = i,
		.action = pctx->action,
	};
	LOG_INF("Button %d action %02x", i, pctx->action);
	k_msgq_put(&action_queue, &act, K_FOREVER);

	/* Reset context */
	pctx->press_ticks = 0;
	pctx->action = 0;
}

static struct gpio_callback gpio_callback;

static inline void on_press(uint8_t i, int64_t now) {
	ctx[i].press_ticks = now;
	/* TODO: long press timeout? */
	k_timer_stop(&ctx[i].exp_timer);
	/* Ensure that sysclock is not stopped so we can measure
	* the duration of the press.
	*/
	pm_sysclock_force_active();
}

static inline void on_release(uint8_t i, int64_t now) {
	int64_t dur = now - ctx[i].press_ticks;
	ctx[i].press_ticks = 0;
	LOG_DBG("Button %d press %lld ms", i, dur);
	if (dur > GLITCH_THRESH_MS) {
		/* Ignore glitches entirely */
		ctx[i].action = (ctx[i].action << BTN_ACTION_SIZE) |
			((dur < SHORT_PRESS_THRESH_MS) ? BTN_ACTION_SHORT : BTN_ACTION_LONG) |
			BTN_ACTION_VALID;
	}
	/* Wait to see if we get any further presses. The period is
	 * forever to make this is a one-shot.
	 */
	LOG_DBG("Starting timer %p", &ctx[i].exp_timer);
	k_timer_start(&ctx[i].exp_timer, K_MSEC(NEXT_PRESS_TIMEOUT_MS), K_FOREVER);
}

static void button_irq_callback(const struct device *dev,
				struct gpio_callback *cb, uint32_t pins)
{
	int64_t now = k_uptime_get();

	for (int i=0; i < ARRAY_SIZE(buttons); i++) {
		if (IS_BIT_SET(pins, buttons[i].pin)) {
			/* State change on button i. If button was previously
			 * pressed, the `press_ticks` field will be non-zero.
			 */
			uint8_t state = gpio_pin_get_dt(&buttons[i]);
			LOG_DBG("Pin %d state %d", i, state);
			if ((ctx[i].press_ticks == 0) != (state == 0)) {
				if (state) {
					/* Press */
					on_press(i, now);
				} else {
					/* Release */
					on_release(i, now);
				}
			}
			/* Clear bit */
			WRITE_BIT(pins, buttons[i].pin, 0);
		}
	}
}

int button_init(void)
{
	int ret;
	int i;
	gpio_port_pins_t mask = 0;
	int64_t now = k_uptime_get();

        for (i=0; i < ARRAY_SIZE(buttons); i++) {
		if (i > 0 && buttons[i].port != buttons[0].port) {
			LOG_ERR("All buttons must be on the same port!");
			return -EINVAL;
		}

		if (!gpio_is_ready_dt(&buttons[i])) {
			LOG_ERR("button device %s is not ready",
			buttons[i].port->name);
			return -ENOENT;
		}

		/* Initialize context */
		k_timer_init(&ctx[i].exp_timer, exp_handler, NULL);
		k_timer_user_data_set(&ctx[i].exp_timer, &ctx[i]);

		ret = gpio_pin_configure_dt(&buttons[i], GPIO_INPUT);
		if (ret != 0) {
			LOG_ERR("failed to configure %s pin %d: %d",
			buttons[i].port->name, buttons[i].pin, ret);
			return ret;
		}

		if(gpio_pin_get_dt(&buttons[i]) != 0) {
			/* Button still pressed from boot. Mark the press time as
			 * early as we can.
			 */
			on_press(i, now);
		}

		ret = gpio_pin_interrupt_configure_dt(&buttons[i], GPIO_INT_EDGE_BOTH);
		if (ret != 0) {
			LOG_ERR("failed to configure interrupt on %s pin %d: %d",
			buttons[i].port->name, buttons[i].pin, ret);
			return ret;
		}

		mask |= BIT(buttons[i].pin);
	}

	LOG_DBG("Init GPIO callback with mask %08x", mask);
	gpio_init_callback(&gpio_callback,
			button_irq_callback,
			mask);

	ret = gpio_add_callback(buttons[0].port, &gpio_callback);
	if (ret < 0) {
		LOG_ERR("Could not set gpio callback.");
		return -EINVAL;
	}

	return 0;
}

uint8_t button_poll(void) {
	uint8_t val = 0;

	for (int i=0; i < ARRAY_SIZE(buttons); i++) {
		val = (val << 1) | (uint8_t)gpio_pin_get_dt(&buttons[i]);
	}

	return val;
}
