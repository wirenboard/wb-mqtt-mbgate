#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "mock_mqtt_observer.h"
#include "fake_mqtt_client.h"
#include "mqtt_fastwrapper.h"

#include <string>
#include <memory>

#define NUM_OBSERVERS 10

using namespace std;

using ::testing::_;

MATCHER_P2(MQTTMessageIs, topic, payload, "") { return string(static_cast<const char *>(arg->topic)) == topic && string(static_cast<const char *>(arg->payload)) == payload; }

class MQTTFastWrapperTest : public ::testing::Test
{   
public:
    PFakeMQTTClient Mqtt;
    PMQTTFastObserver FastObserver;
    shared_ptr<MockMQTTObserver> observers[NUM_OBSERVERS];
    
    void SetUp()
    {
        Mqtt = make_shared<FakeMQTTClient>();
        FastObserver = make_shared<TMQTTFastObserver>();

        for (int i = 0; i < NUM_OBSERVERS; i++) {
            observers[i] = make_shared<MockMQTTObserver>();
        }
        
        Mqtt->Observe(FastObserver);

        FastObserver->Observe(observers[0], "test/topic/1");
        FastObserver->Observe(observers[1], "test/topic/1");

        FastObserver->Observe(observers[2], "test/topic/2");

        FastObserver->Observe(observers[3], "test/topic/3");

        FastObserver->Observe(observers[4], "test/topic/4");

        FastObserver->Observe(observers[5], "test/topic/5");
        FastObserver->Observe(observers[6], "test/topic/5");
        FastObserver->Observe(observers[7], "test/topic/5");
    }

    void TearDown() {}
};

TEST_F(MQTTFastWrapperTest, ConnectTest)
{
    EXPECT_CALL(*observers[0], OnConnect(_)).Times(1);
    EXPECT_CALL(*observers[1], OnConnect(_)).Times(1);
    EXPECT_CALL(*observers[2], OnConnect(_)).Times(1);
    EXPECT_CALL(*observers[3], OnConnect(_)).Times(1);
    EXPECT_CALL(*observers[4], OnConnect(_)).Times(1);
    EXPECT_CALL(*observers[5], OnConnect(_)).Times(1);
    EXPECT_CALL(*observers[6], OnConnect(_)).Times(1);
    EXPECT_CALL(*observers[7], OnConnect(_)).Times(1);

    FastObserver->OnConnect(0);
}

TEST_F(MQTTFastWrapperTest, MessageTest)
{
    EXPECT_CALL(*observers[0], OnMessage(MQTTMessageIs("test/topic/1", "HelloWorld1"))).Times(1);
    EXPECT_CALL(*observers[1], OnMessage(MQTTMessageIs("test/topic/1", "HelloWorld1"))).Times(1);
    
    EXPECT_CALL(*observers[2], OnMessage(MQTTMessageIs("test/topic/2", "HelloWorld2"))).Times(1);
    EXPECT_CALL(*observers[3], OnMessage(MQTTMessageIs("test/topic/3", "HelloWorld3"))).Times(1);
    
    EXPECT_CALL(*observers[5], OnMessage(MQTTMessageIs("test/topic/5", "HelloWorld5"))).Times(3);
    EXPECT_CALL(*observers[6], OnMessage(MQTTMessageIs("test/topic/5", "HelloWorld5"))).Times(3);
    EXPECT_CALL(*observers[7], OnMessage(MQTTMessageIs("test/topic/5", "HelloWorld5"))).Times(3);

    
    Mqtt->PushMessage("test/topic/1", "HelloWorld1");

    Mqtt->PushMessage("test/topic/2", "HelloWorld2");
    Mqtt->PushMessage("test/topic/3", "HelloWorld3");

    for (int i = 0; i < 3; i++)
        Mqtt->PushMessage("test/topic/5", "HelloWorld5");
}
