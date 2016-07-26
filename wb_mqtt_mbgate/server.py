#!/usr/bin/env python

# Modbus TCP to MQTT gateway proof-of-concept

from mosquitto import Mosquitto
from threading import RLock
import json
import argparse
import signal
from Queue import Queue
import thread

from pymodbus.server.sync import ModbusTcpServer
from pymodbus.datastore import ModbusSparseDataBlock
from pymodbus.datastore import ModbusSlaveContext, ModbusServerContext

from type_convert import mqtt2modbus, modbus2mqtt

from pprint import pprint
import logging


def dev2topic(t):
    parts = t.split("/")
    return str("/devices/" + parts[0] + "/controls/" + parts[1])


def topic2dev(t):
    parts = t.split("/")
    return str(parts[2] + "/" + parts[4])


def benchmark(func_, get=False):

    from time import clock

    if func_.__name__ not in benchmark.func_dt:
        benchmark.func_dt[func_.__name__] = {"ncalls": 0, "dt": 0}

    def decorator(*args, **kargs):
        benchmark.func_dt["idle"] += clock() - benchmark.last_idle

        dt = clock()
        ret = func_(*args, **kargs)
        benchmark.func_dt[func_.__name__]["dt"] += clock() - dt
        benchmark.func_dt[func_.__name__]["ncalls"] += 1

        benchmark.last_idle = clock()
        return ret

    return decorator

benchmark.func_dt = {"idle": 0}
benchmark.last_idle = 0


def singleton(class_):
    instances = {}

    def getInstance(*args, **kwargs):
        if class_ not in instances:
            instances[class_] = class_(*args, **kwargs)
        return instances[class_]

    return getInstance


class MQTTChannelConfig(dict):
    def __init__(self, *args, **kargs):
        super(MQTTChannelConfig, self).__init__(*args, **kargs)

    def regSize(self):
        f = self["format"] if "format" in self else None
        s = self["size"] if "size" in self else None

        if f is None:
            return 1
        else:
            if f == "varchar":
                return s
            else:
                return s / 2


class MQTTClientException(Exception):
    def __init__(self, msg):
        self.msg = msg

    def __str__(self):
        return repr(self.msg)


@singleton
class MQTTClient(Mosquitto):
    def __init__(self, client_id="", clean_session=True, userdata=None):

        Mosquitto.__init__(self, client_id, clean_session, userdata)

        # init local callback list
        self.DataBlocks = list()
        self.DataQueue = Queue()
        self.DataProcessing = False
        self.on_message = self.__class__.ProcessOnMessage
        self.on_connect = self.__class__.ProcessOnConnect
        self.on_publish = self.__class__.ProcessOnPublish

    def addModbusDataBlock(self, block, single=True):
        if single:
            self.DataBlocks.append(block)
        elif type(block) is list:
            for b in block:
                self.DataBlocks.append(b)
        else:
            raise MQTTClientException("can't add this as datablock")

    def QueueMessage(self, topic, msg, qos=0, retain=True):
        if self.DataProcessing:
            self.DataQueue.put((topic, msg, qos, retain))
        else:
            self.DataProcessing = True
            self.publish(topic, msg, qos, retain)

    # MQTT callbacks
    def ProcessOnMessage(self, userdata, msg):
        # find data block with this topic
        # print "Received message from " + msg.topic

        for datablock in self.DataBlocks:
            if datablock.CheckTopic(topic2dev(msg.topic)):
                datablock.setValues(topic2dev(msg.topic), msg.payload)

    def ProcessOnConnect(self, userdata, rc):
        # Subscribe to all required topics
        for datablock in self.DataBlocks:
            for topic in datablock.RegTopicMap.keys():
                # print "Subscribing on topic " + dev2topic(topic)
                self.subscribe(dev2topic(topic))

    def ProcessOnPublish(self, userdata, mid):
        if self.DataQueue.empty():
            self.DataProcessing = False
        elif self.DataProcessing:
            # Push next message from queue to MQTT
            topic, msg, qos, retain = self.DataQueue.get()
            self.publish(topic, msg, qos, retain)


