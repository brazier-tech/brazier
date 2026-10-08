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

#include "../include/brazier/vendor/WebSocket/WebSocketManager.hpp"

namespace brazier {

    WebSocketManager& WebSocketManager::instance() {
        static WebSocketManager instance;
        return instance;
    }

    void WebSocketManager::add_session(std::shared_ptr<WebSocketSession> session) {
        std::unique_lock lock(mutex_);
        sessions_.push_back(session);
        session_count_.store(sessions_.size(), std::memory_order_release);

        Logger::log("WebSocket session added [ID: " + std::string(session->get_id()) + "], total: " + std::to_string(sessions_.size()), "INFO");

        if (global_message_handler_) {
            session->set_message_handler(global_message_handler_);
        }
        if (global_binary_handler_) {
            session->set_binary_handler(global_binary_handler_);
        }
        if (global_close_handler_) {
            session->set_close_handler(global_close_handler_);
        }
        if (global_error_handler_) {
            session->set_error_handler(global_error_handler_);
        }
    }

    void WebSocketManager::remove_session(std::shared_ptr<WebSocketSession> session) {
        std::unique_lock lock(mutex_);
        for (size_t i = 0; i < sessions_.size(); ++i) {
            if (sessions_[i] == session) {
                sessions_[i] = std::move(sessions_.back());
                sessions_.pop_back();
                session_count_.store(sessions_.size(), std::memory_order_release);
                Logger::log("WebSocket session removed [ID: " + std::string(session->get_id()) + "], remaining: " + std::to_string(sessions_.size()), "INFO");
                break;
            }
        }
    }

    void WebSocketManager::close_all() {
        std::shared_lock lock(mutex_);
        for (auto& session : sessions_) {
            if (session && session->is_open()) {
                session->close();
            }
        }
    }

    void WebSocketManager::broadcast(const std::string& message) {
        std::shared_lock lock(mutex_);
        for (auto& session : sessions_) {
            if (session && session->is_open()) {
                session->send(message);
            }
        }
    }

    void WebSocketManager::broadcast_binary(const std::vector<uint8_t>& data) {
        std::shared_lock lock(mutex_);
        for (auto& session : sessions_) {
            if (session && session->is_open()) {
                session->send_binary(data);
            }
        }
    }

    void WebSocketManager::set_global_message_handler(WebSocketSession::MessageHandler handler) {
        std::unique_lock lock(mutex_);
        global_message_handler_ = std::move(handler);
        for (auto& session : sessions_) {
            session->set_message_handler(global_message_handler_);
        }
    }

    void WebSocketManager::set_global_binary_handler(WebSocketSession::BinaryHandler handler) {
        std::unique_lock lock(mutex_);
        global_binary_handler_ = std::move(handler);
        for (auto& session : sessions_) {
            session->set_binary_handler(global_binary_handler_);
        }
    }

    void WebSocketManager::set_global_close_handler(WebSocketSession::CloseHandler handler) {
        std::unique_lock lock(mutex_);
        global_close_handler_ = std::move(handler);
        for (auto& session : sessions_) {
            session->set_close_handler(global_close_handler_);
        }
    }

    void WebSocketManager::set_global_error_handler(WebSocketSession::ErrorHandler handler) {
        std::unique_lock lock(mutex_);
        global_error_handler_ = std::move(handler);
        for (auto& session : sessions_) {
            session->set_error_handler(global_error_handler_);
        }
    }

    std::shared_ptr<WebSocketSession> WebSocketManager::get_session(const std::string& session_id) {
        std::shared_lock lock(mutex_);
        for (auto& session : sessions_) {
            if (session && session->get_id() == session_id) {
                return session;
            }
        }
        return nullptr;
    }

}