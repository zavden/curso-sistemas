# T01 — Builder en C

---

## 1. El problema que resuelve Builder

Cuando un objeto tiene muchos parametros de configuracion, el
constructor se vuelve inmanejable:

```c
/* Constructor con 8 parametros — inusable */
Server *server_new(
    const char *host,
    int port,
    int max_connections,
    int timeout_ms,
    bool use_tls,
    const char *cert_path,
    const char *key_path,
    const char *log_file
);

/* ¿Cual es cual? */
Server *s = server_new("0.0.0.0", 8080, 100, 5000,
                        true, "/etc/cert.pem",
                        "/etc/key.pem", "/var/log/app.log");
/* ¿5000 es el timeout o max_connections? Imposible saber. */
```

Problemas:
- No se sabe que significa cada parametro por posicion
- La mayoria de parametros tienen defaults razonables
- Agregar un parametro rompe todos los call sites
- No hay forma de validar combinaciones (TLS requiere cert + key)

**Builder**: separar la configuracion de la construccion. El caller
setea solo los parametros que necesita, y llama `build()` al final.

---

## 2. El patron en C: config struct

En C, la forma mas natural de Builder es un **config struct**
con designated initializers (C99):

```c
/* Paso 1: definir el config struct con defaults */
typedef struct {
    const char *host;
    int         port;
    int         max_connections;
    int         timeout_ms;
    bool        use_tls;
    const char *cert_path;
    const char *key_path;
    const char *log_file;
} ServerConfig;

/* Paso 2: macro o funcion para defaults */
#define SERVER_CONFIG_DEFAULT {     \
    .host            = "0.0.0.0",  \
    .port            = 8080,       \
    .max_connections = 100,        \
    .timeout_ms      = 30000,      \
    .use_tls         = false,      \
    .cert_path       = NULL,       \
    .key_path        = NULL,       \
    .log_file        = NULL,       \
}

/* Paso 3: build function */
Server *server_build(const ServerConfig *cfg);

/* Uso: solo seteas lo que necesitas */
ServerConfig cfg = SERVER_CONFIG_DEFAULT;
cfg.port = 9090;
cfg.use_tls = true;
cfg.cert_path = "/etc/cert.pem";
cfg.key_path = "/etc/key.pem";

Server *s = server_build(&cfg);
```

```
  Ventajas sobre constructor con N parametros:
  1. Cada campo tiene nombre → auto-documentado
  2. Defaults para todo → solo cambias lo que necesitas
  3. Agregar un campo no rompe call sites existentes
  4. Puedes validar combinaciones en server_build()
```

---

## 3. Designated initializers (C99)

Los designated initializers son la base del builder en C. Campos
no mencionados se inicializan a cero/NULL:

```c
/* Sin designated initializers (C89) — orden importa */
ServerConfig cfg = { "0.0.0.0", 8080, 100, 30000, false, NULL, NULL, NULL };

/* Con designated initializers (C99) — orden no importa */
ServerConfig cfg = {
    .port = 9090,
    .use_tls = true,
    .cert_path = "/etc/cert.pem",
};
/* Campos no mencionados: host=NULL, max_connections=0, etc. */
```

```
  CUIDADO: campos no mencionados se ponen a CERO, no al default.
  Por eso usamos SERVER_CONFIG_DEFAULT primero:

  ServerConfig cfg = SERVER_CONFIG_DEFAULT;  // defaults correctos
  cfg.port = 9090;                           // override solo port

  O en una sola expresion (compound literal, C99):
  Server *s = server_build(&(ServerConfig){
      .host = "0.0.0.0",
      .port = 9090,
      .max_connections = 50,
  });
  // Aqui los demas campos SON cero (no defaults)
  // Usa SERVER_CONFIG_DEFAULT si necesitas defaults reales
```

### 3.1 Default function vs default macro

```c
/* Opcion A: macro (compile time, sin overhead) */
#define SERVER_CONFIG_DEFAULT { .host = "0.0.0.0", .port = 8080, ... }

/* Opcion B: funcion (mas flexible, puede leer env vars) */
ServerConfig server_config_default(void) {
    ServerConfig cfg = {0};
    cfg.host = "0.0.0.0";
    cfg.port = 8080;
    cfg.max_connections = 100;
    cfg.timeout_ms = 30000;

    /* Puede leer del entorno */
    const char *env_port = getenv("SERVER_PORT");
    if (env_port) cfg.port = atoi(env_port);

    return cfg;
}

/* Uso: */
ServerConfig cfg = server_config_default();
cfg.use_tls = true;
Server *s = server_build(&cfg);
```

---

## 4. Implementacion completa: HTTP Server

### 4.1 El config struct

```c
/* server.h */
#ifndef SERVER_H
#define SERVER_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Server Server;

typedef enum {
    LOG_NONE,
    LOG_ERROR,
    LOG_WARN,
    LOG_INFO,
    LOG_DEBUG,
} LogLevel;

typedef struct {
    /* Red */
    const char *host;
    int         port;
    int         backlog;

    /* Conexiones */
    int         max_connections;
    int         timeout_ms;
    size_t      max_request_size;

    /* TLS */
    bool        use_tls;
    const char *cert_path;
    const char *key_path;

    /* Logging */
    LogLevel    log_level;
    const char *log_file;    /* NULL = stderr */

    /* Worker threads */
    int         num_workers; /* 0 = auto (num CPUs) */
} ServerConfig;

#define SERVER_CONFIG_DEFAULT {          \
    .host             = "0.0.0.0",      \
    .port             = 8080,           \
    .backlog          = 128,            \
    .max_connections  = 1024,           \
    .timeout_ms       = 30000,          \
    .max_request_size = 1024 * 1024,    \
    .use_tls          = false,          \
    .cert_path        = NULL,           \
    .key_path         = NULL,           \
    .log_level        = LOG_INFO,       \
    .log_file         = NULL,           \
    .num_workers      = 0,              \
}

/* Construir el server desde la config */
Server *server_build(const ServerConfig *cfg);

/* Destruir */
void server_destroy(Server *s);

/* Operar */
int server_start(Server *s);
void server_stop(Server *s);

#endif /* SERVER_H */
```

### 4.2 La funcion build con validacion

