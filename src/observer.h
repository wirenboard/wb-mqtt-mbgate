/*!\file observer.h
 * \brief Gateway observer interface
 * \author Nikita webconn Maslov <n.maslov@contactless.ru>
 */

#pragma once

#include <wblib/mqtt.h>

#include "modbus_wrapper.h"
#include "mqtt_converters.h"

class TGatewayObserver : public IModbusServerObserver
{
public:
    TGatewayObserver(const std::string& topic, PMQTTConverter conv, WBMQTT::PMqttClient mqtt);

    // Modbus callbacks
    TReplyState OnSetValue(TStoreType type, uint8_t unit_id, uint16_t start, unsigned count, const void* data) override;
    void OnCacheAllocate(TStoreType type, uint8_t area, const TModbusCacheAddressRange& cache) override;
    // no need of OnGetValue, use cache instead
    
protected:
    /*! Pointer to Modbus cache area */
    void* Cache;

    /*! Size of allocated cache - safety */
    size_t CacheSize;

    /*! Pointer to MQTT converter */
    PMQTTConverter Conv;

    /*! Bridged MQTT topic */
    std::string Topic;

    /*! Pointer to MQTT client */
    WBMQTT::PMqttClient Mqtt;

private:
    void OnMessage(const WBMQTT::TMqttMessage& message);
};

typedef std::shared_ptr<TGatewayObserver> PGatewayObserver;
