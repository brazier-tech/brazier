/**
 * Copyright (c) 2026 Kirill Sergeev, Nikolay Sugonyako, Andrey Agarkov, Hlib Safiannikov.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This source code is licensed under the LGPL license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include <boost/beast/http.hpp>

namespace brazier {

    namespace beast = boost::beast;
    namespace http = beast::http;

    class Middleware {
    public:
        using Request = http::request<http::string_body>;
        using Response = http::response<http::string_body>;

        virtual bool handle(Request& req, Response& res) = 0;
        virtual ~Middleware() = default;

        virtual void setCors(const Request& req, Response& res);
    };
}