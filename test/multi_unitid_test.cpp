#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "modbus_wrapper.h"
#include "mock_modbus_observer.h"
#include "fake_modbus_backend.h"

using namespace std;
using ::testing::_;
using ::testing::Return;

class MultiUnitIDTest : public ::testing::Test
{
public:
    shared_ptr<TFakeModbusBackend> Backend;
    PModbusServer Server;
    shared_ptr<MockModbusServerObserver> obs1, obs2, obs3, obs4;

    void SetUp() 
    {
        Backend = make_shared<TFakeModbusBackend>();
        Server = make_shared<TModbusServer>(Backend);

        Backend->AllocateCache(1, 10, 10, 10, 10);
        Backend->AllocateCache(2, 10, 10, 10, 10);
        Backend->AllocateCache(5, 10, 10, 10, 10);

        obs1 = make_shared<MockModbusServerObserver>();
        obs2 = make_shared<MockModbusServerObserver>();
        obs3 = make_shared<MockModbusServerObserver>();
        obs4 = make_shared<MockModbusServerObserver>();

        TModbusAddressRange r1(0, 5), r2(5, 3), r3(2, 5), r4(4, 5);

        Server->Observe(obs1, COIL, r1, 1);
        Server->Observe(obs2, COIL, r2, 1);

        Server->Observe(obs3, COIL, r3, 2);
        Server->Observe(obs4, COIL, r4, 5);

    }
    void TearDown() {}
};

TEST_F(MultiUnitIDTest, AllocateTest)
{
    TModbusCacheAddressRange or1(0, 5, Backend->GetCache(COIL, 1));
    TModbusCacheAddressRange or2(5, 3, static_cast<char *>(Backend->GetCache(COIL, 1)) + 5);
    TModbusCacheAddressRange or3(2, 5, static_cast<char *>(Backend->GetCache(COIL, 2)) + 2);
    TModbusCacheAddressRange or4(4, 5, static_cast<char *>(Backend->GetCache(COIL, 5)) + 4);

    EXPECT_CALL(*obs1, OnCacheAllocate(COIL, 1, or1)).Times(1);
    EXPECT_CALL(*obs2, OnCacheAllocate(COIL, 1, or2)).Times(1);
    EXPECT_CALL(*obs3, OnCacheAllocate(COIL, 2, or3)).Times(1);
    EXPECT_CALL(*obs4, OnCacheAllocate(COIL, 5, or4)).Times(1);

    Server->AllocateCache();
}

TEST_F(MultiUnitIDTest, ReadRequestTest)
{
    EXPECT_CALL(*obs1, OnCacheAllocate(COIL, 1, _)).Times(1);
    EXPECT_CALL(*obs2, OnCacheAllocate(COIL, 1, _)).Times(1);
    EXPECT_CALL(*obs3, OnCacheAllocate(COIL, 2, _)).Times(1);
    EXPECT_CALL(*obs4, OnCacheAllocate(COIL, 5, _)).Times(1);

    Server->AllocateCache();

    // query for obs1
    uint8_t q1[] = {
        1,          // modbus unit ID
        0x01,       // read coils
        0x00, 0x00, // address
        0x00, 0x01  // count
    };
    TModbusQuery query1(q1, sizeof (q1), 1);

    // query for no one
    uint8_t q2[] = {
        8,          // undefined cache
        0x01,
        0x00, 0x00,
        0x00, 0x01
    };
    TModbusQuery query2(q2, sizeof (q2), 1);

    Backend->PushQuery(query1);
    Backend->PushQuery(query2);

    EXPECT_CALL(*obs1, OnGetValue(COIL, 1, 0, 1, Backend->GetCache(COIL, 1))).WillOnce(Return(REPLY_OK));

    while (!Backend->IncomingQueries.empty())
        Server->Loop();
}
