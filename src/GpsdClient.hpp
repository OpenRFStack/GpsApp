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
#include <functional>
#include <atomic>
#include <string>

using FixCallback = std::function<void(const GpsFix&)>;

class GpsdClient {
public:
    explicit GpsdClient(const Config& cfg);
    ~GpsdClient();

    // Blocking: connects to gpsd and calls cb for each valid fix until running is false.
    void run(const std::atomic<bool>& running, FixCallback cb);

private:
    bool connect();
    void disconnect();
    bool readline(std::string& out, bool* timed_out = nullptr);

    const Config& cfg_;
    int           fd_ = -1;
};
