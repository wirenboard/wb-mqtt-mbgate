/*!\file observer.h
 * \brief Gateway observer interface
 * \author Nikita webconn Maslov <n.maslov@contactless.ru>
 */

#pragma once

#include <string>
#include <memory>

#include <wbmqtt/mqtt_wrapper.h>
#include "modbus_wrapper.h"
#include "mqtt_converters.h"

class TGatewayObserver : public IModbusServerObserver, public IMQTTObserver
{
public:
    TGatewayObserver(const std::string &topic, PMQTTConverter conv, std::weak_ptr<TMQTTClientBase> mqtt);

    // MQTT callbacks
    virtual void OnConnect(int rc);
    virtual void OnMessage(const struct mosquitto_message *message);
    virtual void OnSubscribe(int mid, int qos_count, const int *granted_qos);

    // Modbus callbacks
    virtual TReplyState OnSetValue(TStoreType type, uint8_t unit_id, uint16_t start, unsigned count, const void *data);
    virtual void OnCacheAllocate(TStoreType type, const TModbusCacheAddressRange& cache);
    // no need of OnGetValue, use cache instead
    
protected:
    /*! Pointer to Modbus cache area */
    void* Cache = nullptr;

    /*! Size of allocated cache - safety */
    size_t CacheSize = 0;

    /*! Pointer to MQTT converter */
    PMQTTConverter Conv;

    /*! Bridged MQTT topic */
    std::string Topic;

    /*! Pointer to MQTT client */
    std::weak_ptr<TMQTTClientBase> Mqtt;
};

typedef std::shared_ptr<TGatewayObserver> PGatewayObserver;
