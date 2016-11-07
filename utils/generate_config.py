#!/usr/bin/python -d

# TODO: allocation error on address space overflow

from mosquitto import Mosquitto, topic_matches_sub

import json
import sys
import time
import random
import argparse

# Salt for address hashtable in case of match
ADDR_SALT = 7079

# Unit IDs reserved for user
RESERVED_UNIT_IDS = [1, 2]

# Unit IDs reserved by Modbus
RESERVED_UNIT_IDS += range(247, 256)

client = None
table = dict()
old_config = None

coil_formats = ["switch", "alarm", "pushbutton"]
retain_hack_topic = None

config_file = None

hostname = ""
port = 0

mb_hostname = "*"
mb_port = 502

c_debug = False
remap_values = False


class RegDiscr(object):
    def __init__(self, topic, meta_type, enabled=False, address=-1, unitId=-1):
        self.topic = topic
        self.address = address
        self.unitId = unitId
        self.meta_type = meta_type
        self.enabled = enabled

    def getSize(self):
        return 1


class Register(RegDiscr):
    def __init__(self, topic, meta_type, enabled = False, address=-1, unitId=-1,\
        format="signed", size=2, max=0, scale=1, byteswap=False, wordswap=False):

        RegDiscr.__init__(self, topic, meta_type, enabled, address, unitId)
        self.format = format
        self.size = size
        self.max = max
        self.scale = scale
        self.byteswap = byteswap
        self.wordswap = wordswap

    def getSize(self):
        if self.size <= 0:
            return 1
        elif self.format == "varchar":
            return self.size
        else:
            return self.size / 2


class RegSpace:
    def __init__(self, name):
        self.name = name
        self.values = list()
        self.addrs = set()

    def append(self, value):
        if value.address >= 0 and (not remap_values):  # handle this as old value
            for i in range(0, value.getSize()):
                self.addrs.add(value.address + i | (value.unitId << 16))
        else:
            # add new address
            addr_hash = hash(value.topic) & 0xFFFFFF
            while (addr_hash in self.addrs) or ((addr_hash >> 16) != ((addr_hash + value.getSize() - 1) >> 16)) \
                                            or (addr_hash >> 16 in RESERVED_UNIT_IDS):
                addr_hash = (addr_hash + ADDR_SALT) & 0xFFFFFF

            for i in range(0, value.getSize()):
                self.addrs.add(addr_hash + i)

            value.address = addr_hash % 65536
            value.unitId = int(addr_hash / 65536) + 1

        self.values.append(value)

    def getValues(self):
        ret = list()

        for val in self.values:
            ret.append(val.__dict__)

        return ret

regs_discr = RegSpace("discretes")
regs_coils = RegSpace("coils")
regs_input = RegSpace("inputs")
regs_holding = RegSpace("holdings")

regs = {"discretes": regs_discr, "coils": regs_coils, "inputs": regs_input, "holdings": regs_holding}


def get_dev_name(topic):
    lst = topic.split("/")
    return str(lst[2] + "/" + lst[4])


def process_table(table):
    global old_config, regs, hostname, port, mb_hostname, mb_port

    result = dict()

    # old config data
    old_registers = []
    old_topics = []

    result["debug"] = False
    result["modbus"] = {"host": mb_hostname, "port": mb_port}
    result["mqtt"] = {"host": hostname, "port": port}

    if old_config is not None:
        print >>sys.stderr, "Old config is detected"
        for c in old_config["registers"].keys():
            for old_reg in old_config["registers"][c]:
                old_reg["category"] = c
                old_registers.append(old_reg)

        for f in ["debug", "modbus", "mqtt"]:
            if f in old_config:
                result[f] = old_config[f]

    # fill configuration object according to old config
    for reg in old_registers:
        process_channel(reg, reg["topic"])
        old_topics.append(reg["topic"])

    for topic in sorted(table.keys()):
        if topic not in old_topics:
            process_channel(table[topic], topic)
        else:
            print >>sys.stderr, "Topic %s taken from old config" % (topic)

    result["registers"] = dict()
    result["registers"]["remap_values"] = False

    for cat in regs.keys():
        result["registers"][cat] = regs[cat].getValues()

    return result


