#include "fake_modbus_backend.h"
#include "observer.h"
#include "modbus_wrapper.h"
#include "mock_mqtt_client.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <string>
#include <memory>

using namespace std;
using ::testing::_;
using ::testing::Return;

class GatewayTest : public ::testing::Test
{
public:
    shared_ptr<MockMQTTClient> Mqtt;
    shared_ptr<TFakeModbusBackend> ModbusBackend;
    shared_ptr<TModbusServer> ModbusServer;

    shared_ptr<TGatewayObserver> observers[3];

    void SetUp() 
    {
        Mqtt = make_shared<MockMQTTClient>();
        ModbusBackend = make_shared<TFakeModbusBackend>();
        ModbusServer = make_shared<TModbusServer>(ModbusBackend);

        ModbusBackend->AllocateCache(100, 100, 100, 100);
    
        PMQTTConverter conv1 = make_shared<TMQTTIntConverter>(TMQTTIntConverter::SIGNED, 1.0, 2);
        observers[0] = make_shared<TGatewayObserver>("device1/topic1", conv1, Mqtt);
        ModbusServer->Observe(observers[0], TStoreType::HOLDING_REGISTER, TModbusAddressRange(0, 1));
        Mqtt->Observe(observers[0]);

        observers[1] = make_shared<TGatewayObserver>("device1/topic2", conv1, Mqtt);
        ModbusServer->Observe(observers[1], TStoreType::HOLDING_REGISTER, TModbusAddressRange(1, 1));
        Mqtt->Observe(observers[1]);

        ModbusServer->AllocateCache();
    }

    void TearDown() 
    {
    }
};

TEST_F(GatewayTest, SingleWriteTest)
{
    // Write 1 register via Preset Single Register
    // Register 0 with value 0x1234 (4660d)
    uint8_t q1[] = {
        0x10
    };
    TModbusQuery query1(q1, sizeof (q1), 0);
}

TEST_F(GatewayTest, MultiWriteTest)
{
    // Write 2 registers from 0 with values 0x1234 (4660d) and 0x5678 (22136)
    uint8_t q1[] = { 
        0x10, 
        0x00, 0x00,
        0x00, 0x02,
        0x04,
        0x12, 0x34,
        0x56, 0x78
    };
    TModbusQuery query1(q1, sizeof (q1), 0);

    uint8_t q2[] = {
        0x10,
        0x00, 0x00,
        0x00, 0x02,
    };
    TModbusQuery query2(q2, sizeof (q2), 0);

    ModbusBackend->PushQuery(query1);

    EXPECT_CALL(*Mqtt, Publish(_, string("/devices/device1/controls/topic1"), string("4660"), _, _)).WillOnce(Return(0));
    EXPECT_CALL(*Mqtt, Publish(_, string("/devices/device1/controls/topic2"), string("22136"), _, _)).WillOnce(Return(0));

    while (!ModbusBackend->IncomingQueries.empty())
        ModbusServer->Loop();
}
