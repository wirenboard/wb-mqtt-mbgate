#include "observer.h"

#include "log.h"

using namespace std;
using namespace WBMQTT;

TGatewayObserver::TGatewayObserver(const string& topic, PMQTTConverter conv, PMqttClient mqtt)
    : Cache(nullptr),
      CacheSize(0),
      Conv(conv),
      Topic(topic),
      Mqtt(mqtt)
{
    Mqtt->Subscribe([this](const TMqttMessage& msg) { this->OnMessage(msg); }, topic);
}

void TGatewayObserver::OnMessage(const TMqttMessage& message)
{
    // no pointer to cache - retire
    if (!Cache)
        return;

    // pack incoming message into Modbus cache
    Conv->Pack(message.Payload, Cache, CacheSize);
}

void TGatewayObserver::OnCacheAllocate(TStoreType type, uint8_t slave_id, const TModbusCacheAddressRange& range)
{
    Cache = range.cbegin()->second.second;
    CacheSize = range.cbegin()->second.first;
}

TReplyState TGatewayObserver::OnSetValue(TStoreType type,
                                         uint8_t unit_id,
                                         uint16_t start,
                                         unsigned count,
                                         const void* data)
{

    TMqttMessage msg(Topic + "/on", Conv->Unpack(data, count), 1, false);

    Mqtt->Publish(msg);

    ::Debug.Log() << "[gateway] Set value via Modbus: " << Topic << " : " << msg.Payload;

    return REPLY_OK;
}
