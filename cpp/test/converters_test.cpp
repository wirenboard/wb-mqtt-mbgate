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

TEST_F(MQTTConvertersTest, DiscreteTest)
{
    string val_true = "1";
    string val_false = "0";
    
    string result;

    uint8_t buffer;
    
    TMQTTDiscrConverter conv;
    
    conv.Pack(val_true, &buffer, 1);
    EXPECT_EQ(buffer, 0xFF);
    result = conv.Unpack(&buffer, 1);
    EXPECT_EQ(result, val_true);

    conv.Pack(val_false, &buffer, 1);
    EXPECT_EQ(buffer, 0x00);
    result = conv.Unpack(&buffer, 1);
    EXPECT_EQ(result, val_false);
}

TEST_F(MQTTConvertersTest, UnsignedIntTest)
{
    float scale = 2.0;

    TMQTTIntConverter s(TMQTTIntConverter::UNSIGNED, scale, 2),
                     sb(TMQTTIntConverter::UNSIGNED, scale, 2, true),
                     sw(TMQTTIntConverter::UNSIGNED, scale, 2, false, true),
                     sbw(TMQTTIntConverter::UNSIGNED, scale, 2, true, true);
    TMQTTIntConverter i(TMQTTIntConverter::UNSIGNED, scale, 4),
                     ib(TMQTTIntConverter::UNSIGNED, scale, 4, true),
                     iw(TMQTTIntConverter::UNSIGNED, scale, 4, false, true),
                     ibw(TMQTTIntConverter::UNSIGNED, scale, 4, true, true);
    TMQTTIntConverter l(TMQTTIntConverter::UNSIGNED, scale, 8),
                     lb(TMQTTIntConverter::UNSIGNED, scale, 8, true),
                     lw(TMQTTIntConverter::UNSIGNED, scale, 8, false, true),
                     lbw(TMQTTIntConverter::UNSIGNED, scale, 8, true, true);

    string result;
    
    string sval = "1234"; // * 2.0 == 0x09A4

    uint8_t sbuf[2];

    s.Pack(sval, &sbuf, 2);
    EXPECT_THAT(sbuf, ElementsAre(0x09, 0xA4));
    result = s.Unpack(sbuf, 2);
    EXPECT_EQ(result, sval);
    
    sb.Pack(sval, &sbuf, 2);
    EXPECT_THAT(sbuf, ElementsAre(0xA4, 0x09));
    result = sb.Unpack(sbuf, 2);
    EXPECT_EQ(result, sval);
    
    sw.Pack(sval, &sbuf, 2);
    EXPECT_THAT(sbuf, ElementsAre(0x09, 0xA4));
    result = sw.Unpack(sbuf, 2);
    EXPECT_EQ(result, sval);
    
    sbw.Pack(sval, &sbuf, 2);
    EXPECT_THAT(sbuf, ElementsAre(0xA4, 0x09));
    result = sbw.Unpack(sbuf, 2);
    EXPECT_EQ(result, sval);
    

    string ival = "12345678"; // * 2.0 == 0x0178C29C
    uint8_t ibuf[4];

    i.Pack(ival, &ibuf, 4);
    EXPECT_THAT(ibuf, ElementsAre(0x01, 0x78, 0xC2, 0x9C));
    result = i.Unpack(ibuf, 4);
    EXPECT_EQ(result, ival);
    
    ib.Pack(ival, &ibuf, 4);
    EXPECT_THAT(ibuf, ElementsAre(0x78, 0x01, 0x9C, 0xC2));
    result = ib.Unpack(ibuf, 4);
    EXPECT_EQ(result, ival);
    
    iw.Pack(ival, &ibuf, 4);
    EXPECT_THAT(ibuf, ElementsAre(0xC2, 0x9C, 0x01, 0x78));
    result = iw.Unpack(ibuf, 4);
    EXPECT_EQ(result, ival);
    
    ibw.Pack(ival, &ibuf, 4);
    EXPECT_THAT(ibuf, ElementsAre(0x9C, 0xC2, 0x78, 0x01));
    result = ibw.Unpack(ibuf, 4);
    EXPECT_EQ(result, ival);
    


    string lval = "12345678"; // * 2.0 == 0x0178C29C
    uint8_t lbuf[8];

    l.Pack(lval, &lbuf, 8);
    EXPECT_THAT(lbuf, ElementsAre(0, 0, 0, 0, 0x01, 0x78, 0xC2, 0x9C));
    result = l.Unpack(lbuf, 8);
    EXPECT_EQ(result, lval);
    
    lb.Pack(lval, &lbuf, 8);
    EXPECT_THAT(lbuf, ElementsAre(0, 0, 0, 0, 0x78, 0x01, 0x9C, 0xC2));
    result = lb.Unpack(lbuf, 8);
    EXPECT_EQ(result, lval);
    
    lw.Pack(lval, &lbuf, 8);
    EXPECT_THAT(lbuf, ElementsAre(0xC2, 0x9C, 0x01, 0x78, 0, 0, 0, 0));
    result = lw.Unpack(lbuf, 8);
    EXPECT_EQ(result, lval);
    
    lbw.Pack(lval, &lbuf, 8);
    EXPECT_THAT(lbuf, ElementsAre(0x9C, 0xC2, 0x78, 0x01, 0, 0, 0, 0));
    result = lbw.Unpack(lbuf, 8);
    EXPECT_EQ(result, lval);
}

