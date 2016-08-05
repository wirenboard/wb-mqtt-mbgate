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

    // check merging
    // --[--)-------- +
    // -----[-----)-- =
    // --[--------)--
    test_range.insert(0, 10, 1);
    test_range.insert(10, 10, 1);

    result_range.insert(0, 20, 1);
    EXPECT_EQ(test_range, result_range);

    test_range.clear();
    result_range.clear();

    // --[----)------- +
    // ----[----)----- =
    // --[------)-----
    test_range.insert(-10, 20, 1);
    test_range.insert(0, 20, 1);

    result_range.insert(-10, 30, 1);
    EXPECT_EQ(test_range, result_range);
    
    test_range.clear();
    result_range.clear();

    // --[---------)---- +
    // ----[-----)------ =
    // --[---------)----
    test_range.insert(10, 30, 1);
    test_range.insert(20, 10, 1);
    
    result_range.insert(10, 30, 1);
    EXPECT_EQ(test_range, result_range);

    test_range.clear();
    result_range.clear();
    
    // ----[-----)------ +
    // --[---------)---- =
    // --[---------)----
    test_range.insert(20, 10, 1);
    test_range.insert(10, 30, 1);
    
    result_range.insert(10, 30, 1);
    EXPECT_EQ(test_range, result_range);

    test_range.clear();
    result_range.clear();

    // ---[---)----[---)--- +
    // ----[----)---------- =
    // ---[-----)--[---)---
    test_range.insert(0, 10, 1);
    test_range.insert(20, 10, 1);
    test_range.insert(5, 10, 1);
    
    result_range.insert(0, 15, 1);
    result_range.insert(20, 10, 1);
    EXPECT_EQ(test_range, result_range);

    test_range.clear();
    result_range.clear();


    // ---[---)----[---)--- +
    // ----------[----)---- =
    // ---[---)--[-----)---
    test_range.insert(0, 10, 1);
    test_range.insert(20, 10, 1);
    test_range.insert(15, 10, 1);
    
    result_range.insert(0, 10, 1);
    result_range.insert(15, 15, 1);
    EXPECT_EQ(test_range, result_range);

    test_range.clear();
    result_range.clear();

    // ---[---)----[---)--- +
    // ------[--------)---- =
    // ---[------------)---
    test_range.insert(0, 10, 1);
    test_range.insert(20, 10, 1);
    test_range.insert(5, 20, 1);
    
    result_range.insert(0, 30, 1);
    EXPECT_EQ(test_range, result_range);

    test_range.clear();
    result_range.clear();
    
    // ---[---)-[)--[---)--- +
    // --[---------------)-- =
    // --[---------------)--
    test_range.insert(0, 10, 1);
    test_range.insert(20, 10, 1);
    test_range.insert(35, 10, 1);
    test_range.insert(-10, 80, 1);
    
    result_range.insert(-10, 80, 1);
    EXPECT_EQ(test_range, result_range);

    test_range.clear();
    result_range.clear();
}

TEST_F(TAddressRangeTest, FailureTest)
{

}
