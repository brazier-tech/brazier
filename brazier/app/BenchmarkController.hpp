#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/beast/http.hpp>
#include "../include/brazier/Core"

class BenchmarkController : public brazier::Controller {
public:
    using Request = boost::beast::http::request<boost::beast::http::string_body>;
    using Response = boost::beast::http::response<boost::beast::http::string_body>;

    boost::asio::awaitable<void> test(const Request& req, Response& res, const brazier::Params& params) {
        res.result(boost::beast::http::status::ok);
        res.set(boost::beast::http::field::content_type, "text/plain");
        res.body() = "";

        co_return;
    }
};