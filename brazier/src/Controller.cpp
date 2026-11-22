/**
 * Copyright (c) 2026 Kirill Sergeev, Nikolay Sugonyako, Andrey Agarkov, Hlib Safiannikov.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This source code is licensed under the LGPL license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../include/brazier/App/Http/Controllers/Controller.hpp"

using namespace brazier;

boost::asio::awaitable<void> Controller::show(const Request& req, Response& res, const Params& params) {
    res.result(http::status::method_not_allowed);
    co_return;
}

boost::asio::awaitable<void> Controller::store(const Request& req, Response& res, const Params& params) {
    res.result(http::status::method_not_allowed);
    co_return;
}

boost::asio::awaitable<void> Controller::delete_(const Request& req, Response& res, const Params& params) {
    res.result(http::status::method_not_allowed);
    co_return;
}

boost::asio::awaitable<void> Controller::update(const Request& req, Response& res, const Params& params) {
    res.result(http::status::method_not_allowed);
    co_return;
}
