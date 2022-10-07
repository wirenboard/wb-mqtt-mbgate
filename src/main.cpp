#include <string>
#include <getopt.h>
#include <cstring>
#include <tuple>

#include "log.h"
#include "modbus_wrapper.h"
#include "modbus_lmb_backend.h"
#include "observer.h"
#include "config_parser.h"
#include "mbgate_exception.h"

#include <wblib/signal_handling.h>

/* for version message */
#define WBMQTT_NAME "wb-mqtt-mbgate"

#define LOG(logger) ::logger.Log() << "[mbgate] "

#define STR(x) #x
#define XSTR(x) STR(x)

#ifdef WBMQTT_DIRTYTREE
#define WBMQTT_DIRTYTREE_MSG "\033[31m\033[5mGIT TREE WAS DIRTY DURING THE BUILD!\033[0m\n"
#else
#define WBMQTT_DIRTYTREE_MSG ""
#endif

using namespace std;
using namespace WBMQTT;

const auto DRIVER_STOP_TIMEOUT_S = chrono::seconds(10);

namespace {
    constexpr auto EXIT_NOTCONFIGURED = 6;   // The program is not configured

    void PrintUsage()
    {
        cout << WBMQTT_NAME << XSTR(WBMQTT_VERSION) 
             << " git " << XSTR(WBMQTT_COMMIT) 
             << " build on " << __DATE__ << " " << __TIME__ << endl
             << WBMQTT_DIRTYTREE_MSG << endl
             << "Usage:" << endl
             << " " << WBMQTT_NAME << " [options]" << endl
             << "Options:" << endl
             << "  -d  level    enable debuging output:" << endl
             << "                 1 - " << WBMQTT_NAME << " only;" << endl
             << "                 2 - mqtt only;" << endl
             << "                 3 - both;" << endl
             << "                 negative values - silent mode (-1, -2, -3))" << endl
             << "  -c  config   config file (default /etc/wb-mqtt-mbgate.conf)" << endl;
    }

    void ParseCommadLine(int     argc,
                         char*   argv[],
                         string& configFile)
    {
        int debugLevel = 0;
        int c;

        while ((c = getopt(argc, argv, "d:c:")) != -1) {
            switch (c) {
            case 'd':
                debugLevel = stoi(optarg);
                break;
            case 'c':
                configFile = optarg;
                break;

            case '?':
            default:
                PrintUsage();
                exit(2);
            }
        }

        switch (debugLevel) {
        case 0:
            break;
        case -1:
            ::Info.SetEnabled(false);
            break;

        case -2:
            WBMQTT::Info.SetEnabled(false);
            break;

        case -3:
            WBMQTT::Info.SetEnabled(false);
            ::Info.SetEnabled(false);
            break;

        case 1:
            ::Debug.SetEnabled(true);
            break;

        case 2:
            WBMQTT::Debug.SetEnabled(true);
            break;

        case 3:
            WBMQTT::Debug.SetEnabled(true);
            ::Debug.SetEnabled(true);
            break;

        default:
            cout << "Invalid -d parameter value " << debugLevel << endl;
            PrintUsage();
            exit(2);
        }

        if (optind < argc) {
            for (int index = optind; index < argc; ++index) {
                cout << "Skipping unknown argument " << argv[index] << endl;
            }
        }
    }
}

class ModbusGatewayBuilder
{
public:
    static tuple<PModbusServer, PMqttClient> fromConfig(IConfigParser& parser)
    {
        PModbusServer modbus;
        PMqttClient client;

        tie(modbus, client) = parser.Build();

        modbus->AllocateCache();
        modbus->Backend()->Listen();
        
        return make_tuple(modbus, client);
    }
};

int main(int argc, char *argv[])
{
    string configFile("/etc/wb-mqtt-mbgate.conf");

    WBMQTT::SignalHandling::Handle({ SIGINT, SIGTERM });
    WBMQTT::SignalHandling::OnSignals({ SIGINT, SIGTERM }, [&]{ WBMQTT::SignalHandling::Stop(); });
    WBMQTT::SetThreadName(WBMQTT_NAME);

    ParseCommadLine(argc, argv, configFile);

    WBMQTT::SignalHandling::SetOnTimeout(DRIVER_STOP_TIMEOUT_S, [&]{
        LOG(Error) << "Driver takes too long to stop. Exiting.";
        exit(1);
    });

    bool running = true;

    WBMQTT::SignalHandling::OnSignals( { SIGINT, SIGTERM }, [&]{ running = false; });

    try {
        PModbusServer s;
        PMqttClient t;
        TJSONConfigParser configParser(configFile, "/usr/share/wb-mqtt-confed/schemas/wb-mqtt-mbgate.schema.json");
        if (configParser.Debug())
            ::Debug.SetEnabled(true);

        tie(s, t) = ModbusGatewayBuilder::fromConfig(configParser);

        LOG(Info) << "Start loops";

        WBMQTT::SignalHandling::Start();

        t->Start();

        while (running) {
            if (s->Loop(1000) == -1)
                    break;
        }

        LOG(Info) << "Shutting down";

        t->Stop();
        WBMQTT::SignalHandling::Wait();
    } catch (const TConfigException &e) {
        LOG(Error) << "FATAL: " << e.what();
        return EXIT_NOTCONFIGURED;
    } catch (const exception &e) {
        LOG(Error) << "FATAL: " << e.what();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
