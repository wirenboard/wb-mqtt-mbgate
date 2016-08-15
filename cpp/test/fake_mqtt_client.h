/*!\file test/fake_mqtt_client.h
 * \brief Fake MQTT client for testing
 * \author Nikita webconn Maslov <n.maslov@contactless.ru>
 */

#pragma once

#include <wbmqtt/mqtt_wrapper.h>
#include <string>
#include <memory>

class FakeMQTTClient : public TMQTTClientBase
{
public:
    FakeMQTTClient(const std::string &_id = "")
        : _Id(_id)
    {}

    virtual ~FakeMQTTClient()
    {}

    virtual void Connect()
    {}

    virtual int Publish(int *mid, const std::string& topic, const std::string& payload="", int qos=0, bool retain=false)
    { return 0; }

    virtual int Subscribe(int *mid, const std::string& sub, int qos = 0)
    { return 0; }

    virtual std::string Id() const
    {
        return _Id;
    }

    void PushMessage(const std::string &topic, const std::string &payload)
    {
        struct mosquitto_message msg;
        msg.topic = const_cast<char *>(topic.c_str());
        msg.payload = const_cast<char *>(payload.c_str());

        for (auto &item: GetObservers())
            item->OnMessage(&msg);
    }

protected:
    std::string _Id;
};

typedef std::shared_ptr<FakeMQTTClient> PFakeMQTTClient;
