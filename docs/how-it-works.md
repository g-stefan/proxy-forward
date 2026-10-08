# How it works

```
client ──► 127.0.0.1:<local-port> ──► proxy-forward ──► <proxy-server>:<proxy-port> ──► internet
                                      adds Proxy-Authorization
client ◄──────────────────────────── proxy-forward ◄── responses, unchanged
```

`proxy-forward` reads the **requests** of the client just enough to find where
each request header ends, and adds the credentials there. It never reads or
changes the **responses**: they are copied back to the client as they arrive.

## One client connection

Each accepted client connection gets a free [connection slot](#connection-slots)
and two threads: one copies client → upstream (and edits the request headers),
the other copies upstream → client.

For each request on the connection:

1. **Request line.** Empty lines before it are skipped (not forwarded).
2. **Header lines**, up to the empty line. A line may end with `\r\n` or `\n`;
   lines are forwarded as received, in the same order, except:
   - every `Proxy-Authorization` line (name in any case) is **removed**, the
     client can not send other credentials;
   - `Content-Length` is read (decimal digits, spaces around are allowed; if
     it appears more than once, all values must be the same);
   - `Transfer-Encoding` containing `chunked` marks a chunked body;
   - an `Upgrade` line, or a request line starting with `CONNECT `, makes the
     connection a [tunnel](#tunnels-connect-and-upgrade) after this request.
3. **Credentials.** Before the empty line that ends the header, this line is
   inserted:

   ```
   Proxy-Authorization: Basic <base64(username:password)>
   ```

   For `--proxy-username=user --proxy-password=p=a:ss`:
   `Proxy-Authorization: Basic dXNlcjpwPWE6c3M=`.
4. **Upstream connection.** For the first request of the client connection,
   `proxy-forward` resolves `--proxy-server` and connects to it. The same
   upstream connection is used for all the following requests of this client
   connection (keep-alive); one client connection = one upstream connection.
5. The header and the start of the body are sent together.
6. **Body**, forwarded unchanged:
   - `Content-Length: N`: exactly `N` bytes;
   - chunked: every chunk size line, chunk data, chunk end, and the trailer
     lines up to the final empty line (the format is checked, the bytes are not
     changed);
   - neither: no body.
7. Back to 1 for the next request. Pipelined requests (sent before the
   previous response arrived) work: the parser only follows the request
   framing.

Data in a request body is never inspected: a body that contains text like
`Proxy-Authorization:` or a whole request is forwarded as it is.

### Tunnels: CONNECT and Upgrade

After a `CONNECT host:port` request (HTTPS sites, any TLS connection through
the proxy), or a request with an `Upgrade` header (WebSocket, ...), the
credentials are added to that request header only. From then on everything is
copied unchanged in both directions until one side closes: the TLS data of an
HTTPS site is never touched, the encryption is end to end between the client
and the site.

### Responses

Responses (status, headers, body) are passed back unchanged. In particular:

- `407 Proxy Authentication Required` from the upstream proxy reaches the
  client: the username or the password is wrong (or the account is locked).
- When the upstream proxy closes the connection, the client connection is
  closed after all the data was sent. When the client closes its connection,
  the upstream connection is closed.

## Errors

| Situation | What the client gets |
|-----------|----------------------|
| The upstream proxy can not be resolved or connected (first request of a connection) | the response below, then the connection is closed |
| Request header (request line and header lines) larger than 64 KiB | the connection is closed, nothing is forwarded |
| `Content-Length` not a number, or two different values | the connection is closed |
| Malformed chunked body (bad chunk size line, missing chunk end, line over 64 KiB) | the connection is closed |
| The client closes in the middle of a header | the connection is closed |

```
HTTP/1.1 502 Bad Gateway
Content-Type: text/plain
Content-Length: 43
Connection: close

Proxy Forward: unable to connect to proxy
```

The connection to the upstream proxy uses the system connect timeout (about
20 seconds on Windows when the host does not answer).

## Timeouts

- **Idle connection**: while waiting for the next request (or for the rest of
  a request header), if no data moves in either direction for **5 minutes**,
  the connection is closed. A long response being received counts as
  activity.
- **No timeout** while a request body is sent or inside a tunnel: they last
  until one side closes the connection, or the instance is closed.
- A client that stops reading a response holds its connection until it reads
  again, closes, or 5 minutes pass without any data moving.

## Connection slots

`--thread-count=N` (default `64`, `1` to `1024`) is the number of client
connections served at the same time. The slots are allocated when the
instance starts, each with about 160 KiB of buffers (two 32 KiB transfer
buffers, a 64 KiB request header buffer and room for the start of a body):
about 10 MiB for 64 slots, about 160 MiB for 1024.

- A busy slot uses two threads; a free slot uses none.
- When every slot is busy, new connections wait in the listen queue (256
  connections) until a slot is free (checked at least every 0.25 seconds).
  They are not refused, they are delayed.
- Browsers keep many connections open, also idle keep-alive connections,
  which hold a slot until the browser closes them or the 5 minute idle
  timeout. If pages hang when many tabs load at once, raise `--thread-count`.

## Stopping

`--close` (or any `WM_CLOSE` sent to the hidden window) stops the instance:
the server thread stops accepting, every connection thread sees the stop
within about 0.1 seconds (all socket waits are at most 100 ms) and closes its
sockets, also when a client stopped reading, then the process ends. A
connection that is just connecting to the upstream proxy ends when the
connect attempt ends (at most the system connect timeout). `--close` waits for
the end of the process, at most 30 seconds.

## Security

- **Local users can use your account.** `proxy-forward` listens only on
  `127.0.0.1`, other computers can not connect, but it asks nothing from
  local clients: every program of every user logged on this computer (for
  example on a shared terminal server) can use the upstream proxy with your
  credentials. Do not run it on a shared machine with an account that must
  stay personal.
- **Basic authentication is not encryption.** The username and the password
  are sent, base64 encoded, in every request to the upstream proxy, over a
  plain TCP connection, as with any HTTP proxy Basic authentication. HTTPS
  sites are still encrypted end to end (the credentials are outside the TLS
  tunnel, in the `CONNECT` request).
- **The password is in the command line** of the process unless an
  [options file](command-line.md#options-file) is used.
- Nothing is logged, responses are not stored.

## Limits

- Windows only (`xyo-win`), listening on IPv4 loopback only.
- Upstream: plain HTTP proxy with Basic authentication. No TLS to the proxy,
  no NTLM, Digest, Negotiate or Kerberos, no SOCKS.
- HTTP/1.x request framing. A `Transfer-Encoding` other than `chunked` is not
  understood (browsers do not send one in requests).
- A request with `Upgrade` turns the connection into a tunnel even if the
  server declines the upgrade; later requests on that connection would not
  get the credentials. Browsers send WebSocket connections through a proxy
  with `CONNECT`, so they are not affected.
- A client that ends its sending side after the request (a half close, for
  example `nc -N`) does not get the response: the end of the client data closes
  the whole connection.
