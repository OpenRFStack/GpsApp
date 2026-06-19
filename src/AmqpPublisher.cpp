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
#include "AmqpPublisher.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <cmath>

using json = nlohmann::json;

AmqpPublisher::AmqpPublisher(const Config& cfg) : cfg_(cfg) {}

AmqpPublisher::~AmqpPublisher() { stop(); }

void AmqpPublisher::start() {
    container_ = new proton::container(*this);
    thread_    = std::thread([this] { container_->run(); });
}

void AmqpPublisher::stop() {
    stopping_ = true;
    if (container_) {
        if (work_queue_)
            work_queue_->add([this] { sender_.connection().close(); });
        else
            // Connection never reached on_sender_open, so there's no work
            // queue to post a close through — stop the reactor directly so
            // thread_.join() below can't block forever (matters if/when
            // reconnect_options with infinite retries is ever added here,
            // as happened in AcquisitionApp's AmqpPublisher/TaskAmqpChannel).
            container_->stop();
        if (thread_.joinable()) thread_.join();
        delete container_;
        container_ = nullptr;
    }
}

void AmqpPublisher::publish(const GpsFix& fix, const std::string& device_id) {
    if (!work_queue_ || stopping_) return;

    json j;
    j["schema"]        = "GPS_FIX";
    j["version"]       = "1.0";
    j["device_id"]     = device_id;
    j["timestamp_utc"] = fix.timestamp_utc;
    j["fix_mode"]      = fix.mode;
    j["latitude_deg"]  = fix.latitude;
    j["longitude_deg"] = fix.longitude;
    if (!std::isnan(fix.altitude_m))  j["altitude_m"]  = fix.altitude_m;
    if (!std::isnan(fix.speed_mps))   j["speed_mps"]   = fix.speed_mps;
    if (!std::isnan(fix.heading_deg)) j["heading_deg"]  = fix.heading_deg;
    if (!std::isnan(fix.climb_mps))   j["climb_mps"]   = fix.climb_mps;
    if (!std::isnan(fix.hdop))        j["hdop"]        = fix.hdop;
    if (fix.satellites > 0)           j["satellites"]  = fix.satellites;
    if (!fix.device.empty())          j["gps_device"]  = fix.device;

    proton::message msg;
    msg.body(j.dump());
    msg.content_type("application/json");

    work_queue_->add([this, msg]() mutable {
        if (sender_ && sender_.credit() > 0)
            sender_.send(msg);
    });
}

void AmqpPublisher::on_container_start(proton::container& c) {
    proton::connection_options opts;
    if (!cfg_.broker_user.empty()) {
        opts.sasl_allowed_mechs("PLAIN");
        opts.sasl_allow_insecure_mechs(true);
        opts.user(cfg_.broker_user).password(cfg_.broker_password);
    } else {
        opts.sasl_allowed_mechs("ANONYMOUS");
    }
    c.connect(cfg_.broker_url, opts);
}

void AmqpPublisher::on_connection_open(proton::connection& conn) {
    conn.open_sender(cfg_.topic);
}

void AmqpPublisher::on_sender_open(proton::sender& s) {
    sender_     = s;
    work_queue_ = &s.work_queue();
    spdlog::info("[AmqpPublisher] connected → {}", cfg_.topic);
}

void AmqpPublisher::on_transport_error(proton::transport& t) {
    spdlog::warn("[AmqpPublisher] transport error: {}", t.error().description());
}

void AmqpPublisher::on_error(const proton::error_condition& e) {
    spdlog::error("[AmqpPublisher] error: {}", e.description());
}
