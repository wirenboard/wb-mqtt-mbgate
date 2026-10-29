#include "config_parser.h"
#include "fake_modbus_backend.h"
#include "mbgate_exception.h"
#include "mock_mqtt_client.h"
#include "modbus_lmb_backend.h"
#include "observer.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

using namespace std;
using namespace WBMQTT;
using namespace ::testing;

namespace
{
    const string SCHEMA_FILE = "wb-mqtt-mbgate.schema.json";

    string Holding(const string& topic, int unitId, int address)
    {
        return R"({"enabled": true, "topic": ")" + topic + R"(", "unitId": )" + to_string(unitId) + R"(, "address": )" +
               to_string(address) + R"(, "format": "signed", "size": 2, "scale": 1})";
    }

    string Registers(const string& holdings = "")
    {
        return R"({"discretes": [], "coils": [], "inputs": [], "holdings": [)" + holdings + "]}";
    }

    string Tcp(const string& host, int port)
    {
        return R"({"host": ")" + host + R"(", "port": )" + to_string(port) + "}";
    }

    string Rtu(const string& path)
    {
        return R"({"path": ")" + path + R"(", "baud_rate": 9600, "parity": "N", "data_bits": 8, "stop_bits": 1})";
    }

    string Port(const string& modbus, const string& registers, const string& name = "")
    {
        return R"({"name": ")" + name + R"(", "modbus": )" + modbus + R"(, "registers": )" + registers + "}";
    }

    string Config(const string& modbus, const string& registers, const vector<string>& additionalPorts = {})
    {
        string config = R"({"debug": false, "mqtt": {"host": "localhost", "port": 1883}, "modbus": )" + modbus +
                        R"(, "registers": )" + registers;
        if (!additionalPorts.empty()) {
            config += R"(, "additional_ports": [)";
            for (size_t i = 0; i < additionalPorts.size(); ++i) {
                config += (i ? ", " : "") + additionalPorts[i];
            }
            config += "]";
        }
        return config + "}";
    }
}

class ConfigParserTest: public Test
{
protected:
    filesystem::path ConfigFile;

    void SetUp() override
    {
        ConfigFile = filesystem::temp_directory_path() / ("mbgate-test-" + to_string(getpid()) + ".conf");
    }

    void TearDown() override
    {
        filesystem::remove(ConfigFile);
    }

    vector<PModbusServer> Build(const string& config)
    {
        ofstream(ConfigFile) << config;
        TJSONConfigParser parser(ConfigFile, SCHEMA_FILE);
        return get<0>(parser.Build());
    }
};

TEST_F(ConfigParserTest, ConfigWithoutAdditionalPorts)
{
    auto servers = Build(Config(Tcp("*", 502), Registers(Holding("dev/ctrl", 1, 0))));

    ASSERT_EQ(servers.size(), 1);
    EXPECT_NE(dynamic_pointer_cast<TModbusTCPBackend>(servers[0]->Backend()), nullptr);
    EXPECT_TRUE(servers[0]->IsObserved(1));
}

TEST_F(ConfigParserTest, AdditionalPortsGetOwnServers)
{
    auto servers = Build(Config(Tcp("*", 502),
                                Registers(Holding("dev/a", 1, 0)),
                                {Port(Tcp("*", 503), Registers(Holding("dev/b", 2, 0)), "SCADA"),
                                 Port(Rtu("/dev/ttyRS485-2"), Registers(Holding("dev/c", 3, 0)))}));

    ASSERT_EQ(servers.size(), 3);
    EXPECT_TRUE(servers[0]->IsObserved(1));
    EXPECT_FALSE(servers[0]->IsObserved(2));
    EXPECT_NE(dynamic_pointer_cast<TModbusTCPBackend>(servers[1]->Backend()), nullptr);
    EXPECT_TRUE(servers[1]->IsObserved(2));
    EXPECT_FALSE(servers[1]->IsObserved(1));
    EXPECT_NE(dynamic_pointer_cast<TModbusRTUBackend>(servers[2]->Backend()), nullptr);
    EXPECT_TRUE(servers[2]->IsObserved(3));
}

TEST_F(ConfigParserTest, PortWithoutEnabledChannelsIsSkipped)
{
    auto servers =
        Build(Config(Tcp("*", 502),
                     Registers(),
                     {Port(Tcp("*", 503), Registers(Holding("dev/b", 2, 0))), Port(Tcp("*", 504), Registers())}));

    ASSERT_EQ(servers.size(), 1);
    EXPECT_TRUE(servers[0]->IsObserved(2));
}

