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

    uint16_t sbuf[1];

    s.Pack(sval, &sbuf, 2);
    EXPECT_THAT(sbuf, ElementsAre(0x09A4));
    result = s.Unpack(sbuf, 2);
    EXPECT_EQ(result, sval);
    
    sb.Pack(sval, &sbuf, 2);
    EXPECT_THAT(sbuf, ElementsAre(0xA409));
    result = sb.Unpack(sbuf, 2);
    EXPECT_EQ(result, sval);
    
    sw.Pack(sval, &sbuf, 2);
    EXPECT_THAT(sbuf, ElementsAre(0x09A4));
    result = sw.Unpack(sbuf, 2);
    EXPECT_EQ(result, sval);
    
    sbw.Pack(sval, &sbuf, 2);
    EXPECT_THAT(sbuf, ElementsAre(0xA409));
    result = sbw.Unpack(sbuf, 2);
    EXPECT_EQ(result, sval);
    

    string ival = "12345678"; // * 2.0 == 0x0178C29C
    string ival_out = "1.23457e+07";
    uint16_t ibuf[2];

    i.Pack(ival, &ibuf, 4);
    EXPECT_THAT(ibuf, ElementsAre(0x0178, 0xC29C));
    result = i.Unpack(ibuf, 4);
    EXPECT_EQ(result, ival_out);
    
    ib.Pack(ival, &ibuf, 4);
    EXPECT_THAT(ibuf, ElementsAre(0x7801, 0x9CC2));
    result = ib.Unpack(ibuf, 4);
    EXPECT_EQ(result, ival_out);
    
    iw.Pack(ival, &ibuf, 4);
    EXPECT_THAT(ibuf, ElementsAre(0xC29C, 0x0178));
    result = iw.Unpack(ibuf, 4);
    EXPECT_EQ(result, ival_out);
    
    ibw.Pack(ival, &ibuf, 4);
    EXPECT_THAT(ibuf, ElementsAre(0x9CC2, 0x7801));
    result = ibw.Unpack(ibuf, 4);
    EXPECT_EQ(result, ival_out);
    


    string lval = "12345678"; // * 2.0 == 0x0178C29C
    string lval_out = "1.23457e+07";
    uint16_t lbuf[4];

    l.Pack(lval, &lbuf, 8);
    EXPECT_THAT(lbuf, ElementsAre(0, 0, 0x0178, 0xC29C));
    result = l.Unpack(lbuf, 8);
    EXPECT_EQ(result, lval_out);
    
    lb.Pack(lval, &lbuf, 8);
    EXPECT_THAT(lbuf, ElementsAre(0, 0, 0x7801, 0x9CC2));
    result = lb.Unpack(lbuf, 8);
    EXPECT_EQ(result, lval_out);
    
    lw.Pack(lval, &lbuf, 8);
    EXPECT_THAT(lbuf, ElementsAre(0xC29C, 0x0178, 0, 0));
    result = lw.Unpack(lbuf, 8);
    EXPECT_EQ(result, lval_out);
    
    lbw.Pack(lval, &lbuf, 8);
    EXPECT_THAT(lbuf, ElementsAre(0x9CC2, 0x7801, 0, 0));
    result = lbw.Unpack(lbuf, 8);
    EXPECT_EQ(result, lval_out);
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

    uint16_t sbuf[1];

    s.Pack(sval, &sbuf, 2);
    EXPECT_THAT(sbuf, ElementsAre(0xF65C));
    result = s.Unpack(sbuf, 2);
    EXPECT_EQ(result, sval);
    
    sb.Pack(sval, &sbuf, 2);
    EXPECT_THAT(sbuf, ElementsAre(0x5CF6));
    result = sb.Unpack(sbuf, 2);
    EXPECT_EQ(result, sval);
    
    sw.Pack(sval, &sbuf, 2);
    EXPECT_THAT(sbuf, ElementsAre(0xF65C));
    result = sw.Unpack(sbuf, 2);
    EXPECT_EQ(result, sval);
    
    sbw.Pack(sval, &sbuf, 2);
    EXPECT_THAT(sbuf, ElementsAre(0x5CF6));
    result = sbw.Unpack(sbuf, 2);
    EXPECT_EQ(result, sval);
    

    string ival = "-12345678"; // * 2.0 == 0xFE873D64
    string ival_out = "-1.23457e+07";
    uint16_t ibuf[2];

    i.Pack(ival, &ibuf, 4);
    EXPECT_THAT(ibuf, ElementsAre(0xFE87, 0x3D64));
    result = i.Unpack(ibuf, 4);
    EXPECT_EQ(result, ival_out);
    
    ib.Pack(ival, &ibuf, 4);
    EXPECT_THAT(ibuf, ElementsAre(0x87FE, 0x643D));
    result = ib.Unpack(ibuf, 4);
    EXPECT_EQ(result, ival_out);
    
    iw.Pack(ival, &ibuf, 4);
    EXPECT_THAT(ibuf, ElementsAre(0x3D64, 0xFE87));
    result = iw.Unpack(ibuf, 4);
    EXPECT_EQ(result, ival_out);
    
    ibw.Pack(ival, &ibuf, 4);
    EXPECT_THAT(ibuf, ElementsAre(0x643D, 0x87FE));
    result = ibw.Unpack(ibuf, 4);
    EXPECT_EQ(result, ival_out);
    


    string lval = "-12345678"; // * 2.0 == 0x0178C29C
    string lval_out = "-1.23457e+07";
    uint16_t lbuf[4];

    l.Pack(lval, &lbuf, 8);
    EXPECT_THAT(lbuf, ElementsAre(0xFFFF, 0xFFFF, 0xFE87, 0x3D64));
    result = l.Unpack(lbuf, 8);
    EXPECT_EQ(result, lval_out);
    
    lb.Pack(lval, &lbuf, 8);
    EXPECT_THAT(lbuf, ElementsAre(0xFFFF, 0xFFFF, 0x87FE, 0x643D));
    result = lb.Unpack(lbuf, 8);
    EXPECT_EQ(result, lval_out);
    
    lw.Pack(lval, &lbuf, 8);
    EXPECT_THAT(lbuf, ElementsAre(0x3D64, 0xFE87, 0xFFFF, 0xFFFF));
    result = lw.Unpack(lbuf, 8);
    EXPECT_EQ(result, lval_out);
    
    lbw.Pack(lval, &lbuf, 8);
    EXPECT_THAT(lbuf, ElementsAre(0x643D, 0x87FE, 0xFFFF, 0xFFFF));
    result = lbw.Unpack(lbuf, 8);
    EXPECT_EQ(result, lval_out);
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

    uint16_t sbuf[1];

    s.Pack(sval, &sbuf, 2);
    EXPECT_THAT(sbuf, ElementsAre(0x2468));
    result = s.Unpack(sbuf, 2);
    EXPECT_EQ(result, sval);
    
    sb.Pack(sval, &sbuf, 2);
    EXPECT_THAT(sbuf, ElementsAre(0x6824));
    result = sb.Unpack(sbuf, 2);
    EXPECT_EQ(result, sval);
    
    sw.Pack(sval, &sbuf, 2);
    EXPECT_THAT(sbuf, ElementsAre(0x2468));
    result = sw.Unpack(sbuf, 2);
    EXPECT_EQ(result, sval);
    
    sbw.Pack(sval, &sbuf, 2);
    EXPECT_THAT(sbuf, ElementsAre(0x6824));
    result = sbw.Unpack(sbuf, 2);
    EXPECT_EQ(result, sval);
    

    string ival = "12345678"; // * 2.0 => 0x24691356
    string ival_out = "1.23457e+07";
    uint16_t ibuf[2];

    i.Pack(ival, &ibuf, 4);
    EXPECT_THAT(ibuf, ElementsAre(0x2469, 0x1356));
    result = i.Unpack(ibuf, 4);
    EXPECT_EQ(result, ival_out);
    
    ib.Pack(ival, &ibuf, 4);
    EXPECT_THAT(ibuf, ElementsAre(0x6924, 0x5613));
    result = ib.Unpack(ibuf, 4);
    EXPECT_EQ(result, ival_out);
    
    iw.Pack(ival, &ibuf, 4);
    EXPECT_THAT(ibuf, ElementsAre(0x1356, 0x2469));
    result = iw.Unpack(ibuf, 4);
    EXPECT_EQ(result, ival_out);
    
    ibw.Pack(ival, &ibuf, 4);
    EXPECT_THAT(ibuf, ElementsAre(0x5613, 0x6924));
    result = ibw.Unpack(ibuf, 4);
    EXPECT_EQ(result, ival_out);
    


    string lval = "12345678"; // * 2.0 => 0x24691356
    string lval_out = "1.23457e+07";
    uint16_t lbuf[4];

    l.Pack(lval, &lbuf, 8);
    EXPECT_THAT(lbuf, ElementsAre(0, 0, 0x2469, 0x1356));
    result = l.Unpack(lbuf, 8);
    EXPECT_EQ(result, lval_out);
    
    lb.Pack(lval, &lbuf, 8);
    EXPECT_THAT(lbuf, ElementsAre(0, 0, 0x6924, 0x5613));
    result = lb.Unpack(lbuf, 8);
    EXPECT_EQ(result, lval_out);
    
    lw.Pack(lval, &lbuf, 8);
    EXPECT_THAT(lbuf, ElementsAre(0x1356, 0x2469, 0, 0));
    result = lw.Unpack(lbuf, 8);
    EXPECT_EQ(result, lval_out);
    
    lbw.Pack(lval, &lbuf, 8);
    EXPECT_THAT(lbuf, ElementsAre(0x5613, 0x6924, 0, 0));
    result = lbw.Unpack(lbuf, 8);
    EXPECT_EQ(result, lval_out);
}

TEST_F(MQTTConvertersTest, IntFormatsTest)
{
    TMQTTIntConverter conv(TMQTTIntConverter::SIGNED, 100.0, 4);
    TMQTTIntConverter bcd_conv(TMQTTIntConverter::BCD, 10.0, 4);

    string val1 = "0.125"; // floating point number
    string val2 = "23.1875";

    string result;
    uint16_t buf[2];
    
    conv.Pack(val1, buf, 4);
    EXPECT_THAT(buf, ElementsAre(0, 12));
    result = conv.Unpack(buf, 4);
    EXPECT_EQ(result, string("0.12"));
}
