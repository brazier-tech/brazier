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
#include <shared_mutex>

namespace brazier {
    /**
     * @brief FusedOnion is a route-local, coroutine-aware request pipeline.
     *
     * The registry is mutable, but each request executes one immutable snapshot.
     * Registering/ejecting handlers rebuilds the snapshot.
     * Normal request execution does not rebuild the chain or take the registry lock.
     */
    class FusedOnion {
    public:
        using Request = boost::beast::http::request<boost::beast::http::string_body>;
        using Response = boost::beast::http::response<boost::beast::http::string_body>;
        using Params = std::unordered_map<std::string, std::string>;
        using Awaitable = boost::asio::awaitable<void>;
        using Id = std::uint64_t;

        /**
         * @brief Per-request storage and state shared by every handler in the pipeline.
         *
         * Provides:
         *  - read-only access to the request, response, and route params
         *  - a type-erased key/value store for arbitrary per-request data
         *  - the exception currently being processed by error interceptors
         *  - a `handled` flag that error interceptors use to stop propagation
         */
        class Context {
        public:
            const Request &request;
            Response &response;
            const Params &params;

            /**
             * @brief Constructs a request context.
             *
             * @param request Reference to the incoming HTTP request.
             * @param response Reference to the outgoing HTTP response.
             * @param params Reference to the route path parameters.
             */
            Context(const Request &request, Response &response, const Params &params) : request(request), response(response), params(params) {
            }

            /**
             * @brief Stores or replaces a value of type @p T in the context.
             *
             * The value is copy/move-assigned into an internal type-erased map keyed by
             * @c std::type_index. Storing a second value of the same type overwrites the first.
             *
             * @tparam T Type of the value to store.
             * @param value Value to store.
             */
            template<typename T>
            void set(T value) {
                data_.insert_or_assign(std::type_index(typeid(T)), std::any(std::move(value)));
            }

            /**
             * @brief Retrieves a mutable pointer to a stored value of type @p T.
             *
             * @tparam T Type of the stored value.
             * @return Pointer to the stored value, or @c nullptr if no value of type @p T is present.
             */
            template<typename T>
            T *get() noexcept {
                const auto it = data_.find(std::type_index(typeid(T)));
                return it == data_.end() ? nullptr : std::any_cast<T>(&it->second);
            }

            /**
             * @brief Retrieves a const pointer to a stored value of type @p T.
             *
             * @tparam T Type of the stored value.
             * @return Const pointer to the stored value, or @c nullptr if no value of type @p T is present.
             */
            template<typename T>
            const T *get() const noexcept {
                const auto it = data_.find(std::type_index(typeid(T)));
                return it == data_.end() ? nullptr : std::any_cast<T>(&it->second);
            }

            /**
             * @brief Removes a stored value of type @p T from the context.
             *
             * No-op if no value of type @p T is present.
             *
             * @tparam T Type of the value to erase.
             */
            template<typename T>
            void erase() {
                data_.erase(std::type_index(typeid(T)));
            }

            /**
             * @brief Returns the exception currently associated with the context.
             *
             * Set by the pipeline before invoking error interceptors, and rewritable from an
             * error interceptor via set_exception().
             *
             * @return Reference to the stored exception pointer; empty when no error has occurred.
             */
            [[nodiscard]] const std::exception_ptr &exception() const noexcept {
                return exception_;
            }

            void set_exception(std::exception_ptr exception) noexcept {
                exception_ = std::move(exception);
            }

            // stops error propagation.
            void handle_error() noexcept {
                error_handled_ = true;
            }

