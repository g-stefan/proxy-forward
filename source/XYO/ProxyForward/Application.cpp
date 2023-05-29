// Proxy Forward
// Copyright (c) 2023 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2023 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#include <XYO/ProxyForward/Dependency.hpp>

#include <XYO/ProxyForward/Application.hpp>
#include <XYO/ProxyForward/Copyright.hpp>
#include <XYO/ProxyForward/License.hpp>
#include <XYO/ProxyForward/Version.hpp>

namespace XYO::ProxyForward {

	static const int defaultThreadCount = 64;

	Multithreading::Semaphore Application::serverStopEvent;

	Application::Application() {
		className_ = "Proxy Forward";
		windowName_ = className_;
		singleInstance_ = true;
		isTrayIconic_ = false;
	};

	Application::~Application(){};

	void Application::showUsage() {
		String msg;
		msg += "Proxy-Forward - forward proxy with authentication\n";
		msg += String("version ") + ProxyForward::Version::version() + " build " + ProxyForward::Version::build() + " " + ProxyForward::Version::datetime() + "\n";
		msg += String(ProxyForward::Copyright::copyright()) + "\n\n";
		msg +=
		    "options:\n"
		    "    --usage                     this info\n"
		    "    --license                   show license\n"
		    "    --version                   show version\n"
		    "    --proxy-server=server       proxy server address\n"
		    "    --proxy-port=port           proxy server port\n"
		    "    --proxy-username=username   proxy server username\n"
		    "    --proxy-password=password   proxy server password\n"
		    "    --local-port=port           local port\n"
		    "    --thread-count=number       number of threads to use (default 16)\n"
		    "    --close                     close proxy found at local port\n";
		msg += "\n";
		MessageBox(NULL, msg.value(), className_, MB_OK);
	};

	void Application::showLicense() {
		String msg;
		msg += ProxyForward::License::license().c_str();
		MessageBox(NULL, msg.value(), className_, MB_OK);
	};

	void Application::showVersion() {
		String msg;
		msg += String("version ") + ProxyForward::Version::version() + " build " + ProxyForward::Version::build() + " " + ProxyForward::Version::datetime() + "\n";
		MessageBox(NULL, msg.value(), className_, MB_OK);
	};

