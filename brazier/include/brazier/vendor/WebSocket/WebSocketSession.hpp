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
#include <boost/beast/websocket.hpp>
#include <boost/beast/http.hpp> 
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/awaitable.hpp>
#include <functional>
#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <brazier/log/Logger.hpp>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace http = beast::http;
using tcp = net::ip::tcp;

namespace brazier {

    class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
    public:
        using MessageHandler = std::function<void(const std::string&, std::shared_ptr<WebSocketSession>)>;
        using BinaryHandler = std::function<void(std::vector<uint8_t>&&, std::shared_ptr<WebSocketSession>)>;
        using CloseHandler = std::function<void(std::shared_ptr<WebSocketSession>)>;
        using ErrorHandler = std::function<void(const std::string&, std::shared_ptr<WebSocketSession>)>;

        explicit WebSocketSession(tcp::socket&& socket);
        ~WebSocketSession();

        net::awaitable<void> run();

        void send(std::string message);
        void send_binary(std::vector<uint8_t> data);
        void close(websocket::close_reason reason = {});

        bool accept_handshake(const http::request<http::string_body>& req, beast::error_code& ec);

        void set_message_handler(MessageHandler handler);
        void set_binary_handler(BinaryHandler handler);
        void set_close_handler(CloseHandler handler);
        void set_error_handler(ErrorHandler handler);

        const char* get_id() const;
        bool is_open() const;

    private:
        static uint64_t generate_session_id();

        websocket::stream<tcp::socket> ws_;
        uint64_t session_id_;
        char session_id_str_[32];

        MessageHandler message_handler_;
        BinaryHandler binary_handler_;
        CloseHandler close_handler_;
        ErrorHandler error_handler_;

        void fast_itoa(uint64_t num, char* buf);
    };

}