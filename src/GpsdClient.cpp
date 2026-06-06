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
#include "GpsdClient.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <chrono>

using json = nlohmann::json;

GpsdClient::GpsdClient(const Config& cfg) : cfg_(cfg) {}

GpsdClient::~GpsdClient() { disconnect(); }

bool GpsdClient::connect() {
    addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::string port = std::to_string(cfg_.gpsd_port);
    if (getaddrinfo(cfg_.gpsd_host.c_str(), port.c_str(), &hints, &res) != 0) {
        spdlog::warn("[GpsdClient] getaddrinfo failed for {}:{}", cfg_.gpsd_host, cfg_.gpsd_port);
        return false;
    }

    fd_ = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd_ < 0) { freeaddrinfo(res); return false; }

    if (::connect(fd_, res->ai_addr, res->ai_addrlen) < 0) {
        freeaddrinfo(res);
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    freeaddrinfo(res);

    // Enable streaming JSON fixes
    const char* watch = "?WATCH={\"enable\":true,\"json\":true}\n";
    if (::write(fd_, watch, strlen(watch)) < 0) {
        disconnect();
        return false;
    }

    spdlog::info("[GpsdClient] connected to gpsd {}:{}", cfg_.gpsd_host, cfg_.gpsd_port);
    return true;
}

void GpsdClient::disconnect() {
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}

bool GpsdClient::readline(std::string& out) {
    out.clear();
    char c;
    while (true) {
        ssize_t n = ::read(fd_, &c, 1);
        if (n <= 0) return false;
        if (c == '\n') return true;
        out += c;
    }
}

static GpsFix parse_tpv(const json& j) {
    GpsFix fix;
    fix.mode = j.value("mode", 0);
    if (j.contains("time") && j["time"].is_string())
        fix.timestamp_utc = j["time"].get<std::string>();
    if (j.contains("lat") && j["lat"].is_number()) fix.latitude    = j["lat"];
    if (j.contains("lon") && j["lon"].is_number()) fix.longitude   = j["lon"];
    if (j.contains("alt") && j["alt"].is_number()) fix.altitude_m  = j["alt"];
    if (j.contains("speed") && j["speed"].is_number()) fix.speed_mps   = j["speed"];
    if (j.contains("track") && j["track"].is_number()) fix.heading_deg = j["track"];
    if (j.contains("climb") && j["climb"].is_number()) fix.climb_mps   = j["climb"];
    if (j.contains("hdop")  && j["hdop"].is_number())  fix.hdop        = j["hdop"];
    if (j.contains("vdop")  && j["vdop"].is_number())  fix.vdop        = j["vdop"];
    if (j.contains("device") && j["device"].is_string()) fix.device    = j["device"];
    return fix;
}

static GpsFix parse_sky(const json& j, GpsFix fix) {
    if (j.contains("satellites") && j["satellites"].is_array())
        fix.satellites = static_cast<int>(j["satellites"].size());
    if (j.contains("hdop") && j["hdop"].is_number()) fix.hdop = j["hdop"];
    if (j.contains("vdop") && j["vdop"].is_number()) fix.vdop = j["vdop"];
    return fix;
}

void GpsdClient::run(const std::atomic<bool>& running, FixCallback cb) {
    GpsFix last_sky_info;

    while (running) {
        if (fd_ < 0) {
            spdlog::info("[GpsdClient] connecting to gpsd …");
            if (!connect()) {
                spdlog::warn("[GpsdClient] connection failed — retry in 5s");
                std::this_thread::sleep_for(std::chrono::seconds(5));
                continue;
            }
        }

        std::string line;
        if (!readline(line)) {
            spdlog::warn("[GpsdClient] connection lost — reconnecting");
            disconnect();
            continue;
        }

        if (line.empty()) continue;

        try {
            auto j = json::parse(line);
            std::string cls = j.value("class", "");

            if (cls == "SKY") {
                last_sky_info = parse_sky(j, last_sky_info);
            } else if (cls == "TPV") {
                GpsFix fix = parse_tpv(j);
                fix.satellites = last_sky_info.satellites;
                if (std::isnan(fix.hdop)) fix.hdop = last_sky_info.hdop;
                if (fix.valid() && fix.mode >= cfg_.min_fix_mode) {
                    cb(fix);
                }
            }
        } catch (const json::exception& e) {
            spdlog::debug("[GpsdClient] JSON parse error: {}", e.what());
        }
    }

    disconnect();
}
