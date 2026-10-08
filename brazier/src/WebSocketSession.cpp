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

#include "../include/brazier/vendor/WebSocket/WebSocketSession.hpp"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <chrono>
#include <random>

namespace brazier {

    static std::atomic<uint64_t> session_counter{ 0 };
    static thread_local std::mt19937_64 rng(std::random_device{}());

    uint64_t WebSocketSession::generate_session_id() {
        auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        return (timestamp << 20) ^ (rng() & 0xFFFFF) ^ session_counter++;
    }

    void WebSocketSession::fast_itoa(uint64_t num, char* buf) {
        char* p = buf;
        if (num == 0) {
            *p++ = '0';
            *p = '\0';
            return;
        }

        char temp[20];
        char* t = temp;
        while (num > 0) {
            *t++ = '0' + (num % 10);
            num /= 10;
        }

        while (t > temp) {
            *p++ = *--t;
        }
        *p = '\0';
    }

    WebSocketSession::WebSocketSession(tcp::socket&& socket)
        : ws_(std::move(socket))
        , session_id_(generate_session_id()) {
        fast_itoa(session_id_, session_id_str_);
        ws_.binary(true);
        ws_.write_buffer_bytes(65536);
        ws_.read_message_max(1024 * 1024 * 10);
        ws_.auto_fragment(false);
    }

    WebSocketSession::~WebSocketSession() {
        if (close_handler_) {
            close_handler_(shared_from_this());
        }
    }

    net::awaitable<void> WebSocketSession::run() {
        try {
            Logger::log("WebSocket connection established [ID: " + std::string(session_id_str_) + "]", "INFO");

            while (ws_.is_open()) {
                beast::flat_buffer buffer;
                co_await ws_.async_read(buffer, net::use_awaitable);

                if (ws_.got_binary()) {
                    if (binary_handler_) {
                        auto size = buffer.size();
                        std::vector<uint8_t> data;
                        data.reserve(size);
                        data.assign(
                            static_cast<const uint8_t*>(buffer.data().data()),
                            static_cast<const uint8_t*>(buffer.data().data()) + size
                        );
                        binary_handler_(std::move(data), shared_from_this());
                    }
                }
                else {
                    if (message_handler_) {
                        auto size = buffer.size();
                        std::string message;
                        message.reserve(size);
                        message.assign(
                            static_cast<const char*>(buffer.data().data()),
                            size
                        );
                        message_handler_(message, shared_from_this());
                    }
                }
            }
        }
        catch (const beast::system_error& se) {
            if (se.code() != websocket::error::closed) {
                Logger::log("WebSocket error [ID: " + std::string(session_id_str_) + "]: " + std::string(se.what()), "ERROR");
                if (error_handler_) {
                    error_handler_(se.what(), shared_from_this());
                }
            }
        }
        catch (const std::exception& e) {
            Logger::log("WebSocket exception [ID: " + std::string(session_id_str_) + "]: " + std::string(e.what()), "ERROR");
            if (error_handler_) {
                error_handler_(e.what(), shared_from_this());
            }
        }

        Logger::log("WebSocket connection closed [ID: " + std::string(session_id_str_) + "]", "INFO");
        co_return;
    }

    void WebSocketSession::send(std::string message) {
        if (!ws_.is_open()) return;

        auto self = shared_from_this();
        net::co_spawn(ws_.get_executor(),
            [self, msg = std::move(message)]() mutable -> net::awaitable<void> {
                try {
                    co_await self->ws_.async_write(net::buffer(msg), net::use_awaitable);
                }
                catch (...) {}
            }, net::detached);
    }

    void WebSocketSession::send_binary(std::vector<uint8_t> data) {
        if (!ws_.is_open()) return;

        auto self = shared_from_this();
        net::co_spawn(ws_.get_executor(),
            [self, d = std::move(data)]() mutable -> net::awaitable<void> {
                try {
                    co_await self->ws_.async_write(net::buffer(d), net::use_awaitable);
                }
                catch (...) {}
            }, net::detached);
    }

    void WebSocketSession::close(websocket::close_reason reason) {
        if (!ws_.is_open()) return;

        auto self = shared_from_this();
        net::co_spawn(ws_.get_executor(),
            [self, reason]() -> net::awaitable<void> {
                try {
                    co_await self->ws_.async_close(reason, net::use_awaitable);
                }
                catch (...) {}
            }, net::detached);
    }

    void WebSocketSession::set_message_handler(MessageHandler handler) {
        message_handler_ = std::move(handler);
    }

    void WebSocketSession::set_binary_handler(BinaryHandler handler) {
        binary_handler_ = std::move(handler);
    }

    void WebSocketSession::set_close_handler(CloseHandler handler) {
        close_handler_ = std::move(handler);
    }

    void WebSocketSession::set_error_handler(ErrorHandler handler) {
        error_handler_ = std::move(handler);
    }

    const char* WebSocketSession::get_id() const {
        return session_id_str_;
    }

    bool WebSocketSession::is_open() const {
        return ws_.is_open();
    }

    bool WebSocketSession::accept_handshake(const http::request<http::string_body>& req, beast::error_code& ec) {
        ws_.accept(req, ec);
        return !ec;
    }

}