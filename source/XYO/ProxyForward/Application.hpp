// Proxy Forward
// Copyright (c) 2023-2026 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2023-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_PROXYFORWARD_APPLICATION_HPP
#define XYO_PROXYFORWARD_APPLICATION_HPP

#ifndef XYO_PROXYFORWARD_DEPENDENCY_HPP
#	include <XYO/ProxyForward/Dependency.hpp>
#endif

namespace XYO::ProxyForward {
	typedef XYO::Multithreading::Semaphore Semaphore;

	class Application : public virtual SimpleApplication {
			XYO_PLATFORM_DISALLOW_COPY_ASSIGN_MOVE(Application);

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
			static Semaphore serverStopEvent;
			// notified by a connection thread when its slot is free
			Semaphore slotFreeEvent;
			Thread serverThread;
			Socket server;
			int threadCount;
			// ---
			// One slot for each connection.
			// Sockets are owned by the server thread while the slot is free
			// and by the connection threads while the slot is busy.
			struct Connection {
					Application *super;
					Socket client;
					Socket proxy;
					Thread reader;
					Thread writer;
					TAtomic<bool> busy;
					TAtomic<bool> activity;
					// set by the first connection thread that ends, the other one ends too,
					// a socket wait does not end when the socket is shut down by this process
					TAtomic<bool> closing;
					char *bufferAtoB;
					char *bufferBtoA;
					char *header;
					size_t bufferStart;
					size_t bufferEnd;
					size_t headerLength;
			};
			Connection *connection;
			// ---
			void stopServer();
			static void threadServer(Application *);
			static void threadAToB(Connection *);
			static void threadBToA(Connection *);
	};

};

#endif
