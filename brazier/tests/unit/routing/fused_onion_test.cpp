/**
 * Copyright (c) 2026 Kirill Sergeev, Nikolay Sugonyako, Andrey Agarkov, Hlib Safiannikov.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This source code is licensed under the LGPL license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/beast/http.hpp>

#include <stdexcept>
#include <type_traits>
#include <string>
#include <vector>

#include "../../../include/brazier/App/Http/Middleware/FusedOnion.hpp"
#include "../../../include/brazier/App/Http/Middleware/CorsMiddleware.hpp"
#include "../../../include/brazier/App/Http/Middleware/AuthMiddleware.hpp"

namespace net = boost::asio;
namespace http = boost::beast::http;
using brazier::FusedOnion;

static_assert(std::is_same_v<brazier::Middleware, FusedOnion::Middleware>);
static_assert(std::is_same_v<brazier::Context, FusedOnion::Context>);
static_assert(std::is_same_v<brazier::Next, FusedOnion::Next>);
static_assert(std::is_same_v<brazier::Awaitable, FusedOnion::Awaitable>);

namespace {

    void RunAwaitable(FusedOnion::Awaitable operation) {
        net::io_context io;
        auto result = net::co_spawn(io, std::move(operation), net::use_future);
        io.run();
        result.get();
    }

    struct UserMarker {
        std::string value;
    };

    FusedOnion::Request MakeRequest() {
        return FusedOnion::Request{http::verb::get, "/", 11};
    }

    FusedOnion::Response MakeResponse() {
        return FusedOnion::Response{http::status::ok, 11};
    }

}

TEST(FusedOnionTest, MiddlewareRunsInOnionOrder) {
    std::vector<std::string> events;

    FusedOnion pipeline(
        [&events](const auto&, auto&, const auto&) -> FusedOnion::Awaitable {
            events.emplace_back("controller");
            co_return;
        }
    );

    pipeline.middleware.use(
        [&events](FusedOnion::Context&, FusedOnion::Next next)
            -> FusedOnion::Awaitable {
            events.emplace_back("outer:before");
            co_await next();
            events.emplace_back("outer:after");
        }
    );

    pipeline.middleware.use(
        [&events](FusedOnion::Context&, FusedOnion::Next next)
            -> FusedOnion::Awaitable {
            events.emplace_back("inner:before");
            co_await next();
            events.emplace_back("inner:after");
        }
    );

    auto request = MakeRequest();
    auto response = MakeResponse();
    FusedOnion::Params params;

    RunAwaitable(pipeline.run(request, response, params));

    const std::vector<std::string> expected{
        "outer:before",
        "inner:before",
        "controller",
        "inner:after",
        "outer:after"
    };
    EXPECT_EQ(events, expected);
}

TEST(FusedOnionTest, RequestAndResponseInterceptorsRunInRegistrationOrder) {
    std::vector<std::string> events;

    FusedOnion pipeline(
        [&events](const auto&, auto&, const auto&) -> FusedOnion::Awaitable {
            events.emplace_back("controller");
            co_return;
        }
    );

    pipeline.request.use([&events](FusedOnion::Context&) -> FusedOnion::Awaitable {
        events.emplace_back("request:1");
        co_return;
    });
    pipeline.request.use([&events](FusedOnion::Context&) -> FusedOnion::Awaitable {
        events.emplace_back("request:2");
        co_return;
    });
    pipeline.response.use([&events](FusedOnion::Context&) -> FusedOnion::Awaitable {
        events.emplace_back("response:1");
        co_return;
    });
    pipeline.response.use([&events](FusedOnion::Context&) -> FusedOnion::Awaitable {
        events.emplace_back("response:2");
        co_return;
    });

    auto request = MakeRequest();
    auto response = MakeResponse();
    FusedOnion::Params params;

    RunAwaitable(pipeline.run(request, response, params));

    const std::vector<std::string> expected{
        "request:1", "request:2", "controller", "response:1", "response:2"
    };
    EXPECT_EQ(events, expected);
}

TEST(FusedOnionTest, MiddlewareCanShortCircuit) {
    bool controller_called = false;

    FusedOnion pipeline(
        [&controller_called](const auto&, auto&, const auto&) -> FusedOnion::Awaitable {
            controller_called = true;
            co_return;
        }
    );

    pipeline.middleware.use(
        [](FusedOnion::Context& context, FusedOnion::Next) -> FusedOnion::Awaitable {
            context.response.result(http::status::unauthorized);
            context.response.body() = "unauthorized";
            co_return; // Deliberately do not call next().
        }
    );

    auto request = MakeRequest();
    auto response = MakeResponse();
    FusedOnion::Params params;

    RunAwaitable(pipeline.run(request, response, params));

    EXPECT_FALSE(controller_called);
    EXPECT_EQ(response.result(), http::status::unauthorized);
    EXPECT_EQ(response.body(), "unauthorized");
}

TEST(FusedOnionTest, ContextDataIsSharedAcrossOnionLayers) {
    std::string observed;

    FusedOnion pipeline(
        [&observed](const auto&, auto&, const auto&) -> FusedOnion::Awaitable {
            co_return;
        }
    );

    pipeline.middleware.use(
        [](FusedOnion::Context& context, FusedOnion::Next next)
            -> FusedOnion::Awaitable {
            context.set(UserMarker{"authenticated"});
            co_await next();
        }
    );

    pipeline.middleware.use(
        [&observed](FusedOnion::Context& context, FusedOnion::Next next)
            -> FusedOnion::Awaitable {
            const auto* marker = context.get<UserMarker>();
            observed = marker ? marker->value : "missing";
            co_await next();
        }
    );

    auto request = MakeRequest();
    auto response = MakeResponse();
    FusedOnion::Params params;

    RunAwaitable(pipeline.run(request, response, params));
    EXPECT_EQ(observed, "authenticated");
}

TEST(FusedOnionTest, ErrorInterceptorCanRecoverAndResponseHooksStillRun) {
    bool response_hook_called = false;

    FusedOnion pipeline(
        [](const auto&, auto&, const auto&) -> FusedOnion::Awaitable {
            throw std::runtime_error("controller failed");
            co_return;
        }
    );

    pipeline.error.use([](FusedOnion::Context& context) -> FusedOnion::Awaitable {
        context.response.result(http::status::internal_server_error);
        context.response.body() = "recovered";
        context.handle_error();
        co_return;
    });

    pipeline.response.use([&response_hook_called](FusedOnion::Context&)
        -> FusedOnion::Awaitable {
        response_hook_called = true;
        co_return;
    });

    auto request = MakeRequest();
    auto response = MakeResponse();
    FusedOnion::Params params;

    EXPECT_NO_THROW(RunAwaitable(pipeline.run(request, response, params)));
    EXPECT_TRUE(response_hook_called);
    EXPECT_EQ(response.body(), "recovered");
}

TEST(FusedOnionTest, EjectRemovesOnlyTheSelectedHandler) {
    int calls = 0;

    FusedOnion pipeline(
        [&calls](const auto&, auto&, const auto&) -> FusedOnion::Awaitable {
            ++calls;
            co_return;
        }
    );

    const auto id = pipeline.request.use(
        [](FusedOnion::Context&) -> FusedOnion::Awaitable {
            co_return;
        }
    );

    EXPECT_EQ(pipeline.request.size(), 1U);
    EXPECT_TRUE(pipeline.request.eject(id));
    EXPECT_FALSE(pipeline.request.eject(id));
    EXPECT_EQ(pipeline.request.size(), 0U);

    auto request = MakeRequest();
    auto response = MakeResponse();
    FusedOnion::Params params;

    RunAwaitable(pipeline.run(request, response, params));
    EXPECT_EQ(calls, 1);
}

TEST(FusedOnionTest, PreAndPostAdaptersAreOnionLayers) {
    std::vector<std::string> events;

    FusedOnion pipeline(
        [&events](const auto&, auto&, const auto&) -> FusedOnion::Awaitable {
            events.emplace_back("controller");
            co_return;
        }
    );

    pipeline.middleware.use(FusedOnion::pre(
        [&events](FusedOnion::Context&) -> FusedOnion::Awaitable {
            events.emplace_back("pre");
            co_return;
        }
    ));

    pipeline.middleware.use(FusedOnion::post(
        [&events](FusedOnion::Context&) -> FusedOnion::Awaitable {
            events.emplace_back("post");
            co_return;
        }
    ));

    auto request = MakeRequest();
    auto response = MakeResponse();
    FusedOnion::Params params;

    RunAwaitable(pipeline.run(request, response, params));

    const std::vector<std::string> expected{"pre", "controller", "post"};
    EXPECT_EQ(events, expected);
}

TEST(FusedOnionTest, AuthMiddlewareFactoryCanShortCircuit) {
    bool controller_called = false;

    FusedOnion pipeline(
        [&controller_called](const auto&, auto&, const auto&) -> FusedOnion::Awaitable {
            controller_called = true;
            co_return;
        }
    );

    brazier::Middleware authentication = brazier::authMiddleware(
        [](const FusedOnion::Request&) {
            return false;
        }
    );
    pipeline.middleware.use(authentication);

    auto request = MakeRequest();
    auto response = MakeResponse();
    FusedOnion::Params params;

    RunAwaitable(pipeline.run(request, response, params));

    EXPECT_FALSE(controller_called);
    EXPECT_EQ(response.result(), http::status::unauthorized);
}

TEST(FusedOnionTest, CorsMiddlewareFactoryAddsHeadersAndContinues) {
    bool controller_called = false;

    FusedOnion pipeline(
        [&controller_called](const auto&, auto&, const auto&) -> FusedOnion::Awaitable {
            controller_called = true;
            co_return;
        }
    );

    pipeline.middleware.use(brazier::corsMiddleware({"https://example.com"}));

    auto request = MakeRequest();
    request.set(http::field::origin, "https://example.com");
    auto response = MakeResponse();
    FusedOnion::Params params;

    RunAwaitable(pipeline.run(request, response, params));

    EXPECT_TRUE(controller_called);
    EXPECT_EQ(response[http::field::access_control_allow_origin], "https://example.com");
    EXPECT_EQ(response[http::field::access_control_allow_credentials], "true");
}

TEST(FusedOnionTest, CorsMiddlewareFactoryRejectsUnconfiguredOrigins) {
    bool controller_called = false;

    FusedOnion pipeline(
        [&controller_called](const auto&, auto&, const auto&) -> FusedOnion::Awaitable {
            controller_called = true;
            co_return;
        }
    );

    pipeline.middleware.use(brazier::corsMiddleware({"https://allowed.example"}));

    auto request = MakeRequest();
    request.set(http::field::origin, "https://blocked.example");
    auto response = MakeResponse();
    FusedOnion::Params params;

    RunAwaitable(pipeline.run(request, response, params));

    EXPECT_FALSE(controller_called);
    EXPECT_EQ(response.result(), http::status::forbidden);
}

TEST(FusedOnionTest, CorsMiddlewareFactoryHandlesOptionsPreflight) {
    bool controller_called = false;

    FusedOnion pipeline(
        [&controller_called](const auto&, auto&, const auto&) -> FusedOnion::Awaitable {
            controller_called = true;
            co_return;
        }
    );

    pipeline.middleware.use(brazier::corsMiddleware({"https://example.com"}));

    auto request = FusedOnion::Request{http::verb::options, "/", 11};
    request.set(http::field::origin, "https://example.com");
    auto response = MakeResponse();
    FusedOnion::Params params;

    RunAwaitable(pipeline.run(request, response, params));

    EXPECT_FALSE(controller_called);
    EXPECT_EQ(response.result(), http::status::no_content);
    EXPECT_EQ(response[http::field::access_control_allow_origin], "https://example.com");
}
