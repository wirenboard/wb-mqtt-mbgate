#include "rtu_frame_parser.h"

#include <modbus/modbus.h>

namespace
{
    const size_t MIN_REQUEST_LENGTH = 4;

    uint16_t Crc16(const uint8_t* data, size_t size)
    {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < size; ++i) {
            crc ^= data[i];
            for (int bit = 0; bit < 8; ++bit) {
                crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
            }
        }
        return crc;
    }

    bool IsStandardFunction(uint8_t function)
    {
        switch (function) {
            case MODBUS_FC_READ_COILS:
            case MODBUS_FC_READ_DISCRETE_INPUTS:
            case MODBUS_FC_READ_HOLDING_REGISTERS:
            case MODBUS_FC_READ_INPUT_REGISTERS:
            case MODBUS_FC_WRITE_SINGLE_COIL:
            case MODBUS_FC_WRITE_SINGLE_REGISTER:
            case MODBUS_FC_WRITE_MULTIPLE_COILS:
            case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
            case MODBUS_FC_MASK_WRITE_REGISTER:
            case MODBUS_FC_WRITE_AND_READ_REGISTERS:
                return true;
            default:
                return false;
        }
    }

    size_t GetRequestLength(const uint8_t* data, size_t size)
    {
        size_t length = 0;
        size_t countIndex = 0;

        switch (data[1]) {
            case MODBUS_FC_READ_COILS:
            case MODBUS_FC_READ_DISCRETE_INPUTS:
            case MODBUS_FC_READ_HOLDING_REGISTERS:
            case MODBUS_FC_READ_INPUT_REGISTERS:
            case MODBUS_FC_WRITE_SINGLE_COIL:
            case MODBUS_FC_WRITE_SINGLE_REGISTER:
                return 8;
            case MODBUS_FC_MASK_WRITE_REGISTER:
                return 10;
            case MODBUS_FC_WRITE_MULTIPLE_COILS:
            case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
                length = 9;
                countIndex = 6;
                break;
            case MODBUS_FC_WRITE_AND_READ_REGISTERS:
                length = 13;
                countIndex = 10;
                break;
            default:
                return MIN_REQUEST_LENGTH;
        }

        return countIndex < size ? length + data[countIndex] : 0;
    }
}

void TRTUFrameParser::Append(const uint8_t* data, size_t size)
{
    Buffer.insert(Buffer.end(), data, data + size);
}

void TRTUFrameParser::Clear()
{
    Buffer.clear();
    Aligned = true;
}

bool TRTUFrameParser::Empty() const
{
    return Buffer.empty();
}

std::vector<uint8_t> TRTUFrameParser::NextFrame()
{
    std::vector<uint8_t> frame;
    size_t skip = 0;

    for (size_t offset = 0; offset + MIN_REQUEST_LENGTH <= Buffer.size(); offset++) {
        size_t length = GetRequestLength(Buffer.data() + offset, Buffer.size() - offset);
        bool boundary = (Aligned && offset == 0);

        if (length <= MODBUS_RTU_MAX_ADU_LENGTH && (boundary || IsStandardFunction(Buffer[offset + 1]))) {
            if (length == 0 || offset + length > Buffer.size()) {
                continue;
            }
            const uint8_t* data = Buffer.data() + offset;
            if (Crc16(data, length - 2) == (data[length - 2] | (data[length - 1] << 8))) {
                frame.assign(data, data + length);
                Buffer.erase(Buffer.begin(), Buffer.begin() + offset + length);
                Aligned = true;
                return frame;
            }
        }

        if (skip == offset) {
            skip++;
        }
    }

    if (skip) {
        Buffer.erase(Buffer.begin(), Buffer.begin() + skip);
        Aligned = false;
    }

    return frame;
}
