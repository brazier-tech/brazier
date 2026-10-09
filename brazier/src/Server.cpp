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

#include "../include/brazier/server.hpp"

brazier::Server::Server(const std::string& host, unsigned short port)
    : acceptor_(io_, tcp::endpoint(net::ip::make_address(host), port))
    , port_(port), host_(host) {
    acceptor_.set_option(tcp::acceptor::reuse_address(true));
    work_guard_ = std::make_unique<net::executor_work_guard<net::io_context::executor_type>>(io_.get_executor());
}

bool brazier::Server::initialize() {
    try {
        Logger::init("debug.log");
        Logger::registerSignalHandlers();

        json drivers = global_config->getJson("filesystem.drivers");

        for (auto& [name, cfg] : drivers.items()) {
            if (name == "default") continue;

            auto driver = std::make_shared<brazier::FileDriver>();
            driver->setRootPath(cfg.value("root", "./"));
            driver->initAsync();

            StorageManager::getInstance().registerDriver(name, driver);
        }

        std::string def = global_config->getNested<std::string>("filesystem.default", "local");
        if (StorageManager::getInstance().hasDriver(def)) {
            StorageManager::getInstance().setDefaultDriver(def);
        }

        initializeConnections();
        RouterRegisterer::init(io_);

        Logger::log("Server initialized on " + host_ + ":" + std::to_string(port_), "SUCCESS");
        return true;
    }
    catch (const std::exception& e) {
        Logger::log("Initialization failed: " + std::string(e.what()), "ERROR");
        return false;
    }
}

void brazier::Server::run() {
    try {
        net::co_spawn(io_, accept_loop(), net::detached);

        int threads_count = std::thread::hardware_concurrency();
        if (threads_count == 0) threads_count = 1;

        Logger::log("Starting " + std::to_string(threads_count) + " worker threads", "INFO");

        for (int i = 0; i < threads_count; ++i) {
            threads_.emplace_back([this] {
                brazier::Engine::init(io_);
                io_.run();
                });
        }

        shutdown_flag_.store(false, std::memory_order_release);
        stats_thread_ = std::thread([this] {
            while (!shutdown_flag_.load(std::memory_order_acquire)) {
                std::this_thread::sleep_for(10s);
                if (shutdown_flag_.load(std::memory_order_acquire)) break;
                Logger::log("STATS - Active connections: " + std::to_string(connection_count_.load()) +
                    ", Total requests: " + std::to_string(total_requests_.load()), "INFO");
            }
            });

        for (auto& t : threads_) {
            if (t.joinable()) {
                t.join();
            }
        }
    }
    catch (const std::exception& e) {
        Logger::log("Server run failed: " + std::string(e.what()), "ERROR");
        throw;
    }
}

void brazier::Server::stop() {
    shutdown_flag_.store(true);

    work_guard_.reset();
    io_.stop();

    if (stats_thread_.joinable()) {
        stats_thread_.join();
    }
    Logger::log("Server stopped", "INFO");
}

unsigned short brazier::Server::getPort() const { return port_; }
const std::string& brazier::Server::getHost() const { return host_; }

void brazier::Server::initializeConnections() {
    try {
        Queue::connect(global_config->get("nosql.host", "127.0.0.1"),
            global_config->get("nosql.port", 6379));
    }
    catch (const std::exception& e) {
        Logger::log("Connection to queue failed: " + std::string(e.what()), "ERROR");
    }

    try {
        Cache::connect(global_config->get("redis.host", "127.0.0.1"),
            global_config->get("redis.port", 6379));
    }
    catch (const std::exception& e) {
        Logger::log("Connection to NOSQL database failed: " + std::string(e.what()), "ERROR");
    }

}

net::awaitable<void> brazier::Server::handle_connection(tcp::socket socket) {
    connection_count_++;

    try {
        http::request<http::string_body> req;
        http::response<http::string_body> res;
        beast::flat_buffer buffer;
        bool keep_alive = true;

        socket.set_option(tcp::no_delay(true));
        socket.set_option(boost::asio::socket_base::keep_alive(true));

        net::steady_timer idle_timer(co_await net::this_coro::executor);
        const auto IDLE_TIMEOUT = std::chrono::seconds(global_config->get("keep-alive-timeout", 60));
        bool timed_out = false;

        auto reset_timer = [&]() {
            idle_timer.expires_after(IDLE_TIMEOUT);
            idle_timer.async_wait([&](beast::error_code ec) {
                if (!ec) {
                    timed_out = true;
                    socket.cancel();
                }
                });
            };

        reset_timer();

        while (keep_alive && !timed_out) {
            beast::error_code ec;
            req = {};

            co_await http::async_read(socket, buffer, req, net::redirect_error(net::use_awaitable, ec));

            if (ec == http::error::end_of_stream) {
                break;
            }
            if (ec) {
                if (ec == net::error::operation_aborted) {
                    break;
                }
                throw boost::system::system_error(ec);
            }

            reset_timer();

            total_requests_++;
            keep_alive = req.keep_alive();

            res = {};
            res.version(req.version());
            res.keep_alive(keep_alive);
            res.set(http::field::connection, keep_alive ? "keep-alive" : "close");
            res.set(http::field::server, "brazier");

            co_await Router::handle_request(req, res);

            if (res.body().empty() && res.count(http::field::content_length) == 0) {
                res.content_length(0);
            }
            else if (!res.body().empty() && res.count(http::field::content_length) == 0) {
                res.content_length(res.body().size());
            }

            res.prepare_payload();

            co_await http::async_write(socket, res, net::use_awaitable);

            buffer.consume(buffer.size());

            if (!keep_alive) {
                break;
            }
        }

        idle_timer.cancel();

        beast::error_code ec;
        socket.shutdown(tcp::socket::shutdown_send, ec);
        socket.close(ec);
    }
    catch (const std::exception& e) {
        Logger::log("Connection error: " + std::string(e.what()), "ERROR");
    }
    catch (...) {
        Logger::log("Unknown connection error", "ERROR");
    }

    connection_count_--;
    co_return;
}

net::awaitable<void> brazier::Server::accept_loop() {
    for (;;) {
        tcp::socket socket = co_await acceptor_.async_accept(net::use_awaitable);
        net::co_spawn(io_, handle_connection(std::move(socket)), net::detached);
    }
}