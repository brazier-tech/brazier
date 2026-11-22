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

#pragma once
#pragma execution_character_set("utf-8")

#include <boost/beast/http.hpp>
#include <boost/asio/awaitable.hpp>
#include <unordered_map>

namespace beast = boost::beast;
namespace http = beast::http;

using Params = std::unordered_map<std::string, std::string>;

namespace brazier {

    class Controller {
    public:
        using Request = http::request<http::string_body>;
        using Response = http::response<http::string_body>;

        virtual boost::asio::awaitable<void> show(const Request& req, Response& res, const Params& params);
        virtual boost::asio::awaitable<void> store(const Request& req, Response& res, const Params& params);
        virtual boost::asio::awaitable<void> delete_(const Request& req, Response& res, const Params& params);
        virtual boost::asio::awaitable<void> update(const Request& req, Response& res, const Params& params);


        virtual ~Controller() = default;
    };

}