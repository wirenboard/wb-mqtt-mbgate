#include "mqtt_converters.h"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <string>

#include <iomanip>

#define FLOAT_MAX_WIDTH 15

using namespace std;

/****************************************************************************
 * Helper functions
 */
namespace
{
    void _SwapBytes(uint16_t* data, size_t size)
    {
        for (; size > 0; size--, data++) {
            uint8_t tmp = *data & 0xFF;
            *data >>= 8;
            *data |= tmp << 8;
        }
    }

    void _SwapWords(uint16_t* data, size_t size)
    {
        const int limit = size / 2;
        for (int i = 0; i < limit; i++) {
            swap(data[i], data[size - i - 1]);
        }
    }
    uint64_t _BcdToInt(uint64_t bcd)
    {
        uint64_t result = 0;
        int offset = 1;

        while (bcd) {
            result += (bcd & 0x0F) * offset;
            offset *= 10;
            bcd >>= 4;
        }

        return result;
    }

    uint64_t _IntToBcd(uint64_t val)
    {
        uint64_t bcd = 0;
        int offset = 0;

        while (val) {
            bcd |= (val % 10) << offset;
            offset += 4;
            val /= 10;
        }

        return bcd;
    }
};

/****************************************************************************
 * Discrete types converter
 */
string TMQTTDiscrConverter::Unpack(const void* _data, size_t size) const
{
    const uint8_t* data = static_cast<const uint8_t*>(_data);

    if (*data)
        return string("1");
    else
        return string("0");
}

void* TMQTTDiscrConverter::Pack(const std::string& value, void* _data, size_t size)
{
    uint8_t res = 0;
    if (atoi(value.c_str()))
        res = 1;
    memcpy(_data, &res, 1);
    return _data;
}

/****************************************************************************
 * Integer types converter
 */
string TMQTTIntConverter::Unpack(const void* _data, size_t size) const
{
    const uint16_t* data = static_cast<const uint16_t*>(_data);

    stringstream ss;

    ss.unsetf(ios::floatfield);

#define PROCESS_VALS()                                                                                                 \
    do {                                                                                                               \
        for (unsigned i = 0; i < Size / 2; i++) {                                                                      \
            uval <<= 16; /* NOLINT(clang-diagnostic-shift-count-overflow) */                                           \
            uval |= data[i];                                                                                           \
        }                                                                                                              \
        if (ByteSwap)                                                                                                  \
            _SwapBytes(regs, Size / 2);                                                                                \
        if (WordSwap)                                                                                                  \
            _SwapWords(regs, Size / 2);                                                                                \
        switch (Type) {                                                                                                \
            case SIGNED:                                                                                               \
                ss << val / Scale;                                                                                     \
                break;                                                                                                 \
            case UNSIGNED:                                                                                             \
                ss << uval / Scale;                                                                                    \
                break;                                                                                                 \
            case BCD:                                                                                                  \
                ss << _BcdToInt(uval) / Scale;                                                                         \
                break;                                                                                                 \
        }                                                                                                              \
        return ss.str();                                                                                               \
    } while (0)

    switch (Size) {
        case 2: {
            union
            {
                uint16_t uval;
                int16_t val;
                uint16_t regs[1];
            };

            PROCESS_VALS();
        } break;
        case 4: {
            union
            {
                uint32_t uval;
                int32_t val;
                uint16_t regs[2];
            };

            PROCESS_VALS();
        } break;
        case 8: {
            union
            {
                uint64_t uval;
                int64_t val;
                uint16_t regs[4];
            };

            PROCESS_VALS();
        } break;
    }

#undef PROCESS_VALS

    return "";
}

