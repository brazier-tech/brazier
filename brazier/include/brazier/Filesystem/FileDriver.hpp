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
#include <thread>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <fstream>

namespace brazier {

    namespace fs = std::filesystem;

    /**
     * @class FileDriver
     * @brief Local filesystem implementation of @ref BaseDriver.
     *
     * @details
     * FileDriver stores files under a configurable root directory. All
     * operation paths are concatenated as `rootPath_ + "/" + path`; paths are
     * not normalized and absolute paths are not treated specially.
     *
     * Synchronous operations use standard C++ filesystem and stream facilities.
     * Asynchronous operations require prior initialization via @c initAsync() and
     * execute the corresponding synchronous operation on an internal
     * Boost.Asio thread pool.
     *
     * @warning The driver does not synchronize access to @c rootPath_. Changing
     * the root path concurrently with other operations is not safe.
     */
    class FileDriver : public BaseDriver {
    private:
        /**
         * @brief Root directory under which all files are stored.
         */
        std::string rootPath_;

        /**
         * @brief Thread pool used by asynchronous operations.
         *
         * @details Created by @c initAsync(). Remains null until initialization.
         */
        std::shared_ptr<boost::asio::thread_pool> ioPool_;

        /**
         * @brief Reserved flag for asynchronous initialization state.
         *
         * @note Not currently used by the implementation; initialization is
         * detected by checking @c ioPool_.
         */
        bool asyncInitialized_ = false;

    public:
        /**
         * @brief Constructs a FileDriver with an empty root path.
         */
        FileDriver() = default;

        /**
         * @brief Destructor.
         */
        ~FileDriver() override = default;

        /**
         * @brief Initializes the asynchronous thread pool.
         *
         * @param[in] threadCount Number of threads in the pool. Defaults to
         * @c std::thread::hardware_concurrency().
         *
         * @details
         * Creates the internal @c boost::asio::thread_pool if it has not been
         * created yet. Repeated calls do not recreate or resize the pool.
         */
        void initAsync(size_t threadCount = std::thread::hardware_concurrency()) override;

        /**
         * @brief Sets the root directory.
         *
         * @param[in] path Root directory path.
         *
         * @details
         * Stores @p path as the root and creates the directory recursively if
         * it does not exist.
         *
         * @throws std::filesystem::filesystem_error If directory creation fails.
         */
        void setRootPath(const std::string& path) override;

        /**
         * @brief Returns the current root directory.
         *
         * @return Root directory path.
         */
        std::string getRootPath() const override;

        /**
         * @brief Writes content to a file.
         *
         * @param[in] path Path relative to the root directory.
         * @param[in] content Content to write.
         *
         * @details
         * Creates parent directories if needed, opens the file with
         * `std::ios::out` (truncating an existing file), and writes @p content.
         *
         * @throws std::runtime_error If parent directories cannot be created or
         * the file cannot be opened for writing.
         *
         * @note Existing files are overwritten. Write success after opening is
         * not explicitly checked.
         */
        void put(const std::string& path, const std::string& content) override;

        /**
         * @brief Reads the entire content of a file.
         *
         * @param[in] path Path relative to the root directory.
         * @return File content.
         *
         * @throws std::runtime_error If the file cannot be opened for reading.
         */
        std::string get(const std::string& path) override;

        /**
         * @brief Checks whether a file exists.
         *
         * @param[in] path Path relative to the root directory.
         * @return @c true if the path exists; otherwise @c false.
         */
        bool exists(const std::string& path) const override;

        /**
         * @brief Deletes a file.
         *
         * @param[in] path Path relative to the root directory.
         *
         * @throws std::runtime_error If the file does not exist.
         * @throws std::filesystem::filesystem_error If removal fails.
         */
        void deleteFile(const std::string& path) override;

        /**
         * @brief Copies a file.
         *
         * @param[in] source Source path relative to the root directory.
         * @param[in] destination Destination path relative to the root directory.
         *
         * @throws std::runtime_error If the source file does not exist.
         * @throws std::filesystem::filesystem_error If the filesystem copy
         * operation fails (for example, depending on destination existence and
         * platform semantics).
         */
        void copy(const std::string& source, const std::string& destination) override;

