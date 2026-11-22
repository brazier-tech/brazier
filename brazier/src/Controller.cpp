/*
 * Copyright (c) 2026 Kirill Sergeev, Nikolay Sugonyako, Andrey Agarkov, Gleb Safiannikov
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This file is part of brazier.
 *
 * brazier is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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
