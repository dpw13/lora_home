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
        return "homeassistant/event/" + self.uid

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
        return f"press_{bin(action)[2:]}"

    def _publish_state(self, btn: int, action: int) -> int:
        """Publish the state to HomeAssistant."""
        msg = {
            "event_type": f"button_{btn}",
            "action": RemoteDevice._action_name(action)
        }
        self.client.publish(self.state_topic, json.dumps(msg))
        # ACK
        return 1

    def _announce(self):
        """Publish an auto-discovery announcement message.

        See https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery
        """
        # TODO: battery voltage

        name = "LoRa Remote"
        msg = {
            "device": {
                "mf": "Wagner Metalworks",
                "name": "LoRa Remote",
                "icon": "mdi:remote",
                "sw": "1.0",
                "sn": self.sn,
                "hw": "1.0",
                "identifiers": [self.sn],
            },
            "origin": {
                "name": "remote_svc",
                "sw": "1.0",
            },
            "name": name,
            "unique_id": self.uid,
            "event_types": [f"button_{i}" for i in range(0,self.btn_count)],
            "~": self.topic,
            "state_topic": "~/state",
        }
        self.client.publish(self.config_topic, json.dumps(msg), retain=True)
