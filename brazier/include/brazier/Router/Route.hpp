/**
 * Copyright (c) 2026 Kirill Sergeev, Nikolay Sugonyako, Andrey Agarkov, Hlib Safiannikov.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This source code is licensed under the LGPL license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>
#include <regex>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <functional>
#include <boost/asio/awaitable.hpp>
#include <boost/beast/http.hpp>

#include "../App/Http/Middleware/FusedOnion.hpp"

namespace beast = boost::beast;
namespace http = beast::http;

using Request = http::request<http::string_body>;
using Response = http::response<http::string_body>;
using Params = std::unordered_map<std::string, std::string>;

namespace brazier {

    class Route {
        FusedOnion pipeline_;
        std::regex pathRegex_;
        std::vector<std::string> paramNames_;
        std::string originalPath_;

    public:
        // Route-scoped interceptor managers. Each manager shares the same registry and publishes a new, consistent fused snapshot on change.
        FusedOnion::RequestManager request;
        FusedOnion::MiddlewareManager middleware;
        FusedOnion::ResponseManager response;
        FusedOnion::ErrorManager error;

        Route(const std::string& path, FusedOnion::Controller handler);

        bool match(const std::string& target, Params& outParams) const;
        boost::asio::awaitable<void> process(
            const Request& req,
            Response& res,
            const Params& params
        ) const;
        const std::string& path() const;

    private:
        static std::regex convertPathToRegex(const std::string& path);
        static std::vector<std::string> extractParamNames(const std::string& path);
    };
}
