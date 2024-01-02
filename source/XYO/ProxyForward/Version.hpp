// Proxy Forward
// Copyright (c) 2023-2024 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2023-2024 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_PROXYFORWARD_VERSION_HPP
#define XYO_PROXYFORWARD_VERSION_HPP

#ifndef XYO_PROXYFORWARD_DEPENDENCY_HPP
#	include <XYO/ProxyForward/Dependency.hpp>
#endif

namespace XYO::ProxyForward::Version {

	const char *version();
	const char *build();
	const char *versionWithBuild();
	const char *datetime();

};

#endif
