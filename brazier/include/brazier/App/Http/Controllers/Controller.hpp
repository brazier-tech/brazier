/**
 * Copyright (c) 2026 Kirill Sergeev, Nikolay Sugonyako, Andrey Agarkov, Hlib Safiannikov.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This source code is licensed under the LGPL license found in the
 * LICENSE file in the root directory of this source tree.
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

        virtual boost::asio::awaitable<void> show(const Request &req, Response &res, const Params &params);

        virtual boost::asio::awaitable<void> store(const Request &req, Response &res, const Params &params);

        virtual boost::asio::awaitable<void> delete_(const Request &req, Response &res, const Params &params);

        virtual boost::asio::awaitable<void> update(const Request &req, Response &res, const Params &params);


        virtual ~Controller() = default;
    };
}
