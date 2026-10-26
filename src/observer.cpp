#include "observer.h"

#include "log.h"

using namespace std;
using namespace WBMQTT;

TGatewayObserver::TGatewayObserver(const string& topic, PMQTTConverter conv, PMqttClient mqtt)
    : Cache(nullptr),
      CacheSize(0),
      Conv(conv),
      Topic(topic),
      Mqtt(mqtt),
      HasValue(false),
      HasReadError(false)
{
    Mqtt->Subscribe([this](const TMqttMessage& msg) { this->OnMessage(msg); }, topic);
    Mqtt->Subscribe([this](const TMqttMessage& msg) { this->OnErrorMessage(msg); }, topic + "/meta/error");
}

void TGatewayObserver::OnMessage(const TMqttMessage& message)
{
    // no pointer to cache - retire
    if (!Cache)
        return;

    // pack incoming message into Modbus cache
    if (!message.Payload.empty()) {
        Conv->Pack(message.Payload, Cache, CacheSize);
    }
    HasValue = !message.Payload.empty();
}

void TGatewayObserver::OnErrorMessage(const TMqttMessage& message)
{
    HasReadError = message.Payload.find('r') != string::npos;
}

TReplyState TGatewayObserver::OnGetValue(TStoreType type, uint8_t unit_id, uint16_t start, unsigned count, void* data)
{
    // let the master know that the value is unavailable instead of replying with a stale one
    return (HasValue && !HasReadError) ? REPLY_CACHED : REPLY_GATEWAY_TARGET_FAILED;
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
