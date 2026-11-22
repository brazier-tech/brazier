/**
 * Copyright (c) 2026 Kirill Sergeev, Nikolay Sugonyako, Andrey Agarkov, Hlib Safiannikov.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This source code is licensed under the LGPL license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../../include/brazier/Router/Router.hpp"

using namespace brazier;

std::unordered_map<http::verb, Router::RouteList>& Router::get_routes() {
    static std::unordered_map<http::verb, RouteList> routes;
    return routes;
}

std::shared_mutex& Router::get_routes_mutex() {
    static std::shared_mutex mutex;
    return mutex;
}

std::shared_ptr<Route> Router::add(http::verb method, const std::string& path, FusedOnion::Controller handler) {
    auto route = std::make_shared<Route>(path, std::move(handler));

    {
        std::unique_lock lock(get_routes_mutex());
        get_routes()[method].push_back(route);
    }

    return route;
}

boost::asio::awaitable<void> Router::handle_request(const Request& req, Response& res) {
    const auto method = req.method();
    const std::string target(req.target());

    RouteList candidates;
    std::shared_lock lock(get_routes_mutex());
    const auto it = get_routes().find(method);
    if (it != get_routes().end())
        candidates = it->second;

    for (const auto& route : candidates) {
        Params params;
        if (route->match(target, params)) {
            co_await route->process(req, res, params);
            co_return;
        }
    }

    res.result(http::status::not_found);
    res.body() = "404 Not Found";
    res.prepare_payload();
    co_return;
}
