#!/usr/bin/env python

from setuptools import setup, find_packages
from os.path import join, dirname

setup(
    name = "wb-mqtt-mbgate",
    long_description = open(join(dirname(__file__), "README.md")).read(),
    version = "1.0.0",
    packages = find_packages(),
    entry_points = {
        "console_scripts":
            ["wb-mqtt-mbgate=wb_mqtt_mbgate.__main__:main"]
    },
    test_suite = "test_type_convert"
    )
