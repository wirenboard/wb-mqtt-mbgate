#!/usr/bin/env python

import json
import sys
import argparse

output_file = sys.stdout


def main():
    global output_file

    parser = argparse.ArgumentParser(description="wb-mqtt-mbgate test config generator")

    parser.add_argument("-c", "--config", help="Output file name", type=str, default="")
    parser.add_argument("-n", "--ncontrols", help="Number of flooding controls", type=int, required=True)
    parser.add_argument("-s", "--server", help="Modbus TCP server listen address", type=str, default="*")
    parser.add_argument("-p", "--port", help="Modbus TCP port", type=int, default=502)
    parser.add_argument("--mqtt-server", help="MQTT hostname", type=str, default="localhost")
    parser.add_argument("--mqtt-port", help="MQTT port", default=1883)

    args = parser.parse_args()

    if args.config != "":
        output_file = open(args.config, "w")

    config = {"debug": False}
    config["modbus"] = {"host": args.server, "port": args.port}
    config["mqtt"] = {"host": args.mqtt_server, "port": args.mqtt_port}

    # fill registers
    config["registers"] = {"coils": [], "discretes": [], "inputs": [], "holdings": []}

    inputs = config["registers"]["inputs"]

    for i in range(1, args.ncontrols + 1):
        obj = {"topic": "flood/ctl%d" % (i),
               "unitId": 1,
               "meta_type": "value",
               "enabled": True,
               "address": i - 1,
               "size": 2,
               "format": "unsigned",
               "scale": 0.1,
               "max": 0,
               "byteswap": False,
               "wordswap": False}

        inputs.append(obj)

    # export config as JSON
    print >>output_file, json.dumps(config, indent=True)


if __name__ == "__main__":
    main()
