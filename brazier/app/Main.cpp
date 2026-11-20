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

#include "../include/brazier/Core"
#include "../include/brazier/Http"
#include "../include/brazier/Engine.hpp"
#include "BenchmarkController.hpp"

using namespace brazier;

int main() {
    setlocale(LC_ALL, ".ACP");

    try {
        auto benchmark_controller = std::make_shared<BenchmarkController>();

        R(GET, "/test", benchmark_controller, test);

        brazier::ConfigManager::initGlobal("config_test.json");
        brazier::ConfigManager::getGlobal().setAutoSave(false);
        std::string https_server_host = brazier::ConfigManager::getGlobal().get("https_server.host", "0.0.0.0");
        int https_server_port = brazier::ConfigManager::getGlobal().get("https_server.port", 8443);
        
        brazier::Logger::log("HTTPS server: " + https_server_host + ":" + std::to_string(https_server_port), "INFO");

        brazier::HttpsServer::TlsConfig tls;
        tls.cert_file = "app/certs/server.crt";
        tls.key_file = "app/certs/server.key";
        brazier::HttpsServer https_server(https_server_host, https_server_port, tls);

        if (!https_server.initialize()) 
            return 1;
        
        auto https_thread = std::thread([&https_server] {
            https_server.run();
            std::string line;

            while (std::getline(std::cin, line)) {
                if (line == "reload") {
                    https_server.reloadTls();
                }
                else if (line == "quit") {
                    https_server.stop();
                    break;
                }
            }            
            });

        std::string server_host = brazier::ConfigManager::getGlobal().get("server.host", "0.0.0.0");
        int server_port = brazier::ConfigManager::getGlobal().get("server.port", 3502);

        brazier::Server http_server(server_host, server_port);

        brazier::ConfigManager::initGlobal("config_test.json");
        brazier::ConfigManager::getGlobal().setAutoSave(false);

        brazier::Logger::log("HTTP server: " + server_host + ":" + std::to_string(server_port), "INFO");

        if (!http_server.initialize()) 
            return 1;

        auto http_thread = std::thread([&http_server] {
            http_server.run();
            std::string line;

            while (std::getline(std::cin, line)) {
                if (line == "quit") {
                    http_server.stop();
                    break;
                }
            }
            });

        http_thread.join();

        return 0;
    
    }
    catch (const std::exception& e) {
        brazier::Logger::log(std::string(e.what()), "ERROR");
        return 1;
    }
}
