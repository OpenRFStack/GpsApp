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
#include <cerrno>
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

    // 1-second receive timeout so readline() can periodically check `running`
    // even when gpsd stops sending (GPS removed, connection stalls).
    timeval tv{1, 0};
    ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

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

// Returns true on a complete line, false on error or timeout.
// Sets *timed_out=true when SO_RCVTIMEO or EINTR fired (no reconnect needed);
// *timed_out=false on a real read error or EOF (connection lost, reconnect).
bool GpsdClient::readline(std::string& out, bool* timed_out) {
    if (timed_out) *timed_out = false;
    out.clear();
    char c;
    while (true) {
        ssize_t n = ::read(fd_, &c, 1);
        if (n > 0) {
            if (c == '\n') return true;
            out += c;
        } else if (n == 0) {
            return false; // EOF — connection closed
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                if (timed_out) *timed_out = true;
                return false; // SO_RCVTIMEO or signal — caller re-checks running
            }
            return false; // real read error
        }
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
                for (int i = 0; i < 50 && running; ++i)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
        }

        std::string line;
        bool timed_out = false;
        if (!readline(line, &timed_out)) {
            if (!running) break; // shutdown requested during SO_RCVTIMEO wait
            if (timed_out) continue; // just a 1s poll tick — no reconnect needed
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
