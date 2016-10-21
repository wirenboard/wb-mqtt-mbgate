/*!\file mock_mqtt_observer.h
 * \brief Mock of MQTT observer
 * \author Nikita webconn Maslov <n.maslov@contactless.ru>
 */

#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <wbmqtt/mqtt_wrapper.h>

class MockMQTTObserver : public IMQTTObserver
{
public:
    virtual ~MockMQTTObserver() {}
    
    MOCK_METHOD1(OnConnect, void(int));
    MOCK_METHOD1(OnMessage, void(const struct mosquitto_message *message));
    MOCK_METHOD3(OnSubscribe, void(int mid, int qos_count, const int *granted_qos));
};
