#!/usr/bin/env python3

# Queue a deployment to the chirpstack FUOTA server

import argparse
import binascii
import grpc
import time
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

# PROTO_PATH=/home/dwagner/git/chirpstack/api/proto
# protoc --proto_path=${PROTO_PATH} --python_out=generated ${PROTO_PATH}/gw/gw.proto
import fuota.fuota_pb2 as fuota
import fuota.fuota_pb2_grpc as fuota_grpc
from google.protobuf import duration_pb2


def get_dl_freq(channel: int):
        freq_base = 923300000 # US915
        return freq_base + channel * 600000


def do_deploy(args: argparse.Namespace):
        with open(args.firmware, "rb") as fw:
                payload = fw.read()
        print(f"Read {len(payload)} B from {args.firmware}")

        with grpc.insecure_channel(f"{args.host}:{args.port}") as channel:
                stub = fuota_grpc.FuotaServerServiceStub(channel)

                cipher = Cipher(algorithms.AES(binascii.unhexlify(args.app_key)), modes.ECB())
                enc = cipher.encryptor()
                # For LoRaWAN 1.0.x the input is 16 bytes of 0x00.
                # For 1.1.x the first byte is 0x20.
                mc_root_key = enc.update(bytes([0x20] + [0] * 15)) + enc.finalize()
                print(f"McRootKey: {binascii.hexlify(mc_root_key)}")

                request = fuota.CreateDeploymentRequest(deployment=
                                fuota.Deployment(
                                        application_id=args.app_id,
                                        multicast_group_type=fuota.CLASS_C,
                                        multicast_dr=args.dr,
                                        multicast_frequency=get_dl_freq(args.channel),
                                        multicast_group_id=0,
                                        multicast_timeout=12, # actual timeout is (2^timeout) seconds
                                        multicast_region=fuota.US915,
                                        unicast_timeout=duration_pb2.Duration(seconds=60),
                                        unicast_attempt_count=3,
                                        fragmentation_fragment_size=args.fragment_size,
                                        fragmentation_block_ack_delay=1, # ???
                                        fragmentation_redundancy=args.fragment_redundancy,
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
                        for k, v in log.fields.items():
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
deploy.add_argument('-a', '--app-id',
                        default="bd555621-d30a-4fcd-b1b8-c870be7075b7",
                        help="Application UUID")
deploy.add_argument('-e', '--eui',
                        required=True,
                        help="Device EUI as a hex string without spaces")
deploy.add_argument('-k', '--app-key',
                        required=True,
                        help="Device AppKey as a hex string without spaces")
deploy.add_argument('-f', '--firmware',
                        required=True,
                        help="Path to the new firmware")

# Defaults below should not need changing
deploy.add_argument('--dr',
                        type=int,
                        default=8,
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
                        # TODO: should scale with the number of frames to send
                        default=92, # This is the number of *extra* parity blocks to send!
                        help="Fragment redundancy")
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