TEST_F(MQTTConvertersTest, SignedIntTest)
{
    float scale = 2.0;

    TMQTTIntConverter s(TMQTTIntConverter::SIGNED, scale, 2),
                     sb(TMQTTIntConverter::SIGNED, scale, 2, true),
                     sw(TMQTTIntConverter::SIGNED, scale, 2, false, true),
                     sbw(TMQTTIntConverter::SIGNED, scale, 2, true, true);
    TMQTTIntConverter i(TMQTTIntConverter::SIGNED, scale, 4),
                     ib(TMQTTIntConverter::SIGNED, scale, 4, true),
                     iw(TMQTTIntConverter::SIGNED, scale, 4, false, true),
                     ibw(TMQTTIntConverter::SIGNED, scale, 4, true, true);
    TMQTTIntConverter l(TMQTTIntConverter::SIGNED, scale, 8),
                     lb(TMQTTIntConverter::SIGNED, scale, 8, true),
                     lw(TMQTTIntConverter::SIGNED, scale, 8, false, true),
                     lbw(TMQTTIntConverter::SIGNED, scale, 8, true, true);

    string result;
    
    string sval = "-1234"; // * 2.0 == 0xF65C

    uint8_t sbuf[2];

    s.Pack(sval, &sbuf, 2);
    EXPECT_THAT(sbuf, ElementsAre(0xF6, 0x5C));
    result = s.Unpack(sbuf, 2);
    EXPECT_EQ(result, sval);
    
    sb.Pack(sval, &sbuf, 2);
    EXPECT_THAT(sbuf, ElementsAre(0x5C, 0xF6));
    result = sb.Unpack(sbuf, 2);
    EXPECT_EQ(result, sval);
    
    sw.Pack(sval, &sbuf, 2);
    EXPECT_THAT(sbuf, ElementsAre(0xF6, 0x5C));
    result = sw.Unpack(sbuf, 2);
    EXPECT_EQ(result, sval);
    
    sbw.Pack(sval, &sbuf, 2);
    EXPECT_THAT(sbuf, ElementsAre(0x5C, 0xF6));
    result = sbw.Unpack(sbuf, 2);
    EXPECT_EQ(result, sval);
    

    string ival = "-12345678"; // * 2.0 == 0xFE873D64
    uint8_t ibuf[4];

    i.Pack(ival, &ibuf, 4);
    EXPECT_THAT(ibuf, ElementsAre(0xFE, 0x87, 0x3D, 0x64));
    result = i.Unpack(ibuf, 4);
    EXPECT_EQ(result, ival);
    
    ib.Pack(ival, &ibuf, 4);
    EXPECT_THAT(ibuf, ElementsAre(0x87, 0xFE, 0x64, 0x3D));
    result = ib.Unpack(ibuf, 4);
    EXPECT_EQ(result, ival);
    
    iw.Pack(ival, &ibuf, 4);
    EXPECT_THAT(ibuf, ElementsAre(0x3D, 0x64, 0xFE, 0x87));
    result = iw.Unpack(ibuf, 4);
    EXPECT_EQ(result, ival);
    
    ibw.Pack(ival, &ibuf, 4);
    EXPECT_THAT(ibuf, ElementsAre(0x64, 0x3D, 0x87, 0xFE));
    result = ibw.Unpack(ibuf, 4);
    EXPECT_EQ(result, ival);
    


    string lval = "-12345678"; // * 2.0 == 0x0178C29C
    uint8_t lbuf[8];

    l.Pack(lval, &lbuf, 8);
    EXPECT_THAT(lbuf, ElementsAre(0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0x87, 0x3D, 0x64));
    result = l.Unpack(lbuf, 8);
    EXPECT_EQ(result, lval);
    
    lb.Pack(lval, &lbuf, 8);
    EXPECT_THAT(lbuf, ElementsAre(0xFF, 0xFF, 0xFF, 0xFF, 0x87, 0xFE, 0x64, 0x3D));
    result = lb.Unpack(lbuf, 8);
    EXPECT_EQ(result, lval);
    
    lw.Pack(lval, &lbuf, 8);
    EXPECT_THAT(lbuf, ElementsAre(0x3D, 0x64, 0xFE, 0x87, 0xFF, 0xFF, 0xFF, 0xFF));
    result = lw.Unpack(lbuf, 8);
    EXPECT_EQ(result, lval);
    
    lbw.Pack(lval, &lbuf, 8);
    EXPECT_THAT(lbuf, ElementsAre(0x64, 0x3D, 0x87, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF));
    result = lbw.Unpack(lbuf, 8);
    EXPECT_EQ(result, lval);
}

