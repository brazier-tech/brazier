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
#include <iostream>
#include <boost/beast/http.hpp>
#include <unordered_map>
#include <memory>
#include <functional>
#include <regex>
#include "Route.hpp"

namespace brazier {

    namespace beast = boost::beast;
    namespace http = beast::http;

    class Router {
        using Request = http::request<http::string_body>;
        using Response = http::response<http::string_body>;
        using Params = std::unordered_map<std::string, std::string>;

        static std::unordered_map<http::verb, std::vector<Route>>& get_routes();

    public:
        static void add(http::verb method, const std::string& path,
            std::function<boost::asio::awaitable<void>(const Request&, Response&, const Params&)> handler);

        static boost::asio::awaitable<void> handle_request(const Request& req, Response& res);
    };
}