```c
/* server.c */
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Server {
    char  host[256];
    int   port;
    int   backlog;
    int   max_connections;
    int   timeout_ms;
    size_t max_request_size;
    bool  use_tls;
    char *cert_path;
    char *key_path;
    LogLevel log_level;
    char *log_file;
    int   num_workers;
    /* ... internal state ... */
};

static char *strdup_safe(const char *s) {
    return s ? strdup(s) : NULL;
}

Server *server_build(const ServerConfig *cfg) {
    /* ── Validacion ────────────────────────────── */
    if (!cfg->host || cfg->host[0] == '\0') {
        fprintf(stderr, "server: host is required\n");
        return NULL;
    }

    if (cfg->port < 1 || cfg->port > 65535) {
        fprintf(stderr, "server: port must be 1-65535 (got %d)\n",
                cfg->port);
        return NULL;
    }

    if (cfg->use_tls) {
        if (!cfg->cert_path) {
            fprintf(stderr, "server: TLS requires cert_path\n");
            return NULL;
        }
        if (!cfg->key_path) {
            fprintf(stderr, "server: TLS requires key_path\n");
            return NULL;
        }
    }

    if (cfg->max_connections < 1) {
        fprintf(stderr, "server: max_connections must be >= 1\n");
        return NULL;
    }

    if (cfg->timeout_ms < 0) {
        fprintf(stderr, "server: timeout_ms must be >= 0\n");
        return NULL;
    }

    /* ── Construccion ──────────────────────────── */
    Server *s = calloc(1, sizeof *s);
    if (!s) return NULL;

    strncpy(s->host, cfg->host, sizeof(s->host) - 1);
    s->port             = cfg->port;
    s->backlog          = cfg->backlog > 0 ? cfg->backlog : 128;
    s->max_connections  = cfg->max_connections;
    s->timeout_ms       = cfg->timeout_ms;
    s->max_request_size = cfg->max_request_size > 0
                        ? cfg->max_request_size : 1024 * 1024;
    s->use_tls          = cfg->use_tls;
    s->cert_path        = strdup_safe(cfg->cert_path);
    s->key_path         = strdup_safe(cfg->key_path);
    s->log_level        = cfg->log_level;
    s->log_file         = strdup_safe(cfg->log_file);
    s->num_workers      = cfg->num_workers > 0
                        ? cfg->num_workers : 4; /* auto */

    return s;
}

void server_destroy(Server *s) {
    if (!s) return;
    free(s->cert_path);
    free(s->key_path);
    free(s->log_file);
    free(s);
}
```

### 4.3 Usos tipicos

```c
/* Minimal: solo defaults */
ServerConfig cfg = SERVER_CONFIG_DEFAULT;
Server *s = server_build(&cfg);

/* Produccion: TLS + custom settings */
ServerConfig cfg = SERVER_CONFIG_DEFAULT;
cfg.port = 443;
cfg.use_tls = true;
cfg.cert_path = "/etc/letsencrypt/cert.pem";
cfg.key_path = "/etc/letsencrypt/key.pem";
cfg.max_connections = 10000;
cfg.num_workers = 8;
cfg.log_level = LOG_WARN;
cfg.log_file = "/var/log/server.log";
Server *s = server_build(&cfg);

/* Desarrollo: rapido y verboso */
ServerConfig cfg = SERVER_CONFIG_DEFAULT;
cfg.port = 3000;
cfg.log_level = LOG_DEBUG;
Server *s = server_build(&cfg);

/* Test: minimo */
ServerConfig cfg = SERVER_CONFIG_DEFAULT;
cfg.port = 0;  /* el OS asigna un puerto libre */
cfg.log_level = LOG_NONE;
Server *s = server_build(&cfg);
```

---

## 5. Builder con function chaining (alternativa)

Otra forma de Builder en C: funciones que modifican la config y
retornan un puntero a ella (permitiendo chaining):

```c
/* server_builder.h */
typedef struct ServerBuilder ServerBuilder;

ServerBuilder *server_builder_new(void);
ServerBuilder *server_builder_host(ServerBuilder *b, const char *host);
ServerBuilder *server_builder_port(ServerBuilder *b, int port);
ServerBuilder *server_builder_tls(ServerBuilder *b,
                                   const char *cert, const char *key);
ServerBuilder *server_builder_workers(ServerBuilder *b, int n);
ServerBuilder *server_builder_log(ServerBuilder *b,
                                   LogLevel level, const char *file);
Server        *server_builder_build(ServerBuilder *b);
void           server_builder_free(ServerBuilder *b);
```

```c
/* server_builder.c */
struct ServerBuilder {
    ServerConfig cfg;
};

ServerBuilder *server_builder_new(void) {
    ServerBuilder *b = malloc(sizeof *b);
    if (!b) return NULL;
    b->cfg = (ServerConfig)SERVER_CONFIG_DEFAULT;
    return b;
}

ServerBuilder *server_builder_host(ServerBuilder *b, const char *host) {
    b->cfg.host = host;
    return b;  /* retorna b para chaining */
}

ServerBuilder *server_builder_port(ServerBuilder *b, int port) {
    b->cfg.port = port;
    return b;
}

ServerBuilder *server_builder_tls(ServerBuilder *b,
                                   const char *cert, const char *key) {
    b->cfg.use_tls = true;
    b->cfg.cert_path = cert;
    b->cfg.key_path = key;
    return b;
}

ServerBuilder *server_builder_workers(ServerBuilder *b, int n) {
    b->cfg.num_workers = n;
    return b;
}

ServerBuilder *server_builder_log(ServerBuilder *b,
                                   LogLevel level, const char *file) {
    b->cfg.log_level = level;
    b->cfg.log_file = file;
    return b;
}

Server *server_builder_build(ServerBuilder *b) {
    Server *s = server_build(&b->cfg);
    free(b);  /* consume el builder */
    return s;
}

void server_builder_free(ServerBuilder *b) {
    free(b);
}
```

```c
/* Uso con chaining: */
Server *s = server_builder_build(
    server_builder_tls(
        server_builder_port(
            server_builder_host(
                server_builder_new(),
                "0.0.0.0"),
            443),
        "/etc/cert.pem", "/etc/key.pem")
);

/* Legible? No mucho. En C, el chaining es feo por la sintaxis.
   Por eso la config struct es el enfoque preferido en C. */
```

```
  Config struct:                  Builder chaining:
  ──────────────                  ────────────────
  ServerConfig cfg = DEFAULT;     ServerBuilder *b = new();
  cfg.port = 443;                 builder_port(b, 443);
  cfg.use_tls = true;             builder_tls(b, cert, key);
  server_build(&cfg);             builder_build(b);

  Pro: simple, sin malloc          Pro: puede validar incrementalmente
  Pro: legible                     Pro: familiar a quien viene de Java
  Con: no puede validar parcial    Con: malloc + free del builder
  Con: todos los campos publicos   Con: chaining feo en C
```