            /**
             * @brief Checks whether an error interceptor has handled the current exception.
             *
             * @return @c true if a handler has called handle_error(), @c false otherwise.
             */
            [[nodiscard]] bool error_handled() const noexcept {
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

        /**
         * @brief Handle passed to a middleware, used to invoke the downstream chain.
         *
         * A middleware may invoke the continuation at most once (or not at all, which short-circuits the rest of the pipeline).
         * Calling it more than once, or calling it when it was constructed without a continuation, throws @c std::logic_error.
         */
        class Next {
        public:
            // return awaitable downstream chain
            using Continuation = std::function<Awaitable()>;

            /**
             * @brief Constructs a Next wrapper around the given continuation.
             *
             * @param continuation Callable that, when invoked, runs the downstream chain.
             */
            explicit Next(Continuation continuation)
                : state_(std::make_shared<State>(std::move(continuation))) {
            }

            // A continuation may be invoked at most once. Omitting next() is valid and short-circuits the remaining pipeline.
            Awaitable operator()() const {
                if (!state_) throw std::logic_error("next() has no continuation");

                bool expected = false;
                if (!state_->invoked.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) throw std::logic_error("middleware called next() more than once");

                return state_->continuation();
            }

        private:
            struct State {
                explicit State(Continuation continuation)
                    : continuation(std::move(continuation)) {
                }

                Continuation continuation;
                std::atomic_bool invoked{false};
            };

            std::shared_ptr<State> state_;
        };

        using Controller = std::function<Awaitable(const Request &, Response &, const Params &)>;
        using RequestInterceptor = std::function<Awaitable(Context &)>;
        using Middleware = std::function<Awaitable(Context &, Next)>;
        using ResponseInterceptor = std::function<Awaitable(Context &)>;
        using ErrorInterceptor = std::function<Awaitable(Context &)>;

        /**
         * @brief Pipeline stages in registration order.
         *
         * @note Handlers of each stage are invoked in the order they were registered.
         */
        enum class Stage {
            request,
            middleware,
            response,
            error
        };

    private:
        using Endpoint = std::function<Awaitable(Context &)>;
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
            /**
             * @brief Constructs the state around a controller and publishes the initial snapshot.
             *
             * @param controller Endpoint executed at the end of the middleware chain.
             */
            explicit State(Controller controller)
                : controller(std::move(controller)) {
                publish();
            }

            /**
             * @brief Registers a handler and republishes the pipeline snapshot.
             *
             * @param stage Stage the handler belongs to.
             * @param handler Handler stored in the variant slot matching @p stage.
             * @return Unique identifier of the registered handler, usable with eject().
             */
            Id add(Stage stage, Handler handler) {
                std::lock_guard lock(mutex);
                const Id id = next_id++;
                entries.push_back(Entry{id, stage, std::move(handler)});
                publish();
                return id;
            }

            /**
             * @brief Removes a handler identified by stage and ID.
             *
             * On success the snapshot is republished, so subsequent requests observe the removal.
             * In-flight requests keep using the snapshot they started with.
             *
             * @param stage Stage the handler belongs to.
             * @param id Identifier returned by add().
             * @return @c true if the handler was removed, @c false if no match was found.
             */
            bool eject(Stage stage, Id id) {
                std::lock_guard lock(mutex);
                const auto it = std::ranges::find_if(entries, [stage, id](const Entry &entry) {
                    return entry.stage == stage && entry.id == id;
                });

                if (it == entries.end())
                    return false;

                entries.erase(it);
                publish();
                return true;
            }

            /**
             * @brief Removes all handlers belonging to a single stage.
             *
             * The snapshot is only republished if at least one handler was actually removed.
             *
             * @param stage Stage whose handlers should be removed.
             */
            void clear(Stage stage) {
                std::lock_guard lock(mutex);
                const auto old_size = entries.size();
                std::erase_if(entries, [stage](const Entry &entry) {
                    return entry.stage == stage;
                });

                if (entries.size() != old_size)
                    publish();
            }

            /**
             * @brief Counts the number of handlers registered for a stage.
             *
             * @param stage Stage to query.
             * @return Number of handlers currently registered for @p stage.
             */
            std::size_t size(Stage stage) const {
                std::lock_guard lock(mutex);
                std::size_t result = 0;
                for (const auto &entry: entries) {
                    if (entry.stage == stage)
                        ++result;
                }
                return result;
            }

            /**
             * @brief Invokes the controller if it is set.
             *
             * @param controller Controller to invoke; may be empty.
             * @param context Request context forwarded to the controller.
             */
            static Awaitable invoke_controller(Controller controller, const Context &context) {
                if (controller)
                    co_await controller(context.request, context.response, context.params);
            }

            /**
             * @brief Invokes a middleware with a Next object that wraps the downstream endpoint.
             *
             * @param middleware Middleware to invoke.
             * @param downstream Next endpoint in the chain.
             * @param context Request context forwarded to the middleware.
             */
            static Awaitable invoke_middleware(Middleware middleware, Endpoint downstream, Context &context) {
                Next next{
                    [downstream = std::move(downstream), &context]() mutable {
                        return downstream(context);
                    }
                };
                co_await middleware(context, std::move(next));
            }

            /**
             * @brief Runs the error interceptors against @p context.
             *
             * Each interceptor is invoked in registration order.
             * If an interceptor throws, the new exception replaces the one stored on the context and the handled flag is reset so later interceptors still see the error.
             * Iteration stops as soon as any interceptor calls `Context::handle_error()`.
             *
             * @param handlers Error interceptors to run.
             * @param context Request context whose exception is being processed.
             */
            static Awaitable run_error_interceptors(const std::vector<ErrorInterceptor> &handlers, Context &context) {
                for (const auto &handler: handlers) {
                    try {
                        if (handler) co_await handler(context);
                    } catch (...) {
                        context.set_exception(std::current_exception());
                        context.reset_error_handled();
                    }

                    if (context.error_handled())
                        co_return;
                }
            }

            /**
             * @brief Builds the onion-style endpoint chain from the given middleware.
             *
             * Middleware registered earlier ends up on the outside of the resulting chain, so it sees the request first and the response last.
             * The innermost endpoint invokes the controller.
             *
             * @param middleware Middleware in registration order.
             * @return Endpoint that runs the middleware chain and then the controller.
             */
            Endpoint compose(const std::vector<Middleware> &middleware) const {
                Endpoint endpoint = [controller = this->controller](const Context &context) {
                    return invoke_controller(controller, context);
                };

                for (auto it = middleware.rbegin(); it != middleware.rend(); ++it) {
                    auto downstream = std::move(endpoint);
                    auto current = *it;
                    endpoint = [current = std::move(current), downstream = std::move(downstream)](Context &context) mutable {
                        return invoke_middleware(current, downstream, context);
                    };
                }

                return endpoint;
            }

            /**
             * @brief Rebuilds the immutable pipeline snapshot from the current entries and publishes it.
             *
             * Copies the registered handlers into a fresh Snapshot, recomposes the middleware chain, and installs the result as the active snapshot. Must be called with @c mutex held.
             */
            void publish() {
                auto next = std::make_shared<Snapshot>();
                std::vector<Middleware> middleware;

                for (const auto &entry: entries) {
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
#if defined(__cpp_lib_atomic_shared_ptr) && \
                __cpp_lib_atomic_shared_ptr >= 201711L

            std::atomic<std::shared_ptr<const Snapshot> > active{};

#else

            mutable std::shared_mutex snapshot_mutex;
            std::shared_ptr<const Snapshot> active;

#endif

            /**
             * @brief Publishes a new immutable pipeline snapshot.
             *
             * @param snapshot Snapshot to publish.
             *
             * @details Uses release semantics when atomic shared pointers are supported.
             * Otherwise, protects the shared pointer with a mutex.
             */
            void publish_snapshot(std::shared_ptr<const Snapshot> snapshot) {
#if defined(__cpp_lib_atomic_shared_ptr) && \
                    __cpp_lib_atomic_shared_ptr >= 201711L

                active.store(std::move(snapshot), std::memory_order_release);

#else

                std::unique_lock lock(snapshot_mutex);
                active = std::move(snapshot);

#endif
            }

            /**
             * @brief Loads the currently published pipeline snapshot.
             *
             * @return A shared pointer to an immutable snapshot.
             *
             * @details Uses acquire semantics when atomic shared pointers are supported.
             * Otherwise, acquires a shared lock while copying the pointer.
             */
            [[nodiscard]] std::shared_ptr<const Snapshot> load_snapshot() const {
#if defined(__cpp_lib_atomic_shared_ptr) && \
                    __cpp_lib_atomic_shared_ptr >= 201711L

                return active.load(std::memory_order_acquire);

#else

                std::shared_lock lock(snapshot_mutex);
                return active;

#endif
            }
        };

    private:
        // Declare state before the public manager handles: member declaration order instead of initialiser-list order to control construction.
        std::shared_ptr<State> state_;

    public:
        template<typename HandlerType, Stage ManagerStage>
        /**
         * @brief Handle to allow registration, removal, and inspection of handlers for one Stage.
         *
         * The three public managers (@ref request, @ref middleware, @ref response, @ref error)
         * are instances of this template specialised with different stage tags.
         *
         * @tparam HandlerType Handler type accepted by use().
         * @tparam ManagerStage Stage this manager administers.
         */
        class Manager {
        public:
            /**
             * @brief Registers a handler of the manager's stage and republishes the snapshot.
             *
             * @param handler Handler to register.
             * @return Unique identifier that can be passed to eject().
             */
            Id use(HandlerType handler) const {
                constexpr auto index = static_cast<std::size_t>(ManagerStage);
                return state_->add(ManagerStage, Handler(std::in_place_index<index>, std::move(handler)));
            }

            /**
             * @brief Removes a handler previously registered with use().
             *
             * @param id Identifier returned by use().
             * @return @c true if the handler was removed, @c false if no match was found.
             */
            bool eject(Id id) const {
                return state_->eject(ManagerStage, id);
            }

            /**
             * @brief Removes every handler registered for the manager's stage.
             */
            void clear() const {
                state_->clear(ManagerStage);
            }

            /**
             * @brief Returns the number of handlers registered for the manager's stage.
             *
             * @return Number of registered handlers.
             */
            [[nodiscard]] std::size_t size() const {
                return state_->size(ManagerStage);
            }

        private:
            /**
             * @brief Constructs a manager bound to a shared pipeline state.
             *
             * Only FusedOnion may construct managers, so that they are always bound to the
             * correct state.
             *
             * @param state Pipeline state the manager operates on.
             */
            explicit Manager(std::shared_ptr<State> state) : state_(std::move(state)) {
            }

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

        /**
         * @brief Constructs a pipeline around a controller with empty handler lists.
         *
         * @param controller Endpoint invoked at the end of the middleware chain.
         */
        explicit FusedOnion(Controller controller) : state_(std::make_shared<State>(std::move(controller))), request(state_), middleware(state_), response(state_), error(state_) {
        }

        /**
         * @brief Executes the pipeline for a single request.
         *
         * The current immutable snapshot is loaded once and used for the entire coroutine;
         * concurrent calls to use()/eject() only affect subsequent requests.
         *
         * @param request Incoming HTTP request.
         * @param response Outgoing HTTP response.
         * @param params Route path parameters.
         * @return Awaitable that completes when the pipeline has finished.
         * @throws Rethrows the first unhandled exception produced by the chain or by an error
         *         interceptor (see execute() for details).
         */
        Awaitable run(const Request &request, Response &response, const Params &params) const {
            // Keep the snapshot alive for the entire coroutine, even when use() or eject() publishes a newer version concurrently.
            return execute(state_->load_snapshot(), request, response, params);
        }

        /**
         * @brief Creates a middleware that runs @p before and then delegates downstream.
         *
         * If the chain is short-circuited by the middleware the interceptor is passed to, that
         * middleware may choose not to call next().
         *
         * @param before Request interceptor to run before the rest of the chain.
         * @return Middleware suitable for registration with @ref middleware.
         */
        static Middleware pre(RequestInterceptor before) {
            return [before = std::move(before)](Context &context, Next next) mutable {
                return invoke_pre(before, context, std::move(next));
            };
        }

        /**
         * @brief Creates a middleware that delegates downstream and then runs @p after.
         *
         * The response interceptor runs even if the downstream chain later throws, as long as
         * the middleware is placed before the throwing stage in the chain.
         *
         * @param after Response interceptor to run after the rest of the chain.
         * @return Middleware suitable for registration with @ref middleware.
         */
        static Middleware post(ResponseInterceptor after) {
            return [after = std::move(after)](Context &context, Next next) mutable {
                return invoke_post(after, context, std::move(next));
            };
        }

        /**
         * @brief Creates a middleware that runs @p before, delegates downstream, then runs @p after.
         *
         * Convenience combination of pre() and post() with a single registration.
         *
         * @param before Request interceptor to run before the rest of the chain.
         * @param after  Response interceptor to run after the rest of the chain.
         * @return Middleware suitable for registration with @ref middleware.
         */
        static Middleware wrap(RequestInterceptor before, ResponseInterceptor after) {
            return [before = std::move(before), after = std::move(after)](Context &context, Next next) mutable {
                return invoke_wrap(before, after, context, std::move(next));
            };
        }

    private:
        /**
         * @brief Implementation of pre(): runs @p before and then @p next.
         *
         * @param before Request interceptor to invoke; may be empty.
         * @param context Request context.
         * @param next Downstream continuation.
         */
        static Awaitable invoke_pre(RequestInterceptor before, Context &context, Next next) {
            if (before)
                co_await before(context);
            co_await next();
        }

        /**
         * @brief Implementation of post(): runs @p next and then @p after.
         *
         * @param after Response interceptor to invoke; may be empty.
         * @param context Request context.
         * @param next Downstream continuation.
         */
        static Awaitable invoke_post(ResponseInterceptor after, Context &context, Next next) {
            co_await next();
            if (after)
                co_await after(context);
        }

        /**
         * @brief Implementation of wrap(): runs @p before, @p next, then @p after.
         *
         * @param before Request interceptor to invoke; may be empty.
         * @param after  Response interceptor to invoke; may be empty.
         * @param context Request context.
         * @param next Downstream continuation.
         */
        static Awaitable invoke_wrap(RequestInterceptor before, ResponseInterceptor after, Context &context, Next next) {
            if (before)
                co_await before(context);
            co_await next();
            if (after)
                co_await after(context);
        }

        /**
         * @brief Runs a request through an immutable snapshot of the pipeline.
         *
         * Request interceptors will be run through in order first, followed by the composed middleware chain to the controller, after which response interceptors will be run through.
         *  1. Request interceptors, in order.
         *  2. The composed middleware chain, terminating in the controller.
         *  3. Response interceptors, in order.
         *
         * Any exception thrown by the first two stages is captured and stored on the context to be passed to the error interceptors.
         * If an error interceptor does not mark the error as handled, the (possibly replaced) exception is rethrown.
         * Response interceptors still run even after a recovered exception, so that cross-cutting response work (headers, compression, instrumentation) is always applied.
         *
         * The same logic runs for exceptions thrown by the response interceptors themselves.
         *
         * @param snapshot Immutable pipeline snapshot; kept alive for the whole coroutine.
         * @param request  Incoming HTTP request.
         * @param response Outgoing HTTP response.
         * @param params   Route path parameters.
         * @throws Rethrows the first unhandled exception produced by the chain or by an error interceptor.
         */
        static Awaitable execute(std::shared_ptr<const Snapshot> snapshot, const Request &request, Response &response, const Params &params) {
            Context context(request, response, params);
            std::exception_ptr failure;

            try {
                for (const auto &handler: snapshot->request) {
                    if (handler)
                        co_await handler(context);
                }

                if (snapshot->onion) co_await snapshot->onion(context);
            } catch (...) {
                failure = std::current_exception();
            }

            if (failure) {
                context.set_exception(failure);
                context.reset_error_handled();
                co_await State::run_error_interceptors(snapshot->error, context);

                if (!context.error_handled())
                    std::rethrow_exception(context.exception());
            }

            // Response hooks also run after a recovered exception to allow for common headers, compression, and response instrumentation.
            failure = nullptr;
            try {
                for (const auto &handler: snapshot->response) {
                    if (handler) co_await handler(context);
                }
            } catch (...) {
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
