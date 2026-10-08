// Created by Grigore Stefan <g_stefan@yahoo.com>
// Public domain (Unlicense) <http://unlicense.org>
// SPDX-FileCopyrightText: 2023-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: Unlicense

// End to end test: this process is the client and a fake upstream proxy,
// proxy-forward (found on the PATH, output/bin with fabricare test) runs between them.
// Single threaded, every wait has a timeout, started proxy-forward processes are
// terminated on failure.

#include <XYO/System.hpp>
#include <XYO/Networking.hpp>

#include <stdio.h>
#include <string>
#include <vector>
#include <chrono>
#include <stdexcept>

using namespace XYO::System;
using namespace XYO::Networking;
using XYO::System::Shell::ProcessId;

// --proxy-username=user --proxy-password=p=a:ss, the password has = and :
static const char *authorization = "Proxy-Authorization: Basic dXNlcjpwPWE6c3M=\r\n";
// milliseconds
static const int timeout = 10 * 1000;

static std::vector<ProcessId> processList;

static void check(bool value, const char *message) {
	if (!value) {
		throw std::runtime_error(message);
	};
};

static int64_t now() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
};

static std::string toString(uint64_t value) {
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value);
	return buffer;
};

static String address(uint16_t port) {
	return String(("127.0.0.1:" + toString(port)).c_str());
};

static std::string toHex(uint64_t value) {
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "%llX", (unsigned long long)value);
	return buffer;
};

// ---

// Binary data, may contain 0
#define BINARY(x) std::string(x, sizeof(x) - 1)

static void writeAll(Socket &socket, const std::string &data) {
	check(socket.write(data.data(), data.size()) == data.size(), "write");
};

// Read exactly size bytes
static std::string readSize(Socket &socket, size_t size) {
	std::string retV;
	char buffer[16384];
	size_t ln;
	int64_t end = now() + timeout;
	while (retV.size() < size) {
		check(now() < end, "read timeout");
		if (socket.waitToRead(100 * 1000) <= 0) {
			continue;
		};
		ln = size - retV.size();
		if (ln > sizeof(buffer)) {
			ln = sizeof(buffer);
		};
		ln = socket.read(buffer, ln);
		check(ln > 0, "connection closed before all data was read");
		retV.append(buffer, ln);
	};
	return retV;
};

// Read until the connection is closed by the peer
static std::string readToEnd(Socket &socket) {
	std::string retV;
	char buffer[16384];
	size_t ln;
	int64_t end = now() + timeout;
	for (;;) {
		check(now() < end, "connection not closed");
		if (socket.waitToRead(100 * 1000) <= 0) {
			continue;
		};
		ln = socket.read(buffer, sizeof(buffer));
		if (ln == 0) {
			return retV;
		};
		retV.append(buffer, ln);
	};
};

static void checkRead(Socket &socket, const std::string &expected, const char *message) {
	check(readSize(socket, expected.size()) == expected, message);
};

static void acceptUpstream(Socket &upstream, Socket &connection) {
	check(upstream.waitToRead(timeout * 1000) > 0, "no connection to the upstream proxy");
	check(upstream.accept(connection), "accept upstream connection");
};

static bool hasNoConnection(Socket &upstream, int milliSeconds) {
	return upstream.waitToRead(milliSeconds * 1000) == 0;
};

// Client data to the upstream connection, in small pieces, without
// filling the socket buffers (the client and the upstream are in this thread)
static std::string transfer(Socket &from, Socket &to, const std::string &data, size_t expectedSize) {
	std::string retV;
	char buffer[16384];
	size_t sent = 0;
	size_t ln;
	int64_t end = now() + timeout;
	while (retV.size() < expectedSize) {
		check(now() < end, "transfer timeout");
		if ((sent < data.size()) && (sent < retV.size() + 32768)) {
			ln = data.size() - sent;
			if (ln > 4096) {
				ln = 4096;
			};
			writeAll(from, data.substr(sent, ln));
			sent += ln;
		};
		if (to.waitToRead((sent < data.size()) ? 1000 : 100 * 1000) <= 0) {
			continue;
		};
		ln = to.read(buffer, sizeof(buffer));
		check(ln > 0, "upstream connection closed during transfer");
		retV.append(buffer, ln);
	};
	return retV;
};

// ---

static uint16_t findFreePort(uint16_t start) {
	uint16_t port;
	for (port = start; port < start + 1000; ++port) {
		Socket socket;
		if (socket.openServerX(address(port)) && socket.listen(1)) {
			return port;
		};
	};
	throw std::runtime_error("no free port");
};

static void openUpstream(Socket &upstream, uint16_t &port, uint16_t start) {
	for (port = start; port < start + 1000; ++port) {
		if (upstream.openServerX(address(port)) && upstream.listen(16)) {
			return;
		};
	};
	throw std::runtime_error("no free port for the upstream proxy");
};

