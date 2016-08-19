#include "config_parser.h"

#include <string>
#include <memory>
#include <fstream>
#include <iostream>
#include <tuple>
#include <jsoncpp/json/json.h>
#include <unistd.h>
#include <time.h>

#include "mqtt_converters.h"
#include "modbus_lmb_backend.h"
#include <wbmqtt/mqtt_wrapper.h>
#include <wbmqtt/utils.h>
#include "observer.h"
#include "mqtt_fastwrapper.h"

#include <log4cpp/Category.hh>
#include <log4cpp/RollingFileAppender.hh>
#include <log4cpp/SyslogAppender.hh>
#include <log4cpp/OstreamAppender.hh>
#include <log4cpp/PatternLayout.hh>

#include "logging.h"

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


TJSONConfigParser::TJSONConfigParser(int argc, char *argv[])
{
    // parse input flags
    string config_file;
    int verbose_level = 0;
    int opt;
    
    while ((opt = getopt(argc, argv, "hc:v")) != -1) {
        switch (opt) {
        case 'c':
            config_file = optarg;
            break;
        case 'v':
            verbose_level++;
            break;
        case 'h':
        default:
            throw ConfigParserException(string("Usage: ") + argv[0] + " -c config_file_name [-v [-v ...]]");
        }
    }

    // parse JSON configuration file
    Json::Reader reader;

    std::ifstream fileStream(config_file);
    if (!fileStream.is_open())
        throw ConfigParserException("Can't open configuration file");

    if (!reader.parse(fileStream, Root, false))
        throw ConfigParserException(string("Can't parse input JSON file: ") 
                    + reader.getFormattedErrorMessages());

    // configure logging
    string logfile = getenv("MQTT_MBGATE_LOGFILE") ? 
                        getenv("MQTT_MBGATE_LOGFILE") : "/var/log/wirenboard/wb-mqtt-mbgate.log";

    size_t max_logsize = getenv("MQTT_MBGATE_MAX_LOGFILE_SIZE") ?
                        atoi(getenv("MQTT_MBGATE_MAX_LOGFILE_SIZE")) * 1024 * 1024 : 1 * 1024 * 1024;

    log4cpp::Category &log_root = log4cpp::Category::getRoot();
    long pid = getpid();

    log_layout = new log4cpp::PatternLayout;
    log_layout->setConversionPattern("%d{%Y-%m-%d %H:%M:%S.%l} %p: %m%n");

    log4cpp::Priority::PriorityLevel priority = log4cpp::Priority::WARN;

    if (verbose_level > 0) {

        switch (verbose_level) {
        case 1:
            priority = log4cpp::Priority::NOTICE;
            break;
        case 2:
            priority = log4cpp::Priority::INFO;
            break;
        case 3:
        default:
            priority = log4cpp::Priority::DEBUG;
        }
        
        appender = new log4cpp::OstreamAppender("cerr", &std::cerr);
    } else if (Root["debug"].asBool()) {
        priority = log4cpp::Priority::INFO;
        appender = new log4cpp::RollingFileAppender("default_log", logfile, max_logsize);
    } else {
        appender = new log4cpp::SyslogAppender("syslog", "wb-mqtt-mbgate[" + to_string(pid) + "]");
    }

    appender->setLayout(log_layout);
    log_root.addAppender(appender);
    log_root.setPriority(priority);
}

TJSONConfigParser::~TJSONConfigParser()
{
}

bool TJSONConfigParser::Debug()
{
    return Root["debug"].asBool();
}

tuple<PModbusServer, PMQTTClient> TJSONConfigParser::Build()
{
    // create Modbus server
    if (!Root["mqtt"]["host"])
        throw ConfigParserException("Modbus TCP server bind address is necessary but is not defined");
    if (!Root["mqtt"]["port"])
        throw ConfigParserException("Modbus TCP bind port is necessary but is not defined");

    string modbus_host = Root["modbus"]["host"].asString();
    int modbus_port = Root["modbus"]["port"].asInt();

    LOG(DEBUG) << "Modbus configuration: host " << modbus_host << ", port " << modbus_port;

    PModbusBackend modbusBackend = make_shared<TModbusTCPBackend>(modbus_host.c_str(), modbus_port);
    PModbusServer modbus = make_shared<TModbusServer>(modbusBackend);

    // create MQTT client
    if (!Root["mqtt"]["host"])
        throw ConfigParserException("MQTT server address is necessary but is not defined");
    if (!Root["mqtt"]["port"])
        throw ConfigParserException("MQTT server port is necessary but is not defined");

    string mqtt_host = Root["mqtt"]["host"].asString();
    int mqtt_port = Root["mqtt"]["port"].asInt();
    int mqtt_keepalive;
    if (!Root["mqtt"]["keepalive"]) {
        mqtt_keepalive = 60;
    } else {
        mqtt_keepalive = Root["mqtt"]["keepalive"].asInt();
    }

    LOG(DEBUG) << "MQTT configuration: host " << mqtt_host << ", port " << mqtt_port << ", keepalive " << mqtt_keepalive;

    TMQTTClient::TConfig mqtt_config;
    mqtt_config.Host = mqtt_host;
    mqtt_config.Port = mqtt_port;
    mqtt_config.Keepalive = mqtt_keepalive;
    mqtt_config.Id = string("mqtt-mbgate-") + to_string(time(NULL));

    PMQTTClient mqtt = make_shared<TMQTTClient>(mqtt_config);
    PMQTTFastObserver fobs = make_shared<TMQTTFastObserver>();

    mqtt->Observe(fobs);

    // create observers and link'em with MQTT and Modbus
    _BuildStore(COIL, Root["registers"]["coils"], modbus, mqtt, fobs);
    _BuildStore(DISCRETE_INPUT, Root["registers"]["discretes"], modbus, mqtt, fobs);
    _BuildStore(HOLDING_REGISTER, Root["registers"]["holdings"], modbus, mqtt, fobs);
    _BuildStore(INPUT_REGISTER, Root["registers"]["inputs"], modbus, mqtt, fobs);

    // append logger if necessary
    if (log4cpp::Category::getRoot().getPriority() >= log4cpp::Priority::INFO) {
        PMQTTLoggingObserver log_obs = make_shared<TMQTTLoggingObserver>();
        mqtt->Observe(log_obs);
    }

    return make_tuple(modbus, mqtt);
}

void TJSONConfigParser::_BuildStore(TStoreType type, Json::Value &list, PModbusServer modbus, PMQTTClient mqtt, PMQTTFastObserver fobs)
{
    LOG(DEBUG) << "Processing store " << type;

    for (const auto &reg_item : list) {
        if (!reg_item["enabled"].asBool())
            continue;

        int address = reg_item["address"].asInt();
        int slave_id = reg_item["unitId"].asInt();
        string topic = reg_item["topic"].asString();
        int size;

        LOG(DEBUG) << "Element " << topic << " : " << address;
        
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

        LOG(DEBUG) << "Creating observer on " << address << ":" << size;

        try {
            modbus->Observe(obs, type, TModbusAddressRange(address, size), slave_id);
            fobs->Observe(obs, expandTopic(topic));
        } catch (const WrongSegmentException &e) {
            throw ConfigParserException(string("Address overlapping: ") + StoreTypeToString(type) + ": topic " + topic);
        }
    }
}
