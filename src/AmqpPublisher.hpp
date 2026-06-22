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
#include <proton/container.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/connection.hpp>
#include <proton/connection_options.hpp>
#include <proton/reconnect_options.hpp>
#include <proton/sender.hpp>
#include <proton/transport.hpp>
#include <proton/work_queue.hpp>
#include <atomic>
#include <thread>
#include <string>

class AmqpPublisher : public proton::messaging_handler {
public:
    explicit AmqpPublisher(const Config& cfg);
    ~AmqpPublisher() override;

    void start();
    void stop();
    void publish(const GpsFix& fix, const std::string& device_id);

    void on_container_start(proton::container&) override;
    void on_connection_open(proton::connection&) override;
    void on_sender_open(proton::sender&) override;
    void on_transport_error(proton::transport&) override;
    void on_error(const proton::error_condition&) override;

private:
    const Config&         cfg_;
    proton::container*    container_  = nullptr;
    proton::sender        sender_;
    proton::work_queue*   work_queue_ = nullptr;
    std::thread           thread_;
    std::atomic<bool>     stopping_{false};
};
