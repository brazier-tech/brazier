/**
 * Copyright (c) 2026 Kirill Sergeev, Nikolay Sugonyako, Andrey Agarkov, Hlib Safiannikov.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This source code is licensed under the LGPL license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "FusedOnion.hpp"

#include <functional>
#include <utility>

namespace brazier {

    // Adapt an application-provided authentication check into Fused Onion middleware.
    inline Middleware authMiddleware(
        std::function<bool(const FusedOnion::Request&)> authenticate
    ) {
        return [authenticate = std::move(authenticate)]
            (Context& context, Next next) -> Awaitable {

            if (!authenticate || !authenticate(context.request)) {
                context.response.result(
                    boost::beast::http::status::unauthorized
                );
                co_return;
            }

            co_await next();
        };
    }

}
