#!/usr/bin/env python

import unittest
from struct import pack

import type_convert


class TypeConvertTest(unittest.TestCase):
    def test_mb2mqtt_integers(self):

        value = 0xABCD
        offset = 10000
        config = { "format": "signed", "size": 4, "scale": 1 }
        result = { offset: pack(">H", 0), offset + 1: pack(">H", 0xABCD) }

        self.assertEqual(type_convert.mqtt2modbus(value, config, offset), result)

        value = -1
        config["size"] = 1
        result = { offset: pack(">H", 0xFFFF) }
        self.assertEqual(type_convert.mqtt2modbus(value, config, offset), result)

        value = 0xFFFF
        config["size"] = 4
        config["format"] = "unsigned"
        result = { offset: pack(">H", 0), offset + 1: pack(">H", 0xFFFF) }
        self.assertEqual(type_convert.mqtt2modbus(value, config, offset), result)

        value = 12345
        config["size"] = 4
        config["format"] = "bcd"
        result = { offset: pack(">H", 0x1), offset + 1: pack(">H", 0x2345) }
        self.assertEqual(type_convert.mqtt2modbus(value, config, offset), result)

    def test_mb2mqtt_string(self):

        value = "HelloWorld"
        offset = 10000
        config = { "format": "varchar", "size": 6 }
        result = { }

        self.assertEqual(type_convert.mqtt2modbus(value, config, offset), result)


if __name__ == "__main__":
    unittest.main()
