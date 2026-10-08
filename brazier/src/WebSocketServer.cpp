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

#include "../include/brazier/WebSocketServer.hpp"

namespace brazier {

    WebSocketServer::WebSocketServer(const std::string& host, unsigned short port)
        : acceptor_(io_, tcp::endpoint(net::ip::make_address(host), port))
        , port_(port), host_(host) {
        acceptor_.set_option(tcp::acceptor::reuse_address(true));
        work_guard_ = std::make_unique<net::executor_work_guard<net::io_context::executor_type>>(io_.get_executor());

        WebSocketRouter::instance().set_global_message_handler([](const std::string& message, std::shared_ptr<WebSocketSession> session) {
            Logger::log("[GLOBAL] Message handler called: " + message, "DEBUG");
            session->send("Echo: " + message);
            });

        WebSocketRouter::instance().set_global_binary_handler([](std::vector<uint8_t>&& data, std::shared_ptr<WebSocketSession> session) {
            Logger::log("[GLOBAL] Binary handler called, size: " + std::to_string(data.size()), "DEBUG");
            session->send("Binary received, size: " + std::to_string(data.size()));
            });

        WebSocketRouter::instance().set_global_close_handler([](std::shared_ptr<WebSocketSession> session) {
            Logger::log("WebSocket session closed [ID: " + std::string(session->get_id()) + "]", "INFO");
            });

        WebSocketRouter::instance().set_global_error_handler([](const std::string& error, std::shared_ptr<WebSocketSession> session) {
            Logger::log("WebSocket error [ID: " + std::string(session->get_id()) + "]: " + error, "ERROR");
            });

        Logger::log("[DEBUG] WebSocketServer constructor: global handlers set", "DEBUG");
    }

    bool WebSocketServer::initialize() {
        try {
            Logger::log("WebSocket server initialized on " + host_ + ":" + std::to_string(port_), "SUCCESS");
            return true;
        }
        catch (const std::exception& e) {
            Logger::log("WebSocket initialization failed: " + std::string(e.what()), "ERROR");
            return false;
        }
    }

    void WebSocketServer::run() {
        try {
            net::co_spawn(io_, accept_loop(), net::detached);

            int threads_count = std::thread::hardware_concurrency();
            if (threads_count == 0) threads_count = 1;

            Logger::log("Starting " + std::to_string(threads_count) + " WebSocket worker threads", "INFO");

            for (int i = 0; i < threads_count; ++i) {
                threads_.emplace_back([this] {
                    io_.run();
                    });
            }

            for (auto& t : threads_) {
                if (t.joinable()) {
                    t.join();
                }
            }
        }
        catch (const std::exception& e) {
            Logger::log("WebSocket server run failed: " + std::string(e.what()), "ERROR");
            throw;
        }
    }

    void WebSocketServer::stop() {
        WebSocketManager::instance().close_all();
        work_guard_.reset();
        io_.stop();
        Logger::log("WebSocket server stopped", "INFO");
    }

    WebSocketManager& WebSocketServer::get_websocket_manager() {
        return WebSocketManager::instance();
    }

    void WebSocketServer::set_websocket_message_handler(WebSocketSession::MessageHandler handler) {
        WebSocketRouter::instance().set_global_message_handler(std::move(handler));
    }

    void WebSocketServer::set_websocket_binary_handler(WebSocketSession::BinaryHandler handler) {
        WebSocketRouter::instance().set_global_binary_handler(std::move(handler));
    }

    void WebSocketServer::set_websocket_close_handler(WebSocketSession::CloseHandler handler) {
        WebSocketRouter::instance().set_global_close_handler(std::move(handler));
    }

    void WebSocketServer::set_websocket_error_handler(WebSocketSession::ErrorHandler handler) {
        WebSocketRouter::instance().set_global_error_handler(std::move(handler));
    }

    void WebSocketServer::add_route(const std::string& path, WebSocketRoute::ConnectHandler handler) {
        Logger::log("[DEBUG] add_route called for path: " + path, "DEBUG");
        WebSocketRouter::instance().add_route(path, std::move(handler));
    }

    WebSocketRoute* WebSocketServer::set_route_message_handler(const std::string& path, WebSocketSession::MessageHandler handler) {
        Logger::log("[DEBUG] set_route_message_handler called for path: " + path, "DEBUG");
        Params dummy;
        auto* route = WebSocketRouter::instance().find_route(path, dummy);
        if (route) {
            route->set_message_handler(std::move(handler));
            Logger::log("[DEBUG] set_route_message_handler: handler set successfully", "DEBUG");
        }
        else {
            Logger::log("[DEBUG] set_route_message_handler: route NOT found for path: " + path, "ERROR");
        }
        return route;
    }

