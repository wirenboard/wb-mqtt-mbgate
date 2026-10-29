/*!\file observer.h
 * \brief Gateway observer interface
 * \author Nikita webconn Maslov <n.maslov@contactless.ru>
 */

#pragma once

#include <atomic>

#include <wblib/mqtt.h>

#include "modbus_wrapper.h"
#include "mqtt_converters.h"

class TGatewayObserver: public IModbusServerObserver
{
public:
    TGatewayObserver(const std::string& topic, PMQTTConverter conv, WBMQTT::PMqttClient mqtt);

    // Modbus callbacks
    TReplyState OnSetValue(TStoreType type, uint8_t unit_id, uint16_t start, unsigned count, const void* data) override;
    void OnCacheAllocate(TStoreType type, uint8_t area, const TModbusCacheAddressRange& cache) override;
    // value itself is taken from cache, here we only report unavailable value
    TReplyState OnGetValue(TStoreType type, uint8_t unit_id, uint16_t start, unsigned count, void* data) override;

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

    /*! Value was received from MQTT (written by MQTT thread, read by Modbus one) */
    std::atomic<bool> HasValue;

    /*! Source of the value reports read error in meta/error */
    std::atomic<bool> HasReadError;

private:
    void OnMessage(const WBMQTT::TMqttMessage& message);
    void OnErrorMessage(const WBMQTT::TMqttMessage& message);
};

typedef std::shared_ptr<TGatewayObserver> PGatewayObserver;
