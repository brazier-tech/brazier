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

#include "../include/brazier/App/Http/Middleware/MiddlewarePipeline.hpp"

using namespace brazier;

MiddlewarePipeline::MiddlewarePipeline(std::vector<std::shared_ptr<Middleware>> chain) {
    this->chain = chain;
}

MiddlewarePipeline::MiddlewarePipeline() : chain({}) {}

void MiddlewarePipeline::use(std::shared_ptr<Middleware> mw) {
    chain.push_back(mw);
}

bool MiddlewarePipeline::run(Request& req, Response& res) {
    for (auto& mw : chain) {
        if (!mw->handle(req, res)) return false;
    }
    return true;
}