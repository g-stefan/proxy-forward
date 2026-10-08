# Reference

For people working on `proxy-forward` itself. Using the program is covered
in [Command line](command-line.md) and [How it works](how-it-works.md).

## fabricare project

```json
{
	"name" : "proxy-forward",
	"make" : "exe",
	"SPDX-License-Identifier": "MIT",
	"sourcePath" : "XYO/ProxyForward",
	"dependency" : [ "xyo-networking", "xyo-system", "xyo-win" ]
},
{
	"name" : "test.01",
	"make" : "exe",
	"category" : "test",
	"SPDX-License-Identifier": "Unlicense",
	"dependency" : [ "xyo-networking", "xyo-system" ]
}
```

`proxy-forward` is a Windows executable built from every source in
`source/XYO/ProxyForward/`. The dependencies bring in `xyo-encoding`,
`xyo-multithreading`, `xyo-data-structures`, `xyo-managed-memory` and
`xyo-platform`. The version lives in `version.json` under the key
`proxy-forward`; `Version.rh` is generated from `Version.Template.rh` by
xyo-version (`fabricare version` bumps the build number).

```bash
fabricare make       # output/bin/proxy-forward.exe
fabricare test       # build test.01 into output/test and run it, output/bin is on the PATH
fabricare install
fabricare release
fabricare clean
```

`fabricare test` does not build the program: run `make` first, otherwise the
test runs an older `proxy-forward` (the one in `output/bin`, or on the
`PATH`).

## Test

`test/test.01.cpp` is an end to end test, single threaded, every wait with a
timeout. It opens a fake upstream proxy on a free local port, starts
`proxy-forward` (from the `PATH`) between it and a client socket, and checks:

| Case | Checks |
|------|--------|
| Single instance | a second start on the same port ends with `0`, the first keeps running |
| GET | the client `Proxy-Authorization` lines (any case) are removed, the credentials are added before the end of the header; the response comes back unchanged |
| POST, pipelined | leading empty lines skipped, `Content-Length` with spaces, a body that looks like a request is not changed, the next request gets the credentials, the upstream connection is reused |
| Chunked | chunk extensions, a chunk that looks like a request, trailer, next request |
| Large body / response | 300 000 / 500 000 bytes, larger than the buffers |
| Close | the client closes, the upstream connection is closed |
| CONNECT | header with credentials, then binary data both ways unchanged (no credentials added in the tunnel), the upstream closes, the client sees the end |
| Header too large | 70 000 bytes without a line end: closed, nothing forwarded |
| `--close` | returns after the instance ended, a new instance starts on the same port right away |
| Bad gateway | upstream port not listening: `502` response with the right `Content-Length` |
| Options file | `@file` with one option per line, a quoted value with a space, an option given twice (last wins), an unknown option; `--close` with nothing running ends with `0` |
| Close with a stalled client | a client that never reads a large response: `--close` still ends the instance |

The test never triggers an error message box (it would wait for a user); if
`proxy-forward` does not answer, the test fails after its timeout and
terminates the instances it started.

## Headers

| Header | Contents |
|--------|----------|
| `<XYO/ProxyForward/Dependency.hpp>` | includes `<XYO/System.hpp>`, `<XYO/Networking.hpp>`, `<XYO/Win.hpp>`; `namespace XYO::ProxyForward` uses `XYO::System`, `XYO::Networking`, `XYO::Win` |
| `<XYO/ProxyForward/Application.hpp>` | `XYO::ProxyForward::Application` |
| `<XYO/ProxyForward/Copyright.hpp>`, `License.hpp`, `Version.hpp` | program metadata |
| `Application.rh`, `Copyright.rh`, `Version.rh` | macros shared by the C++ code and the Windows resource script |

## Macros

| Macro | Meaning |
|-------|---------|
| `XYO_PROXYFORWARD_LIBRARY` | if defined, `Application.cpp` does not define the entry point (`XYO_APPLICATION_WINMAIN` is skipped); no fabricare project sets it |
| `XYO_PROXYFORWARD_NO_VERSION` | `Version.rh` gives `0.0.0` / build `0` instead of the generated version |
| `XYO_PROXYFORWARD_VERSION_ABCD`, `_STR`, `_STR_BUILD`, `_STR_DATETIME`, `_STR_WITH_BUILD` | version, from `version.json` |
| `XYO_PROXYFORWARD_COPYRIGHT`, `_PUBLISHER`, `_COMPANY`, `_CONTACT` | copyright strings |

## class XYO::ProxyForward::Application

Derived from `XYO::Win::SimpleApplication` (a hidden window and a message
loop). Entry point: `XYO_APPLICATION_WINMAIN(XYO::ProxyForward::Application)`.

