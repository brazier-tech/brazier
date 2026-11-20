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
    net::steady_timer timer(io_);

    auto state = std::make_shared<State>();

    timer.expires_after(timeout_);
    timer.async_wait([state, this, &resolver](const boost::system::error_code& e) {
        if (e == net::error::operation_aborted) 
            return;

        if (state->st != State::pending)        
            return;

        state->st = State::timed_out;
        boost::system::error_code ignored;
        socket_.close(ignored);
        resolver.cancel();
        });

	async_resolve(resolver, state);

    while (state->st == State::pending)
        io_.run_one();

    boost::system::error_code ignored;
    timer.cancel();

    if (state->st == State::timed_out)
        throw timeout_exception("connect timeout to " + host_ + ":" + std::to_string(port_));

    if (state->st == State::failed)
        throw boost::system::system_error(state->ec, "connect failed to " + host_ + ":" + std::to_string(port_));

    return true;
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

    {
        boost::system::error_code ec;
        socket_.non_blocking(false, ec);
    }

    auto state = std::make_shared<State>();
    net::steady_timer timer(io_);

    timer.expires_after(std::chrono::seconds(timeout_sec));
    timer.async_wait([state, this](const boost::system::error_code& e) {
        if (e == net::error::operation_aborted) 
            return;

        if (state->st != State::pending)        
            return;

        state->st = State::timed_out;
        boost::system::error_code ignored;
        socket_.close(ignored);
        });

    http::async_read(socket_, state->buffer, state->res,
        [state](const boost::system::error_code& e, std::size_t) {
            if (state->st != State::pending)        
                return;

            if (e) { 
                state->st = State::failed; 
                state->ec = e; 
            }
            else
                state->st = State::done; 
        });

    while (state->st == State::pending)
        io_.run_one();

    timer.cancel();

    if (state->st == State::done)
        return std::move(state->res);

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

void brazier::TcpClient::async_resolve(tcp::resolver& resolver, std::shared_ptr<State> state) {
    resolver.async_resolve(host_, std::to_string(port_), 
        [state, this](const boost::system::error_code& ec, tcp::resolver::results_type results) 
        {
            if (state->st != State::pending) 
                return;

            if (ec) { 
                state->st = State::failed; 
                state->ec = ec; 
                return; 
            }

            net::async_connect(socket_, results,
                [state](const boost::system::error_code& e2, const tcp::endpoint&) {
                    if (state->st != State::pending) 
                        return;

                    if (e2) { 
                        state->st = State::failed; 
                        state->ec = e2; 
                    }
                    else { 
                        state->st = State::connected; 
                    }
                });
        });
}

brazier::TcpClient::timeout_exception::timeout_exception(const std::string& _Message) : base(_Message) {}
brazier::TcpClient::timeout_exception::timeout_exception(const char* _Message) : base(_Message) {}