En C, la config struct es casi siempre preferida. El chaining
brilla en Rust (seccion T02) donde la sintaxis es mas natural.

---

## 6. Validacion incremental vs validacion final

### 6.1 Validacion final (en build)

```c
Server *server_build(const ServerConfig *cfg) {
    /* Validar TODO al final */
    if (cfg->use_tls && !cfg->cert_path) {
        return NULL;  // TLS sin cert
    }
    /* ... construir ... */
}
```

Simple pero: el error se detecta tarde. El caller no sabe que
campo falta hasta que llama `build`.

### 6.2 Validacion incremental (en cada setter)

```c
ServerBuilder *server_builder_tls(ServerBuilder *b,
                                   const char *cert, const char *key) {
    if (!cert || !key) {
        fprintf(stderr, "TLS requires both cert and key\n");
        b->cfg.use_tls = false;  /* marcar como invalido */
        return b;
    }
    b->cfg.use_tls = true;
    b->cfg.cert_path = cert;
    b->cfg.key_path = key;
    return b;
}
```

Detecta errores temprano, pero complica cada setter.

### 6.3 Patron: error flag en el builder

```c
struct ServerBuilder {
    ServerConfig cfg;
    bool has_error;
    char error_msg[256];
};

ServerBuilder *server_builder_port(ServerBuilder *b, int port) {
    if (b->has_error) return b;  /* skip si ya hay error */
    if (port < 1 || port > 65535) {
        b->has_error = true;
        snprintf(b->error_msg, sizeof b->error_msg,
                 "port must be 1-65535, got %d", port);
        return b;
    }
    b->cfg.port = port;
    return b;
}

Server *server_builder_build(ServerBuilder *b) {
    if (b->has_error) {
        fprintf(stderr, "Build error: %s\n", b->error_msg);
        free(b);
        return NULL;
    }
    Server *s = server_build(&b->cfg);
    free(b);
    return s;
}
```

```
  Con error flag:
  1. El primer error se captura
  2. Los setters posteriores son no-ops (skip si error)
  3. build() retorna NULL y reporta el error
  4. Similar a Result en Rust (pero manual)
```

---

## 7. Builder para objetos complejos

### 7.1 HTTP Request builder

```c
typedef enum { HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_DELETE } HttpMethod;

typedef struct {
    char key[64];
    char value[256];
} Header;

typedef struct {
    HttpMethod   method;
    const char  *url;
    Header      *headers;
    int          num_headers;
    int          cap_headers;
    const char  *body;
    size_t       body_len;
    int          timeout_ms;
    bool         follow_redirects;
    int          max_redirects;
} HttpRequestBuilder;

HttpRequestBuilder http_request_new(HttpMethod method, const char *url) {
    HttpRequestBuilder b = {0};
    b.method = method;
    b.url = url;
    b.timeout_ms = 30000;
    b.follow_redirects = true;
    b.max_redirects = 10;
    b.cap_headers = 16;
    b.headers = calloc(b.cap_headers, sizeof(Header));
    b.num_headers = 0;
    return b;
}

HttpRequestBuilder *http_request_header(HttpRequestBuilder *b,
                                         const char *key, const char *val) {
    if (b->num_headers >= b->cap_headers) {
        b->cap_headers *= 2;
        b->headers = realloc(b->headers, b->cap_headers * sizeof(Header));
    }
    strncpy(b->headers[b->num_headers].key, key, 63);
    strncpy(b->headers[b->num_headers].value, val, 255);
    b->num_headers++;
    return b;
}

HttpRequestBuilder *http_request_body(HttpRequestBuilder *b,
                                       const char *body, size_t len) {
    b->body = body;
    b->body_len = len;
    return b;
}

HttpRequestBuilder *http_request_timeout(HttpRequestBuilder *b, int ms) {
    b->timeout_ms = ms;
    return b;
}

/* Build: valida y crea la request final (inmutable) */
typedef struct {
    HttpMethod   method;
    char        *url;
    Header      *headers;
    int          num_headers;
    char        *body;
    size_t       body_len;
    int          timeout_ms;
    bool         follow_redirects;
    int          max_redirects;
} HttpRequest;

HttpRequest *http_request_build(HttpRequestBuilder *b) {
    if (!b->url) return NULL;
    if (b->method == HTTP_POST && !b->body) {
        fprintf(stderr, "POST requires a body\n");
        return NULL;
    }

    HttpRequest *req = malloc(sizeof *req);
    if (!req) return NULL;

    req->method = b->method;
    req->url = strdup(b->url);
    req->headers = b->headers;  /* transfer ownership */
    b->headers = NULL;           /* builder no longer owns */
    req->num_headers = b->num_headers;
    req->body = b->body ? strndup(b->body, b->body_len) : NULL;
    req->body_len = b->body_len;
    req->timeout_ms = b->timeout_ms;
    req->follow_redirects = b->follow_redirects;
    req->max_redirects = b->max_redirects;

    return req;
}

void http_request_builder_free(HttpRequestBuilder *b) {
    free(b->headers);
}

/* Uso: */
HttpRequestBuilder b = http_request_new(HTTP_POST, "https://api.example.com");
http_request_header(&b, "Content-Type", "application/json");
http_request_header(&b, "Authorization", "Bearer token123");
http_request_body(&b, "{\"name\":\"Alice\"}", 16);
http_request_timeout(&b, 5000);

HttpRequest *req = http_request_build(&b);
/* b.headers transferido a req — no llamar builder_free si build OK */
```

### 7.2 Builder vs telescoping constructor

```
  Telescoping constructor (anti-pattern):
  ─────────────────────────────────────
  HttpRequest *req_new(const char *url);
  HttpRequest *req_new2(const char *url, const char *body);
  HttpRequest *req_new3(const char *url, const char *body, int timeout);
  /* ... N! combinaciones posibles */

  Builder:
  ────────
  HttpRequestBuilder b = http_request_new(GET, url);
  http_request_header(&b, ...);
  http_request_body(&b, ...);
  HttpRequest *req = http_request_build(&b);
  /* Una sola forma de construir, parametros opcionales claros */
```

---

## 8. Builder en la practica: ejemplos reales

### 8.1 SQLite (config struct)

