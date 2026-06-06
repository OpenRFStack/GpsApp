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
#include "GpsLogger.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <cmath>

using json = nlohmann::json;

GpsLogger::GpsLogger(const Config& cfg) {
    std::filesystem::create_directories(
        std::filesystem::path(cfg.fix_log_path).parent_path());
    file_.open(cfg.fix_log_path, std::ios::app);
    if (!file_)
        spdlog::warn("[GpsLogger] cannot open log file: {}", cfg.fix_log_path);
}

void GpsLogger::log(const GpsFix& fix) {
    json j;
    j["schema"]        = "GPS_FIX";
    j["version"]       = "1.0";
    j["timestamp_utc"] = fix.timestamp_utc;
    j["fix_mode"]      = fix.mode;
    j["latitude_deg"]  = fix.latitude;
    j["longitude_deg"] = fix.longitude;
    if (!std::isnan(fix.altitude_m))  j["altitude_m"]  = fix.altitude_m;
    if (!std::isnan(fix.speed_mps))   j["speed_mps"]   = fix.speed_mps;
    if (!std::isnan(fix.heading_deg)) j["heading_deg"]  = fix.heading_deg;
    if (!std::isnan(fix.climb_mps))   j["climb_mps"]   = fix.climb_mps;
    if (!std::isnan(fix.hdop))        j["hdop"]        = fix.hdop;
    if (!std::isnan(fix.vdop))        j["vdop"]        = fix.vdop;
    if (fix.satellites > 0)           j["satellites"]  = fix.satellites;
    if (!fix.device.empty())          j["device"]      = fix.device;

    std::lock_guard<std::mutex> lk(mu_);
    if (file_) file_ << j.dump() << '\n';
}
