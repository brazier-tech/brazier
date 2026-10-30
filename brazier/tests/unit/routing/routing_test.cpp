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

#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <chrono>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "../../../include/brazier/Core"
#include "../../../include/brazier/Http"

#include "main.h"
#include "tcp_test_client.hpp"

namespace beast = boost::beast;
namespace http = beast::http;

using namespace brazier;

class TestController : public brazier::Controller {
public:
    using Request = http::request<http::string_body>;
    using Response = http::response<http::string_body>;

    boost::asio::awaitable<void> show(const Request& req, Response& res, const Params& params) override {
        res.result(http::status::ok);
        res.set(http::field::content_type, "text/plain");
        res.body() = "TestController show method called";
        co_return;
    }

    boost::asio::awaitable<void> json_response(const Request& req, Response& res, const Params& params) {
        res.result(http::status::ok);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"status":"success","message":"JSON response from TestController"})";
        co_return;
    }

    boost::asio::awaitable<void> echo_post(const Request& req, Response& res, const Params& params) {
        res.result(http::status::ok);
        res.set(http::field::content_type, "application/json");
        res.body() = req.body();
        co_return;
    }
};

template<typename AsyncOp>
auto RunAsync(AsyncOp&& op) {
    boost::asio::io_context io_context;
    auto future = boost::asio::co_spawn(
        io_context,
        std::forward<AsyncOp>(op),
        boost::asio::use_future
    );
    std::thread([&io_context]() { io_context.run(); }).detach();
    return future.get();
}

bool IsServerRunning() {
    try {
        net::io_context io;
        beast::tcp_stream stream(io);
        tcp::resolver resolver(io);
        auto results = resolver.resolve(host_global, std::to_string(port_global));

        stream.expires_after(std::chrono::milliseconds(500));
        stream.connect(results);
        return true;
    }
    catch (...) {
        return false;
    }
}

std::string baseUrl() {
    return "http://" + host_global + ":" + std::to_string(port_global);
}

class RoutingTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        server_available_ = IsServerRunning();
        http_url_base = baseUrl();    
    }

    void SetUp() override {
        if (!server_available_) {
            GTEST_SKIP() << "Server is not running";
        }
    }

    static bool server_available_;
    static std::string http_url_base;
};

bool RoutingTest::server_available_ = false;
std::string RoutingTest::http_url_base = "";

TEST_F(RoutingTest, AddRouteAndGet) {
    auto test_controller = std::make_shared<TestController>();

    R(GET, "/test", test_controller, show);
    R(GET, "/test/json", test_controller, json_response);
    R(POST, "/test/echo", test_controller, echo_post);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    brazier::HttpClient client;
    client.set_verify_ssl(false);
    client.set_timeout(std::chrono::seconds(10));

    try {
        auto response = RunAsync([&]() -> boost::asio::awaitable<brazier::Response> {
            co_return co_await client.get(http_url_base + "/test");
            });

        EXPECT_EQ(response.result_int(), 200);
        EXPECT_EQ(response.body(), "TestController show method called");
        EXPECT_EQ(response[boost::beast::http::field::content_type], "text/plain");
    }
    catch (const std::exception& e) {
        FAIL() << "Request failed: " << e.what();
    }
}

TEST_F(RoutingTest, JsonResponse) {
    brazier::HttpClient client;
    client.set_verify_ssl(false);
    client.set_timeout(std::chrono::seconds(5));

    try {
        auto response = RunAsync([&]() -> boost::asio::awaitable<brazier::Response> {
            co_return co_await client.get(http_url_base + "/test/json");
            });

        EXPECT_EQ(response.result_int(), 200);
        EXPECT_EQ(response[boost::beast::http::field::content_type], "application/json");

        auto json = nlohmann::json::parse(response.body());
        EXPECT_EQ(json["status"], "success");
        EXPECT_EQ(json["message"], "JSON response from TestController");
    }
    catch (const std::exception& e) {
        FAIL() << "Request failed: " << e.what();
    }
}

