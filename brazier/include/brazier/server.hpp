#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/config.hpp>
#include <thread>
#include <vector>
#include <memory>
#include <atomic>
#include "vendor/Handlers/ENV.hpp"
#include "Database/Queue.hpp"
#include "Database/Cache.hpp"
#include "Router/RouterRegisterer.hpp"
#include "Router/Router.hpp"
#include "Engine.hpp"
#include "Filesystem/Filesystem.hpp"
#include "vendor/ConfigManager.hpp"
#include "Platform/SystemInfo.hpp"
#include "Platform/SocketOptions.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using namespace std::chrono_literals;

namespace brazier {

    class Server {
    private:
        std::vector<std::unique_ptr<net::io_context>> io_contexts_;
        std::vector<std::unique_ptr<tcp::acceptor>>   acceptors_;
        std::vector<std::unique_ptr<net::executor_work_guard<net::io_context::executor_type>>> work_guards_;

        unsigned short port_;
        std::string host_;
        std::vector<std::thread> threads_;

        std::atomic<int> connection_count_{ 0 };
        std::atomic<int> total_requests_{ 0 };

        std::thread stats_thread_;
        std::atomic<bool> shutdown_flag_{ false };
        std::atomic<bool> shutting_down_{ false };

        int max_connections_ = 10000;

    public:
        Server(const std::string& host, unsigned short port);

        bool initialize();
        void run();
        void stop();

        unsigned short getPort() const;
        const std::string& getHost() const;

    private:
        void initializeConnections();

        net::awaitable<void> handle_connection(tcp::socket socket);
        net::awaitable<void> accept_loop(tcp::acceptor& acceptor);
        net::awaitable<void> accept_and_dispatch(tcp::acceptor& acceptor, int worker_begin);
    };
}