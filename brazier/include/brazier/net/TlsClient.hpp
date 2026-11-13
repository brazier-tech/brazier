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

#include <chrono>
#include <fstream>
#include <memory>
#include <optional>
#include <string>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>

#include <openssl/ssl.h>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

namespace brazier {

    class TlsClient {
    public:
        TlsClient(const std::string& host, unsigned short port);

        void set_ca(const std::string& ca_file);
        void set_client_cert(const std::string& cert_file, const std::string& key_file);

        bool connect();
        bool send_raw(const std::string& data);
        bool is_connection_closed(std::chrono::milliseconds timeout = std::chrono::seconds(2));
        
        std::optional<http::response<http::string_body>> read_response(std::chrono::milliseconds timeout = std::chrono::seconds(5));

    private:
        std::string host_;
        unsigned short port_;
        net::io_context io;
        ssl::context ctx{ ssl::context::tls_client };
        std::unique_ptr<ssl::stream<beast::tcp_stream>> stream;
		std::chrono::milliseconds kSocketTimeoutSec = std::chrono::seconds(5);
    };

}