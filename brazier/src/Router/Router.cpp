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

#include "../../include/brazier/Router/Router.hpp"

using namespace brazier;

std::unordered_map<http::verb, std::vector<Route>>& Router::get_routes() {
    static std::unordered_map<http::verb, std::vector<Route>> routes;
    return routes;
}

void Router::add(http::verb method, const std::string& path,
    std::function<boost::asio::awaitable<void>(const Request&, Response&, const Params&)> handler) {
    get_routes()[method].emplace_back(path, std::move(handler));
}

boost::asio::awaitable<void> Router::handle_request(const Request& req, Response& res) {
    auto method = req.method();
    std::string target = std::string(req.target());
    auto& routes = get_routes(); 

    if (routes.find(method) != routes.end()) {
        for (const auto& route : routes[method]) {
            Params params;
            if (route.match(target, params)) {
                co_await route.process(req, res, params);
                co_return;
            }
        }
    }

    res.result(http::status::not_found);
    res.body() = "404 Not Found";
    res.prepare_payload();
    co_return;
}