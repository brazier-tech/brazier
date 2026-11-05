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

#include "../../include/brazier/net/TlsClient.hpp"

brazier::TlsClient::TlsClient(const std::string& host, unsigned short port, bool with_client_cert, const std::string& cert, const std::string& key)
: host_(host), port_(port), ctx(ssl::context::tls_client) {
    ctx.set_verify_mode(ssl::verify_none);

    if (with_client_cert) {
        ctx.use_certificate_chain_file(cert);
        ctx.use_private_key_file(key, ssl::context::pem);
        tcp::resolver resolver(io);
        auto results = resolver.resolve(host_, std::to_string(port_));

        beast::get_lowest_layer(*stream).expires_after(std::chrono::seconds(kSocketTimeoutSec));
        beast::get_lowest_layer(*stream).connect(results);
        beast::get_lowest_layer(*stream).expires_never();
        stream->handshake(ssl::stream_base::client);
    }
}

bool brazier::TlsClient::send_raw(const std::string& data) {
    beast::get_lowest_layer(*stream).expires_after(std::chrono::seconds(kSocketTimeoutSec));
    net::write(*stream, net::buffer(data));

    return true;
}

std::optional<http::response<http::string_body>> brazier::TlsClient::read_response(int timeout_sec) {
    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    beast::get_lowest_layer(*stream).expires_after(std::chrono::seconds(timeout_sec));
    http::read(*stream, buffer, res);

    return res;
}

bool brazier::TlsClient::is_connection_closed(int timeout_sec) {
    if (!stream)
        return true;

    beast::get_lowest_layer(*stream).expires_after(std::chrono::seconds(timeout_sec));
    char c;
    beast::error_code ec;
    auto n = stream->read_some(net::buffer(&c, 1), ec);
    
    if (ec == net::error::eof || ec == net::error::connection_reset ||
        ec == ssl::error::stream_truncated || ec == beast::error::timeout) {
        return true;
    }
    
    if (ec == net::error::timed_out)
        return false;
    
    if (!ec && n > 0)
        return false;
    
    return true;
}