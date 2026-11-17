#pragma once

/*!
 * \file rtu_frame_parser.h
 * \brief Parses Modbus RTU requests from a shared bus byte stream
 */

#include <cstddef>
#include <cstdint>
#include <vector>

/*!
 * Looks for requests with valid CRC at any position of received data, so responses
 * of other slaves on the bus don't break frame boundaries as in libmodbus
 */
class TRTUFrameParser
{
public:
    void Append(const uint8_t* data, size_t size);
    void Clear();
    bool Empty() const;

    // Returns next master request, other slaves responses are skipped
    std::vector<uint8_t> NextRequest();

private:
    std::vector<uint8_t> Buffer;

    // Buffer starts after line silence or after previous request
    bool Aligned = true;
};
