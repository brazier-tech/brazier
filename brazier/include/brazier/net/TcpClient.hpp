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

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <thread>

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>

namespace brazier {

    namespace net = boost::asio;
    namespace http = boost::beast::http;
    using tcp = net::ip::tcp;

    class TcpClient {
		std::chrono::microseconds timeout_;

    public:

        class timeout_exception : public std::runtime_error {
        public:
            using base = std::runtime_error;

            explicit timeout_exception(const std::string& _Message);
            explicit timeout_exception(const char* _Message);
        };

        TcpClient(std::string host, uint16_t port, std::chrono::microseconds timeout = std::chrono::seconds(5));

        bool connect();
        bool send_raw(const std::string& data);
        bool is_connection_closed(int wait_sec = 2);

        std::optional<http::response<http::string_body>> read_response(int timeout_sec = 10);

    private:
        struct State {
            enum status { 
                pending, 
                connected, 
                done, 
                failed, 
                timed_out 
            } 

            st = pending;
            boost::system::error_code ec;
            boost::beast::flat_buffer buffer;
            http::response<http::string_body> res;
        };

        std::string host_;
        uint16_t port_;
        net::io_context io_;
        tcp::socket socket_;

        void async_resolve(tcp::resolver& resolver, std::shared_ptr<State> state);
    };
}