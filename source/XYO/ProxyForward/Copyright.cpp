// Proxy Forward
// Copyright (c) 2023-2025 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2023-2025 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#include <XYO/ProxyForward/Copyright.hpp>
#include <XYO/ProxyForward/Copyright.rh>

namespace XYO::ProxyForward::Copyright {

	static const char *copyright_ = XYO_PROXYFORWARD_COPYRIGHT;
	static const char *publisher_ = XYO_PROXYFORWARD_PUBLISHER;
	static const char *company_ = XYO_PROXYFORWARD_COMPANY;
	static const char *contact_ = XYO_PROXYFORWARD_CONTACT;

	const char *copyright() {
		return copyright_;
	};

	const char *publisher() {
		return publisher_;
	};

	const char *company() {
		return company_;
	};

	const char *contact() {
		return contact_;
	};

};
