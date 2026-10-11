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

#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/config.hpp>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pem.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "Platform/SocketOptions.hpp"
#include "Platform/SystemInfo.hpp"

#include "TLS/TicketKeyStore.hpp"
#include "Database/Queue.hpp"
#include "Database/Cache.hpp"
#include "Router/RouterRegisterer.hpp"
#include "Router/Router.hpp"
#include "Engine.hpp"
#include "Filesystem/Filesystem.hpp"
#include "vendor/ConfigManager.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = net::ip::tcp;
using namespace std::chrono_literals;

namespace brazier {

    class HttpsServer {
    public:
        struct TlsConfig {
            std::string cert_file;
            std::string key_file;
            std::string ca_file;
            std::string cert_pem;
            std::string key_pem;

            std::vector<std::pair<std::string, std::string>> conf;

            bool require_client_cert = false;
            bool verify_client_cert = false;

            std::chrono::seconds handshake_timeout{ 15 };
        };

    private:
        std::chrono::seconds keep_alive_timeout_{ 60 };

        int max_body_size_ = 1024 * 1024;
        int max_header_size_ = 8 * 1024;
        int max_connections_ = 10000;

        std::vector<std::unique_ptr<net::io_context>> io_contexts_;
        std::vector<std::unique_ptr<tcp::acceptor>>   acceptors_;
        std::vector<std::unique_ptr<
            net::executor_work_guard<net::io_context::executor_type>>> work_guards_;

        std::shared_ptr<ssl::context> ssl_ctx_;
        std::shared_mutex             ssl_ctx_mutex_;

        std::thread       stats_thread_;
        std::atomic<bool> shutdown_flag_{ false };

        std::mutex              stats_mutex_;
        std::condition_variable stats_cv_;

        std::atomic<bool>       shutting_down_{ false };
        std::mutex              shutdown_mutex_;
        std::condition_variable shutdown_cv_;

        unsigned short port_;
        std::string    host_;

        std::vector<std::thread> threads_;

        std::atomic<int> connection_count_{ 0 };
        std::atomic<int> total_requests_{ 0 };

        TicketKeyStore ticket_store_;
        TlsConfig      tls_;
        bool           tls_config_from_user_ = false;

        struct ConnectionGuard {
            HttpsServer& srv;
            explicit ConnectionGuard(HttpsServer& s) noexcept : srv(s) {}
            ~ConnectionGuard() { srv.release_connection(); }

            ConnectionGuard(const ConnectionGuard&) = delete;
            ConnectionGuard& operator=(const ConnectionGuard&) = delete;
        };

        std::string server_name_ = "brazier";
        std::string hsts_header_ = "max-age=31536000";
        bool        hsts_enabled_ = true;

        std::string static_headers_;

    public:
        HttpsServer(const std::string& host, unsigned short port);
        HttpsServer(const std::string& host, unsigned short port,
            const TlsConfig& tls);

        void setTlsConfig(const TlsConfig& tls);

        bool reloadTls();
        bool reloadTls(const TlsConfig& new_tls);

        bool initialize();

        void run();
        void stop();

        unsigned short     getPort() const;
        const std::string& getHost() const;

        int getMaxConnections() const { return max_connections_; }
        int getMaxBodySize()    const { return max_body_size_; }
        int getMaxHeaderSize()  const { return max_header_size_; }

    private:
        void initializeConnections();

        void load_common_config_from_global();
        void load_tls_config_from_global();
        void load_limits_from_config();

        void configure_tls();
        void configure_ssl_ctx(ssl::context& ctx);
        void apply_ssl_conf(ssl::context& ctx);
        void load_cert_from_memory(ssl::context& ctx,
            const std::string& cert_pem,
            const std::string& key_pem);

        std::shared_ptr<ssl::context> get_ssl_ctx();

        void release_connection();

        static int compute_max_body_size(int ram_mb, int max_conn);
        static int compute_max_header_size(int ram_mb, int max_conn);

        net::awaitable<void> handle_connection(tcp::socket socket);
        net::awaitable<void> accept_loop(tcp::acceptor& acceptor);
        net::awaitable<void> accept_and_dispatch(tcp::acceptor& acceptor,
            int worker_begin);
    };

}