#!/usr/bin/env python3

# Subscribes to MQTT topics to listen to proprietary LoRaWAN
# frames sent by the remotes. Transforms the data sent by the
# remote into downlinked commands for various controllers.

# NOTE that for this to work efficiently, devices should be in Class C
# state. If Chirpstack does not properly recognize this but the device is
# in fact listening, you can manually update the device state in postgres:
#
# $ docker compose exec postgres psql -U chirpstack
# ...
# chirpstack=# select dev_eui, application_id, enabled_class from device;
#       dev_eui       |            application_id            | enabled_class
# --------------------+--------------------------------------+---------------
#  \x4aeabf04c55ea86c | bd555621-d30a-4fcd-b1b8-c870be7075b7 | A
#  \x903035e30ede904c | bd555621-d30a-4fcd-b1b8-c870be7075b7 | A
#  \x332cb18ea9d5f5b0 | bd555621-d30a-4fcd-b1b8-c870be7075b7 | A
#
# chirpstack=# update device set enabled_class='C' where dev_eui = '\x4aeabf04c55ea86c';
# UPDATE 1
#
# chirpstack=# select name, dev_eui, application_id, enabled_class from device;
#        name        |      dev_eui       |            application_id            | enabled_class
# -------------------+--------------------+--------------------------------------+---------------
#  Garage Controller | \x4aeabf04c55ea86c | bd555621-d30a-4fcd-b1b8-c870be7075b7 | C
#  Remote A          | \x903035e30ede904c | bd555621-d30a-4fcd-b1b8-c870be7075b7 | A
#
# chirpstack=# \q

import argparse
import base64
import binascii
import json
import logging
import random
import struct
from typing import Any

from remote_const import GateState, GateCmd, COMMANDS, PortId, GATE_TOPIC

from paho.mqtt.reasoncodes import ReasonCode
from paho.mqtt.properties import Properties
import paho.mqtt.client as mqtt

# Generated with
#
# PROTO_PATH=/home/dwagner/git/chirpstack/api/proto
# protoc --proto_path=${PROTO_PATH} --python_out=generated ${PROTO_PATH}/gw/gw.proto
import gw.gw_pb2 as gateway

logger = logging.getLogger(__name__)


devices: dict[int, Any] = {}


def send_cmd(client: mqtt.Client, port: int, cmd: GateCmd) -> int:
    """Send a downlink message to the controller to change state."""
    if port in devices:
        # TODO: might should be part of GateStateMachine below
        (topic, eui) = devices[port]
        # We respond by writing to the `application` endpoint, which is
        # serviced by chirpstack. Downlink frames are sent immediately in
        # a Class C context. Class C is initiated by the device by sending
        # a MAC command.
        logger.info("Sending cmd %s to %s port %d", cmd, eui, port)
        rsp = struct.Struct("B")
        payload = rsp.pack(cmd)
        msg = {
            "devEui": eui,
            "confirmed": False,
            "fPort": port,
            "data": base64.b64encode(payload).decode(),
        }
        logger.debug(msg)
        client.publish(f"{topic}/command/down", json.dumps(msg))

        # ACK
        return 1
    else:
        logger.warning("Destination eui for port %d is unknown", port)
        # NACK: no action taken
        return 0


