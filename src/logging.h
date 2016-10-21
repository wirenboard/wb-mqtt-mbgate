/*!\file logging.h
 * \brief Logging utilities
 * \author Nikita webconn Maslov <n.maslov@contactless.ru>
 */

#pragma once

#include <log4cpp/Category.hh>
#define LOG(x) (log4cpp::Category::getRoot() << log4cpp::Priority::x )

#include <memory>
#include <wbmqtt/mqtt_wrapper.h>

class TMQTTLoggingObserver : public IMQTTObserver
{
public:
    virtual ~TMQTTLoggingObserver();
    
    virtual void OnConnect(int rc);
    virtual void OnMessage(const struct mosquitto_message *message);
    virtual void OnSubscribe(int mid, int qos_count, const int *granted_qos);
};

typedef std::shared_ptr<TMQTTLoggingObserver> PMQTTLoggingObserver;
