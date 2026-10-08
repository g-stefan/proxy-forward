---
name: proxy-forward
description: >-
  How to use proxy-forward, the XYO Windows program (C++,
  XYO::ProxyForward, on xyo-networking / xyo-system / xyo-win) that runs
  a local HTTP proxy on 127.0.0.1:<local-port> forwarding to an upstream
  HTTP proxy and adding "Proxy-Authorization: Basic" credentials to every
  request, so Chrome and other clients that can not store proxy
  credentials can use an authenticated proxy. Covers the options
  (--proxy-server, --proxy-port, --proxy-username, --proxy-password,
  --local-port, --thread-count, --close, @file), one hidden instance per
  port, --close waiting for the end, message boxes instead of console
  output, exit codes, start "" in batch files, client setup (Chrome,
  curl, git, system proxy), request handling (CONNECT / Upgrade tunnels,
  chunked, keep-alive), 502 / 407, timeouts, connection slots, security
  and the end to end test. Use when setting up, scripting,
  troubleshooting or changing proxy-forward, or working in its
  repository.
---

# proxy-forward

Local HTTP proxy that adds proxy authentication:

```
proxy-forward --proxy-server=proxy.example.com --proxy-port=3128 --proxy-username=user --proxy-password=secret --local-port=8080
```

Clients use `http://127.0.0.1:8080` as their proxy **without** credentials;
`proxy-forward` forwards each connection to `proxy.example.com:3128` and adds
`Proxy-Authorization: Basic base64(user:secret)` to every request header.
Purpose: Chrome (and many tools) can be given a proxy but not its
username / password.

Windows only, a GUI program with a hidden window: **no console output**,
usage and errors are **message boxes** (they block until OK is pressed). Not
a library.

Full documentation: `docs/` in the proxy-forward repository
(`X:\Storage\XYO\Gitea\CPP\proxy-forward\docs` on this machine): README,
getting-started, **command-line**, **how-it-works**, reference. The whole
implementation is `source/XYO/ProxyForward/Application.cpp`.

## Pick a command

| Want | Command |
|------|---------|
| Start | `proxy-forward --proxy-server=H --proxy-port=P --proxy-username=U --proxy-password=W --local-port=8080` |
| Start from a batch file / at logon | `start "" proxy-forward @%USERPROFILE%\proxy-forward.txt` (a batch file waits for GUI programs) |
| Stop (waits until it ended, port free) | `proxy-forward --close --local-port=8080` |
| Change options | `--close` first, then start again (a start on a busy port does nothing) |
| More simultaneous connections | `--thread-count=256` (1..1024, default 64, ~160 KiB each) |
| Several accounts / proxies | one instance per `--local-port` |
| Check | `curl -x http://127.0.0.1:8080 -I https://example.com/` |
| Help / version / license | `--help` (or `--usage`), `--version`, `--license` (message box) |

Options file (`@file`): arguments split on spaces / tabs / line ends,
`"..."` groups, `\"` literal quote, no BOM, no nested `@file`. Keeps the
password out of the process command line.

## Client settings

| Client | Setting |
|--------|---------|
| Chrome / Edge | `chrome.exe --proxy-server="http://127.0.0.1:8080"` or the Windows system proxy |
| Windows | Settings → Network & internet → Proxy → Manual: `127.0.0.1`, port `8080` |
| curl | `-x http://127.0.0.1:8080` / `HTTPS_PROXY` |
| git | `git config --global http.proxy http://127.0.0.1:8080` |

Use `127.0.0.1`, not `localhost` (listens on IPv4 loopback only). Do not
give the client credentials: its `Proxy-Authorization` is removed.

## Hard rules

1. **Option syntax** is `--name=value`, value = everything after the first
   `=` (passwords may contain `=` and `:`). Unknown options and non `--`
   arguments are **silently ignored** — a typo shows up as
   `Error: proxy-server is empty`. Last value wins. Case sensitive.
2. **One instance per local port** (hidden window class `Proxy Forward`,
   title `Proxy#<local-port>`, port matched as text). Starting again on a
   running port exits `0` and changes nothing.
3. **`--close`** posts `WM_CLOSE` and waits for the process to end, up to
   30 s; exit `0` (also if nothing was running), `1` if it did not end in
   time (no message box). 3.0.0 build 16 and earlier returned immediately
   and could hang with a client that stopped reading: use a newer build.
