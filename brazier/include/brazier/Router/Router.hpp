/**
 * Copyright (c) 2026 Kirill Sergeev, Nikolay Sugonyako, Andrey Agarkov, Hlib Safiannikov.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This source code is licensed under the LGPL license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <iostream>
#include <boost/beast/http.hpp>
#include <unordered_map>
#include <memory>
#include <functional>
#include <regex>
#include <vector>
#include <shared_mutex>
#include <mutex>

#include "Route.hpp"

namespace brazier {
    namespace beast = boost::beast;
    namespace http = beast::http;

    class Router {
        using Request = http::request<http::string_body>;
        using Response = http::response<http::string_body>;
        using Params = std::unordered_map<std::string, std::string>;
        using RouteList = std::vector<std::shared_ptr<Route> >;

        static std::unordered_map<http::verb, RouteList> &get_routes();

        static std::shared_mutex &get_routes_mutex();

    public:
        static std::shared_ptr<Route> add(http::verb method, const std::string &path, FusedOnion::Controller handler);

        static boost::asio::awaitable<void> handle_request(const Request &req, Response &res);
    };
}
