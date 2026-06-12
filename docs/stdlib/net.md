# std::net

TCP and UDP networking primitives.

## Import

```
use std::net::{ tcpConnect, tcpSend, tcpRecv, close, resolve };
```

## Functions

### TCP

| Function | Signature | Description |
|----------|-----------|-------------|
| `tcpConnect` | `(string, int32) -> int32` | Connect to host:port, returns socket fd |
| `tcpListen` | `(string, int32) -> int32` | Bind and listen on address:port |
| `tcpAccept` | `(int32) -> int32` | Accept incoming connection |
| `tcpSend` | `(int32, string) -> int32` | Send string data on socket |
| `tcpRecv` | `(int32, int32) -> string` | Receive up to N bytes from socket |

### UDP

| Function | Signature | Description |
|----------|-----------|-------------|
| `udpBind` | `(string, int32) -> int32` | Bind UDP socket to address:port |
| `udpSendTo` | `(int32, string, string, int32) -> int32` | Send data to host:port |
| `udpRecvFrom` | `(int32, int32) -> string` | Receive up to N bytes |

### Common

| Function | Signature | Description |
|----------|-----------|-------------|
| `close` | `(int32) -> void` | Close a socket |
| `setTimeout` | `(int32, int32) -> void` | Set socket timeout in milliseconds |
| `resolve` | `(string) -> string` | Resolve hostname to IP address |

## Example

### TCP Client

```
use std::net::{ tcpConnect, tcpSend, tcpRecv, close };
use std::log::println;

int32 sock = tcpConnect("example.com", 80);
tcpSend(sock, "GET / HTTP/1.0\r\nHost: example.com\r\n\r\n");
string response = tcpRecv(sock, 4096);
println(response);
close(sock);
```

### TCP Server

```
use std::net::{ tcpListen, tcpAccept, tcpSend, tcpRecv, close };
use std::log::println;

int32 server = tcpListen("0.0.0.0", 8080);
int32 client = tcpAccept(server);

string msg = tcpRecv(client, 1024);
println(msg);

tcpSend(client, "HTTP/1.0 200 OK\r\n\r\nHello!");
close(client);
close(server);
```

## See Also

- [std::process](process.md) — Process control
- [std::os](os.md) — OS primitives
