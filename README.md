# Proxy Forward

Forward proxy with authentication

# Purpose

Google Chrome does't support http proxy authentication, this will allow that.\
It will act as local TCP/IP redirect and it will provide required headers for proxy authorization.

# Usage

```
proxy-forward	--proxy-server=... --proxy-port=... --proxy-username=... --proxy-password=... --local-port=...
proxy-forward	--close --local-port=...
```

## License

Copyright (c) 2023-2024 Grigore Stefan
Licensed under the [MIT](LICENSE) license.
