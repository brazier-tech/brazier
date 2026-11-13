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

#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/use_future.hpp>

#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "../../../include/brazier/Core"
#include "../../../include/brazier/Http"
#include "main.h"
#include "tls_test_client.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

using namespace brazier;

namespace {

    constexpr int kMaxResponseTimeMs = 1500;

    constexpr int kClientTimeoutSec = 10;

    constexpr int kRouteRegistrationDelayMs = 200;

}

class TestController : public brazier::Controller {
public:
    using Request = http::request<http::string_body>;
    using Response = http::response<http::string_body>;

    net::awaitable<void> show(const Request& req, Response& res, const Params& params) override {
        res.result(http::status::ok);
        res.set(http::field::content_type, "text/plain");
        res.body() = "TestController show method called";
        co_return;
    }

    net::awaitable<void> json_response(const Request& req, Response& res,
        const Params& params) {
        res.result(http::status::ok);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"status":"success","message":"JSON response from TestController"})";
        co_return;
    }

    net::awaitable<void> echo_post(const Request& req, Response& res,
        const Params& params) {
        res.result(http::status::ok);
        res.set(http::field::content_type, "application/json");
        res.body() = req.body();
        co_return;
    }
};

template <typename AsyncOp>
auto RunAsync(AsyncOp&& op) {
    net::io_context io;
    auto future = net::co_spawn(io, std::forward<AsyncOp>(op), net::use_future);
    io.run();
    return future.get();
}

bool IsHttpsServerReady() {
    try {
        net::io_context io;
        ssl::context ctx(ssl::context::tls_client);
        ctx.set_verify_mode(ssl::verify_none);

        ssl::stream<tcp::socket> stream(io, ctx);
        tcp::resolver resolver(io);
        auto results = resolver.resolve(https_host_global,
            std::to_string(https_port_global));
        net::connect(stream.next_layer(), results);
        stream.handshake(ssl::stream_base::client);

        boost::system::error_code ec;
        stream.shutdown(ec);

        return true;
    }
    catch (...) {
        return false;
    }
}

std::string BaseUrl() {
    return "https://" + https_host_global + ":" +
        std::to_string(https_port_global);
}

bool TryConnectWithTlsVersion(int version) {
    try {
        net::io_context io;
        ssl::context ctx(ssl::context::tls_client);
        ctx.set_verify_mode(ssl::verify_none);

        switch (version) {
        case TLS1_VERSION:
            ctx.set_options(ssl::context::no_tlsv1_1 |
                ssl::context::no_tlsv1_2 |
                ssl::context::no_tlsv1_3);
            break;
        case TLS1_1_VERSION:
            ctx.set_options(ssl::context::no_tlsv1 |
                ssl::context::no_tlsv1_2 |
                ssl::context::no_tlsv1_3);
            break;
        case TLS1_2_VERSION:
            ctx.set_options(ssl::context::no_tlsv1 |
                ssl::context::no_tlsv1_1 |
                ssl::context::no_tlsv1_3);
            break;
        case TLS1_3_VERSION:
            ctx.set_options(ssl::context::no_tlsv1 |
                ssl::context::no_tlsv1_1 |
                ssl::context::no_tlsv1_2);
            break;
        default:
            return false;
        }

        ssl::stream<tcp::socket> stream(io, ctx);
        tcp::resolver resolver(io);

        auto results = resolver.resolve(https_host_global, std::to_string(https_port_global));
        net::connect(stream.next_layer(), results);
        stream.handshake(ssl::stream_base::client);

        return true;
    }
    catch (...) {
        return false;
    }
}

class HttpsRoutingTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        auto test_controller = std::make_shared<TestController>();

        R(GET, "/test", test_controller, show);
        R(GET, "/test/json", test_controller, json_response);
        R(POST, "/test/echo", test_controller, echo_post);

        std::this_thread::sleep_for(
            std::chrono::milliseconds(kRouteRegistrationDelayMs));
    }

    void SetUp() override {
        if (!IsHttpsServerReady()) {
            GTEST_SKIP() << "HTTPS server is not running on "
                << https_host_global << ":" << https_port_global;
        }
    }

    brazier::HttpClient MakeClient() {
        brazier::HttpClient client;
        client.set_verify_ssl(false);
        client.set_timeout(std::chrono::seconds(kClientTimeoutSec));
        return client;
    }
};

TEST_F(HttpsRoutingTest, AddRouteAndGet) {
    auto client = MakeClient();

    try {
        auto response = RunAsync([&]() -> net::awaitable<brazier::Response> {
            co_return co_await client.get(BaseUrl() + "/test");
            });

        EXPECT_EQ(response.result_int(), 200);
        EXPECT_EQ(response.body(), "TestController show method called");
        EXPECT_EQ(response[http::field::content_type], "text/plain");
    }
    catch (const std::exception& e) {
        FAIL() << "HTTPS request failed: " << e.what();
    }
}

TEST_F(HttpsRoutingTest, JsonResponse) {
    auto client = MakeClient();

    try {
        auto response = RunAsync([&]() -> net::awaitable<brazier::Response> {
            co_return co_await client.get(BaseUrl() + "/test/json");
            });

        EXPECT_EQ(response.result_int(), 200);
        EXPECT_EQ(response[http::field::content_type], "application/json");

        auto json = nlohmann::json::parse(response.body());
        EXPECT_EQ(json["status"], "success");
        EXPECT_EQ(json["message"], "JSON response from TestController");
    }
    catch (const std::exception& e) {
        FAIL() << "HTTPS request failed: " << e.what();
    }
}

TEST_F(HttpsRoutingTest, PostWithBody) {
    auto client = MakeClient();

    nlohmann::json request_body = {
        {"name",  "Test User"},
        {"age",   25},
        {"email", "test@example.com"}
    };

    try {
        auto response = RunAsync([&]() -> net::awaitable<brazier::Response> {
            co_return co_await client.post(BaseUrl() + "/test/echo", request_body);
            });

        EXPECT_EQ(response.result_int(), 200);
        EXPECT_EQ(response[http::field::content_type], "application/json");

        auto response_json = nlohmann::json::parse(response.body());
        EXPECT_EQ(response_json["name"], "Test User");
        EXPECT_EQ(response_json["age"], 25);
        EXPECT_EQ(response_json["email"], "test@example.com");
    }
    catch (const std::exception& e) {
        FAIL() << "HTTPS request failed: " << e.what();
    }
}

TEST_F(HttpsRoutingTest, NotFound) {
    auto client = MakeClient();

    try {
        auto response = RunAsync([&]() -> net::awaitable<brazier::Response> {
            co_return co_await client.get(BaseUrl() + "/nonexistent");
            });

        EXPECT_EQ(response.result_int(), 404);
    }
    catch (const std::exception& e) {
        FAIL() << "HTTPS request failed: " << e.what();
    }
}

TEST_F(HttpsRoutingTest, ResponseTime) {
    auto client = MakeClient();
    auto start = std::chrono::steady_clock::now();

    try {
        auto response = RunAsync([&]() -> net::awaitable<brazier::Response> {
            co_return co_await client.get(BaseUrl() + "/test");
            });

        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        EXPECT_EQ(response.result_int(), 200);
        EXPECT_LT(duration.count(), kMaxResponseTimeMs);
    }
    catch (const std::exception& e) {
        FAIL() << "HTTPS request failed: " << e.what();
    }
}

