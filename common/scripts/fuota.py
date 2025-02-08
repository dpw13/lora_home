#!/usr/bin/env python3

# Queue a deployment to the chirpstack FUOTA server

import argparse
import binascii
import grpc
import math
import time
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

# PROTO_PATH=/home/dwagner/git/chirpstack/api/proto
# protoc --proto_path=${PROTO_PATH} --python_out=generated ${PROTO_PATH}/gw/gw.proto
import fuota.fuota_pb2 as fuota
import fuota.fuota_pb2_grpc as fuota_grpc
from google.protobuf import duration_pb2

DL_FRAME_RATE = 1 # 1 packet per second, see chirpstack network scheduler config

def get_dl_freq(channel: int):
        freq_base = 923300000 # US915
        return freq_base + channel * 600000


def do_deploy(args: argparse.Namespace):
        with open(args.firmware, "rb") as fw:
                payload = fw.read()
        print(f"Read {len(payload)} B from {args.firmware}")

        uncoded_frames = len(payload) / args.fragment_size
        redundancy_frames = math.ceil(uncoded_frames * (args.fragment_redundancy / 100.0))
        uncoded_frames = math.ceil(uncoded_frames)
        print(f"Expecting {uncoded_frames} frames and {redundancy_frames} extra parity blocks")

        frame_count = uncoded_frames + redundancy_frames
        # Add 20% to be certain
        dl_duration = 1.2 * (frame_count // DL_FRAME_RATE)
        mcast_timeout_log2 = int(math.ceil(math.log2(dl_duration)))
        print(f"Downlink expected to take about {dl_duration} s, setting mcast timeout to {mcast_timeout_log2}")

        with grpc.insecure_channel(f"{args.host}:{args.port}") as channel:
                stub = fuota_grpc.FuotaServerServiceStub(channel)

                cipher = Cipher(algorithms.AES(binascii.unhexlify(args.app_key)), modes.ECB())
                enc = cipher.encryptor()
                # For LoRaWAN 1.0.x the input is 16 bytes of 0x00.
                # For 1.1.x the first byte is 0x20.
                mc_root_key = enc.update(bytes([0x20] + [0] * 15)) + enc.finalize()
                #print(f"McRootKey: {binascii.hexlify(mc_root_key)}")

                request = fuota.CreateDeploymentRequest(deployment=
                                fuota.Deployment(
                                        application_id=args.app_id,
                                        multicast_group_type=fuota.CLASS_C,
                                        multicast_dr=args.dr,
                                        multicast_frequency=get_dl_freq(args.channel),
                                        multicast_group_id=0,
                                        multicast_timeout=mcast_timeout_log2, # actual timeout is (2^timeout) seconds
                                        multicast_region=fuota.US915,
                                        unicast_timeout=duration_pb2.Duration(seconds=args.timeout),
                                        unicast_attempt_count=3,
                                        fragmentation_fragment_size=args.fragment_size,
                                        fragmentation_block_ack_delay=1, # ???
                                        fragmentation_redundancy=redundancy_frames,
                                        devices=[
                                                fuota.DeploymentDevice(
                                                        dev_eui=args.eui,
                                                        mc_root_key=binascii.hexlify(mc_root_key),
                                                ),
                                        ],
                                        payload=payload,
                                        ))
                response = stub.CreateDeployment(request=request)
                print("Deployment created: " + response.id)


def do_status(args: argparse.Namespace):
        with grpc.insecure_channel(f"{args.host}:{args.port}") as channel:
                stub = fuota_grpc.FuotaServerServiceStub(channel)

                request = fuota.GetDeploymentStatusRequest(id=args.id)
                response = stub.GetDeploymentStatus(request=request)

                print(f"Deployment {args.id} status: {response}")


def do_logs(args: argparse.Namespace):
        with grpc.insecure_channel(f"{args.host}:{args.port}") as channel:
                stub = fuota_grpc.FuotaServerServiceStub(channel)

                request = fuota.GetDeploymentDeviceLogsRequest(
                                deployment_id=args.id,
                                dev_eui=args.eui)
                response = stub.GetDeploymentDeviceLogs(request=request)

                for log in response.logs:
                        ts = log.created_at.seconds + log.created_at.nanos/1e9
                        tstr = time.ctime(ts)
                        print(f"{tstr}: {log.command} port {log.f_port}")
                        for k, v in sorted(log.fields.items(), key=lambda kv: (kv[1], kv[0])):
                                print(f"  {k}: {v}")
                #print(f"Deployment {args.id} logs for {args.eui}: {response}")


parser = argparse.ArgumentParser(
        prog="fuota.py",
        description="Chirpstack firmware update OTA client"
)
parser.add_argument('-H', '--host',
                        default="potato6300",
                        help="gRPC host")
parser.add_argument('-p', '--port',
                        type=int,
                        default=8091,
                        help="gRPC port")
subparsers = parser.add_subparsers(
                        required=True,
                        help='subcommand help')

deploy = subparsers.add_parser('deploy', help='Create new deployment')
deploy.add_argument('-t', '--timeout',
                        type=int,
                        default=60,
                        help="Unicast timeout. Set to update interval of device")
deploy.add_argument('-e', '--eui',
                        required=True,
                        help="Device EUI as a hex string without spaces")
deploy.add_argument('-k', '--app-key',
                        required=True,
                        help="Device AppKey as a hex string without spaces")
deploy.add_argument('-f', '--firmware',
                        required=True,
                        help="Path to the new firmware")
deploy.add_argument('-a', '--app-id',
                        default="bd555621-d30a-4fcd-b1b8-c870be7075b7",
                        help="Application UUID (optional)")

# Defaults below should not need changing
deploy.add_argument('--dr',
                        type=int,
                        default=12, # Needs to be at least 10 to meet payload size requirements
                        help="Multicast downlink data rate")
deploy.add_argument('-c', '--channel',
                        type=int,
                        default=0,
                        help="US915 channel")
deploy.add_argument('-s', '--fragment-size',
                        type=int,
                        default=232,
                        help="Fragment size")
deploy.add_argument('-r', '--fragment-redundancy',
                        type=int,
                        default=10,
                        help="Fragment redundancy in percent")
deploy.set_defaults(func=do_deploy)


status = subparsers.add_parser('status', help='Get deployment status')
status.add_argument('-i', '--id',
                        required=True,
                        help="Deployment ID")
status.set_defaults(func=do_status)


logs = subparsers.add_parser('logs', help='Get deployment logs')
logs.add_argument('-i', '--id',
                        required=True,
                        help="Deployment ID")
logs.add_argument('-e', '--eui',
                        required=True,
                        help="Device EUI as a hex string without spaces")
logs.set_defaults(func=do_logs)


args = parser.parse_args()
args.func(args)
