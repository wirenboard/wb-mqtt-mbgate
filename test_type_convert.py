#!/usr/bin/env python

import unittest
from struct import pack

import type_convert


class TypeConvert_MQTT2MB_Test(unittest.TestCase):
    def test_integers(self):

        value = 0xABCD
        offset = 10000
        config = { "format": "signed", "size": 4, "scale": 1 }
        result = { offset: 0, offset + 1: 0xABCD }

        self.assertEqual(type_convert.mqtt2modbus(value, config, offset), result)

        value = -1
        config["size"] = 1
        result = { offset: 0xFFFF }
        self.assertEqual(type_convert.mqtt2modbus(value, config, offset), result)

        value = 0xFFFF
        config["size"] = 4
        config["format"] = "unsigned"
        result = { offset: 0, offset + 1: 0xFFFF }
        self.assertEqual(type_convert.mqtt2modbus(value, config, offset), result)

        value = 12345
        config["size"] = 4
        config["format"] = "bcd"
        result = { offset: 0x1, offset + 1: 0x2345 }
        self.assertEqual(type_convert.mqtt2modbus(value, config, offset), result)

        value = 0xABCDEF
        config["wordswap"] = True
        config["format"] = "unsigned"
        result = { offset: 0xCDEF, offset + 1: 0xAB }
        self.assertEqual(type_convert.mqtt2modbus(value, config, offset), result)

        config["byteswap"] = True
        result = { offset: 0xEFCD, offset + 1: 0xAB00 }
        self.assertEqual(type_convert.mqtt2modbus(value, config, offset), result)

        config["wordswap"] = False
        result = { offset: 0xAB00, offset + 1: 0xEFCD }
        self.assertEqual(type_convert.mqtt2modbus(value, config, offset), result)

        value = 100
        config["scale"] = 0.25
        result = { offset: 0, offset + 1: 25 << 8 }
        self.assertEqual(type_convert.mqtt2modbus(value, config, offset), result)

        # convert float to integer
        value = 0.1234
        config["scale"] = 100
        result = { offset: 0, offset + 1: 12 << 8 }
        self.assertEqual(type_convert.mqtt2modbus(value, config, offset), result)

    def test_float(self):

        value = 1.2345  # float: 0x3f9e0419
        config = { "format": "float", "size": 4, "scale": 1 }
        offset = 10024
        result = { offset: 0x3f9e, offset + 1: 0x0419 }
        self.assertEqual(type_convert.mqtt2modbus(value, config, offset), result)

        config["size"] = 8  # for 1.2345 double: 0x3FF3C083126E978D
        result = { offset: 0x3FF3, offset + 1: 0xC083,
                   offset + 2: 0x126E, offset + 3: 0x978D }
        self.assertEqual(type_convert.mqtt2modbus(value, config, offset), result)

    def test_errors(self):

        # check if we fail on wrong size
        value = 102030
        offset = 10002
        config = { "format": "unsigned", "size": 9, "scale": 1 }

        with self.assertRaises(type_convert.ConvertException):
            type_convert.mqtt2modbus(value, config, offset)

        with self.assertRaises(type_convert.ConvertException):
            config["size"] = -2
            type_convert.mqtt2modbus(value, config, offset)

        with self.assertRaises(type_convert.ConvertException):
            config["size"] = 2
            config["format"] = "float"
            type_convert.mqtt2modbus(value, config, offset)

        with self.assertRaises(type_convert.ConvertException):
            config["format"] = "bcd"
            value = -1.234
            type_convert.mqtt2modbus(value, config, offset)

    def test_string(self):

        value = "HelloWorld"
        offset = 10000
        config = { "format": "varchar", "size": 3 }
        result = { offset: ord("H"),
                   offset + 1: ord("e"),
                   offset + 2: ord("l") }

        self.assertEqual(type_convert.mqtt2modbus(value, config, offset), result)

        config["wordswap"] = True
        result = { offset: ord("l"),
                   offset + 1: ord("e"),
                   offset + 2: ord("H") }
        self.assertEqual(type_convert.mqtt2modbus(value, config, offset), result)

        config["byteswap"] = True
        result = { offset: ord("l") << 8,
                   offset + 1: ord("e") << 8,
                   offset + 2: ord("H") << 8 }
        self.assertEqual(type_convert.mqtt2modbus(value, config, offset), result)


