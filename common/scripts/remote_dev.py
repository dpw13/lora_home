"""Python class for the LoRa remote HomeAssistant device."""

import json
import logging
import struct

import paho.mqtt.client as mqtt

logger = logging.getLogger(__name__)


class RemoteDevice:
    """Translates LoRa proprietary remote messages to HA device.

    See https://www.home-assistant.io/integrations/device_trigger.mqtt/
    """

    def __init__(
        self,
        client: mqtt.Client,
        btn_count: int,
    ):
        self.client = client
        self.uid = "lora_remote"
        self.sn = "lora_remote_todo"
        self.btn_count = btn_count
        self._announce()

    @property
    def topic(self):
        """The HomeAssistant component MQTT topic"""
        return "homeassistant/device_automation/" + self.uid

    @property
    def config_topic(self):
        return self.topic + "/config"

    @property
    def state_topic(self):
        return self.topic + "/state"

    def on_uplink(self, payload: bytes) -> bytes:
        """Process proprietary LoRa frame from remote."""
        rsp = 0
        if payload[1] == 0x01:
            # Remote command
            s = struct.Struct("HBB")
            (battery, btn, action) = s.unpack_from(payload, 2)
            logger.info("Battery %f V button %d action %d", battery / 1000, btn, action)
            # Publish to HA
            rsp = self._publish_state(btn, action)

        ack_msg = struct.Struct("BBB")
        ack_payload = ack_msg.pack(0xE0, 0x00, rsp)
        logger.debug("Sending ack %s", rsp)
        return ack_payload

    @staticmethod
    def _action_name(action: int) -> str:
        action_seq: list[str] = []
        action_names = ['short', 'long']
        while action > 0:
            # Actions are encoded in two bits. The upper bit indicates the action
            # is valid, and the lower bit indicates short vs long.
            action_seq.append(action_names[action & 0x01])
            action = action >> 2
        # TODO: could be reversed
        return "press_" + "_".join(action_seq)

    def _publish_state(self, btn: int, action: int) -> int:
        """Publish the state to HomeAssistant."""
        msg = RemoteDevice._action_name(action),
        self.client.publish(f"{self.topic}/button_{btn}", msg)
        # ACK
        return 1

    def _announce(self):
        for btn in range(0, self.btn_count):
            for action_len in range(1,3):
                for action_bin in range(0, 2**action_len):
                    seq = bin(action_bin)[2:].rjust(action_len, '0')
                    action = 0
                    for char in seq:
                        if char == '1':
                            action = (action << 2) + 3
                        else:
                            action = (action << 2) + 2
                    self._announce_single(btn, action)

    def _announce_single(self, btn: int, action: int):
        """Publish an auto-discovery announcement message.

        See https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery
        """
        # TODO: battery voltage

        button_name = f"button_{btn}"
        action_name = self._action_name(action)
        msg = {
            "device": {
                "mf": "Wagner Metalworks",
                "name": "LoRa Remote",
                "mdl": "Adafruit Feather M0",
                "sw": "1.0",
                "sn": self.sn,
                "hw": "1.0",
                "identifiers": [self.sn],
            },
            "origin": {
                "name": "remote_svc",
                "sw": "1.0",
            },
            "automation_type": "trigger",
            "type": action_name,
            "subtype": button_name,
            "topic": f"{self.topic}/{button_name}",
            "payload": action_name,
        }
        # Need separate config topics for each type/subtype combination. This appears to be what HA
        # requires so even though there is a *lot* of duplication, this is what we're going with.
        self.client.publish(f"{self.topic}/{button_name}_action_{action}/config", json.dumps(msg), retain=True)
