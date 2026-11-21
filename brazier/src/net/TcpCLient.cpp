/**
 * Copyright (c) 2026 Kirill Sergeev, Nikolay Sugonyako, Andrey Agarkov, Hlib Safiannikov.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This source code is licensed under the LGPL license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../../include/brazier/net/TcpClient.hpp"

#include <utility>

using namespace brazier;

TcpClient::TcpClient(net::any_io_executor ex, std::string host, uint16_t port, std::chrono::microseconds timeout): timeout_(timeout), ex_(std::move(ex)), host_(std::move(host)), port_(port), socket_(ex_) {}

awaitable<bool> TcpClient::connect() {
    tcp::resolver resolver(ex_);
    net::steady_timer timer(ex_);
    timer.expires_after(timeout_);

    timer.async_wait([&](const boost::system::error_code& e) {
        if (!e) {
            boost::system::error_code ignored;

            socket_.close(ignored);
            resolver.cancel();
        }
        });

    boost::system::error_code ec;

    auto results = co_await resolver.async_resolve(host_, std::to_string(port_), net::redirect_error(net::use_awaitable, ec));

    if (!ec) {
        co_await net::async_connect(socket_, results, net::redirect_error(net::use_awaitable, ec));
    }

    timer.cancel();
    co_return !ec;
}

awaitable<bool> TcpClient::send_raw(std::string data) {
    if (!socket_.is_open())
        co_return false;

    boost::system::error_code ec;

    co_await net::async_write(socket_, net::buffer(data), net::redirect_error(net::use_awaitable, ec));

    co_return !ec;
}

awaitable<bool> TcpClient::is_connection_closed(std::chrono::milliseconds timeout) {
    if (!socket_.is_open())
        co_return true;

    net::steady_timer timer(ex_);
    timer.expires_after(timeout);
    timer.async_wait([this](const boost::system::error_code& e) {
        if (!e) {
            boost::system::error_code ignored;
            socket_.cancel(ignored);
        }
        });

    char c = 0;
    boost::system::error_code ec;
    co_await socket_.async_read_some(net::buffer(&c, 1), net::redirect_error(net::use_awaitable, ec));

    timer.cancel();

    if (!ec) {
        auto dst = buffer_.prepare(1);
        net::buffer_copy(dst, net::buffer(&c, 1));
        buffer_.commit(1);

        co_return false;
    }

    if (ec == net::error::operation_aborted)
        co_return false;

    co_return true;
}

awaitable<std::optional<http::response<http::string_body>>>
TcpClient::read_response(std::chrono::milliseconds timeout) {
    if (!socket_.is_open())
        co_return std::nullopt;

    net::steady_timer timer(ex_);
    timer.expires_after(timeout);
    timer.async_wait([this](const boost::system::error_code& e) {
        if (!e) {
            boost::system::error_code ignored;
            socket_.close(ignored);
        }
        });

    http::response<http::string_body> res;
    boost::system::error_code ec;

    co_await http::async_read(socket_, buffer_, res, net::redirect_error(net::use_awaitable, ec));

    timer.cancel();

    if (ec)
        co_return std::nullopt;

    co_return res;
}