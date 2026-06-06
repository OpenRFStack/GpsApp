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
#include "Config.hpp"
#include "GpsdClient.hpp"
#include "GpsLogger.hpp"
#include "AmqpPublisher.hpp"
#include <spdlog/spdlog.h>
#include <csignal>
#include <atomic>
#include <chrono>

static std::atomic<bool> g_running{true};

static void sig_handler(int) { g_running = false; }

int main(int argc, char* argv[]) {
    if (argc < 2) {
        spdlog::error("Usage: {} <config.xml>", argv[0]);
        return 1;
    }

    Config cfg;
    if (!cfg.load(argv[1])) return 1;

    spdlog::info("sdr_gps v1.0 — gpsd={}:{} broker={} topic={}",
                 cfg.gpsd_host, cfg.gpsd_port, cfg.broker_url, cfg.topic);

    std::signal(SIGINT,  sig_handler);
    std::signal(SIGTERM, sig_handler);

    GpsLogger    logger(cfg);
    AmqpPublisher publisher(cfg);
    publisher.start();

    auto last_publish = std::chrono::steady_clock::now() - std::chrono::seconds(60);

    GpsdClient client(cfg);
    client.run(g_running, [&](const GpsFix& fix) {
        logger.log(fix);

        auto now = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_publish).count();

        if (elapsed_ms >= cfg.publish_interval_ms) {
            publisher.publish(fix, cfg.device_id);
            last_publish = now;
            spdlog::info("[GPS] {:.6f},{:.6f} alt={:.1f}m spd={:.1f}m/s hdg={:.0f}° sats={} mode={}",
                         fix.latitude, fix.longitude, fix.altitude_m,
                         fix.speed_mps, fix.heading_deg, fix.satellites, fix.mode);
        }
    });

    publisher.stop();
    spdlog::info("sdr_gps exiting");
    return 0;
}