	int Application::main(int cmdN, char *cmdS[]) {
		int i;
		String opt;
		size_t optIndex;
		String optValue;
		TDynamicArray<String> cmdLine;
		String msg;
		bool doClose = false;

		threadCount = defaultThreadCount;

		if (!Network::isValid()) {
			MessageBox(NULL, "Network not initialized!", className_, MB_ICONERROR);
		};

		if (cmdN == 1) {
			showUsage();
			return 0;
		};

		for (i = 1; i < cmdN; ++i) {
			if (StringCore::beginWith(cmdS[i], "@")) {
				String content;
				if (System::Shell::fileGetContents(&cmdS[i][1], content)) {
					int cmdNX;
					char **cmdSX;
					int m;
					System::Shell::mainArgsSet(content, cmdNX, cmdSX);
					for (m = 0; m < cmdNX; ++m) {
						cmdLine.push(cmdSX[m]);
					};
					System::Shell::mainArgsDelete(cmdNX, cmdSX);
					continue;
				};
				msg = String("Error: file not found - ") + &cmdS[i][1] + "\n";
				MessageBox(NULL, msg.value(), className_, MB_ICONERROR);
				return 1;
			};
			cmdLine.push(cmdS[i]);
		};

		// ---

		for (i = 0; i < cmdLine.length(); ++i) {
			if (StringCore::beginWith(cmdLine[i], "--")) {
				opt = cmdLine[i].index(2);
				optValue = "";
				if (opt.indexOf("=", 0, optIndex)) {
					optValue = opt.substring(optIndex + 1);
					opt = opt.substring(0, optIndex);
				};
				if (opt == "usage") {
					showUsage();
					return 0;
				};
				if (opt == "license") {
					showLicense();
					return 0;
				};
				if (opt == "version") {
					showVersion();
					return 0;
				};
				if (opt == "proxy-server") {
					proxyServer = optValue;
					continue;
				};
				if (opt == "proxy-port") {
					proxyPort = optValue;
					continue;
				};
				if (opt == "proxy-username") {
					proxyUsername = optValue;
					continue;
				};
				if (opt == "proxy-password") {
					proxyPassword = optValue;
					continue;
				};
				if (opt == "local-port") {
					localPort = optValue;
					continue;
				};
				if (opt == "thread-count") {
					if (optValue.length() == 0) {
						MessageBox(NULL, "Error: thread-count is empty\n", className_, MB_ICONERROR);
						return 1;
					};
					sscanf(optValue.value(), "%d", &threadCount);
					continue;
				};
				if (opt == "close") {
					doClose = true;
					continue;
				};
				continue;
			};
		};

		//---
		if (localPort.length() == 0) {
			MessageBox(NULL, "Error: local-port is empty\n", className_, MB_ICONERROR);
			return 1;
		};
		// ---
		windowName = String("Proxy#") + localPort;
		windowName_ = (char *)windowName.value();
		// ---
		HWND wndApp;
		wndApp = getSingleInstanceWindow();
		if (doClose) {
			if (wndApp) {
				PostMessage(wndApp, WM_CLOSE, 0, 0);
			};
			return 0;
		};
		if (wndApp) {
			return 0;
		};
		// ---
		if (proxyServer.length() == 0) {
			MessageBox(NULL, "Error: proxy-server is empty\n", className_, MB_ICONERROR);
			return 1;
		};
		if (proxyPort.length() == 0) {
			MessageBox(NULL, "Error: proxy-port is empty\n", className_, MB_ICONERROR);
			return 1;
		};
		if (proxyUsername.length() == 0) {
			MessageBox(NULL, "Error: proxy-username is empty\n", className_, MB_ICONERROR);
			return 1;
		};
		if (proxyPassword.length() == 0) {
			MessageBox(NULL, "Error: proxy-password is empty\n", className_, MB_ICONERROR);
			return 1;
		};
		// ---
		proxyAuthorization = "Proxy-Authorization: Basic ";
		proxyAuthorization += Base64::encode(proxyUsername + ":" + proxyPassword);
		proxyAuthorization += "\r\n";
		proxyAddress = proxyServer + ":" + proxyPort;
		// ---
		if (threadCount <= 0) {
			threadCount = defaultThreadCount;
		};
		// ---
		return SimpleApplication::main(cmdN, cmdS);
	};

	void Application::setCreateStruct(CREATESTRUCT &createStruct) {
		SimpleApplication::setCreateStruct(createStruct);
		//
		createStruct.x = 128;
		createStruct.y = 64;
		createStruct.cx = 320;
		createStruct.cy = 240;
	};

	int Application::setShowCmd(int swShow) {
		return SW_HIDE;
	};

	LRESULT Application::windowProcedure(UINT uMsg, WPARAM wParam, LPARAM lParam) {
		HWND hWnd = *this;

		switch (uMsg) {
		case WM_CREATE: {
			client = new Socket[threadCount]();
			proxy = new Socket[threadCount]();
			thread = new Thread[threadCount]();
			internal = new Internal[threadCount]();
			// ---
			threadIndex = 0;
			// ---
			for (threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
				internal[threadIndex].super = this;
				internal[threadIndex].client = &client[threadIndex];
				internal[threadIndex].proxy = &proxy[threadIndex];
				internal[threadIndex].bufferAtoB = new char[32768 + 8];
				internal[threadIndex].bufferBtoA = new char[32768 + 8];
				internal[threadIndex].index = threadIndex;
			};

			serverStopEvent.reset();

			serverThread.start((ThreadProcedure)threadServer, this);
		};
		    break;
		case WM_DESTROY: {
			if (serverStopEvent.peek()) {
				break;
			};
			serverStopEvent.notify();
			server.close();
			for (threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
				client[threadIndex].close();
				proxy[threadIndex].close();
			};
			serverThread.join();

			for (threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
				thread[threadIndex].join();
			};

			delete[] client;
			delete[] thread;

			for (threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
				delete[] internal[threadIndex].bufferAtoB;
				delete[] internal[threadIndex].bufferBtoA;
			};

			delete[] internal;
		};
		    break;
		default:
			break;
		};

		return SimpleApplication::windowProcedure(uMsg, wParam, lParam);
	};

