#include "logging.h"

TMQTTLoggingObserver::~TMQTTLoggingObserver()
{

}

void TMQTTLoggingObserver::OnConnect(int rc)
{
    LOG(INFO) << "Connected to MQTT server";
}

void TMQTTLoggingObserver::OnMessage(const struct mosquitto_message *message)
{
    LOG(DEBUG) << "Received MQTT message on " << static_cast<const char *>(message->topic) << ": " << static_cast<const char *>(message->payload);
}

void TMQTTLoggingObserver::OnSubscribe(int mid, int qos_count, const int *granted_qos)
{
}
