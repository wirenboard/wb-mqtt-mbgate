#include "config_parser.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <time.h>
#include <tuple>
#include <unistd.h>

#include <wblib/json_utils.h>
#include <wblib/mqtt.h>

#include "log.h"
#include "mbgate_exception.h"
#include "modbus_lmb_backend.h"
#include "mqtt_converters.h"
#include "observer.h"

using namespace std;
using namespace WBMQTT;
using namespace WBMQTT::JSON;

#define LOG(logger) ::logger.Log() << "[config] "

namespace
{
    string expandTopic(const string& t)
    {
        auto lst = StringSplit(t, '/');
        return string("/devices/") + lst[0] + "/controls/" + lst[1];
    }

    string PortName(const Json::Value& port, Json::ArrayIndex index)
    {
        auto name = port["name"].asString();
        return "Additional port " + to_string(index + 1) + (name.empty() ? "" : " (" + name + ")");
    }

    bool IsAnyHost(const string& host)
    {
        return host.empty() || host == "*" || host == "0.0.0.0";
    }

    bool IsSameBinding(const Json::Value& a, const Json::Value& b)
    {
        if (a.isMember("path") || b.isMember("path")) {
            return a["path"].asString() == b["path"].asString();
        }
        auto host_a = a["host"].asString();
        auto host_b = b["host"].asString();
        return a["port"].asInt() == b["port"].asInt() && (host_a == host_b || IsAnyHost(host_a) || IsAnyHost(host_b));
    }
};

IConfigParser::~IConfigParser()
{}

TJSONConfigParser::TJSONConfigParser(const string& config_file, const string& schema_file): Root(Parse(config_file))
{
    try {
        auto schema(Parse(schema_file));
        Validate(Root, schema);
    } catch (const std::runtime_error& e) {
        throw TConfigException(e.what());
    }
}

bool TJSONConfigParser::Debug()
{
    bool debug = false;
    Get(Root, "debug", debug);
    return debug;
}

tuple<vector<PModbusServer>, PMqttClient> TJSONConfigParser::Build()
{
    _CheckPortConflicts();

    // create MQTT client
    string mqtt_host = Root["mqtt"]["host"].asString();
    int mqtt_port = Root["mqtt"]["port"].asInt();
    int mqtt_keepalive = 60;
    Get(Root["mqtt"], "keepalive", mqtt_keepalive);

    LOG(Debug) << "MQTT configuration: host " << mqtt_host << ", port " << mqtt_port << ", keepalive "
               << mqtt_keepalive;

    TMosquittoMqttConfig mqtt_config;
    mqtt_config.Host = mqtt_host;
    mqtt_config.Port = mqtt_port;
    mqtt_config.Keepalive = mqtt_keepalive;
    mqtt_config.Id = string("mqtt-mbgate-") + to_string(time(NULL));

    PMqttClient mqtt = NewMosquittoMqttClient(mqtt_config);

    vector<PModbusServer> servers;
    auto addPort = [&](const string& name, const Json::Value& modbus_data, const Json::Value& registers) {
        auto server = _BuildPort(name, modbus_data, registers, mqtt);
        if (server) {
            servers.push_back(server);
        } else {
            LOG(Info) << name << " has no enabled channels, skipping it";
        }
    };

    addPort("Main port", Root["modbus"], Root["registers"]);
    const auto& additional_ports = Root["additional_ports"];
    for (Json::ArrayIndex i = 0; i < additional_ports.size(); ++i) {
        addPort(PortName(additional_ports[i], i), additional_ports[i]["modbus"], additional_ports[i]["registers"]);
    }

    if (servers.empty()) {
        throw TEmptyConfigException();
    }

    return make_tuple(servers, mqtt);
}

