/**
 * Copyright (c) 2026 Kirill Sergeev, Nikolay Sugonyako, Andrey Agarkov, Hlib Safiannikov.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This source code is licensed under the LGPL license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "FusedOnion.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace brazier {

    // CORS is ordinary Fused Onion middleware. Register it on both the
    // resource route and an OPTIONS route when preflight requests are needed.
    inline Middleware corsMiddleware(std::vector<std::string> allowed_origins = {}) {
        return [allowed_origins = std::move(allowed_origins)]
            (Context& context, Next next) -> Awaitable {

            const auto origin_header =
                context.request[boost::beast::http::field::origin];
            const std::string origin = origin_header.empty()
                ? std::string{}
                : std::string(origin_header.data(), origin_header.size());

            // Requests without an Origin header are not CORS requests.
            if (origin.empty()) {
                co_await next();
                co_return;
            }

            const bool allowed = allowed_origins.empty() ||
                std::find(allowed_origins.begin(), allowed_origins.end(), origin)
                    != allowed_origins.end();

            if (!allowed) {
                context.response.result(boost::beast::http::status::forbidden);
                context.response.body() = "CORS: Origin not allowed";
                context.response.set(
                    boost::beast::http::field::access_control_allow_origin,
                    "null"
                );
                co_return;
            }

            context.response.set(
                boost::beast::http::field::access_control_allow_origin,
                origin
            );
            context.response.set(
                boost::beast::http::field::access_control_allow_credentials,
                "true"
            );
            context.response.set(
                boost::beast::http::field::access_control_allow_methods,
                "GET, HEAD, POST, PUT, PATCH, DELETE, OPTIONS"
            );
            context.response.set(
                boost::beast::http::field::access_control_allow_headers,
                "Content-Type, Authorization"
            );
            context.response.set(
                boost::beast::http::field::access_control_expose_headers,
                "Set-Cookie"
            );
            context.response.set(
                boost::beast::http::field::vary,
                "Origin"
            );

            // The router matches methods before the route-local pipeline runs, so an OPTIONS route must also be registered for preflight.
            if (context.request.method() == boost::beast::http::verb::options) {
                context.response.result(boost::beast::http::status::no_content);
                context.response.body().clear();
                co_return;
            }

            co_await next();
        };
    }

}
