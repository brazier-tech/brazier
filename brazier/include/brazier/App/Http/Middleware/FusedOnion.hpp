/**
* Copyright (c) 2026 Kirill Sergeev, Nikolay Sugonyako, Andrey Agarkov, Hlib Safiannikov.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This source code is licensed under the LGPL license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/beast/http.hpp>

#include <algorithm>
#include <any>
#include <atomic>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace brazier {

    /**
     * @brief FusedOnion is a route-local, coroutine-aware request pipeline.
     *
     * The registry is mutable, but each request executes one immutable snapshot. Registering/ejecting handlers rebuilds the snapshot. Normal request execution does not rebuild the chain or take the registry lock.
     */
    class FusedOnion {
    public:
        using Request = boost::beast::http::request<boost::beast::http::string_body>;
        using Response = boost::beast::http::response<boost::beast::http::string_body>;
        using Params = std::unordered_map<std::string, std::string>;
        using Awaitable = boost::asio::awaitable<void>;
        using Id = std::uint64_t;

        class Context {
        public:
            const Request& request;
            Response& response;
            const Params& params;

            Context(const Request& request, Response& response, const Params& params) : request(request), response(response), params(params) {}

            template <typename T>
            void set(T value) {
                data_.insert_or_assign(std::type_index(typeid(T)), std::any(std::move(value)));
            }

            template <typename T>
            T* get() noexcept {
                const auto it = data_.find(std::type_index(typeid(T)));
                return it == data_.end() ? nullptr : std::any_cast<T>(&it->second);
            }

            template <typename T>
            const T* get() const noexcept {
                const auto it = data_.find(std::type_index(typeid(T)));
                return it == data_.end() ? nullptr : std::any_cast<T>(&it->second);
            }

            template <typename T>
            void erase() {
                data_.erase(std::type_index(typeid(T)));
            }

            const std::exception_ptr& exception() const noexcept {
                return exception_;
            }

            // Replace the exception currently being processed by error hooks.
            void set_exception(std::exception_ptr exception) noexcept {
                exception_ = std::move(exception);
            }

            // Call this from an error interceptor to stop propagation.
            void handle_error() noexcept {
                error_handled_ = true;
            }

            bool error_handled() const noexcept {
                return error_handled_;
            }

            // Used by the pipeline when an error hook itself throws.
            void reset_error_handled() noexcept {
                error_handled_ = false;
            }

        private:
            std::unordered_map<std::type_index, std::any> data_;
            std::exception_ptr exception_;
            bool error_handled_ = false;

            friend class FusedOnion;
        };

        class Next {
        public:
            using Continuation = std::function<Awaitable()>;

            explicit Next(Continuation continuation) : state_(std::make_shared<State>(std::move(continuation))) {}

            // A continuation may be invoked at most once. Omitting next() is valid and short-circuits the remaining pipeline.
            Awaitable operator()() const {
                if (!state_)
                    throw std::logic_error("next() has no continuation");

                bool expected = false;
                if (!state_->invoked.compare_exchange_strong(
                        expected, true, std::memory_order_acq_rel))
                    throw std::logic_error("middleware called next() more than once");

                return state_->continuation();
            }

        private:
            struct State {
                explicit State(Continuation continuation) : continuation(std::move(continuation)) {}

                Continuation continuation;
                std::atomic_bool invoked{false};
            };

            std::shared_ptr<State> state_;
        };

        using Controller = std::function<Awaitable(const Request&, Response&, const Params&)>;
        using RequestInterceptor = std::function<Awaitable(Context&)>;
        using Middleware = std::function<Awaitable(Context&, Next)>;
        using ResponseInterceptor = std::function<Awaitable(Context&)>;
        using ErrorInterceptor = std::function<Awaitable(Context&)>;

        enum class Stage {
            request,
            middleware,
            response,
            error
        };

    private:
        using Endpoint = std::function<Awaitable(Context&)>;
        using Handler = std::variant<RequestInterceptor, Middleware, ResponseInterceptor, ErrorInterceptor>;

        struct Entry {
            Id id;
            Stage stage;
            Handler handler;
        };

        struct Snapshot {
            std::vector<RequestInterceptor> request;
            Endpoint onion;
            std::vector<ResponseInterceptor> response;
            std::vector<ErrorInterceptor> error;
        };

        struct State {
            explicit State(Controller controller)
                : controller(std::move(controller)) {
                publish();
            }

            Id add(Stage stage, Handler handler) {
                std::lock_guard lock(mutex);
                const Id id = next_id++;
                entries.push_back(Entry{id, stage, std::move(handler)});
                publish();
                return id;
            }

            bool eject(Stage stage, Id id) {
                std::lock_guard lock(mutex);
                const auto it = std::find_if(
                    entries.begin(), entries.end(),
                    [stage, id](const Entry& entry) {
                        return entry.stage == stage && entry.id == id;
                    }
                );

                if (it == entries.end())
                    return false;

                entries.erase(it);
                publish();
                return true;
            }

            void clear(Stage stage) {
                std::lock_guard lock(mutex);
                const auto old_size = entries.size();
                entries.erase(
                    std::remove_if(entries.begin(), entries.end(),
                        [stage](const Entry& entry) {
                            return entry.stage == stage;
                        }),
                    entries.end()
                );

                if (entries.size() != old_size)
                    publish();
            }

            std::size_t size(Stage stage) const {
                std::lock_guard lock(mutex);
                std::size_t result = 0;
                for (const auto& entry : entries) {
                    if (entry.stage == stage) {
                        ++result;
                    }
                }
                return result;
            }

            static Awaitable invoke_controller(Controller controller, Context& context) {
                if (controller)
                    co_await controller(context.request, context.response, context.params);
            }

            static Awaitable invoke_middleware(Middleware middleware, Endpoint downstream, Context& context) {
                Next next{
                    [downstream = std::move(downstream), &context]() mutable {
                        return downstream(context);
                    }
                };
                co_await middleware(context, std::move(next));
            }

            static Awaitable run_error_interceptors(const std::vector<ErrorInterceptor>& handlers, Context& context) {
                for (const auto& handler : handlers) {
                    try {
                        if (handler)
                            co_await handler(context);
                    }
                    catch (...) {
                        context.set_exception(std::current_exception());
                        context.reset_error_handled();
                    }

                    if (context.error_handled())
                        co_return;
                }
            }

            Endpoint compose(const std::vector<Middleware>& middleware) const {
                Endpoint endpoint = [controller = this->controller](Context& context) {
                    return invoke_controller(controller, context);
                };

                for (auto it = middleware.rbegin(); it != middleware.rend(); ++it) {
                    auto downstream = std::move(endpoint);
                    auto current = *it;
                    endpoint = [current = std::move(current),
                                downstream = std::move(downstream)](Context& context) mutable {
                        return invoke_middleware(current, downstream, context);
                    };
                }

                return endpoint;
            }

            void publish() {
                auto next = std::make_shared<Snapshot>();
                std::vector<Middleware> middleware;

                for (const auto& entry : entries) {
                    switch (entry.stage) {
                    case Stage::request:
                        next->request.push_back(std::get<0>(entry.handler));
                        break;
                    case Stage::middleware:
                        middleware.push_back(std::get<1>(entry.handler));
                        break;
                    case Stage::response:
                        next->response.push_back(std::get<2>(entry.handler));
                        break;
                    case Stage::error:
                        next->error.push_back(std::get<3>(entry.handler));
                        break;
                    }
                }

                next->onion = compose(middleware);
                publish_snapshot(std::move(next));
            }

            Controller controller;
            mutable std::mutex mutex;
            std::vector<Entry> entries;
            Id next_id = 1;
            std::shared_ptr<const Snapshot> active;

            void publish_snapshot(std::shared_ptr<const Snapshot> snapshot) {
                std::atomic_store_explicit(
                    &active,
                    std::move(snapshot),
                    std::memory_order_release
                );
            }
            [[nodiscard]]
            std::shared_ptr<const Snapshot> load_snapshot() const {
                return std::atomic_load_explicit(
                    &active,
                    std::memory_order_acquire
                );
            }
        };

    private:
        // Declare state before the public manager handles: member declaration order instead of initialiser-list order to control construction.
        std::shared_ptr<State> state_;

    public:
        template <typename HandlerType, Stage ManagerStage>
        class Manager {
        public:
            Id use(HandlerType handler) const {
                constexpr auto index = static_cast<std::size_t>(ManagerStage);
                return state_->add(
                    ManagerStage,
                    Handler(std::in_place_index<index>, std::move(handler))
                );
            }

            bool eject(Id id) const {
                return state_->eject(ManagerStage, id);
            }

            void clear() const {
                state_->clear(ManagerStage);
            }

            std::size_t size() const {
                return state_->size(ManagerStage);
            }

        private:
            explicit Manager(std::shared_ptr<State> state) : state_(std::move(state)) {}

            std::shared_ptr<State> state_;
            friend class FusedOnion;
        };

        using RequestManager = Manager<RequestInterceptor, Stage::request>;
        using MiddlewareManager = Manager<Middleware, Stage::middleware>;
        using ResponseManager = Manager<ResponseInterceptor, Stage::response>;
        using ErrorManager = Manager<ErrorInterceptor, Stage::error>;

        RequestManager request;
        MiddlewareManager middleware;
        ResponseManager response;
        ErrorManager error;

        explicit FusedOnion(Controller controller) : state_(std::make_shared<State>(std::move(controller))), request(state_), middleware(state_), response(state_), error(state_) {}

        Awaitable run(const Request& request, Response& response, const Params& params) const {
            // Keep the snapshot alive for the entire coroutine, even when use() or eject() publishes a newer version concurrently.
            return execute(state_->load_snapshot(), request, response, params);
        }

        static Middleware pre(RequestInterceptor before) {
            return [before = std::move(before)](Context& context, Next next) mutable {
                return invoke_pre(before, context, std::move(next));
            };
        }

        static Middleware post(ResponseInterceptor after) {
            return [after = std::move(after)](Context& context, Next next) mutable {
                return invoke_post(after, context, std::move(next));
            };
        }

        static Middleware wrap(RequestInterceptor before, ResponseInterceptor after) {
            return [before = std::move(before), after = std::move(after)]
                (Context& context, Next next) mutable {
                    return invoke_wrap(before, after, context, std::move(next));
                };
        }

    private:
        static Awaitable invoke_pre(RequestInterceptor before, Context& context, Next next) {
            if (before)
                co_await before(context);
            co_await next();
        }

        static Awaitable invoke_post(ResponseInterceptor after, Context& context, Next next
        ) {
            co_await next();
            if (after)
                co_await after(context);
        }

        static Awaitable invoke_wrap(RequestInterceptor before, ResponseInterceptor after, Context& context, Next next
        ) {
            if (before)
                co_await before(context);
            co_await next();
            if (after)
                co_await after(context);
        }

        static Awaitable execute(std::shared_ptr<const Snapshot> snapshot, const Request& request, Response& response, const Params& params) {
            Context context(request, response, params);
            std::exception_ptr failure;

            try {
                for (const auto& handler : snapshot->request) {
                    if (handler)
                        co_await handler(context);
                }

                if (snapshot->onion)
                    co_await snapshot->onion(context);
            }
            catch (...) {
                failure = std::current_exception();
            }

            if (failure) {
                context.set_exception(failure);
                context.reset_error_handled();
                co_await State::run_error_interceptors(snapshot->error, context);

                if (!context.error_handled()) {
                    std::rethrow_exception(context.exception());
                }
            }

            // Response hooks also run after a recovered exception to allow for common headers, compression, and response instrumentation.
            failure = nullptr;
            try {
                for (const auto& handler : snapshot->response) {
                    if (handler)
                        co_await handler(context);
                }
            }
            catch (...) {
                failure = std::current_exception();
            }

            if (failure) {
                context.set_exception(failure);
                context.reset_error_handled();
                co_await State::run_error_interceptors(snapshot->error, context);

                if (!context.error_handled())
                    std::rethrow_exception(context.exception());
            }
        }
    };

    using Pipeline = FusedOnion;
    using Context = FusedOnion::Context;
    using Next = FusedOnion::Next;
    using Awaitable = FusedOnion::Awaitable;
    using Middleware = FusedOnion::Middleware;

}