def process_channel(obj, topic):
    global regs

    if "category" in obj:
        cat = obj["category"]
        del obj["category"]

        if obj["meta_type"] in coil_formats:
            regs[cat].append(RegDiscr(**obj))
        else:
            regs[cat].append(Register(**obj))

        return

    text = False

    if "meta_type" not in obj:
        print "WARNING: Incompete cell " + topic
        return

    if obj["meta_type"] == "text":
        text = True

    if obj["meta_type"] in coil_formats:
        if "readonly" in obj and obj["readonly"]:
            regs["discretes"].append(RegDiscr(topic, obj["meta_type"]))
        else:
            regs["coils"].append(RegDiscr(topic, obj["meta_type"]))
    else:  # double format by default, but changeable by user
        if "readonly" in obj and obj["readonly"]:
            if text:
                regs["inputs"].append(Register(topic, obj["meta_type"], size=-1, format="varchar"))
            else:
                regs["inputs"].append(Register(topic, obj["meta_type"]))
        else:
            if text:
                regs["holdings"].append(Register(topic, obj["meta_type"], size=-1, format="varchar"))
            else:
                regs["holdings"].append(Register(topic, obj["meta_type"]))


# apply MQTT retained hack to collect all MQTT retained values, then die
def mqtt_on_message(arg0, arg1, arg2=None):
    msg = arg2 or arg1

    print >>sys.stderr, msg.topic

    if msg.topic == retain_hack_topic:
        print >>config_file, json.dumps(process_table(table), indent=True)
        sys.exit(0)

    if msg.retain:
        if not topic_matches_sub("/devices/+/controls/name", msg.topic):
            if get_dev_name(msg.topic) not in table:
                table[get_dev_name(msg.topic)] = {} # {"meta_type": "text"}

        dname = get_dev_name(msg.topic)
        if topic_matches_sub("/devices/+/controls/+/meta/type", msg.topic):
            table[dname]["meta_type"] = msg.payload
            # FIXME: crazy read-onlys
            if "readonly" not in table[dname]:
                if msg.payload in ["switch", "pushbutton", "range", "rgb"]:
                    table[dname]["readonly"] = False
                else:
                    table[dname]["readonly"] = True
        elif topic_matches_sub("/devices/+/controls/+", msg.topic):
            table[dname]["value"] = msg.payload
        elif topic_matches_sub("/devices/+/controls/+/meta/readonly", msg.topic):
            if int(msg.payload) == 1:
                table[dname]["readonly"] = True


def main(args=None):

    global old_config, config_file, retain_hack_topic, hostname, port

    if args is None:
        parser = argparse.ArgumentParser(description="Config generator/updater for wb-mqtt-mbgate")
        parser.add_argument("-c", "--config", help="config file to create/update",
                            type=str, default="")
        parser.add_argument("-f", "--force-create", help="force creating new config file",
                            action="store_true")
        parser.add_argument("-s", "--server", help="MQTT server hostname", type=str, default="localhost")
        parser.add_argument("-p", "--port", help="MQTT server port", type=int, default=1883)

        args = parser.parse_args()

    if args.config == "":
        config_file = sys.stdout
    else:
        if not args.force_create:
            try:
                old_config = json.loads(open(args.config, "r").read())
                if "remap_values" in old_config["registers"]:
                    global remap_values
                    remap_values = old_config["registers"]["remap_values"]
                    del old_config["registers"]["remap_values"]

            except:
                print "Failed to open config"
                pass
        config_file = open(args.config, "w")

    client_id = str(time.time()) + str(random.randint(0, 100000))

    client = Mosquitto(client_id)

    hostname = args.server
    port = args.port

    client.connect(args.server, args.port)

    client.on_message = mqtt_on_message

    client.subscribe("/devices/+/controls/+/meta/+")

    # apply retained-hack to be sure that all data is received
    retain_hack_topic = "/tmp/%s/retain_hack" % (client_id)
    client.subscribe(retain_hack_topic)
    client.publish(retain_hack_topic, '1')

    while 1:
        rc = client.loop()
        if rc != 0:
            break

if __name__ == "__main__":
    main()
