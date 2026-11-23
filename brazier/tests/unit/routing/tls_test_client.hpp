#pragma once

#include <chrono>
#include <fstream>
#include <memory>
#include <optional>
#include <string>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>

#include <openssl/ssl.h>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

namespace tls_test {

    inline constexpr int kSocketTimeoutSec = 5;

    struct TlsClient {
        net::io_context io;
        ssl::context    ctx;
        std::unique_ptr<ssl::stream<beast::tcp_stream>> stream;

        TlsClient(const std::string& host, unsigned short port, bool with_client_cert = false, const std::string& cert = "", const std::string& key = "")
            : host_(host), port_(port), ctx(ssl::context::tls_client) 
        {
            ctx.set_verify_mode(ssl::verify_none);

            if (with_client_cert) {
                ctx.use_certificate_chain_file(cert);
                ctx.use_private_key_file(key, ssl::context::pem);
            }
        }

        bool connect(int timeout_sec = kSocketTimeoutSec) {
            try {
                stream = std::make_unique<ssl::stream<beast::tcp_stream>>(io, ctx);
                tcp::resolver resolver(io);
                auto results = resolver.resolve(host_, std::to_string(port_));

                beast::get_lowest_layer(*stream).expires_after(std::chrono::seconds(timeout_sec));
                beast::get_lowest_layer(*stream).connect(results);
                beast::get_lowest_layer(*stream).expires_never();
                stream->handshake(ssl::stream_base::client);

                return true;
            }
            catch (const std::exception&) {
                return false;
            }
        }

        bool send_raw(const std::string& data) {
            try {
                beast::get_lowest_layer(*stream).expires_after(std::chrono::seconds(kSocketTimeoutSec));
                net::write(*stream, net::buffer(data));

                return true;
            }
            catch (const std::exception&) {
                return false;
            }
        }

        std::optional<http::response<http::string_body>> read_response(int timeout_sec = kSocketTimeoutSec) {
            try {
                beast::flat_buffer buffer;
                http::response<http::string_body> res;
                beast::get_lowest_layer(*stream).expires_after(std::chrono::seconds(timeout_sec));
                http::read(*stream, buffer, res);

                return res;
            }
            catch (const std::exception&) {
                return std::nullopt;
            }
        }

        bool is_connection_closed(int timeout_sec = kSocketTimeoutSec) {
            if (!stream) 
                return true;

            try {
                beast::get_lowest_layer(*stream).expires_after(std::chrono::seconds(timeout_sec));
                char c;
                beast::error_code ec;
                auto n = stream->read_some(net::buffer(&c, 1), ec);

                if (ec == net::error::eof || ec == net::error::connection_reset ||
                    ec == ssl::error::stream_truncated || ec == beast::error::timeout) {
                    return true;
                }

                if (ec == net::error::timed_out) 
                    return false;

                if (!ec && n > 0) 
                    return false;

                return true;
            }
            catch (const std::exception&) {
                return true;
            }
        }

    private:
        std::string   host_;
        unsigned short port_;
    };

} 