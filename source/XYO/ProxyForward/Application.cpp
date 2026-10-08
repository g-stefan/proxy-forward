// Proxy Forward
// Copyright (c) 2023-2026 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2023-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#include <XYO/ProxyForward/Dependency.hpp>

#include <XYO/ProxyForward/Application.hpp>
#include <XYO/ProxyForward/Copyright.hpp>
#include <XYO/ProxyForward/License.hpp>
#include <XYO/ProxyForward/Version.hpp>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

namespace XYO::ProxyForward {

	static const int defaultThreadCount = 64;
	static const int maximumThreadCount = 1024;
	static const uint16_t listenQueue = 256;
	// size of the transfer buffers
	static const size_t bufferSize = 32768;
	// maximum size of a request header
	static const size_t headerSizeMax = 65536;
	// sockets are waited at most this interval, then stop and closing are checked, in microseconds
	static const uint32_t waitInterval = 100 * 1000;
	// time to wait for the next request of a connection without data in both directions,
	// in wait intervals, 5 minutes
	static const int idleTimeout = 5 * 60 * 10;
	// server thread wait for a free connection slot, when all are busy, then stop is checked, in milliseconds
	static const int slotWaitInterval = 250;
	// --close waits for the running instance to end, in milliseconds
	static const DWORD closeTimeout = 30 * 1000;

	Semaphore Application::serverStopEvent;

	Application::Application() {
		className_ = "Proxy Forward";
		windowName_ = className_;
		singleInstance_ = true;
		isTrayIconic_ = false;
		threadCount = defaultThreadCount;
		connection = nullptr;
	};

	Application::~Application() {
		stopServer();
	};

