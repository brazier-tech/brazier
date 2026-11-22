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
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with brazier; if not, see <https://www.gnu.org/licenses/>.
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
