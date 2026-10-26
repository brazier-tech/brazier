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

#include "../include/brazier/vendor/ConfigManager.hpp"
#include <sstream>

namespace brazier {

    namespace {
        std::unique_ptr<brazier::ConfigManager> g_global_config = nullptr;
    }

    void ConfigManager::flattenJson(const std::string& prefix, const json& j,
        std::unordered_map<std::string, std::string>& output) {
        if (j.is_object()) {
            for (auto& [key, value] : j.items()) {
                std::string newPrefix = prefix.empty() ? key : prefix + "." + key;
                flattenJson(newPrefix, value, output);
            }
        }
        else if (j.is_array()) {
            output[prefix] = j.dump();
        }
        else if (j.is_string()) {
            output[prefix] = j.get<std::string>();
        }
        else {
            output[prefix] = j.dump();
        }
    }

    json ConfigManager::unflattenMap(const std::unordered_map<std::string, std::string>& map) const {
        json result;

        for (const auto& [key, value] : map) {
            std::vector<std::string> parts;
            std::stringstream ss(key);
            std::string part;
            while (std::getline(ss, part, '.')) {
                parts.push_back(part);
            }

            json* current = &result;
            for (size_t i = 0; i < parts.size() - 1; ++i) {
                if (!current->contains(parts[i])) {
                    (*current)[parts[i]] = json::object();
                }
                current = &(*current)[parts[i]];
            }

            const std::string& lastPart = parts.back();
            try {
                (*current)[lastPart] = json::parse(value);
            }
            catch (...) {
                (*current)[lastPart] = value;
            }
        }

        return result;
    }

    ConfigManager::ConfigManager(const std::string& configFilePath,
        std::shared_ptr<BaseDriver> driver)
        : driver_(driver)
        , configFilePath_(configFilePath) {
        if (driver_) {
            load();
        }
    }

    ConfigManager::ConfigManager(const std::string& configFilePath,
        const std::string& driverName) : configFilePath_(configFilePath) {
        try {
            Filesystem fs = Filesystem::driver(driverName);
            driver_ = fs.getDriver();
        }
        catch (std::exception& e) {
            throw std::runtime_error("Failed to initialize ConfigManager with driver '" + driverName + "': " + e.what());
        }
    }

    void ConfigManager::load() {
        if (!driver_) {
            throw std::runtime_error("Driver is not initialized");
        }

        std::lock_guard<std::mutex> lock(mutex_);

        if (!driver_->exists(configFilePath_)) {
            config_.clear();
            return;
        }

        try {
            std::string content = driver_->get(configFilePath_);
            json j = json::parse(content);

            config_.clear();
            flattenJson("", j, config_);
        }
        catch (const std::exception& e) {
            throw std::runtime_error("Failed to load config: " + std::string(e.what()));
        }
    }

    void ConfigManager::save() {
        if (!driver_) {
            throw std::runtime_error("Driver is not initialized");
        }

        json j = unflattenMap(config_);
        std::string content = j.dump(4);
        driver_->put(configFilePath_, content);
    }

    bool ConfigManager::has(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_.find(key) != config_.end();
    }

    bool ConfigManager::remove(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t erased = config_.erase(key);
        if (erased > 0 && autoSave_) {
            save();
        }
        return erased > 0;
    }