TEST_F(RoutingTest, PostWithBody) {
    brazier::HttpClient client;
    client.set_verify_ssl(false);
    client.set_timeout(std::chrono::seconds(5));

    nlohmann::json request_body = {
        {"name", "Test User"},
        {"age", 25},
        {"email", "test@example.com"}
    };

    try {
        auto response = RunAsync([&]() -> boost::asio::awaitable<brazier::Response> {
            co_return co_await client.post(http_url_base + "/test/echo", request_body);
            });

        EXPECT_EQ(response.result_int(), 200);
        EXPECT_EQ(response[boost::beast::http::field::content_type], "application/json");

        auto response_json = nlohmann::json::parse(response.body());
        EXPECT_EQ(response_json["name"], "Test User");
        EXPECT_EQ(response_json["age"], 25);
        EXPECT_EQ(response_json["email"], "test@example.com");
    }
    catch (const std::exception& e) {
        FAIL() << "Request failed: " << e.what();
    }
}

TEST_F(RoutingTest, NotFound) {
    brazier::HttpClient client;
    client.set_verify_ssl(false);
    client.set_timeout(std::chrono::seconds(5));

    try {
        auto response = RunAsync([&]() -> boost::asio::awaitable<brazier::Response> {
            co_return co_await client.get(http_url_base + "/nonexistent");
            });

        EXPECT_EQ(response.result_int(), 404);
    }
    catch (const std::exception& e) {
        FAIL() << "Request failed: " << e.what();
    }
}

TEST_F(RoutingTest, ResponseTime) {
    brazier::HttpClient client;
    client.set_verify_ssl(false);
    client.set_timeout(std::chrono::seconds(5));

    auto start = std::chrono::steady_clock::now();

    try {
        auto response = RunAsync([&]() -> boost::asio::awaitable<brazier::Response> {
            co_return co_await client.get(http_url_base + "/test");
            });

        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        EXPECT_EQ(response.result_int(), 200);
        EXPECT_LT(duration.count(), 1000);
    }
    catch (const std::exception& e) {
        FAIL() << "Request failed: " << e.what();
    }
}

TEST_F(RoutingTest, MultipleRequests) {
    brazier::HttpClient client;
    client.set_verify_ssl(false);
    client.set_timeout(std::chrono::seconds(5));

    boost::asio::io_context io_context;
    std::vector<std::future<brazier::Response>> futures;

    for (int i = 0; i < 10; ++i) {
        auto future = boost::asio::co_spawn(
            io_context,
            [&]() -> boost::asio::awaitable<brazier::Response> {
                co_return co_await client.get(http_url_base + "/test");
            },
            boost::asio::use_future
        );
        futures.push_back(std::move(future));
    }

    std::thread io_thread([&io_context]() { io_context.run(); });

    for (auto& future : futures) {
        try {
            auto response = future.get();
            EXPECT_EQ(response.result_int(), 200);
            EXPECT_EQ(response.body(), "TestController show method called");
        }
        catch (const std::exception& e) {
            FAIL() << "Request failed: " << e.what();
        }
    }

    io_context.stop();
    io_thread.join();
}

TEST_F(RoutingTest, PayloadTooLarge) {
    ASSERT_NE(g_test_server, nullptr);
    const std::size_t limit = g_test_server->getMaxBodySize();
    const std::size_t body_size = limit * 4;

    tcp_test::TcpClient c(host_global, port_global);
    ASSERT_TRUE(c.connect());

    std::string headers =
        "POST /ext/test HTTP/1.1\r\n"
        "Host: " + host_global + "\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(body_size) + "\r\n"
        "Connection: close\r\n\r\n";

    ASSERT_TRUE(c.send_raw(headers));
    auto res = c.read_response(5);

    if (res.has_value()) {
        EXPECT_EQ(res->result_int(), 413);
    }
    else {
        EXPECT_TRUE(c.is_connection_closed(2)) << "No response and connection not closed";
    }
}