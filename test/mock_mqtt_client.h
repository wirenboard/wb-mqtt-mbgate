#pragma once

#include <wbmqtt/mqtt_wrapper.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>


class MockMQTTClient : public TMQTTClientBase {
public:
    ~MockMQTTClient();

    MOCK_METHOD0(Connect, void());
    MOCK_METHOD5(Publish, int(int *, const std::string &, const std::string &, int, bool));
    MOCK_METHOD3(Subscribe, int(int *, const std::string &, int));
    MOCK_CONST_METHOD0(Id, std::string());
};
