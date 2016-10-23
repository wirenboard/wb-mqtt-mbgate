#include <iostream>
#include <memory>
#include <string>

#include <signal.h>
#include <getopt.h>
#include <cstring>

/* for version message */
#define WBMQTT_NAME "wb-mqtt-mbgate"
#define WBMQTT_COPYRIGHT "2016 Contactless Devices, LLC"
#define WBMQTT_WRITERS "Nikita webconn Maslov <n.maslov@contactless.ru>"
#include <wbmqtt/version.h>

#include "modbus_wrapper.h"
#include "modbus_lmb_backend.h"
#include "observer.h"

#include <wbmqtt/mqtt_wrapper.h>

#include <thread>
#include <tuple>

#include "config_parser.h"

using namespace std;

class TDummyModbusServerObserver : public IModbusServerObserver
{
public:
    ~TDummyModbusServerObserver() {}

    virtual TReplyState OnSetValue(TStoreType type, uint8_t unit_id, uint16_t start, unsigned count, const void *data)
    {
        cout << "Set value on slave id " << unit_id << ": [" << start << ":" << start + count << ")" << endl;
        return REPLY_OK;
    }
    
    virtual TReplyState OnGetValue(TStoreType type, uint8_t unit_id, uint16_t start, unsigned count, void *data)
    {
        cout << "Get value on slave id " << unit_id << ": [" << start << ":" << start + count << ")" << endl;
        return REPLY_OK;
    }
};

class ModbusGatewayBuilder
{
public:
    static tuple<PModbusServer, PMQTTClient> fromConfig(PConfigParser parser)
    {
        PModbusServer modbus;
        PMQTTClient client;

        tie(modbus, client) = parser->Build();

        modbus->AllocateCache();
        modbus->Backend()->Listen();
        
        client->ConnectAsync();

        return make_tuple(modbus, client);
    }

    static PModbusServer ModbusDummy(const char *host = "127.0.0.1", int port = 502)
    {
        // create libmodbus context
        PModbusBackend backend = make_shared<TModbusTCPBackend>(host, port);

        // create server and drop context into it
        PModbusServer server = make_shared<TModbusServer>(backend);

        // create dummy observer
        PModbusServerObserver obs = make_shared<TDummyModbusServerObserver>();
        TModbusAddressRange range(0, 10);
        server->Observe(obs, COIL, range);
        server->Observe(obs, HOLDING_REGISTER, range);

        server->AllocateCache();

        backend->Listen();

        return server;
    }

    static tuple<PModbusServer, PMQTTClient> ModbusMQTTTest(const char *host = "127.0.0.1", int port = 502)
    {
        // create libmodbus context
        PModbusBackend backend;// = make_shared<TModbusTCPBackend>(host, port);

        // create Modbus server
        PModbusServer server;// = make_shared<TModbusServer>(backend);

        // create MQTT client
        TMQTTClient::TConfig mqtt_config;
        mqtt_config.Port = 1883;
        mqtt_config.Host = "wirenboard-sgqye5nt.local";
        PMQTTClient mqtt_client = make_shared<TMQTTClient>(mqtt_config);

        // create observers
        PGatewayObserver ext_observer = make_shared<TGatewayObserver>("wb-ms-thls-v2_32/External Sensor 1", make_shared<TMQTTIntConverter>(TMQTTIntConverter::BCD, 10, 4), mqtt_client);
        /* server->Observe(ext_observer, HOLDING_REGISTER, TModbusAddressRange(0, 2)); */

        mqtt_client->Observe(ext_observer);

        /* server->AllocateCache(); */
        
        /* backend->Listen(); */
        mqtt_client->ConnectAsync();

        return make_tuple(server, mqtt_client);
    }
};

bool running = true;

void sighndlr(int signal)
{
    if (signal == SIGINT)
        LOG(NOTICE) << "SIGINT caught";
    else
        LOG(NOTICE) << "SIGTERM caught";

    running = false;
}

void set_sighandler()
{
    struct sigaction act;
    memset(&act, 0, sizeof (act));
    act.sa_handler = sighndlr;

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    act.sa_mask = set;

    sigaction(SIGINT, &act, 0);
    sigaction(SIGTERM, &act, 0);
}


/* describe common arguments using getopt() long format */
static const struct option long_options[] = {
    { "version", 0, NULL, 'v' },
};

void parse_common_args(int argc, char *argv[])
{
    while (1) {
        int c = getopt_long(argc, argv, "v", long_options, NULL);

        if (c == -1) {
            break;
        } else if (c == 'v') {
            cerr << VERSION_MESSAGE();
            exit(0);
        }
    }
}

int main(int argc, char *argv[])
{
    // parse --version etc.
    parse_common_args(argc, argv);


    set_sighandler();

    PModbusServer s;
    PMQTTClient t;

    try {
        tie(s, t) = ModbusGatewayBuilder::fromConfig(make_shared<TJSONConfigParser>(argc, argv));
    } catch (const ConfigParserException& e) {
        cerr << e.what() << endl;
        return 1;
    }

    LOG(INFO) << "Start loops";

    t->StartLoop();

    while (running) {
        try {
            if (s->Loop() == -1)
                break;
        } catch (const ModbusException &e) {
            LOG(ERROR) << "Modbus loop error: " << e.what();
            break;
        }
    }

    LOG(INFO) << "Shutting down";

    t->StopLoop();

    return 0;
}
