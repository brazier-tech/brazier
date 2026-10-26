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

brazier::HttpsServer::HttpsServer(const std::string& host, unsigned short port)
    : port_(port), host_(host) {}

brazier::HttpsServer::HttpsServer(const std::string& host, unsigned short port,
    const TlsConfig& tls)
    : port_(port), host_(host), tls_(tls), tls_config_from_user_(true) {}

void brazier::HttpsServer::setTlsConfig(const TlsConfig& tls) {
    tls_ = tls;
    tls_config_from_user_ = true;
}

bool brazier::HttpsServer::initialize() {
    try {
        Logger::init("debug.log");
        Logger::registerSignalHandlers();

        json drivers = ConfigManager::getGlobal().getJson("filesystem.drivers");

        for (auto& [name, cfg] : drivers.items()) {
            if (name == "default") continue;

            auto driver = std::make_shared<brazier::FileDriver>();
            driver->setRootPath(cfg.value("root", "./"));
            driver->initAsync();

            StorageManager::getInstance().registerDriver(name, driver);
        }

        std::string def = ConfigManager::getGlobal().getNested<std::string>(
            "filesystem.default", "local");
        if (StorageManager::getInstance().hasDriver(def)) {
            StorageManager::getInstance().setDefaultDriver(def);
        }

        load_common_config_from_global();
        load_tls_config_from_global();
        load_limits_from_config();

        ssl_ctx_ = std::make_shared<ssl::context>(ssl::context::tls_server);
        configure_tls();

        const tcp::endpoint endpoint(net::ip::make_address(host_), port_);

        const int n = platform::get_thread_count();
        const bool split_accept = !platform::has_reuse_port();
        const int io_count = n;

        io_contexts_.reserve(io_count);
        work_guards_.reserve(io_count);
        acceptors_.reserve(split_accept ? 1 : io_count);

        for (int i = 0; i < io_count; ++i) {
            auto io = std::make_unique<net::io_context>();

            const bool needs_acceptor = !split_accept || (i == 0);

            if (needs_acceptor) {
                auto acc = std::make_unique<tcp::acceptor>(*io);
                acc->open(endpoint.protocol());
                acc->set_option(tcp::acceptor::reuse_address(true));

                const auto native = static_cast<platform::NativeSocket>(
                    acc->native_handle());

                if (!platform::set_reuse_port(native)
                    && platform::has_reuse_port()) {
                    Logger::log("SO_REUSEPORT setsockopt failed on worker " +
                        std::to_string(i), "WARNING");
                }

                if (!platform::set_defer_accept(native, 1)
                    && platform::has_defer_accept()) {
                    Logger::log("TCP_DEFER_ACCEPT setsockopt failed on worker " +
                        std::to_string(i), "WARNING");
                }

                acc->bind(endpoint);
                acc->listen(boost::asio::socket_base::max_listen_connections);
                acceptors_.push_back(std::move(acc));
            }

            work_guards_.push_back(std::make_unique<
                net::executor_work_guard<net::io_context::executor_type>>(
                    io->get_executor()));

            io_contexts_.push_back(std::move(io));
        }

        initializeConnections();
        RouterRegisterer::init(*io_contexts_[0]);

        Logger::log("HTTPS server initialized on " + host_ + ":" +
            std::to_string(port_) + " [TLS, io_contexts=" +
            std::to_string(io_count) + ", acceptors=" +
            std::to_string(acceptors_.size()) + ", dispatch=" +
            (split_accept ? "round-robin" : "SO_REUSEPORT") +
            ", SO_REUSEPORT=" +
            (platform::has_reuse_port() ? "yes" : "no") +
            ", TCP_DEFER_ACCEPT=" +
            (platform::has_defer_accept() ? "yes" : "no") +
            "]", "SUCCESS");
        return true;
    }
    catch (const std::exception& e) {
        Logger::log("HTTPS initialization failed: " + std::string(e.what()),
            "ERROR");
        return false;
    }
}

void brazier::HttpsServer::run() {
    try {
        const int io_count = static_cast<int>(io_contexts_.size());
        const bool split_accept = !platform::has_reuse_port();

        if (split_accept) {
            net::co_spawn(*io_contexts_[0],
                accept_and_dispatch(*acceptors_[0], 0),
                net::detached);
        }
        else {
            for (int i = 0; i < io_count; ++i) {
                net::co_spawn(*io_contexts_[i],
                    accept_loop(*acceptors_[i]),
                    net::detached);
            }
        }

        Logger::log("Starting " + std::to_string(io_count) +
            " io_context(s) over " + std::to_string(io_count) +
            " thread(s), dispatch=" +
            (split_accept ? "round-robin" : "SO_REUSEPORT"),
            "INFO");

        for (auto& io : io_contexts_) {
            auto* io_ptr = io.get();
            threads_.emplace_back([io_ptr] {
                brazier::Engine::init(*io_ptr);
                io_ptr->run();
                });
        }

        shutdown_flag_.store(false, std::memory_order_release);

        stats_thread_ = std::thread([this] {
            std::unique_lock<std::mutex> lock(stats_mutex_);
            while (!shutdown_flag_.load(std::memory_order_acquire)) {
                const bool woke = stats_cv_.wait_for(
                    lock,
                    std::chrono::seconds(10),
                    [this] {
                        return shutdown_flag_.load(std::memory_order_acquire);
                    });

                if (woke) break;

                lock.unlock();
                Logger::log("HTTPS STATS - Active connections: " +
                    std::to_string(connection_count_.load()) +
                    ", Total requests: " +
                    std::to_string(total_requests_.load()),
                    "INFO");
                lock.lock();
            }
            });

        for (auto& t : threads_) {
            if (t.joinable()) t.join();
        }

        if (stats_thread_.joinable()) stats_thread_.join();

        Logger::log("HTTPS server stopped", "INFO");
    }
    catch (const std::exception& e) {
        Logger::log("HTTPS server run failed: " + std::string(e.what()), "ERROR");
        throw;
    }
}

void brazier::HttpsServer::stop() {
    Logger::log("HTTPS server stopping (graceful)...", "INFO");

    shutdown_flag_.store(true, std::memory_order_release);
    shutting_down_.store(true, std::memory_order_release);

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_cv_.notify_all();
    }

    for (auto& acc : acceptors_) {
        boost::system::error_code ignore;
        acc->close(ignore);
    }

    for (auto& wg : work_guards_) wg->reset();

    {
        std::unique_lock<std::mutex> lock(shutdown_mutex_);
        const bool drained = shutdown_cv_.wait_for(
            lock,
            std::chrono::seconds(10),
            [this] {
                return connection_count_.load(std::memory_order_acquire) == 0;
            });

        if (!drained) {
            Logger::log("Graceful shutdown timeout: " +
                std::to_string(connection_count_.load()) +
                " connections still active, forcing stop", "WARNING");
        }
    }

    for (auto& io : io_contexts_) io->stop();

    Logger::log("HTTPS server stop() signaled", "INFO");
}

unsigned short brazier::HttpsServer::getPort() const { return port_; }
const std::string& brazier::HttpsServer::getHost() const { return host_; }

void brazier::HttpsServer::release_connection() {
    if (connection_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        std::lock_guard<std::mutex> lock(shutdown_mutex_);
        shutdown_cv_.notify_all();
    }
}