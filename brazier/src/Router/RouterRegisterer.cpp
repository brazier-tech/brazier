/**
 * Copyright (c) 2026 Kirill Sergeev, Nikolay Sugonyako, Andrey Agarkov, Hlib Safiannikov.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This source code is licensed under the LGPL license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../../include/brazier/Router/RouterRegisterer.hpp"

using namespace brazier;

void RouterRegisterer::init(boost::asio::io_context& io) {
#define R(method, path, controller, handler) \
            Router::add(method, path, [controller](const Request& req, Response& res, const Params& params) -> boost::asio::awaitable<void> { \
                co_await controller->handler(req, res, params); \
            })

#undef R
}