#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "address_range.h"
#include <memory>
#include <iostream>
#include <exception>

using namespace std;

using ::testing::_;
using ::testing::ElementsAre;


typedef TAddressRange<int> TestAddressRange;
typedef TAddressRange<std::shared_ptr<int>> TestPtrAddressRange;

class TAddressRangeTest : public ::testing::Test
{
protected:
    void SetUp() {}
    void TearDown() {}

};


TEST_F(TAddressRangeTest, IntersectionTest)
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

TEST_F(TAddressRangeTest, EmptyIntersectionTest)
{
    TestPtrAddressRange r1(0, 10), r2(5, 10), result(0, 15);

    r1 += r2;

    EXPECT_EQ(r1, result);
}

TEST_F(TAddressRangeTest, OperatorTest)
{
    TestAddressRange r1, r2, r3, result;

    // basic insert()
    r1.insert(1, 10, 1);
    r2.insert(2, 10, 1);

    r1.insert(r2);

    result.insert(1, 11, 1);
    EXPECT_EQ(r1, result);

    r1.clear();
    r2.clear();
    result.clear();

    // overloaded operators
    r1.insert(1, 10, 1);
    r2.insert(5, 10, 1);
    r3 = r1 + r2;

    result.insert(1, 14, 1);
    EXPECT_EQ(r3, result);

    r1.clear();
    r2.clear();
    r3.clear();
    result.clear();

    // different parameters
    r1.insert(0, 10, 1);
    r2.insert(10, 10, 2);
    r3.insert(20, 10, 3);

    r1 += r2 + r3;
    r2 += r1;

    EXPECT_EQ(r1, r2);
}

TEST_F(TAddressRangeTest, InRangeTest)
{
    TestAddressRange r1, r2;

    r1.insert(0, 10, 1);
    r2.insert(10, 10, 2);

    r1 += r2;

    EXPECT_EQ(r1.inRange(0), true);
    EXPECT_EQ(r1.inRange(20), false);
    EXPECT_EQ(r1.inRange(1, 5), true);
    EXPECT_EQ(r1.inRange(4), true);
    EXPECT_EQ(r1.inRange(15, 3), true);
    EXPECT_EQ(r1.inRange(10), true);
    EXPECT_EQ(r1.inRange(-15, 5), false);
    EXPECT_EQ(r1.inRange(21), false);
    EXPECT_EQ(r1.inRange(8, 4), false);
}

TEST_F(TAddressRangeTest, GetParamTest)
{
    TestAddressRange r1(0, 10, 1), r2(10, 10, 2), r3(100, 25, -1);

    r1 += r2 + r3;

    EXPECT_EQ(r1.getParam(0), 1);
    EXPECT_EQ(r1.getParam(10), 2);
    EXPECT_EQ(r1.getParam(100), -1);
    EXPECT_EQ(r1.getParam(9), 1);
    
    EXPECT_THROW(r1.getParam(20), WrongSegmentException);
    EXPECT_THROW(r1.getParam(8, 4), WrongSegmentException);
    EXPECT_THROW(r1.getParam(0, 11), WrongSegmentException);
}

TEST_F(TAddressRangeTest, InsertFailureTest)
{
    TestAddressRange r(0, 20, 1);
    
    EXPECT_THROW(r + TestAddressRange(10, 20, 2), WrongSegmentException);
    EXPECT_THROW(r.insert(10, 20, 2), WrongSegmentException);
    EXPECT_THROW(TestAddressRange(2, 1, 1) + TestAddressRange(0, 15, 2), WrongSegmentException);
}

TEST_F(TAddressRangeTest, DiffIntersectionTest)
{
    TestAddressRange test_range;
    TestAddressRange result_range;

    test_range.insert(0, 1, 1);
    test_range.insert(1, 1, 2);
}

TEST_F(TAddressRangeTest, MultiSegmentsTest)
{
    TestAddressRange r, r1, r2, r3, r4, r5;

    r1 = TestAddressRange(0, 5, 1);
    r2 = TestAddressRange(5, 5, 2);
    r3 = TestAddressRange(10, 5, 1);
    r4 = TestAddressRange(15, 5, 3);

    r5 = TestAddressRange(40, 5, 1);


    r = r1 + r2 + r3 + r4 + r5;
    auto list1 = r.getSegments(1, 14);
    EXPECT_THAT(list1, ElementsAre(TestAddressRange(1, 4, 1), r2, TestAddressRange(10, 5, 1)));
    
    try {
    auto list2 = r.getSegments(4, 2);
    EXPECT_THAT(list2, ElementsAre(TestAddressRange(4, 1, 1), TestAddressRange(5, 1, 2)));
    } catch (const WrongSegmentException &e) {
        cerr << e.what() << endl;
    }

    EXPECT_THROW(r.getSegments(13, 40), WrongSegmentException);
}
