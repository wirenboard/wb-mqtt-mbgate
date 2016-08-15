#include "config_parser.h"

#include <string>
#include <memory>
#include <fstream>
#include <iostream>
#include <jsoncpp/json/json.h>

#include "mqtt_converters.h"
#include "modbus_lmb_backend.h"
#include <wbmqtt/mqtt_wrapper.h>
#include <wbmqtt/utils.h>
#include "observer.h"
#include "mqtt_fastwrapper.h"

#include <tuple>

using namespace std;

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


TJSONConfigParser::TJSONConfigParser(const std::string &filename)
{
    Json::Reader reader;

    std::ifstream fileStream(filename);

    if (!reader.parse(fileStream, Root, false))
        throw ConfigParserException(string("Can't parse input JSON file: ") 
                    + reader.getFormattedErrorMessages());
}

bool TJSONConfigParser::Debug()
{
    return Root["debug"].asBool();
}

tuple<PModbusServer, PMQTTClient> TJSONConfigParser::Build()
{
    // create Modbus server
    string modbus_host = Root["modbus"]["host"].asString();
    int modbus_port = Root["modbus"]["port"].asInt();

    cerr << "Modbus configuration: host " << modbus_host << ", port " << modbus_port << endl;

    PModbusBackend modbusBackend = make_shared<TModbusTCPBackend>(modbus_host.c_str(), modbus_port);
    PModbusServer modbus = make_shared<TModbusServer>(modbusBackend);

    // create MQTT client
    string mqtt_host = Root["mqtt"]["host"].asString();
    int mqtt_port = Root["mqtt"]["port"].asInt();

    cerr << "MQTT configuration: host " << mqtt_host << ", port " << mqtt_port << endl;

    TMQTTClient::TConfig mqtt_config;
    mqtt_config.Host = mqtt_host;
    mqtt_config.Port = mqtt_port;

    PMQTTClient mqtt = make_shared<TMQTTClient>(mqtt_config);
    PMQTTFastObserver fobs = make_shared<TMQTTFastObserver>();

    mqtt->Observe(fobs);

    // create observers and link'em with MQTT and Modbus
    _BuildStore(COIL, Root["registers"]["coils"], modbus, mqtt, fobs);
    _BuildStore(DISCRETE_INPUT, Root["registers"]["discretes"], modbus, mqtt, fobs);
    _BuildStore(HOLDING_REGISTER, Root["registers"]["holdings"], modbus, mqtt, fobs);
    _BuildStore(INPUT_REGISTER, Root["registers"]["inputs"], modbus, mqtt, fobs);

    return make_tuple(modbus, mqtt);
}

void TJSONConfigParser::_BuildStore(TStoreType type, Json::Value &list, PModbusServer modbus, PMQTTClient mqtt, PMQTTFastObserver fobs)
{
    cerr << "Processing store " << type << endl;

    for (const auto &reg_item : list) {
        if (!reg_item["enabled"].asBool())
            continue;

        int address = reg_item["address"].asInt();
        string topic = reg_item["topic"].asString();
        int size;

        cerr << "\tElement " << topic << " : " << address << endl;
        
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
                    throw ConfigParserException("Unknown integer format: " + format);
                }

                conv = make_shared<TMQTTIntConverter>(int_type, scale, size, byteswap, wordswap);
                size /= 2;
            }
        }

        obs = make_shared<TGatewayObserver>(expandTopic(topic), conv, mqtt);

        /* cerr << "Creating observer on " << address << ":" << size << endl; */
        modbus->Observe(obs, type, TModbusAddressRange(address, size));
        fobs->Observe(obs, expandTopic(topic));
    }
}