        /**
         * @brief Moves or renames a file.
         *
         * @param[in] source Source path relative to the root directory.
         * @param[in] destination Destination path relative to the root directory.
         *
         * @throws std::runtime_error If the source file does not exist.
         * @throws std::filesystem::filesystem_error If the filesystem rename
         * operation fails.
         */
        void move(const std::string& source, const std::string& destination) override;

        /**
         * @brief Asynchronously writes content to a file.
         *
         * @param[in] path Path relative to the root directory.
         * @param[in] content Content to write.
         * @return Awaitable operation.
         *
         * @details
         * Requires @c initAsync() to have been called. Posts work to the internal
         * thread pool and invokes put().
         *
         * @throws std::runtime_error If the asynchronous pool is not initialized.
         * Exceptions from put() propagate through the awaitable.
         */
        boost::asio::awaitable<void> putAsync(const std::string& path, const std::string& content) override;

        /**
         * @brief Asynchronously reads a file.
         *
         * @param[in] path Path relative to the root directory.
         * @return Awaitable operation producing file content.
         *
         * @details
         * Requires @c initAsync() to have been called. Posts work to the internal
         * thread pool and invokes put().
         *
         * @throws std::runtime_error If the asynchronous pool is not initialized.
         * Exceptions from get() propagate through the awaitable.
         */
        boost::asio::awaitable<std::string> getAsync(const std::string& path) override;

        /**
         * @brief Asynchronously checks file existence.
         *
         * @param[in] path Path relative to the root directory.
         * @return Awaitable operation producing @c true if the path exists;
         * otherwise @c false.
         *
         * @details
         * Requires @c initAsync() to have been called. Posts work to the internal
         * thread pool and invokes put().
         *
         * @throws std::runtime_error If the asynchronous pool is not initialized.
         */
        boost::asio::awaitable<bool> existsAsync(const std::string& path) const override;

        /**
         * @brief Asynchronously deletes a file.
         *
         * @param[in] path Path relative to the root directory.
         * @return Awaitable operation.
         *
         * @details
         * Requires initAsync() to have been called. Posts work to the internal
         * thread pool and invokes put().@throws std::runtime_error If the asynchronous pool is not initialized.
         * 
         * Exceptions from deleteFile() propagate through the awaitable.
         */
        boost::asio::awaitable<void> deleteFileAsync(const std::string& path) override;

        /**
         * @brief Asynchronously copies a file.
         *
         * @param[in] source Source path relative to the root directory.
         * @param[in] destination Destination path relative to the root directory.
         * @return Awaitable operation.
         * 
         * @details
         * Requires initAsync() to have been called. Posts work to the internal
         * thread pool and invokes put().@throws std::runtime_error If the asynchronous pool is not initialized.
         * 
         * @throws std::runtime_error If the asynchronous pool is not initialized.
         * Exceptions from copy() propagate through the awaitable.
         */
        boost::asio::awaitable<void> copyAsync(const std::string& source, const std::string& destination) override;

        /**
         * @brief Asynchronously moves a file.
         *
         * @param[in] source Source path relative to the root directory.
         * @param[in] destination Destination path relative to the root directory.
         * @return Awaitable operation.
         *
         * @details
         * Requires initAsync() to have been called. Posts work to the internal
         * thread pool and invokes put().@throws std::runtime_error If the asynchronous pool is not initialized.
         *
         * @throws std::runtime_error If the asynchronous pool is not initialized.
         * Exceptions from move() propagate through the awaitable.
         */
        boost::asio::awaitable<void> moveAsync(const std::string& source, const std::string& destination) override;

        /**
         * @brief Returns the driver type identifier.
         *
         * @return @ref brazier::File.
         */
        std::string getDriverType() const override { return brazier::File; }

        /**
         * @brief Indicates that asynchronous operations are supported.
         *
         * @return Always @c true.
         *
         * @note Asynchronous operations still require initAsync() first.
         */
        bool isAsyncSupported() const override { return true; }

    };
}