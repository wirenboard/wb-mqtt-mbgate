#include <iostream>
#include <memory>
#include <string>

#include <unistd.h>

#include "modbus_wrapper.h"
#include "modbus_lmb_backend.h"

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
    static PModbusServer fromConfig()
    {
        return PModbusServer();
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
};

int main(int argc, char *argv[])
{
    int opt = 0;

    string hostname = "127.0.0.1";
    int port = 502;

    while ((opt = getopt(argc, argv, "hs:p:")) != -1) {
        switch (opt) {
        case 's':
            hostname = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 'h':
        default:
            cerr << "Usage: " << argv[0] << " -s hostname -p port" << endl;
            return 1;
        }
    }

    PModbusServer s = ModbusGatewayBuilder::ModbusDummy(hostname.c_str(), port);

    while (1) {
        s->Loop();
    }

    return 0;
}
