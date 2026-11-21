/**
 * Copyright (c) 2026 Kirill Sergeev, Nikolay Sugonyako, Andrey Agarkov, Hlib Safiannikov.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This source code is licensed under the LGPL license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "BaseDriver.hpp"
#include "FileDriver.hpp"
#include "StorageManager.hpp"
#include <memory>
#include <optional>

namespace brazier {

    /**
     * @class Filesystem
     * @brief High-level facade for performing storage operations through a driver.
     *
     * @details
     * Filesystem owns a @ref BaseDriver instance and forwards all storage
     * operations to it. The driver is obtained in one of three ways:
     * - the default driver registered in @ref StorageManager (default constructor);
     * - a named driver registered in @ref StorageManager (name constructor and the static driver() factory);
     * - an externally supplied driver (shared_ptr constructor).
     *
     * All synchronous and asynchronous operations are thin wrappers around the
     * corresponding @ref BaseDriver methods; error and exception behavior is
     * determined by the underlying driver.
     *
     * @note Copies of a Filesystem share the same driver instance because the
     * driver is held by @c std::shared_ptr.
     */
    class Filesystem {
    private:
        /**
         * @brief Underlying storage driver.
         */
        std::shared_ptr<BaseDriver> driver_;

        /**
         * @brief Name under which the driver was resolved.
         *
         * @details
         * Set to @"default" for the default constructor, to the requested name
         * for the name constructor and driver(), and to @"custom" for the
         * shared_ptr constructor.
         */
        std::string driverName_;

    public:
        /**
         * @brief Constructs a Filesystem using the default driver.
         *
         * @details
         * Retrieves the default driver from @ref StorageManager and sets
         * @c driverName_ to @"default".
         *
         * @throws std::runtime_error If no default driver is registered.
         */
        Filesystem();

        /**
         * @brief Constructs a Filesystem using a named driver.
         *
         * @param[in] name Name of the driver registered in @ref StorageManager.
         *
         * @throws std::runtime_error If no driver with the given name is
         * registered.
         */
        explicit Filesystem(const std::string& name);

        /**
         * @brief Constructs a Filesystem from an externally supplied driver.
         *
         * @param[in] driver Driver instance to use. Must not be null.
         *
         * @details
         * Sets @c driverName_ to @"custom".
         *
         * @throws std::runtime_error If @p driver is null.
         */
        explicit Filesystem(std::shared_ptr<BaseDriver> driver);

        /**
         * @brief Creates a Filesystem for the named driver.
         *
         * @param[in] name Name of the driver registered in @ref StorageManager.
         * @return Filesystem bound to the named driver.
         *
         * @throws std::runtime_error If no driver with the given name is
         * registered.
         */
        static Filesystem driver(const std::string& name);

        /**
         * @brief Checks whether a driver with the given name is registered.
         *
         * @param[in] name Driver name.
         * @return @c true if the driver is registered; otherwise @c false.
         */
        static bool hasDriver(const std::string& name);

        /**
         * @brief Returns the name under which the driver was resolved.
         *
         * @return @"default", the requested driver name, or @"custom".
         */
        std::string getDriverName() const;

        /**
         * @brief Returns the driver type.
         *
         * @return Driver type identifier as reported by the underlying driver
         * (for example @ref brazier::S3, @ref brazier::File, @ref brazier::FTP,
         * @ref brazier::WebDAV).
         */
        std::string getDriverType() const;

        /**
         * @brief Reads content from the specified path.
         *
         * @param[in] path Path interpreted by the underlying driver.
         * @return File/object content.
         *
         * @note Exception behavior is determined by the underlying driver.
         */
        std::string get(const std::string& path);

        /**
         * @brief Checks whether a file/object exists at the specified path.
         *
         * @param[in] path Path interpreted by the underlying driver.
         * @return @c true if the path exists; otherwise @c false.
         */
        bool exists(const std::string& path) const;

        /**
         * @brief Stores content at the specified path.
         *
         * @param[in] path Path interpreted by the underlying driver.
         * @param[in] content Content to store.
         *
         * @note Exception behavior is determined by the underlying driver.
         */
        void put(const std::string& path, const std::string& content);

        /**
         * @brief Deletes the file/object at the specified path.
         *
         * @param[in] path Path interpreted by the underlying driver.
         *
         * @note Exception behavior is determined by the underlying driver.
         */
        void deleteFile(const std::string& path);

        /**
         * @brief Copies a file/object.
         *
         * @param[in] source Source path.
         * @param[in] destination Destination path.
         *
         * @note Exception behavior is determined by the underlying driver.
         */
        void copy(const std::string& source, const std::string& destination);

        /**
         * @brief Moves a file/object.
         *
         * @param[in] source Source path.
         * @param[in] destination Destination path.
         *
         * @note Exception behavior is determined by the underlying driver.
         */
        void move(const std::string& source, const std::string& destination);

        /**
         * @brief Asynchronously stores content at the specified path.
         *
         * @param[in] path Path interpreted by the underlying driver.
         * @param[in] content Content to store.
         * @return Awaitable operation.
         *
         * @note Requires the underlying driver to support asynchronous
         * operations (see @ref BaseDriver::isAsyncSupported and
         * @ref BaseDriver::initAsync). Exception behavior is determined by the
         * underlying driver.
         */
        boost::asio::awaitable<void> putAsync(const std::string& path, const std::string& content);

        /**
         * @brief Asynchronously reads content from the specified path.
         *
         * @param[in] path Path interpreted by the underlying driver.
         * @return Awaitable operation producing the content.
         *
         * @note Exception behavior is determined by the underlying driver.
         */
        boost::asio::awaitable<std::string> getAsync(const std::string& path);

        /**
         * @brief Asynchronously checks whether a file/object exists.
         *
         * @param[in] path Path interpreted by the underlying driver.
         * @return Awaitable operation producing @c true if the path exists;
         * otherwise @c false.
         *
         * @note Exception behavior is determined by the underlying driver.
         */
        boost::asio::awaitable<bool> existsAsync(const std::string& path) const;

        /**
         * @brief Asynchronously deletes the file/object at the specified path.
         *
         * @param[in] path Path interpreted by the underlying driver.
         * @return Awaitable operation.
         *
         * @note Exception behavior is determined by the underlying driver.
         */
        boost::asio::awaitable<void> deleteFileAsync(const std::string& path);

        /**
         * @brief Asynchronously copies a file/object.
         *
         * @param[in] source Source path.
         * @param[in] destination Destination path.
         * @return Awaitable operation.
         *
         * @note Exception behavior is determined by the underlying driver.
         */
        boost::asio::awaitable<void> copyAsync(const std::string& source, const std::string& destination);

        /**
         * @brief Asynchronously moves a file/object.
         *
         * @param[in] source Source path.
         * @param[in] destination Destination path.
         * @return Awaitable operation.
         *
         * @note Exception behavior is determined by the underlying driver.
         */
        boost::asio::awaitable<void> moveAsync(const std::string& source, const std::string& destination);

        /**
         * @brief Returns the underlying driver.
         *
         * @return Shared pointer to the driver. Never null after successful
         * construction.
         */
        std::shared_ptr<BaseDriver> getDriver() const;
    };
}