    std::vector<std::string> ConfigManager::getKeys() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> keys;
        keys.reserve(config_.size());
        for (const auto& [key, _] : config_) {
            keys.push_back(key);
        }
        return keys;
    }

    json ConfigManager::getAllAsJson() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return unflattenMap(config_);
    }

    void ConfigManager::loadFromJson(const json& j) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.clear();
        flattenJson("", j, config_);
        if (autoSave_) {
            save();
        }
    }

    void ConfigManager::clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.clear();
        if (autoSave_) {
            save();
        }
    }

    void ConfigManager::setAutoSave(bool autoSave) {
        std::lock_guard<std::mutex> lock(mutex_);
        autoSave_ = autoSave;
    }

    void ConfigManager::reload() {
        load();
    }

    std::unordered_map<std::string, std::string> ConfigManager::getRawData() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_;
    }

    bool ConfigManager::hasNested(const std::string& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_.find(key) != config_.end();
    }

    std::vector<std::string> ConfigManager::getKeysNested() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> keys;
        keys.reserve(config_.size());
        for (const auto& [key, _] : config_) {
            keys.push_back(key);
        }
        return keys;
    }

    void ConfigManager::loadFromJsonNested(const json& j) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.clear();
        flattenJson("", j, config_);
        if (autoSave_) {
            save();
        }
    }

    json ConfigManager::getNestedJson(const std::string& prefix) const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::unordered_map<std::string, std::string> filtered;
        if (prefix.empty()) {
            filtered = config_;
        }
        else {
            for (const auto& [key, value] : config_) {
                if (key.find(prefix) == 0) {
                    std::string subKey = key.substr(prefix.length() + 1);
                    filtered[subKey] = value;
                }
            }
        }

        return unflattenMap(filtered);
    }

    json ConfigManager::getJson(const std::string& key) const {
        std::unordered_map<std::string, std::string> filtered;
        std::string prefix = key;

        for (const auto& [configKey, value] : config_) {
            if (configKey == key) {
                try {
                    return json::parse(value);
                }
                catch (...) {
                    return value;
                }
            }
            else if (configKey.find(prefix + ".") == 0) {
                std::string subKey = configKey.substr(prefix.length() + 1);
                filtered[subKey] = value;
            }
        }

        if (!filtered.empty()) {
            return unflattenMap(filtered);
        }

        return json::object();
    }

    template<typename T>
    T ConfigManager::getNested(const std::string& path, const T& defaultValue) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = config_.find(path);
        if (it != config_.end()) {
            try {
                return fromString<T>(it->second);
            }
            catch (std::exception& e) {
                Logger::log(e.what(), "ERROR");
                return defaultValue;
            }
        }

        std::string prefix = path + ".";
        std::unordered_map<std::string, std::string> filtered;

        for (const auto& [key, value] : config_) {
            if (key.find(prefix) == 0) {
                std::string subKey = key.substr(prefix.length());
                filtered[subKey] = value;
            }
        }

        if (!filtered.empty()) {
            json nestedJson = unflattenMap(filtered);
            try {
                if constexpr (std::is_same_v<T, json>) {
                    return nestedJson;
                }
                else if constexpr (std::is_same_v<T, std::string>) {
                    return nestedJson.dump(4);
                }
                else {
                    return nestedJson.get<T>();
                }
            }
            catch (...) {
                return defaultValue;
            }
        }

        return defaultValue;
    }

    template<typename T>
    void ConfigManager::setNested(const std::string& path, const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);

        if constexpr (std::is_same_v<T, json>) {
            json j = value;

            std::vector<std::string> toRemove;
            for (const auto& [key, _] : config_) {
                if (key == path || key.find(path + ".") == 0) {
                    toRemove.push_back(key);
                }
            }
            for (const auto& key : toRemove) {
                config_.erase(key);
            }

            flattenJson(path, j, config_);
        }
        else {
            config_[path] = toString(value);
        }

        if (autoSave_) {
            save();
        }
    }

    bool ConfigManager::hasNestedPath(const std::string& path) const {
        std::lock_guard<std::mutex> lock(mutex_);

        if (config_.find(path) != config_.end()) {
            return true;
        }

        std::string prefix = path + ".";
        for (const auto& [key, _] : config_) {
            if (key.find(prefix) == 0) {
                return true;
            }
        }

        return false;
    }

    bool ConfigManager::removeNested(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<std::string> toRemove;
        for (const auto& [key, _] : config_) {
            if (key == path || key.find(path + ".") == 0) {
                toRemove.push_back(key);
            }
        }

        if (toRemove.empty()) {
            return false;
        }

        for (const auto& key : toRemove) {
            config_.erase(key);
        }

        if (autoSave_) {
            save();
        }

        return true;
    }

    std::vector<std::string> ConfigManager::getKeysWithPrefix(const std::string& prefix) const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<std::string> keys;
        if (prefix.empty()) {
            keys.reserve(config_.size());
            for (const auto& [key, _] : config_) {
                keys.push_back(key);
            }
        }
        else {
            for (const auto& [key, _] : config_) {
                if (key.find(prefix) == 0) {
                    keys.push_back(key);
                }
            }
        }

        return keys;
    }

    void ConfigManager::initGlobal(const std::string& filepath)
    {
        auto configDriver = std::make_shared<brazier::FileDriver>();
        configDriver->setRootPath("./");
        configDriver->initAsync();

        g_global_config = std::make_unique<ConfigManager>(filepath, configDriver);
        g_global_config->load();
    }

    ConfigManager& ConfigManager::getGlobal()
    {
        if (!g_global_config) {
            throw std::runtime_error("Global config is not initialized. Call initGlobal() first.");
        }
        return *g_global_config;
    }

    template std::string ConfigManager::getNested<std::string>(const std::string&, const std::string&);
    template int ConfigManager::getNested<int>(const std::string&, const int&);
    template bool ConfigManager::getNested<bool>(const std::string&, const bool&);
    template double ConfigManager::getNested<double>(const std::string&, const double&);
    template json ConfigManager::getNested<json>(const std::string&, const json&);

    template void ConfigManager::setNested<std::string>(const std::string&, const std::string&);
    template void ConfigManager::setNested<int>(const std::string&, const int&);
    template void ConfigManager::setNested<bool>(const std::string&, const bool&);
    template void ConfigManager::setNested<double>(const std::string&, const double&);
    template void ConfigManager::setNested<json>(const std::string&, const json&);

}