TEST_F(ConfigParserTest, AllPortsEmpty)
{
    EXPECT_THROW(Build(Config(Tcp("*", 502), Registers(), {Port(Tcp("*", 503), Registers())})), TEmptyConfigException);
}

TEST_F(ConfigParserTest, SameTcpPortOnAnyHostConflicts)
{
    EXPECT_THROW(Build(Config(Tcp("*", 502),
                              Registers(Holding("dev/a", 1, 0)),
                              {Port(Tcp("192.168.1.10", 502), Registers(Holding("dev/b", 1, 0)))})),
                 TConfigException);
}

TEST_F(ConfigParserTest, SameTcpPortOnDifferentHostsAllowed)
{
    auto servers = Build(Config(Tcp("10.0.0.1", 502),
                                Registers(Holding("dev/a", 1, 0)),
                                {Port(Tcp("10.0.0.2", 502), Registers(Holding("dev/b", 1, 0)))}));

    EXPECT_EQ(servers.size(), 2);
}

TEST_F(ConfigParserTest, SameSerialDeviceConflicts)
{
    EXPECT_THROW(Build(Config(Rtu("/dev/ttyRS485-1"),
                              Registers(Holding("dev/a", 1, 0)),
                              {Port(Rtu("/dev/ttyRS485-1"), Registers(Holding("dev/b", 1, 0)))})),
                 TConfigException);
}

TEST_F(ConfigParserTest, EmptyPortsConflictToo)
{
    EXPECT_THROW(Build(Config(Tcp("*", 502), Registers(Holding("dev/a", 1, 0)), {Port(Tcp("*", 502), Registers())})),
                 TConfigException);
}

TEST_F(ConfigParserTest, TcpAndRtuDoNotConflict)
{
    auto servers = Build(Config(Tcp("*", 502),
                                Registers(Holding("dev/a", 1, 0)),
                                {Port(Rtu("/dev/ttyRS485-1"), Registers(Holding("dev/b", 1, 0)))}));

    EXPECT_EQ(servers.size(), 2);
}

TEST_F(ConfigParserTest, SameTopicAndAddressOnDifferentPorts)
{
    auto servers = Build(Config(Tcp("*", 502),
                                Registers(Holding("dev/a", 1, 0)),
                                {Port(Tcp("*", 503), Registers(Holding("dev/a", 1, 0)))}));

    EXPECT_EQ(servers.size(), 2);
}

TEST_F(ConfigParserTest, OverlappingAddressesOnOnePortNamePort)
{
    try {
        Build(
            Config(Tcp("*", 502),
                   Registers(Holding("dev/a", 1, 0)),
                   {Port(Tcp("*", 503), Registers(Holding("dev/b", 1, 0) + ", " + Holding("dev/c", 1, 0)), "SCADA")}));
        FAIL() << "TConfigException expected";
    } catch (const TConfigException& e) {
        EXPECT_THAT(e.what(), HasSubstr("Additional port 1 (SCADA)"));
    }
}

TEST(MultiPortGatewayTest, SameTopicUpdatesCachesOfAllPorts)
{
    auto mqtt = make_shared<NiceMock<MockMQTTClient>>();
    vector<TMqttMessageHandler> handlers;
    ON_CALL(*mqtt, Subscribe(_, Matcher<const string&>(_)))
        .WillByDefault(Invoke([&](TMqttMessageHandler handler, const string&) { handlers.push_back(handler); }));

    vector<shared_ptr<TFakeModbusBackend>> backends;
    vector<PModbusServer> servers;
    for (int i = 0; i < 2; ++i) {
        backends.push_back(make_shared<TFakeModbusBackend>());
        servers.push_back(make_shared<TModbusServer>(backends.back()));
        auto observer = make_shared<TGatewayObserver>("/devices/dev/controls/a",
                                                      make_shared<TMQTTIntConverter>(TMQTTIntConverter::SIGNED, 1.0, 2),
                                                      mqtt);
        servers.back()->Observe(observer, HOLDING_REGISTER, TModbusAddressRange(i, 1), 1);
        servers.back()->AllocateCache();
    }

    ASSERT_EQ(handlers.size(), 2);
    for (auto& handler: handlers) {
        handler(TMqttMessage("/devices/dev/controls/a", "42", 0, false));
    }

    EXPECT_EQ(static_cast<uint16_t*>(backends[0]->GetCache(HOLDING_REGISTER, 1))[0], 42);
    EXPECT_EQ(static_cast<uint16_t*>(backends[1]->GetCache(HOLDING_REGISTER, 1))[1], 42);
}
