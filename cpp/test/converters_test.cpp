#include "mqtt_converters.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <string>
#include <cstring>

using namespace std;
using ::testing::ElementsAre;

class MQTTConvertersTest : public ::testing::Test
{
public:
    void SetUp() {}
    void TearDown() {}
};

TEST_F(MQTTConvertersTest, FloatTest)
{
    TMQTTFloatConverter f(4), fb(4, true), fw(4, false, true), fbw(4, true, true);
    TMQTTFloatConverter d(8), db(8, true), dw(8, false, true), dbw(8, true, true);

    /* float a = -1.2345; // 0xBF9E0419 */
    /* double b = 1.23456; // 0x3FF3C0C1FC8F3238 */
    string a = "-1.2345";
    string b = "1.23456";

    string result;

    uint8_t fbuffer[4];
    uint8_t dbuffer[8];

    // float tests
    f.Pack(a, fbuffer, 4);
    EXPECT_THAT(fbuffer, ElementsAre(0xBF, 0x9E, 0x04, 0x19));
    result = f.Unpack(fbuffer, 4);
    EXPECT_EQ(result, a);

    fb.Pack(a, fbuffer, 4);
    EXPECT_THAT(fbuffer, ElementsAre(0x9E, 0xBF, 0x19, 0x04));
    result = fb.Unpack(fbuffer, 4);
    EXPECT_EQ(result, a);

    fw.Pack(a, fbuffer, 4);
    EXPECT_THAT(fbuffer, ElementsAre(0x04, 0x19, 0xBF, 0x9E));
    result = fw.Unpack(fbuffer, 4);
    EXPECT_EQ(result, a);
    
    fbw.Pack(a, fbuffer, 4);
    EXPECT_THAT(fbuffer, ElementsAre(0x19, 0x04, 0x9E, 0xBF));
    result = fbw.Unpack(fbuffer, 4);
    EXPECT_EQ(result, a);
    
    // double tests
    /* double b = 1.23456; // 0x3FF3C0C1FC8F3238 */
    d.Pack(b, dbuffer, 8);
    EXPECT_THAT(dbuffer, ElementsAre(0x3F, 0xF3, 0xC0, 0xC1, 0xFC, 0x8F, 0x32, 0x38));
    result = d.Unpack(dbuffer, 8);
    EXPECT_EQ(result, b);

    db.Pack(b, dbuffer, 8);
    EXPECT_THAT(dbuffer, ElementsAre(0xF3, 0x3F, 0xC1, 0xC0, 0x8F, 0xFC, 0x38, 0x32));
    result = db.Unpack(dbuffer, 8);
    EXPECT_EQ(result, b);

    dw.Pack(b, dbuffer, 8);
    EXPECT_THAT(dbuffer, ElementsAre(0x32, 0x38, 0xFC, 0x8F, 0xC0, 0xC1, 0x3F, 0xF3));
    result = dw.Unpack(dbuffer, 8);
    EXPECT_EQ(result, b);
    
    dbw.Pack(b, dbuffer, 8);
    EXPECT_THAT(dbuffer, ElementsAre(0x38, 0x32, 0x8F, 0xFC, 0xC1, 0xC0, 0xF3, 0x3F));
    result = dbw.Unpack(dbuffer, 8);
    EXPECT_EQ(result, b);
}

TEST_F(MQTTConvertersTest, TextTest)
{
    string val = "Hello12345";
    string result;

    TMQTTTextConverter t(10), tb(10, true), tw(10, false, true), tbw(10, true, true);

    uint16_t buffer[10] = { 0 };
    
    t.Pack(val, buffer, 10);
    EXPECT_THAT(buffer, ElementsAre('H', 'e', 'l', 'l', 'o', '1', '2', '3', '4', '5'));
    result = t.Unpack(buffer, 10);
    EXPECT_EQ(result, val);
    memset(buffer, 0, 20);

    tb.Pack(val, buffer, 10);
    EXPECT_THAT(buffer, ElementsAre('H' << 8, 'e' << 8, 'l' << 8, 'l' << 8, 'o' << 8, '1' << 8, '2' << 8, '3' << 8, '4' << 8, '5' << 8));
    result = tb.Unpack(buffer, 10);
    EXPECT_EQ(result, val);
    memset(buffer, 0, 20);
    
    tw.Pack(val, buffer, 10);
    EXPECT_THAT(buffer, ElementsAre('5', '4', '3', '2', '1', 'o', 'l', 'l', 'e', 'H'));
    result = tw.Unpack(buffer, 10);
    EXPECT_EQ(result, val);
    memset(buffer, 0, 20);

    tbw.Pack(val, buffer, 10);
    EXPECT_THAT(buffer, ElementsAre('5' << 8, '4' << 8, '3' << 8, '2' << 8, '1' << 8, 'o' << 8, 'l' << 8, 'l' << 8, 'e' << 8, 'H' << 8));
    result = tbw.Unpack(buffer, 10);
    EXPECT_EQ(result, val);
    memset(buffer, 0, 20);
}