class GateStateMachine:
    """Translates gate controller state messages to messages HA understands.

    See https://www.home-assistant.io/integrations/cover.mqtt/
    """

    HA_OPEN = "state_open"
    HA_OPENING = "state_opening"
    HA_CLOSED = "state_closed"
    HA_CLOSING = "state_closing"

    GATE_UID = {
        PortId.GATE_STATE: "driveway_gate",
        PortId.GARAGE1_STATE: "garage_A",
        PortId.GARAGE2_STATE: "garage_B",
    }

    def __init__(
        self, client: mqtt.Client, port: PortId, state: GateState = GateState.CLOSED
    ):
        self.client = client
        self.port = port
        self.state = state
        self.uid = self.GATE_UID[self.port]
        self.eui, _ = devices[self.port.value]
        client.message_callback_add(
            self.topic, GateStateMachine.msg_callback, self.cmd_topic
        )
        self._announce()

    @property
    def topic(self):
        return "/homeassistant/cover/" + self.uid

    @property
    def cmd_topic(self):
        return self.topic + "/command"

    @property
    def config_topic(self):
        return self.topic + "/config"

    @property
    def state_topic(self):
        return self.topic + "/state"

    def _publish_state(self, ha_state: str):
        """Publish the state to HomeAssistant."""
        self.client.publish(self.state_topic, ha_state, retain=True)

    def _announce(self):
        """Publish an auto-discovery announcement message.

        See https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery
        """
        name = self.uid.replace('_', ' ').capitalize()
        msg = {
            "device": {
                "mf": "Wagner Metalworks",
                "mdl": "LoRa Gate Controller",
                "sw": "1.0",
                "sn": self.eui,
                "hw": "1.0",
                "identifiers": [self.eui]
            },
            "origin": {
                "name": "remote_svc",
                "sw": "1.0",
            },
            "device_class": "cover",
            "name": name,
            "unique_id": self.uid,
            "~": self.topic,
            "command_topic": "~/command",
            "state_topic": "~/state",
        }
        self.client.publish(self.config_topic, msg, retain=True)

    def cmd_callback(self, msg: mqtt.MQTTMessage):
        """Handle any MQTT message published to this topic."""
        path = msg.topic.split("/")
        if path[-1] == "command":
            if msg.payload.lower() == "open":
                send_cmd(self.client, self.port.value, GateCmd.MOM_OPEN)
            elif msg.payload.lower() == "close":
                send_cmd(self.client, self.port.value, GateCmd.CLOSE)
            else:
                logger.warning("Unknown command payload %s", msg.payload)

    @staticmethod
    def msg_callback(
        client: mqtt.Client, obj: "GateStateMachine", msg: mqtt.MQTTMessage
    ):
        """MQTT message callback."""
        obj.msg_callback(msg)

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


def on_remote_button(client: mqtt.Client, idx: int, action: int):
    """Handle button press on remote."""
    if idx in COMMANDS:
        port, action_map = COMMANDS[idx]
        cmd = action_map.get(action, 0)

        logger.debug("port %d cmd %s", port, cmd)

        if not cmd:
            logger.info("No action taken")
            return 0

        return send_cmd(client, port, cmd)


gate_sm: dict[PortId, GateStateMachine] = {}


def on_application_uplink(client: mqtt.Client, path: list[str], obj: Any):
    """Register topics and EUI based on LoRa uplink port."""
    port = obj.get("fPort", 0)
    if port != 0:
        topic = "/".join(path[:-2])
        eui = obj["deviceInfo"]["devEui"]
        data = (topic, eui)
        if port not in devices or devices[port] != data:
            logger.info("Registering port %d to %s", port, eui)
        devices[port] = data
        try:
            port_id = PortId(port)
            state = GateState(obj.get("data").get("state"))
            if port_id not in gate_sm:
                gate_sm[port_id] = GateStateMachine(client, port_id, state)
            # Update state and possibly publish to HA
            gate_sm[port_id].update(state)
        except Exception as e:
            # Need to check which exceptions are thrown with unidentified
            # ports, as those are harmless and should not warn.
            logger.warning("Could not update gate state: %s", e)


def send_downlink(
    client: mqtt.Client,
    topic: str,
    gateway_id: str,
    downlink_id: str | None,
    tx_context: Any,
    payload: str,
):
    """Send downlink by publishing a serialized downlink message."""
    if downlink_id is None:
        downlink_id = random.getrandbits(31)

    ack_dl = gateway.DownlinkFrame()
    ack_dl.gateway_id = gateway_id
    ack_dl.downlink_id = downlink_id

    fi = ack_dl.items.add()
    fi.phy_payload = payload
    # Respond relative to the uplink
    fi.tx_info.context = tx_context
    # Shared downlink frequency
    fi.tx_info.frequency = 923300000
    fi.tx_info.power = -1
    # SIGH. All downlink datarates (defined in chirpstack/lrwn/src/region/us915.rs
    # and NOT in a .toml) are in the 500 kHz channel and all are CR 4/5. SF ranges
    # from 12 (slowest symbol rate) down to 7 (fastest symbol rate). These settings
    # should (must?) match the downlink config on the device.
    fi.tx_info.modulation.lora.bandwidth = 500000
    fi.tx_info.modulation.lora.spreading_factor = 12
    fi.tx_info.modulation.lora.code_rate = gateway.CodeRate.CR_4_5
    # All downlinks also appear to have this set for RX2 on 923300000
    fi.tx_info.modulation.lora.polarization_inversion = True

    fi.tx_info.timing.delay.delay.seconds = 1
    logger.debug("ACK: %s", ack_dl)

    client.publish(topic, ack_dl.SerializeToString())


