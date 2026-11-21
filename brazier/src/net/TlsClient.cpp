/**
 * Copyright (c) 2026 Kirill Sergeev, Nikolay Sugonyako, Andrey Agarkov, Hlib Safiannikov.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This source code is licensed under the LGPL license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../../include/brazier/net/TlsClient.hpp"

#include <utility>

using namespace brazier;

TlsClient::TlsClient(net::any_io_executor ex, std::string host, unsigned short port) : ex_(std::move(ex)), host_(std::move(host)), port_(port) {
    ctx_.set_verify_mode(ssl::verify_none);
}

void TlsClient::set_ca(const std::string& ca_file) {
    ctx_.load_verify_file(ca_file);
    ctx_.set_verify_mode(ssl::verify_peer);
    ctx_.set_verify_callback(ssl::host_name_verification(host_));
}

void TlsClient::set_client_cert(const std::string& cert_file, const std::string& key_file) {
    ctx_.use_certificate_chain_file(cert_file);
    ctx_.use_private_key_file(key_file, ssl::context::pem);
}

awaitable<bool> TlsClient::connect() {
    stream_ = std::make_unique<ssl::stream<beast::tcp_stream>>(ex_, ctx_);

    tcp::resolver resolver(ex_);

    beast::error_code ec;

    auto results = co_await resolver.async_resolve(host_, std::to_string(port_), net::redirect_error(net::use_awaitable, ec));

    if (ec)
        co_return false;

    beast::get_lowest_layer(*stream_).expires_after(kSocketTimeoutSec);

    co_await beast::get_lowest_layer(*stream_).async_connect(results, net::redirect_error(net::use_awaitable, ec));

    if (ec)
        co_return false;

    beast::get_lowest_layer(*stream_).expires_after(kSocketTimeoutSec);

    co_await stream_->async_handshake(ssl::stream_base::client, net::redirect_error(net::use_awaitable, ec));

    if (ec)
        co_return false;

    beast::get_lowest_layer(*stream_).expires_never();

    co_return true;
}

awaitable<bool> TlsClient::send_raw(std::string data) {
    if (!stream_)
        co_return false;

    beast::get_lowest_layer(*stream_).expires_after(kSocketTimeoutSec);

    beast::error_code ec;

    co_await net::async_write(*stream_, net::buffer(data), net::redirect_error(net::use_awaitable, ec));

    co_return !ec;
}

awaitable<bool> TlsClient::is_connection_closed(std::chrono::milliseconds timeout) {
    if (!stream_)
        co_return true;

    beast::get_lowest_layer(*stream_).expires_after(timeout);

    char c = 0;
    beast::error_code ec;

    co_await stream_->async_read_some(net::buffer(&c, 1), net::redirect_error(net::use_awaitable, ec));

    if (!ec) {
        auto dst = buffer_.prepare(1);
        net::buffer_copy(dst, net::buffer(&c, 1));
        buffer_.commit(1);

        co_return false;
    }

    if (ec == beast::error::timeout)
        co_return false;

    co_return true;
}

awaitable<std::optional<http::response<http::string_body>>>
TlsClient::read_response(std::chrono::milliseconds timeout) {
    if (!stream_)
        co_return std::nullopt;

    beast::get_lowest_layer(*stream_).expires_after(timeout);

    http::response<http::string_body> res;
    beast::error_code ec;

    co_await http::async_read(*stream_, buffer_, res, net::redirect_error(net::use_awaitable, ec));

    if (ec)
        co_return std::nullopt;

    co_return res;
}