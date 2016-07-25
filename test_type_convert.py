#!/usr/bin/env python

import unittest
from struct import pack

import type_convert


class TypeConvert_MQTT2MB_Test(unittest.TestCase):
    def test_integers(self):

        value = 0xABCD
        config = { "format": "signed", "size": 4, "scale": 1 }
        result = [ 0, 0xABCD ]

        self.assertEqual(type_convert.mqtt2modbus(value, config), result)

        value = -1
        config["size"] = 1
        result = [  0xFFFF ]
        self.assertEqual(type_convert.mqtt2modbus(value, config), result)

        value = 0xFFFF
        config["size"] = 4
        config["format"] = "unsigned"
        result = [  0,  0xFFFF ]
        self.assertEqual(type_convert.mqtt2modbus(value, config), result)

        value = 12345
        config["size"] = 4
        config["format"] = "bcd"
        result = [  0x1,  0x2345 ]
        self.assertEqual(type_convert.mqtt2modbus(value, config), result)

        value = 0xABCDEF
        config["wordswap"] = True
        config["format"] = "unsigned"
        result = [  0xCDEF,  0xAB ]
        self.assertEqual(type_convert.mqtt2modbus(value, config), result)

        config["byteswap"] = True
        result = [  0xEFCD,  0xAB00 ]
        self.assertEqual(type_convert.mqtt2modbus(value, config), result)

        config["wordswap"] = False
        result = [  0xAB00,  0xEFCD ]
        self.assertEqual(type_convert.mqtt2modbus(value, config), result)

        value = 100
        config["scale"] = 0.25
        result = [  0,  25 << 8 ]
        self.assertEqual(type_convert.mqtt2modbus(value, config), result)

        # convert float to integer
        value = 0.1234
        config["scale"] = 100
        result = [  0,  12 << 8 ]
        self.assertEqual(type_convert.mqtt2modbus(value, config), result)

    def test_float(self):

        value = 1.2345  # float: 0x3f9e0419
        config = { "format": "float", "size": 4, "scale": 1 }

        result = [  0x3f9e,  0x0419 ]
        self.assertEqual(type_convert.mqtt2modbus(value, config), result)

        config["size"] = 8  # for 1.2345 double: 0x3FF3C083126E978D
        result = [  0x3FF3,  0xC083,
                    0x126E,  0x978D ]
        self.assertEqual(type_convert.mqtt2modbus(value, config), result)

    def test_errors(self):

        # check if we fail on wrong size
        value = 102030

        config = { "format": "unsigned", "size": 9, "scale": 1 }

        with self.assertRaises(type_convert.ConvertException):
            type_convert.mqtt2modbus(value, config)

        with self.assertRaises(type_convert.ConvertException):
            config["size"] = -2
            type_convert.mqtt2modbus(value, config)

        with self.assertRaises(type_convert.ConvertException):
            config["size"] = 2
            config["format"] = "float"
            type_convert.mqtt2modbus(value, config)

        with self.assertRaises(type_convert.ConvertException):
            config["format"] = "bcd"
            value = -1.234
            type_convert.mqtt2modbus(value, config)

    def test_string(self):

        value = "HelloWorld"

        config = { "format": "varchar", "size": 3 }
        result = [  ord("H"),
                    ord("e"),
                    ord("l") ]

        self.assertEqual(type_convert.mqtt2modbus(value, config), result)

        config["wordswap"] = True
        result = [  ord("l"),
                    ord("e"),
                    ord("H") ]
        self.assertEqual(type_convert.mqtt2modbus(value, config), result)

        config["byteswap"] = True
        result = [  ord("l") << 8,
                    ord("e") << 8,
                    ord("H") << 8 ]
        self.assertEqual(type_convert.mqtt2modbus(value, config), result)


class TypeConvert_MB2MQTT_Test(unittest.TestCase):
    def test_integers(self):

        value = 0xABCD

        config = { "format": "signed", "size": 4, "scale": 1 }
        result = [  0,  0xABCD ]

        self.assertEqual(type_convert.modbus2mqtt(result, config), value)

        value = -1
        config["size"] = 1
        result = [  0xFFFF ]
        self.assertEqual(type_convert.modbus2mqtt(result, config), value)

        value = 0xFFFF
        config["size"] = 4
        config["format"] = "unsigned"
        result = [  0,  0xFFFF ]
        self.assertEqual(type_convert.modbus2mqtt(result, config), value)

        value = 12345
        config["size"] = 4
        config["format"] = "bcd"
        result = [  0x1,  0x2345 ]
        self.assertEqual(type_convert.modbus2mqtt(result, config), value)

        value = 0xABCDEF
        config["wordswap"] = True
        config["format"] = "unsigned"
        result = [  0xCDEF,  0xAB ]
        self.assertEqual(type_convert.modbus2mqtt(result, config), value)

        config["byteswap"] = True
        result = [  0xEFCD,  0xAB00 ]
        self.assertEqual(type_convert.modbus2mqtt(result, config), value)

        config["wordswap"] = False
        result = [  0xAB00,  0xEFCD ]
        self.assertEqual(type_convert.modbus2mqtt(result, config), value)

        value = 100
        config["scale"] = 0.25
        result = [  0,  25 << 8 ]
        self.assertEqual(type_convert.modbus2mqtt(result, config), value)

        # convert float to integer
        value = 1
        config["scale"] = 100
        result = [  0,  112 << 8 ]
        self.assertEqual(type_convert.modbus2mqtt(result, config), value)

    def test_float(self):

        value = 1.2345  # float: 0x3f9e0419
        config = { "format": "float", "size": 4, "scale": 1 }

        result = [  0x3f9e,  0x0419 ]
        self.assertAlmostEqual(type_convert.modbus2mqtt(result, config), value, places=4)

        config["size"] = 8  # for 1.2345 double: 0x3FF3C083126E978D
        result = [  0x3FF3,  0xC083,
                    0x126E,  0x978D ]
        self.assertAlmostEqual(type_convert.modbus2mqtt(result, config), value, places=4)

    def test_string(self):

        value = "Hel"

        config = { "format": "varchar", "size": 3 }
        result = [  ord("H"),
                    ord("e"),
                    ord("l") ]

        self.assertEqual(type_convert.modbus2mqtt(result, config), value)

        config["wordswap"] = True
        result = [  ord("l"),
                    ord("e"),
                    ord("H") ]
        self.assertEqual(type_convert.modbus2mqtt(result, config), value)

        config["byteswap"] = True
        result = [  ord("l") << 8,
                    ord("e") << 8,
                    ord("H") << 8 ]
        self.assertEqual(type_convert.modbus2mqtt(result, config), value)



if __name__ == "__main__":
    unittest.main()
