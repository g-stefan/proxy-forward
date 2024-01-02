// Proxy Forward
// Copyright (c) 2023-2024 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2023-2024 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_PROXYFORWARD_DEPENDENCY_HPP
#define XYO_PROXYFORWARD_DEPENDENCY_HPP

#ifndef XYO_SYSTEM_HPP
#	include <XYO/System.hpp>
#endif

#ifndef XYO_NETWORKING_HPP
#	include <XYO/Networking.hpp>
#endif

#ifndef XYO_WIN_HPP
#	include <XYO/Win.hpp>
#endif

namespace XYO::ProxyForward {
	using namespace XYO::System;
	using namespace XYO::Networking;
	using namespace XYO::Win;
};

#endif
