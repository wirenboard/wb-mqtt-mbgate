#!/usr/bin/env python

import argparse
import json
import logging
import threading
import sys
import signal
from time import sleep, time

from pymodbus.client.sync import ModbusTcpClient as ModbusClient


def poll_range(num, den):
    ret = [den] * (num / den)
    if num % den > 0:
        ret.append(num % den)
    return ret


def modbus_poller(id, stop_event, config):
    log = logging.getLogger("worker-" + str(id))
    log.setLevel(logging.DEBUG)

    client = ModbusClient(host=config["address"], port=config["port"])
    client.connect()

    log.info("Worker started")

    t_max = -1

    while not stop_event.is_set():
        log.info("Poller turn")

        t0 = time()

        address = 0
        for i in poll_range(config["num_controls"], config["chunk_size"]):
            result = client.read_input_registers(address=address, count=i, unit=1)
            address += i
            if result is not None:
                if result.function_code >= 0x80 and result.function_code != 132:
                    log.warn("Server returned error!")
                    print result
                    stop_event.set()
                    break
                elif result.function_code == 132:
                    print "Server fault: " + str(result)
                    sleep(1)
                    break

        t = time() - t0
        log.info("Request took " + str(t) + " s")

        if t > t_max:
            t_max = t

        sleep(config["thread"]["interval"])

    log.info("Worker shutting down")
    log.info("Max worker process time: " + str(t_max))
    client.close()


def validate_config(config):
    ret = "address" in config and "port" in config and "num_threads" in config and "thread" in config

    if ret:
        thread = config["thread"]
        ret = "actions" in thread and type(thread["actions"]) is list
        ret = ret and "interval" in thread

    return ret


class StopperHandler:
    def __init__(self, event):
        self.ev = event

    def __call__(self, signum, frame):
        print "Signal received"
        self.ev.set()


def main():

    # configure logging
    logging.basicConfig()
    log = logging.getLogger()
    log.setLevel(logging.INFO)

    # parse arguments
    parser = argparse.ArgumentParser(description="Modbus TCP test and benchmark utility")
    parser.add_argument("-c", "--config", help="poller config file", type=str, required=True)
    parser.add_argument("-s", "--server", help="Modbus server address", type=str, required=True)
    parser.add_argument("-p", "--port", help="Modbus server port", type=int, required=True)
    parser.add_argument("-t", "--nthreads", help="Number of polling threads", type=int, required=True)
    parser.add_argument("-f", "--fq", help="Polling frequency", type=float, required=True)
    parser.add_argument("-n", "--ncontrols", help="Number of registers", type=int, required=True)
    parser.add_argument("--chunk-size", help="Number of registers to read at once", type=int, default=50)

    args = parser.parse_args()

    # open config file
    config = json.loads(open(args.config).read())

    config["address"] = args.server
    config["port"] = args.port
    config["num_threads"] = args.nthreads
    config["num_controls"] = args.ncontrols
    config["chunk_size"] = args.chunk_size

    if abs(args.fq) < 0.00001:
        print >>sys.stderr, "Warning: limit frequency to 0.00001 Hz (10s interval)"
        args.fq = 0.00001

    config["thread"]["interval"] = 1 / args.fq

    # check if all required fiels are here
    # if not validate_config(config):
        # print >>sys.stderr, "ERROR: Wrong config format"
        # sys.exit(1)

    # init threads and exit event
    ev_exit = threading.Event()

    # create signal handlers to stop
    sighndlr = StopperHandler(ev_exit)
    signal.signal(signal.SIGINT, sighndlr)
    signal.signal(signal.SIGTERM, sighndlr)


    threads = []
    for i in range(config["num_threads"]):
        threads.append(threading.Thread(target=modbus_poller, args=(i, ev_exit, config)))

    # start threads
    for i in range(len(threads)):
        threads[i].start()

    # just wait for stop event
    while not ev_exit.is_set():
        sleep(0.1)

    log.info("Time to sleep, snakeys")

    # join all threads
    for i in range(len(threads)):
        threads[i].join()

    # that's all


if __name__ == "__main__":
    main()
