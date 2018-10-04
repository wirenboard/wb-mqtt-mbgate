#include "observer.h"

#include <vector>
#include <string>
#include <wbmqtt/utils.h>
#include "logging.h"

using namespace std;

TGatewayObserver::TGatewayObserver(const string &topic, PMQTTConverter conv, weak_ptr<TMQTTClientBase> mqtt)
    : Cache(nullptr)
    , CacheSize(0)
    , Conv(conv)
    , Topic(topic)
    , Mqtt(mqtt)
{}

void TGatewayObserver::OnConnect(int rc)
{
    auto _Mqtt = Mqtt.lock();
    if (!_Mqtt) // TODO: error handling
        return;

    _Mqtt->Subscribe(nullptr, Topic, 0);
}

void TGatewayObserver::OnMessage(const struct mosquitto_message *msg)
{
    // no pointer to cache - retire
    if (!Cache)
        return;

    // topic should match with this observer
    if (Topic != static_cast<char *>(msg->topic))
        return;

    // pack incoming message into Modbus cache
    Conv->Pack(static_cast<char *>(msg->payload), Cache, CacheSize);
}

void TGatewayObserver::OnSubscribe(int mid, int qos_count, const int *granted_qos)
{
}

void TGatewayObserver::OnCacheAllocate(TStoreType type, uint8_t slave_id, const TModbusCacheAddressRange &range)
{
    Cache = range.cbegin()->second.second;
    CacheSize = range.cbegin()->second.first;
}

TReplyState TGatewayObserver::OnSetValue(TStoreType type, uint8_t unit_id, uint16_t start, unsigned count, const void *data)
{
    auto _Mqtt = Mqtt.lock();
    if (!_Mqtt) { // TODO: error handling
        LOG(ERROR) << "MQTT client is not available!";
        return REPLY_SERVER_FAILURE;
    }

    string res = Conv->Unpack(data, count);
    
    // add "/on" suffix to topics
    string topic = Topic;
    topic += "/on";

    _Mqtt->Publish(nullptr, topic, res);

    LOG(DEBUG) << "Set value via Modbus: " << Topic << " : " << res;

    return REPLY_OK;
}