```c
/* sqlite3_open_v2 usa flags como builder simplificado */
sqlite3 *db;
int rc = sqlite3_open_v2(
    "app.db",
    &db,
    SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_WAL,
    NULL  /* VFS name */
);
```

### 8.2 libcurl (chaining con setopt)

```c
/* curl usa un builder implicito: setopt + perform */
CURL *curl = curl_easy_init();
curl_easy_setopt(curl, CURLOPT_URL, "https://example.com");
curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
CURLcode res = curl_easy_perform(curl);  /* "build" + execute */
curl_easy_cleanup(curl);
```

```
  curl es un builder disfrazado:
  - curl_easy_init()    = builder_new()
  - curl_easy_setopt()  = builder_set_field()
  - curl_easy_perform() = builder_build() + execute()
  - curl_easy_cleanup() = builder_destroy()

  Diferencia: curl NO separa build de ejecucion.
  El objeto mutable se configura y luego se usa.
```

### 8.3 POSIX socket (multi-step build)

```c
/* Crear un socket TCP es un builder implicito: */
int fd = socket(AF_INET, SOCK_STREAM, 0);  /* "new" */

/* "set" options */
int opt = 1;
setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);

struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

/* "build" = bind + listen */
struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port = htons(8080),
    .sin_addr.s_addr = INADDR_ANY,
};
bind(fd, (struct sockaddr *)&addr, sizeof addr);
listen(fd, 128);
```

---

## 9. Builder con ownership transfer

Un punto delicado en C: quien posee la memoria cuando el builder
"construye" el objeto.

```
  Estrategia 1: Builder copia todo
  ─────────────────────────────────
  build() hace strdup/memcpy de todo.
  El builder y el objeto son independientes.
  Pro: simple, sin sorpresas.
  Con: allocations extras.

  Estrategia 2: Builder transfiere ownership
  ───────────────────────────────────────────
  build() mueve punteros del builder al objeto.
  El builder queda vacio despues de build().
  Pro: sin copias, eficiente.
  Con: el builder es inutilizable despues de build().

  Estrategia 3: Builder presta, objeto copia
  ──────────────────────────────────────────
  El builder tiene const char* (no posee).
  build() hace strdup al construir el objeto.
  Pro: el builder es reutilizable.
  Con: el caller debe mantener los strings vivos durante build.
```

```c
/* Estrategia 3 (la mas comun en C): */
ServerConfig cfg = SERVER_CONFIG_DEFAULT;
cfg.host = "0.0.0.0";     /* cfg.host apunta a string literal (no copia) */
cfg.log_file = argv[1];   /* cfg.log_file apunta al argumento (no copia) */

Server *s = server_build(&cfg);
/* server_build COPIA los strings internamente con strdup */
/* cfg puede destruirse — server tiene sus propias copias */
```

---

## 10. Comparacion con Rust (preview)

```c
/* C: config struct + build function */
ServerConfig cfg = SERVER_CONFIG_DEFAULT;
cfg.port = 443;
cfg.use_tls = true;
Server *s = server_build(&cfg);
if (!s) { /* error — pero cual? */ }
server_destroy(s);
```

```rust
// Rust: method chaining + consume self
let server = ServerBuilder::new()
    .port(443)
    .tls("/cert.pem", "/key.pem")
    .build()?;   // Result<Server, BuildError>
// server se destruye automaticamente (Drop)
```

```
  Diferencia              C                        Rust
  ──────────              ─                        ────
  Defaults                Macro o funcion          Default trait
  Chaining                Feo (nested calls)       Natural (. syntax)
  Validacion              NULL o error code        Result<T, E>
  Ownership               Manual (strdup)          Move semantics
  Uso despues de build    Posible (bug)            Imposible (moved)
  Compile-time safety     Ninguna                  Typestate builder (T03)
```

---

## Errores comunes

### Error 1 — Olvidar los defaults

```c
/* BUG: sin defaults, campos son 0/NULL */
ServerConfig cfg = {0};
cfg.port = 8080;
Server *s = server_build(&cfg);
/* cfg.host = NULL, cfg.max_connections = 0
   → crash o comportamiento inesperado */

/* CORRECTO: siempre inicializar con defaults */
ServerConfig cfg = SERVER_CONFIG_DEFAULT;
cfg.port = 8080;
```

### Error 2 — Config struct con punteros dangling

```c
ServerConfig cfg = SERVER_CONFIG_DEFAULT;
{
    char buf[256];
    snprintf(buf, sizeof buf, "/tmp/log_%d.txt", getpid());
    cfg.log_file = buf;  /* apunta a variable local */
}
/* buf salio del scope — cfg.log_file es dangling */

Server *s = server_build(&cfg);
/* Si server_build hace strdup: funciona (copia antes de que muera) */
/* Si no hace strdup: UB (accede a memoria liberada) */
```

Documentar claramente: "la config solo necesita ser valida
durante la llamada a `server_build()`".

### Error 3 — Builder sin validacion de combinaciones

```c
ServerConfig cfg = SERVER_CONFIG_DEFAULT;
cfg.use_tls = true;
/* Olvido cert_path y key_path */

Server *s = server_build(&cfg);
/* Si build no valida: crea server que crashea al aceptar conexion */
/* Si build valida: retorna NULL con mensaje */
```

Siempre validar combinaciones en `build()`. TLS requiere cert +
key. Workers > 0 si no es auto. Etc.

### Error 4 — Reutilizar builder despues de transfer

```c
HttpRequestBuilder b = http_request_new(GET, url);
http_request_header(&b, "Accept", "text/html");

HttpRequest *req1 = http_request_build(&b);
/* build() transfirio b.headers a req1 — b.headers = NULL */

http_request_header(&b, "Cache-Control", "no-cache");
/* BUG: b.headers es NULL → crash al acceder */

HttpRequest *req2 = http_request_build(&b);
/* BUG: req2 no tiene headers */
```

Si build() transfiere ownership, el builder queda inutilizable.
Documentarlo o hacer que build() copie (no transfiera).

---

## Ejercicios

### Ejercicio 1 — Config struct basico

Escribe un builder con config struct para un `Logger`:

```c
typedef struct {
    LogLevel level;
    const char *file_path;  /* NULL = stderr */
    bool include_timestamp;
    bool include_source_location;
    size_t max_file_size;   /* 0 = sin limite */
} LoggerConfig;
```

Define `LOGGER_CONFIG_DEFAULT` y `logger_build()`.

