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

#include "../../include/brazier/Database/Queue.hpp"

using namespace brazier;

redisContext* Queue::context_ = nullptr;

void Queue::connect(const std::string& host, int port) {
    if (context_) {
        throw std::runtime_error("Already connected to Redis.");
    }

    context_ = redisConnect(host.c_str(), port);
    if (context_ == nullptr || context_->err) {
        if (context_) {
            std::string err = "Connection error: " + std::string(context_->errstr);
            redisFree(context_);
            context_ = nullptr;

            throw std::runtime_error(err);
        }
        else {
            Logger::log("Failed to allocate Redis context.", "ERROR");
            throw std::runtime_error("Failed to allocate Redis context.");
        }
    }

    Logger::log("Successfully connected to Redis " + host + ':' + std::to_string(port), "SUCCESS");
}

void Queue::disconnect() {
    if (context_) {
        redisFree(context_);
        context_ = nullptr;
        std::cout << "Disconnected from Redis." << std::endl;
    }
}

void Queue::push(const std::string& queue_name, const std::string& value) {
    if (!context_) {
        Logger::log("Not connected to Redis.", "ERROR");
        throw std::runtime_error("Not connected to Redis.");
    }

    redisReply* reply = (redisReply*)redisCommand(context_, "RPUSH %s %s", queue_name.c_str(), value.c_str());
    if (reply == nullptr) {
        Logger::log("Failed to execute RPUSH command.", "ERROR");
        throw std::runtime_error("Failed to execute RPUSH command.");
    }

    freeReplyObject(reply);
    std::cout << "Pushed '" << value << "' to queue '" << queue_name << "'." << std::endl;
}

std::string Queue::pop(const std::string& queue_name) {
    if (!context_) {
        Logger::log("Not connected to Redis.", "ERROR");
        throw std::runtime_error("Not connected to Redis.");
    }

    redisReply* reply = (redisReply*)redisCommand(context_, "LPOP %s", queue_name.c_str());
    if (reply == nullptr) {
        Logger::log("Failed to execute LPOP command.", "ERROR");
        throw std::runtime_error("Failed to execute LPOP command.");
    }

    std::string result;
    if (reply->type == REDIS_REPLY_STRING) {
        result = std::string(reply->str, reply->len);
    }
    else if (reply->type == REDIS_REPLY_NIL) {
        result = "";
    }
    else {
        freeReplyObject(reply);
        Logger::log("Unexpected reply type from Redis.", "ERROR");
        throw std::runtime_error("Unexpected reply type from Redis.");
    }

    freeReplyObject(reply);
    std::cout << "Popped '" << result << "' from queue '" << queue_name << "'." << std::endl;
    return result;
}

int64_t Queue::length(const std::string& queue_name) {
    if (!context_) {
        Logger::log("Not connected to Redis.", "ERROR");
        throw std::runtime_error("Not connected to Redis.");
    }

    redisReply* reply = (redisReply*)redisCommand(context_, "LLEN %s", queue_name.c_str());
    if (reply == nullptr) {
        Logger::log("Failed to execute LLEN command.", "ERROR");
        throw std::runtime_error("Failed to execute LLEN command.");
    }

    int64_t len = reply->integer;
    freeReplyObject(reply);
    std::cout << "Queue '" << queue_name << "' length: " << len << std::endl;
    return len;
}