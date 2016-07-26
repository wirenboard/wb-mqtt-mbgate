#!/usr/bin/python -d

from mosquitto import Mosquitto, topic_matches_sub

import json
import sys
import time
import random
import argparse


client = None
table = dict()

coil_formats = ["switch", "alarm", "pushbutton"]
retain_hack_topic = None

config_file = None


class RegDiscr(object):
    def __init__(self, topic, meta_type):
        self.topic = topic
        self.address = -1
        self.unitId = -1
        self.meta_type = meta_type
        self.enabled = False


class Register(RegDiscr):
    def __init__(self, topic, meta_type, format="double", size=8, max=0, scale=1, byteswap=False, wordswap=False):
        RegDiscr.__init__(self, topic, meta_type)
        self.format = format
        self.size = size
        self.max = max
        self.scale = scale
        self.byteswap = byteswap
        self.wordswap = wordswap


class RegSpace:
    def __init__(self, name):
        self.name = name
        self.values = list()
        self.address = 0

    def append(self, value):
        value.address = self.address % 65536
        value.unitId = int(self.address / 65536) + 1
        if isinstance(value, Register) and value.size > 0:
            self.address += int(value.size) / 2
        else:
            self.address += 1
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


def get_dev_name(topic):
    lst = topic.split("/")
    return str(lst[2] + "/" + lst[4])


def process_table(table):
    result = dict()
    result["address"] = "127.0.0.1"
    result["port"] = 502

    for topic in table.keys():
        process_channel(table[topic], topic)

    result["registers"] = dict()
    result["registers"]["discretes"] = regs_discr.getValues()
    result["registers"]["coils"] = regs_coils.getValues()
    result["registers"]["inputs"] = regs_input.getValues()
    result["registers"]["holdings"] = regs_holding.getValues()

    return result


def process_channel(obj, topic):
    global regs_discr, regs_coils, regs_input, regs_holding

    text = False

    if obj["meta_type"] == "text":
        text = True

    if obj["meta_type"] in coil_formats:
        if "readonly" in obj and obj["readonly"]:
            regs_discr.append(RegDiscr(topic, obj["meta_type"]))
        else:
            regs_coils.append(RegDiscr(topic, obj["meta_type"]))
    else:  # double format by default, but changeable by user
        if "readonly" in obj and obj["readonly"]:
            if text:
                regs_input.append(Register(topic, obj["meta_type"], size=-1, format="varchar"))
            else:
                regs_input.append(Register(topic, obj["meta_type"]))
        else:
            if text:
                regs_holding.append(Register(topic, obj["meta_type"], size=-1, format="varchar"))
            else:
                regs_holding.append(Register(topic, obj["meta_type"]))


# apply MQTT retained hack to collect all MQTT retained values, then die
def mqtt_on_message(arg0, arg1, arg2=None):
    msg = arg2 or arg1

    if msg.topic == retain_hack_topic:
        print >>config_file, json.dumps(process_table(table), indent=True)
        sys.exit(0)

    if msg.retain:
        print >>sys.stderr, msg.topic

        if not topic_matches_sub("/devices/+/controls/name", msg.topic):
            if get_dev_name(msg.topic) not in table:
                table[get_dev_name(msg.topic)] = {"new": True}

        dname = get_dev_name(msg.topic)
        if not table[dname]["new"]:
            if topic_matches_sub("/devices/+/controls/+/meta/type", msg.topic):
                table[dname]["meta_type"] = msg.payload
            elif topic_matches_sub("/devices/+/controls/+", msg.topic):
                table[dname]["value"] = msg.payload
            elif topic_matches_sub("/devices/+/controls/+/meta/readonly", msg.topic):
                if int(msg.payload) != 0:
                    table[dname]["readonly"] = True


def main(args=None):

    global table, config_file

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
                table = json.loads(open(args.config, "r").read())
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
