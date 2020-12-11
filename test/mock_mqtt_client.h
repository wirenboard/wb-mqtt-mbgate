#pragma once

#include <wblib/mqtt.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>


class MockMQTTClient : public WBMQTT::TMqttClient {
public:
    ~MockMQTTClient();

    MOCK_METHOD0(Start, void());
    MOCK_METHOD0(Stop, void());
    MOCK_METHOD1(Publish, void(const WBMQTT::TMqttMessage & message));
    MOCK_METHOD1(PublishSynced, WBMQTT::TFuture<void>(const WBMQTT::TMqttMessage & message));
    MOCK_METHOD2(Subscribe, void(WBMQTT::TMqttMessageHandler callback, const std::string & topic));
    MOCK_METHOD2(Subscribe, void(WBMQTT::TMqttMessageHandler callback, const std::vector<std::string> & topics));
    MOCK_METHOD1(Unsubscribe, void(const std::string & topic));
    MOCK_METHOD1(Unsubscribe, void(const std::vector<std::string> & topics));
    MOCK_METHOD1(WaitForReady, void(std::function<void()> readyCallback));
};