static std::string proxyCommand(uint16_t localPort, uint16_t upstreamPort) {
	return "proxy-forward --proxy-server=127.0.0.1 --proxy-port=" + toString(upstreamPort) +
	       " --proxy-username=user --proxy-password=p=a:ss --local-port=" + toString(localPort) +
	       " --thread-count=8";
};

static bool waitTerminated(ProcessId processId, int milliSeconds) {
	int64_t end = now() + milliSeconds;
	while (!Shell::isProcessTerminated(processId)) {
		if (now() >= end) {
			return false;
		};
		Thread::sleep(50);
	};
	return true;
};

// Start proxy-forward and wait until it accepts connections
static ProcessId startProxyCommand(const std::string &command, uint16_t localPort) {
	ProcessId processId = Shell::executeNoWait(command.c_str());
	check(processId != 0, "unable to start proxy-forward, is output/bin on the PATH?");
	processList.push_back(processId);

	int64_t end = now() + timeout;
	for (;;) {
		Socket client;
		if (client.openClientX(address(localPort))) {
			return processId;
		};
		// exited, an error message box is not expected
		check(!Shell::isProcessTerminated(processId), "proxy-forward exited at start");
		check(now() < end, "proxy-forward does not accept connections");
		Thread::sleep(50);
	};
};

static ProcessId startProxy(uint16_t localPort, uint16_t upstreamPort) {
	return startProxyCommand(proxyCommand(localPort, upstreamPort), localPort);
};

static void closeProxy(uint16_t localPort) {
	std::string command = "proxy-forward --close --local-port=" + toString(localPort);
	check(Shell::execute(command.c_str()) == 0, "proxy-forward --close failed");
};

// ---

// Requests on one keep-alive connection, the authorization is added to each
static void testRequests(Socket &upstream, uint16_t localPort) {
	Socket client;
	Socket connection;
	std::string request;
	std::string expected;
	std::string response;
	std::string body;
	std::string chunk;
	size_t k;

	check(client.openClientX(address(localPort)), "connect to proxy-forward");

	// GET, the authorization of the client is removed, any case
	writeAll(client,
	         "GET http://example.test/a HTTP/1.1\r\n"
	         "Host: example.test\r\n"
	         "Proxy-Authorization: Basic d3Jvbmc6d3Jvbmc=\r\n"
	         "Accept: */*\r\n"
	         "proxy-authorization:Basic eA==\r\n"
	         "\r\n");
	acceptUpstream(upstream, connection);
	expected = std::string("GET http://example.test/a HTTP/1.1\r\n"
	                       "Host: example.test\r\n"
	                       "Accept: */*\r\n") +
	           authorization + "\r\n";
	checkRead(connection, expected, "GET header");
	response = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
	writeAll(connection, response);
	checkRead(client, response, "GET response");
	printf("GET: ok\n");

	// Pipelined POST and GET, the body looks like a request, it is not changed
	body = "hello\r\n\r\nGET http://example.test/x HTTP/1.1\r\n\r\n";
	request = "\r\nPOST http://example.test/b HTTP/1.1\r\n"
	          "Host: example.test\r\n"
	          "content-length:  " +
	          toString(body.size()) + " \r\n\r\n" + body +
	          "GET http://example.test/c HTTP/1.1\r\n"
	          "Host: example.test\r\n"
	          "\r\n";
	writeAll(client, request);
	expected = "POST http://example.test/b HTTP/1.1\r\n"
	           "Host: example.test\r\n"
	           "content-length:  " +
	           toString(body.size()) + " \r\n" + authorization + "\r\n" + body +
	           "GET http://example.test/c HTTP/1.1\r\n"
	           "Host: example.test\r\n" +
	           authorization + "\r\n";
	checkRead(connection, expected, "pipelined POST and GET");
	check(hasNoConnection(upstream, 200), "keep-alive, the upstream connection is used again");
	printf("POST with Content-Length, pipelined: ok\n");

	// Chunked body with extension and trailer, then a GET
	chunk = "\r\n\r\nGET http://example.test/y HTTP/1.1\r\n\r\n";
	body = "5;name=value\r\nhello\r\n" + toHex(chunk.size()) + "\r\n" + chunk + "\r\n0\r\nX-Trailer: 1\r\n\r\n";
	request = "POST http://example.test/d HTTP/1.1\r\n"
	          "Host: example.test\r\n"
	          "Transfer-Encoding: chunked\r\n"
	          "\r\n" +
	          body +
	          "GET http://example.test/e HTTP/1.1\r\n"
	          "Host: example.test\r\n"
	          "\r\n";
	writeAll(client, request);
	expected = std::string("POST http://example.test/d HTTP/1.1\r\n"
	                       "Host: example.test\r\n"
	                       "Transfer-Encoding: chunked\r\n") +
	           authorization + "\r\n" + body +
	           "GET http://example.test/e HTTP/1.1\r\n"
	           "Host: example.test\r\n" +
	           authorization + "\r\n";
	checkRead(connection, expected, "chunked POST and GET");
	printf("POST chunked: ok\n");

	// Large body, larger than the buffers, then a GET
	body.clear();
	for (k = 0; k < 300000; ++k) {
		body += static_cast<char>("GET \r\n:Proxy-Authorization"[k % 26]);
	};
	request = "PUT http://example.test/f HTTP/1.1\r\n"
	          "Content-Length: " +
	          toString(body.size()) + "\r\n\r\n" + body +
	          "GET http://example.test/g HTTP/1.1\r\n\r\n";
	expected = "PUT http://example.test/f HTTP/1.1\r\n"
	           "Content-Length: " +
	           toString(body.size()) + "\r\n" + authorization + "\r\n" + body +
	           "GET http://example.test/g HTTP/1.1\r\n" + authorization + "\r\n";
	check(transfer(client, connection, request, expected.size()) == expected, "large body");
	printf("PUT large body: ok\n");

	// Large response
	body.clear();
	for (k = 0; k < 500000; ++k) {
		body += static_cast<char>('a' + (k % 26));
	};
	response = "HTTP/1.1 200 OK\r\nContent-Length: " + toString(body.size()) + "\r\n\r\n" + body;
	check(transfer(connection, client, response, response.size()) == response, "large response");
	printf("Large response: ok\n");

	// Client closes, the upstream connection is closed
	client.close();
	check(readToEnd(connection).empty(), "upstream connection closed after the client");
	printf("Close: ok\n");
};

