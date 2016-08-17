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
    shared_ptr<TFakeModbusBackend> Backend;
    PModbusServer Server;
public:
    void SetUp() 
    {
        Backend = make_shared<TFakeModbusBackend>();
        Server = make_shared<TModbusServer>(Backend);

        Backend->AllocateCache(1, 10, 10, 10, 10);
        Backend->AllocateCache(2, 10, 10, 10, 10);
        Backend->AllocateCache(5, 10, 10, 10, 10);
    }
    void TearDown() {}
};

TEST_F(MultiUnitIDTest, ObserveTest)
{
    shared_ptr<MockModbusServerObserver> obs1, obs2, obs3, obs4;

    obs1 = make_shared<MockModbusServerObserver>();
    obs2 = make_shared<MockModbusServerObserver>();
    obs3 = make_shared<MockModbusServerObserver>();
    obs4 = make_shared<MockModbusServerObserver>();

    TModbusAddressRange r1(0, 5), r2(5, 3), r3(2, 5), r4(4, 5);

    Server->Observe(obs1, COIL, r1, 1);
    Server->Observe(obs2, COIL, r2, 1);

    Server->Observe(obs3, COIL, r3, 2);
    Server->Observe(obs4, COIL, r4, 5);

    EXPECT_CALL(*obs1, OnCacheAllocate());

    Server->AllocateCache();
}