TEST_F(HttpsRoutingTest, MultipleRequests) {
    constexpr int kParallel = 10;

    net::io_context io;
    std::vector<std::future<brazier::Response>> futures;

    for (int i = 0; i < kParallel; ++i) {
        auto future = net::co_spawn(
            io,
            [&]() -> net::awaitable<brazier::Response> {
                brazier::HttpClient client;
                client.set_verify_ssl(false);
                client.set_timeout(std::chrono::seconds(kClientTimeoutSec));
                co_return co_await client.get(BaseUrl() + "/test");
            },
            net::use_future);
        futures.push_back(std::move(future));
    }

    std::thread io_thread([&io]() { io.run(); });

    bool has_failures = false;
    std::string first_error;
    for (auto& future : futures) {
        try {
            auto response = future.get();
            EXPECT_EQ(response.result_int(), 200);
            EXPECT_EQ(response.body(), "TestController show method called");
        }
        catch (const std::exception& e) {
            if (!has_failures) {
                first_error = e.what();
                has_failures = true;
            }
        }
    }

    io.stop();
    io_thread.join();

    if (has_failures) {
        FAIL() << "HTTPS request failed: " << first_error;
    }
}

TEST_F(HttpsRoutingTest, HstsHeaderPresent) {
    auto client = MakeClient();

    auto response = RunAsync([&]() -> net::awaitable<brazier::Response> {
        co_return co_await client.get(BaseUrl() + "/test");
        });

    EXPECT_EQ(response.result_int(), 200);
    EXPECT_TRUE(response.count(http::field::strict_transport_security));
    EXPECT_NE(response[http::field::strict_transport_security].find("max-age="),
        std::string::npos);
}

TEST_F(HttpsRoutingTest, RejectsTls11) {
    EXPECT_FALSE(TryConnectWithTlsVersion(TLS1_1_VERSION))
        << "Server should reject TLS 1.1";
}

TEST_F(HttpsRoutingTest, AcceptsTls12) {
    EXPECT_TRUE(TryConnectWithTlsVersion(TLS1_2_VERSION))
        << "Server should accept TLS 1.2";
}

TEST_F(HttpsRoutingTest, AcceptsTls13) {
    EXPECT_TRUE(TryConnectWithTlsVersion(TLS1_3_VERSION))
        << "Server should accept TLS 1.3";
}

TEST_F(HttpsRoutingTest, ServesExpectedCertificate) {
    net::io_context io;
    ssl::context ctx(ssl::context::tls_client);
    ctx.set_verify_mode(ssl::verify_none);

    ssl::stream<tcp::socket> stream(io, ctx);
    tcp::resolver resolver(io);
    auto results = resolver.resolve(https_host_global,
        std::to_string(https_port_global));

    ASSERT_NO_THROW(net::connect(stream.next_layer(), results));
    ASSERT_NO_THROW(stream.handshake(ssl::stream_base::client));

    X509* cert = SSL_get_peer_certificate(stream.native_handle());
    ASSERT_NE(cert, nullptr) << "Server did not present a certificate";

    char cn[256] = { 0 };
    X509_NAME* subject = X509_get_subject_name(cert);
    X509_NAME_get_text_by_NID(subject, NID_commonName, cn, sizeof(cn));

    EXPECT_STREQ(cn, "localhost")
        << "Certificate CN mismatch. Got: " << cn;

    X509_free(cert);
}

TEST_F(HttpsRoutingTest, PayloadTooLarge) {
    ASSERT_NE(g_test_https_server, nullptr);
    const std::size_t limit = g_test_https_server->getMaxBodySize();
    const std::size_t body_size = limit * 4;

    tls_test::TlsClient c(https_host_global, https_port_global);
    ASSERT_TRUE(c.connect());

    std::string headers =
        "POST /test/echo HTTP/1.1\r\n"
        "Host: " + https_host_global + "\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(body_size) + "\r\n"
        "Connection: close\r\n"
        "\r\n";

    ASSERT_TRUE(c.send_raw(headers));
    auto res = c.read_response(5);

    if (res.has_value()) {
        EXPECT_EQ(res->result_int(), 413);
    }
    else {
        EXPECT_TRUE(c.is_connection_closed(2))
            << "No response and connection not closed";
    }
}