	void Application::showUsage() {
		String msg;
		msg += "Proxy-Forward - forward proxy with authentication\n";
		msg += String("version ") + ProxyForward::Version::version() + " build " + ProxyForward::Version::build() + " " + ProxyForward::Version::datetime() + "\n";
		msg += String(ProxyForward::Copyright::copyright()) + "\n\n";
		msg +=
		    "options:\n"
		    "    --help                      this info\n"
		    "    --usage                     this info\n"
		    "    --license                   show license\n"
		    "    --version                   show version\n"
		    "    --proxy-server=server       proxy server address\n"
		    "    --proxy-port=port           proxy server port\n"
		    "    --proxy-username=username   proxy server username\n"
		    "    --proxy-password=password   proxy server password\n"
		    "    --local-port=port           local port\n"
		    "    --thread-count=number       number of connections, 1 to 1024 (default 64)\n"
		    "    --close                     close proxy found at local port, wait for it to end\n"
		    "    @file                       read options from file\n";
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

		if (cmdN == 1) {
			showUsage();
			return 0;
		};

		for (i = 1; i < cmdN; ++i) {
			if (StringCore::beginWith(cmdS[i], "@")) {
				String content;
				if (System::Shell::fileGetContents(&cmdS[i][1], content)) {
					XYO::System::ShellArguments shellArguments;
					int m;
					shellArguments.set(content);
					for (m = 0; m < shellArguments.cmdN; ++m) {
						cmdLine.push(shellArguments.cmdS[m]);
					};
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
				if (opt == "help") {
					showUsage();
					return 0;
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
					char *end = nullptr;
					long value = 0;
					if (optValue.length() > 0) {
						value = strtol(optValue.value(), &end, 10);
					};
					if ((end == nullptr) || (*end != 0) || (value < 1) || (value > maximumThreadCount)) {
						MessageBox(NULL, "Error: thread-count must be a number from 1 to 1024\n", className_, MB_ICONERROR);
						return 1;
					};
					threadCount = static_cast<int>(value);
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
				// Wait for the instance to end, the local port is free after --close,
				// a new instance started right after it would find the old window and exit.
				// No message box on timeout, --close is used from scripts.
				DWORD processId = 0;
				HANDLE process = nullptr;
				DWORD waitResult = WAIT_OBJECT_0;
				GetWindowThreadProcessId(wndApp, &processId);
				if (processId != 0) {
					process = OpenProcess(SYNCHRONIZE, FALSE, processId);
				};
				PostMessage(wndApp, WM_CLOSE, 0, 0);
				if (process) {
					waitResult = WaitForSingleObject(process, closeTimeout);
					CloseHandle(process);
				};
				if (waitResult != WAIT_OBJECT_0) {
					return 1;
				};
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
		if (!Network::isValid()) {
			MessageBox(NULL, "Error: network not initialized\n", className_, MB_ICONERROR);
			return 1;
		};
		if (!(server.openServerX(String("127.0.0.1:") + localPort) && server.listen(listenQueue))) {
			server.close();
			msg = String("Error: unable to listen on local port ") + localPort + "\n";
			MessageBox(NULL, msg.value(), className_, MB_ICONERROR);
			return 1;
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
		switch (uMsg) {
		case WM_CREATE: {
			int index;
			connection = new Connection[threadCount]();
			for (index = 0; index < threadCount; ++index) {
				Connection &c = connection[index];
				c.super = this;
				c.busy.set(false);
				c.activity.set(false);
				c.closing.set(false);
				c.bufferAtoB = new char[bufferSize];
				c.bufferBtoA = new char[bufferSize];
				// the request header, the authorization, the header end and the start of the body
				c.header = new char[headerSizeMax + proxyAuthorization.length() + 2 + bufferSize];
				c.bufferStart = 0;
				c.bufferEnd = 0;
				c.headerLength = 0;
			};

			serverStopEvent.reset();

			if (!serverThread.start((ThreadProcedure)threadServer, this)) {
				MessageBox(NULL, "Error: unable to start server thread\n", className_, MB_ICONERROR);
				PostMessage(*this, WM_CLOSE, 0, 0);
			};
		};
		    break;
		case WM_DESTROY:
			stopServer();
			break;
		default:
			break;
		};

		return SimpleApplication::windowProcedure(uMsg, wParam, lParam);
	};

	void Application::stopServer() {
		int index;

		if (!connection) {
			return;
		};

		// Threads check the stop event after each socket wait,
		// sockets are closed only by the thread that owns them
		serverStopEvent.notify();
		serverThread.join();

		for (index = 0; index < threadCount; ++index) {
			connection[index].reader.join();
		};

		for (index = 0; index < threadCount; ++index) {
			delete[] connection[index].bufferAtoB;
			delete[] connection[index].bufferBtoA;
			delete[] connection[index].header;
		};

		delete[] connection;
		connection = nullptr;
	};

	void Application::threadServer(Application *this_) {
		int index;
		int retV;

		while (!serverStopEvent.peek()) {
			for (index = 0; index < this_->threadCount; ++index) {
				if (!this_->connection[index].busy.get()) {
					break;
				};
			};
			if (index >= this_->threadCount) {
				this_->slotFreeEvent.waitFor(slotWaitInterval);
				continue;
			};

			retV = this_->server.waitToRead(waitInterval);
			if (retV == 0) {
				continue;
			};
			if (retV < 0) {
				break;
			};

			Connection &c = this_->connection[index];
			if (!this_->server.accept(c.client)) {
				continue;
			};

			c.busy.set(true);
			c.activity.set(false);
			c.closing.set(false);
			if (!c.reader.start((ThreadProcedure)threadAToB, &c)) {
				c.client.close();
				c.busy.set(false);
			};
		};

		this_->server.close();

		if (!serverStopEvent.peek()) {
			PostMessage(*this_, WM_CLOSE, 0, 0);
		};
	};

	// ---

	// Wait for data to read, false on stop, on connection closing or error.
	// With useIdleTimeout, false also when there is no data in both directions for idleTimeout.
	static bool waitData(Socket &socket, Application::Connection *connection, bool useIdleTimeout) {
		int idle = 0;
		int retV;
		while (!(Application::serverStopEvent.peek() || connection->closing.get())) {
			retV = socket.waitToRead(waitInterval);
			if (retV > 0) {
				return true;
			};
			if (retV < 0) {
				return false;
			};
			if (useIdleTimeout) {
				if (connection->activity.exchange(false)) {
					idle = 0;
					continue;
				};
				++idle;
				if (idle >= idleTimeout) {
					return false;
				};
			};
		};
		return false;
	};

	// Write all data, false on stop, on connection closing or error.
	// A write to a peer that does not read blocks until the peer reads,
	// wait until the socket can be written, in pieces, so stop and closing are seen.
	static bool writeData(Socket &socket, Application::Connection *connection, const char *data, size_t size) {
		size_t ln;
		int retV;
		while (size > 0) {
			if (Application::serverStopEvent.peek() || connection->closing.get()) {
				return false;
			};
			retV = socket.waitToWrite(waitInterval);
			if (retV < 0) {
				return false;
			};
			if (retV == 0) {
				continue;
			};
			ln = size;
			if (ln > bufferSize) {
				ln = bufferSize;
			};
			if (socket.write(data, ln) != ln) {
				return false;
			};
			data += ln;
			size -= ln;
		};
		return true;
	};

	// Read more data from the client, false at the end of the stream
	static bool fillBuffer(Application::Connection *c, bool useIdleTimeout) {
		size_t ln;
		if (c->bufferStart == c->bufferEnd) {
			c->bufferStart = 0;
			c->bufferEnd = 0;
		};
		if (c->bufferEnd >= bufferSize) {
			return false;
		};
		if (!waitData(c->client, c, useIdleTimeout)) {
			return false;
		};
		ln = c->client.read(c->bufferAtoB + c->bufferEnd, bufferSize - c->bufferEnd);
		if (ln == 0) {
			return false;
		};
		c->bufferEnd += ln;
		return true;
	};

	// Append a line from the client, with the line end, to the header.
	// Returns the line length, 0 at the end of the stream or if the header is too large.
	static size_t readLine(Application::Connection *c, bool useIdleTimeout) {
		size_t lineStart = c->headerLength;
		size_t ln;
		const char *data;
		const char *lineEnd;

		for (;;) {
			if (c->bufferStart < c->bufferEnd) {
				ln = c->bufferEnd - c->bufferStart;
				if (ln > headerSizeMax - c->headerLength) {
					ln = headerSizeMax - c->headerLength;
				};
				data = c->bufferAtoB + c->bufferStart;
				lineEnd = static_cast<const char *>(memchr(data, '\n', ln));
				if (lineEnd) {
					ln = lineEnd - data + 1;
				};
				memcpy(c->header + c->headerLength, data, ln);
				c->headerLength += ln;
				c->bufferStart += ln;
				if (lineEnd) {
					return c->headerLength - lineStart;
				};
				if (c->headerLength >= headerSizeMax) {
					return 0;
				};
				continue;
			};
			if (!fillBuffer(c, useIdleTimeout)) {
				return 0;
			};
		};
	};

	static bool isEmptyLine(const char *line, size_t ln) {
		if (ln == 1) {
			return (line[0] == '\n');
		};
		if (ln == 2) {
			return (line[0] == '\r') && (line[1] == '\n');
		};
		return false;
	};

	// Check if the line is the header with the name (lowercase),
	// set the value, without spaces and line end
	static bool getHeader(const char *line, size_t ln, const char *name, const char *&value, size_t &valueLength) {
		size_t k;
		size_t end;

		for (k = 0; name[k] != 0; ++k) {
			if (k >= ln) {
				return false;
			};
			if (tolower(static_cast<unsigned char>(line[k])) != name[k]) {
				return false;
			};
		};
		if ((k >= ln) || (line[k] != ':')) {
			return false;
		};

		++k;
		while ((k < ln) && ((line[k] == ' ') || (line[k] == '\t'))) {
			++k;
		};
		end = ln;
		while ((end > k) && ((line[end - 1] == '\r') || (line[end - 1] == '\n') || (line[end - 1] == ' ') || (line[end - 1] == '\t'))) {
			--end;
		};

		value = line + k;
		valueLength = end - k;
		return true;
	};

	static bool parseDecimal(const char *value, size_t ln, uint64_t &number) {
		size_t k;
		number = 0;
		if (ln == 0) {
			return false;
		};
		for (k = 0; k < ln; ++k) {
			if ((value[k] < '0') || (value[k] > '9')) {
				return false;
			};
			if (number > (UINT64_MAX - 9) / 10) {
				return false;
			};
			number = number * 10 + (value[k] - '0');
		};
		return true;
	};

	// Chunk size line: hex size, optional extensions after ';'
	static bool parseChunkSize(const char *line, size_t ln, uint64_t &size) {
		size_t k;
		int digit;
		size = 0;
		for (k = 0; k < ln; ++k) {
			char x = line[k];
			if ((x >= '0') && (x <= '9')) {
				digit = x - '0';
			} else if ((x >= 'a') && (x <= 'f')) {
				digit = x - 'a' + 10;
			} else if ((x >= 'A') && (x <= 'F')) {
				digit = x - 'A' + 10;
			} else {
				break;
			};
			if (size > (UINT64_MAX >> 4)) {
				return false;
			};
			size = (size << 4) | digit;
		};
		if (k == 0) {
			return false;
		};
		return (k < ln) && ((line[k] == ';') || (line[k] == ' ') || (line[k] == '\t') || (line[k] == '\r') || (line[k] == '\n'));
	};

	static bool hasToken(const char *value, size_t ln, const char *token) {
		size_t tokenLength = strlen(token);
		size_t k;
		size_t m;
		for (k = 0; k + tokenLength <= ln; ++k) {
			for (m = 0; m < tokenLength; ++m) {
				if (tolower(static_cast<unsigned char>(value[k + m])) != token[m]) {
					break;
				};
			};
			if (m == tokenLength) {
				return true;
			};
		};
		return false;
	};

	// Forward size bytes of the request body from the client to the proxy
	static bool forwardBody(Application::Connection *c, uint64_t size) {
		size_t ln;
		while (size > 0) {
			if (c->bufferStart == c->bufferEnd) {
				if (!fillBuffer(c, false)) {
					return false;
				};
			};
			ln = c->bufferEnd - c->bufferStart;
			if (ln > size) {
				ln = static_cast<size_t>(size);
			};
			if (!writeData(c->proxy, c, c->bufferAtoB + c->bufferStart, ln)) {
				return false;
			};
			c->bufferStart += ln;
			size -= ln;
		};
		return true;
	};

	// Forward a chunked request body from the client to the proxy
	static bool forwardChunked(Application::Connection *c) {
		size_t ln;
		uint64_t size;

		for (;;) {
			c->headerLength = 0;
			ln = readLine(c, false);
			if (ln == 0) {
				return false;
			};
			if (!parseChunkSize(c->header, ln, size)) {
				return false;
			};
			if (!writeData(c->proxy, c, c->header, ln)) {
				return false;
			};
			if (size == 0) {
				break;
			};
			if (!forwardBody(c, size)) {
				return false;
			};
			// chunk data end
			c->headerLength = 0;
			ln = readLine(c, false);
			if (!isEmptyLine(c->header, ln)) {
				return false;
			};
			if (!writeData(c->proxy, c, c->header, ln)) {
				return false;
			};
		};

		// trailer, ends with an empty line
		for (;;) {
			c->headerLength = 0;
			ln = readLine(c, false);
			if (ln == 0) {
				return false;
			};
			if (!writeData(c->proxy, c, c->header, ln)) {
				return false;
			};
			if (isEmptyLine(c->header, ln)) {
				return true;
			};
		};
	};

	// Forward everything from the client to the proxy
	static void forwardTunnel(Application::Connection *c) {
		size_t ln;

		if (c->bufferStart < c->bufferEnd) {
			if (!writeData(c->proxy, c, c->bufferAtoB + c->bufferStart, c->bufferEnd - c->bufferStart)) {
				return;
			};
		};
		c->bufferStart = 0;
		c->bufferEnd = 0;

		for (;;) {
			if (!waitData(c->client, c, false)) {
				break;
			};
			ln = c->client.read(c->bufferAtoB, bufferSize);
			if (ln == 0) {
				break;
			};
			if (!writeData(c->proxy, c, c->bufferAtoB, ln)) {
				break;
			};
		};
	};

	// Client to proxy, reads each request header and adds the proxy authorization.
	// Owns the connection sockets, closes them at the end.
	void Application::threadAToB(Application::Connection *c) {
		Application *super = c->super;
		const String &authorization = super->proxyAuthorization;
		bool isProxyOpen = false;
		bool isWriterStarted = false;
		bool isValid;
		bool isTunnel;
		bool isChunked;
		bool hasContentLength;
		uint64_t contentLength;
		uint64_t number;
		size_t ln;
		size_t lineStart;
		const char *line;
		const char *value;
		size_t valueLength;

		c->bufferStart = 0;
		c->bufferEnd = 0;

		for (;;) {
			// request line, empty lines before it are ignored
			c->headerLength = 0;
			ln = readLine(c, true);
			if (ln == 0) {
				break;
			};
			if (isEmptyLine(c->header, ln)) {
				continue;
			};

			isValid = true;
			isTunnel = (ln > 8) && (memcmp(c->header, "CONNECT ", 8) == 0);
			isChunked = false;
			hasContentLength = false;
			contentLength = 0;

			// header lines
			for (;;) {
				lineStart = c->headerLength;
				ln = readLine(c, true);
				if (ln == 0) {
					isValid = false;
					break;
				};
				line = c->header + lineStart;
				if (isEmptyLine(line, ln)) {
					c->headerLength = lineStart;
					memcpy(c->header + c->headerLength, authorization.value(), authorization.length());
					c->headerLength += authorization.length();
					memcpy(c->header + c->headerLength, "\r\n", 2);
					c->headerLength += 2;
					break;
				};
				// replaced by the proxy authorization
				if (getHeader(line, ln, "proxy-authorization", value, valueLength)) {
					c->headerLength = lineStart;
					continue;
				};
				if (getHeader(line, ln, "content-length", value, valueLength)) {
					if (!parseDecimal(value, valueLength, number)) {
						isValid = false;
						break;
					};
					if (hasContentLength && (number != contentLength)) {
						isValid = false;
						break;
					};
					hasContentLength = true;
					contentLength = number;
					continue;
				};
				if (getHeader(line, ln, "transfer-encoding", value, valueLength)) {
					if (hasToken(value, valueLength, "chunked")) {
						isChunked = true;
					};
					continue;
				};
				if (getHeader(line, ln, "upgrade", value, valueLength)) {
					isTunnel = true;
					continue;
				};
			};
			if (!isValid) {
				break;
			};
			if (isChunked) {
				contentLength = 0;
			};

			// send the start of the body with the header, one write, no small packet delay
			if (isTunnel || (contentLength > 0)) {
				ln = c->bufferEnd - c->bufferStart;
				if ((!isTunnel) && (ln > contentLength)) {
					ln = static_cast<size_t>(contentLength);
				};
				memcpy(c->header + c->headerLength, c->bufferAtoB + c->bufferStart, ln);
				c->headerLength += ln;
				c->bufferStart += ln;
				if (!isTunnel) {
					contentLength -= ln;
				};
			};

			if (!isProxyOpen) {
				if (!c->proxy.openClientX(super->proxyAddress)) {
					static const char badGateway[] =
					    "HTTP/1.1 502 Bad Gateway\r\n"
					    "Content-Type: text/plain\r\n"
					    "Content-Length: 43\r\n"
					    "Connection: close\r\n"
					    "\r\n"
					    "Proxy Forward: unable to connect to proxy\r\n";
					writeData(c->client, c, badGateway, sizeof(badGateway) - 1);
					break;
				};
				isProxyOpen = true;
			};

			if (!writeData(c->proxy, c, c->header, c->headerLength)) {
				break;
			};

			if (!isWriterStarted) {
				if (!c->writer.start((ThreadProcedure)threadBToA, c)) {
					break;
				};
				isWriterStarted = true;
			};

			if (isTunnel) {
				forwardTunnel(c);
				break;
			};
			if (isChunked) {
				if (!forwardChunked(c)) {
					break;
				};
				continue;
			};
			if (!forwardBody(c, contentLength)) {
				break;
			};
		};

		// end the writer
		c->closing.set(true);
		c->proxy.shutdown();
		if (isWriterStarted) {
			c->writer.join();
		};
		c->client.close();
		c->proxy.close();

		c->busy.set(false);
		super->slotFreeEvent.notify();
	};

	// Proxy to client
	void Application::threadBToA(Application::Connection *c) {
		size_t ln;

		for (;;) {
			if (!waitData(c->proxy, c, false)) {
				break;
			};
			ln = c->proxy.read(c->bufferBtoA, bufferSize);
			if (ln == 0) {
				break;
			};
			if (!writeData(c->client, c, c->bufferBtoA, ln)) {
				break;
			};
			c->activity.set(true);
		};

		// the client must see the end of the proxy connection, end the reader
		c->closing.set(true);
		c->client.shutdown();
	};

};

#ifndef XYO_PROXYFORWARD_LIBRARY
XYO_APPLICATION_WINMAIN(XYO::ProxyForward::Application);
#endif
