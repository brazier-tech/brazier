#include "../include/brazier/server.hpp"

brazier::Server::Server(const std::string& host, unsigned short port)
    : port_(port), host_(host) {}

bool brazier::Server::initialize() {
    try {
        Logger::init("debug.log");
        Logger::registerSignalHandlers();

        json drivers = global_config->getJson("filesystem.drivers");

        for (auto& [name, cfg] : drivers.items()) {
            if (name == "default") continue;

            auto driver = std::make_shared<brazier::FileDriver>();
            driver->setRootPath(cfg.value("root", "./"));
            driver->initAsync();
            StorageManager::getInstance().registerDriver(name, driver);
        }

        std::string def = global_config->getNested<std::string>("filesystem.default", "local");
        if (StorageManager::getInstance().hasDriver(def)) {
            StorageManager::getInstance().setDefaultDriver(def);
        }

        keep_alive_timeout_ = std::chrono::seconds(
            global_config->get("protocol.keep_alive_timeout", 60));
        max_connections_ = global_config->get("protocol.max_connections", 10000);

        const tcp::endpoint endpoint(net::ip::make_address(host_), port_);
        const int n = platform::get_thread_count();
        const bool split_accept = !platform::has_reuse_port();
        const int io_count = n;

        io_contexts_.reserve(io_count);
        work_guards_.reserve(io_count);
        acceptors_.reserve(split_accept ? 1 : io_count);

        for (int i = 0; i < io_count; ++i) {
            auto io = std::make_unique<net::io_context>();
            const bool needs_acceptor = !split_accept || (i == 0);

            if (needs_acceptor) {
                auto acc = std::make_unique<tcp::acceptor>(*io);
                acc->open(endpoint.protocol());
                acc->set_option(tcp::acceptor::reuse_address(true));

                const auto native = static_cast<platform::NativeSocket>(acc->native_handle());

                if (!platform::set_reuse_port(native) && platform::has_reuse_port()) {
                    Logger::log("SO_REUSEPORT setsockopt failed on worker " + std::to_string(i), "WARNING");
                }
                if (!platform::set_defer_accept(native, 1) && platform::has_defer_accept()) {
                    Logger::log("TCP_DEFER_ACCEPT setsockopt failed on worker " + std::to_string(i), "WARNING");
                }

                acc->bind(endpoint);
                acc->listen(boost::asio::socket_base::max_listen_connections);
                acceptors_.push_back(std::move(acc));
            }

            work_guards_.push_back(std::make_unique<
                net::executor_work_guard<net::io_context::executor_type>>(io->get_executor()));

            io_contexts_.push_back(std::move(io));
        }

        initializeConnections();
        RouterRegisterer::init(*io_contexts_[0]);

        Logger::log("HTTP Server initialized on " + host_ + ":" + std::to_string(port_) +
            " [io_contexts=" + std::to_string(io_count) +
            ", acceptors=" + std::to_string(acceptors_.size()) +
            ", dispatch=" + (split_accept ? "round-robin" : "SO_REUSEPORT") + "]",
            "SUCCESS");
        return true;
    }
    catch (const std::exception& e) {
        Logger::log("Initialization failed: " + std::string(e.what()), "ERROR");
        return false;
    }
}

void brazier::Server::run() {
    try {
        const int io_count = static_cast<int>(io_contexts_.size());
        const bool split_accept = !platform::has_reuse_port();

        if (split_accept) {
            net::co_spawn(*io_contexts_[0],
                accept_and_dispatch(*acceptors_[0], 0),
                net::detached);
        }
        else {
            for (int i = 0; i < io_count; ++i) {
                net::co_spawn(*io_contexts_[i],
                    accept_loop(*acceptors_[i]),
                    net::detached);
            }
        }

        for (auto& io : io_contexts_) {
            auto* io_ptr = io.get();
            threads_.emplace_back([io_ptr] {
                brazier::Engine::init(*io_ptr);
                io_ptr->run();
                });
        }

        shutdown_flag_.store(false, std::memory_order_release);
        stats_thread_ = std::thread([this] {
            while (!shutdown_flag_.load(std::memory_order_acquire)) {
                std::this_thread::sleep_for(10s);
                if (shutdown_flag_.load(std::memory_order_acquire)) break;
                Logger::log("STATS - Active connections: " + std::to_string(connection_count_.load()) +
                    ", Total requests: " + std::to_string(total_requests_.load()), "INFO");
            }
            });
       
        Logger::log("Starting " + std::to_string(io_count) + " io_context(s) over " +
            std::to_string(threads_.size()) + " thread(s)", "INFO");

        for (auto& t : threads_) {
            if (t.joinable()) t.join();
        }
    }
    catch (const std::exception& e) {
        Logger::log("Server run failed: " + std::string(e.what()), "ERROR");
        throw;
    }
}

void brazier::Server::stop() {
    Logger::log("HTTP Server stopping...", "INFO");
    shutdown_flag_.store(true);
    shutting_down_.store(true);

    for (auto& acc : acceptors_) {
        boost::system::error_code ignore;
        acc->close(ignore);
    }
    for (auto& wg : work_guards_) wg.reset();
    for (auto& io : io_contexts_) io->stop();

    if (stats_thread_.joinable()) stats_thread_.join();
    Logger::log("HTTP Server stopped", "INFO");
}

