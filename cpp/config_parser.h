/*!\file config_parser.h
 * \brief JSON config file parsing tool
 * \author Nikita webconn Maslov <n.maslov@contactless.ru>
 */

#include "mqtt_converters.h"
#include <wbmqtt/mqtt_wrapper.h>
#include "modbus_wrapper.h"

#include <jsoncpp/json/json.h>
#include <string>
#include <exception>
#include <memory>
#include <tuple>

/*! Config parser exception */
class ConfigParserException : public std::exception
{
    std::string msg;
public:
    ConfigParserException(const std::string &_msg) : msg(_msg) {}
    virtual const char *what() const throw()
    {
        return msg.c_str();
    }
};

/*! Interface of config file parser
 * Takes configuration file, builds all observers
 * and connects it between Modbus and MQTT
 */
class IConfigParser
{
public:
    /*! Create gateway structure and get actual Modbus server and MQTT client */
    virtual std::tuple<PModbusServer, PMQTTClient> Build() = 0;

    /*! Check if debug logging is required */
    virtual bool Debug() = 0;

    /*! Virtual destructor */
    virtual ~IConfigParser();
};

typedef std::shared_ptr<IConfigParser> PConfigParser;

/*! JSON config file parser */
class TJSONConfigParser : public IConfigParser
{
public:
    TJSONConfigParser(const std::string &filename);

    virtual std::tuple<PModbusServer, PMQTTClient> Build();
    virtual bool Debug();

private:
    void _BuildStore(TStoreType type, Json::Value &list, PModbusServer modbus, PMQTTClient mqtt);

protected:
    Json::Value Root;
};