    WebSocketRoute* WebSocketServer::set_route_binary_handler(const std::string& path, WebSocketSession::BinaryHandler handler) {
        Params dummy;
        auto* route = WebSocketRouter::instance().find_route(path, dummy);
        if (route) {
            route->set_binary_handler(std::move(handler));
        }
        return route;
    }

    WebSocketRoute* WebSocketServer::set_route_close_handler(const std::string& path, WebSocketSession::CloseHandler handler) {
        Params dummy;
        auto* route = WebSocketRouter::instance().find_route(path, dummy);
        if (route) {
            route->set_close_handler(std::move(handler));
        }
        return route;
    }

    WebSocketRoute* WebSocketServer::set_route_error_handler(const std::string& path, WebSocketSession::ErrorHandler handler) {
        Params dummy;
        auto* route = WebSocketRouter::instance().find_route(path, dummy);
        if (route) {
            route->set_error_handler(std::move(handler));
        }
        return route;
    }

    std::string WebSocketServer::extract_path(const http::request<http::string_body>& req) {
        std::string target = std::string(req.target());
        size_t query_pos = target.find('?');
        if (query_pos != std::string::npos) {
            target = target.substr(0, query_pos);
        }
        return target;
    }

    net::awaitable<void> WebSocketServer::handle_websocket(tcp::socket socket) {
        beast::flat_buffer buffer;
        http::request<http::string_body> req;

        co_await http::async_read(socket, buffer, req, net::use_awaitable);

        std::string path = extract_path(req);

        if (!websocket::is_upgrade(req)) {
            http::response<http::string_body> res{ http::status::bad_request, req.version() };
            res.set(http::field::content_type, "text/plain");
            res.body() = "WebSocket upgrade required";
            res.prepare_payload();
            co_await http::async_write(socket, res, net::use_awaitable);
            co_return;
        }

        auto session = std::make_shared<WebSocketSession>(std::move(socket));

        beast::error_code ec;
        session->accept_handshake(req, ec);
        if (ec) {
            Logger::log("WebSocket accept failed: " + ec.message(), "ERROR");
            co_return;
        }

        Params params;
        WebSocketRoute* route = WebSocketRouter::instance().find_route(path, params);

        if (route) {
            if (route->get_message_handler()) {
                session->set_message_handler(route->get_message_handler());
            }
            else {
                session->set_message_handler(WebSocketRouter::instance().get_global_message_handler());
            }

            if (route->get_binary_handler()) {
                session->set_binary_handler(route->get_binary_handler());
            }
            else {
                session->set_binary_handler(WebSocketRouter::instance().get_global_binary_handler());
            }

            if (route->get_error_handler()) {
                session->set_error_handler(route->get_error_handler());
            }
            else {
                session->set_error_handler(WebSocketRouter::instance().get_global_error_handler());
            }

            auto global_close = WebSocketRouter::instance().get_global_close_handler();
            auto route_close = route->get_close_handler();

            session->set_close_handler([global_close, route_close](std::shared_ptr<WebSocketSession> s) {
                if (route_close) {
                    route_close(s);
                }
                if (global_close) {
                    global_close(s);
                }
                WebSocketManager::instance().remove_session(s);
                });

            route->on_connect(session, params);
        }
        else {
            session->set_message_handler(WebSocketRouter::instance().get_global_message_handler());
            session->set_binary_handler(WebSocketRouter::instance().get_global_binary_handler());
            session->set_error_handler(WebSocketRouter::instance().get_global_error_handler());

            auto global_close = WebSocketRouter::instance().get_global_close_handler();
            session->set_close_handler([global_close](std::shared_ptr<WebSocketSession> s) {
                if (global_close) {
                    global_close(s);
                }
                WebSocketManager::instance().remove_session(s);
                });
        }

        WebSocketManager::instance().add_session(session);
        co_await session->run();
        co_return;
    }

    net::awaitable<void> WebSocketServer::handle_connection(tcp::socket socket) {
        connection_count_++;
        co_await handle_websocket(std::move(socket));
        connection_count_--;
        co_return;
    }

    net::awaitable<void> WebSocketServer::accept_loop() {
        for (;;) {
            tcp::socket socket = co_await acceptor_.async_accept(net::use_awaitable);
            net::co_spawn(io_, handle_connection(std::move(socket)), net::detached);
        }
    }

}