// CONNECT, the data after the request is not changed, both directions
static void testConnect(Socket &upstream, uint16_t localPort) {
	Socket client;
	Socket connection;
	std::string data;
	std::string expected;

	check(client.openClientX(address(localPort)), "connect to proxy-forward");

	data = BINARY("\x16\x03\x01\x00\x05GET / HTTP/1.1\r\nProxy-Authorization: x\r\n\r\n");
	writeAll(client, "CONNECT example.test:443 HTTP/1.1\r\nHost: example.test:443\r\n\r\n" + data);
	acceptUpstream(upstream, connection);
	expected = std::string("CONNECT example.test:443 HTTP/1.1\r\nHost: example.test:443\r\n") + authorization + "\r\n" + data;
	checkRead(connection, expected, "CONNECT header and data");

	data = BINARY("HTTP/1.1 200 Connection established\r\n\r\n\x16\x03\x01\x00\x00");
	writeAll(connection, data);
	checkRead(client, data, "CONNECT response");

	data = "GET / HTTP/1.1\r\nHost: example.test\r\n\r\n";
	writeAll(client, data);
	checkRead(connection, data, "tunnel data, no authorization added");

	// the upstream closes, the client sees the end
	connection.close();
	check(readToEnd(client).empty(), "client connection closed after the upstream");
	printf("CONNECT: ok\n");
};

// Request header larger than the limit, the connection is closed, nothing forwarded
static void testHeaderTooLarge(Socket &upstream, uint16_t localPort) {
	Socket client;
	std::string data;

	check(client.openClientX(address(localPort)), "connect to proxy-forward");
	data = "GET http://example.test/";
	data.append(70000, 'a');
	client.write(data.data(), data.size());
	readToEnd(client);
	check(hasNoConnection(upstream, 200), "header too large is not forwarded");
	printf("Header too large: ok\n");
};

// Upstream proxy not reachable: 502
static void testBadGateway(uint16_t localPort) {
	Socket client;
	std::string response;
	const char *body = "Proxy Forward: unable to connect to proxy\r\n";

	check(client.openClientX(address(localPort)), "connect to proxy-forward");
	writeAll(client, "GET http://example.test/ HTTP/1.1\r\nHost: example.test\r\n\r\n");
	response = readToEnd(client);
	check(response.compare(0, 26, "HTTP/1.1 502 Bad Gateway\r\n") == 0, "502 status");
	check(response.find("\r\nContent-Length: " + toString(strlen(body)) + "\r\n") != std::string::npos, "502 Content-Length");
	check(response.size() > strlen(body) && response.compare(response.size() - strlen(body), strlen(body), body) == 0, "502 body");
	printf("Bad gateway: ok\n");
};

