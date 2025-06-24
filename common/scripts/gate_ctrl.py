"""Python class for the gate controller/HA integration over MQTT."""

import base64
import json
import logging
import struct
from typing import Any

from remote_const import GateState, GateCmd, PortId

import paho.mqtt.client as mqtt

logger = logging.getLogger(__name__)


class GateStateMachine:
    """Translates gate controller state messages to messages HA understands.

    See https://www.home-assistant.io/integrations/cover.mqtt/
    """

    HA_OPEN = "open"
    HA_OPENING = "opening"
    HA_CLOSED = "closed"
    HA_CLOSING = "closing"

    GATE_UID = {
        PortId.GATE_STATE: "driveway_gate",
        PortId.GARAGE1_STATE: "garage_A",
        PortId.GARAGE2_STATE: "garage_B",
    }
    DEVICE_CLASS = {
        PortId.GATE_STATE: "gate",
        PortId.GARAGE1_STATE: "garage",
        PortId.GARAGE2_STATE: "garage",
    }

    def __init__(
        self,
        client: mqtt.Client,
        port: PortId,
        topic: str,
        eui: str,
        state: GateState = GateState.CLOSED,
    ):
        self.client = client
        self.port = port
        self.state = state
        self.uid = self.GATE_UID[self.port]
        # The controller topic
        self.ctrl_topic = topic
        self.eui = eui
        logger.debug("Subscribing to %s", self.cmd_topic)
        client.message_callback_add(self.cmd_topic, self._cmd_callback)
        self._announce()

    @property
    def topic(self):
        """The HomeAssistant component MQTT topic"""
        return "homeassistant/cover/" + self.uid

    @property
    def cmd_topic(self):
        return self.topic + "/command"

    @property
    def config_topic(self):
        return self.topic + "/config"

    @property
    def state_topic(self):
        return self.topic + "/state"

    @property
    def device_class(self):
        return self.DEVICE_CLASS[self.port]

    def _publish_state(self, ha_state: str):
        """Publish the state to HomeAssistant."""
        self.client.publish(self.state_topic, ha_state, retain=True)

    def _announce(self):
        """Publish an auto-discovery announcement message.

        See https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery
        """
        name = self.uid.replace("_", " ").capitalize()
        msg = {
            "device": {
                "mf": "Wagner Metalworks",
                "mdl": "LoRa Gate Controller",
                "sw": "1.0",
                "sn": self.eui,
                "hw": "1.0",
                "identifiers": [self.eui],
            },
            "origin": {
                "name": "remote_svc",
                "sw": "1.0",
            },
            "device_class": self.device_class,
            "name": name,
            "unique_id": self.uid,
            "~": self.topic,
            "command_topic": "~/command",
            "state_topic": "~/state",
        }
        self.client.publish(self.config_topic, json.dumps(msg), retain=True)

    def command(self, cmd: GateCmd):
        """Send a command to the controller."""
        # We respond by writing to the `application` endpoint, which is
        # serviced by chirpstack. Downlink frames are sent immediately in
        # a Class C context. Class C is initiated by the device by sending
        # a MAC command.
        logger.info("Sending cmd %s to %s port %d", cmd, self.eui, self.port)
        rsp = struct.Struct("B")
        payload = rsp.pack(cmd.value)
        msg = {
            "devEui": self.eui,
            "confirmed": False,
            "fPort": self.port.value,
            "data": base64.b64encode(payload).decode(),
        }
        logger.debug(msg)
        self.client.publish(f"{self.ctrl_topic}/command/down", json.dumps(msg))

    # This is a callback so we don't have control over which arguments we get passed.
    # pylint: disable=unused-argument
    def _cmd_callback(self, client: mqtt.Client, userdata: Any, msg: mqtt.MQTTMessage):
        """Handle any MQTT message published to this topic."""
        logger.debug("Got command callback at %s: %s", msg.topic, msg.payload)
        if msg.payload.lower() == b"open":
            self.command(GateCmd.MOM_OPEN)
        elif msg.payload.lower() == b"close":
            self.command(GateCmd.CLOSE)
        else:
            logger.warning("Unknown command payload %s", msg.payload)

    def update(self, state: GateState):
        """Update gate state and publish message to HomeAssistant."""
        if state == GateState.MOVING:
            if self.state == GateState.CLOSED:
                self._publish_state(self.HA_OPENING)
            else:
                self._publish_state(self.HA_CLOSING)
        elif state in (GateState.HOLD_OPEN, GateState.MOM_OPEN):
            self._publish_state(self.HA_OPEN)
        elif state == GateState.CLOSED:
            self._publish_state(self.HA_CLOSED)

        self.state = state