void* TMQTTIntConverter::Pack(const std::string& value, void* _data, size_t size)
{
    uint16_t* data = static_cast<uint16_t*>(_data);

    stringstream ss;
    ss << value;
    double num_value;
    ss >> num_value;

#define PROCESS_VALS()                                                                                                 \
    do {                                                                                                               \
        if (Type == SIGNED)                                                                                            \
            val = num_value * Scale;                                                                                   \
        else                                                                                                           \
            uval = num_value * Scale;                                                                                  \
        if (Type == BCD)                                                                                               \
            uval = _IntToBcd(uval);                                                                                    \
        if (WordSwap)                                                                                                  \
            _SwapWords(regs, Size / 2);                                                                                \
        if (ByteSwap)                                                                                                  \
            _SwapBytes(regs, Size / 2);                                                                                \
        for (int i = (Size / 2) - 1; i >= 0; i--) {                                                                    \
            data[i] = uval & 0xFFFF;                                                                                   \
            uval >>= 16; /* // NOLINT(clang-diagnostic-shift-count-overflow) */                                        \
        }                                                                                                              \
    } while (0)

    switch (Size) {
        case 2: {
            union
            {
                uint16_t uval;
                int16_t val;
                uint16_t regs[1];
            };

            PROCESS_VALS();
        } break;
        case 4: {
            union
            {
                uint32_t uval;
                int32_t val;
                uint16_t regs[2];
            };

            PROCESS_VALS();
        } break;
        case 8: {
            union
            {
                uint64_t uval;
                int64_t val;
                uint16_t regs[4];
            };

            PROCESS_VALS();
        } break;
    }

    return _data;
}

/****************************************************************************
 * Float types converter
 */
string TMQTTFloatConverter::Unpack(const void* _data, size_t size) const
{
    const uint8_t* data = static_cast<const uint8_t*>(_data);

    stringstream ss;

    switch (Size) {
        case 4: {
            union
            {
                float result;
                uint32_t whole;
                uint16_t regs[2];
            };

            whole = 0;

            for (int i = 0; i < 4; i++) {
                whole <<= 8;
                whole |= data[i];
            }

            if (ByteSwap)
                _SwapBytes(regs, 2);

            if (WordSwap)
                _SwapWords(regs, 2);

            ss << result / Scale;
            return ss.str();
        }
        case 8: {
            union
            {
                double result;
                uint64_t whole;
                uint16_t regs[4];
            };

            whole = 0;

            for (int i = 0; i < 8; i++) {
                whole <<= 8;
                whole |= data[i];
            }

            if (ByteSwap)
                _SwapBytes(regs, 4);

            if (WordSwap)
                _SwapWords(regs, 4);

            ss << result / Scale;
            return ss.str();
        }
    }

    // TODO: error handling
    return "";
}

void* TMQTTFloatConverter::Pack(const std::string& value, void* _data, size_t size)
{
    uint8_t* data = static_cast<uint8_t*>(_data);

    stringstream ss;

    switch (Size) {
        case 4: {
            union
            {
                float result;
                uint32_t whole;
                uint16_t regs[2];
            };

            ss << value;
            ss >> result;
            result *= Scale;

            if (ByteSwap)
                _SwapBytes(regs, 2);

            if (WordSwap)
                _SwapWords(regs, 2);

            for (int i = 3; i >= 0; i--) {
                data[i] = whole & 0xFF;
                whole >>= 8;
            }
            break;
        }
        case 8: {
            union
            {
                double result;
                uint64_t whole;
                uint16_t regs[4];
            };

            ss << value;
            ss >> result;
            result *= Scale;

            if (ByteSwap)
                _SwapBytes(regs, 4);

            if (WordSwap)
                _SwapWords(regs, 4);

            for (int i = 7; i >= 0; i--) {
                data[i] = whole & 0xFF;
                whole >>= 8;
            }
            break;
        }
    }
    return _data;
}

/****************************************************************************
 * String types converter
 */
string TMQTTTextConverter::Unpack(const void* _data, size_t size) const
{
    stringstream ss;
    const char* data = static_cast<const char*>(_data);

    if (WordSwap) {
        for (int i = Size - 1; i >= 0; i--)
            ss << data[(ByteSwap ? (2 * i + 1) : (2 * i))];
    } else {
        for (unsigned i = 0; i < Size; i++)
            ss << data[(ByteSwap ? (2 * i + 1) : (2 * i))];
    }

    return ss.str();
}

void* TMQTTTextConverter::Pack(const std::string& value, void* _data, size_t size)
{
    stringstream ss;
    char* data = static_cast<char*>(_data);

    ss << value << std::setw(Size - value.size()) << std::setfill((char)0) << "";
    
    if (WordSwap) {
        for (int i = Size - 1; i >= 0; i--)
            ss >> data[(ByteSwap ? (2 * i + 1) : (2 * i))];
    } else {
        for (unsigned i = 0; i < Size; i++)
            ss >> data[(ByteSwap ? (2 * i + 1) : (2 * i))];
    }

    return _data;
}
