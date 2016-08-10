#include "fake_modbus_backend.h"
#include "modbus_wrapper.h"
#include "mock_modbus_observer.h"

#include <memory>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace std;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;

class ModbusServerTest : public ::testing::Test
{
public:
    std::shared_ptr<TFakeModbusBackend> Backend;
    PModbusServer Server;

    void SetUp() 
    {
        Backend = make_shared<TFakeModbusBackend>();
        Server = shared_ptr<TModbusServer>(new TModbusServer(Backend));  
    
        // pre-allocate caches to get valid pointers everywhere
        Backend->AllocateCache(100, 100, 100, 100);
    }
    void TearDown() {}
};

TEST_F(ModbusServerTest, AllocationTest)
{
    TModbusAddressRange range1(0, 10), range2(10, 10), range3(30, 5);
 
    std::shared_ptr<MockModbusServerObserver> obs1, obs2, obs3;

    obs1 = make_shared<MockModbusServerObserver>();
    obs2 = make_shared<MockModbusServerObserver>();
    obs3 = make_shared<MockModbusServerObserver>();

    Server->Observe(obs1, COIL, range1);
    Server->Observe(obs2, COIL, range2);
    Server->Observe(obs3, INPUT_REGISTER, range3);

    // get cache pointers
    TModbusCacheAddressRange cache_range1(0, 10, Backend->GetCache(COIL));
    TModbusCacheAddressRange cache_range2(10, 10, static_cast<uint8_t *>(Backend->GetCache(COIL)) + 10);
    TModbusCacheAddressRange cache_range3(30, 5, static_cast<uint16_t *>(Backend->GetCache(INPUT_REGISTER)) + 30);


    EXPECT_CALL(*obs1, OnCacheAllocate(COIL, cache_range1))
        .Times(1);
    EXPECT_CALL(*obs2, OnCacheAllocate(COIL, cache_range2))
        .Times(1);
    EXPECT_CALL(*obs3, OnCacheAllocate(INPUT_REGISTER, cache_range3))
        .Times(1);

    Server->AllocateCache();
    Backend->SetSlave(0x42);
}

TEST_F(ModbusServerTest, ReadCallbackTest)
{
    TModbusAddressRange range1(0, 10), range2(10, 10), range3(40, 10);
    std::shared_ptr<MockModbusServerObserver> obs1, obs2;

    obs1 = make_shared<MockModbusServerObserver>();
    obs2 = make_shared<MockModbusServerObserver>();

    Server->Observe(obs1, DISCRETE_INPUT, range1);
    Server->Observe(obs2, DISCRETE_INPUT, range2);
    Server->Observe(obs1, HOLDING_REGISTER, range3);

    TModbusCacheAddressRange cache_range1(0, 10, Backend->GetCache(DISCRETE_INPUT));
    TModbusCacheAddressRange cache_range2(10, 10, static_cast<uint8_t *>(Backend->GetCache(DISCRETE_INPUT)) + 10);
    TModbusCacheAddressRange cache_range3(40, 10, static_cast<uint16_t *>(Backend->GetCache(HOLDING_REGISTER)) + 40);

    // test allocators again, with same allocator for different ranges
    EXPECT_CALL(*obs1, OnCacheAllocate(DISCRETE_INPUT, cache_range1)).Times(1);
    EXPECT_CALL(*obs1, OnCacheAllocate(HOLDING_REGISTER, cache_range3)).Times(1);
    EXPECT_CALL(*obs2, OnCacheAllocate(DISCRETE_INPUT, cache_range2)).Times(1);

    Server->AllocateCache();

    // form read requests and send them to server
    uint8_t q1[] = { 0x02, 0x00, 0x05, 0x00, 0x02 };
    uint8_t q2[] = { 0x02, 0x00, 0x0A, 0x00, 0x02 };
    TModbusQuery read1(q1, sizeof (q1), 0);
    TModbusQuery read2(q2, sizeof (q2), 0);

    Backend->PushQuery(read1);
    Backend->PushQuery(read2);

    Backend->SetSlave(0x42);

    EXPECT_CALL(*obs1, OnGetValue(DISCRETE_INPUT, 0x42, 5, 2, static_cast<uint8_t *>(Backend->GetCache(DISCRETE_INPUT)) + 5)).WillOnce(Return(REPLY_CACHED));
    EXPECT_CALL(*obs2, OnGetValue(DISCRETE_INPUT, 0x42, 10, 2, static_cast<uint8_t *>(Backend->GetCache(DISCRETE_INPUT)) + 10)).WillOnce(Return(REPLY_CACHED));

    while (!Backend->IncomingQueries.empty())
        Server->Loop();
}

TEST_F(ModbusServerTest, WriteCallbackTest)
{
    TModbusAddressRange range1(0, 2), range2(10, 10), range3(40, 10);
    std::shared_ptr<MockModbusServerObserver> obs1, obs2;

    obs1 = make_shared<MockModbusServerObserver>();
    obs2 = make_shared<MockModbusServerObserver>();

    Server->Observe(obs1, DISCRETE_INPUT, range1);
    Server->Observe(obs2, DISCRETE_INPUT, range2);
    Server->Observe(obs1, HOLDING_REGISTER, range3);

    TModbusCacheAddressRange cache_range1(0, 2, Backend->GetCache(DISCRETE_INPUT));
    TModbusCacheAddressRange cache_range2(10, 10, static_cast<uint8_t *>(Backend->GetCache(DISCRETE_INPUT)) + 10);
    TModbusCacheAddressRange cache_range3(40, 10, static_cast<uint16_t *>(Backend->GetCache(HOLDING_REGISTER)) + 40);

    // test allocators again, with same allocator for different ranges
    EXPECT_CALL(*obs1, OnCacheAllocate(DISCRETE_INPUT, cache_range1)).Times(1);
    EXPECT_CALL(*obs1, OnCacheAllocate(HOLDING_REGISTER, cache_range3)).Times(1);
    EXPECT_CALL(*obs2, OnCacheAllocate(DISCRETE_INPUT, cache_range2)).Times(1);

    Server->AllocateCache();

    // form read requests and send them to server
    uint8_t q1[] = { 0x10, 0x00, 40, 0x00, 0x02, 0x12, 0x34, 0x56, 0x78 };
    /* uint8_t q2[] = { 0x10, 0x00, 0x0A, 0x00, 0x02 }; */
    TModbusQuery write1(q1, sizeof (q1), 0);
    /* TModbusQuery read2(q2, sizeof (q2), 0); */

    Backend->PushQuery(write1);
    /* Backend->PushQuery(read2); */

    EXPECT_CALL(*obs1, OnSetValue(HOLDING_REGISTER, 0x42, 40, 2, static_cast<uint16_t *>(Backend->GetCache(HOLDING_REGISTER)) + 40)).WillOnce(Return(REPLY_CACHED));

    Backend->SetSlave(0x42);

    while (!Backend->IncomingQueries.empty())
        Server->Loop();
}
