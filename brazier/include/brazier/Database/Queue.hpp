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

#include <hiredis/hiredis.h>
#include <iostream>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <brazier/log/Logger.hpp>

    namespace brazier {

    class Queue {
    public:
        static void connect(const std::string& host = "127.0.0.1", int port = 6379);
        static void disconnect();
        static void push(const std::string& queue_name, const std::string& value);
        static std::string pop(const std::string& queue_name);
        static int64_t length(const std::string& queue_name);

    private:
        static redisContext* context_;
    };
}