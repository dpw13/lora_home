"""Gate controller Python constants."""

import enum

class GateCmd(enum.Enum):
    """Commands for gates."""

    TOGGLE = 1
    CLOSE = 2
    MOM_OPEN = 3
    HOLD_OPEN = 4


class GateState(enum.Enum):
    """States for gates.

    See app_protocol.h
    """

    CLOSED = 1
    MOVING = 2
    MOM_OPEN = 3
    HOLD_OPEN = 4


class PortId(enum.Enum):
    """LoRa port IDs."""

    # Other ports not listed here are used for more detailed status reporting.
    GATE_STATE = 128
    GARAGE1_STATE = 129
    GARAGE2_STATE = 130

COMMANDS = {
    # Gate
    0: (
        PortId.GATE_STATE,
        {
            0x2: GateCmd.MOM_OPEN,  # single short
            0x3: GateCmd.HOLD_OPEN,  # single long
            0xA: GateCmd.CLOSE,  # double short
        },
    ),
    # Garage 1
    1: (
        PortId.GARAGE1_STATE,
        {
            0x2: GateCmd.TOGGLE,  # single short
            0x3: GateCmd.HOLD_OPEN,  # single long
            0xA: GateCmd.CLOSE,  # double short
        },
    ),
    # Garage 2
    2: (
        PortId.GARAGE2_STATE,
        {
            0x2: GateCmd.TOGGLE,  # single short
            0x3: GateCmd.HOLD_OPEN,  # single long
            0xA: GateCmd.CLOSE,  # double short
        },
    ),
}
