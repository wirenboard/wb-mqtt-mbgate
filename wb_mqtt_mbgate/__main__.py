#!/usr/bin/env python

from wb_mqtt_mbgate import generate_config, server
import argparse

def main():
    parser = argparse.ArgumentParser("description = Modbus TCP to WirenBoard MQTT gateway")

    gengroup = parser.add_argument_group("Config file generator")
    gengroup.add_argument("-g", "--generate-config", help="generate/update config file and exit",
            action="store_true")
    gengroup.add_argument("--create", help="force generating new config file",
            action="store_true")

    parser.add_argument("-c", "--config", help="config file name", type=str,
            default="/etc/wb-mqtt-mbgate.conf")
    parser.add_argument("-s", "--server", help="MQTT server hostname", type=str,
            default="localhost")
    parser.add_argument("-p", "--port", help="MQTT server port", type=int,
            default=1883)
    parser.add_argument("-v", help="Verbose output", action="store_true")

    args = parser.parse_args()

    if args.generate_config:
        generate_config.main(args)
    else:
        server.main(args)

if __name__ == "__main__":
    main()