void TJSONConfigParser::_CheckPortConflicts()
{
    vector<pair<string, Json::Value>> ports{{"Main port", Root["modbus"]}};
    const auto& additional_ports = Root["additional_ports"];
    for (Json::ArrayIndex i = 0; i < additional_ports.size(); ++i) {
        ports.emplace_back(PortName(additional_ports[i], i), additional_ports[i]["modbus"]);
    }

    for (size_t i = 0; i < ports.size(); ++i) {
        for (size_t j = i + 1; j < ports.size(); ++j) {
            if (IsSameBinding(ports[i].second, ports[j].second)) {
                throw TConfigException(ports[i].first + " and " + ports[j].first + " have the same connection settings");
            }
        }
    }
}

PModbusBackend TJSONConfigParser::_BuildBackend(const Json::Value& modbus_data)
{
    if (modbus_data.isMember("path")) {
        TModbusRTUBackendArgs args{};

        args.Device = modbus_data["path"].asCString();

        args.BaudRate = modbus_data.get("baud_rate", args.BaudRate).asInt();
        args.DataBits = modbus_data.get("data_bits", args.DataBits).asInt();
        args.StopBits = modbus_data.get("stop_bits", args.StopBits).asInt();
        args.Parity = modbus_data.get("parity", std::string(1, args.Parity)).asString()[0];

        LOG(Debug) << "Modbus configuration: device " << args.Device << ", baud rate " << args.BaudRate << ", parity "
                   << args.Parity << ", data bits " << args.DataBits << ", stop bits " << args.StopBits;

        return make_shared<TModbusRTUBackend>(args);
    }

    string modbus_host = modbus_data["host"].asString();
    int modbus_port = modbus_data["port"].asInt();

    LOG(Debug) << "Modbus configuration: host " << modbus_host << ", port " << modbus_port;
    return make_shared<TModbusTCPBackend>(modbus_host.c_str(), modbus_port);
}

PModbusServer TJSONConfigParser::_BuildPort(const string& name,
                                            const Json::Value& modbus_data,
                                            const Json::Value& registers,
                                            PMqttClient mqtt)
{
    LOG(Debug) << "Processing " << name;

    PModbusServer modbus = make_shared<TModbusServer>(_BuildBackend(modbus_data));

    bool any_enabled = false;

    // create observers and link'em with MQTT and Modbus
    try {
        any_enabled |= _BuildStore(COIL, registers["coils"], modbus, mqtt);
        any_enabled |= _BuildStore(DISCRETE_INPUT, registers["discretes"], modbus, mqtt);
        any_enabled |= _BuildStore(HOLDING_REGISTER, registers["holdings"], modbus, mqtt);
        any_enabled |= _BuildStore(INPUT_REGISTER, registers["inputs"], modbus, mqtt);
    } catch (const TConfigException& e) {
        throw TConfigException(name + ": " + e.what());
    }

    return any_enabled ? modbus : nullptr;
}

bool TJSONConfigParser::_BuildStore(TStoreType type, const Json::Value& list, PModbusServer modbus, PMqttClient mqtt)
{
    LOG(Debug) << "Processing store " << type;

    bool enabled = false;

    for (const auto& reg_item: list) {
        if (!reg_item["enabled"].asBool()) {
            continue;
        }
        enabled = true;

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
                // old version config may contain varchar registers with negative size value (-1)
                // set this registers size to 1
                if (size < 0)
                    size = 1;
                conv = make_shared<TMQTTTextConverter>(size, byteswap, wordswap);
            } else if (format == "float") {
                double scale = reg_item["scale"].asFloat();
                conv = make_shared<TMQTTFloatConverter>(size, byteswap, wordswap, scale);
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
                    throw TConfigException("Unknown integer format: " + format);
                }

                conv = make_shared<TMQTTIntConverter>(int_type, scale, size, byteswap, wordswap);
                size /= 2;
            }
        }

        obs = make_shared<TGatewayObserver>(expandTopic(topic), conv, mqtt);

        LOG(Debug) << "Creating observer on " << address << ":" << size;

        try {
            modbus->Observe(obs, type, TModbusAddressRange(address, size), slave_id);
        } catch (const WrongSegmentException& e) {
            throw TConfigException(string("Address overlapping: ") + StoreTypeToString(type) + ": topic " + topic);
        }
    }
    return enabled;
}
