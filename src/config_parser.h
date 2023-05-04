/*!\file config_parser.h
 * \brief JSON config file parsing tool
 * \author Nikita webconn Maslov <n.maslov@contactless.ru>
 */

#include <tuple>

#include <wblib/json_utils.h>
#include <wblib/mqtt.h>

#include "modbus_wrapper.h"
#include "mqtt_converters.h"

/*! Interface of config file parser
 * Takes configuration file, builds all observers
 * and connects them between Modbus and MQTT
 */
class IConfigParser
{
public:
    /*! Create gateway structure and get actual Modbus server and MQTT client */
    virtual std::tuple<PModbusServer, WBMQTT::PMqttClient> Build() = 0;

    /*! Check if debug logging is required */
    virtual bool Debug() = 0;

    /*! Virtual destructor */
    virtual ~IConfigParser();
};

/*! JSON config file parser */
class TJSONConfigParser: public IConfigParser
{
public:
    TJSONConfigParser(const std::string& config_file, const std::string& schema_file);
    ~TJSONConfigParser() = default;

    virtual std::tuple<PModbusServer, WBMQTT::PMqttClient> Build();
    virtual bool Debug();

private:
    void _BuildStore(TStoreType type, const Json::Value& list, PModbusServer modbus, WBMQTT::PMqttClient mqtt);

protected:
    Json::Value Root;
};
