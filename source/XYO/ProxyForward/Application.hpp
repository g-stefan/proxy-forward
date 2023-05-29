// Proxy Forward
// Copyright (c) 2023 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2023 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_PROXYFORWARD_APPLICATION_HPP
#define XYO_PROXYFORWARD_APPLICATION_HPP

#ifndef XYO_PROXYFORWARD_DEPENDENCY_HPP
#	include <XYO/ProxyForward/Dependency.hpp>
#endif

namespace XYO::ProxyForward {

	class Application : public virtual SimpleApplication {
			XYO_DISALLOW_COPY_ASSIGN_MOVE(Application);

		public:
			Application();
			~Application();

			void showUsage();
			void showLicense();
			void showVersion();

			void setCreateStruct(CREATESTRUCT &);
			LRESULT windowProcedure(UINT uMsg, WPARAM wParam, LPARAM lParam);
			int setShowCmd(int);
			int main(int cmdN, char *cmdS[]);

			String windowName;
			String localPort;
			String proxyServer;
			String proxyPort;
			String proxyUsername;
			String proxyPassword;
			String proxyAuthorization;
			String proxyAddress;
			// ---
			static Multithreading::Semaphore serverStopEvent;
			Thread serverThread;
			Socket server;
			Socket *client;
			Socket *proxy;
			Thread *thread;
			int threadCount;
			int threadIndex;
			// ---
			struct Internal {
					Application *super;
					Socket *client;
					Socket *proxy;
					char *bufferAtoB;
					char *bufferBtoA;
					int index;
			};
			Internal *internal;
			// ---
			static void threadServer(Application *);
			static void threadAToB(Internal *);
			static void threadBToA(Internal *);
	};

};

#endif
