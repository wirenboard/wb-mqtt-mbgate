#!/usr/bin/env python

from struct import pack, unpack


class ConvertException(Exception):
    def __init__(self, value):
        self.value = value

    def __str__(self):
        return repr(self.value)

def int2bcd(int_value):
    result = 0
    offset = 0
    while int_value != 0:
        result |= (int_value % 10) << offset
        int_value /= 10
        offset += 4
    return result

def bcd2int(bcd_value):
    result = 0
    offset = 1
    while bcd_value != 0:
        result += (bcd_value & 0xF) * offset
        offset *= 10
        bcd_value >>= 4
    return result

def mqtt2modbus(value, config, offset = None):
    ret = None
    format = config["format"]
    size = config["size"]

    if format != "varchar":
        scale = config["scale"]

    str_format = ">"  # big endian first

    if format in ["signed", "unsigned", "bcd"]:
        if format == "bcd":
            if value < 0:
                raise ConvertException("bcd can't be negative: %d" % (value))
            value = int2bcd(int(float(value) * scale))
        else:
            value = int(float(value) * scale)

        if size in [1, 2]:  # doesn't matter for Modbus
            if format == "signed":
                str_format += "h"
            else:
                str_format += "H"
        elif size in [3, 4]:
            if format == "signed":
                str_format += "i"
            else:
                str_format += "I"
        elif size in [5, 6, 7, 8]:
            if format == "signed":
                str_format += "q"
            else:
                str_format += "Q"
        else:
            raise ConvertException("integer too large: %d" % (size))
    elif format == "float":
        value = float(value) * scale
        if size == 4:
            str_format += "f"
        elif size == 8:
            str_format += "d"
        else:
            raise ConvertException("wrong float size: %d" % (size))
    elif format == "varchar":
        # each symbol in string stored in 1 register
        old_value = str(value)
        value = ""

        for c in old_value:
            value += pack("xc", c)

        str_format += str(int(size * 2)) + "s"

    ret = pack(str_format, value)

    # deal with endiness
    values_list = list()

    if "wordswap" in config and config["wordswap"]:
        values_list = [ret[i-2:i] for i in range(len(ret), 0, -2)]
    else:
        values_list = [ret[i:i+2] for i in range(0, len(ret), 2)]

    if "byteswap" in config and config["byteswap"]:
        old_values = list(values_list)
        values_list = []
        for val in old_values:
            values_list.append(val[::-1])

    # convert values to integers
    for i in range(0, len(values_list)):
        values_list[i] = unpack(">H", values_list[i])[0]

    # required map from addresses to registers
    # or not, I don't really know by now
    if offset is None:
        if "address" in config:
            offset = config["address"]
        else:
            raise ConvertException("No offset is defined neither in config nor in args")

    return dict(zip(range(offset, offset + len(values_list)), values_list))


def modbus2mqtt(values, config):
    ret = None
    format = config["format"]
    size = config["size"]

    if format != "varchar":
        scale = config["scale"]

    byteswap = False
    if "byteswap" in config and config["byteswap"]:
        byteswap = True

    wordswap = False
    if "wordswap" in config and config["wordswap"]:
        wordswap = True

    # convert data from raw registers to struct
    raw_value = ""

    for v in values[::(-1 if wordswap else 1)]:
        fmt = "<H" if byteswap else ">H"
        raw_value += pack(fmt, v)

    # unpack struct
    fmt = ">"
    if format in ["signed", "unsigned", "bcd"]:
        if size in [1,2]:
            if format == "signed":
                fmt += "h"
            else:
                fmt += "H"
        elif size in [3,4]:
            if format == "signed":
                fmt += "i"
            else:
                fmt += "I"
        elif size in [5, 6, 7, 8]:
            if format == "signed":
                fmt += "q"
            else:
                fmt += "Q"
        else:
            raise ConvertException("integer too large: %d" % (size))
    elif format == "float":
        if size == 4:
            fmt += "f"
        elif size == 8:
            fmt += "d"
        else:
            raise ConvertException("wrong float size: %d" % (size))
    elif format == "varchar":
        raw_value = raw_value[1::2]  # remove padding bytes
        fmt += str(size) + "s"

    ret = unpack(fmt, raw_value)[0]

    if format == "bcd":
        ret = bcd2int(ret)

    if format != "varchar":
        ret /= scale

    return ret
