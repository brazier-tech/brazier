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

brazier::TlsClient::TlsClient(const std::string& host, unsigned short port)
: host_(host), port_(port), ctx(ssl::context::tls_client) {
    ctx.set_verify_mode(ssl::verify_none);
}

void brazier::TlsClient::set_ca(const std::string& ca_file) {
    ctx.load_verify_file(ca_file);
    ctx.set_verify_mode(ssl::verify_peer);
}

void brazier::TlsClient::set_client_cert(const std::string& cert_file,
    const std::string& key_file) {
    ctx.use_certificate_chain_file(cert_file);
    ctx.use_private_key_file(key_file, ssl::context::pem);
}

bool brazier::TlsClient::connect() {
    stream = std::make_unique<ssl::stream<beast::tcp_stream>>(io, ctx);

    tcp::resolver resolver(io);
    auto results = resolver.resolve(host_, std::to_string(port_));

    beast::get_lowest_layer(*stream).expires_after(kSocketTimeoutSec);
    beast::get_lowest_layer(*stream).connect(results);
    beast::get_lowest_layer(*stream).expires_never();
    stream->handshake(ssl::stream_base::client);
    return true;
}

bool brazier::TlsClient::is_connection_closed(std::chrono::milliseconds timeout) {
    if (!stream) 
        return true;

    beast::get_lowest_layer(*stream).expires_after(timeout);
    char c;
    beast::error_code ec;
    stream->read_some(net::buffer(&c, 1), ec);

    if (!ec) 
        return false;

    if (ec == beast::error::timeout)
        return false;
    
    return true;
}