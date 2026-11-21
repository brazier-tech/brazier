/**
 * Copyright (c) 2026 Kirill Sergeev, Nikolay Sugonyako, Andrey Agarkov, Hlib Safiannikov.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This source code is licensed under the LGPL license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>

#include <openssl/ssl.h>

namespace brazier {

    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace net = boost::asio;
    namespace ssl = net::ssl;

    using tcp = net::ip::tcp;

    template<typename T>
    using awaitable = net::awaitable<T>;

    /**
     * @class TlsClient
     * @brief Asynchronous TLS client for a single host/port.
     *
     * @details
     * TlsClient performs non-blocking TLS I/O on a caller-supplied executor.
     * Every operation is exposed as a @c boost::asio::awaitable and must be
     * awaited from a coroutine running on the same executor.
     *
     * The executor passed to the constructor must be actively driven
     * (@c io_context::run(), a @c thread_pool, etc.); otherwise no operation
     * will ever complete.
     *
     * Typical usage:
     * @code
     * brazier::TlsClient client(co_await net::this_coro::executor, host, port);
     * client.set_ca("ca.pem");
     * 
     * if (!co_await client.connect())
     *     co_return;
     * 
     * co_await client.send_raw(request);
     * 
     * auto resp = co_await client.read_response();
     * @endcode
     *
     * @warning The SSL context is created with @c ssl::verify_none by default;
     * the server certificate is not validated unless set_ca() is called.
     *
     * @warning Methods are not internally synchronized. Concurrent calls on
     * the same instance must be serialized by the caller; overlapping
     * operations on @c stream_ are undefined behavior.
     *
     * @warning The instance must outlive every coroutine that awaits its
     * methods. Awaiting a method on a destroyed client is undefined behavior.
     *
     * @note The class is non-copyable and non-movable due to the
     * @c std::unique_ptr member.
     */
    class TlsClient {
    public:
        /**
         * @brief Constructs a client bound to @p host : @p port.
         *
         * @param[in] ex Executor on which all asynchronous operations run. Must
         * remain alive and driven for the lifetime of the client.
         * @param[in] host Target host name or IP address.
         * @param[in] port Target TCP port.
         *
         * @details
         * Initializes the SSL context as a TLS client and sets
         * @c ssl::verify_none. No connection is attempted until connect() is
         * awaited.
         */
        TlsClient(net::any_io_executor ex, std::string host, unsigned short port);

        /**
         * @brief Enables server certificate verification using @p ca_file.
         *
         * @param[in] ca_file Path to a PEM CA bundle.
         *
         * @details
         * Loads @p ca_file into the SSL context, switches verification to
         * @c ssl::verify_peer, and installs host-name verification against the
         * host passed to the constructor.
         *
         * @throws boost::system::system_error If @p ca_file cannot be loaded.
         *
         * @note Synchronous; must be called before connect().
         */
        void set_ca(const std::string& ca_file);

        /**
         * @brief Configures a client certificate and private key.
         *
         * @param[in] cert_file Path to a PEM client certificate chain.
         * @param[in] key_file Path to the matching PEM private key.
         *
         * @throws boost::system::system_error If either file cannot be loaded.
         *
         * @note Synchronous; must be called before connect().
         */
        void set_client_cert(const std::string& cert_file, const std::string& key_file);

        /**
         * @brief Resolves the host, connects, and performs the TLS handshake.
         *
         * @return @c true on success, @c false on any failure.
         *
         * @details
         * Creates a fresh stream, asynchronously resolves @c host_ : @c port_,
         * connects to the first endpoint, and performs the TLS client
         * handshake. Both the connect and the handshake stages are bounded by
         * @c kSocketTimeoutSec.
         *
         * Any previous stream is discarded before the attempt.
         *
         * @note Errors are reported through the return value; the method does
         * not throw.
         */
        awaitable<bool> connect();

        /**
         * @brief Sends raw bytes over the established TLS stream.
         *
         * @param[in] data Bytes to send. Passed by value so the buffer remains
         * valid across suspension points.
         * @return @c true on success; @c false if no connection is established
         * or the write fails or times out.
         *
         * @details
         * Bounded by @c kSocketTimeoutSec. Errors are reported through the
         * return value.
         */
        awaitable<bool> send_raw(std::string data);

        /**
         * @brief Probes whether the TLS connection has been closed.
         *
         * @param[in] timeout Maximum time to wait for a single-byte read. Defaults to 2 seconds.
         * 
         * @return @c true if the connection is closed (or was never established); @c false if it appears open.
         *
         * @details
         * Issues a one-byte asynchronous read:
         * - data received: the byte is buffered and the call returns @c false;
         * - timeout: the connection is treated as open (@c false);
         * - EOF, reset, or any other error: the connection is treated as
         *   closed (@c true).
         *
         * The buffered byte is preserved for a subsequent read_response().
         *
         * @note The timeout bounds each individual read, not the total probe;
         * the SSL layer may still need to perform handshake-related work.
         */
        awaitable<bool> is_connection_closed(std::chrono::milliseconds timeout = std::chrono::seconds(2));

        /**
         * @brief Reads and parses one HTTP response from the stream.
         *
         * @param[in] timeout Read timeout. Defaults to 5 seconds.
         * @return The parsed response, or @c std::nullopt on any error,
         * including timeout and malformed input.
         *
         * @details
         * Uses an internal @c flat_buffer that persists across calls, so
         * bytes preserved by is_connection_closed() are visible here.
         *
         * @note No diagnostics are exposed on failure; the caller receives
         * @c std::nullopt.
         */
        awaitable<std::optional<http::response<http::string_body>>>
            read_response(std::chrono::milliseconds timeout = std::chrono::seconds(5));

    private:
        /**
         * @brief Executor on which all asynchronous operations run.
         */
        net::any_io_executor ex_;

        /**
         * @brief Target host name or IP address used by connect() and host-name
         * verification in set_ca().
         */
        std::string host_;

        /**
         * @brief Target TCP port used by connect().
         */
        unsigned short port_;

        /**
         * @brief SSL context shared by every stream created by this instance.
         */
        ssl::context ctx_{ ssl::context::tls_client };

        /**
         * @brief Active TLS stream. Null until connect() is awaited.
         */
        std::unique_ptr<ssl::stream<beast::tcp_stream>> stream_;

        /**
         * @brief Buffer that persists across asynchronous reads.
         *
         * @details Required for @c http::async_read, which may suspend with
         * partially received data, and used by is_connection_closed() to
         * preserve the probed byte.
         */
        beast::flat_buffer buffer_;

        /**
         * @brief Timeout applied to connect, TLS handshake, and send_raw().
         */
        std::chrono::milliseconds kSocketTimeoutSec{ std::chrono::seconds(5) };
    };

}