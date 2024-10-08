#include "mbgate_exception.h"

TConfigException::TConfigException(const std::string& message): std::runtime_error("Configuration error: " + message)
{}

TEmptyConfigException::TEmptyConfigException(): std::runtime_error("empty configuration")
{}
