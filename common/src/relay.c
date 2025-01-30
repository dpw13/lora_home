#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/lorawan/lorawan.h>
#include <services/lorawan_services.h>

LOG_MODULE_REGISTER(relay, CONFIG_LORAWAN_SERVICES_LOG_LEVEL);

static const struct gpio_dt_spec relays[] = {
#if DT_NODE_EXISTS(DT_ALIAS(relay0))
	GPIO_DT_SPEC_GET(DT_ALIAS(relay0), gpios),
#if DT_NODE_EXISTS(DT_ALIAS(relay1))
	GPIO_DT_SPEC_GET(DT_ALIAS(relay1), gpios),
#endif
#endif
};

struct relay_svc_context {
	/** Work item for regular uplink messages */
	struct k_work_delayable relay_work;
	/**
	 * Keepalive retransmission interval in milliseconds
	 */
	uint32_t period;
	/**
	 * The time (possibly in the past) when the relay should next close.
	 */
	int64_t close_timeout;
	/**
	 * The current (software) state of the relays
	 */
	uint8_t relay_state;
};

struct relay_msg_t {
	uint32_t open_duration_ms;
};

K_SEM_DEFINE(ctx_sem, 1, 1);
static struct relay_svc_context ctx;

static void downlink_info(uint8_t port, uint8_t flags, int16_t rssi, int8_t snr, uint8_t len,
			  const uint8_t *data)
{
	LOG_INF("Received from port %d, flags %d, RSSI %ddB, SNR %ddB", port, flags, rssi, snr);
	if (data && len >= sizeof(struct relay_msg_t)) {
		const struct relay_msg_t *msg = (struct relay_msg_t *)data;

		LOG_INF("Relay opened for %d ms", msg->open_duration_ms);

		k_sem_take(&ctx_sem, K_FOREVER);
		if (msg->open_duration_ms == __UINT32_MAX__) {
			/* Leave open until further notice */
			ctx.close_timeout = ~0UL;
		} else {
			ctx.close_timeout = k_uptime_ticks() + k_ms_to_ticks_ceil64(msg->open_duration_ms);
		}
		k_sem_give(&ctx_sem);
	}
}

/* This callback must have a static lifetime */
static const struct lorawan_downlink_cb downlink_cb[] = {
	{
		.port = CONFIG_LORAWAN_PORT_RELAY_BASE+0,
		.cb = downlink_info
	}, {
		.port = CONFIG_LORAWAN_PORT_RELAY_BASE+1,
		.cb = downlink_info
	},
};

static void relay_work_handler(struct k_work *work) {
	uint8_t new_state;
	int i;

	k_sem_take(&ctx_sem, K_FOREVER);
	/* Relay is active if we have not met the timeout */
	/* TODO: wrong, need to set all bits */
	new_state = (k_uptime_ticks() < ctx.close_timeout);
	if (new_state != ctx.relay_state) {
		ctx.relay_state = 0;
		LOG_INF("Relay state %d", new_state);
		for (i=0; i<sizeof(relays); i++) {
			gpio_pin_set_dt(&relays[i], new_state);
			if (new_state) {
				ctx.relay_state |= BIT(i);
			}
		}
	}

	for (i=0; i<sizeof(relays); i++) {
		lorawan_services_schedule_uplink(CONFIG_LORAWAN_PORT_RELAY_BASE + i, &ctx.relay_state, sizeof(ctx.relay_state), 500);
	}
	lorawan_services_reschedule_work(&ctx.relay_work, K_MSEC(ctx.period));
	k_sem_give(&ctx_sem);
}

int lorawan_relay_run(void) {
	ctx.period = 30000;
	ctx.relay_state = 0;

	for (int i=0; i<sizeof(relays); i++) {
		gpio_pin_configure_dt(&relays[i], GPIO_OUTPUT_LOW);
		lorawan_register_downlink_callback(&downlink_cb[i]);
	}

	k_work_init_delayable(&ctx.relay_work, relay_work_handler);
	lorawan_services_reschedule_work(&ctx.relay_work, K_NO_WAIT);

        return 0;
}