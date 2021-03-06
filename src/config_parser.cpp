#include "config_parser.h"

#include <string>
#include <memory>
#include <fstream>
#include <iostream>
#include <tuple>
#include <unistd.h>
#include <time.h>

#include <wblib/json_utils.h>
#include <wblib/mqtt.h>

#include "log.h"
#include "mqtt_converters.h"
#include "modbus_lmb_backend.h"
#include "observer.h"

using namespace std;
using namespace WBMQTT;
using namespace WBMQTT::JSON;

#define LOG(logger) ::logger.Log() << "[config] "

namespace {
    string expandTopic(const string &t)
    {
        auto lst = StringSplit(t, '/');
        return string("/devices/") + lst[0] + "/controls/" + lst[1];
    }
};

IConfigParser::~IConfigParser()
{
}

TJSONConfigParser::TJSONConfigParser(const string& config_file, const string& schema_file) :
    Root(Parse(config_file))
{
    auto schema(Parse(schema_file));
    Validate(Root, schema);
}

bool TJSONConfigParser::Debug()
{
    bool debug = false;
    Get(Root, "debug", debug);
    return debug;
}

tuple<PModbusServer, PMqttClient> TJSONConfigParser::Build()
{
    // create Modbus server
    string modbus_host = Root["modbus"]["host"].asString();
    int modbus_port = Root["modbus"]["port"].asInt();

    LOG(Debug) << "Modbus configuration: host " << modbus_host << ", port " << modbus_port;

    PModbusBackend modbusBackend = make_shared<TModbusTCPBackend>(modbus_host, modbus_port);
    PModbusServer modbus = make_shared<TModbusServer>(modbusBackend);

    // create MQTT client
    string mqtt_host = Root["mqtt"]["host"].asString();
    int mqtt_port = Root["mqtt"]["port"].asInt();
    int mqtt_keepalive = 60;
    Get(Root["mqtt"], "keepalive", mqtt_keepalive);

    LOG(Debug) << "MQTT configuration: host " << mqtt_host << ", port " << mqtt_port << ", keepalive " << mqtt_keepalive;

    TMosquittoMqttConfig mqtt_config;
    mqtt_config.Host = mqtt_host;
    mqtt_config.Port = mqtt_port;
    mqtt_config.Keepalive = mqtt_keepalive;
    mqtt_config.Id = string("mqtt-mbgate-") + to_string(time(NULL));

    PMqttClient mqtt = NewMosquittoMqttClient(mqtt_config);

    // create observers and link'em with MQTT and Modbus
    _BuildStore(COIL, Root["registers"]["coils"], modbus, mqtt);
    _BuildStore(DISCRETE_INPUT, Root["registers"]["discretes"], modbus, mqtt);
    _BuildStore(HOLDING_REGISTER, Root["registers"]["holdings"], modbus, mqtt);
    _BuildStore(INPUT_REGISTER, Root["registers"]["inputs"], modbus, mqtt);

    return make_tuple(modbus, mqtt);
}

void TJSONConfigParser::_BuildStore(TStoreType type, const Json::Value& list, PModbusServer modbus, PMqttClient mqtt)
{
    LOG(Debug) << "Processing store " << type;

    for (const auto &reg_item : list) {
        if (!reg_item["enabled"].asBool())
            continue;

        int address = reg_item["address"].asInt();
        int slave_id = reg_item["unitId"].asInt();
        string topic = reg_item["topic"].asString();
        int size;

        LOG(Debug) << "Element " << topic << " : " << address;
        
        PGatewayObserver obs;
        PMQTTConverter conv;

        if (type == COIL || type == DISCRETE_INPUT) {
            conv = make_shared<TMQTTDiscrConverter>();
            size = 1;
        } else {
            string format = reg_item["format"].asString();
            bool byteswap = reg_item["byteswap"].asBool();
            bool wordswap = reg_item["wordswap"].asBool();
            size = reg_item["size"].asInt();

            if (format == "varchar") {
                conv = make_shared<TMQTTTextConverter>(size, byteswap, wordswap);
            } else if (format == "float") {
                conv = make_shared<TMQTTFloatConverter>(size, byteswap, wordswap);
                size /= 2;
            } else {
                double scale = reg_item["scale"].asFloat();
                
                TMQTTIntConverter::IntegerType int_type;
                if (format == "signed") {
                    int_type = TMQTTIntConverter::SIGNED;
                } else if (format == "unsigned") {
                    int_type = TMQTTIntConverter::UNSIGNED;
                } else if (format == "bcd") {
                    int_type = TMQTTIntConverter::BCD;
                } else {
                    throw runtime_error("Unknown integer format: " + format);
                }

                conv = make_shared<TMQTTIntConverter>(int_type, scale, size, byteswap, wordswap);
                size /= 2;
            }
        }

        obs = make_shared<TGatewayObserver>(expandTopic(topic), conv, mqtt);

        LOG(Debug) << "Creating observer on " << address << ":" << size;

        try {
            modbus->Observe(obs, type, TModbusAddressRange(address, size), slave_id);
        } catch (const WrongSegmentException &e) {
            throw runtime_error(string("Address overlapping: ") + StoreTypeToString(type) + ": topic " + topic);
        }
    }
}