| Member | Role |
|--------|------|
| `main(cmdN, cmdS)` | expands `@file`, parses the options, single instance check (`getSingleInstanceWindow()` finds the window `Proxy#<local-port>`), `--close`, validates, builds `proxyAuthorization` and `proxyAddress`, opens and listens on `127.0.0.1:<local-port>`, then `SimpleApplication::main` (creates the window, runs the message loop) |
| `windowProcedure` | `WM_CREATE`: allocates `threadCount` `Connection` slots, starts `serverThread`; `WM_DESTROY`: `stopServer()` |
| `setShowCmd` | returns `SW_HIDE`, the window is never shown |
| `stopServer()` | notifies `serverStopEvent`, joins the server thread and every connection thread, frees the slots; called on `WM_DESTROY` and by the destructor |
| `threadServer` | finds a free slot (waits on `slotFreeEvent` when all are busy), waits for a connection, accepts it into the slot, starts `threadAToB` |
| `threadAToB` | client → upstream: parses each request, edits the header, opens the upstream connection, starts `threadBToA`, forwards bodies and tunnels; owns the slot sockets and closes them at the end, then frees the slot |
| `threadBToA` | upstream → client: copies everything; at the end sets `closing` and shuts down the client socket so `threadAToB` ends too |
| `proxyAuthorization` | `Proxy-Authorization: Basic <base64>\r\n` |
| `proxyAddress` | `<proxy-server>:<proxy-port>` passed to `Socket::openClientX` |
| `serverStopEvent` | static `Semaphore`, only `peek()`-ed by the threads (never consumed), set by `stopServer` |

### struct Application::Connection

One slot per allowed connection. The sockets are owned by the server thread
while the slot is free and by the connection threads while it is busy.

| Field | Role |
|-------|------|
| `client`, `proxy` | the two sockets |
| `reader`, `writer` | `threadAToB`, `threadBToA` |
| `busy` | set by the server thread at accept, cleared by `threadAToB` at the end |
| `activity` | set by `threadBToA` after each block sent to the client, resets the idle timeout of `threadAToB` |
| `closing` | set by the first connection thread that ends; the other one sees it after its next wait (a socket wait does not end when the socket is shut down by this process) |
| `bufferAtoB`, `bufferBtoA` | transfer buffers, `bufferSize` each |
| `header` | the request header being built: up to `headerSizeMax`, plus the credentials, `\r\n`, and up to `bufferSize` of body start |
| `bufferStart`, `bufferEnd`, `headerLength` | positions in `bufferAtoB` and `header` |

### Constants (Application.cpp)

| Constant | Value | Meaning |
|----------|-------|---------|
| `defaultThreadCount` / `maximumThreadCount` | 64 / 1024 | `--thread-count` |
| `listenQueue` | 256 | listen backlog |
| `bufferSize` | 32 KiB | transfer buffers, largest single socket write |
| `headerSizeMax` | 64 KiB | request header limit, also chunk size and trailer lines |
| `waitInterval` | 100 ms | every socket wait (read and write); stop and `closing` are checked after it |
| `idleTimeout` | 5 min (3000 wait intervals) | idle client connection between requests |
| `slotWaitInterval` | 250 ms | server thread wait for a free slot |
| `closeTimeout` | 30 s | `--close` wait for the instance to end |

### Helpers (Application.cpp, static)

| Function | Role |
|----------|------|
| `waitData(socket, c, useIdleTimeout)` | wait until `socket` can be read; false on stop, `closing`, error, or idle timeout |
| `writeData(socket, c, data, size)` | wait until `socket` can be written, write in pieces of at most `bufferSize`; false on stop, `closing` or error. Never blocks without checking stop, so a client that does not read can not block `stopServer` |
| `fillBuffer` / `readLine` | read from the client into `bufferAtoB`, append one line to `header` (limit `headerSizeMax`) |
| `getHeader(line, ln, name, value, valueLength)` | case insensitive header name match, value trimmed |
| `parseDecimal`, `parseChunkSize`, `hasToken` | `Content-Length`, chunk size line, `Transfer-Encoding` token |
| `forwardBody`, `forwardChunked`, `forwardTunnel` | the three body modes |

## Rules when changing the code

- Sockets are closed only by the thread that owns them; other threads only
  set `closing` or call `shutdown()`.
- Every socket wait and write must come back at most every `waitInterval` to
  check `serverStopEvent` and `closing`, otherwise `--close` can hang.
- Keep the error paths of the test free of message boxes.
- Keep `docs/` and the skill (`.claude/skills/proxy-forward/SKILL.md`) in step
  with `Application.cpp` when the behavior changes.
- Licensing follows REUSE: `source/`, `docs/` and `README.md` are MIT (source
  files also carry SPDX headers); `test/`, config files, `fabricare.json`,
  `version.json` and `.claude/` are Unlicense. Every new top level file or
  folder needs a `Files:` entry in `.reuse/dep5` (check with
  `python -m reuse lint`).