**Prediccion**: Que defaults son razonables para desarrollo
vs produccion?

<details><summary>Respuesta</summary>

```c
#define LOGGER_CONFIG_DEFAULT {              \
    .level                   = LOG_INFO,     \
    .file_path               = NULL,         \
    .include_timestamp       = true,         \
    .include_source_location = false,        \
    .max_file_size           = 0,            \
}

typedef struct {
    LogLevel level;
    FILE *output;
    bool include_timestamp;
    bool include_source_location;
    size_t max_file_size;
    char *file_path;
} Logger;

Logger *logger_build(const LoggerConfig *cfg) {
    Logger *l = calloc(1, sizeof *l);
    if (!l) return NULL;

    l->level = cfg->level;
    l->include_timestamp = cfg->include_timestamp;
    l->include_source_location = cfg->include_source_location;
    l->max_file_size = cfg->max_file_size;

    if (cfg->file_path) {
        l->output = fopen(cfg->file_path, "a");
        if (!l->output) {
            free(l);
            return NULL;
        }
        l->file_path = strdup(cfg->file_path);
    } else {
        l->output = stderr;
        l->file_path = NULL;
    }

    return l;
}

void logger_destroy(Logger *l) {
    if (!l) return;
    if (l->file_path) {
        fclose(l->output);
        free(l->file_path);
    }
    free(l);
}
```

Defaults razonables:

| Campo | Desarrollo | Produccion |
|-------|-----------|-----------|
| level | LOG_DEBUG | LOG_WARN |
| file_path | NULL (stderr) | "/var/log/app.log" |
| include_timestamp | true | true |
| include_source_location | true | false |
| max_file_size | 0 (sin limite) | 10*1024*1024 (10 MB) |

El default del macro es un intermedio (LOG_INFO, stderr). Cada
entorno lo ajusta segun necesidad.

</details>

---

### Ejercicio 2 — Validacion cruzada

Agrega validacion a `server_build()` para estas reglas:

1. Si `use_tls`, cert_path y key_path son obligatorios
2. `port` debe estar entre 1 y 65535
3. `max_connections` debe ser >= 1
4. Si `log_file` esta seteado, debe ser escribible
5. `num_workers` debe ser 0 (auto) o 1-256

Retorna un `BuildResult` con codigo de error y mensaje.

**Prediccion**: Es mejor validar cada campo por separado o
validar combinaciones al final?

<details><summary>Respuesta</summary>

```c
typedef enum {
    BUILD_OK,
    BUILD_INVALID_PORT,
    BUILD_INVALID_CONNECTIONS,
    BUILD_TLS_MISSING_CERT,
    BUILD_TLS_MISSING_KEY,
    BUILD_INVALID_LOG_FILE,
    BUILD_INVALID_WORKERS,
} BuildResultCode;

typedef struct {
    BuildResultCode code;
    char message[256];
} BuildResult;

BuildResult server_validate(const ServerConfig *cfg) {
    BuildResult r = { BUILD_OK, "" };

    if (cfg->port < 1 || cfg->port > 65535) {
        r.code = BUILD_INVALID_PORT;
        snprintf(r.message, sizeof r.message,
                 "port %d out of range 1-65535", cfg->port);
        return r;
    }

    if (cfg->max_connections < 1) {
        r.code = BUILD_INVALID_CONNECTIONS;
        snprintf(r.message, sizeof r.message,
                 "max_connections must be >= 1");
        return r;
    }

    if (cfg->use_tls && !cfg->cert_path) {
        r.code = BUILD_TLS_MISSING_CERT;
        snprintf(r.message, sizeof r.message,
                 "TLS enabled but cert_path is NULL");
        return r;
    }

    if (cfg->use_tls && !cfg->key_path) {
        r.code = BUILD_TLS_MISSING_KEY;
        snprintf(r.message, sizeof r.message,
                 "TLS enabled but key_path is NULL");
        return r;
    }

    if (cfg->log_file) {
        FILE *f = fopen(cfg->log_file, "a");
        if (!f) {
            r.code = BUILD_INVALID_LOG_FILE;
            snprintf(r.message, sizeof r.message,
                     "cannot open log file: %s", cfg->log_file);
            return r;
        }
        fclose(f);
    }

    if (cfg->num_workers < 0 || cfg->num_workers > 256) {
        r.code = BUILD_INVALID_WORKERS;
        snprintf(r.message, sizeof r.message,
                 "num_workers must be 0-256, got %d", cfg->num_workers);
        return r;
    }

    return r;
}

Server *server_build_checked(const ServerConfig *cfg, BuildResult *out) {
    *out = server_validate(cfg);
    if (out->code != BUILD_OK) return NULL;
    return server_build(cfg);
}

/* Uso: */
ServerConfig cfg = SERVER_CONFIG_DEFAULT;
cfg.port = 443;
cfg.use_tls = true;
/* Olvidamos cert_path */

BuildResult result;
Server *s = server_build_checked(&cfg, &result);
if (!s) {
    fprintf(stderr, "Build failed: %s\n", result.message);
    // "Build failed: TLS enabled but cert_path is NULL"
}
```

Validar cada campo individualmente Y combinaciones al final.
Los campos individuales detectan typos (port = -1). Las
combinaciones detectan dependencias (TLS sin cert).

</details>

---

### Ejercicio 3 — Default function con env vars

Escribe `server_config_from_env()` que lea defaults de variables
de entorno:

```
  SERVER_HOST=0.0.0.0
  SERVER_PORT=8080
  SERVER_TLS=true
  SERVER_CERT=/etc/cert.pem
  SERVER_KEY=/etc/key.pem
  SERVER_LOG_LEVEL=debug
```

**Prediccion**: Que pasa si una env var tiene un valor invalido
(ej. SERVER_PORT=abc)?

<details><summary>Respuesta</summary>

