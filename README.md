# C Web Servers

HTTP web servers written from scratch in C using the **Windows Sockets (Winsock2)** API. Two server architectures are implemented — a multi-threaded server and a thread-pooled server — along with a shared routing module that handles static file serving with content-type detection.

## Architectures

### Multi-Threaded Server

[`MultiThreadedServer.c`](MultiThreadedServer.c) — Spawns a new thread for every incoming connection using `CreateThread`. Each thread independently receives the HTTP request, routes it, and closes the socket. Simple but unbounded in thread count.

### Thread-Pooled Server

[`ThreadPooledServer.c`](ThreadPooledServer.c) — Pre-allocates a fixed pool of **32 worker threads** and dispatches connections through a circular queue protected by a `CRITICAL_SECTION` mutex and a counting `Semaphore`. The main thread only accepts connections and enqueues them; workers wake on semaphore signal, grab a socket, and handle the request. This design caps concurrency and avoids the overhead of repeatedly creating/destroying threads.

## Routing & Static File Serving

[`route.c`](route.c) / [`route.h`](route.h) — Shared routing module used by both servers:

- Extracts the URL path from the HTTP `GET` request
- Normalises directory paths (e.g. `/` → `/index.html`, `/blog/` → `/blog/index.html`)
- Detects content type by file extension (HTML, CSS, JS, PNG, JPEG, ICO, WebP, SVG, Web Manifest)
- Reads the file into a heap buffer with `fseek`/`ftell` for exact `Content-Length`
- Responds with proper HTTP/1.1 status codes (`200`, `404`, `405`, `415`, `500`)

## Building

A build script is included for compilation with GCC (MSYS2 UCRT64):

```bat
run.bat
```

This compiles `ThreadPooledServer.c` and `route.c`, links against `-lws2_32`, and launches the server on **port 8080**.

To compile manually:

```bash
gcc ThreadPooledServer.c route.c -o ThreadPooledServer.exe -lws2_32
gcc MultiThreadedServer.c route.c -o MultiThreadedServer.exe -lws2_32
```

## Tech Stack

- **Language:** C (C99)
- **Networking:** Winsock2 (`ws2tcpip.h`)
- **Concurrency:** Win32 Threads, Critical Sections, Semaphores
- **Compiler:** GCC (MSYS2 UCRT64)
- **Platform:** Windows
