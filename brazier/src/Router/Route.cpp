/**
* Copyright (c) 2026 Kirill Sergeev, Nikolay Sugonyako, Andrey Agarkov, Hlib Safiannikov.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This source code is licensed under the LGPL license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../../include/brazier/Router/Route.hpp"

using namespace brazier;

Route::Route(const std::string& path, FusedOnion::Controller handler) : pipeline_(std::move(handler)), originalPath_(path), request(pipeline_.request), middleware(pipeline_.middleware), response(pipeline_.response), error(pipeline_.error) {
    pathRegex_ = convertPathToRegex(path);
    paramNames_ = extractParamNames(path);
}

bool Route::match(const std::string& target, Params& outParams) const {
    std::string pathOnly = target;
    const auto queryPos = target.find('?');

    if (queryPos != std::string::npos) {
        pathOnly = target.substr(0, queryPos);
    }

    std::smatch match;
    if (std::regex_match(pathOnly, match, pathRegex_)) {
        for (std::size_t i = 0; i < paramNames_.size(); ++i) {
            outParams[paramNames_[i]] = match[i + 1];
        }
        return true;
    }

    return false;
}

boost::asio::awaitable<void> Route::process(const Request& req, Response& res, const Params& params) const {
    return pipeline_.run(req, res, params);
}

const std::string& Route::path() const {
    return originalPath_;
}

std::regex Route::convertPathToRegex(const std::string& path) {
    std::string regexStr = "^";
    std::istringstream stream(path);
    std::string token;

    while (std::getline(stream, token, '/')) {
        if (token.empty())
            continue;

        if (token[0] == ':')
            regexStr += "/([^/]+)";
        else
            regexStr += "/" + token;
    }

    if (regexStr == "^") {
        regexStr += "/?$";
    } else {
        regexStr += "$";
    }

    return std::regex(regexStr);
}

std::vector<std::string> Route::extractParamNames(const std::string& path) {
    std::vector<std::string> names;
    std::istringstream stream(path);
    std::string token;

    while (std::getline(stream, token, '/')) {
        if (!token.empty() && token[0] == ':')
            names.push_back(token.substr(1));
    }

    return names;
}