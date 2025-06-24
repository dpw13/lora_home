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
import json
import logging
import random
import struct
from typing import Any

from remote_const import GateState, COMMANDS, PortId
from gate_ctrl import GateStateMachine

from paho.mqtt.reasoncodes import ReasonCode
from paho.mqtt.properties import Properties
import paho.mqtt.client as mqtt

# Generated with
#
# PROTO_PATH=/home/dwagner/git/chirpstack/api/proto
# protoc --proto_path=${PROTO_PATH} --python_out=generated ${PROTO_PATH}/gw/gw.proto
import gw.gw_pb2 as gateway

logger = logging.getLogger(__name__)


gate_sm: dict[PortId, GateStateMachine] = {}


def on_remote_button(idx: int, action: int):
    """Handle button press on remote."""
    if idx in COMMANDS:
        port, action_map = COMMANDS[idx]
        cmd = action_map.get(action, 0)

        logger.debug("port %d cmd %s", port, cmd)

        if not cmd:
            logger.info("No action taken")
            return 0

        if port in gate_sm:
            gate_sm[port].command(cmd)
            # ACK
            return 1
        else:
            logger.warning("Destination controller for port %d is unknown", port)
            # NACK: no action taken
            return 0


def on_application_uplink(client: mqtt.Client, path: list[str], obj: Any):
    """Register topics and EUI based on LoRa uplink port."""
    port = obj.get("fPort", 0)
    if port != 0:
        topic = "/".join(path[:-2])
        eui = obj["deviceInfo"]["devEui"]

        try:
            port_id = PortId(port)
            state = GateState(obj.get("object").get("state"))
            if port_id not in gate_sm:
                logger.info("Registering port %d to %s", port, eui)
                gate_sm[port_id] = GateStateMachine(client, port_id, topic, eui, state)
            # Update state and possibly publish to HA
            gate_sm[port_id].update(state)
        except ValueError:
            # ValueError due to invalid port ID can be ignored, we just don't
            # do anything with messages we don't do anything for.
            pass


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

    if uplink.phy_payload[0] == 0xE0:
        # proprietary frame: this is us
        # logger.debug(uplink)
        rsp = 0
        if uplink.phy_payload[1] == 0x01:
            # Remote command
            s = struct.Struct("HBB")
            (battery, btn, action) = s.unpack_from(uplink.phy_payload, 2)
            logger.info("Battery %f V button %d action %d", battery / 1000, btn, action)
            # Remote press handlers
            rsp = on_remote_button(btn, action)
            # Acknowledge
        logger.debug("Sending ack %s", rsp)

        # Send acknowledgement by writing to the gateway MQTT topic, which
        # is serviced by the Chirpstack gateway bridge. The bridge simply
        # forwards protobuf messages received over MQTT directly to the
        # BasicStation running on the gateway, so we are effectively talking
        # directly to the BasicStation here.
        ack_topic = "/".join(path[:-2]) + "/command/down"
        ack_msg = struct.Struct("BBB")
        ack_payload = ack_msg.pack(0xE0, 0x00, rsp)
        send_downlink(
            client, ack_topic, path[2], None, uplink.rx_info.context, ack_payload
        )


# pylint: disable=unused-argument
def on_message(client: mqtt.Client, userdata: Any, msg: mqtt.MQTTMessage):
    """Handle messages on subscribed topics.

    The callback for when a PUBLISH message is received from the server.
    """
    logger.debug("rx %s", msg.topic)

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
        # logger.debug("%s: %s", msg.topic, uplink)
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
    client.subscribe("homeassistant/cover/+/command")
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

    parser.add_argument("-H", "--host", default="potato6300", help="The MQTT host")
    parser.add_argument("-p", "--port", type=int, default=1883, help="The MQTT port")
    parser.add_argument(
        "-k", "--keepalive", type=int, default=60, help="The MQTT keepalive interval"
    )
    parser.add_argument(
        "-v", "--verbose", action="store_true", help="Enable verbose logging"
    )

    args = parser.parse_args()

    if args.verbose:
        logger.setLevel(logging.DEBUG)

    mqttc.connect(args.host, args.port, args.keepalive)

    # Blocking call that processes network traffic, dispatches callbacks and
    # handles reconnecting.
    # Other loop*() functions are available that give a threaded interface and a
    # manual interface.
    mqttc.loop_forever()


if __name__ == "__main__":
    main()
