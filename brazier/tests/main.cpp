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

#include "main.h"

constexpr bool kStartHttpServer = true;
constexpr bool kStartHttpsServer = false;

std::shared_ptr<brazier::Server>      g_test_server;
std::shared_ptr<brazier::HttpsServer> g_test_https_server;
std::atomic<bool> g_server_ready{ false };
std::atomic<bool> g_https_server_ready{ false };
std::thread g_server_thread;
std::thread g_https_server_thread;

int port_global;
std::string host_global;
int https_port_global;
std::string https_host_global;

namespace {

    std::string normalizeHost(const std::string& host) {
        return (host == "0.0.0.0") ? "127.0.0.1" : host;
    }

    bool WaitForServer(const std::string& host, int port, int max_attempts = 30) {
        boost::asio::io_context io;
        boost::asio::ip::tcp::socket socket(io);
        boost::asio::ip::tcp::endpoint endpoint(
            boost::asio::ip::make_address(host), port);

        for (int i = 0; i < max_attempts; ++i) {
            boost::system::error_code ec;
            socket.connect(endpoint, ec);
            if (!ec) {
                socket.close();
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        return false;
    }

} 

int main(int argc, char** argv) {
    try {
        testing::InitGoogleTest(&argc, argv);
        brazier::ConfigManager::initGlobal("config_test.json");
        brazier::global_config->setAutoSave(false);

        host_global = normalizeHost(
            brazier::global_config->get("server.host", std::string("127.0.0.1")));
        port_global = brazier::global_config->get<int>("server.port", 3502);

        https_host_global = normalizeHost(
            brazier::global_config->get("https_server.host", std::string("127.0.0.1")));
        https_port_global = brazier::global_config->get<int>("https_server.port", 8443);

        if (kStartHttpServer) {
            g_test_server = std::make_shared<brazier::Server>(host_global, port_global);
            g_server_thread = std::thread([]() {
                try {
                    if (!g_test_server->initialize()) {
                        brazier::Logger::log("Failed to init HTTP test server", "ERROR");
                        return;
                    }
                    g_server_ready = true;
                    brazier::Logger::log("HTTP test server initialized", "INFO");
                    g_test_server->run();
                }
                catch (const std::exception& e) {
                    brazier::Logger::log("HTTP test server error: " +
                        std::string(e.what()), "ERROR");
                }
                });

            if (!WaitForServer(host_global, port_global)) {
                brazier::Logger::log("HTTP server failed to start within timeout", "ERROR");
            }
        }

        if (kStartHttpsServer) {
            g_test_https_server = std::make_shared<brazier::HttpsServer>(
                https_host_global, https_port_global);
            g_https_server_thread = std::thread([]() {
                try {
                    if (!g_test_https_server->initialize()) {
                        brazier::Logger::log("Failed to init HTTPS test server", "ERROR");
                        return;
                    }
                    g_https_server_ready = true;
                    brazier::Logger::log("HTTPS test server initialized", "INFO");
                    g_test_https_server->run();
                }
                catch (const std::exception& e) {
                    brazier::Logger::log("HTTPS test server error: " +
                        std::string(e.what()), "ERROR");
                }
                });

            if (!WaitForServer(https_host_global, https_port_global)) {
                brazier::Logger::log("HTTPS server failed to start within timeout", "ERROR");
            }
        }

        int result = RUN_ALL_TESTS();

        if (g_test_server)       g_test_server->stop();
        if (g_test_https_server) g_test_https_server->stop();
        if (g_server_thread.joinable())       g_server_thread.join();
        if (g_https_server_thread.joinable()) g_https_server_thread.join();

        return result;
    }
    catch (const std::exception& e) {
        brazier::Logger::log("Exception: " + std::string(e.what()), "ERROR");
        return -1;
    }
}