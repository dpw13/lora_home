/**
 * Proprietary local LoRa protocol numbers
 */

#ifndef __LORA_APP_PROTOCOL__
#define __LORA_APP_PROTOCOL__

#include <stdint.h>

/**
 * LoRa proprietary MHDR
 */
#define LORA_MHDR_PROPRIETARY   0xE0

struct lora_prop_uplink_t {
	uint8_t mhdr;
	uint8_t type;
	uint16_t battery_lvl;
};

struct lora_prop_downlink_t {
	uint8_t mhdr;
	uint8_t type;
};

/* Communication to/from 3-button remote */
#define LORA_PROP_TYPE_REMOTE	0x01

enum remote_cmd_t {
	BTN_A_SINGLE = 0x10,	/*< Short single press */
	BTN_A_DBL,		/*< Short double press */
	BTN_A_HOLD,		/*< Long press */

	BTN_B_SINGLE = 0x18,
	BTN_B_DBL,
	BTN_B_HOLD,

	BTN_C_SINGLE = 0x18,
	BTN_C_DBL,
	BTN_C_HOLD,
};

struct lora_remote_uplink_t {
        struct lora_prop_uplink_t hdr;
        uint8_t cmd;
};

struct lora_remote_downlink_t {
        struct lora_prop_downlink_t hdr;
        uint8_t payload;
};

/**
 * LoRaWAN port IDs
 */
#define LORAWAN_PORT_GATE_RELAY		0x80
#define LORAWAN_PORT_GARAGE0_RELAY	0x81
#define LORAWAN_PORT_GARAGE1_RELAY	0x82

enum entrance_state_t {
	DOOR_STATE_UNKNOWN = 0,
	DOOR_STATE_CLOSED,
	DOOR_STATE_MOVING,
	DOOR_STATE_MOM_OPEN,  /*< Momentary open */
	DOOR_STATE_HOLD_OPEN,
};

enum entrance_cmd_t {
	DOOR_CMD_TOGGLE = 1,
	DOOR_CMD_CLOSE,
	DOOR_CMD_MOM_OPEN,
	DOOR_CMD_HOLD_OPEN,
};

struct lorawan_entr_uplink_t {
	uint8_t state;
};

struct lorawan_entr_downlink_t {
	uint8_t cmd;
};

/* TODO: unimplemented */
#define LORAWAN_PORT_GATE_SCHED		0x90

/**
 * Schedule a time to hold the gate open. Only one pending
 * hold is stored, so sending this message again with values
 * less than the current time cancels the hold.
 */
struct lorawan_entr_sched_downlink_t {
	uint64_t hold_start;		/*< Hold start in 64-bit unix epoch */
	uint32_t hold_duration_s;	/*< Hold duration in seconds */
};

struct lorawan_entr_sched_uplink_t {
	/* TODO, confirmation? */
};

#endif