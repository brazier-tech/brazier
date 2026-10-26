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

#include "../include/brazier/Engine.hpp"

boost::asio::io_context*& brazier::Engine::io_ctx_ptr() {
    static thread_local boost::asio::io_context* ptr = nullptr;
    return ptr;
}

void brazier::Engine::init(boost::asio::io_context& io_ctx) {
    io_ctx_ptr() = &io_ctx;
}

boost::asio::io_context& brazier::Engine::get_io_context() {
    auto* ptr = io_ctx_ptr();
    if (!ptr) {
        throw std::runtime_error("IO context not initialized for this thread");
    }
    return *ptr;
}