TEST_F(MQTTConvertersTest, BcdTest)
{
    float scale = 2.0;

    TMQTTIntConverter s(TMQTTIntConverter::BCD, scale, 2),
                     sb(TMQTTIntConverter::BCD, scale, 2, true),
                     sw(TMQTTIntConverter::BCD, scale, 2, false, true),
                     sbw(TMQTTIntConverter::BCD, scale, 2, true, true);
    TMQTTIntConverter i(TMQTTIntConverter::BCD, scale, 4),
                     ib(TMQTTIntConverter::BCD, scale, 4, true),
                     iw(TMQTTIntConverter::BCD, scale, 4, false, true),
                     ibw(TMQTTIntConverter::BCD, scale, 4, true, true);
    TMQTTIntConverter l(TMQTTIntConverter::BCD, scale, 8),
                     lb(TMQTTIntConverter::BCD, scale, 8, true),
                     lw(TMQTTIntConverter::BCD, scale, 8, false, true),
                     lbw(TMQTTIntConverter::BCD, scale, 8, true, true);

    string result;
    
    string sval = "1234"; // * 2.0 => 0x2468

    uint8_t sbuf[2];

    s.Pack(sval, &sbuf, 2);
    EXPECT_THAT(sbuf, ElementsAre(0x24, 0x68));
    result = s.Unpack(sbuf, 2);
    EXPECT_EQ(result, sval);
    
    sb.Pack(sval, &sbuf, 2);
    EXPECT_THAT(sbuf, ElementsAre(0x68, 0x24));
    result = sb.Unpack(sbuf, 2);
    EXPECT_EQ(result, sval);
    
    sw.Pack(sval, &sbuf, 2);
    EXPECT_THAT(sbuf, ElementsAre(0x24, 0x68));
    result = sw.Unpack(sbuf, 2);
    EXPECT_EQ(result, sval);
    
    sbw.Pack(sval, &sbuf, 2);
    EXPECT_THAT(sbuf, ElementsAre(0x68, 0x24));
    result = sbw.Unpack(sbuf, 2);
    EXPECT_EQ(result, sval);
    

    string ival = "12345678"; // * 2.0 => 0x24691356
    uint8_t ibuf[4];

    i.Pack(ival, &ibuf, 4);
    EXPECT_THAT(ibuf, ElementsAre(0x24, 0x69, 0x13, 0x56));
    result = i.Unpack(ibuf, 4);
    EXPECT_EQ(result, ival);
    
    ib.Pack(ival, &ibuf, 4);
    EXPECT_THAT(ibuf, ElementsAre(0x69, 0x24, 0x56, 0x13));
    result = ib.Unpack(ibuf, 4);
    EXPECT_EQ(result, ival);
    
    iw.Pack(ival, &ibuf, 4);
    EXPECT_THAT(ibuf, ElementsAre(0x13, 0x56, 0x24, 0x69));
    result = iw.Unpack(ibuf, 4);
    EXPECT_EQ(result, ival);
    
    ibw.Pack(ival, &ibuf, 4);
    EXPECT_THAT(ibuf, ElementsAre(0x56, 0x13, 0x69, 0x24));
    result = ibw.Unpack(ibuf, 4);
    EXPECT_EQ(result, ival);
    


    string lval = "12345678"; // * 2.0 => 0x24691356
    uint8_t lbuf[8];

    l.Pack(lval, &lbuf, 8);
    EXPECT_THAT(lbuf, ElementsAre(0, 0, 0, 0, 0x24, 0x69, 0x13, 0x56));
    result = l.Unpack(lbuf, 8);
    EXPECT_EQ(result, lval);
    
    lb.Pack(lval, &lbuf, 8);
    EXPECT_THAT(lbuf, ElementsAre(0, 0, 0, 0, 0x69, 0x24, 0x56, 0x13));
    result = lb.Unpack(lbuf, 8);
    EXPECT_EQ(result, lval);
    
    lw.Pack(lval, &lbuf, 8);
    EXPECT_THAT(lbuf, ElementsAre(0x13, 0x56, 0x24, 0x69, 0, 0, 0, 0));
    result = lw.Unpack(lbuf, 8);
    EXPECT_EQ(result, lval);
    
    lbw.Pack(lval, &lbuf, 8);
    EXPECT_THAT(lbuf, ElementsAre(0x56, 0x13, 0x69, 0x24, 0, 0, 0, 0));
    result = lbw.Unpack(lbuf, 8);
    EXPECT_EQ(result, lval);
}

TEST_F(MQTTConvertersTest, IntFormatsTest)
{
    TMQTTIntConverter conv(TMQTTIntConverter::SIGNED, 1.0, 4);

    string val1 = "0703"; // oct
    string val2 = "0x12345"; // hex

    string result;
    uint8_t buf[4];
    
    conv.Pack(val1, buf, 4);
    EXPECT_THAT(buf, ElementsAre(0, 0, 0x01, 0xC3));
    result = conv.Unpack(buf, 4);
    EXPECT_EQ(result, string("451"));
    
    conv.Pack(val2, buf, 4);
    EXPECT_THAT(buf, ElementsAre(0, 0x01, 0x23, 0x45));
    result = conv.Unpack(buf, 4);
    EXPECT_EQ(result, string("74565"));
}
