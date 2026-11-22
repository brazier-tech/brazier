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
#include "Middleware.hpp"

namespace brazier {

	class CorsMiddleware : public Middleware {

		std::vector<std::string> allowed_domains;
		std::string allow_origin;

	public:
		bool handle(Request& req, Response& res) {
			std::string origin = std::string(req[http::field::origin]);
			bool allowed = this->allowed_domains.empty() ||
				std::find(allowed_domains.begin(), allowed_domains.end(), origin) != allowed_domains.end();

			if (!allowed) {
				res.result(http::status::forbidden);
				res.body() = "CORS: Origin not allowed";
				res.set(http::field::access_control_allow_origin, "null");
				res.prepare_payload();
				return false;
			}

			this->setCors(req, res);

			if (req.method() == http::verb::options) {
				res.result(http::status::ok);
				res.body() = "CORS preflight";
				res.prepare_payload();
				return true;
			}
		}
	};
}