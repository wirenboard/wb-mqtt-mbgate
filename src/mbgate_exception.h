#pragma once

#include <exception>

class TConfigException : public std::runtime_error {
public:
    explicit TConfigException(const std::string &message);
};