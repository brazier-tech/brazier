#include <gtest/gtest.h>
#include "../../../include/brazier/net/TlsClient.hpp"
#include "main.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include <optional>
#include <string>
#include <utility>

std::string ca_cert() {
    return brazier::ConfigManager::getGlobal().get("https_server.tls.ca_file", std::string("certs/ca.crt"));
}

class TlsClientTest : public ::testing::Test {
protected:
    boost::asio::io_context io_;

    brazier::TlsClient make_client(const std::string& host, int port) {
        return brazier::TlsClient(io_.get_executor(), host, port);
    }

    brazier::TlsClient make_client() {
        return make_client(https_host_global, https_port_global);
    }

    template<typename T, typename F>
    T run(F&& coro) {
        auto fut = boost::asio::co_spawn(io_, std::forward<F>(coro), boost::asio::use_future);
        io_.run();

        return fut.get();
    }
};

TEST_F(TlsClientTest, ConnectToInvalidHost) {
    EXPECT_FALSE(run<bool>([this]() -> boost::asio::awaitable<bool> {
        auto client = make_client("invalid_host", 12345);

        co_return co_await client.connect();
        }));
}

TEST_F(TlsClientTest, ConnectToInvalidPort) {
    EXPECT_FALSE(run<bool>([this]() -> boost::asio::awaitable<bool> {
        auto client = make_client(https_host_global, 0);

        co_return co_await client.connect();
        }));
}

TEST_F(TlsClientTest, ConnectToValidHostAndPort) {
    EXPECT_TRUE(run<bool>([this]() -> boost::asio::awaitable<bool> {
        auto client = make_client();

        co_return co_await client.connect();
        }));
}

TEST_F(TlsClientTest, ConnectWithCaVerifiesServer) {
    EXPECT_TRUE(run<bool>([this]() -> boost::asio::awaitable<bool> {
        auto client = make_client();
        client.set_ca(ca_cert());

        co_return co_await client.connect();
        }));
}

TEST_F(TlsClientTest, ConnectWithInvalidCaFails) {
    auto client = make_client();
    EXPECT_THROW(client.set_ca("app/certs/invalid_ca.crt"), std::exception);
}

TEST_F(TlsClientTest, SendRawData) {
    EXPECT_TRUE(run<bool>([this]() -> boost::asio::awaitable<bool> {
        auto client = make_client();
        client.set_ca(ca_cert());

        if (!co_await client.connect())
            co_return false;

        std::string request = "GET / HTTP/1.1\r\nHost: " + std::string(https_host_global) + "\r\nConnection: close\r\n\r\n";

        co_return co_await client.send_raw(std::move(request));
        }));
}

TEST_F(TlsClientTest, ReadResponse) {
    auto response = run<std::optional<brazier::http::response<brazier::http::string_body>>>(
        [this]() -> boost::asio::awaitable<std::optional<brazier::http::response<brazier::http::string_body>>> {
            auto client = make_client();
            client.set_ca(ca_cert());

            if (!co_await client.connect())
                co_return std::nullopt;

            std::string request = "GET / HTTP/1.1\r\nHost: " + std::string(https_host_global) + "\r\nConnection: close\r\n\r\n";

            if (!co_await client.send_raw(std::move(request)))
                co_return std::nullopt;

            co_return co_await client.read_response();
        });

    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->result_int(), 404);
}