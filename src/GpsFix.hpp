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
#include <cmath>

struct GpsFix {
    std::string timestamp_utc;
    int    mode        = 0;     // 0=no fix, 2=2D, 3=3D
    double latitude    = NAN;
    double longitude   = NAN;
    double altitude_m  = NAN;
    double speed_mps   = NAN;
    double heading_deg = NAN;
    double climb_mps   = NAN;
    double hdop        = NAN;
    double vdop        = NAN;
    int    satellites  = 0;
    std::string device;

    bool valid() const { return mode >= 2 && !std::isnan(latitude) && !std::isnan(longitude); }
};
