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
#include <memory>
#include "Router.hpp"
#include "WSRouter.hpp"
#include <boost/asio/io_context.hpp>
#include "types.hpp"

namespace brazier {

    class RouterRegisterer {
        using Request = http::request<http::string_body>;
        using Response = http::response<http::string_body>;
        using Params = std::unordered_map<std::string, std::string>;

    public:
        static void init(boost::asio::io_context& io);
    };

    #define R(method, path, controller, handler) \
            Router::add(method, path, [controller](const Request& req, Response& res, const Params& params) -> boost::asio::awaitable<void> { \
                co_await controller->handler(req, res, params); \
            })

    #define WS_R(path, controller, handler) \
        brazier::WebSocketRouter::instance().add_route(path, \
            [controller](std::shared_ptr<brazier::WebSocketSession> session, const brazier::Params& params) { \
                controller->handler(session, params); \
            })

    #define WS_MSG(path, controller, handler) \
        { \
            brazier::Params dummy_params; \
            auto* route = brazier::WebSocketRouter::instance().find_route(path, dummy_params); \
            if (route) { \
                route->set_message_handler([controller](const std::string& msg, std::shared_ptr<brazier::WebSocketSession> session) { \
                    controller->handler(msg, session); \
                }); \
            } else { \
            } \
        } 

    #define WS_BIN(path, controller, handler) \
        { \
            brazier::Params dummy_params; \
            auto* route = brazier::WebSocketRouter::instance().find_route(path, dummy_params); \
            if (route) { \
                route->set_binary_handler([controller](std::vector<uint8_t>&& data, std::shared_ptr<brazier::WebSocketSession> session) { \
                    controller->handler(std::move(data), session); \
                }); \
            } \
        }

    #define WS_CLOSE(path, controller, handler) \
        { \
            brazier::Params dummy_params; \
            auto* route = brazier::WebSocketRouter::instance().find_route(path, dummy_params); \
            if (route) { \
                route->set_close_handler([controller](std::shared_ptr<brazier::WebSocketSession> session) { \
                    controller->handler(session); \
                }); \
            } \
        }

    #define WS_ERR(path, controller, handler) \
        { \
            brazier::Params dummy_params; \
            auto* route = brazier::WebSocketRouter::instance().find_route(path, dummy_params); \
            if (route) { \
                route->set_error_handler([controller](const std::string& error, std::shared_ptr<brazier::WebSocketSession> session) { \
                    controller->handler(error, session); \
                }); \
            } \
        }

    #define WS_G_MSG(controller, handler) \
        brazier::WebSocketRouter::instance().set_global_message_handler( \
            [controller](const std::string& msg, std::shared_ptr<brazier::WebSocketSession> session) { \
                controller->handler(msg, session); \
            })

    #define WS_G_BIN(controller, handler) \
        brazier::WebSocketRouter::instance().set_global_binary_handler( \
            [controller](std::vector<uint8_t>&& data, std::shared_ptr<brazier::WebSocketSession> session) { \
                controller->handler(std::move(data), session); \
            })

    #define WS_G_CLOSE(controller, handler) \
        brazier::WebSocketRouter::instance().set_global_close_handler( \
            [controller](std::shared_ptr<brazier::WebSocketSession> session) { \
                controller->handler(session); \
            })

    #define WS_G_ERR(controller, handler) \
        brazier::WebSocketRouter::instance().set_global_error_handler( \
            [controller](const std::string& error, std::shared_ptr<brazier::WebSocketSession> session) { \
                controller->handler(error, session); \
            })
}