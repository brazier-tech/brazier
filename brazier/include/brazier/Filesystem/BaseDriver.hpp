/**
 * Copyright (c) 2026 Kirill Sergeev, Nikolay Sugonyako, Andrey Agarkov, Hlib Safiannikov.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This source code is licensed under the LGPL license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>
#include <memory>
#include <thread>
#include <boost/asio/awaitable.hpp>

 /**
  * @namespace brazier
  * @brief Namespace of the brazier library.
  */
namespace brazier {

    /**
     * @class BaseDriver
     * @brief Abstract base interface for a storage driver.
     *
     * @details
     * The interface defines synchronous and asynchronous operations on
     * files/objects in a storage backend. A concrete implementation defines
     * the storage type (local filesystem, S3, FTP, WebDAV, etc.), error
     * handling rules, overwrite behavior, and interpretation of relative paths.
     *
     * @note Asynchronous methods return @c boost::asio::awaitable and are
     * intended to be used from Boost.Asio coroutines. Whether initAsync()
     * must be called before asynchronous operations is implementation-defined.
     *
     * @warning The exception/error-handling contract is not fixed by this
     * interface and must be documented by each concrete driver.
     */
    class BaseDriver {
    public:
        /**
         * @brief Virtual destructor.
         */
        virtual ~BaseDriver() = default;

        /**
         * @brief Initializes the asynchronous infrastructure of the driver.
         *
         * @param[in] threadCount Number of threads used for asynchronous
         * processing. Defaults to @c std::thread::hardware_concurrency().
         *
         * @note If the implementation requires initialization before
         * asynchronous operations, this method must be called before using them.
         */
        virtual void initAsync(size_t threadCount = std::thread::hardware_concurrency()) = 0;

        /**
         * @brief Sets the root path of the driver.
         *
         * @param[in] path Root path against which subsequent operation paths
         * may be interpreted.
         *
         * @note The semantics of relative paths are implementation-defined.
         */
        virtual void setRootPath(const std::string& path) = 0;

        /**
         * @brief Returns the current root path of the driver.
         *
         * @return The root path.
         */
        virtual std::string getRootPath() const = 0;

        /**
         * @brief Stores content at the specified path.
         *
         * @param[in] path Path to the file/object.
         * @param[in] content Content to store.
         *
         * @note Behavior when the file/object already exists is
         * implementation-defined.
         */
        virtual void put(const std::string& path, const std::string& content) = 0;

        /**
         * @brief Reads content from the specified path.
         *
         * @param[in] path Path to the file/object.
         * @return Content of the file/object.
         */
        virtual std::string get(const std::string& path) = 0;

        /**
         * @brief Checks whether a file/object exists at the specified path.
         *
         * @param[in] path Path to the file/object.
         * @return @c true if the file/object exists; otherwise @c false.
         */
        virtual bool exists(const std::string& path) const = 0;

        /**
         * @brief Deletes the file/object at the specified path.
         *
         * @param[in] path Path to the file/object.
         */
        virtual void deleteFile(const std::string& path) = 0;

        /**
         * @brief Copies a file/object.
         *
         * @param[in] source Source path.
         * @param[in] destination Destination path.
         */
        virtual void copy(const std::string& source, const std::string& destination) = 0;

        /**
         * @brief Moves a file/object.
         *
         * @param[in] source Source path.
         * @param[in] destination Destination path.
         */
        virtual void move(const std::string& source, const std::string& destination) = 0;

        /**
         * @brief Asynchronously stores content at the specified path.
         *
         * @param[in] path Path to the file/object.
         * @param[in] content Content to store.
         * @return An asynchronous operation that completes without a return value.
         */
        virtual boost::asio::awaitable<void> putAsync(const std::string& path, const std::string& content) = 0;

        /**
         * @brief Asynchronously reads content from the specified path.
         *
         * @param[in] path Path to the file/object.
         * @return An asynchronous operation whose result is the content of the
         * file/object.
         */
        virtual boost::asio::awaitable<std::string> getAsync(const std::string& path) = 0;

        /**
         * @brief Asynchronously checks whether a file/object exists.
         *
         * @param[in] path Path to the file/object.
         * @return An asynchronous operation whose result is @c true if the
         * file/object exists; otherwise @c false.
         */
        virtual boost::asio::awaitable<bool> existsAsync(const std::string& path) const = 0;

        /**
         * @brief Asynchronously deletes the file/object at the specified path.
         *
         * @param[in] path Path to the file/object.
         * @return An asynchronous operation that completes without a return value.
         */
        virtual boost::asio::awaitable<void> deleteFileAsync(const std::string& path) = 0;

        /**
         * @brief Asynchronously copies a file/object.
         *
         * @param[in] source Source path.
         * @param[in] destination Destination path.
         * @return An asynchronous operation that completes without a return value.
         */
        virtual boost::asio::awaitable<void> copyAsync(const std::string& source, const std::string& destination) = 0;

        /**
         * @brief Asynchronously moves a file/object.
         *
         * @param[in] source Source path.
         * @param[in] destination Destination path.
         * @return An asynchronous operation that completes without a return value.
         */
        virtual boost::asio::awaitable<void> moveAsync(const std::string& source, const std::string& destination) = 0;

        /**
         * @brief Returns the driver type.
         *
         * @return String identifier of the driver type. Expected values are
         * described by the @ref S3, @ref File, @ref FTP, and @ref WebDAV
         * constants.
         */
        virtual std::string getDriverType() const = 0;

        /**
         * @brief Checks whether the driver supports asynchronous operations.
         *
         * @return @c true if asynchronous operations are supported; otherwise
         * @c false.
         */
        virtual bool isAsyncSupported() const = 0;
    };

    /**
     * @brief Identifier of the S3 driver.
     */
    constexpr auto S3 = "S3";

    /**
     * @brief Identifier of the filesystem driver.
     */
    constexpr auto File = "Filesystem";

    /**
     * @brief Identifier of the FTP driver.
     */
    constexpr auto FTP = "FTP";

    /**
     * @brief Identifier of the WebDAV driver.
     */
    constexpr auto WebDAV = "WebDAV";
}