	void Application::threadServer(Application *_this) {
		int threadIndex;
		if (_this->server.openServerX(String("127.0.0.1") + ":" + _this->localPort)) {
			if (_this->server.listen(4)) {
				while (_this->server && (!serverStopEvent.peek())) {

					while (!serverStopEvent.peek()) {
						for (threadIndex = 0; threadIndex < _this->threadCount; ++threadIndex) {
							if (_this->thread[threadIndex].isRunning()) {
								continue;
							};
							break;
						};
						if (threadIndex < _this->threadCount) {
							break;
						};
						Thread::sleep(1);
					};
					if (serverStopEvent.peek()) {
						break;
					};

					_this->proxy[threadIndex].close();
					_this->client[threadIndex].close();

					while (!serverStopEvent.peek()) {
						if (_this->server.waitToRead(1000) == 0) {
							continue;
						};
						break;
					};
					if (serverStopEvent.peek()) {
						break;
					};

					if (_this->server.accept(_this->client[threadIndex])) {
						if (_this->proxy[threadIndex].openClientX(_this->proxyAddress)) {
							_this->thread[threadIndex].start((ThreadProcedure)threadAToB, &_this->internal[threadIndex]);
							continue;
						};
						_this->proxy[threadIndex].close();
						_this->client[threadIndex].close();
					};
				};
			};
		};
		PostMessage(*_this, WM_CLOSE, 0, 0);
	};

	void Application::threadAToB(Application::Internal *_internal) {
		Thread writer;
		size_t ln;

		while ((*(_internal->client)) && (*(_internal->proxy))) {
			String line;
			if (Stream::readLn((*(_internal->client)), line, 32768)) {
				if (_internal->proxy->write(line.value(), line.length()) == line.length()) {
					size_t index;
					if (line.indexOf("Host: ", 0, index)) {
						if (_internal->proxy->write(_internal->super->proxyAuthorization.value(), _internal->super->proxyAuthorization.length()) == _internal->super->proxyAuthorization.length()) {
							writer.start((ThreadProcedure)threadBToA, _internal);
							while ((*(_internal->client)) && (*(_internal->proxy)) && (!serverStopEvent.peek())) {

								while (!serverStopEvent.peek()) {
									if (_internal->client->waitToRead(10 * 1000) == 0) {
										continue;
									};
									break;
								};
								if (serverStopEvent.peek()) {
									break;
								};

								ln = _internal->client->read(_internal->bufferAtoB, 32768);
								if (_internal->proxy->write(_internal->bufferAtoB, ln) != ln) {
									break;
								};
								if (ln == 0) {
									break;
								};
							};
						};
					};
					continue;
				};
			};
			break;
		};

		_internal->proxy->shutdown();
		writer.join();
	};

	void Application::threadBToA(Application::Internal *_internal) {
		size_t ln;
		while ((*(_internal->client)) && (*(_internal->proxy)) && (!serverStopEvent.peek())) {

			while (!serverStopEvent.peek()) {
				if (_internal->proxy->waitToRead(10 * 1000) == 0) {
					continue;
				};
				break;
			};
			if (serverStopEvent.peek()) {
				break;
			};

			ln = _internal->proxy->read(_internal->bufferBtoA, 32768);
			if (_internal->client->write(_internal->bufferBtoA, ln) != ln) {
				break;
			};
			if (ln == 0) {
				break;
			};
		};
	};

};

#ifndef XYO_PROXYFORWARD_LIBRARY
XYO_APPLICATION_WINMAIN(XYO::ProxyForward::Application);
#endif
