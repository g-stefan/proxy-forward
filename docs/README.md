# Proxy Forward — Documentation

`proxy-forward` is a small local HTTP proxy for Windows that adds proxy
authentication. It listens on `127.0.0.1:<local-port>`, forwards every
connection to an upstream HTTP proxy server, and adds the header
`Proxy-Authorization: Basic ...` (built from a username and a password) to
every request it forwards:

```
proxy-forward --proxy-server=proxy.example.com --proxy-port=3128 --proxy-username=user --proxy-password=secret --local-port=8080
```

A browser, or any other program, is then pointed at `127.0.0.1:8080` as its
proxy, without credentials, and reaches the internet through
`proxy.example.com:3128` as `user`.

```
browser / curl / git ...      no credentials
        |  http://127.0.0.1:8080
proxy-forward                 adds Proxy-Authorization: Basic base64(user:secret)
        |  proxy.example.com:3128
upstream HTTP proxy           checks the credentials
        |
internet
```

It is a Windows program without a console and without a visible window: it
runs in the background until it is closed with `--close`. It is not a
library.

## Why it exists

Google Chrome (and other Chromium browsers) can be told which proxy to use
(`--proxy-server=...`, policies, system settings), but there is no way to give
it the proxy username and password ahead of time. A proxy that requires
authentication makes it ask the user interactively, which does not work for
unattended, kiosk, automated or scripted browsers. The same is true for many
other programs that support a proxy but not proxy authentication.

`proxy-forward` moves the credentials out of the client: the client talks to
an open local proxy, `proxy-forward` adds the credentials on the way to the
real proxy.

| Need | What `proxy-forward` does |
|------|---------------------------|
| Use an authenticated proxy from Chrome without a login prompt | Chrome uses `127.0.0.1:<local-port>`, the credentials are added to each request |
| Use an authenticated proxy from a tool that has no proxy credentials option | same, point the tool at the local port |
| HTTPS sites | `CONNECT` requests get the credentials, then the TLS data is passed through untouched (end to end encryption is kept) |
| Several upstream proxies or accounts | one instance per local port |
| Start and stop from scripts | `proxy-forward ... --local-port=N` starts one instance per port, `proxy-forward --close --local-port=N` stops it and waits for it to end |

## Concepts at a glance

| Need | Command |
|------|---------|
| Start | `proxy-forward --proxy-server=host --proxy-port=port --proxy-username=user --proxy-password=password --local-port=8080` |
| Start with the options in a file | `proxy-forward @proxy-forward.txt` |
| Stop | `proxy-forward --close --local-port=8080` |
| More simultaneous connections | `--thread-count=256` (1 to 1024, default 64) |
| Help / version / license | `proxy-forward --help`, `--version`, `--license` (shown in a message box) |

## What to know before using it

- **Windows only.** It is built on `xyo-win` (a hidden window and a Windows
  message loop). Messages and errors are shown in **message boxes**, there is
  no console output.
- **Listens only on `127.0.0.1` (IPv4).** Other computers can not use it, but
  every program and every user on this computer can, with your credentials.
  Use `127.0.0.1`, not `localhost`, in the client settings.
- **One instance per local port.** Starting it again with the same
  `--local-port` does nothing (exit code `0`), even with other options:
  `--close` it first.
- **Only Basic authentication** to a plain HTTP proxy. No TLS to the proxy,
  no NTLM / Digest / Negotiate, no SOCKS. The credentials are only base64
  encoded, they cross the network to the proxy in clear text, like any
  HTTP proxy Basic authentication.
- **The password is on the command line**, visible to other programs of the
  same user. Put the options in a file and start it with `@file`, see
  [Command line](command-line.md#options-file).

## Contents

| Document | What it covers |
|----------|----------------|
| [Getting started](getting-started.md) | Download or build, first run, configure Chrome and other clients, start at logon, stop |
| [Command line](command-line.md) | Options, the options file, single instance, `--close`, messages, exit codes |
| [How it works](how-it-works.md) | Request handling, what is changed and what is not, keep-alive, bodies, `CONNECT`, errors, timeouts, connection slots, limits, security |
| [Reference](reference.md) | Source layout, the `Application` class, constants, macros, fabricare project, tests |

## Source map

```
source/XYO/ProxyForward/
    Application[.hpp, .cpp]             the program: options, single instance, server thread, connection threads, HTTP request parsing
    Dependency.hpp                      xyo-system, xyo-networking, xyo-win
    Copyright / License / Version       program metadata (XYO::ProxyForward::Version::version(), ...)
    Version.Template.rh -> Version.rh   version header, generated by xyo-version from version.json
    Application.rc / .rh / .manifest    Windows executable resources (icon, version info, manifest)
    Application.ico                     icon
test/
    test.01.cpp                         end to end test: fake client and fake upstream proxy around proxy-forward
fabricare.json                          proxy-forward (exe) and test.01 (test)
version.json                            the program version
```

## AI assistant skill

A Claude Code skill describing how to use this program lives in
[`.claude/skills/proxy-forward/`](../.claude/skills/proxy-forward/SKILL.md).
It is picked up automatically inside this repository; copy the folder to
`~/.claude/skills/` to have it available in other projects.
