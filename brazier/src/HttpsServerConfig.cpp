/*
 * Copyright (c) 2026 Kirill Sergeev, Nikolay Sugonyako, Andrey Agarkov, Gleb Safyannikov
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This file is part of brazier.
 *
 * brazier is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * brazier is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with brazier; if not, see <https://www.gnu.org/licenses/>.
 */

#include "../include/brazier/HttpsServer.hpp"

int brazier::HttpsServer::compute_max_body_size(int ram_mb, int max_conn) {
    constexpr int kBodyRamBudgetPct = 25;
    constexpr int kMinBody = 64 * 1024;
    constexpr int kMaxBodyCap = 16 * 1024 * 1024;

    if (max_conn <= 0) max_conn = 1;

    const std::int64_t ram_bytes = (std::int64_t)(ram_mb) * 1024 * 1024;
    const std::int64_t budget = ram_bytes * kBodyRamBudgetPct / 100;
    std::int64_t per_conn = budget / (std::int64_t)(max_conn);

    if (per_conn < kMinBody)    per_conn = kMinBody;
    if (per_conn > kMaxBodyCap) per_conn = kMaxBodyCap;
    return static_cast<int>(per_conn);
}

int brazier::HttpsServer::compute_max_header_size(int ram_mb, int max_conn) {
    constexpr int kHeaderRamBudgetPct = 1;
    constexpr int kMinHeader = 4 * 1024;
    constexpr int kMaxHeaderCap = 32 * 1024;

    if (max_conn <= 0) max_conn = 1;

    const std::int64_t ram_bytes = (std::int64_t)(ram_mb) * 1024 * 1024;
    const std::int64_t budget = ram_bytes * kHeaderRamBudgetPct / 100;
    std::int64_t per_conn = budget / (std::int64_t)(max_conn);

    if (per_conn < kMinHeader)    per_conn = kMinHeader;
    if (per_conn > kMaxHeaderCap) per_conn = kMaxHeaderCap;
    return static_cast<int>(per_conn);
}

void brazier::HttpsServer::load_common_config_from_global() {
    keep_alive_timeout_ = std::chrono::seconds(
        global_config->get("protocol.keep_alive_timeout", 60));

    server_name_ = global_config->get("protocol.server_name",
        std::string("brazier"));

    hsts_enabled_ = global_config->get("https_server.hsts.enabled", true);
    hsts_header_ = global_config->get("https_server.hsts.header",
        std::string("max-age=31536000"));

    static_headers_.clear();
    static_headers_ += "Server: " + server_name_ + "\r\n";
    if (hsts_enabled_) {
        static_headers_ += "Strict-Transport-Security: " +
            hsts_header_ + "\r\n";
    }
}

void brazier::HttpsServer::load_tls_config_from_global() {
    if (tls_config_from_user_) return;

    tls_.cert_pem = global_config->get("https_server.tls.cert_pem",
        std::string(""));
    tls_.key_pem = global_config->get("https_server.tls.key_pem",
        std::string(""));

    tls_.cert_file = global_config->get("https_server.tls.cert_file",
        std::string("server.crt"));
    tls_.key_file = global_config->get("https_server.tls.key_file",
        std::string("server.key"));
    tls_.ca_file = global_config->get("https_server.tls.ca_file",
        std::string(""));

    tls_.require_client_cert =
        global_config->get("https_server.tls.require_client_cert", false);
    tls_.verify_client_cert =
        global_config->get("https_server.tls.verify_client_cert", false);

    tls_.handshake_timeout = std::chrono::seconds(
        global_config->get("https_server.tls.handshake_timeout", 15));

    try {
        json conf = global_config->getJson("https_server.tls.conf");
        if (conf.is_array()) {
            for (const auto& item : conf) {
                if (!item.is_array() || item.empty() || item.size() > 2) {
                    throw std::runtime_error(
                        "https_server.tls.conf: each entry must be [command] "
                        "or [command, value]");
                }
                std::string cmd = item[0].get<std::string>();
                std::string val = item.size() > 1
                    ? item[1].get<std::string>() : "";
                tls_.conf.emplace_back(std::move(cmd), std::move(val));
            }
        }
    }
    catch (const std::exception& e) {
        Logger::log("https_server.tls.conf not loaded: " +
            std::string(e.what()), "WARNING");
    }
}

void brazier::HttpsServer::load_limits_from_config() {
    const int ram_mb = platform::get_system_memory_mb();

    const int testing_conn =
        global_config->get("protocol.max_connections_testing", 0);
    const int testing_body =
        global_config->get("protocol.max_body_size_testing", 0);
    const int testing_hdr =
        global_config->get("protocol.max_header_size_testing", 0);

    const bool testing_mode =
        testing_conn > 0 || testing_body > 0 || testing_hdr > 0;

    if (testing_conn > 0) {
        max_connections_ = testing_conn;
    }
    else if (int v = global_config->get("protocol.max_connections", 0); v > 0) {
        max_connections_ = v;
    }
    else {
        const int fd_limit = platform::get_fd_limit();
        max_connections_ = fd_limit * 8 / 10;
    }

    if (testing_body > 0) {
        max_body_size_ = testing_body;
    }
    else if (int v = global_config->get("protocol.max_body_size", 0); v > 0) {
        max_body_size_ = v;
    }
    else {
        max_body_size_ = compute_max_body_size(ram_mb, max_connections_);
    }

    if (testing_hdr > 0) {
        max_header_size_ = testing_hdr;
    }
    else if (int v = global_config->get("protocol.max_header_size", 0); v > 0) {
        max_header_size_ = v;
    }
    else {
        max_header_size_ = compute_max_header_size(ram_mb, max_connections_);
    }

    Logger::log(
        std::string(testing_mode ? "[TESTING] " : "") +
        "Final HTTP limits: max_connections=" +
        std::to_string(max_connections_) +
        ", max_body=" + std::to_string(max_body_size_ / 1024) + "KB" +
        ", max_header=" + std::to_string(max_header_size_ / 1024) + "KB" +
        " (RAM=" + std::to_string(ram_mb) + "MB)",
        testing_mode ? "WARNING" : "INFO");
}

void brazier::HttpsServer::initializeConnections() {
    try {
        Queue::connect(global_config->get("nosql.host", "127.0.0.1"),
            global_config->get("nosql.port", 6379));
    }
    catch (const std::exception& e) {
        Logger::log("Connection to queue failed: " + std::string(e.what()),
            "ERROR");
    }

    try {
        Cache::connect(global_config->get("redis.host", "127.0.0.1"),
            global_config->get("redis.port", 6379));
    }
    catch (const std::exception& e) {
        Logger::log("Connection to NOSQL database failed: " +
            std::string(e.what()), "ERROR");
    }

}