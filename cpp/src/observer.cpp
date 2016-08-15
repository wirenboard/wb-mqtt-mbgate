#include "observer.h"

#include <vector>
#include <string>

#include <iostream>

#include <wbmqtt/utils.h>

using namespace std;

TGatewayObserver::TGatewayObserver(const string &topic, PMQTTConverter conv, weak_ptr<TMQTTClientBase> mqtt)
    : Cache(nullptr)
    , CacheSize(0)
    , Conv(conv)
    , Mqtt(mqtt)

{
    auto pieces = StringSplit(topic, '/');
    Topic = string("/devices/") + pieces[0] + string("/controls/") + pieces[1];
}

void TGatewayObserver::OnConnect(int rc)
{
    auto _Mqtt = Mqtt.lock();
    if (!_Mqtt) // TODO: error handling
        return;

    _Mqtt->Subscribe(nullptr, Topic, 0);
    cerr << "Connected to MQTT server" << endl;
}

void TGatewayObserver::OnMessage(const struct mosquitto_message *msg)
{
    // no pointer to cache - retire
    if (!Cache)
        return;

    // topic should match with this observer
    if (Topic != static_cast<char *>(msg->topic))
        return;

    cerr << "Received message from " << static_cast<char *>(msg->topic) << ": " << static_cast<char *>(msg->payload) << endl;

    // pack incoming message into Modbus cache
    Conv->Pack(static_cast<char *>(msg->payload), Cache, CacheSize);
}

void TGatewayObserver::OnSubscribe(int mid, int qos_count, const int *granted_qos)
{
    cerr << "Subscribed on topic" << endl;
}

void TGatewayObserver::OnCacheAllocate(TStoreType type, const TModbusCacheAddressRange &range)
{
    Cache = range.cbegin()->second.second;
    CacheSize = range.cbegin()->second.first;
    cerr << "Cache allocated" << endl;
}

TReplyState TGatewayObserver::OnSetValue(TStoreType type, uint8_t unit_id, uint16_t start, unsigned count, const void *data)
{
    auto _Mqtt = Mqtt.lock();
    if (!_Mqtt) { // TODO: error handling
        cerr << "MQTT client is not available!!!" << endl;
        return REPLY_SERVER_FAILURE;
    }

    string res = Conv->Unpack(data, count);
    _Mqtt->Publish(nullptr, Topic, res);
    cerr << "Set value w/Modbus: " << Topic << " : " << res << endl;

    return REPLY_OK;
}
