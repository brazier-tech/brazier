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
#include <chrono>
#include <regex>
#include <iomanip>
#include <sstream>
#include <cctype>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/version.hpp>
#include <nlohmann/json.hpp>
#include <brazier/log/Logger.hpp>

namespace brazier {

    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace net = boost::asio;
    namespace ssl = net::ssl;
    using tcp = net::ip::tcp;
    using Response = http::response<http::string_body>;
    using Request = http::request<http::string_body>;

    class HttpClient {
    public:
        using json = nlohmann::json;

        HttpClient();
        HttpClient(const HttpClient&) = delete;
        HttpClient& operator=(const HttpClient&) = delete;
        HttpClient(HttpClient&&) = default;
        HttpClient& operator=(HttpClient&&) = default;

        ~HttpClient() = default;


        net::awaitable<Response> get(const std::string& url, const json& body = json{});
        net::awaitable<Response> post(const std::string& url, const json& body);
        net::awaitable<Response> put(const std::string& url, const json& body);
        net::awaitable<Response> del(const std::string& url, const json& body = json{});

        void set_timeout(const std::chrono::milliseconds& timeout);
        bool is_success(const Response& res) const;

        void set_verify_ssl(bool verify);

    private:
        ssl::context ctx_;
        std::chrono::milliseconds timeout_ = std::chrono::seconds(30);
        bool verify_ssl_ = true;

        struct UrlParts {
            std::string protocol;
            std::string host;
            std::string port;
            std::string path;
            std::string query;
        };

        UrlParts parse_url(const std::string& url);

        net::awaitable<Response> send_request(const std::string& url, http::verb method, const json& body);
        net::awaitable<Response> send_http_request(const UrlParts& url_parts, http::verb method, const json& body);
        net::awaitable<Response> send_https_request(const UrlParts& url_parts, http::verb method, const json& body);

        void setup_common_headers(Request& req, const std::string& host);
        std::string json_to_query_string(const json& j);
        std::string encode_url(const std::string& value);

    };

}