/*
 * Copyright (c) 2026 Kirill Sergeev, Nikolay Sugonyako, Andrey Agarkov, Gleb Safyannikov
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This file is part of brazier.
 *
 * brazier is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * brazier is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with brazier; if not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <nlohmann/json.hpp>
#include <stdexcept>
#include "../Filesystem/Filesystem.hpp"
#include <memory>
#include <unordered_map>
#include <mutex>
#include <any>
#include <string>
#include <brazier/log/Logger.hpp>

namespace brazier {

    using json = nlohmann::json;

    class ConfigManager {
    private:
        std::shared_ptr<BaseDriver> driver_;
        std::string configFilePath_;
        std::unordered_map<std::string, std::string> config_;
        mutable std::mutex mutex_;
        bool autoSave_ = false;

        template<typename T>
        std::string toString(const T& value) {
            if constexpr (std::is_same_v<T, std::string>) {
                return value;
            }
            else if constexpr (std::is_same_v<T, const char*>) {
                return std::string(value);
            }
            else if constexpr (std::is_same_v<T, char*>) {
                return std::string(value);
            }
            else {
                json j = value;
                return j.dump();
            }
        }

        template<typename T>
        T fromString(const std::string& str) {
            if constexpr (std::is_same_v<T, std::string>) {
                return str;
            }
            else {
                json j = json::parse(str);
                return j.get<T>();
            }
        }

        void flattenJson(const std::string& prefix, const json& j,
            std::unordered_map<std::string, std::string>& output);
        json unflattenMap(const std::unordered_map<std::string, std::string>& map) const;

    public:
        ConfigManager(const std::string& configFilePath = "config.json",
            std::shared_ptr<BaseDriver> driver = nullptr);

        ConfigManager(const std::string& configFilePath = "config.json",
            const std::string& driverName = "local");

        ~ConfigManager() = default;

        void load();
        void save();

        template<typename T>
        T get(const std::string& key, const T& defaultValue = T{}) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = config_.find(key);
            if (it != config_.end()) {
                try {
                    return fromString<T>(it->second);
                }
                catch (std::exception& e) {
                    Logger::log(e.what(), "ERROR");
                    return defaultValue;
                }
            }
            return defaultValue;
        }

        std::string get(const std::string key, const char* defaultValue) {
			return get<std::string>(key, std::string(defaultValue));
        }

        template<int N>
        std::string get(const std::string key, const char (&defaultValue)[N]) {
            return get<std::string>(key, std::string(defaultValue, N - 1));
        }

        template<typename T>
        void set(const std::string& key, const T& value) {
            std::lock_guard<std::mutex> lock(mutex_);
            config_[key] = toString(value);
            if (autoSave_) {
                save();
            }
        }

        bool has(const std::string& key);
        bool remove(const std::string& key);
        std::vector<std::string> getKeys() const;
        json getAllAsJson() const;
        void loadFromJson(const json& j);
        void clear();
        void setAutoSave(bool autoSave);
        void reload();
        std::unordered_map<std::string, std::string> getRawData() const;

        bool hasNested(const std::string& key) const;
        std::vector<std::string> getKeysNested() const;
        void loadFromJsonNested(const json& j);
        json getNestedJson(const std::string& prefix = "") const;

        json getJson(const std::string& key) const;

        template<typename T>
        T getNested(const std::string& path, const T& defaultValue = T{});

        template<typename T>
        void setNested(const std::string& path, const T& value);

        bool hasNestedPath(const std::string& path) const;
        bool removeNested(const std::string& path);
        std::vector<std::string> getKeysWithPrefix(const std::string& prefix) const;

        static void initGlobal();
        static void initGlobal(std::string filepath);
    };
}