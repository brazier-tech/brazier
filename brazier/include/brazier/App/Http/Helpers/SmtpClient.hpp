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

#include <string>
#include <boost/beast/core.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/awaitable.hpp>
#include "../../../vendor/Handlers/ENV.hpp"
#include <brazier/log/Logger.hpp>
#include "../../../Engine.hpp"

namespace brazier
{
    namespace beast = boost::beast;
    namespace net = boost::asio;
    namespace ssl = net::ssl;
    using tcp = net::ip::tcp;

    class SmtpClient {
    public:
        explicit SmtpClient();
        ~SmtpClient();

        net::awaitable<int> send(const std::string& recipient, const std::string& message, const std::string& sender_name = "");

    private:
        std::string host;
        std::string smtp_host;
        std::string port;
        std::string username;
        std::string password;

        net::io_context& io_ctx_;
        ssl::context ssl_ctx_;

        net::awaitable<void> write_line(ssl::stream<tcp::socket>& socket, const std::string& line);

        net::awaitable<void> read_response(ssl::stream<tcp::socket>& socket, int expected_code);
    };
}