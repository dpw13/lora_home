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


#define LORA_PROP_TYPE_ACK	0x00
/* Communication to/from 3-button remote */
#define LORA_PROP_TYPE_REMOTE	0x01

struct lora_remote_uplink_t {
        struct lora_prop_uplink_t hdr;
        uint8_t btn;
        uint8_t action;
};

struct lora_remote_downlink_t {
        struct lora_prop_downlink_t hdr;
        uint8_t payload;
};

/**
 * LoRaWAN port IDs
 */

/* Charger messages. See renogy_internal.h */
#define LORAWAN_PORT_CHARGER_SYS	0x10
#define LORAWAN_PORT_CHARGER_BAT_PARAM	0x11
#define LORAWAN_PORT_CHARGER_DYN_STATUS	0x12
#define LORAWAN_PORT_CHARGER_STATS	0x13

/* System info */
#define LORAWAN_PORT_BOOT_STATUS	0x20

struct lorawan_boot_status_uplink_t {
	uint32_t reset_reason;
	uint8_t version[32];
};

#define LORAWAN_PORT_UPDATE_STATUS	0x21

struct lorawan_update_status_uplink_t {
	uint16_t received;
	uint16_t lost;
	uint16_t expected;
};

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