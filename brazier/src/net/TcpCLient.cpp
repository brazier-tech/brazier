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

#include "../../include/brazier/net/TcpClient.hpp"

brazier::TcpClient::TcpClient(std::string host, uint16_t port, std::chrono::microseconds timeout) 
    : host_(std::move(host)), port_(port), timeout_(timeout), socket_(io_) {}

bool brazier::TcpClient::connect() {
    tcp::resolver resolver(io_);
    boost::system::error_code ec;

    auto endpoints = resolver.resolve(host_, std::to_string(port_), ec);

    if (ec)
        throw boost::system::system_error(ec, "resolve failed");

    auto deadline = std::chrono::steady_clock::now() + timeout_;

    for (const auto& endpoint : endpoints) {
        if (std::chrono::steady_clock::now() >= deadline)
            break;

        socket_.close(ec);
        ec.clear();

        bool done = false;
        boost::system::error_code op_ec;

        socket_.async_connect(endpoint, [&](const boost::system::error_code& error) {
            op_ec = error;
            done = true;
            });

        while (!done) {
            if (std::chrono::steady_clock::now() >= deadline) {
                socket_.close();
                io_.restart();

                throw timeout_exception("connect timeout to " + host_ + ":" + std::to_string(port_));
            }

            io_.run_one();
        }

        if (!op_ec)
            return true;

        ec = op_ec;
    }

    throw boost::system::system_error(ec ? ec : boost::asio::error::timed_out, "connect failed to " + host_ + ":" + std::to_string(port_));
}

bool brazier::TcpClient::send_raw(const std::string& data) {
    if (!socket_.is_open())
        return false;

    net::write(socket_, net::buffer(data));
    return true;
}

std::optional<brazier::http::response<brazier::http::string_body>> brazier::TcpClient::read_response(int timeout_sec) {
    if (!socket_.is_open())
        return std::nullopt;

    socket_.non_blocking(true);
    boost::beast::flat_buffer buffer;
    http::response<http::string_body> res;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_sec);

    while (std::chrono::steady_clock::now() < deadline) {
        boost::system::error_code ec;
        http::read(socket_, buffer, res, ec);

        if (!ec)
            return res;

        if (ec != net::error::would_block && ec != net::error::try_again)
            return std::nullopt;
    }

    return std::nullopt;
}

bool brazier::TcpClient::is_connection_closed(int wait_sec) {
    if (!socket_.is_open())
        return true;

    socket_.non_blocking(true);
    char c = 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(wait_sec);

    while (std::chrono::steady_clock::now() < deadline) {
        boost::system::error_code ec;
        socket_.read_some(net::buffer(&c, 1), ec);

        if (ec == net::error::eof || ec == net::error::connection_reset)
            return true;

        if (ec != net::error::would_block && ec != net::error::try_again)
            return false;

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return false;
}