/**
 * Copyright (c) 2026 Kirill Sergeev, Nikolay Sugonyako, Andrey Agarkov, Hlib Safiannikov.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This source code is licensed under the LGPL license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>

namespace brazier {

    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace net = boost::asio;
    namespace ssl = net::ssl;

    using tcp = net::ip::tcp;

    template<typename T>
    using awaitable = net::awaitable<T>;

    /**
     * @class TcpClient
     * @brief Asynchronous plain-TCP client for a single host/port.
     *
     * @details
     * TcpClient performs non-blocking TCP I/O on a caller-supplied executor.
     * Every operation is exposed as a @c boost::asio::awaitable and must be
     * awaited from a coroutine running on the same executor.
     *
     * The executor passed to the constructor must be actively driven
     * (@c io_context::run(), a @c thread_pool, etc.); otherwise no operation
     * will ever complete.
     *
     * Typical usage:
     * @code
     * brazier::TcpClient client(co_await net::this_coro::executor, host, port);
     * if (!co_await client.connect())
     *     co_return;
     * co_await client.send_raw(request);
     * auto resp = co_await client.read_response();
     * @endcode
     *
     * @warning Methods are not internally synchronized. Concurrent calls on
     * the same instance must be serialized by the caller; overlapping
     * operations on @c socket_ are undefined behavior.
     *
     * @warning The instance must outlive every coroutine that awaits its
     * methods. Awaiting a method on a destroyed client is undefined behavior.
     *
     * @note The class is non-copyable and non-movable due to the
     * @c boost::asio::ip::tcp::socket and @c boost::beast::flat_buffer
     * members.
     */
    class TcpClient {
        /**
         * @brief Timeout applied to connect().
         *
         * @details Stored separately because it is configurable per instance
         * through the constructor.
         */
        std::chrono::microseconds timeout_;

    public:
        /**
         * @brief Constructs a client bound to @p host : @p port.
         *
         * @param[in] ex Executor on which all asynchronous operations run. Must
         * remain alive and driven for the lifetime of the client.
         * @param[in] host Target host name or IP address.
         * @param[in] port Target TCP port.
         * @param[in] timeout Connect timeout. Defaults to 5 seconds.
         *
         * @details
         * Creates the internal socket on @p ex. No connection is attempted
         * until connect() is awaited.
         */
        TcpClient(net::any_io_executor ex, std::string host, uint16_t port,
            std::chrono::microseconds timeout = std::chrono::seconds(5));

        /**
         * @brief Resolves the host and establishes a TCP connection.
         *
         * @return @c true on success, @c false on any failure (including
         * timeout, resolution failure, and connection error).
         *
         * @details
         * Asynchronously resolves @c host_ : @c port_ and connects to the
         * first endpoint. Both stages are bounded by @c timeout_.
         *
         * @note Errors are reported through the return value; the method does
         * not throw. A previous connection, if any, is left as-is; this method
         * only opens the socket and does not close an existing one first.
         */
        awaitable<bool> connect();

        /**
         * @brief Sends raw bytes over the established TCP connection.
         *
         * @param[in] data Bytes to send. Passed by value so the buffer remains
         * valid across suspension points.
         * @return @c true on success; @c false if the socket is not open or
         * the write fails.
         *
         * @note Errors are reported through the return value.
         */
        awaitable<bool> send_raw(std::string data);

        /**
         * @brief Probes whether the TCP connection has been closed.
         *
         * @param[in] timeout Maximum time to wait for a single-byte read.
         * Defaults to 2 seconds.
         * @return @c true if the connection is closed (or was never
         * established); @c false if it appears open.
         *
         * @details
         * Issues a one-byte asynchronous read:
         * - data received: the byte is buffered and the call returns @c false;
         * - read timeout (@c operation_aborted from the internal timer): the
         *   connection is treated as open (@c false);
         * - EOF, reset, or any other error: the connection is treated as
         *   closed (@c true).
         *
         * The buffered byte is preserved for a subsequent read_response().
         */
        awaitable<bool> is_connection_closed(std::chrono::milliseconds timeout = std::chrono::seconds(2));

        /**
         * @brief Reads and parses one HTTP response from the connection.
         *
         * @param[in] timeout Read timeout. Defaults to 10 seconds.
         * @return The parsed response, or @c std::nullopt on any error,
         * including timeout, EOF, and malformed input.
         *
         * @details
         * Uses an internal @c flat_buffer that persists across calls, so
         * bytes preserved by is_connection_closed() are visible here.
         *
         * @note No diagnostics are exposed on failure; the caller receives
         * @c std::nullopt.
         */
        awaitable<std::optional<http::response<http::string_body>>>
            read_response(std::chrono::milliseconds timeout = std::chrono::seconds(10));

    private:
        /**
         * @brief Executor on which all asynchronous operations run.
         */
        net::any_io_executor ex_;

        /**
         * @brief Target host name or IP address used by connect().
         */
        std::string host_;

        /**
         * @brief Target TCP port used by connect().
         */
        uint16_t port_;

        /**
         * @brief Underlying TCP socket. Always open from construction onward;
         * closed on error or after a failed read_response() timeout.
         */
        tcp::socket socket_;

        /**
         * @brief Buffer that persists across asynchronous reads.
         *
         * @details Required for @c http::async_read, which may suspend with
         * partially received data, and used by is_connection_closed() to
         * preserve the probed byte.
         */
        beast::flat_buffer buffer_;
    };
}