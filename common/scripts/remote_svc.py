#!/usr/bin/env python3

# Subscribes to MQTT topics to listen to proprietary LoRaWAN
# frames sent by the remotes. Transforms the data sent by the
# remote into downlinked commands for various controllers.

import base64
import binascii
import json
import paho.mqtt.client as mqtt
import random
import struct

# Generated with
# 
# PROTO_PATH=/home/dwagner/git/chirpstack/api/proto
# protoc --proto_path=${PROTO_PATH} --python_out=generated ${PROTO_PATH}/gw/gw.proto
import gw.gw_pb2 as gateway

devices = {}

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, reason_code, properties):
    print(f"Connected with result code {reason_code}")
    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    #client.subscribe("$SYS/#")
    client.subscribe("us915_0/gateway/#")
    client.subscribe("application/#")
    #client.subscribe("us915_0/gateway/#")

def on_remote_button(client, idx: int):
    if (idx == 0):
        # Gate
        if (128 in devices):
            rsp = struct.Struct("L")
            # Relay open time in ms
            msg = rsp.pack(10000)
            (topic, eui) = devices[128]
            msg = {
                "devEui": eui,
                "confirmed": False,
                "fPort": 128,
                "data": base64.b64encode(msg).decode()
            }
            client.publish(f"{topic}/command/down", json.dumps(msg))

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    path = msg.topic.split("/")
    if (path[0] == "application" and path[-1] == "up"):
        obj : dict = json.loads(msg.payload)
        port = obj.get("fPort", 0)
        if (port != 0):
            topic = "/".join(path[:-2])
            eui = obj["deviceInfo"]["devEui"]
            data = (topic, eui)
            if (port not in devices or devices[port] != data):
                print(f"Registering port {port} to {data}")
            devices[port] = data
    elif (path[1] == "gateway" and path[-1] == "down"):
        downlink = gateway.DownlinkFrame()
        downlink.ParseFromString(msg.payload)
        print(f"{msg.topic}: {downlink}")
    elif (path[1] == "gateway" and path[-1] == "up"):
        uplink = gateway.UplinkFrame()
        uplink.ParseFromString(msg.payload)
        #print(f"{msg.topic}: {uplink}")
        # The LoRa payload (aka LoRaWAN PHY payload) MHDR (MAC header) determines
        # the frame type in the upper 3 bits of the first byte. Remaining bits in
        # the first byte are reserved, apart from the bottom 2 which are the LoRa
        # version and should be set to zero. Use a byte of 0xE0 (frame type 7) for
        # proprietary non-LoRaWAN LoRa-compatible messages.
        # Presumably the MIC is not calculated for proprietary frames since it's
        # not defined which key should be used.
        ack_msg = struct.Struct("BBB")

        if (uplink.phy_payload[0] == 0xE0):
            print(f"{msg.topic}: phy payload = " + binascii.hexlify(uplink.phy_payload).decode())
            print(uplink)
            # proprietary frame, this is us
            rsp = 0
            if (uplink.phy_payload[1] == 0x01):
                # Remote command
                s = struct.Struct("HB")
                (battery, payload) = s.unpack_from(uplink.phy_payload, 2)
                print(f"Battery {battery:x} button idx: {payload}")
                # Remote press handlers
                on_remote_button(client, payload)
                # Acknowledge
                rsp = 1

            # Send acknowledgement
            # TODO: proprietary ack still not getting received
            ack_topic = "/".join(path[:-2]) + "/command/down"
            ack_dl = gateway.DownlinkFrame()
            ack_dl.gateway_id = path[2]
            ack_dl.downlink_id = random.getrandbits(31)

            fi = ack_dl.items.add()
            fi.phy_payload = ack_msg.pack(0xE0, 0x00, rsp)
            # Respond using identical parameters to the uplink
            fi.tx_info.context = uplink.rx_info.context
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
            # All downlinks also appear to have this set
            fi.tx_info.modulation.lora.polarization_inversion = True
            
            fi.tx_info.timing.delay.delay.seconds = 1
            print(f"ACK: {ack_dl}")

            client.publish(ack_topic, ack_dl.SerializeToString())

mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqttc.on_connect = on_connect
mqttc.on_message = on_message

mqttc.connect("potato6300", 1883, 60)

# Blocking call that processes network traffic, dispatches callbacks and
# handles reconnecting.
# Other loop*() functions are available that give a threaded interface and a
# manual interface.
mqttc.loop_forever()