TEST_F(HttpsRoutingTest, HeaderTooLarge) {
    ASSERT_NE(g_test_https_server, nullptr);
    const std::size_t limit = g_test_https_server->getMaxHeaderSize();
    const std::size_t header_size = limit * 4;

    tls_test::TlsClient c(https_host_global, https_port_global);
    ASSERT_TRUE(c.connect());

    std::string huge(header_size, 'x');
    std::string req =
        "GET /test HTTP/1.1\r\n"
        "Host: " + https_host_global + "\r\n"
        "X-Huge: " + huge + "\r\n"
        "Connection: close\r\n"
        "\r\n";

    ASSERT_TRUE(c.send_raw(req));

    auto res = c.read_response(5);
    bool closed = c.is_connection_closed(3);

    bool acceptable = (res.has_value() &&
        res->result_int() >= 400 &&
        res->result_int() < 500)
        || (!res.has_value() && closed);

    EXPECT_TRUE(acceptable)
        << "Expected 4xx or close, got has_value=" << res.has_value()
        << ", status=" << (res.has_value() ? res->result_int() : 0);
}

TEST_F(HttpsRoutingTest, ConnectionLimit) {
    ASSERT_NE(g_test_https_server, nullptr);
    const int limit = g_test_https_server->getMaxConnections();

    if (limit <= 0 || limit > 100) {
        GTEST_SKIP() << "max_connections=" << limit
            << " is not suitable. Set http.max_connections_testing to 10..100.";
    }

    std::vector<std::unique_ptr<tls_test::TlsClient>> held;
    held.reserve(limit);

    for (int i = 0; i < limit; ++i) {
        auto c = std::make_unique<tls_test::TlsClient>(
            https_host_global, https_port_global);
        ASSERT_TRUE(c->connect())
            << "Handshake #" << i << " failed within limit " << limit;
        held.push_back(std::move(c));
    }

    tls_test::TlsClient extra(https_host_global, https_port_global);
    const bool connected = extra.connect();

    if (connected) {
        EXPECT_TRUE(extra.is_connection_closed(3))
            << "Server accepted connection beyond limit " << limit
            << " and did not close it";
    }
}

TEST_F(HttpsRoutingTest, ConnectionsReleasedAfterClose) {
    ASSERT_NE(g_test_https_server, nullptr);
    const int limit = g_test_https_server->getMaxConnections();

    if (limit <= 0 || limit > 100) {
        GTEST_SKIP() << "max_connections=" << limit
            << " is not suitable. Set http.max_connections_testing to 10..100.";
    }

    const int iterations = limit + 5;

    for (int i = 0; i < iterations; ++i) {
        tls_test::TlsClient c(https_host_global, https_port_global);
        ASSERT_TRUE(c.connect())
            << "Connection #" << i << " rejected — "
            "ConnectionGuard likely not releasing the counter";

        ASSERT_TRUE(c.send_raw(
            "GET /test HTTP/1.1\r\n"
            "Host: " + https_host_global + "\r\n"
            "Connection: close\r\n"
            "\r\n"));

        auto res = c.read_response();
        ASSERT_TRUE(res.has_value()) << "No response at iteration " << i;
        EXPECT_EQ(res->result_int(), 200);
    }
}

TEST_F(HttpsRoutingTest, ConnectionsReleasedAfterProtocolError) {
    ASSERT_NE(g_test_https_server, nullptr);
    const int limit = g_test_https_server->getMaxConnections();

    if (limit <= 0 || limit > 100) {
        GTEST_SKIP() << "max_connections=" << limit
            << " is not suitable. Set http.max_connections_testing to 10..100.";
    }

    const int iterations = limit + 5;

    for (int i = 0; i < iterations; ++i) {
        tls_test::TlsClient c(https_host_global, https_port_global);
        ASSERT_TRUE(c.connect())
            << "Connection #" << i << " rejected after protocol errors — "
            "ConnectionGuard likely not releasing on exceptions";

        c.send_raw("this is not an HTTP request\r\n\r\n");
        c.is_connection_closed(2);
    }
}