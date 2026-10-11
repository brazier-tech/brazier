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

#include "../include/brazier/vendor/WebSocket/WebSocketSession.hpp"
#include <memory>
#include <vector>
#include <atomic>
#include <shared_mutex>
#include <brazier/log/Logger.hpp>

namespace brazier {

    class WebSocketManager {
    public:
        static WebSocketManager& instance();

        void add_session(std::shared_ptr<WebSocketSession> session);
        void remove_session(std::shared_ptr<WebSocketSession> session);

        void broadcast(const std::string& message);
        void broadcast_binary(const std::vector<uint8_t>& data);

        void close_all();

        std::shared_ptr<WebSocketSession> get_session(const std::string& session_id);

        size_t get_session_count() const { return session_count_.load(std::memory_order_acquire); }

        void set_global_message_handler(WebSocketSession::MessageHandler handler);
        void set_global_binary_handler(WebSocketSession::BinaryHandler handler);
        void set_global_close_handler(WebSocketSession::CloseHandler handler);
        void set_global_error_handler(WebSocketSession::ErrorHandler handler);

    private:
        WebSocketManager() = default;
        ~WebSocketManager() = default;
        WebSocketManager(const WebSocketManager&) = delete;
        WebSocketManager& operator=(const WebSocketManager&) = delete;

        std::vector<std::shared_ptr<WebSocketSession>> sessions_;
        mutable std::shared_mutex mutex_;
        std::atomic<size_t> session_count_{ 0 };

        WebSocketSession::MessageHandler global_message_handler_;
        WebSocketSession::BinaryHandler global_binary_handler_;
        WebSocketSession::CloseHandler global_close_handler_;
        WebSocketSession::ErrorHandler global_error_handler_;
    };

}