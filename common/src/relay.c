#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/lorawan/lorawan.h>
#include <services/lorawan_services.h>

#include "app_protocol.h"

LOG_MODULE_REGISTER(relay, LOG_LEVEL_DBG);

static const struct gpio_dt_spec relays[] = {
#if DT_NODE_EXISTS(DT_ALIAS(relay0))
	GPIO_DT_SPEC_GET(DT_ALIAS(relay0), gpios),
#if DT_NODE_EXISTS(DT_ALIAS(relay1))
	GPIO_DT_SPEC_GET(DT_ALIAS(relay1), gpios),
#endif
#endif
};

struct relay_svc_context {
	/** The relay GPIO */
	const struct gpio_dt_spec *relay;
	/** Work item for regular uplink messages */
	struct k_work_delayable uplink_work;
	/** Work item for maintaining the entrance state */
	struct k_work_delayable entr_state_work;
	/** Work item for managing relay close duration */
	struct k_work_delayable open_relay_work;
	/** Keepalive retransmission interval in milliseconds */
	uint32_t period;
	/** The next time in ticks that the entrance is expected to change state. */
	int64_t entr_transition;
	/** The current (software) state of the relays */
	enum entrance_state_t entr_state;
	/** The next (software) state of the relays */
	enum entrance_state_t nx_entr_state;
	/** The LoRaWAN port number */
	uint8_t port;
};

K_SEM_DEFINE(ctx_sem, 1, 1);
static struct relay_svc_context ctx[ARRAY_SIZE(relays)];

#define RELAY_CLOSED	1
#define RELAY_OPEN	0

#define MOVEMENT_TICKS	k_sec_to_ticks_ceil64(CONFIG_ENTRANCE_MOVEMENT_DURATION)
#ifdef CONFIG_ENTRANCE_HAS_AUTO_CLOSE
#define HAS_AUTO_CLOSE	1
#define AUTOCLOSE_TICKS	k_sec_to_ticks_ceil64(CONFIG_ENTRANCE_AUTO_CLOSE_INTERVAL)
#else
#define HAS_AUTO_CLOSE	0
#define AUTOCLOSE_TICKS	0
#endif

static void close_relay(struct relay_svc_context *ctx, uint32_t duration_ms) {
	struct k_work_sync sync;

	/* Cancel work if it exists */
	k_work_cancel_delayable_sync(&ctx->open_relay_work, &sync);
	gpio_pin_set_dt(ctx->relay, 1);
	if (duration_ms > 0) {
		/* Duration of 0 means leave closed */
		LOG_DBG("Closing relay for %d ms", duration_ms);
		k_work_schedule_for_queue(&k_sys_work_q, &ctx->open_relay_work, K_MSEC(duration_ms));
	} else {
		LOG_DBG("Holding relay closed");
	}
}

/**
 * Open the relay. This is mostly used to release the relay if the relay is being held closed.
 * Must not be called from the open relay handler as it cancels the work.
 */
static void release_relay(struct relay_svc_context *ctx) {
	/* We don't care that the open relay handler may be running; in either case the
	 * relay will be released. */
	LOG_DBG("Releasing relay");
	k_work_cancel_delayable(&ctx->open_relay_work);
	gpio_pin_set_dt(ctx->relay, 1);
}

static void open_relay_handler(struct k_work *work) {
	const struct k_work_delayable *open_relay_work = k_work_delayable_from_work(work);
	struct relay_svc_context *ctx = CONTAINER_OF(open_relay_work, struct relay_svc_context, open_relay_work);

	LOG_DBG("Opening relay");
	gpio_pin_set_dt(ctx->relay, 0);
}

