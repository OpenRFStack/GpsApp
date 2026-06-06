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
#include <string>

struct Config {
    // GPSD
    std::string gpsd_host  = "localhost";
    int         gpsd_port  = 2947;

    // AMQP
    std::string broker_url      = "amqp://localhost:5672";
    std::string broker_user     = "";
    std::string broker_password = "";
    std::string topic           = "gps.location";

    // Logging
    std::string fix_log_path        = "/var/lib/sdr/gps/fixes.jsonl";
    int         publish_interval_ms = 1000;
    int         min_fix_mode        = 2;   // 2=2D, 3=3D
    std::string device_id           = "sdr-node";

    bool load(const char* path);
};
