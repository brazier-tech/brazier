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

#include "../include/brazier/vendor/Handlers/ENV.hpp"

using namespace brazier;

std::unordered_map<std::string, std::string> ENV::env_variables; 
bool ENV::initialized = false;
const std::string ENV::env_file_path = "./.env";

void ENV::initialize() {
    if (initialized) return;

    std::ifstream env_file(env_file_path);
    if (!env_file.is_open()) {
        throw std::runtime_error("Could not open .env file at path: " + env_file_path);
    }

    std::string line;
    while (std::getline(env_file, line)) {
        line.erase(0, line.find_first_not_of(' '));
        line.erase(line.find_last_not_of(' ') + 1);

        if (line.empty() || line[0] == '#') {
            continue;
        }

        size_t delimiter_pos = line.find('=');
        if (delimiter_pos != std::string::npos) {
            std::string key = line.substr(0, delimiter_pos);
            std::string value = line.substr(delimiter_pos + 1);
            key.erase(0, key.find_first_not_of(' '));
            key.erase(key.find_last_not_of(' ') + 1);
            value.erase(0, value.find_first_not_of(' '));
            value.erase(value.find_last_not_of(' ') + 1);

            env_variables[key] = value;
        }
    }

    env_file.close();
    initialized = true;
}

std::string ENV::get(const std::string& key) {
    initialize();
    if (env_variables.find(key) != env_variables.end()) {
        return env_variables[key];
    }
    throw std::runtime_error("Environment variable not found: " + key);
}