class TypeConvert_MB2MQTT_Test(unittest.TestCase):
    def test_integers(self):

        value = 0xABCD
        offset = 10000
        config = { "format": "signed", "size": 4, "scale": 1 }
        result = { offset: 0, offset + 1: 0xABCD }

        self.assertEqual(type_convert.modbus2mqtt(result.values(), config), value)

        value = -1
        config["size"] = 1
        result = { offset: 0xFFFF }
        self.assertEqual(type_convert.modbus2mqtt(result.values(), config), value)

        value = 0xFFFF
        config["size"] = 4
        config["format"] = "unsigned"
        result = { offset: 0, offset + 1: 0xFFFF }
        self.assertEqual(type_convert.modbus2mqtt(result.values(), config), value)

        value = 12345
        config["size"] = 4
        config["format"] = "bcd"
        result = { offset: 0x1, offset + 1: 0x2345 }
        self.assertEqual(type_convert.modbus2mqtt(result.values(), config), value)

        value = 0xABCDEF
        config["wordswap"] = True
        config["format"] = "unsigned"
        result = { offset: 0xCDEF, offset + 1: 0xAB }
        self.assertEqual(type_convert.modbus2mqtt(result.values(), config), value)

        config["byteswap"] = True
        result = { offset: 0xEFCD, offset + 1: 0xAB00 }
        self.assertEqual(type_convert.modbus2mqtt(result.values(), config), value)

        config["wordswap"] = False
        result = { offset: 0xAB00, offset + 1: 0xEFCD }
        self.assertEqual(type_convert.modbus2mqtt(result.values(), config), value)

        value = 100
        config["scale"] = 0.25
        result = { offset: 0, offset + 1: 25 << 8 }
        self.assertEqual(type_convert.modbus2mqtt(result.values(), config), value)

        # convert float to integer
        value = 1
        config["scale"] = 100
        result = { offset: 0, offset + 1: 112 << 8 }
        self.assertEqual(type_convert.modbus2mqtt(result.values(), config), value)

    def test_float(self):

        value = 1.2345  # float: 0x3f9e0419
        config = { "format": "float", "size": 4, "scale": 1 }
        offset = 10024
        result = { offset: 0x3f9e, offset + 1: 0x0419 }
        self.assertAlmostEqual(type_convert.modbus2mqtt(result.values(), config), value, places=4)

        config["size"] = 8  # for 1.2345 double: 0x3FF3C083126E978D
        result = { offset: 0x3FF3, offset + 1: 0xC083,
                   offset + 2: 0x126E, offset + 3: 0x978D }
        self.assertAlmostEqual(type_convert.modbus2mqtt(result.values(), config), value, places=4)

    def test_string(self):

        value = "Hel"
        offset = 10000
        config = { "format": "varchar", "size": 3 }
        result = { offset: ord("H"),
                   offset + 1: ord("e"),
                   offset + 2: ord("l") }

        self.assertEqual(type_convert.modbus2mqtt(result.values(), config), value)

        config["wordswap"] = True
        result = { offset: ord("l"),
                   offset + 1: ord("e"),
                   offset + 2: ord("H") }
        self.assertEqual(type_convert.modbus2mqtt(result.values(), config), value)

        config["byteswap"] = True
        result = { offset: ord("l") << 8,
                   offset + 1: ord("e") << 8,
                   offset + 2: ord("H") << 8 }
        self.assertEqual(type_convert.modbus2mqtt(result.values(), config), value)



if __name__ == "__main__":
    unittest.main()
