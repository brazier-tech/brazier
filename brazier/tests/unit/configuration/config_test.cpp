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

#include <gtest/gtest.h>
#include <string>
#include "../../../include/brazier/Core"

TEST(ConfigTest, LoadConfig) {
	brazier::ConfigManager::initGlobal();

	bool is_initialized = true;
	try {
		brazier::ConfigManager::getGlobal();
	}
	catch (const std::runtime_error&) {
		is_initialized = false;
	}

	EXPECT_TRUE(is_initialized);
}

TEST(ConfigTest, GetConfigValue) {
	brazier::ConfigManager::initGlobal("config_test.json");
	std::string value = brazier::ConfigManager::getGlobal().get("app_name", "default_app");
	EXPECT_EQ(value, "brazierApp");
}

TEST(ConfigTest, SetConfigValue) {
	brazier::ConfigManager::initGlobal("config_test.json");
	brazier::ConfigManager::getGlobal().set<std::string>("app_name", "NewLightApp");

	std::string value = brazier::ConfigManager::getGlobal().get("app_name", "default_app");

	EXPECT_EQ(value, "NewLightApp");
}

TEST(ConfigTest, RemoveConfigValue) {
	brazier::ConfigManager::initGlobal("config_test.json");
	brazier::ConfigManager::getGlobal().remove("app_name");
	std::string value = brazier::ConfigManager::getGlobal().get("app_name", "default_app");

	EXPECT_EQ(value, "default_app");
}

TEST(ConfigTest, HasConfigValue) {
	brazier::ConfigManager::initGlobal("config_test.json");
	brazier::ConfigManager::getGlobal().remove("app_name");
	bool has_value = brazier::ConfigManager::getGlobal().has("app_name");

	EXPECT_FALSE(has_value);
}

TEST(ConfigTest, GetKeysWithPrefix) {
	brazier::ConfigManager::initGlobal("config_test.json");
	brazier::ConfigManager::getGlobal().set("server.host", "0.0.0.0");
	brazier::ConfigManager::getGlobal().set<int>("server.port", 3502);
	nlohmann::json keys = brazier::ConfigManager::getGlobal().getNestedJson("server");

	EXPECT_EQ(keys["host"], "0.0.0.0");
	EXPECT_EQ(keys["port"], 3502);
}

TEST(ConfigTest, ClearConfig) {
	brazier::ConfigManager::initGlobal("config_test.json");
	brazier::ConfigManager::getGlobal().clear();
	std::string value = brazier::ConfigManager::getGlobal().get("app_name", "default_app");

	EXPECT_EQ(value, "default_app");
}

TEST(ConfigTest, ReloadConfig) {
	brazier::ConfigManager::initGlobal("config_test.json");
	brazier::ConfigManager::getGlobal().set<std::string>("app_name", "TempApp");
	brazier::ConfigManager::getGlobal().reload();
	std::string value = brazier::ConfigManager::getGlobal().get("app_name", "default_app");

	EXPECT_EQ(value, "brazierApp");
}