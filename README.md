# Brazier Framework

Brazier is a modern C++ MVC framework designed for creating high-performance asynchronous APIs.
The framework combines the power of Boost.Asio with modern C++20/23 features.

## Features

- **Full asynchrony** based on Boost.Asio coroutines
- **MVC architecture** with clear separation of concerns
- **Built-in database migration** system
- **WebSockets** technology support
- **HTTPS / TLS** support with SNI, ALPN-ready, mTLS
- **High performance** with minimal overhead

## Requirements

### **Required components:**

- **PostgreSQL** 12+ (for database operations)
- **CMake** 3.14+ (build system)
- **vcpkg** (C++ dependency manager)
- **C++ compiler** with C++20 support (GCC 10+, Clang 10+, MSVC 2019 16.8+)
- **config.json** with environment configuration
- **Brazier libraries** (pre-built)

## Project structure

```text
your_project/
├── config.json                  # Environment configuration
├── CMakeLists.txt               # Build file
├── vcpkg.json                   # vcpkg dependencies
├── your_project/
│   └── Proj.cpp           # Main application file
└── include/                    # Header files
```

## Configuration files

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 4.2)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PROJECT_NAME "brazier_docs")
project(${PROJECT_NAME} VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

if(MSVC)
    add_compile_options(/utf-8)
endif()
find_package(brazier CONFIG REQUIRED COMPONENTS core orm logger)                 

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/include/*.hpp"
)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp ${APP_SOURCES})        

add_executable("init_orm" utils/init_orm.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    brazier::brazier
)

target_link_libraries("init_orm" PRIVATE 
    brazier::brazier
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
```

### vcpkg.json

```json
{
  "name": "your_project",
  "version": "1.0.0",
  "dependencies": [
    "brazier"
  ]
}
```

Add all the other packages you need to the end of the list

## Brazier configuration

### Overview

`ConfigManager` is a flexible configuration management system in brazier that provides:
- Loading configuration from JSON files
- Working with nested parameters using dot notation
- Automatic saving of changes
- Support for different filesystem drivers
- Type-safe value retrieval with default values

### Configuration File Structure

#### Complete Structure with Comments

```json
{
  "server": {
    "host": "0.0.0.0",
    "port": 3501,
    "keep-alive-timeout": 60
  },
  "https_server": {
    "host": "0.0.0.0",
    "port": 8443,
    "tls": {
      "cert_file": "app/certs/server.crt",
      "key_file": "app/certs/server.key",
      "ca_file": "",
      "require_client_cert": false,
      "verify_client_cert": false,
      "handshake_timeout": 15,
      "conf": []
    }
  },
  "database": {
    "host": "127.0.0.1",
    "port": 5432,
    "username": "username",
    "password": "passowrd",
    "database": "database"
  },
  "redis": {
    "host": "127.0.0.1",
    "port": 6379
  },
  "nosql": {
    "host": "127.0.0.1",
    "port": 6379
  },
  "filesystem": {
    "drivers": {
      "local": {
        "root": "./storage"
      },
      "root": {
        "root": "./"
      },
      "default": "local"
    }
  }
}
```

To initialize the global configuration manager, you need to run `ConfigManager::initGlobal()` at the beginning of program execution.

```cpp
#include <brazier/vendor/ConfigManager.hpp>

int main() {
    ConfigManager::initGlobal();
    
    std::string host = ConfigManager::getGlobal().get("server.host", "127.0.0.1");
    int port = ConfigManager::getGlobal().get("server.port", 3501);
    
    brazier::Server server(host, port);
    server.initialize();
    server.run();
    
    return 0;
}
```

You can immediately convert data to the required types and work with nested config parameters.
```cpp
std::string host = config.getNested<std::string>("server.host", "localhost");
int port = config.getNested<int>("server.port", 8080);
bool debug = config.getNested<bool>("app.debug", false);
json database = config.getNested<json>("database", json::object());
```

#### Saving and reloading
```cpp
auto& config = *global_config;

config.save();               // Manual save
config.reload();             // Reload from file
config.setAutoSave(true);    // Auto-save on changes
config.setAutoSave(false);   // Manual save only
config.clear();              // Clear all values
```
## Brazier Logging System

Brazier provides a high-performance logging system with colored output, log rotation, and signal handling.
Logging is done both to file and console with color coding.


### Key Features

- **Colored logging** to console
- **Automatic log rotation**
- **Thread safety** using mutexes
- **High performance** with time caching
- **Signal handling** for graceful shutdown
- **File logging** with buffering

| Level     | Color      | Description              |
|-----------|------------|--------------------------|
| ERROR     |  Red       | Critical errors          |
| WARNING   |  Yellow    | Warnings                 |
| INFO      |  Blue      | Informational messages   |
| SUCCESS   |  Green     | Successful operations    |
| HELP      |  Dark blue | Support messages         |
| CMD       |  Purple    | Commands to use          |
| DEBUG     |            | Debug information        |

### Usage examples

```cpp
Logger::log("Database connection established", "SUCCESS");

std::string username = "john_doe";
Logger::log("User " + username + " logged in", "INFO");
Logger::log("Debugging username", "DEBUG");

try {
    
} catch (const std::exception& e) {
    Logger::log(std::string("Exception: ") + e.what(), "ERROR");
}
```

### Automatic log rotation

All logs are saved and can be found at your_project_path/Storage/Logs

```text
your_project_path/
└── Storage/
    └── Logs/
        ├── app.log      # Current log file
        └── app_old.log  # Previous log after rotation

```

The system automatically rotates logs when the line limit is reached:

- Current file: `debug.log`
- Old file: `debug_old.log`
- Default limit: 10,000 lines

### Custom log levels

```cpp
Logger::log("Custom business event", "BUSINESS");
Logger::log("Performance metric", "METRIC");
Logger::log("Security audit", "AUDIT");
```

## Log format

### Console output (with colors):

```text
2024-01-15 14:30:25 [INFO] Application started
2024-01-15 14:30:26 [SUCCESS] Database connected
2024-01-15 14:30:27 [ERROR] Connection timeout
```

The brazier logging system is ready for use in production environments and provides all necessary functions for effective application monitoring and debugging.

## Brazier Routing System

Brazier provides a powerful routing system with support for asynchronous handlers, parameterized paths, and CORS.
The system is built on Boost.Beast and Boost.Asio coroutines.

### 1. Router

Central class for managing routes and handling requests.

### 2. Route

Represents a single endpoint with parameters and a handler.

### 3. RouterRegisterer

Simplifies route declaration through macros.

### Route registration

```cpp
auto userController = std::make_shared<UserController>();
    
// Basic routes
R(GET, "/api/users", userController, getUsers);
R(POST, "/api/users", userController, createUser);
    
// Parameterized routes
R(GET, "/api/users/:id", userController, getUserById);
R(PUT, "/api/users/:id", userController, updateUser);
R(DELETE_, "/api/users/:id", userController, deleteUser);
    
// Nested parameters
R(GET, "/api/users/:userId/posts/:postId", userController, getUserPost);
    
// CORS for API endpoints
CORS("/api/users", userController);
CORS("/api/users/:id", userController);
```

### Route types

#### Static routes

```cpp
R(GET, "/api/health", healthController, checkHealth);
R(GET, "/api/version", infoController, getVersion);
```

#### Parameterized routes

```cpp
R(GET, "/api/users/:id", userController, getUserById);
R(GET, "/api/categories/:categoryId/products/:productId", productController, getProduct);
```

### Request handling

#### Accessing parameters

```cpp
boost::asio::awaitable<void> getUserPost(const Request& req, Response& res, const Params& params) {
    auto userId = params.at("userId");
    auto postId = params.at("postId");
    
    auto user = co_await userService.findById(userId);
    auto post = co_await postService.findByUserAndId(userId, postId);
    
    nlohmann::json response = {
        {"user", user},
        {"post", post}
    };
    
    res.result(http::status::ok);
    res.body() = response.dump();
    res.set(http::field::content_type, "application/json");
    co_return;
}
```

#### Working with request body

```cpp
boost::asio::awaitable<void> createUser(const Request& req, Response& res, const Params& params) {
    try {
        auto body = nlohmann::json::parse(req.body());
        
        if (!body.contains("email") || !body.contains("name")) {
            res.result(http::status::bad_request);
            co_return;
        }
        
        auto user = co_await userService.create(body);
        
        res.result(http::status::created);
        res.body() = user.toJson().dump();
        res.set(http::field::content_type, "application/json");
        
    } catch (const std::exception& e) {
        res.result(http::status::bad_request);
    }
    co_return;
}
```

## Brazier HTTPS Server

Brazier ships with a first-class HTTPS server built on **Boost.Beast + Boost.Asio + OpenSSL**.
It is API-compatible with the plain HTTP `Server` class — switching between them, or running
both side by side, is a couple of lines in your `main`.

### Features

- **TLS 1.2 / 1.3** by default, with per-server override via `conf`
- **Session resumption** via RFC 5077 tickets with automatic key rotation
- **Hot-reload** of certificates without restarting the server
- **Certificate from file OR from memory (PEM string)**
- **HSTS** header sent automatically on every response
- **Keep-alive** over TLS with configurable idle timeout
- **Graceful shutdown** with `close_notify` and a bounded drain timeout
- **Handshake timeout** to protect against Slowloris-style attacks
- **mTLS** (mutual TLS / client certificates) with custom CA
- **Multi-io_context** — N worker threads, one per CPU core
- **`SO_REUSEPORT`** on Linux/macOS, round-robin dispatch on Windows
- **`TCP_DEFER_ACCEPT`** on Linux — less wake-ups, more throughput
- **Dynamic limits** — body size, header size, and connection count
  auto-tuned to the host hardware at startup
- **Raw OpenSSL tuning** through `SSL_CONF_cmd` — no code changes for
  cipher / protocol tweaks
- **Same Router / Engine / Middleware** as the HTTP server — routing is transport-agnostic

### Requirements

- **OpenSSL** 3.x (installed via vcpkg — the `openssl` package)
- **Boost** 1.82+ (`boost::asio::cancel_after` is used internally)
- A **PEM-encoded** certificate chain and matching private key

### Configuration

Add an `https_server` section to your `config.json` alongside the existing `server`:

```json
{
  "server": {
    "host": "0.0.0.0",
    "port": 3501,
    "keep-alive-timeout": 60
  },
  "https_server": {
    "host": "0.0.0.0",
    "port": 8443,
    "tls": {
      "cert_file": "app/certs/server.crt",
      "key_file": "app/certs/server.key",
      "ca_file": "",
      "require_client_cert": false,
      "verify_client_cert": false,
      "handshake_timeout": 15,
      "conf": []
    }
  },
  "http": {
    "keep_alive_timeout": 60,
    "max_connections": 50000,
    "max_body_size": 1048576,
    "max_header_size": 8192
  }
}
```

#### TLS section reference

| Key                    | Type      | Default       | Description |
|------------------------|-----------|---------------|-------------|
| `cert_file`            | `string`  | `server.crt`  | Path to PEM certificate chain (leaf + intermediates) |
| `key_file`             | `string`  | `server.key`  | Path to PEM private key |
| `cert_pem`             | `string`  | `""`          | Certificate as an in-memory PEM string (overrides `cert_file`) |
| `key_pem`              | `string`  | `""`          | Private key as an in-memory PEM string (overrides `key_file`) |
| `ca_file`              | `string`  | `""`          | Path to a CA bundle — required for mTLS |
| `require_client_cert`  | `bool`    | `false`       | Reject connections without a client certificate |
| `verify_client_cert`   | `bool`    | `false`       | Verify client certificate if presented (but don't require) |
| `handshake_timeout`    | `int`     | `15`          | Seconds to wait for TLS handshake before closing |
| `conf`                 | `array`   | `[]`          | Raw `SSL_CONF_cmd` commands — see below |

> **Paths are resolved relative to the process's current working
> directory**, not to the `config.json` file. Either run the binary from
> the project root, or use absolute paths. On Windows, always use forward
> slashes (`"C:/certs/server.crt"`) — backslashes are escape characters in JSON.

#### `conf` array — raw OpenSSL commands

The `conf` array passes commands directly to `SSL_CONF_cmd`. Each entry is
a two-element array `["command", "value"]`, or a one-element array for
commands without arguments.

```json
"conf": [
  ["CipherString", "ECDHE+AESGCM:ECDHE+CHACHA20"],
  ["Options", "-SessionTicket"],
  ["MinProtocol", "TLSv1.3"]
]
```

These are passed verbatim to OpenSSL. See the OpenSSL documentation for
`SSL_CONF_cmd` for the full list. Brazier does not validate them — if
OpenSSL rejects a command, `initialize()` fails with a clear error.

#### HTTP section reference

Global HTTP limits shared by both `Server` and `HttpsServer`.

| Key                       | Type  | Default       | Description |
|---------------------------|-------|---------------|-------------|
| `keep_alive_timeout`      | `int` | `60`          | Seconds to keep an idle connection open (legacy `keep-alive-timeout` at top level is also accepted) |
| `max_connections`         | `int` | `ulimit × 0.8` | Max simultaneous connections; if unset, derived from `RLIMIT_NOFILE` |
| `max_body_size`           | `int` | auto          | Max request body in bytes; auto-derived from RAM and `max_connections` |
| `max_header_size`         | `int` | auto          | Max total request header size; auto-derived from RAM and `max_connections` |
| `max_connections_testing` | `int` | `0`           | Test-only override for `max_connections` |
| `max_body_size_testing`   | `int` | `0`           | Test-only override for `max_body_size` |
| `max_header_size_testing` | `int` | `0`           | Test-only override for `max_header_size` |

**Auto-derivation rules:**

- `max_connections` = `RLIMIT_NOFILE × 0.8` (POSIX) or `16384 × 0.8` (Windows)
- `max_body_size` = `(RAM × 25%) / max_connections`, clamped to `[64 KB, 16 MB]`
- `max_header_size` = `(RAM × 1%) / max_connections`, clamped to `[4 KB, 32 KB]`

Any explicit value in the config wins over auto-derivation. The `_testing`
keys take priority over everything else and are intended for the test suite.

### Using the HTTPS server

```cpp
#include <brazier/Core>
#include <brazier/orm.hpp>

int main() {
    try {
        brazier::ConfigManager::initGlobal("config.json");
        brazier::ConfigManager::getGlobal().setAutoSave(false);

        std::string host = brazier::ConfigManager::getGlobal().get("https_server.host", "0.0.0.0");
        int port = brazier::ConfigManager::getGlobal().get("https_server.port", 8443);

        brazier::HttpsServer server(host, port);

        if (!server.initialize()) return 1;
        server.run();   // blocks until stop()

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
```

`HttpsServer` reads everything it needs from `https_server.*` in
`global_config`: host, port, TLS settings, the `conf` array, handshake
timeout, and mTLS flags. You don't pass them explicitly — just make sure
`ConfigManager::initGlobal()` runs first.

### Overriding TLS programmatically

When the certificate path is only known at runtime, or you're running
multiple `HttpsServer` instances with different certificates, pass a
`TlsConfig` directly:

```cpp
brazier::HttpsServer::TlsConfig tls;
tls.cert_file = "/var/lib/myapp/api.crt";
tls.key_file  = "/var/lib/myapp/api.key";
tls.conf = {
    { "min_protocol", "TLSv1.3" }
};
tls.handshake_timeout = std::chrono::seconds(10);

brazier::HttpsServer api("0.0.0.0", 9443, tls);
api.initialize();
api.run();
```

When `TlsConfig` is passed explicitly, `https_server.tls.*` from the JSON
is ignored entirely — the code-level config wins.

### Loading certificates from memory (PEM strings)

If the certificate comes from a secret manager, a mounted Kubernetes secret,
or any source other than a plain file path, put the PEM content directly
into `config.json` using `cert_pem` / `key_pem` instead of `cert_file` /
`key_file`:

```json
"https_server": {
  "tls": {
    "cert_pem": "-----BEGIN CERTIFICATE-----\nMIIF...\n-----END CERTIFICATE-----\n",
    "key_pem":  "-----BEGIN PRIVATE KEY-----\nMIIE...\n-----END PRIVATE KEY-----\n"
  }
}
```

Note the `\n` escapes — PEM is multi-line, and JSON strings must escape
newlines. The `nlohmann::json` parser converts them to real newlines before
OpenSSL sees them.

`cert_pem` may contain the full chain (leaf + intermediates) — Brazier parses
and registers each certificate automatically. `key_pem` must be the matching
private key; the pair is validated with `SSL_CTX_check_private_key` at load
time. If the key does not match the certificate, `initialize()` fails with
`"Certificate/private key mismatch"`.

`cert_pem` takes priority over `cert_file`. If both are set, the in-memory
copy is used. This lets you supply a file path for hot-reload and a cached
PEM for the initial load — but note that `reloadTls()` (no args) needs
`cert_file` / `key_file` to know where to re-read from.

For programmatic sources (Vault API, custom secret fetch, database), skip
the config entirely and pass `TlsConfig` directly:

```cpp
brazier::HttpsServer::TlsConfig tls;
tls.cert_pem = fetchPemFromVault("tls/server.crt");
tls.key_pem  = fetchPemFromVault("tls/server.key");

brazier::HttpsServer server("0.0.0.0", 8443, tls);
server.initialize();
```

When `TlsConfig` is passed explicitly, `https_server.tls.*` from `config.json`
is ignored entirely — the code-level config wins.

### Hot-reload of TLS certificates

Reload certificates without restarting the server or dropping existing
connections:

```cpp
// Re-read from files (cert_file / key_file must be set)
server.reloadTls();

// Or supply a new config explicitly — useful for Vault / K8s / DB sources
brazier::HttpsServer::TlsConfig new_tls = server.getTlsConfig();
new_tls.cert_pem = fetchPemFromVault("tls/server.crt");
new_tls.key_pem  = fetchPemFromVault("tls/server.key");
server.reloadTls(new_tls);
```

**Guarantees:**

- Existing connections continue on the old `SSL_CTX` and finish normally.
- New handshakes use the new `SSL_CTX`.
- If the new config is invalid, the operation fails and the old `SSL_CTX`
  stays active — the server is never left in a broken state.
- Session ticket keys are shared across contexts, so resumption keeps
  working across reloads.

The reload is implemented with `std::shared_ptr<ssl::context>` plus a
`std::shared_mutex`. The cost per connection is one atomic refcount and one
shared-lock acquire — invoked once per connection, not per request.

**Common trigger patterns:**

```cpp
// 1. Console command
std::thread([&server] {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "reload") server.reloadTls();
        if (line == "quit")   server.stop();
    }
}).detach();

// 2. SIGHUP (Linux / macOS)
std::signal(SIGHUP, [](int) {
    g_reload_requested.store(true, std::memory_order_release);
});
// ... in a background thread ...
if (g_reload_requested.exchange(false)) server.reloadTls();

// 3. File watcher
std::thread([&server] {
    fs::file_time_type last{};
    while (server.running()) {
        auto now = fs::last_write_time("app/certs/server.crt");
        if (last != fs::file_time_type{} && now != last) {
            server.reloadTls();
        }
        last = now;
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}).detach();
```

### Session resumption

Brazier uses **stateless session tickets** (RFC 5077) — no per-session state
is kept on the server. This is the only resumption mechanism in TLS 1.3,
and it is the recommended mechanism for TLS 1.2 as well.

Tickets are encrypted with rotating keys:

- **Rotation interval** — a new key every 12 hours.
- **Key lifetime** — old keys are kept for 48 hours, so clients with valid
  tickets can still resume.
- **Per-server isolation** — each `HttpsServer` has its own key store.
  Multiple servers in the same process do not share tickets.

Resumption typically costs **0.3–0.8 ms** versus **2–3 ms** for a full
handshake — roughly a 10× CPU reduction on resumed sessions. This matters
most for connection-churn workloads.

### Threading model

On startup, `HttpsServer` creates **N io_context instances, one per CPU core**:

| Platform      | io_contexts            | Acceptors          | Dispatch |
|---------------|------------------------|--------------------|----------|
| Linux / macOS | `hardware_concurrency()` | same as io_contexts | `SO_REUSEPORT` — kernel hashes 4-tuples |
| Windows       | `hardware_concurrency()` | 1                  | round-robin `co_spawn` from a single acceptor |

Each io_context owns its own thread, and each thread owns its own IOCP
(Windows) or epoll instance (Linux). There is no cross-thread signalling on
the hot path — completions for a connection always run on the thread that
owns its io_context.

The startup log tells you exactly what happened:

```
[SUCCESS] HTTPS server initialized on 0.0.0.0:8443 [TLS, io_contexts=12, acceptors=1, dispatch=round-robin, SO_REUSEPORT=no, TCP_DEFER_ACCEPT=no]
[INFO] Starting 12 io_context(s) over 12 thread(s), dispatch=round-robin
```

### Mutual TLS (mTLS)

mTLS requires each client to present a certificate signed by a CA you trust.
This is common for service-to-service communication, IoT devices, and
internal APIs.

Enable it in the config:

```json
"tls": {
  "cert_file": "app/certs/server.crt",
  "key_file":  "app/certs/server.key",
  "ca_file":   "app/certs/ca.crt",
  "require_client_cert": true
}
```

With `require_client_cert: true`, the server sends a `CertificateRequest`
during the handshake. Clients that cannot present a valid certificate are
rejected **before** any HTTP request is processed — the TLS handshake
simply fails.

Set `verify_client_cert: true` (without `require_client_cert`) to ask for a
client certificate but accept clients that don't present one. Useful when
client certs are optional.

> **`ca_file` is required when `require_client_cert: true`.** Without a CA
> bundle the server has no way to validate client certificates.

### Response headers set automatically

For every response, `HttpsServer` sets:

| Header                     | Value                          | Why |
|----------------------------|--------------------------------|-----|
| `Server`                   | `brazier` (configurable)       | Identifies the framework |
| `Strict-Transport-Security`| `max-age=31536000` (configurable) | HSTS — tells browsers to use HTTPS for a year |
| `Connection`               | `keep-alive` or `close`        | Matches the request |

Configure via `http.server_name`, `http.hsts_enabled`, and `http.hsts_header`.
You don't need to set these in your controllers.

### Lifecycle and timeouts

| Phase                    | Timeout | Config key |
|--------------------------|---------|------------|
| TLS handshake            | 15 s    | `https_server.tls.handshake_timeout` |
| Idle keep-alive          | 60 s    | `http.keep_alive_timeout` (or legacy `keep-alive-timeout`) |
| Graceful TLS shutdown    | 5 s     | — |
| Graceful server shutdown | 10 s    | — |

If any of these fire, the connection is closed cleanly — you'll see a
corresponding `[DEBUG] HTTPS client disconnected` line in the log, not an
error.

### Shutting down

```cpp
server.stop();   // returns after all in-flight connections complete (up to 10 s)
```

`stop()` performs a graceful shutdown:

1. Acceptors are closed — no new connections.
2. The work guard is released — `io_context::run()` exits when idle.
3. Waits up to **10 seconds** for the active connection count to reach zero.
4. `io_context::stop()` — force-cancels anything still running.
5. Worker threads are joined.

If `run()` is executing on a different thread, join that thread **after**
`stop()` returns. Do **not** call `stop()` from inside a request handler —
it will deadlock waiting for the calling connection to close.

### Verifying a running server

```bash
// Basic request (accepts self-signed certificates)
curl -vk https://localhost:8443/

// Strict verification against your own CA
curl -v --cacert app/certs/server.crt https://localhost:8443/

// Inspect the handshake
openssl s_client -connect localhost:8443 -servername localhost

// Verify that TLS 1.1 is rejected
openssl s_client -connect localhost:8443 -tls1_1
```

> **Windows `curl.exe` uses Schannel, which does not support ECDSA server
> certificates.** If your cert is ECDSA P-256, use curl from `Git for Windows`
> (which links against OpenSSL), or stick with `openssl s_client`. `ab`
> (ApacheBench) uses its own OpenSSL and works with both cert types.

### Common pitfalls

- **`use_certificate_chain_file: cannot find the file`** — the path in
  `cert_file` is relative to the process's *current working directory*, not
  the config file. Run from the project root or use absolute paths.

- **`Certificate/private key mismatch`** — the cert and key in `cert_file` /
  `key_file` don't belong to the same pair. Brazier rejects this at startup
  with a clear error instead of failing later at handshake time.

- **`SSL_CONF_cmd failed for 'min_protocol=...'`** — some OpenSSL builds
  (notably via vcpkg) don't register `min_protocol` in `SSL_CONF_cmd`.
  Use TLS options in code instead, or check the OpenSSL documentation for
  the correct command name in your build.

- **`no shared cipher` / `alert 40` on handshake** — the client and server
  could not agree on a cipher suite. The usual cause is a mismatch between
  the certificate key type and the client's cipher list: an ECDSA certificate
  cannot satisfy an RSA-only client, and vice versa.

- **Browser says "certificate not trusted"** — expected for self-signed
  certificates. Either click through the warning, add the cert to the user
  root store, or use a certificate from a public CA.

- **`WSAECONNRESET` / `WSAECONNABORTED` in logs** — these are normal client
  disconnect events, not errors. Brazier logs them at `DEBUG` level.

- **`TLS shutdown error` on every connection** — this is
  `APPLICATION_DATA_AFTER_CLOSE_NOTIFY`, a benign artefact of clients that
  send data after the shutdown alert. Logged at `DEBUG` and safe to ignore.

- **Hot-reload says `reloadTls: source is PEM, nothing to reload`** — you
  supplied the certificate only as `cert_pem` / `key_pem`, with no
  `cert_file` / `key_file`. Set the file paths as well, or call
  `reloadTls(new_tls)` with fresh PEM strings from your secret source.

## Brazier WebSocket Routing System

Brazier provides a complete WebSocket routing system with support for parameterized paths, multiple message types, and global handlers. The system integrates seamlessly with the existing HTTP router.

### WebSocket Router

WebSocketRouter is a singleton class that manages all WebSocket routes and their handlers.

| Method	                          | Description                          |
|-------------------------------------|--------------------------------------|
| ``instance()	 ``                     | Returns singleton instance           |
| ``add_route(path, handler)``	          | Registers a new WebSocket route      |
| ``find_route(target, params)``	      | Finds route matching the target path |
| ``set_global_message_handler(handler)`` |	Sets global text message handler     |
| ``set_global_binary_handler(handler)``  |	Sets global binary message handler   |
| ``set_global_close_handler(handler)``	  | Sets global close handler            |
| ``set_global_error_handler(handler)``	  | Sets global error handler            |

### WebSocket Route Types

#### 1. Connect Route (WS_R)

Handles new WebSocket connection establishment.

**Handler Signature:**

```cpp
void handler(std::shared_ptr<WebSocketSession> session, const Params& params)
```

**Use Case:** Initialize connection, send welcome message, set up session-specific data.

```cpp
class ChatController {
public:
    void onConnect(std::shared_ptr<WebSocketSession> session, const Params& params) {
        std::string room = params.at("room");
        session->send("Welcome to room: " + room);
    }
};

WS_R("/chat/:room", ChatController, onConnect);
```

#### 2. Text Message Route (WS_MSG)

Handles incoming text messages.

**Handler Signature:**

```cpp
void handler(const std::string& message, std::shared_ptr<WebSocketSession> session)
```

**Use Case:** Process chat messages, commands, JSON data.

```cpp
class ChatController {
public:
    void onMessage(const std::string& message, std::shared_ptr<WebSocketSession> session) {
        if (message == "ping") {
            session->send("pong");
        } else {
            session->send("Echo: " + message);
        }
    }
};


WS_MSG("/chat/:room", ChatController, onMessage);
```

#### 3. Binary Message Route (WS_BIN)

Handles incoming binary messages.

**Handler Signature:**

```cpp
void handler(std::vector<uint8_t>&& data, std::shared_ptr<WebSocketSession> session)
```

**Use Case:** File uploads, image transfers, binary protocols.

```cpp
class FileController {
public:
    void onBinary(std::vector<uint8_t>&& data, std::shared_ptr<WebSocketSession> session) {
        std::ofstream file("upload_" + std::to_string(time(nullptr)) + ".bin", std::ios::binary);
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        session->send("File received: " + std::to_string(data.size()) + " bytes");
    }
};

WS_BIN("/upload", FileController, onBinary);
```

#### 4. Close Route (WS_CLOSE)

Handles connection closure events.

**Handler Signature:**

```cpp
void handler(std::shared_ptr<WebSocketSession> session)
```

**Use Case:** Cleanup resources, update user lists, log disconnections.

```cpp
class ChatController {
public:
    void onClose(std::shared_ptr<WebSocketSession> session) {
        active_users_.erase(session->get_id());
        WebSocketManager::instance().broadcast("User " + std::string(session->get_id()) + " left");
    }
};

WS_CLOSE("/chat/:room", ChatController, onClose);
```

#### 5. Error Route (WS_ERR)

Handles WebSocket errors.

**Handler Signature:**

```cpp
void handler(const std::string& error, std::shared_ptr<WebSocketSession> session)
```

**Use Case:** Logging, error recovery, client notification.

```cpp
class ChatController {
public:
    void onError(const std::string& error, std::shared_ptr<WebSocketSession> session) {
        Logger::log("WebSocket error: " + error, "ERROR");
        session->send("Error occurred: " + error);
    }
};

WS_ERR("/chat/:room", ChatController, onError);
```
### WebSocket Macros

|Macro	| Description |
|-------|-------------|
|``WS_R(path, controller, handler)``	|Register WebSocket connect handler|
|``WS_MSG(path, controller, handler)``	|Register text message handler|
|``WS_BIN(path, controller, handler)``	|Register binary message handler|
|``WS_CLOSE(path, controller, handler)`` |	Register close handler|
|``WS_ERR(path, controller, handler)``	|Register error handler|

### Global Handlers

Global handlers are called for every WebSocket connection, regardless of the route path.

#### Global Handler Macros

|Macro	|Description|
|-------|-----------|
|``WS_G_MSG(controller, handler)``|	Global text message handler|
|``WS_G_BIN(controller, handler) ``|	Global binary message handler|
|``WS_G_CLOSE(controller, handler)``|	Global close handler|
|``WS_G_ERR(controller, handler)``|Global error handler|

#### Global Handler Example

```cpp
class GlobalController {
public:
    void onGlobalMessage(const std::string& message, std::shared_ptr<WebSocketSession> session) {
        // Log every message
        Logger::log("Message from " + std::string(session->get_id()) + ": " + message);
        
        // Common commands available on any path
        if (message == "help") {
            session->send("Available commands: stats, time, ping");
        } else if (message == "stats") {
            session->send("Active sessions: " + 
                std::to_string(WebSocketManager::instance().get_session_count()));
        }
    }
    
    void onGlobalClose(std::shared_ptr<WebSocketSession> session) {
        Logger::log("Session closed: " + std::string(session->get_id()));
    }
};

// Register global handlers (called once)
WS_G_MSG(GlobalController, onGlobalMessage);
WS_G_CLOSE(GlobalController, onGlobalClose);
```

### Handler Priority

Route-specific handlers have priority over global handlers

Global handlers are called when no route-specific handler exists

Global close and error handlers are always called in addition to route-specific ones

```text
Client connects
    ↓
Find matching route
    ↓
┌─────────────────────────────────────┐
│ If route has message handler:       │
│   → Use route handler               │
│ Else:                               │
│   → Use global message handler      │
└─────────────────────────────────────┘
    ↓
Close and Error: route + global both called
```

## Controllers in brazier Framework

Controllers in brazier are central components that handle HTTP requests and return responses.
They implement application business logic, work with data models, and form HTTP responses.

### Base structure

```cpp
#include <brazier/Core>
#include <brazier/orm.hpp>
#include <brazier/Http>

class UserController : public brazier::Controller {
public:
    using Request = http::request<http::string_body>;
    using Response = http::response<http::string_body>;
    using Params = std::unordered_map<std::string, std::string>;

    // Override REST methods
    boost::asio::awaitable<void> show(const Request& req, Response& res, const Params& params) override;
    boost::asio::awaitable<void> store(const Request& req, Response& res, const Params& params) override;
    boost::asio::awaitable<void> update(const Request& req, Response& res, const Params& params) override;
    boost::asio::awaitable<void> delete_(const Request& req, Response& res, const Params& params) override;
    
    // Custom methods
    boost::asio::awaitable<void> login(const Request& req, Response& res, const Params& params);
    boost::asio::awaitable<void> logout(const Request& req, Response& res, const Params& params);
    
    // CORS handler
    void setCors(const Request& req, Response& res) override;
};
```

### Base Controller Methods

The base Controller class provides default implementations that return 405 Method Not Allowed.

You can override the REST methods.

|Method	|HTTP Method	|Default Behavior	|Typical Use|
|-------|------------|------------------|-----------|
|``show(req, res, params)``	|GET	|405 Not Allowed|	Get single resource by ID
|``store(req, res, params)``	|POST	|405 Not Allowed|	Create new resource
|``update(req, res, params)``	|PUT	|405 Not Allowed|	Update existing resource
|``delete_(req, res, params)``|	DELETE|	405 Not Allowed|	Delete resource

#### Overriding REST methods:

```cpp
class UserController : public brazier::Controller {
public:
    // GET /users/:id - Get single user
    boost::asio::awaitable<void> show(const Request& req, Response& res, const Params& params) override {
        auto userId = std::stoi(params.at("id"));
        auto user = co_await User::find(userId);
        
        if (!user) {
            res.result(http::status::not_found);
            co_return;
        }
        
        res.result(http::status::ok);
        res.body() = user.toJson().dump();
        res.set(http::field::content_type, "application/json");
        co_return;
    }
    
    // POST /users - Create new user
    boost::asio::awaitable<void> store(const Request& req, Response& res, const Params& params) override {
        auto body = nlohmann::json::parse(req.body());
        
        auto user = co_await User::create(body);
        
        res.result(http::status::created);
        res.body() = user.toJson().dump();
        res.set(http::field::content_type, "application/json");
        co_return;
    }
    
    // PUT /users/:id - Update user
    boost::asio::awaitable<void> update(const Request& req, Response& res, const Params& params) override {
        auto userId = std::stoi(params.at("id"));
        auto body = nlohmann::json::parse(req.body());
        
        auto user = co_await User::update(userId, body);
        
        res.result(http::status::ok);
        res.body() = user.toJson().dump();
        co_return;
    }
    
    // DELETE /users/:id - Delete user
    boost::asio::awaitable<void> delete_(const Request& req, Response& res, const Params& params) override {
        auto userId = std::stoi(params.at("id"));
        
        co_await User::remove(userId);
        
        res.result(http::status::no_content);
        co_return;
    }
};
```

### Custom Controller Methods

You can add any number of custom methods beyond the base REST methods:

```cpp
class UserController : public brazier::Controller {
public:
    // Custom method - not in base Controller
    boost::asio::awaitable<void> login(const Request& req, Response& res, const Params& params) {
        auto body = nlohmann::json::parse(req.body());
        std::string email = body["email"];
        std::string password = body["password"];
        
        auto user = co_await User::authenticate(email, password);
        
        if (!user) {
            res.result(http::status::unauthorized);
            co_return;
        }
        
        std::string token = generateJWT(user);
        
        nlohmann::json response;
        response["token"] = token;
        response["user"] = user.toJson();
        
        res.result(http::status::ok);
        res.body() = response.dump();
        res.set(http::field::content_type, "application/json");
        co_return;
    }
    
    boost::asio::awaitable<void> logout(const Request& req, Response& res, const Params& params) {
        // Clear session, invalidate token
        res.result(http::status::ok);
        co_return;
    }
    
    boost::asio::awaitable<void> profile(const Request& req, Response& res, const Params& params) {
        // Get current user profile
        auto userId = getCurrentUserId(req);
        auto user = co_await User::find(userId);
        
        res.result(http::status::ok);
        res.body() = user.toJson().dump();
        co_return;
    }
};
```

### CORS Handlers

#### Default CORS implementation

The base Controller class provides default CORS handlers:

```cpp
void Controller::setCors(const Request& req, Response& res) {
    res.set(http::field::access_control_allow_origin, "*");
    res.set(http::field::access_control_allow_credentials, "true");
    res.set(http::field::access_control_allow_methods, "GET, POST, PUT, DELETE, OPTIONS");
    res.set(http::field::access_control_allow_headers, "*");
    res.set(http::field::access_control_expose_headers, "Set-Cookie");
}

void Controller::setCorsHeaders(const Request& req, Response& res) {
    this->setCors(req, res);
}
```
#### Overriding CORS for specific controllers

```cpp
class ApiController : public brazier::Controller {
public:
    void setCors(const Request& req, Response& res) override {
        // Restrict to specific origin
        res.set(http::field::access_control_allow_origin, "https://myapp.com");
        res.set(http::field::access_control_allow_credentials, "true");
        res.set(http::field::access_control_allow_methods, "GET, POST, PUT, DELETE");
        res.set(http::field::access_control_allow_headers, "Content-Type, Authorization, X-API-Key");
        res.set(http::field::access_control_expose_headers, "X-Total-Count");
    }
};

class PublicController : public brazier::Controller {
public:
    void setCors(const Request& req, Response& res) override {
        // Allow any origin (public API)
        std::string origin = req.at(http::field::origin);
        res.set(http::field::access_control_allow_origin, origin);
        res.set(http::field::access_control_allow_methods, "GET, OPTIONS");
    }
};
```

### Working with Requests

#### Parsing request body

```cpp
boost::asio::awaitable<void> createUser(const Request& req, Response& res, const Params& params) {
    try {
        // Parse JSON body
        auto body = nlohmann::json::parse(req.body());
        
        // Validate required fields
        if (!body.contains("name") || !body.contains("email")) {
            res.result(http::status::bad_request);
            res.body() = R"({"error":"Missing required fields"})";
            co_return;
        }
        
        std::string name = body["name"];
        std::string email = body["email"];
        
        // Create user
        auto user = co_await User::create(name, email);
        
        res.result(http::status::created);
        res.body() = user.toJson().dump();
        res.set(http::field::content_type, "application/json");
        
    } catch (const nlohmann::json::parse_error& e) {
        res.result(http::status::bad_request);
        res.body() = R"({"error":"Invalid JSON"})";
    } catch (const std::exception& e) {
        res.result(http::status::internal_server_error);
        res.body() = R"({"error":"Internal server error"})";
    }
    
    co_return;
}
```

#### Working with URL parameters

```cpp
boost::asio::awaitable<void> getUser(const Request& req, Response& res, const Params& params) {
    // Get parameters from URL (e.g., /users/:id)
    auto userId = params.at("id");
    
    // Optional parameters with default
    std::string fields = params.count("fields") ? params.at("fields") : "all";
    
    auto user = co_await User::find(std::stoi(userId));
    
    if (!user) {
        res.result(http::status::not_found);
        res.body() = R"({"error":"User not found"})";
        co_return;
    }
    
    nlohmann::json response;
    if (fields == "all") {
        response = user.toJson();
    } else {
        response = user.toJson(fields);
    }
    
    res.result(http::status::ok);
    res.body() = response.dump();
    res.set(http::field::content_type, "application/json");
    co_return;
}
```

#### Working with query parameters

```cpp
boost::asio::awaitable<void> getUsers(const Request& req, Response& res, const Params& params) {
    // Parse query string
    auto target = std::string(req.target());
    auto query_pos = target.find('?');
    
    std::unordered_map<std::string, std::string> query;
    if (query_pos != std::string::npos) {
        std::string query_string = target.substr(query_pos + 1);
        // Parse key=value&key2=value2
        // ...
    }
    
    int page = std::stoi(query.count("page") ? query["page"] : "1");
    int limit = std::stoi(query.count("limit") ? query["limit"] : "20");
    
    auto users = co_await User::paginate(page, limit);
    
    nlohmann::json response;
    response["data"] = users;
    response["page"] = page;
    response["limit"] = limit;
    
    res.result(http::status::ok);
    res.body() = response.dump();
    res.set(http::field::content_type, "application/json");
    co_return;
}
```

#### Accessing headers

```cpp
boost::asio::awaitable<void> authEndpoint(const Request& req, Response& res, const Params& params) {
    // Get authorization header
    auto auth = req.find(http::field::authorization);
    if (auth == req.end()) {
        res.result(http::status::unauthorized);
        co_return;
    }
    
    std::string token = auth->value().to_string();
    
    // Remove "Bearer " prefix if present
    if (token.rfind("Bearer ", 0) == 0) {
        token = token.substr(7);
    }
    
    auto user = co_await validateToken(token);
    
    if (!user) {
        res.result(http::status::unauthorized);
        co_return;
    }
    
    // Continue with request
    co_return;
}
```

#### Working with Responses

```cpp
// 200 OK with JSON
res.result(http::status::ok);
res.body() = json.dump();
res.set(http::field::content_type, "application/json");
res.prepare_payload();

// 201 Created
res.result(http::status::created);
res.body() = createdResource.toJson().dump();
res.set(http::field::location, "/api/users/" + std::to_string(id));
res.prepare_payload();

// 204 No Content (for DELETE)
res.result(http::status::no_content);
res.body() = "";
res.prepare_payload();

// 400 Bad Request
res.result(http::status::bad_request);
res.body() = R"({"error":"Invalid input"})";
res.prepare_payload();

// 401 Unauthorized
res.result(http::status::unauthorized);
res.set(http::field::www_authenticate, "Bearer");
res.body() = R"({"error":"Authentication required"})";
res.prepare_payload();

// 403 Forbidden
res.result(http::status::forbidden);
res.body() = R"({"error":"Access denied"})";
res.prepare_payload();

// 404 Not Found
res.result(http::status::not_found);
res.body() = R"({"error":"Resource not found"})";
res.prepare_payload();

// 405 Method Not Allowed
res.result(http::status::method_not_allowed);
res.set(http::field::allow, "GET, POST");
res.body() = R"({"error":"Method not allowed"})";
res.prepare_payload();

// 500 Internal Server Error
res.result(http::status::internal_server_error);
res.body() = R"({"error":"Internal server error"})";
res.prepare_payload();
```

#### Setting custom headers

```cpp
res.set(http::field::content_type, "application/json");
res.set(http::field::cache_control, "no-cache");
res.set(http::field::expires, "0");
res.set(http::field::x_custom_header, "custom_value");
res.set("X-RateLimit-Limit", "100");
res.set("X-RateLimit-Remaining", "95");
```

## Helpers in brazier Framework

Brazier provides a comprehensive set of utility classes (helpers) for common tasks:
- working with cookies; 
- code generation;
- data validation; 
- HTTP requests;
- email sending; 
- other helper functions.

### Code Class - Code Generation

**Purpose**

Generate random codes for various purposes: verification codes, one-time passwords, authentication tokens, backup codes, and API keys.

|Method	|Description	|Return Type|
|-------|-------------|-----------|
|``generateRandomCode(length)``	|Generate random alphanumeric code	|``std::string``
|``generateNumericCode(length)``	|Generate numeric-only code	|``std::string``
|``generateMultipleCodes(count, length, numeric)``	|Generate multiple codes at once |``std::vector<std::string>``
|``base64_encode(bytes)``	|Base64 encode string	|``std::string``

#### Usage Examples

Single code generation

```cpp
#include <brazier/App/Http/Helpers/Code.hpp>

// Alphanumeric code (letters + digits)
std::string verificationCode = Code::generateRandomCode(16);
// Example output: "Ab3kL9mN2pXy8zQ1w"

// Numeric code (only digits)
std::string smsCode = Code::generateNumericCode(6);
// Example output: "384290"

// Email verification token
std::string emailToken = Code::generateRandomCode(32);
Multiple codes generation
cpp
// Generate 10 backup codes (alphanumeric, 8 chars each)
auto backupCodes = Code::generateMultipleCodes(10, 8);
// Example output: ["Ab3kL9mN", "Xy8zQ1wV", "r4F7tY2p", ...]

// Generate 5 numeric OTP codes (4 digits each)
auto otpCodes = Code::generateMultipleCodes(5, 4, true);
// Example output: ["3842", "9017", "5623", "7481", "2395"]

// Generate API keys
auto apiKeys = Code::generateMultipleCodes(100, 32);
Base64 encoding
cpp
std::string original = "Hello World";
std::string encoded = Code::base64_encode(original);
// Output: "SGVsbG8gV29ybGQ="

// For SMTP authentication
std::string authString = '\0' + username + '\0' + password;
std::string encodedAuth = Code::base64_encode(authString);
Real-world examples
cpp
// Generate verification code for email
std::string generateEmailVerificationCode() {
    return Code::generateRandomCode(32);
}

// Generate 2FA backup codes
std::vector<std::string> generate2FABackupCodes() {
    return Code::generateMultipleCodes(10, 8);
}

// Generate session token
std::string generateSessionToken() {
    return Code::generateRandomCode(64);
}

// Generate numeric OTP
std::string generateOTP() {
    return Code::generateNumericCode(6);
}
```

### Cookie Class - HTTP Cookies

**Purpose**

Simplified HTTP cookie handling: parsing cookies from requests and setting cookies in responses.

|Method	|Description	|Return Type|
|-------|---------------|-----------|
|``parseCookies(cookieHeader)``|	Parse cookie header string into key-value map	|``std::map<std::string, std::string>``
|``set(response, cookies)``	|Set multiple cookies in HTTP response|	``void``

#### Usage Examples

**Parsing cookies from request**

```cpp
#include <brazier/App/Http/Helpers/Cookie.hpp>

boost::asio::awaitable<void> show(const Request& req, Response& res, const Params& params) {
    // Get cookie header from request
    auto cookieHeader = req.find(http::field::cookie);
    
    if (cookieHeader != req.end()) {
        auto cookies = Cookie::parseCookies(std::string(cookieHeader->value()));
        
        std::string sessionId = cookies["session_id"];
        std::string userId = cookies["user_id"];
        std::string theme = cookies["theme"];
        
        // Use cookie values
        if (cookies.find("auth_token") != cookies.end()) {
            // User is authenticated
        }
    }
    
    co_return;
}
```

**Setting cookies in response**

```cpp
boost::asio::awaitable<void> login(const Request& req, Response& res, const Params& params) {
    // Single cookie
    std::map<std::string, std::string> cookies;
    cookies["session_id"] = Code::generateRandomCode(32);
    Cookie::set(res, cookies);
    
    // Multiple cookies
    std::map<std::string, std::string> authCookies = {
        {"auth_token", "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9"},
        {"user_id", "12345"},
        {"session_id", Code::generateRandomCode(32)},
        {"refresh_token", Code::generateRandomCode(64)}
    };
    Cookie::set(res, authCookies);
    
    res.result(http::status::ok);
    co_return;
}
```

**Complete authentication example**
```cpp
class AuthController : public brazier::Controller {
public:
    boost::asio::awaitable<void> login(const Request& req, Response& res, const Params& params) {
        auto body = nlohmann::json::parse(req.body());
        std::string email = body["email"];
        std::string password = body["password"];
        
        auto user = co_await authenticate(email, password);
        
        if (!user) {
            res.result(http::status::unauthorized);
            co_return;
        }
        
        // Set authentication cookies
        std::map<std::string, std::string> cookies = {
            {"access_token", generateAccessToken(user)},
            {"refresh_token", generateRefreshToken(user)},
            {"user_id", std::to_string(user.id)},
            {"session_id", Code::generateRandomCode(32)}
        };
        Cookie::set(res, cookies);
        
        res.result(http::status::ok);
        co_return;
    }
    
    boost::asio::awaitable<void> logout(const Request& req, Response& res, const Params& params) {
        // Clear cookies by setting expired values
        std::map<std::string, std::string> expiredCookies = {
            {"access_token", ""},
            {"refresh_token", ""},
            {"user_id", ""},
            {"session_id", ""}
        };
        Cookie::set(res, expiredCookies);
        
        res.result(http::status::ok);
        co_return;
    }
};
```

### Validator Class - Data Validation

**Purpose**

Validate user input data: passwords, email addresses, and other common data types.

|Method	|Description	|Return Type|
|-------|-------------|------------|
|``password(password)``|	Validate password| ``bool``
|``email(email)`` |	Validate email format (RFC 5322)	| ``bool``
|``passwordError(password)`` | Get detailed password error message|	``std::string``

**Password Requirements:**
- Length: 8-20 characters
- At least one digit (0-9)
- At least one uppercase letter (A-Z)
- At least one special character (!@#$%^&* etc.)

#### Usage Examples

**Basic validation**

```cpp
#include <brazier/App/Http/Helpers/Validator.hpp>

// Password validation
if (Validator::password("MySecure123!")) {
    // Password is valid
} else {
    std::string error = Validator::passwordError("MySecure123!");
}

// Email validation
if (Validator::email("user@example.com")) {
    // Email is valid
} else {
    // Invalid email format
}
```

**Complete registration example**

```cpp
boost::asio::awaitable<void> register(const Request& req, Response& res, const Params& params) {
    auto body = nlohmann::json::parse(req.body());
    
    std::string email = body["email"];
    std::string password = body["password"];
    
    // Validate email
    if (!Validator::email(email)) {
        res.result(http::status::bad_request);
        res.body() = R"({"error":"Invalid email format"})";
        co_return;
    }
    
    // Validate password
    if (!Validator::password(password)) {
        std::string error = Validator::passwordError(password);
        nlohmann::json response;
        response["error"] = error;
        res.body() = response.dump();
        res.result(http::status::bad_request);
        co_return;
    }
    
    // Create user
    auto user = co_await User::create(email, password);
    
    res.result(http::status::created);
    co_return;
}
```

**Password validation with detailed errors**

```cpp
std::string validatePasswordWithDetails(const std::string& password) {
    if (password.size() < 8) {
        return "Password must be at least 8 characters long";
    }
    if (password.size() > 20) {
        return "Password must not exceed 20 characters";
    }
    
    if (Validator::password(password)) {
        return ""; // Valid
    }
    
    return Validator::passwordError(password);
}

// Usage
std::string error = validatePasswordWithDetails("weak");
if (!error.empty()) {
    // Show error to user
}
```

## HttpClient Class — Asynchronous HTTP Requests
  
  The `HttpClient` class provides asynchronous methods for performing HTTP/HTTPS requests to external APIs.
  It is built on **Boost.Beast** and **Boost.Asio** with C++20 coroutine support (`awaitable`). All network
  operations are non‑blocking, making it suitable for concurrent environments.
  
### Public Methods
  
  | Method                    | Description                                                                 | Return Type              |
  |---------------------------|-----------------------------------------------------------------------------|--------------------------|
  | `get(url, body = json{})` | GET request; `body` is converted to query string                            | `net::awaitable<Response>` |
  | `post(url, body)`         | POST request with JSON body                                                 | `net::awaitable<Response>` |
  | `put(url, body)`          | PUT request to update a resource                                            | `net::awaitable<Response>` |
  | `del(url, body = json{})` | DELETE request; `body` is optional                                          | `net::awaitable<Response>` |
  | `set_timeout(timeout)`    | Sets timeout (milliseconds) for all operations                              | `void`                   |
  | `set_verify_ssl(verify)`  | Enable/disable SSL certificate verification (on by default)                 | `void`                   |
  | `is_success(response)`    | Checks if the response status is 2xx                                        | `bool`                   |
  
  ---
  
  ### Usage
  
  Include the header:

  ```cpp
  #include <brazier/Http>
  ```
  
  All request calls must be performed inside a coroutine launched via `net::co_spawn` or similar.
  
  #### Basic GET Request

  ```cpp
  #include <brazier/Http>
  #include <boost/asio/co_spawn.hpp>
  #include <boost/asio/use_awaitable.hpp>
  
  using namespace brazier;
  
  net::awaitable<void> fetch_users() {
      HttpClient client;
      nlohmann::json params = {
          {"page", 1},
          {"limit", 10}
      };
  
      Response response = co_await client.get("https://api.example.com/users", params);
  
      if (client.is_success(response)) {
          auto users = nlohmann::json::parse(response.body());
          // process users
      }
  }
  
  int main() {
      net::io_context ioc;
      net::co_spawn(ioc, fetch_users(), net::detached);
      ioc.run();
  }
  ```
  
  #### POST Request with JSON Body

  ```cpp
  net::awaitable<void> create_user() {
      HttpClient client;
  
      nlohmann::json userData = {
          {"name", "John Doe"},
          {"email", "john@example.com"},
          {"age", 30}
      };
  
      Response response = co_await client.post("https://api.example.com/users", userData);
  
      if (response.result() == http::status::created) {
          auto createdUser = nlohmann::json::parse(response.body());
          std::cout << "User created with ID: " << createdUser["id"] << std::endl;
      }
  }
  ```
  
  #### PUT Request (Update)

  ```cpp
  net::awaitable<void> update_user() {
      HttpClient client;
  
      nlohmann::json updateData = {
          {"name", "Jane Doe"},
          {"age", 31}
      };
  
      Response response = co_await client.put("https://api.example.com/users/123", updateData);
  
      if (client.is_success(response)) {
          // update successful
      }
  }
  ```
  
  #### DELETE Request

  ```cpp
  net::awaitable<void> delete_user() {
      HttpClient client;
      nlohmann::json emptyBody; // or just {}
  
      Response response = co_await client.del("https://api.example.com/users/123", emptyBody);
  
      if (response.result() == http::status::no_content) {
          // user deleted
      }
  }
  ```
  
  #### Setting Timeout

  ```cpp
  net::awaitable<void> fetch_with_timeout() {
      HttpClient client;
      client.set_timeout(std::chrono::milliseconds(5000)); // 5 seconds
  
      try {
          Response response = co_await client.get("https://slow-api.example.com/data");
          // process response
      } catch (const std::exception& e) {
          std::cerr << "Error: " << e.what() << std::endl;
      }
  }
  ```
  
  #### Disabling SSL Verification (Testing Only)

  ```cpp
  net::awaitable<void> test_self_signed() {
      HttpClient client;
      client.set_verify_ssl(false);
  
      Response response = co_await client.get("https://self-signed.badssl.com");
      // ...
  }
  ```
  
  ### Important Notes
  
  - **Coroutines**: all request methods must be called from within a coroutine launched with `net::co_spawn` and the appropriate executor.
  - **Exceptions**: errors (invalid URL, timeout, DNS, SSL) throw exceptions. Always handle them in your code.
  - **Thread safety**: `HttpClient` objects are not thread‑safe; use separate instances per thread or synchronise access.
  - **Headers**: the library automatically adds `Host`, `User‑Agent`, `Accept`, and `Connection: close`. For POST/PUT/DELETE it also sets `Content‑Type: application/json`.
  - **Timeouts**: apply to each phase (connect, write, read). A timeout throws an exception.
  
  ---

### SmtpClient Class - Email Sending

**Purpose**

Send emails using SMTP protocol with SSL/TLS encryption support.

|Method|	Description	|Return Type
|-|-|-
``send(recipient, message, sender_name)``|	Send email to recipient	| ``net::awaitable<int>``

**Configuration**

The SmtpClient uses environment variables for configuration:

|Variable	|Description
|-|-
``SMTP_HOST``	|SMTP server hostname
``SMTP_PORT``	|SMTP server port
``SMTP_USERNAME``	|Authentication username
``SMTP_PASSWORD``	|Authentication password
``S_HOST``	|Local hostname for EHLO

#### Usage Examples

**Basic email sending**

```cpp
#include <brazier/App/Http/Helpers/SmtpClient.hpp>

boost::asio::awaitable<void> sendEmail(const Request& req, Response& res, const Params& params) {
    SmtpClient smtp;
    
    std::string recipient = "user@example.com";
    std::string message = "<h1>Welcome!</h1><p>Thank you for registering.</p>";
    std::string senderName = "My App Support";
    
    int result = co_await smtp.send(recipient, message, senderName);
    
    if (result == 0) {
        res.result(http::status::ok);
        res.body() = "Email sent successfully";
    } else {
        res.result(http::status::internal_server_error);
        res.body() = "Failed to send email";
    }
    
    co_return;
}
```

**HTML email with formatting**

```cpp
std::string createHtmlEmail(const std::string& username, const std::string& verificationCode) {
    std::string html = R"(
        <!DOCTYPE html>
        <html>
        <head>
            <style>
                body { font-family: Arial, sans-serif; }
                .container { padding: 20px; background-color: #f4f4f4; }
                .content { background-color: white; padding: 20px; border-radius: 5px; }
                .code { font-size: 24px; font-weight: bold; color: #0066cc; }
            </style>
        </head>
        <body>
            <div class="container">
                <div class="content">
                    <h2>Welcome, )" + username + R"(!</h2>
                    <p>Please verify your email address using the code below:</p>
                    <p class="code">)" + verificationCode + R"(</p>
                    <p>This code expires in 24 hours.</p>
                </div>
            </div>
        </body>
        </html>
    )";
    
    return html;
}

boost::asio::awaitable<void> sendVerificationEmail(const std::string& email, const std::string& username) {
    SmtpClient smtp;
    std::string verificationCode = Code::generateRandomCode(32);
    std::string htmlMessage = createHtmlEmail(username, verificationCode);
    
    int result = co_await smtp.send(email, htmlMessage, "Verification Service");
    
    if (result == 0) {
        // Save verification code to database
        co_await saveVerificationCode(email, verificationCode);
    }
    
    co_return;
}
```

**Complete registration with email**

```cpp
class AuthController : public brazier::Controller {
public:
    boost::asio::awaitable<void> register(const Request& req, Response& res, const Params& params) {
        auto body = nlohmann::json::parse(req.body());
        
        std::string email = body["email"];
        std::string username = body["username"];
        std::string password = body["password"];
        
        // Validate input
        if (!Validator::email(email)) {
            res.result(http::status::bad_request);
            res.body() = R"({"error":"Invalid email"})";
            co_return;
        }
        
        if (!Validator::password(password)) {
            nlohmann::json response;
            response["error"] = Validator::passwordError(password);
            res.body() = response.dump();
            res.result(http::status::bad_request);
            co_return;
        }
        
        // Create user
        auto user = co_await User::create(email, username, password);
        
        // Send welcome email
        SmtpClient smtp;
        std::string welcomeMessage = createWelcomeEmail(username);
        
        co_await smtp.send(email, welcomeMessage, "MyApp Team");
        
        // Send verification email
        std::string verificationCode = Code::generateRandomCode(32);
        std::string verifyMessage = createVerificationEmail(username, verificationCode);
        
        co_await smtp.send(email, verifyMessage, "Verification Service");
        
        // Save verification code
        co_await saveVerificationCode(email, verificationCode);
        
        res.result(http::status::created);
        nlohmann::json response;
        response["message"] = "User registered. Please check your email for verification.";
        res.body() = response.dump();
        co_return;
    }
    
private:
    std::string createWelcomeEmail(const std::string& username) {
        return R"(
            <h1>Welcome, )" + username + R"(!</h1>
            <p>Thank you for joining MyApp!</p>
            <p>We're excited to have you on board.</p>
        )";
    }
    
    std::string createVerificationEmail(const std::string& username, const std::string& code) {
        return R"(
            <h1>Verify Your Email</h1>
            <p>Hello )" + username + R"(,</p>
            <p>Please click the link below to verify your email address:</p>
            <p><a href="https://myapp.com/verify?code=)" + code + R"(">Verify Email</a></p>
            <p>This link expires in 24 hours.</p>
        )";
    }
};
```