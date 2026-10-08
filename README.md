# Proxy Forward

Forward proxy with authentication
- `proxy-forward --proxy-server=proxy.example.com --proxy-port=3128 --proxy-username=user --proxy-password=secret --local-port=8080`
runs a local HTTP proxy on `127.0.0.1:8080` that forwards to
`proxy.example.com:3128` and adds `Proxy-Authorization: Basic ...` to every
request.
- Clients (Chrome, curl, git, ...) use `http://127.0.0.1:8080` as their
proxy without credentials.
- `proxy-forward --close --local-port=8080` stops it.

Built on `xyo-networking`, `xyo-system` and `xyo-win`. Windows only.

# Purpose

Google Chrome doesn't support HTTP proxy authentication settings (it can be
given a proxy, but not the proxy username and password), this will allow
that.It will act as local TCP/IP redirect and it will provide required headers for
proxy authorization.

# Usage

```
proxy-forward --proxy-server=... --proxy-port=... --proxy-username=... --proxy-password=... --local-port=... [--thread-count=...]
proxy-forward --close --local-port=...
proxy-forward @file
```

## Documentation

- [Overview](docs/README.md) - purpose and design
- [Getting started](docs/getting-started.md) - download or build, first run, configure Chrome and other clients, start at logon
- [Command line](docs/command-line.md) - options, options file, single instance, `--close`, exit codes, messages
- [How it works](docs/how-it-works.md) - request handling, tunnels, errors, timeouts, connection slots, security, limits
- [Reference](docs/reference.md) - source layout, `Application` class, constants, fabricare project, tests

A Claude Code skill for this program is in
[.claude/skills/proxy-forward](.claude/skills/proxy-forward/SKILL.md).

## License

Copyright (c) 2023-2026 Grigore Stefan
Licensed under the [MIT](LICENSE) license.