4. **Exit codes**: `0` ok / already running / closed / help; `1` error
   (after a message box: `local-port is empty`, `proxy-server is empty`,
   `proxy-port`, `proxy-username`, `proxy-password` empty,
   `thread-count must be a number from 1 to 1024`, `file not found - f`,
   `unable to listen on local port N`, `network not initialized`).
   The upstream is not checked at start.
5. **Requests**: request line and headers parsed (CRLF or LF), empty
   lines before the request line skipped, all `Proxy-Authorization`
   lines removed, credentials line inserted before the empty line,
   everything else unchanged. Bodies by `Content-Length` (digits, equal
   duplicates) or `chunked` (extensions, trailers), forwarded unchanged;
   keep-alive and pipelining work; one upstream connection per client
   connection, opened at the first request.
6. **Tunnels**: `CONNECT` or any `Upgrade` header → credentials on that
   request only, then raw copy both ways (HTTPS stays end to end).
7. **Responses are never parsed**: a `407` comes from the upstream (bad
   credentials). `proxy-forward`'s own error is only
   `HTTP/1.1 502 Bad Gateway` + `Proxy Forward: unable to connect to proxy`
   (upstream unreachable). Header > 64 KiB, bad `Content-Length`, bad
   chunks: connection closed without a response.
8. **Timeouts**: 5 min idle between requests (no data either way); none
   in bodies or tunnels; upstream connect uses the system timeout.
9. **Slots**: all busy → new connections wait in the listen queue (256),
   not refused. Browsers keep idle connections; raise `--thread-count` if
   loading stalls.
10. **Security**: loopback only, but no local authentication — every local
    user / program can use the account. Basic auth is base64, clear text
    to the upstream. Plain HTTP proxy only: no TLS to the proxy, no NTLM /
    Digest / Kerberos / SOCKS. A client that half-closes after its request
    loses the response. A declined `Upgrade` leaves a tunnel without
    credentials for later requests.

## Troubleshooting

| Symptom | Cause |
|---------|-------|
| Nothing happens at start | normal: it runs hidden (check Task Manager) or an instance already runs on that port |
| Message box `unable to listen on local port` | port in use, or not a number |
| Client: connection refused on 127.0.0.1:port | not running on that port, or the client uses `localhost` → `::1` |
| `502 Bad Gateway` | `--proxy-server` / `--proxy-port` wrong or unreachable |
| `407 Proxy Authentication Required` | wrong username / password (upstream answer) |
| New options not used | the old instance still runs: `--close` then start |
| Script stuck after starting | batch file waits for GUI programs: use `start ""` |
| Script stuck on an error | a message box waits for OK |

## Working inside this repository

- fabricare projects: `proxy-forward` (`make: exe`, source
  `source/XYO/ProxyForward`, depends on `xyo-networking`, `xyo-system`,
  `xyo-win`) and `test.01` (category test). See the `fabricare` skill for
  running fabricare on this machine. `fabricare make` then
  `fabricare test` (the test runs `output/bin/proxy-forward`, not rebuilt
  by `test`), `fabricare clean` afterwards.
- `test/test.01.cpp`: end to end, single threaded, fake client + fake
  upstream on free ports, starts real `proxy-forward` processes; covers
  auth injection, pipelining, chunked, large bodies, CONNECT, too large
  header, 502, single instance, `--close` waiting, close with a stalled
  client. Must never hit an error message box (it would hang); it
  terminates the instances it started on failure.
- Threading (see `docs/reference.md`): per slot `threadAToB` (owns and
  closes sockets, frees the slot) and `threadBToA`; `closing` flag ends
  the pair; static `serverStopEvent` only `peek()`-ed. Every socket wait
  **and write** (`writeData`: `waitToWrite` + pieces of `bufferSize`)
  returns at least every 100 ms to check stop / `closing` — a blocking
  write to a client that does not read would hang `--close`.
- Uses `xyo-networking` `Socket` (`openServerX`, `openClientX`, `accept`,
  `waitToRead` / `waitToWrite` in µs, `read` / `write` returning bytes,
  0 on end or error, `shutdown` = both directions) — see the `xyo-system`,
  `xyo-multithreading` skills for the lower layers.
- Keep `docs/` and this skill in step with `Application.cpp`. Licensing
  follows REUSE via `.reuse/dep5` (`source/`, `docs/`, `README.md` MIT;
  `test/`, config, `fabricare.json`, `version.json`, `.claude/`
  Unlicense); check with `python -m reuse lint`. Files use CRLF.
