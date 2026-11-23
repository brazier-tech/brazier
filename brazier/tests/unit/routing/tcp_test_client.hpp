#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <thread>

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>

namespace tcp_test {

    namespace net = boost::asio;
    namespace http = boost::beast::http;
    using tcp = net::ip::tcp;

    class TcpClient {
    public:
        TcpClient(std::string host, uint16_t port) : host_(std::move(host)), port_(port), socket_(io_) {}

        bool connect() {
            try {
                tcp::resolver resolver(io_);
                auto results = resolver.resolve(host_, std::to_string(port_));
                net::connect(socket_, results);
                return true;
            }
            catch (...) {
                return false;
            }
        }

        bool send_raw(const std::string& data) {
            if (!socket_.is_open()) 
                return false;

            try {
                net::write(socket_, net::buffer(data));
                return true;
            }
            catch (...) {
                return false;
            }
        }

        std::optional<http::response<http::string_body>> read_response(int timeout_sec = 10) {
            if (!socket_.is_open()) 
                return std::nullopt;

            try {
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

                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }

                return std::nullopt;
            }
            catch (...) {
                return std::nullopt;
            }
        }

        bool is_connection_closed(int wait_sec = 2) {
            if (!socket_.is_open()) 
                return true;

            try {
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
            catch (...) {
                return true;
            }
        }

    private:
        std::string host_;
        uint16_t port_;
        net::io_context io_;
        tcp::socket socket_;
    };
}