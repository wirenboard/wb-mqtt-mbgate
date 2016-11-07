/*!\file config_parser.h
 * \brief JSON config file parsing tool
 * \author Nikita webconn Maslov <n.maslov@contactless.ru>
 */

#include "mqtt_converters.h"
#include <wbmqtt/mqtt_wrapper.h>
#include "modbus_wrapper.h"
#include "mqtt_fastwrapper.h"

#include <jsoncpp/json/json.h>
#include <string>
#include <exception>
#include <memory>
#include <tuple>

#include <log4cpp/Appender.hh>
#include <log4cpp/PatternLayout.hh>

/*! Default log file if MQTT_MBGATE_LOGFILE environment variable is not set */
#define DEFAULT_LOG_FILE "/var/log/wirenboard/wb-mqtt-mbgate.log"

/*! Default log file maximum size in megabytes if MQTT_MBGATE_MAX_LOGFILE_SIZE 
 * environment variable is not set 
 */
#define DEFAULT_LOG_FILE_SIZE 1


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
    virtual ~ConfigParserException() throw() {}
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
    TJSONConfigParser(int argc, char *argv[]);
    ~TJSONConfigParser();

    virtual std::tuple<PModbusServer, PMQTTClient> Build();
    virtual bool Debug();

private:
    void _BuildStore(TStoreType type, Json::Value &list, PModbusServer modbus, PMQTTClient mqtt, PMQTTFastObserver obs);

protected:
    Json::Value Root;

    log4cpp::PatternLayout *log_layout;
    log4cpp::Appender *appender;
};
