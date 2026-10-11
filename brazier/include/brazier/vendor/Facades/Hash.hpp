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

#include <string.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <openssl/thread.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <brazier/log/Logger.hpp>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <coroutine>

namespace brazier {

    class Hash {
    public:
        static inline uint32_t iterations = 10;
        static inline uint32_t memory_cost = 16;
        static inline uint32_t parallelism = 1;
        static inline uint32_t output_length = 32;

        static std::vector<unsigned char> generateSalt(int length);
        static std::pair<std::string, std::vector<unsigned char>> hash(const std::string& password, std::vector<unsigned char> salt = {});
        static std::vector<uint8_t> hexStringToBytes(const std::string& hex);
        static std::string bytesToHexString(const std::vector<uint8_t>& bytes);
        static bool verify(const std::string& password, const std::string& stored_hash, const std::vector<unsigned char>& salt);
        static boost::asio::awaitable<std::pair<std::string, std::vector<unsigned char>>> awaitableHash(const std::string& password, std::vector<unsigned char> salt = {});
        static boost::asio::awaitable<bool> awaitableVerify(const std::string& password, const std::string& stored_hash, const std::vector<unsigned char>& salt);
    };
}