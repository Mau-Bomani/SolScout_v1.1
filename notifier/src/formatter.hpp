
#pragma once

#include "types.hpp"
#include <string>

class Formatter {
public:
    static std::string format_alert_message(const InboundAlert& alert);
};
