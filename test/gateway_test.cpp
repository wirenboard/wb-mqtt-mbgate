#include "fake_modbus_backend.h"
#include "mock_mqtt_client.h"
#include "modbus_wrapper.h"
#include "observer.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

using namespace std;
using namespace WBMQTT;
using namespace ::testing;

class GatewayTest: public ::testing::Test
{
public:
    shared_ptr<NiceMock<MockMQTTClient>> Mqtt;
    shared_ptr<TFakeModbusBackend> ModbusBackend;
    shared_ptr<TModbusServer> ModbusServer;

    shared_ptr<TGatewayObserver> observers[5];

    void SetUp()
    {
        Mqtt = make_shared<NiceMock<MockMQTTClient>>();
        ModbusBackend = make_shared<TFakeModbusBackend>();
        ModbusServer = make_shared<TModbusServer>(ModbusBackend);

        ModbusBackend->AllocateCache(0, 100, 100, 100, 100);

        PMQTTConverter conv1 = make_shared<TMQTTIntConverter>(TMQTTIntConverter::SIGNED, 1.0, 2);
        observers[0] = make_shared<TGatewayObserver>("/devices/device1/controls/topic1", conv1, Mqtt);
        ModbusServer->Observe(observers[0], TStoreType::HOLDING_REGISTER, TModbusAddressRange(0, 1));

        observers[1] = make_shared<TGatewayObserver>("/devices/device1/controls/topic2", conv1, Mqtt);
        ModbusServer->Observe(observers[1], TStoreType::HOLDING_REGISTER, TModbusAddressRange(1, 1));

        // test a coil to write on "#/on"
        PMQTTConverter coil_conv = make_shared<TMQTTDiscrConverter>();
        observers[2] = make_shared<TGatewayObserver>("/devices/device1/controls/coil1", coil_conv, Mqtt);
        ModbusServer->Observe(observers[2], TStoreType::COIL, TModbusAddressRange(0, 1));

        observers[3] = make_shared<TGatewayObserver>("/devices/device1/controls/coil2", coil_conv, Mqtt);
        ModbusServer->Observe(observers[3], TStoreType::COIL, TModbusAddressRange(1, 1));

        observers[4] = make_shared<TGatewayObserver>("/devices/device1/controls/coil3", coil_conv, Mqtt);
        ModbusServer->Observe(observers[4], TStoreType::COIL, TModbusAddressRange(2, 1));

        ModbusServer->AllocateCache();
    }

    void TearDown()
    {}
};

TEST_F(GatewayTest, SingleWriteTest)
{
    // Write 1 register via Preset Single Register
    // Register 0 with value 0x1234 (4660d)
    uint8_t q1[] = {0x06, 0x00, 0x00, 0x12, 0x34};
    TModbusQuery query1(q1, sizeof(q1), 0);
    ModbusBackend->PushQuery(query1);

    // Write 1 register via Preset Multiple Registers
    // Register 1 with value 0x5678 (22136)
    uint8_t q2[] = {0x10, 0x00, 0x01, 0x00, 0x01, 0x02, 0x56, 0x78};
    TModbusQuery query2(q2, sizeof(q2), 0);
    ModbusBackend->PushQuery(query2);

    EXPECT_CALL(*Mqtt,
                Publish(AllOf(Field(&TMqttMessage::Topic, "/devices/device1/controls/topic1/on"),
                              Field(&TMqttMessage::Payload, "4660"))));

    EXPECT_CALL(*Mqtt,
                Publish(AllOf(Field(&TMqttMessage::Topic, "/devices/device1/controls/topic2/on"),
                              Field(&TMqttMessage::Payload, "22136"))));

    while (!ModbusBackend->IncomingQueries.empty())
        ModbusServer->Loop();
}

TEST_F(GatewayTest, MultiWriteTest)
{
    // Write 2 registers from 0 with values 0x1234 (4660d) and 0x5678 (22136)
    uint8_t q1[] = {0x10, 0x00, 0x00, 0x00, 0x02, 0x04, 0x12, 0x34, 0x56, 0x78};
    TModbusQuery query1(q1, sizeof(q1), 0);

    ModbusBackend->PushQuery(query1);

    EXPECT_CALL(*Mqtt,
                Publish(AllOf(Field(&TMqttMessage::Topic, "/devices/device1/controls/topic1/on"),
                              Field(&TMqttMessage::Payload, "4660"))));

    EXPECT_CALL(*Mqtt,
                Publish(AllOf(Field(&TMqttMessage::Topic, "/devices/device1/controls/topic2/on"),
                              Field(&TMqttMessage::Payload, "22136"))));

    while (!ModbusBackend->IncomingQueries.empty())
        ModbusServer->Loop();
}

TEST_F(GatewayTest, CoilWriteTest)
{
    // Test 'Write single coil' command
    uint8_t q1[] = {0x05, 0x00, 0x00, 0xff, 0x00};
    TModbusQuery query1(q1, sizeof(q1), 0);
    ModbusBackend->PushQuery(query1);

    // Test 'Write multiple coils' command for coil2 and coil3
    uint8_t q2[] = {0x0f, 0x00, 0x01, 0x00, 0x02, 0x01, 0x02};
    TModbusQuery query2(q2, sizeof(q2), 0);
    ModbusBackend->PushQuery(query2);

    EXPECT_CALL(*Mqtt,
                Publish(AllOf(Field(&TMqttMessage::Topic, "/devices/device1/controls/coil1/on"),
                              Field(&TMqttMessage::Payload, "1"))));

    EXPECT_CALL(*Mqtt,
                Publish(AllOf(Field(&TMqttMessage::Topic, "/devices/device1/controls/coil2/on"),
                              Field(&TMqttMessage::Payload, "0"))));

    EXPECT_CALL(*Mqtt,
                Publish(AllOf(Field(&TMqttMessage::Topic, "/devices/device1/controls/coil3/on"),
                              Field(&TMqttMessage::Payload, "1"))));

    while (!ModbusBackend->IncomingQueries.empty())
        ModbusServer->Loop();
}