def on_gateway_uplink(
    client: mqtt.Client, path: list[str], uplink: gateway.UplinkFrame
):
    """Handle a gateway uplink message."""
    # The LoRa payload (aka LoRaWAN PHY payload) MHDR (MAC header) determines
    # the frame type in the upper 3 bits of the first byte. Remaining bits in
    # the first byte are reserved, apart from the bottom 2 which are the LoRa
    # version and should be set to zero. Use a byte of 0xE0 (frame type 7) for
    # proprietary non-LoRaWAN LoRa-compatible messages.
    # Presumably the MIC is not calculated for proprietary frames since it's
    # not defined which key should be used.
    ack_msg = struct.Struct("BBB")

    if uplink.phy_payload[0] == 0xE0:
        # proprietary frame: this is us
        logger.debug(
            "    phy payload = %s", binascii.hexlify(uplink.phy_payload).decode()
        )
        logger.debug(uplink)
        rsp = 0
        if uplink.phy_payload[1] == 0x01:
            # Remote command
            s = struct.Struct("HBB")
            (battery, btn, action) = s.unpack_from(uplink.phy_payload, 2)
            logger.info("Battery %f V button %d action %d", battery / 1000, btn, action)
            # Remote press handlers
            rsp = on_remote_button(client, btn, action)
            # Acknowledge
        logger.debug("Sending ack %s", rsp)

        # Send acknowledgement by writing to the gateway MQTT topic, which
        # is serviced by the Chirpstack gateway bridge. The bridge simply
        # forwards protobuf messages received over MQTT directly to the
        # BasicStation running on the gateway, so we are effectively talking
        # directly to the BasicStation here.
        ack_topic = "/".join(path[:-2]) + "/command/down"
        ack_payload = ack_msg.pack(0xE0, 0x00, rsp)
        send_downlink(
            client, ack_topic, path[2], None, uplink.rx_info.context, ack_payload
        )


def on_message(client: mqtt.Client, userdata: Any, msg: mqtt.MQTTMessage):
    """Handle messages on subscribed topics.

    The callback for when a PUBLISH message is received from the server.
    """
    path = msg.topic.split("/")
    if path[0] == "application" and path[-1] == "up":
        obj: dict = json.loads(msg.payload)
        on_application_uplink(client, path, obj)
    # elif (path[1] == "gateway" and path[-1] == "down"):
    #    downlink = gateway.DownlinkFrame()
    #    downlink.ParseFromString(msg.payload)
    #    logger.debug(f"{msg.topic}: {downlink}")
    elif path[1] == "gateway" and path[-1] == "up":
        uplink = gateway.UplinkFrame()
        uplink.ParseFromString(msg.payload)
        logger.debug("%s: %s", msg.topic, uplink)
        on_gateway_uplink(client, path, uplink)


# The callback for when the client receives a CONNACK response from the server.
def on_connect(
    client: mqtt.Client,
    userdata: Any,
    flags: mqtt.ConnectFlags,
    reason_code: ReasonCode,
    properties: Properties,
) -> None:
    """Subscribe to desired topics."""
    logger.info("Connected with result code %s", reason_code)
    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    # client.subscribe("$SYS/#")
    client.subscribe("us915_0/gateway/#")
    client.subscribe("application/#")
    # client.subscribe("us915_0/gateway/#")


def main():
    """Connect to MQTT and loop."""
    logging.basicConfig(
        format="%(asctime)s [%(levelname)4s] %(name)s: %(message)s", level=logging.INFO
    )

    mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    mqttc.on_connect = on_connect
    mqttc.on_message = on_message

    parser = argparse.ArgumentParser(
        prog="remote_svc.py", description="LoRa MQTT and remote adapter"
    )

    parser.add_argument("-h", "--host", default="potato6300", help="The MQTT host")
    parser.add_argument("-p", "--port", type=int, default=1883, help="The MQTT port")
    parser.add_argument(
        "-k", "--keepalive", type=int, default=60, help="The MQTT keepalive interval"
    )

    args = parser.parse_args()

    mqttc.connect(args.host, args.port, args.keepalive)

    # Blocking call that processes network traffic, dispatches callbacks and
    # handles reconnecting.
    # Other loop*() functions are available that give a threaded interface and a
    # manual interface.
    mqttc.loop_forever()


if __name__ == "__main__":
    main()