```c
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static LogLevel parse_log_level(const char *s) {
    if (!s) return LOG_INFO;
    if (strcasecmp(s, "none")  == 0) return LOG_NONE;
    if (strcasecmp(s, "error") == 0) return LOG_ERROR;
    if (strcasecmp(s, "warn")  == 0) return LOG_WARN;
    if (strcasecmp(s, "info")  == 0) return LOG_INFO;
    if (strcasecmp(s, "debug") == 0) return LOG_DEBUG;
    return LOG_INFO;  /* default si no reconoce */
}

static bool parse_bool(const char *s) {
    if (!s) return false;
    return strcasecmp(s, "true") == 0
        || strcasecmp(s, "1") == 0
        || strcasecmp(s, "yes") == 0;
}

ServerConfig server_config_from_env(void) {
    ServerConfig cfg = (ServerConfig)SERVER_CONFIG_DEFAULT;

    const char *val;

    if ((val = getenv("SERVER_HOST")))
        cfg.host = val;

    if ((val = getenv("SERVER_PORT"))) {
        int port = atoi(val);
        if (port > 0 && port <= 65535) {
            cfg.port = port;
        } else {
            fprintf(stderr, "WARNING: invalid SERVER_PORT=%s, "
                    "using default %d\n", val, cfg.port);
        }
    }

    if ((val = getenv("SERVER_TLS")))
        cfg.use_tls = parse_bool(val);

    if ((val = getenv("SERVER_CERT")))
        cfg.cert_path = val;

    if ((val = getenv("SERVER_KEY")))
        cfg.key_path = val;

    if ((val = getenv("SERVER_LOG_LEVEL")))
        cfg.log_level = parse_log_level(val);

    if ((val = getenv("SERVER_LOG_FILE")))
        cfg.log_file = val;

    if ((val = getenv("SERVER_WORKERS"))) {
        int n = atoi(val);
        if (n >= 0 && n <= 256) cfg.num_workers = n;
    }

    return cfg;
}

/* Uso: */
ServerConfig cfg = server_config_from_env();
cfg.port = 9090;  /* override programatico */
Server *s = server_build(&cfg);
```

Si una env var tiene valor invalido (SERVER_PORT=abc):
- `atoi("abc")` retorna 0
- El check `port > 0 && port <= 65535` falla
- Se imprime un warning y se usa el default

No es UB ni crash — solo usa el default. El warning informa al
operador. En produccion, podrias retornar error en lugar de warning.

Nota: los strings de `getenv()` apuntan a la memoria del entorno.
Son validos durante la vida del proceso, pero `server_build` debe
copiarlos con strdup si los necesita despues.

</details>

---

### Ejercicio 4 — Builder reutilizable

Modifica el HttpRequestBuilder para que `build()` **copie** en
lugar de transferir ownership, permitiendo reutilizar el builder
para crear multiples requests similares.

**Prediccion**: Es mas eficiente transferir o copiar?

<details><summary>Respuesta</summary>

```c
HttpRequest *http_request_build(const HttpRequestBuilder *b) {
    if (!b->url) return NULL;

    HttpRequest *req = malloc(sizeof *req);
    if (!req) return NULL;

    req->method = b->method;
    req->url = strdup(b->url);
    req->timeout_ms = b->timeout_ms;
    req->follow_redirects = b->follow_redirects;
    req->max_redirects = b->max_redirects;

    /* COPIAR headers (no transferir) */
    req->num_headers = b->num_headers;
    req->headers = malloc(b->num_headers * sizeof(Header));
    memcpy(req->headers, b->headers,
           b->num_headers * sizeof(Header));

    /* COPIAR body */
    if (b->body) {
        req->body = strndup(b->body, b->body_len);
        req->body_len = b->body_len;
    } else {
        req->body = NULL;
        req->body_len = 0;
    }

    return req;
}

/* Ahora puedes reutilizar: */
HttpRequestBuilder b = http_request_new(HTTP_GET, "https://api.example.com");
http_request_header(&b, "Authorization", "Bearer token");

/* Request 1: /users */
b.url = "https://api.example.com/users";
HttpRequest *r1 = http_request_build(&b);

/* Request 2: /posts (reutiliza el header Authorization) */
b.url = "https://api.example.com/posts";
HttpRequest *r2 = http_request_build(&b);

/* Ambas tienen Authorization header */
```

Transferir es mas eficiente (sin malloc/memcpy), pero impide
reutilizar. Copiar es menos eficiente pero permite reutilizacion.

Regla: si el builder se usa una sola vez → transferir.
Si el builder es un "template" para multiples objetos → copiar.

En Rust, el tipo sistema resuelve esto: `build(self)` consume
(transfer), `build(&self)` presta (copy). El compilador previene
usar un builder consumido.

</details>

---

### Ejercicio 5 — Funcion convenience sobre el builder

Escribe funciones convenience que cubran los casos de uso mas
comunes sin exponer el config struct:

```c
Server *server_simple(int port);
Server *server_tls(int port, const char *cert, const char *key);
Server *server_dev(void);
```

**Prediccion**: Estas funciones son Simple Factories o Builders?

<details><summary>Respuesta</summary>

```c
Server *server_simple(int port) {
    ServerConfig cfg = SERVER_CONFIG_DEFAULT;
    cfg.port = port;
    return server_build(&cfg);
}

Server *server_tls(int port, const char *cert, const char *key) {
    ServerConfig cfg = SERVER_CONFIG_DEFAULT;
    cfg.port = port;
    cfg.use_tls = true;
    cfg.cert_path = cert;
    cfg.key_path = key;
    return server_build(&cfg);
}

Server *server_dev(void) {
    ServerConfig cfg = SERVER_CONFIG_DEFAULT;
    cfg.port = 3000;
    cfg.log_level = LOG_DEBUG;
    cfg.max_connections = 10;
    cfg.num_workers = 1;
    return server_build(&cfg);
}

/* Uso: */
Server *s1 = server_simple(8080);       /* rapido, defaults */
Server *s2 = server_tls(443, cert, key); /* produccion */
Server *s3 = server_dev();               /* desarrollo */
```

Son **Simple Factories** construidas sobre el Builder. El builder
(config struct + build) es la infraestructura. Las funciones
convenience son factories que preconfiguran el builder para casos
comunes.

Este es un patron muy comun:
- Builder: para usuarios que necesitan control total
- Convenience factories: para el 80% de los casos de uso
- Ambos coexisten en la API publica

Ejemplos reales:
- `fopen()` es una convenience factory (vs `open()` con flags)
- `pthread_create()` con `NULL` attrs es convenience vs attrs builder

</details>

---

### Ejercicio 6 — Builder para SQL query

Diseña un builder para queries SQL SELECT:

```c
/* Uso deseado: */
SqlQuery *q = sql_select("name", "age")
    ->from("users")
    ->where("age > 18")
    ->where("active = true")
    ->order_by("name", ASC)
    ->limit(10)
    ->build();

printf("%s\n", sql_query_to_string(q));
// SELECT name, age FROM users WHERE age > 18 AND active = true
// ORDER BY name ASC LIMIT 10
```