static void command_state(struct relay_svc_context *ctx, enum entrance_cmd_t cmd) {
	k_sem_take(&ctx_sem, K_FOREVER);

	int64_t now = k_uptime_ticks();

	switch (ctx->entr_state) {
		case DOOR_STATE_CLOSED:
			switch (cmd) {
				case DOOR_CMD_TOGGLE:
				case DOOR_CMD_MOM_OPEN:
					close_relay(ctx, CONFIG_ENTRANCE_RELAY_MOMENTARY_TIME_MS);
					ctx->entr_state = DOOR_STATE_MOVING;
					if (HAS_AUTO_CLOSE) {
						ctx->nx_entr_state = DOOR_STATE_MOM_OPEN;
					} else {
						/* With no auto close a momentary press always holds the door open */
						ctx->nx_entr_state = DOOR_STATE_HOLD_OPEN;
					}
					ctx->entr_transition = now + MOVEMENT_TICKS;
					break;
				case DOOR_CMD_CLOSE:
					/* Do nothing */
					break;
				case DOOR_CMD_HOLD_OPEN:
					if (HAS_AUTO_CLOSE) {
						close_relay(ctx, 0);
					} else {
						/* With no auto close hold open is just a momentary press */
						close_relay(ctx, CONFIG_ENTRANCE_RELAY_MOMENTARY_TIME_MS);
					}
					ctx->entr_state = DOOR_STATE_MOVING;
					ctx->nx_entr_state = DOOR_STATE_HOLD_OPEN;
					ctx->entr_transition = now + MOVEMENT_TICKS;
					break;
				default:
					break;
			}
			break;
		case DOOR_STATE_HOLD_OPEN:
			switch (cmd) {
				case DOOR_CMD_TOGGLE:
				case DOOR_CMD_CLOSE:
					close_relay(ctx, CONFIG_ENTRANCE_RELAY_MOMENTARY_TIME_MS);
					ctx->entr_state = DOOR_STATE_MOVING;
					ctx->nx_entr_state = DOOR_STATE_CLOSED;
					ctx->entr_transition = now + MOVEMENT_TICKS;
					break;
				case DOOR_CMD_MOM_OPEN:
					if (HAS_AUTO_CLOSE) {
						release_relay(ctx);
						ctx->entr_state = DOOR_STATE_MOM_OPEN;
						ctx->nx_entr_state = DOOR_STATE_MOVING;
						ctx->entr_transition = now + AUTOCLOSE_TICKS;
					}
					/* Otherwise don't do anything */
					break;
				case DOOR_CMD_HOLD_OPEN:
					/* nothing */
					break;
			}
			break;
		case DOOR_STATE_MOM_OPEN:
			switch (cmd) {
				case DOOR_CMD_CLOSE:
				case DOOR_CMD_TOGGLE:
					close_relay(ctx, CONFIG_ENTRANCE_RELAY_MOMENTARY_TIME_MS);
					ctx->entr_state = DOOR_STATE_MOVING;
					ctx->nx_entr_state = DOOR_STATE_CLOSED;
					ctx->entr_transition = now + MOVEMENT_TICKS;
					break;
				case DOOR_CMD_HOLD_OPEN:
					if (HAS_AUTO_CLOSE) {
						/* Going from momentary open to hold open just involves keeping the
						 * relay closed in this case. Cancel further state transitions.
						 */
						close_relay(ctx, 0);
						ctx->entr_state = DOOR_STATE_HOLD_OPEN;
						ctx->nx_entr_state = DOOR_STATE_HOLD_OPEN;
						ctx->entr_transition = 0;
					}
					/* Without auto close, both open states are the same */
					break;
				case DOOR_CMD_MOM_OPEN:
					break;
			}
		case DOOR_STATE_MOVING:
			LOG_WRN("Command received while door is moving! Ignoring for now");
			/* TODO: handle canceling and resuming movement, etc */
			break;
		default:
			LOG_ERR("Invalid entrance state: %d", ctx->entr_state);
			goto err;
	}

	LOG_DBG("entrance command %d: state is now %d", cmd, ctx->entr_state);

	if (ctx->entr_state != ctx->nx_entr_state) {
		lorawan_services_reschedule_work(&ctx->entr_state_work, K_TIMEOUT_ABS_TICKS(ctx->entr_transition));
	}

err:
	k_sem_give(&ctx_sem);
}

static void downlink_info(uint8_t port, uint8_t flags, int16_t rssi, int8_t snr, uint8_t len,
			  const uint8_t *data, void *context)
{
	struct relay_svc_context *ctx = context;

	LOG_INF("Received from port %d, flags %d, RSSI %ddB, SNR %ddB", port, flags, rssi, snr);

	if (data && len >= sizeof(struct lorawan_entr_downlink_t)) {
		const struct lorawan_entr_downlink_t *msg = (struct lorawan_entr_downlink_t *)data;

		LOG_INF("Entrance command: %d", msg->cmd);
		command_state(ctx, msg->cmd);
	}
}

