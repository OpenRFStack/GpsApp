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
#include <tinyxml2.h>
#include <spdlog/spdlog.h>
#include <cstdlib>

using namespace tinyxml2;

static const char* text(XMLElement* el) {
    return el && el->GetText() ? el->GetText() : nullptr;
}

bool Config::load(const char* path) {
    XMLDocument doc;
    if (doc.LoadFile(path) != XML_SUCCESS) {
        spdlog::error("Config: failed to open {}: {}", path, doc.ErrorStr());
        return false;
    }

    auto* root = doc.FirstChildElement("gps_logger");
    if (!root) { spdlog::error("Config: missing <gps_logger> root"); return false; }

    if (auto* g = root->FirstChildElement("gpsd")) {
        if (const char* v = text(g->FirstChildElement("host")))    gpsd_host = v;
        if (auto* e = g->FirstChildElement("port"))                 gpsd_port = e->IntText(gpsd_port);
    }

    if (auto* b = root->FirstChildElement("broker")) {
        if (const char* v = text(b->FirstChildElement("url")))      broker_url = v;
        if (const char* v = text(b->FirstChildElement("username"))) broker_user = v;
        if (const char* v = text(b->FirstChildElement("password"))) broker_password = v;
        if (const char* v = text(b->FirstChildElement("topic")))    topic = v;
    }

    if (auto* l = root->FirstChildElement("logging")) {
        if (const char* v = text(l->FirstChildElement("fix_log")))         fix_log_path = v;
        if (auto* e = l->FirstChildElement("publish_interval_ms"))         publish_interval_ms = e->IntText(publish_interval_ms);
        if (auto* e = l->FirstChildElement("min_fix_mode"))                min_fix_mode = e->IntText(min_fix_mode);
        if (const char* v = text(l->FirstChildElement("device_id")))       device_id = v;
    }

    return true;
}