Implementa el builder (config struct o chaining).

**Prediccion**: Que parte de SQL NO deberia construirse con
string concatenation? (pista: seguridad)

<details><summary>Respuesta</summary>

```c
typedef enum { ASC, DESC } SortOrder;

typedef struct {
    char columns[512];
    char table[64];
    char conditions[512];  /* WHERE clauses joined by AND */
    int  num_conditions;
    char order_column[64];
    SortOrder order_dir;
    bool has_order;
    int  limit;            /* -1 = no limit */
} SqlQueryBuilder;

SqlQueryBuilder sql_select_new(const char *columns) {
    SqlQueryBuilder b = {0};
    strncpy(b.columns, columns, 511);
    b.limit = -1;
    return b;
}

SqlQueryBuilder *sql_from(SqlQueryBuilder *b, const char *table) {
    strncpy(b->table, table, 63);
    return b;
}

SqlQueryBuilder *sql_where(SqlQueryBuilder *b, const char *condition) {
    if (b->num_conditions > 0) {
        strncat(b->conditions, " AND ", 511 - strlen(b->conditions));
    }
    strncat(b->conditions, condition, 511 - strlen(b->conditions));
    b->num_conditions++;
    return b;
}

SqlQueryBuilder *sql_order_by(SqlQueryBuilder *b, const char *col,
                               SortOrder dir) {
    strncpy(b->order_column, col, 63);
    b->order_dir = dir;
    b->has_order = true;
    return b;
}

SqlQueryBuilder *sql_limit(SqlQueryBuilder *b, int n) {
    b->limit = n;
    return b;
}

char *sql_build(const SqlQueryBuilder *b) {
    char *sql = malloc(2048);
    if (!sql) return NULL;

    int pos = snprintf(sql, 2048, "SELECT %s FROM %s",
                       b->columns, b->table);

    if (b->num_conditions > 0) {
        pos += snprintf(sql + pos, 2048 - pos,
                        " WHERE %s", b->conditions);
    }
    if (b->has_order) {
        pos += snprintf(sql + pos, 2048 - pos,
                        " ORDER BY %s %s", b->order_column,
                        b->order_dir == ASC ? "ASC" : "DESC");
    }
    if (b->limit >= 0) {
        snprintf(sql + pos, 2048 - pos, " LIMIT %d", b->limit);
    }

    return sql;
}

/* Uso: */
SqlQueryBuilder b = sql_select_new("name, age");
sql_from(&b, "users");
sql_where(&b, "age > 18");
sql_where(&b, "active = true");
sql_order_by(&b, "name", ASC);
sql_limit(&b, 10);

char *sql = sql_build(&b);
printf("%s\n", sql);
// SELECT name, age FROM users WHERE age > 18 AND active = true
// ORDER BY name ASC LIMIT 10
free(sql);
```

**Seguridad**: los VALORES de usuario nunca deben concatenarse
directamente en el SQL. En lugar de:
```c
sql_where(&b, "name = 'Alice'");  /* SQL INJECTION si viene del user */
```
Usar parametros preparados:
```c
sql_where(&b, "name = ?");
/* Y luego bind el valor con prepared statements */
```

El builder debe construir el **template** de la query, no la query
final con valores. Los valores van en bind parameters, nunca en
strings concatenados. Esto aplica a cualquier lenguaje.

</details>

---

### Ejercicio 7 — Builder que produce struct inmutable

Diseña un builder donde el objeto construido sea inmutable
(todos los campos `const`). El builder es mutable; despues de
`build()`, el resultado no puede modificarse.

**Prediccion**: Puedes hacer un struct con todos los campos const
en C?

<details><summary>Respuesta</summary>

```c
/* Objeto inmutable */
typedef struct {
    const char *const host;
    const int port;
    const bool use_tls;
    const int max_connections;
} ImmutableServerConfig;

/* Problema: no puedes asignar campos individualmente
   porque son const. Solo puedes inicializarlos al crear. */

/* Solucion: el builder usa un struct mutable, y build()
   copia a un struct inmutable via compound literal o memcpy. */

typedef struct {
    char *host;
    int port;
    bool use_tls;
    int max_connections;
} ServerConfigBuilder;

ImmutableServerConfig *build_immutable(const ServerConfigBuilder *b) {
    /* Allocar y copiar de una vez */
    ImmutableServerConfig *cfg = malloc(sizeof *cfg);
    if (!cfg) return NULL;

    /* Trick: memcpy ignora const (es UB tecnico pero universalmente
       usado). La alternativa correcta es un union type-pun o
       simplemente usar el struct mutable y no modificar despues. */
    ImmutableServerConfig tmp = {
        .host = strdup(b->host),
        .port = b->port,
        .use_tls = b->use_tls,
        .max_connections = b->max_connections,
    };
    memcpy(cfg, &tmp, sizeof tmp);
    return cfg;
}
```

En la practica, C no tiene una buena forma de hacer structs
inmutables. El patron mas comun es:
1. Usar un **opaque pointer** (`const ServerConfig *`)
2. Solo exponer getters, no el struct
3. La "inmutabilidad" es por convencion y encapsulacion, no
   por el tipo sistema

```c
/* Patron opaque + getters = inmutabilidad real */
/* server_config.h */
typedef struct ServerConfig ServerConfig;  /* opaque */
const char *server_config_host(const ServerConfig *c);
int server_config_port(const ServerConfig *c);
/* No hay setters → el usuario no puede modificar */
```

Rust resuelve esto trivialmente: los structs son inmutables por
defecto. Solo `&mut` permite modificar.

</details>

---

### Ejercicio 8 — Builder pattern vs init function

Compara estas dos APIs para crear un `ThreadPool`:

```c
/* A: init function con muchos parametros */
ThreadPool *pool_new(int num_threads, int queue_size,
                     void (*on_error)(int), bool daemon);

/* B: config struct builder */
typedef struct {
    int num_threads;
    int queue_size;
    void (*on_error)(int);
    bool daemon;
} PoolConfig;
#define POOL_CONFIG_DEFAULT { 4, 1024, NULL, false }
ThreadPool *pool_build(const PoolConfig *cfg);
```

Lista ventajas de cada enfoque. A partir de cuantos parametros
el builder es claramente mejor?

<details><summary>Respuesta</summary>

**Init function (A):**
- Mas conciso para pocos parametros
- El compilador verifica que pasas todos los argumentos
- No necesitas definir un struct extra
- IDE muestra los nombres de parametros al escribir

