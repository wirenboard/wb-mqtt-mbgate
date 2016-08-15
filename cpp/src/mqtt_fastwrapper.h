/*!\file mqtt_fast_observer.h
 * \brief MQTT observer manager
 * Observer designed to manage with lots of small and simple gateway observers quickly
 * \author Nikita webconn Maslov <n.maslov@contactless.ru>
 */

#pragma once

#include <wbmqtt/mqtt_wrapper.h>
#include <map>
#include <list>
#include <string>
#include <memory>

class TMQTTFastObserver : public IMQTTObserver
{
public:
    virtual ~TMQTTFastObserver();

    virtual void OnConnect(int rc);
    virtual void OnMessage(const struct mosquitto_message *message);
    virtual void OnSubscribe(int mid, int qos_count, const int *granted_qos);

    void Observe(PMQTTObserver observer, const std::string &topic)
    {
        Observers[topic].push_back(observer);
    }

protected:
    std::map<std::string, std::list<PMQTTObserver>> Observers;
};

typedef std::shared_ptr<TMQTTFastObserver> PMQTTFastObserver;
