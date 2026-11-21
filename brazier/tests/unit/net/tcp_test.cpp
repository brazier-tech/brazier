#include <gtest/gtest.h>
#include "../../../include/brazier/net/TcpClient.hpp"
#include "main.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include <optional>
#include <string>
#include <utility>

class TcpClientTest : public ::testing::Test {
protected:
    boost::asio::io_context io_;

    const int kTestPort = port_global;
    const std::string kTestHost = host_global;

    brazier::TcpClient make_client(const std::string& host, uint16_t port) {
        return brazier::TcpClient(io_.get_executor(), host, port);
    }

    brazier::TcpClient make_client() {
        return make_client(kTestHost, static_cast<uint16_t>(kTestPort));
    }

    template<typename F>
    auto run(F&& coro_factory) {
        auto fut = boost::asio::co_spawn(io_, std::forward<F>(coro_factory), boost::asio::use_future);

        io_.run();
        return fut.get();
    }
};

TEST_F(TcpClientTest, ConnectToInvalidHost) {
    EXPECT_FALSE(run([this]() -> brazier::awaitable<bool> {
        auto client = make_client("invalid_host", 12345);

        co_return co_await client.connect();
        }));
}

TEST_F(TcpClientTest, ConnectToInvalidPort) {
    EXPECT_FALSE(run([this]() -> brazier::awaitable<bool> {
        auto client = make_client(kTestHost, 0);

        co_return co_await client.connect();
        }));
}

TEST_F(TcpClientTest, ConnectToValidHostAndPort) {
    EXPECT_TRUE(run([this]() -> brazier::awaitable<bool> {
        auto client = make_client();

        co_return co_await client.connect();
        }));
}

TEST_F(TcpClientTest, SendRawData) {
    EXPECT_TRUE(run([this]() -> brazier::awaitable<bool> {
        auto client = make_client();

        if (!co_await client.connect())
            co_return false;

        std::string request = "GET / HTTP/1.1\r\nHost: " + kTestHost + "\r\n\r\n";

        co_return co_await client.send_raw(std::move(request));
        }));
}

TEST_F(TcpClientTest, ReadResponse) {
    auto response = run([this]() -> brazier::awaitable<
        std::optional<brazier::http::response<brazier::http::string_body>>> {
            auto client = make_client();

            if (!co_await client.connect())
                co_return std::nullopt;

            std::string request = "GET / HTTP/1.1\r\nHost: " + kTestHost + "\r\n\r\n";

            if (!co_await client.send_raw(std::move(request)))
                co_return std::nullopt;

            co_return co_await client.read_response();
        });

    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->result_int(), 404);
}

TEST_F(TcpClientTest, IsConnectionClosed) {
    EXPECT_FALSE(run([this]() -> brazier::awaitable<bool> {
        auto client = make_client();
        if (!co_await client.connect())
            co_return true;

        co_return co_await client.is_connection_closed();
        }));
}

TEST_F(TcpClientTest, ReadResponseWithTimeout) {
    auto response = run([this]() -> brazier::awaitable<
        std::optional<brazier::http::response<brazier::http::string_body>>> {
            auto client = make_client();

            if (!co_await client.connect())
                co_return std::nullopt;

            std::string request = "GET / HTTP/1.1\r\nHost: " + kTestHost + "\r\n\r\n";

            if (!co_await client.send_raw(std::move(request)))
                co_return std::nullopt;

            co_return co_await client.read_response(std::chrono::seconds(5));
        });

    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->result_int(), 404);
}