/**
 * Copyright (c) 2026 Kirill Sergeev, Nikolay Sugonyako, Andrey Agarkov, Hlib Safiannikov.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This source code is licensed under the LGPL license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <unordered_map>
#include <memory>
#include <mutex>
#include "BaseDriver.hpp"

namespace brazier {

    /**
     * @class StorageManager
     * @brief Process-wide registry of named storage drivers.
     *
     * @details
     * StorageManager is a singleton that maps user-defined driver names to
     * @ref BaseDriver instances and tracks a single default driver name. It is
     * used by @ref Filesystem to resolve which driver to use.
     *
     * The instance is created lazily on the first call to getInstance() and
     * lives until program termination.
     *
     * @warning The class declares a @c std::mutex but the current
     * implementation does not lock it. Access from multiple threads is
     * therefore not synchronized and is unsafe.
     *
     * @note This class is non-copyable and non-movable by virtue of its
     * private default constructor and implicit deletion of copy/move
     * operations caused by the @c std::mutex member.
     */
    class StorageManager {
    private:
        /**
         * @brief Private constructor; enforces the singleton pattern.
         */
        StorageManager() = default;

        /**
         * @brief Private destructor; the singleton lives until program termination.
         */
        ~StorageManager() = default;

        /**
         * @brief Map of driver name to driver instance.
         */
        std::unordered_map<std::string, std::shared_ptr<BaseDriver>> drivers_;

        /**
         * @brief Name of the current default driver.
         *
         * @details Empty until setDefaultDriver() is called successfully.
         */
        std::string defaultDriverName_;

        /**
         * @brief Mutex intended to guard access to @c drivers_ and
         * @c defaultDriverName_.
         *
         * @warning Currently declared but never locked by the implementation.
         */
        mutable std::mutex mutex_;

    public:
        /**
         * @brief Returns the singleton instance.
         *
         * @return Reference to the process-wide StorageManager.
         *
         * @details The instance is constructed on first use (function-local static) 
         * and is thread-safe under C++11 and later for the
         * initialization itself.
         */
        static StorageManager& getInstance();

        /**
         * @brief Registers or replaces a driver under the given name.
         *
         * @param[in] name Name to register the driver under.
         * @param[in] driver Driver instance. Ownership is shared via
         * @c std::shared_ptr.
         *
         * @note If a driver with the same name already exists, it is silently
         * replaced.
         * @note A null @p driver is stored as-is; no validation is performed.
         */
        void registerDriver(const std::string& name, std::shared_ptr<BaseDriver> driver);

        /**
         * @brief Removes a driver registration.
         *
         * @param[in] name Name of the driver to remove.
         *
         * @note Removing a name that is not registered is a no-op.
         * @note If the removed driver was the current default, the stored
         * default name is not cleared; subsequent getDefaultDriver() will
         * throw until a new default is set.
         */
        void unregisterDriver(const std::string& name);

        /**
         * @brief Sets the default driver by name.
         *
         * @param[in] name Name of an already registered driver.
         *
         * @throws std::runtime_error If no driver with the given name is
         * registered.
         */
        void setDefaultDriver(const std::string& name);

        /**
         * @brief Returns the current default driver.
         *
         * @return Shared pointer to the default driver.
         *
         * @throws std::runtime_error If no default driver has been set, or if
         * the default name no longer refers to a registered driver (for
         * example, after unregisterDriver() removed it).
         */
        std::shared_ptr<BaseDriver> getDefaultDriver() const;

        /**
         * @brief Returns a driver by name.
         *
         * @param[in] name Driver name.
         * @return Shared pointer to the registered driver.
         *
         * @throws std::runtime_error If no driver with the given name is
         * registered.
         */
        std::shared_ptr<BaseDriver> getDriver(const std::string& name) const;

        /**
         * @brief Checks whether a driver with the given name is registered.
         *
         * @param[in] name Driver name.
         * @return @c true if the name is registered; otherwise @c false.
         */
        bool hasDriver(const std::string& name) const;

        /**
         * @brief Returns the names of all registered drivers.
         *
         * @return Vector of driver names.
         *
         * @note The order of the returned names is unspecified because
         * @c drivers_ is an @c std::unordered_map.
         */
        std::vector<std::string> getDriverNames() const;
    };
}