**Config struct (B):**
- Cada campo tiene nombre explícito (auto-documentado)
- Defaults para todo → solo cambias lo que necesitas
- Agregar un campo no rompe call sites existentes
- Puedes pasar la config entre funciones
- Puedes serializar/deserializar la config
- Puedes tener multiples configs predefinidas

**Umbral:** la mayoria de las guias de estilo C dicen:

| Parametros | Recomendacion |
|-----------|--------------|
| 1-3 | Funcion directa |
| 4-6 | Config struct empieza a valer la pena |
| 7+ | Config struct casi obligatorio |

Con 4 parametros (el ejemplo), es borderline. Si todos son
obligatorios y claros, la funcion directa esta bien. Si hay
defaults o combinaciones opcionales, el config struct gana.

Regla practica: si te preguntas "este parametro es el tercero o
el cuarto?", ya necesitas un config struct.

</details>

---

### Ejercicio 9 — Macro builder (X-macros)

Usa X-macros para generar automaticamente el config struct,
los defaults, y las funciones setter del builder a partir de
una definicion unica:

```c
#define SERVER_FIELDS(X) \
    X(const char *, host,            "0.0.0.0")  \
    X(int,          port,            8080)        \
    X(int,          max_connections, 100)         \
    X(bool,         use_tls,         false)       \
```

**Prediccion**: Es esto un buen patron o metaprogramacion
excesiva?

<details><summary>Respuesta</summary>

```c
/* Definicion de campos con X-macro */
#define SERVER_FIELDS(X) \
    X(const char *, host,            "0.0.0.0")  \
    X(int,          port,            8080)        \
    X(int,          max_connections, 100)         \
    X(int,          timeout_ms,      30000)       \
    X(bool,         use_tls,         false)

/* Generar el struct */
typedef struct {
    #define FIELD_DECL(type, name, def) type name;
    SERVER_FIELDS(FIELD_DECL)
    #undef FIELD_DECL
} ServerConfig;

/* Generar los defaults */
#define FIELD_DEFAULT(type, name, def) .name = def,
#define SERVER_CONFIG_DEFAULT { SERVER_FIELDS(FIELD_DEFAULT) }

/* Generar una funcion de print (debug) */
void server_config_print(const ServerConfig *cfg) {
    #define FIELD_PRINT(type, name, def) \
        printf("  " #name " = ");        \
        _Generic((cfg->name),            \
            const char *: printf("%s", cfg->name), \
            int:          printf("%d", cfg->name),  \
            bool:         printf("%s", cfg->name ? "true" : "false") \
        );                               \
        printf("\n");
    SERVER_FIELDS(FIELD_PRINT)
    #undef FIELD_PRINT
}
```

**Es metaprogramacion excesiva?** Depende:

- **Bueno si**: tienes muchos config structs similares (10+ campos),
  necesitas generar setters/getters/print/serialize para todos,
  y el equipo conoce X-macros.

- **Excesivo si**: solo un struct, pocos campos, o el equipo no
  esta familiarizado con X-macros. El codigo generado es dificil
  de debuggear y los errores de macro son crpticos.

Regla: si el boilerplate que eliminas es >50 lineas y se repite
en multiples structs, X-macros valen la pena. Si es un solo
struct, escribelo a mano — es mas claro.

En Rust, los derive macros (`#[derive(Debug, Default, Builder)]`)
resuelven esto de forma mas segura y legible.

</details>

---

### Ejercicio 10 — Reflexion: builder en C

Responde sin ver la respuesta:

1. Cual es la forma idiomatica de builder en C? Por que no se
   usa method chaining como en Java/Rust?

2. Nombra un ejemplo de builder que ya usaste en C sin saberlo
   (pista: networking, curses, o threading).

3. El builder de T01 no puede garantizar en compile time que
   campos obligatorios esten seteados. Es esto un problema en
   la practica? Que hace el builder de Rust (T03) para resolverlo?

4. Si tuvieras que elegir entre un constructor con 12 parametros
   o un config struct, cual elegirias y por que?

<details><summary>Respuesta</summary>

**1. Forma idiomatica en C:**

Config struct con designated initializers (C99). No se usa method
chaining porque en C la sintaxis es fea (funciones anidadas sin
operador `.`):

```c
// Java/Rust: natural
builder.host("0.0.0.0").port(8080).build()

// C: ilegible
server_build(server_port(server_host(server_new(), "0.0.0.0"), 8080))
```

El config struct logra lo mismo con mejor legibilidad:
```c
ServerConfig cfg = SERVER_CONFIG_DEFAULT;
cfg.host = "0.0.0.0";
cfg.port = 8080;
Server *s = server_build(&cfg);
```

**2. Builders en la practica:**

- `struct sockaddr_in` + `bind()`: configuras los campos del
  struct (familia, puerto, direccion) y luego llamas bind().
- `pthread_attr_t`: inicializas con `pthread_attr_init`,
  configuras con `pthread_attr_set*`, pasas a `pthread_create`.
- `struct termios`: obtienes con `tcgetattr`, modificas campos,
  aplicas con `tcsetattr`.
- `struct sigaction`: configuras handler, flags, mask, luego
  llamas `sigaction()`.

Todos siguen el patron: init struct → set fields → call function.

**3. Campos obligatorios:**

En la practica, la validacion en `build()` es suficiente para la
mayoria de los casos. Los programadores C estan acostumbrados a
errores en runtime (NULL return, error codes).

El builder de Rust T03 (Typestate Builder) resuelve esto usando
el **tipo sistema**: el builder tiene estados (generics) que
cambian al setear campos obligatorios. `build()` solo esta
disponible cuando todos los campos obligatorios estan seteados.
Un campo faltante es un error de **compilacion**, no de runtime.

Esto es imposible en C: el tipo sistema de C no tiene generics
ni estados parametricos.

**4. 12 parametros:**

Config struct, sin duda. Con 12 parametros posicionales:
- Imposible recordar el orden
- Facil confundir dos `int` consecutivos
- Agregar parametro 13 rompe TODOS los call sites
- No puedes tener defaults

Con config struct:
- Cada campo tiene nombre
- Defaults cubren el 80% de los casos
- Agregar un campo es backward-compatible
- Puedes reutilizar configs entre tests

La unica razon para no usar config struct seria si la funcion
tiene 1-2 parametros, todos obligatorios y sin ambiguedad.

</details>