class MQTTDataBlock(ModbusSparseDataBlock):

    def __init__(self, reg_config):
        self.Config = reg_config

        # Create register maps with topics and addresses as keys
        self.RegTopicMap = {}
        self.RegAddrMap = {}

        for item in self.Config:
            self.RegTopicMap[item["topic"]] = item
            self.RegAddrMap[item["address"]] = item

        # Create Modbus-specified fields
        self.address = 0
        self.default_value = 0

        # Mutex to manipulate data within different threads
        self.DataLock = RLock()

    # Check if topic is in this datablock
    def CheckTopic(self, topic):
        return topic in self.RegTopicMap

    # Internal functions of PyModbus
    @benchmark
    def validate(self, address, count=1):
        address -= 1
        while count > 0:
            if address not in self.RegAddrMap:
                return False

            item_len = self.RegAddrMap[address].regSize()
            count -= item_len
            address += item_len

        return count == 0

    @benchmark
    def getValues(self, address, count=1):
        if isinstance(address, str):  # for MQTT topics
            pass
        else:  # for Modbus access
            address -= 1
            value_container = []
            while count > 0:
                conf = self.RegAddrMap[address]

                with self.DataLock:
                    value_container += conf["mb_value"]

                item_len = conf.regSize()
                count -= item_len
                address += item_len

            return value_container

        return None

    @benchmark
    def setValues(self, address, values):
        """
        Address could be either number or string:
         * number is for Modbus access
         * string is for access from MQTT handler

        In MQTT case, only one value is expected (str)
        """

        value_container = None

        if isinstance(address, str):  # MQTT topic case
            if not isinstance(values, str):
                raise TypeError("Only one string value is expected")
            # try to find specified value
            value_container = self.RegTopicMap[address]

            # convert value to appropriate type
            values = mqtt2modbus(values, value_container)
            with self.DataLock:
                value_container["mb_value"] = values
        else:  # Modbus register case
            address -= 1  # address space should start from 0
            count = len(values)

            offset = 0

            while count > 0:
                value_container = self.RegAddrMap[address]

                item_len = value_container.regSize()

                cur_value = values[offset:offset + item_len]

                with self.DataLock:
                    value_container["mb_value"] = cur_value

                topic = dev2topic(value_container["topic"])
                payload = modbus2mqtt(cur_value, value_container)
                MQTTClient().QueueMessage(topic, payload)

                count -= item_len
                address += item_len
                offset += item_len

        # parse values
        return values


class ContextBuilderException(Exception):
    def __init__(self, msg):
        self.msg = msg

    def __str__(self):
        return repr(self.msg)


class ModbusContextBuilder:
    def __init__(self, config_file_name):
        with open(config_file_name) as data_file:
            self.config = json.load(data_file)
            self.units = dict()

    def _buildUnits(self):
        for block in ["discretes", "coils", "inputs", "holdings"]:
            self._buildBlock(block)

    def _buildBlock(self, block):
        try:
            configs = self.config["registers"][block]
        except KeyError:
            raise ContextBuilderException("no such data block: %s" % (block))

        for item in configs:
            unitId = item["unitId"]
            c = MQTTChannelConfig(item)

            if unitId not in self.units:
                self.units[unitId] = dict()
            if block not in self.units[unitId]:
                self.units[unitId][block] = list()

            if ("enabled" not in item or item["enabled"]) and c.regSize() > 0:
                self.units[unitId][block].append(c)

    def buildContext(self):
        units = dict()

        self._buildUnits()

        for unitId in self.units.keys():
            unitData = self.units[unitId]

            # build data blocks
            _di = MQTTDataBlock(unitData["discretes"])
            _co = MQTTDataBlock(unitData["coils"])
            _ir = MQTTDataBlock(unitData["inputs"])
            _hr = MQTTDataBlock(unitData["holdings"])

            MQTTClient().addModbusDataBlock([_di, _co, _ir, _hr], single=False)

            units[unitId] = ModbusSlaveContext(
                di = _di,
                co = _co,
                ir = _ir,
                hr = _hr)

        context = ModbusServerContext(slaves=units, single=False)

        return context


def main(args=None):

    if args is not None:
        parser = argparse.ArgumentParser(description="Modbus TCP to MQTT gateway")
        parser.add_argument("-c", "--config", help="config file name", type=str,
                    default="/etc/wb-mqtt-mbgate.conf")
        parser.add_argument("-s", "--server", help="MQTT server hostname", type=str,
                    default="localhost")
        parser.add_argument("-p", "--port", help="MQTT server port", type=int,
                    default=1883)
        parser.add_argument("-v", help="Verbose output", action="store_true")

        args = parser.parse_args()

    # configure logging

    modbus_context = ModbusContextBuilder(args.config).buildContext()

    # Run MQTT client in another thread
    mqtt_client = MQTTClient()
    mqtt_client.connect(args.server, args.port)
    mqtt_client.loop_start()

    # Run Modbus in current thread
    modbus_server = ModbusTcpServer(context=modbus_context)

    # signal handlers
    def sighndlr(signum, msg):
        def server_killer():
            print "Shutting down..."
            modbus_server.shutdown()
        thread.start_new_thread(server_killer, ())


    signal.signal(signal.SIGINT, sighndlr)
    signal.signal(signal.SIGTERM, sighndlr)

    try:
        # StartTcpServer(context=modbus_context)
        modbus_server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        print "Stopping..."
        mqtt_client.loop_stop()
        modbus_server.shutdown()

        print "###############################################"
        print "Benchmark"
        pprint(benchmark.func_dt)


if __name__ == "__main__":
    main()
