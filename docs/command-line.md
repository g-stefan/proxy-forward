# Command line

```
proxy-forward --proxy-server=... --proxy-port=... --proxy-username=... --proxy-password=... --local-port=... [--thread-count=N]
proxy-forward --close --local-port=...
proxy-forward @file
proxy-forward --help | --usage | --license | --version
```

`proxy-forward` is a Windows GUI program: it has no console, everything it
has to say (usage, version, errors) is shown in a **message box**, which waits
for the user to press OK.

## Options

| Option | Meaning |
|--------|---------|
| `--proxy-server=server` | upstream HTTP proxy: host name, IPv4 address, or IPv6 address in brackets (`[2001:db8::1]`). Required to start |
| `--proxy-port=port` | upstream proxy port. Required to start |
| `--proxy-username=username` | proxy account. Required to start, must not be empty |
| `--proxy-password=password` | proxy password. Required to start, must not be empty |
| `--local-port=port` | local port, `proxy-forward` listens on `127.0.0.1:port`. Required, also for `--close` |
| `--thread-count=number` | number of connections served at the same time, `1` to `1024`, default `64`. See [How it works](how-it-works.md#connection-slots) |
| `--close` | close the instance running on `--local-port`, wait for it to end |
| `@file` | read more options from `file`, see [Options file](#options-file) |
| `--help`, `--usage` | show the usage |
| `--license` | show the license |
| `--version` | show the version, build number and build date |

Rules:

- Options are `--name=value`. The value is everything after the **first**
  `=`, so a password may contain `=` and `:` (`--proxy-password=p=a:ss`).
  Quote the whole option if the value has spaces:
  `"--proxy-password=my secret"`.
- An option given twice: the last one wins.
- Unknown options (`--something`) and arguments that do not start with `--`
  are **ignored**, without a message. A misspelled option shows up as a
  missing one: `--proxy-sever=...` gives `Error: proxy-server is empty`.
- `--help`, `--usage`, `--license` and `--version` are acted on when they are
  reached, left to right, and end the program; with no argument at all the
  usage is shown.
- `--local-port` is matched as text to find a running instance: start and
  close with the same spelling (`8080`, not `08080`).
- No option is case insensitive: `--Local-Port` is unknown.

## Options file

An argument that starts with `@` is a file name; the file content is split
into arguments like a command line, and they are used in place of `@file`:

- spaces, tabs and line ends separate arguments, so one option per line
  works;
- double quotes group: `"--proxy-password=my secret"`;
- `\"` is a literal double quote;
- a file named inside the file (`@other`) is **not** read, it is ignored.

The file is read as bytes: save it as ANSI or as UTF-8 **without a BOM** (a
BOM in front of the first option makes it unknown, so it is ignored). Use
ASCII in the username and the password.

`@file` can be mixed with other options; the order counts (the last value
wins):

```bat
proxy-forward @%USERPROFILE%\proxy-forward.txt --thread-count=256
```

A missing file is an error: `Error: file not found - <file>`, exit code `1`.

Putting the password in a file keeps it out of the process command line,
which other programs of the same user can read (Task Manager, `wmic`,
PowerShell `Get-CimInstance Win32_Process`). Restrict the file to your
account, for example in `%USERPROFILE%`.

## Single instance

Each instance creates a hidden window, class `Proxy Forward`, title
`Proxy#<local-port>`. When the program starts:

1. `--local-port` is required (`Error: local-port is empty` otherwise).
2. If a window `Proxy#<local-port>` exists:
   - with `--close`: it is closed (see below);
   - without `--close`: the program **ends right away with exit code `0`**,
     the running instance keeps its options. To change the options, `--close`
     first.
3. If no instance runs and `--close` is given: nothing to do, exit code `0`.
4. Otherwise the proxy options are checked, the port is opened and the
   instance starts.

So the start command is safe to run several times (at logon, from a
script): only the first one starts an instance.

## --close

```bat
proxy-forward --close --local-port=8080
```

Sends `WM_CLOSE` to the instance window, then **waits for that process to
end**, at most 30 seconds. The instance stops accepting connections, closes
all open connections (each connection thread sees the stop within about
0.1 seconds) and ends. When `--close` returns, the local port is free and a
new instance can be started right away.

Other options on the same command line are not needed and are ignored.

## Exit codes

| Code | When |
|------|------|
| `0` | the instance ran and was closed; or it was already running on that port; or `--close` closed it (or nothing was running); or `--help`, `--usage`, `--license`, `--version` |
| `1` | an error (a message box was shown first), or `--close` waited 30 seconds and the instance did not end (no message box) |

The instance itself runs until it is closed, so a command that starts one
only returns at that time. In a batch file use `start "" proxy-forward ...`
(a batch file waits for GUI programs); from a console, typing the command
returns at once.

## Messages

| Message | Cause |
|---------|-------|
| `Error: local-port is empty` | no `--local-port` |
| `Error: proxy-server is empty`, `proxy-port`, `proxy-username`, `proxy-password` | that option is missing or empty (checked only when starting a new instance) |
| `Error: thread-count must be a number from 1 to 1024` | bad `--thread-count` |
| `Error: file not found - <file>` | `@file` can not be read |
| `Error: network not initialized` | Windows sockets could not be initialized |
| `Error: unable to listen on local port <port>` | the port is used by another program, or it is not a valid port number |
| `Error: unable to start server thread` | out of resources, the instance ends |

The upstream proxy address is not checked at start: it is resolved and
connected for each new client connection. If it can not be reached the
client gets a `502 Bad Gateway` response, see
[How it works](how-it-works.md#errors).
