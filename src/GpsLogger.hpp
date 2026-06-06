/*
========================================================================
Project: OpenRFStack
Author:  Brendan Michaud
Year:    2026
Part of OpenRFStack (https://github.com/OpenRFStack)

Licensed under the Personal Use License.
Do not use for commercial, organizational, or military purposes.
Contact author for permission: https://github.com/OpenRFStack
========================================================================
*/
#pragma once
#include "GpsFix.hpp"
#include "Config.hpp"
#include <fstream>
#include <mutex>

class GpsLogger {
public:
    explicit GpsLogger(const Config& cfg);
    void log(const GpsFix& fix);
private:
    std::ofstream file_;
    std::mutex    mu_;
};
