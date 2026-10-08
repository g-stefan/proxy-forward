# Getting started

## 1. Get the executable

### From a release

Each release has these archives (`release/` after `fabricare release`, or the
GitHub releases page of [g-stefan/proxy-forward](https://github.com/g-stefan/proxy-forward)):

| Archive | Contents | Needs |
|---------|----------|-------|
| `xyo.proxy-forward.vX.Y.Z.win64-msvc-2026.static.bin.zip` | `proxy-forward.exe`, static build | nothing, copy it anywhere |
| `xyo.proxy-forward.vX.Y.Z.win64-msvc-2026.bin.zip` | `proxy-forward.exe` | `xyo-platform.dll`, `xyo-managed-memory.dll`, `xyo-multithreading.dll`, `xyo-encoding.dll`, `xyo-system.dll`, `xyo-networking.dll`, `xyo-win.dll` next to it or on the `PATH` (the XYO SDK), and the Visual C++ runtime |
| `xyo.proxy-forward.vX.Y.Z.sha512.json` | SHA-512 of every archive | |

On a machine without the XYO SDK, use the **static** build.

Use a build after **3.0.0 build 16**: in 3.0.0 build 16 and earlier
`--close` returns before the running instance has ended (a new instance
started right after it can find the old one and exit), and an instance with a
client that stopped reading a response can not be closed.

### Build it

`proxy-forward` is built with [fabricare](https://github.com/g-stefan/fabricare).
`xyo-platform`, `xyo-managed-memory`, `xyo-data-structures`,
`xyo-multithreading`, `xyo-encoding`, `xyo-system`, `xyo-networking` and
`xyo-win` must be installed to the SDK first. From the repository root:

```bash
fabricare make       # build into output/  (output/bin/proxy-forward.exe)
fabricare test       # build and run the end to end test (test/test.01.cpp)
fabricare install    # copy output/bin to ~/.fabricare/<platform>/bin
fabricare clean      # remove output/ and temp/
```

fabricare sets up the compiler environment (MSVC `vcvarsall.bat`) by itself.
`fabricare --static make` builds the static variant, `fabricare release`
packs `output/` into the `release/` archives listed above.

## 2. First run

You need the address and port of the upstream HTTP proxy and an account on
it. Pick a free local port, here `8080`:

```bat
proxy-forward --proxy-server=proxy.example.com --proxy-port=3128 --proxy-username=user --proxy-password=secret --local-port=8080
```

Nothing is shown: the program runs in the background (look for
`proxy-forward.exe` in the Task Manager). If something is wrong, a message box
explains it, for example `Error: unable to listen on local port 8080` when
the port is used by another program.

Check it with curl (no credentials on the curl side):

```bat
curl -x http://127.0.0.1:8080 -I https://example.com/
```

- `HTTP/1.1 200 Connection established` followed by the site response: it
  works.
- `HTTP/1.1 407 Proxy Authentication Required`: the upstream proxy rejected
  the username or the password.
- `HTTP/1.1 502 Bad Gateway` with `Proxy Forward: unable to connect to proxy`:
  `proxy-forward` could not connect to `--proxy-server` / `--proxy-port`.
- `Failed to connect to 127.0.0.1 port 8080`: `proxy-forward` is not
  running on that port.

## 3. Point the client at it

Always use `127.0.0.1`: `proxy-forward` listens only on the IPv4 loopback
address, `localhost` may be tried as `::1` first.

| Client | Setting |
|--------|---------|
| Chrome, Edge, other Chromium browsers | start with `--proxy-server="http://127.0.0.1:8080"` (add `--proxy-bypass-list="<local>;*.intranet.example"` for exceptions), or use the system proxy settings |
| Windows system proxy (used by Chromium browsers and many programs) | Settings, Network & internet, Proxy, Manual proxy setup: address `127.0.0.1`, port `8080` |
| curl | `-x http://127.0.0.1:8080`, or `HTTPS_PROXY=http://127.0.0.1:8080` |
| git | `git config --global http.proxy http://127.0.0.1:8080` |
| programs using environment variables | `HTTP_PROXY=http://127.0.0.1:8080`, `HTTPS_PROXY=http://127.0.0.1:8080` |

The client must not be configured with proxy credentials: if it sends its own
`Proxy-Authorization` header, `proxy-forward` removes it and adds its own.

Example, a Chrome shortcut that uses the proxy:

```bat
"C:\Program Files\Google\Chrome\Application\chrome.exe" --proxy-server="http://127.0.0.1:8080"
```

## 4. Keep the password off the command line

Put the options in a file, readable only by you, for example
`%USERPROFILE%\proxy-forward.txt`:

```
--proxy-server=proxy.example.com
--proxy-port=3128
--proxy-username=user
--proxy-password=secret
--local-port=8080
```

and start it with:

```bat
proxy-forward @%USERPROFILE%\proxy-forward.txt
```

See [Command line](command-line.md#options-file) for the file format.

## 5. Start at logon

`proxy-forward` is a Windows (GUI) program. A batch file waits for it to end,
so start it with `start`:

```bat
@echo off
rem proxy-forward-start.cmd
start "" proxy-forward @%USERPROFILE%\proxy-forward.txt
```

Put a shortcut to this file (or directly to `proxy-forward.exe` with the
arguments) in the Startup folder (`Win+R`, `shell:startup`), or create a Task
Scheduler task "At log on" that runs `proxy-forward.exe` with the arguments.
Starting it when it already runs on that port does nothing, so this is safe
to run more than once.

## 6. Stop it, change the options

```bat
proxy-forward --close --local-port=8080
```

`--close` asks the instance on that port to end and waits until it has ended
(at most 30 seconds), so the port is free when it returns. Open connections
are closed. To change the options (other account, other upstream proxy),
close it and start it again:

```bat
proxy-forward --close --local-port=8080
start "" proxy-forward @%USERPROFILE%\proxy-forward.txt
```

## 7. Several proxies

Each local port is a separate instance, with its own upstream proxy and
account:

```bat
start "" proxy-forward --proxy-server=proxy-a.example.com --proxy-port=3128 --proxy-username=alice --proxy-password=a --local-port=8080
start "" proxy-forward --proxy-server=proxy-b.example.com --proxy-port=8000 --proxy-username=bob --proxy-password=b --local-port=8081
```

## Next

- [Command line](command-line.md): all the options, exit codes, messages.
- [How it works](how-it-works.md): what happens to requests, limits, security.