/* This callback must have a static lifetime */
static const struct lorawan_downlink_cb downlink_cb[] = {
	{
		.port = CONFIG_LORAWAN_PORT_RELAY_BASE+0,
		.cb = downlink_info,
		.ctx = &ctx[0],
	}, {
		.port = CONFIG_LORAWAN_PORT_RELAY_BASE+1,
		.cb = downlink_info,
		.ctx = &ctx[1],
	},
};

static void entr_state_work_handler(struct k_work *work) {
	const struct k_work_delayable *entr_state_work = k_work_delayable_from_work(work);
	struct relay_svc_context *ctx = CONTAINER_OF(entr_state_work, struct relay_svc_context, entr_state_work);
	uint64_t pending = 0;
	/* Default is to not create any further transitions */
	uint8_t nx_state = ctx->nx_entr_state;;

	k_sem_take(&ctx_sem, K_FOREVER);
	int64_t now = k_uptime_ticks();
	/* Process timed state transitions */

	switch (ctx->nx_entr_state) {
		case DOOR_STATE_MOVING:
			pending = now + MOVEMENT_TICKS;
			/* Where we end up depends on where we started */
			switch (ctx->entr_state) {
				case DOOR_STATE_CLOSED:
					nx_state = DOOR_STATE_HOLD_OPEN;
					break;
				case DOOR_STATE_MOM_OPEN:
				case DOOR_STATE_HOLD_OPEN:
					nx_state = DOOR_STATE_CLOSED;
					break;
				default:
					LOG_WRN("Unexpected movement origin state %d", ctx->entr_state);
			}
			break;
		case DOOR_STATE_MOM_OPEN:
			if (HAS_AUTO_CLOSE) {
				pending = now + AUTOCLOSE_TICKS;
				nx_state = DOOR_STATE_MOVING;
			}
			/* With no auto close, MOM_OPEN == HOLD_OPEN */
			break;
		default:
			/* Don't generate new pending transitions for any other cases */
			break;
	}
	ctx->entr_state = ctx->nx_entr_state;
	ctx->entr_transition = pending; // Zero if no new pending transitions
	if (pending > 0) {
		ctx->nx_entr_state = nx_state;
	}
	k_sem_give(&ctx_sem);

	LOG_DBG("entrance changed to state %d", ctx->entr_state);

	/* Schedule uplink of updated state */
	lorawan_services_reschedule_work(&ctx->uplink_work, K_NO_WAIT);
	/* If there are pending transitions, schedule them */
	if (pending > 0) {
		lorawan_services_reschedule_work(&ctx->entr_state_work, K_TIMEOUT_ABS_TICKS(pending));
	}
}

static void uplink_work_handler(struct k_work *work) {
	const struct k_work_delayable *uplink_work = k_work_delayable_from_work(work);
	struct relay_svc_context *ctx = CONTAINER_OF(uplink_work, struct relay_svc_context, uplink_work);
	struct lorawan_entr_uplink_t msg = {
		.state = ctx->entr_state,
	};

	lorawan_services_schedule_uplink(ctx->port, (uint8_t *)&msg, sizeof(struct lorawan_entr_uplink_t), 500);

	/* Reschedule periodic uplink */
	lorawan_services_reschedule_work(&ctx->uplink_work, K_MSEC(ctx->period));
	k_sem_give(&ctx_sem);
}

int lorawan_relay_run(void) {
	for (int i=0; i < ARRAY_SIZE(relays); i++) {
		ctx[i].period = 30000;
		ctx[i].entr_state = DOOR_STATE_CLOSED;
		ctx[i].nx_entr_state = DOOR_STATE_CLOSED;
		ctx[i].entr_transition = 0;
		ctx[i].relay = &relays[i];
		ctx[i].port = CONFIG_LORAWAN_PORT_RELAY_BASE + i;

		gpio_pin_configure_dt(&relays[i], GPIO_OUTPUT_LOW);
		lorawan_register_downlink_callback(&downlink_cb[i]);

		k_work_init_delayable(&ctx[i].entr_state_work, entr_state_work_handler);
		k_work_init_delayable(&ctx[i].uplink_work, uplink_work_handler);
		k_work_init_delayable(&ctx[i].open_relay_work, open_relay_handler);

		/* Send first uplink immediately */
		lorawan_services_reschedule_work(&ctx[i].uplink_work, K_NO_WAIT);
	}
	/* Switch to class C for lower-latency relay response */
	lorawan_services_class_c_start();

        return 0;
}