// Options from a file: one per line, quotes, last value wins, unknown options ignored
static void testOptionsFile(Socket &upstream, uint16_t localPort, uint16_t upstreamPort) {
	const char *fileName = "test.01.options.txt";
	std::string options;
	std::string expected;
	ProcessId processId;
	Socket client;
	Socket connection;

	options = "--proxy-server=127.0.0.1\r\n";
	options += "--proxy-port=" + toString(upstreamPort) + "\r\n";
	options += "\"--proxy-username=us er\"\r\n";
	options += "--proxy-password=wrong\r\n";
	options += "--proxy-password=pw\r\n";
	options += "--unknown-option=1\r\n";
	check(Shell::filePutContents(fileName, reinterpret_cast<const uint8_t *>(options.data()), options.size()), "write options file");
	processId = startProxyCommand("proxy-forward --local-port=" + toString(localPort) + " @" + fileName, localPort);

	check(client.openClientX(address(localPort)), "connect to proxy-forward");
	writeAll(client, "GET http://example.test/ HTTP/1.1\r\n\r\n");
	acceptUpstream(upstream, connection);
	expected = "GET http://example.test/ HTTP/1.1\r\nProxy-Authorization: Basic dXMgZXI6cHc=\r\n\r\n";
	checkRead(connection, expected, "credentials from the options file");
	client.close();
	readToEnd(connection);

	closeProxy(localPort);
	check(Shell::isProcessTerminated(processId), "options file instance ends");
	Shell::removeFile(fileName);

	// nothing to close
	closeProxy(localPort);
	printf("Options file: ok\n");
};

// A client that does not read a response must not block the close
static void testCloseWithStalledClient(Socket &upstream, uint16_t localPort, ProcessId processId) {
	Socket client;
	Socket connection;
	std::string data;
	int64_t start;
	int64_t lastWrite;
	bool isStalled = false;

	check(client.openClientX(address(localPort)), "connect to proxy-forward");
	writeAll(client, "GET http://example.test/large HTTP/1.1\r\nHost: example.test\r\n\r\n");
	acceptUpstream(upstream, connection);
	readSize(connection, strlen("GET http://example.test/large HTTP/1.1\r\nHost: example.test\r\n") + strlen(authorization) + 2);
	writeAll(connection, "HTTP/1.1 200 OK\r\nContent-Length: 1000000000\r\n\r\n");

	// write until all the buffers are full, the client never reads
	data.assign(4096, 'x');
	start = now();
	lastWrite = start;
	while (now() - start < timeout) {
		if (connection.waitToWrite(100 * 1000) > 0) {
			writeAll(connection, data);
			lastWrite = now();
			continue;
		};
		if (now() - lastWrite > 1000) {
			isStalled = true;
			break;
		};
	};
	check(isStalled, "the response was not blocked by the client");

	start = now();
	closeProxy(localPort);
	check(waitTerminated(processId, timeout), "proxy-forward does not end with a stalled client");
	printf("Close with a stalled client: ok (%d ms)\n", static_cast<int>(now() - start));
};

void test() {
	Socket upstream;
	uint16_t upstreamPort;
	uint16_t unusedPort;
	uint16_t localPort;
	uint16_t localPort2;
	ProcessId processId;
	ProcessId processId2;

	check(Network::isValid(), "network");

	openUpstream(upstream, upstreamPort, 23500);
	localPort = findFreePort(upstreamPort + 1);
	processId = startProxy(localPort, upstreamPort);

	// A second instance on the same local port ends right away, the first one still works
	check(Shell::execute(proxyCommand(localPort, upstreamPort).c_str()) == 0, "second instance");
	check(!Shell::isProcessTerminated(processId), "first instance still running");
	printf("Single instance: ok\n");

	testRequests(upstream, localPort);
	testConnect(upstream, localPort);
	testHeaderTooLarge(upstream, localPort);

	// --close returns after the instance ended, a new one can start right away
	closeProxy(localPort);
	check(Shell::isProcessTerminated(processId), "--close returns after the instance ended");
	processId = startProxy(localPort, upstreamPort);
	printf("Close and start again: ok\n");

	// Upstream proxy not listening
	unusedPort = findFreePort(localPort + 1);
	localPort2 = findFreePort(unusedPort + 1);
	processId2 = startProxy(localPort2, unusedPort);
	testBadGateway(localPort2);
	closeProxy(localPort2);
	check(waitTerminated(processId2, timeout), "second proxy ends");

	testOptionsFile(upstream, localPort2, upstreamPort);

	testCloseWithStalledClient(upstream, localPort, processId);

	processList.clear();
	printf("Done.\n");
};

int main(int cmdN, char *cmdS[]) {
	try {
		test();
		return 0;
	} catch (const std::exception &e) {
		printf("* Error: %s\n", e.what());
	} catch (...) {
		printf("* Error: Unknown\n");
	};

	for (ProcessId processId : processList) {
		Shell::terminateProcess(processId, 0);
	};

	return 1;
};
