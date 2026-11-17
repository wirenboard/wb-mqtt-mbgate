#include <gtest/gtest.h>

#include "rtu_frame_parser.h"

#include <vector>

using namespace std;

namespace
{
    typedef vector<uint8_t> TBytes;

    TBytes WithCrc(TBytes data)
    {
        uint16_t crc = 0xFFFF;
        for (auto b: data) {
            crc ^= b;
            for (int bit = 0; bit < 8; ++bit) {
                crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
            }
        }
        data.push_back(crc & 0xFF);
        data.push_back(crc >> 8);
        return data;
    }

    TBytes ReadRequest(uint8_t slave, uint8_t function, uint16_t address, uint16_t count)
    {
        return WithCrc({slave,
                        function,
                        uint8_t(address >> 8),
                        uint8_t(address & 0xFF),
                        uint8_t(count >> 8),
                        uint8_t(count & 0xFF)});
    }

    TBytes ReadRegistersResponse(uint8_t slave, uint8_t function, uint16_t count)
    {
        TBytes data{slave, function, uint8_t(count * 2)};
        for (uint16_t i = 0; i < count * 2; ++i) {
            data.push_back(uint8_t(0x5D + i));
        }
        return WithCrc(data);
    }

    TBytes operator+(TBytes a, const TBytes& b)
    {
        a.insert(a.end(), b.begin(), b.end());
        return a;
    }
}

class TRTUFrameParserTest: public ::testing::Test
{
protected:
    void Append(const TBytes& data)
    {
        Parser.Append(data.data(), data.size());
    }

    vector<TBytes> ReadAll()
    {
        vector<TBytes> frames;
        for (auto frame = Parser.NextRequest(); !frame.empty(); frame = Parser.NextRequest()) {
            frames.push_back(frame);
        }
        return frames;
    }

    TRTUFrameParser Parser;
};

TEST_F(TRTUFrameParserTest, SingleRequest)
{
    auto request = ReadRequest(11, 0x04, 3001, 6);
    Append(request);

    EXPECT_EQ(ReadAll(), vector<TBytes>{request});
    EXPECT_TRUE(Parser.Empty());
}

TEST_F(TRTUFrameParserTest, RequestInChunks)
{
    auto request = ReadRequest(11, 0x03, 100, 2);
    Append(TBytes(request.begin(), request.begin() + 3));
    EXPECT_TRUE(ReadAll().empty());

    Append(TBytes(request.begin() + 3, request.end()));
    EXPECT_EQ(ReadAll(), vector<TBytes>{request});
}

TEST_F(TRTUFrameParserTest, SeveralRequests)
{
    auto first = ReadRequest(11, 0x02, 1001, 3);
    auto second = ReadRequest(12, 0x01, 2001, 2);
    Append(first + second);

    EXPECT_EQ(ReadAll(), (vector<TBytes>{first, second}));
}

TEST_F(TRTUFrameParserTest, WriteMultipleRegisters)
{
    auto request = WithCrc({11, 0x10, 0x00, 0x10, 0x00, 0x02, 0x04, 0x01, 0x02, 0x03, 0x04});
    Append(TBytes(request.begin(), request.begin() + 8));
    EXPECT_TRUE(ReadAll().empty());

    Append(TBytes(request.begin() + 8, request.end()));
    EXPECT_EQ(ReadAll(), vector<TBytes>{request});
}

TEST_F(TRTUFrameParserTest, RequestAfterOtherSlaveResponse)
{
    auto otherRequest = ReadRequest(12, 0x04, 23001, 6);
    auto otherResponse = ReadRegistersResponse(12, 0x04, 6);
    auto request = ReadRequest(11, 0x04, 3001, 6);

    Append(otherRequest);
    EXPECT_EQ(ReadAll(), vector<TBytes>{otherRequest});

    Append(otherResponse);
    EXPECT_TRUE(ReadAll().empty());

    Append(request);
    EXPECT_EQ(ReadAll(), vector<TBytes>{request});
    EXPECT_TRUE(Parser.Empty());
}

TEST_F(TRTUFrameParserTest, RequestAfterShortResponse)
{
    auto otherResponse = WithCrc({12, 0x02, 0x01, 0x05});
    auto request = ReadRequest(11, 0x01, 2001, 2);
    Append(otherResponse + request);

    EXPECT_EQ(ReadAll(), vector<TBytes>{request});
}

TEST_F(TRTUFrameParserTest, RequestAfterLongResponse)
{
    auto otherResponse = ReadRegistersResponse(12, 0x04, 64);
    auto request = ReadRequest(11, 0x04, 3100, 64);
    Append(otherResponse);
    EXPECT_TRUE(ReadAll().empty());
    Append(request);

    EXPECT_EQ(ReadAll(), vector<TBytes>{request});
}

TEST_F(TRTUFrameParserTest, GarbageIsDropped)
{
    Append(TBytes(300, 0xFF));
    EXPECT_TRUE(ReadAll().empty());

    auto request = ReadRequest(11, 0x03, 0, 1);
    Append(request);
    EXPECT_EQ(ReadAll(), vector<TBytes>{request});
    EXPECT_TRUE(Parser.Empty());
}

TEST_F(TRTUFrameParserTest, UnsupportedFunctionAtFrameBoundary)
{
    auto request = WithCrc({11, 0x11});
    Append(request);
    EXPECT_EQ(ReadAll(), vector<TBytes>{request});

    Append(TBytes{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07} + request);
    EXPECT_TRUE(ReadAll().empty());

    Parser.Clear();
    Append(request);
    EXPECT_EQ(ReadAll(), vector<TBytes>{request});
}
