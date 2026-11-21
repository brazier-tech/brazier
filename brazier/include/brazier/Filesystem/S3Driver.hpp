/**
 * Copyright (c) 2026 Kirill Sergeev, Nikolay Sugonyako, Andrey Agarkov, Hlib Safiannikov.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This source code is licensed under the LGPL license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "BaseDriver.hpp"
#include <filesystem>
#include <boost/asio/thread_pool.hpp>

namespace brazier {

    namespace fs = std::filesystem;

    /**
     * @class S3Driver
     * @brief S3 implementation of @ref BaseDriver.
     *
     * @details
     * Implements the @ref BaseDriver interface for an S3-backed storage.
     *
     * The class mirrors the structure of @ref FileDriver: it holds a root path,
     * a lazily created Boost.Asio thread pool, and an initialization flag, and
     * declares both synchronous and asynchronous operation sets.
     *
     * @note The concrete behavior (path interpretation, error handling,
     * overwrite semantics, credential configuration) is not expressed by this
     * header and must be documented by the implementation.
     *
     * @warning This header does not include `<thread>` or `<memory>`, yet the
     * interface uses @c std::thread::hardware_concurrency() and
     * @c std::shared_ptr. It currently relies on transitive includes from
     * @ref BaseDriver and Boost headers; adding the includes explicitly is
     * recommended.
     */
    class S3Driver : public BaseDriver {
    private:
        /**
         * @brief Root path used by the driver.
         *
         * @note Its meaning for an S3 backend (bucket, prefix, or both) is
         * implementation-defined.
         */
        std::string rootPath_;

        /**
         * @brief Thread pool used by asynchronous operations.
         *
         * @details Expected to be created by @c initAsync(). Null until then.
         */
        std::shared_ptr<boost::asio::thread_pool> ioPool_;

        /**
         * @brief Reserved flag for asynchronous initialization state.
         *
         * @note Not used by any code visible in this header.
         */
        bool asyncInitialized_ = false;

    public:
        /**
         * @brief Constructs an S3Driver.
         */
        S3Driver() = default;

        /**
         * @brief Destructor.
         */
        ~S3Driver() override = default;

        /**
         * @brief Initializes the asynchronous thread pool.
         *
         * @param[in] threadCount Number of threads in the pool. Defaults to
         * @c std::thread::hardware_concurrency().
         *
         * @note Whether asynchronous operations require a prior call to this
         * method is implementation-defined; similar to @ref FileDriver.
         */
        void initAsync(size_t threadCount = std::thread::hardware_concurrency()) override;

        /**
         * @brief Sets the root path.
         *
         * @param[in] path Root path (for example, a bucket or a prefix within a
         * bucket). Semantics are implementation-defined.
         */
        void setRootPath(const std::string& path) override;

        /**
         * @brief Returns the current root path.
         *
         * @return Root path.
         */
        std::string getRootPath() const override;

        /**
         * @brief Stores an object.
         *
         * @param[in] path Object path.
         * @param[in] content Object content.
         *
         * @note Overwrite behavior and error reporting are
         * implementation-defined.
         */
        void put(const std::string& path, const std::string& content) override;

        /**
         * @brief Reads an object.
         *
         * @param[in] path Object path.
         * @return Object content.
         *
         * @note Behavior when the object is missing is implementation-defined.
         */
        std::string get(const std::string& path) override;

        /**
         * @brief Checks whether an object exists.
         *
         * @param[in] path Object path.
         * @return @c true if the object exists; otherwise @c false.
         */
        bool exists(const std::string& path) const override;

        /**
         * @brief Deletes an object.
         *
         * @param[in] path Object path.
         *
         * @note Behavior when the object is missing is implementation-defined.
         */
        void deleteFile(const std::string& path) override;

        /**
         * @brief Copies an object.
         *
         * @param[in] source Source object path.
         * @param[in] destination Destination object path.
         *
         * @note Whether this is implemented as a server-side copy, download +
         * upload, or otherwise is implementation-defined.
         */
        void copy(const std::string& source, const std::string& destination) override;

        /**
         * @brief Moves an object.
         *
         * @param[in] source Source object path.
         * @param[in] destination Destination object path.
         *
         * @note S3 has no native move; the implementation may combine copy and
         * delete. Overwrite behavior is implementation-defined.
         */
        void move(const std::string& source, const std::string& destination) override;

        /**
         * @brief Asynchronously stores an object.
         *
         * @param[in] path Object path.
         * @param[in] content Object content.
         * @return Awaitable operation.
         */
        boost::asio::awaitable<void> putAsync(const std::string& path, const std::string& content) override;

        /**
         * @brief Asynchronously reads an object.
         *
         * @param[in] path Object path.
         * @return Awaitable operation producing the object content.
         */
        boost::asio::awaitable<std::string> getAsync(const std::string& path) override;

        /**
         * @brief Asynchronously checks object existence.
         *
         * @param[in] path Object path.
         * @return Awaitable operation producing @c true if the object exists;
         * otherwise @c false.
         */
        boost::asio::awaitable<bool> existsAsync(const std::string& path) const override;

        /**
         * @brief Asynchronously deletes an object.
         *
         * @param[in] path Object path.
         * @return Awaitable operation.
         */
        boost::asio::awaitable<void> deleteFileAsync(const std::string& path) override;

        /**
         * @brief Asynchronously copies an object.
         *
         * @param[in] source Source object path.
         * @param[in] destination Destination object path.
         * @return Awaitable operation.
         */
        boost::asio::awaitable<void> copyAsync(const std::string& source, const std::string& destination) override;

        /**
         * @brief Asynchronously moves an object.
         *
         * @param[in] source Source object path.
         * @param[in] destination Destination object path.
         * @return Awaitable operation.
         */
        boost::asio::awaitable<void> moveAsync(const std::string& source, const std::string& destination) override;

        /**
         * @brief Returns the driver type identifier.
         *
         * @return @ref brazier::S3 (@"S3").
         */
        std::string getDriverType() const override { return S3; }

        /**
         * @brief Indicates that asynchronous operations are supported.
         *
         * @return Always @c true.
         */
        bool isAsyncSupported() const override { return true; }
    };
}