unsigned short brazier::Server::getPort() const { return port_; }
const std::string& brazier::Server::getHost() const { return host_; }

void brazier::Server::initializeConnections() {
    try {
        Queue::connect(global_config->get("nosql.host", "127.0.0.1"),
            global_config->get("nosql.port", 6379));
    }
    catch (const std::exception& e) {
        Logger::log("Connection to queue failed: " + std::string(e.what()), "ERROR");
    }

    try {
        Cache::connect(global_config->get("redis.host", "127.0.0.1"),
            global_config->get("redis.port", 6379));
    }
    catch (const std::exception& e) {
        Logger::log("Connection to NOSQL database failed: " + std::string(e.what()), "ERROR");
    }
}

net::awaitable<void> brazier::Server::handle_connection(tcp::socket socket) {
    connection_count_++;

    try {
        http::request<http::string_body> req;
        http::response<http::string_body> res;
        beast::flat_buffer buffer;
        bool keep_alive = true;

        socket.set_option(tcp::no_delay(true));
        socket.set_option(boost::asio::socket_base::keep_alive(true));

        net::steady_timer idle_timer(co_await net::this_coro::executor);
        const auto IDLE_TIMEOUT = keep_alive_timeout_;
        bool timed_out = false;

        auto reset_timer = [&]() {
            idle_timer.expires_after(IDLE_TIMEOUT);
            idle_timer.async_wait([&](beast::error_code ec) {
                if (!ec) {
                    timed_out = true;
                    socket.cancel();
                }
                });
            };

        reset_timer();

        while (keep_alive && !timed_out) {
            beast::error_code ec;
            req = {};

            co_await http::async_read(socket, buffer, req, net::redirect_error(net::use_awaitable, ec));

            if (ec == http::error::end_of_stream) break;
            if (ec) {
                if (ec == net::error::operation_aborted) break;
                throw boost::system::system_error(ec);
            }

            reset_timer();
            total_requests_++;
            keep_alive = req.keep_alive();

            res = {};
            res.version(req.version());
            res.keep_alive(keep_alive);
            res.set(http::field::connection, keep_alive ? "keep-alive" : "close");
            res.set(http::field::server, "brazier");

            co_await Router::handle_request(req, res);

            if (res.body().empty() && res.count(http::field::content_length) == 0) {
                res.content_length(0);
            }
            else if (!res.body().empty() && res.count(http::field::content_length) == 0) {
                res.content_length(res.body().size());
            }

            res.prepare_payload();
            co_await http::async_write(socket, res, net::use_awaitable);

            buffer.consume(buffer.size());
            if (!keep_alive) break;
        }

        idle_timer.cancel();

        beast::error_code ec;
        socket.shutdown(tcp::socket::shutdown_send, ec);
        socket.close(ec);
    }
    catch (const std::exception& e) {
        Logger::log("Connection error: " + std::string(e.what()), "ERROR");
    }
    catch (...) {
        Logger::log("Unknown connection error", "ERROR");
    }

    connection_count_--;
    co_return;
}

net::awaitable<void> brazier::Server::accept_loop(tcp::acceptor& acceptor) {
    for (;;) {
        if (shutting_down_.load(std::memory_order_acquire)) co_return;

        beast::error_code ec;
        tcp::socket socket = co_await acceptor.async_accept(net::redirect_error(net::use_awaitable, ec));

        if (ec) {
            if (ec == net::error::operation_aborted || shutting_down_.load(std::memory_order_acquire)) co_return;
            Logger::log("Accept error: " + ec.message(), "ERROR");
            continue;
        }

        if (connection_count_.load(std::memory_order_acquire) >= max_connections_) {
            Logger::log("Connection limit reached, rejecting", "WARNING");
            boost::system::error_code ignore;
            socket.close(ignore);
            continue;
        }

        net::co_spawn(acceptor.get_executor(), handle_connection(std::move(socket)), net::detached);
    }
}

net::awaitable<void> brazier::Server::accept_and_dispatch(tcp::acceptor& acceptor, int worker_begin) {
    const int worker_count = static_cast<int>(io_contexts_.size()) - worker_begin;
    int next = 0;

    for (;;) {
        if (shutting_down_.load(std::memory_order_acquire)) co_return;

        const int idx = worker_begin + (next++ % worker_count);

        tcp::socket socket(*io_contexts_[idx]);

        beast::error_code ec;
        co_await acceptor.async_accept(socket, net::redirect_error(net::use_awaitable, ec));

        if (ec) {
            if (ec == net::error::operation_aborted || shutting_down_.load(std::memory_order_acquire)) co_return;
            Logger::log("Accept error: " + ec.message(), "ERROR");
            continue;
        }

        if (connection_count_.load(std::memory_order_acquire) >= max_connections_) {
            Logger::log("Connection limit reached, rejecting", "WARNING");
            boost::system::error_code ignore;
            socket.close(ignore);
            continue;
        }

        net::co_spawn(*io_contexts_[idx], handle_connection(std::move(socket)), net::detached);
    }
}