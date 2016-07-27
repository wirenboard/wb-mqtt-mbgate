#!/usr/bin/python -d

from mosquitto import Mosquitto, topic_matches_sub

import json
import sys
import time
import random
import argparse


client = None
table = dict()
old_config = None

coil_formats = ["switch", "alarm", "pushbutton"]
retain_hack_topic = None

config_file = None


class RegDiscr(object):
    def __init__(self, topic, meta_type, enabled = False, address=-1, unitId=-1):
        self.topic = topic
        self.address = address
        self.unitId = unitId
        self.meta_type = meta_type
        self.enabled = enabled

    def getSize(self):
        return 1


class Register(RegDiscr):
    def __init__(self, topic, meta_type, enabled = False, address=-1, unitId=-1,
    format="double", size=8, max=0, scale=1, byteswap=False, wordswap=False):
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
        self.address = 0

    def append(self, value):
        if value.address >= 0:  # handle this as old value
            if self.address < value.address + value.getSize():
                self.address = value.address + value.getSize()
        else:
            value.address = self.address % 65536
            value.unitId = int(self.address / 65536) + 1
            self.address += value.getSize()

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

regs = { "discretes": regs_discr, "coils": regs_coils, "inputs": regs_input, "holdings": regs_holding }

def get_dev_name(topic):
    lst = topic.split("/")
    return str(lst[2] + "/" + lst[4])


def process_table(table):
    global old_config, regs

    result = dict()

    # old config data
    old_registers = []
    old_topics = []

    result["address"] = "127.0.0.1"
    result["port"] = 502
    result["debug"] = False

    if old_config is not None:
        print >>sys.stderr, "Old config is detected"
        for c in old_config["registers"].keys():
            for old_reg in old_config["registers"][c]:
                old_reg["category"] = c
                old_registers.append(old_reg)

        for f in ["address", "port", "debug"]:
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
                table[get_dev_name(msg.topic)] = {}

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

    global old_config, config_file, retain_hack_topic

    if args is None:
        parser = argparse.ArgumentParser(description="Config generator/updater for wb-mqtt-mbgate")
        parser.add_argument("-c", "--config", help="config file to create/update",
                            type=str, default="")
        parser.add_argument("--create", help="force creating new config file",
                            action="store_true")

        args = parser.parse_args()

    if args.config == "":
        config_file = sys.stdout
    else:
        if not args.create:
            try:
                old_config = json.loads(open(args.config, "r").read())
            except:
                pass
        config_file = open(args.config, "w")

    client_id = str(time.time()) + str(random.randint(0, 100000))

    client = Mosquitto(client_id)
    client.connect("localhost", 1883)

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
