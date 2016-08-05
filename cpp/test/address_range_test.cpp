#include <gtest/gtest.h>

#include <address_range.h>

typedef TAddressRange<int> TestAddressRange;

class TAddressRangeTest : public ::testing::Test
{
protected:
    void SetUp() {}
    void TearDown() {}

};



TEST_F(TAddressRangeTest, AddressTest)
{
    TestAddressRange test_range;
    TestAddressRange result_range;

    // check mergeing
    // --[--)-------- +
    // -----[-----)-- ==
    // --[--------)--
    test_range.insert(0, 10, 1);
    test_range.insert(10, 10, 1);

    result_range.insert(0, 20, 1);
    ASSERT_EQ(test_range, result_range);
}
