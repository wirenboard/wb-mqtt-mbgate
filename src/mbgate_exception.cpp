#include "mbgate_exception.h"
#include <string>

TConfigException::TConfigException(const std::string &message)
        : std::runtime_error("Configuration error: " + message) {}