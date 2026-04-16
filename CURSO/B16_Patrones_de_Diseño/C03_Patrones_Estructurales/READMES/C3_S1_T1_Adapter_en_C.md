# T01 — Adapter en C

---

## 1. El problema: APIs incompatibles

Un adapter traduce la interfaz de un modulo existente a la
interfaz que otro modulo espera. El codigo existente no se
modifica — el adapter actua como intermediario:

```
  Sin adapter:
  ─────────────
  Modulo A (espera API X) ──✗──> Modulo B (ofrece API Y)
                                 No son compatibles

  Con adapter:
  ────────────
  Modulo A (espera API X) ──> [Adapter: X→Y] ──> Modulo B (API Y)
                               Traduce llamadas
```

Ejemplo concreto: tu codigo usa un logger con firma
`void log_msg(int level, const char *msg)`, pero una libreria
de terceros ofrece `syslog(int priority, const char *fmt, ...)`.

```
  Tu codigo                 Adapter               Libreria
  ─────────                 ───────               ────────
  log_msg(INFO, "hello")    translate_level()      syslog(LOG_INFO,
                            format_msg()             "%s", "hello")
  log_msg(ERR, "fail")     translate_level()      syslog(LOG_ERR,
                            format_msg()             "%s", "fail")
```

---

## 2. Adapter con wrapper functions

La forma mas simple: funciones que traducen una llamada a otra.

### 2.1 Ejemplo: adaptar syslog a nuestra interfaz de logger

```c
/* my_logger.h — interfaz que nuestro codigo espera */
#ifndef MY_LOGGER_H
#define MY_LOGGER_H

typedef enum {
    MY_LOG_DEBUG,
    MY_LOG_INFO,
    MY_LOG_WARN,
    MY_LOG_ERROR,
} MyLogLevel;

typedef struct MyLogger MyLogger;

MyLogger *my_logger_create(void);
void      my_logger_destroy(MyLogger *log);
void      my_logger_write(MyLogger *log, MyLogLevel level, const char *msg);

#endif
```

```c
/* syslog_adapter.c — adapter que traduce a syslog */
#include "my_logger.h"
#include <syslog.h>
#include <stdlib.h>
#include <string.h>

struct MyLogger {
    char ident[64];  /* Identificador para syslog */
    int  opened;
};

/* Tabla de traduccion: nuestros niveles → syslog priorities */
static int translate_level(MyLogLevel level) {
    switch (level) {
        case MY_LOG_DEBUG: return LOG_DEBUG;
        case MY_LOG_INFO:  return LOG_INFO;
        case MY_LOG_WARN:  return LOG_WARNING;
        case MY_LOG_ERROR: return LOG_ERR;
    }
    return LOG_INFO;
}

MyLogger *my_logger_create(void) {
    MyLogger *log = calloc(1, sizeof(*log));
    if (!log) return NULL;

    strncpy(log->ident, "myapp", sizeof(log->ident) - 1);
    openlog(log->ident, LOG_PID | LOG_NDELAY, LOG_USER);
    log->opened = 1;
    return log;
}

void my_logger_destroy(MyLogger *log) {
    if (log && log->opened) {
        closelog();
    }
    free(log);
}

void my_logger_write(MyLogger *log, MyLogLevel level, const char *msg) {
    if (!log || !log->opened) return;
    syslog(translate_level(level), "%s", msg);
}
```

```c
/* main.c — usa MY interfaz, no sabe que es syslog detras */
#include "my_logger.h"

int main(void) {
    MyLogger *log = my_logger_create();

    my_logger_write(log, MY_LOG_INFO, "Application started");
    my_logger_write(log, MY_LOG_ERROR, "Something went wrong");

    my_logger_destroy(log);
    return 0;
}
```

```
  El caller (main.c) no incluye <syslog.h>.
  No conoce LOG_INFO, LOG_ERR, openlog, closelog.
  Solo conoce my_logger.h — la interfaz adaptada.

  Si manana cambias syslog por otra libreria,
  solo reescribes el adapter .c, no main.c.
```

---

## 3. Adapter con opaque struct intermedio

Para adaptar una libreria con estado interno complejo:

### 3.1 Ejemplo: adaptar dos librerias de JSON a una interfaz comun

```c
/* json_parser.h — interfaz comun que nuestro codigo espera */
#ifndef JSON_PARSER_H
#define JSON_PARSER_H

typedef struct JsonParser JsonParser;
typedef struct JsonValue  JsonValue;

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT,
} JsonType;

JsonParser *json_parser_create(void);
void        json_parser_destroy(JsonParser *p);

JsonValue  *json_parse(JsonParser *p, const char *input);
void        json_value_free(JsonValue *val);

JsonType    json_type(const JsonValue *val);
double      json_as_number(const JsonValue *val);
const char *json_as_string(const JsonValue *val);
int         json_as_bool(const JsonValue *val);
JsonValue  *json_object_get(const JsonValue *val, const char *key);
size_t      json_array_len(const JsonValue *val);
JsonValue  *json_array_get(const JsonValue *val, size_t index);

#endif
```

```c
/* cjson_adapter.c — adapter para cJSON (libreria real) */
#include "json_parser.h"
#include <cJSON.h>      /* Libreria real de terceros */
#include <stdlib.h>

struct JsonParser {
    int hooks_installed;  /* Estado del adapter */
};

/* JsonValue envuelve cJSON* */
struct JsonValue {
    cJSON *inner;     /* Puntero a la estructura de cJSON */
    int    owned;     /* 1 si debemos hacer cJSON_Delete */
};

JsonParser *json_parser_create(void) {
    JsonParser *p = calloc(1, sizeof(*p));
    return p;
}

void json_parser_destroy(JsonParser *p) {
    free(p);
}

JsonValue *json_parse(JsonParser *p, const char *input) {
    (void)p;
    cJSON *root = cJSON_Parse(input);
    if (!root) return NULL;

    JsonValue *val = calloc(1, sizeof(*val));
    val->inner = root;
    val->owned = 1;  /* json_value_free debe hacer cJSON_Delete */
    return val;
}

void json_value_free(JsonValue *val) {
    if (!val) return;
    if (val->owned) {
        cJSON_Delete(val->inner);
    }
    free(val);
}

JsonType json_type(const JsonValue *val) {
    if (cJSON_IsNull(val->inner))   return JSON_NULL;
    if (cJSON_IsBool(val->inner))   return JSON_BOOL;
    if (cJSON_IsNumber(val->inner)) return JSON_NUMBER;
    if (cJSON_IsString(val->inner)) return JSON_STRING;
    if (cJSON_IsArray(val->inner))  return JSON_ARRAY;
    if (cJSON_IsObject(val->inner)) return JSON_OBJECT;
    return JSON_NULL;
}

double json_as_number(const JsonValue *val) {
    return val->inner->valuedouble;
}

const char *json_as_string(const JsonValue *val) {
    return val->inner->valuestring;
}

int json_as_bool(const JsonValue *val) {
    return cJSON_IsTrue(val->inner);
}

/* Hijos y arrays: retornan JsonValue NO-owned (no hacer Delete) */
static JsonValue *wrap_unowned(cJSON *node) {
    if (!node) return NULL;
    JsonValue *val = calloc(1, sizeof(*val));
    val->inner = node;
    val->owned = 0;  /* No es dueño — el root lo libera */
    return val;
}

JsonValue *json_object_get(const JsonValue *val, const char *key) {
    cJSON *child = cJSON_GetObjectItemCaseSensitive(val->inner, key);
    return wrap_unowned(child);
}

size_t json_array_len(const JsonValue *val) {
    return (size_t)cJSON_GetArraySize(val->inner);
}

JsonValue *json_array_get(const JsonValue *val, size_t index) {
    cJSON *item = cJSON_GetArrayItem(val->inner, (int)index);
    return wrap_unowned(item);
}
```

### 3.2 Intercambiar la implementacion

Si queremos usar otra libreria (ej: jsmn), escribimos otro
adapter con el mismo header:

```c
/* jsmn_adapter.c — adapter alternativo para jsmn */
#include "json_parser.h"
#include <jsmn.h>       /* Libreria diferente */
#include <stdlib.h>
#include <string.h>

/* Internamente, jsmn usa tokens en vez de arbol.
   El adapter traduce esta representacion a nuestra API. */

struct JsonParser {
    jsmn_parser parser;
};

struct JsonValue {
    jsmntok_t *tokens;
    const char *json_str;  /* El string original */
    int         root_idx;  /* Indice del token raiz */
    int         num_tokens;
    int         owned;
};

/* ... implementar todas las funciones de json_parser.h
   usando jsmn_parse, tokens, etc. ... */
```

```
  El codigo que usa json_parser.h no cambia:

  Linkear con cjson_adapter.o → usa cJSON
  Linkear con jsmn_adapter.o  → usa jsmn

  Mismo programa, diferente backend, sin recompilar main.c
  (solo re-linkear).
```

---

## 4. Adapter con function pointers (vtable)

Para poder cambiar de backend en **runtime** (no solo en link time):

```c
/* storage.h — interfaz abstracta de almacenamiento */
#ifndef STORAGE_H
#define STORAGE_H

#include <stddef.h>

typedef struct Storage Storage;

/* Operaciones que todo backend debe implementar */
typedef struct {
    int    (*open)(Storage *s, const char *path);
    void   (*close)(Storage *s);
    int    (*read)(Storage *s, const char *key,
                   char *buf, size_t buf_size);
    int    (*write)(Storage *s, const char *key,
                    const char *data, size_t len);
    int    (*delete)(Storage *s, const char *key);
} StorageOps;

struct Storage {
    const StorageOps *ops;
    void             *backend_data;  /* Estado del backend concreto */
};

/* Funciones de conveniencia */
static inline int storage_open(Storage *s, const char *path) {
    return s->ops->open(s, path);
}
static inline void storage_close(Storage *s) {
    s->ops->close(s);
}
static inline int storage_read(Storage *s, const char *key,
                               char *buf, size_t buf_size) {
    return s->ops->read(s, key, buf, buf_size);
}
static inline int storage_write(Storage *s, const char *key,
                                const char *data, size_t len) {
    return s->ops->write(s, key, data, len);
}
static inline int storage_delete(Storage *s, const char *key) {
    return s->ops->delete(s, key);
}

#endif
```

```c
/* file_storage.c — adapter a filesystem */
#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char base_path[256];
} FileBackend;

static void make_path(const FileBackend *fb, const char *key,
                      char *out, size_t out_size) {
    snprintf(out, out_size, "%s/%s", fb->base_path, key);
}

static int file_open(Storage *s, const char *path) {
    FileBackend *fb = calloc(1, sizeof(*fb));
    strncpy(fb->base_path, path, sizeof(fb->base_path) - 1);
    s->backend_data = fb;
    return 0;
}

static void file_close(Storage *s) {
    free(s->backend_data);
    s->backend_data = NULL;
}

static int file_read(Storage *s, const char *key,
                     char *buf, size_t buf_size) {
    FileBackend *fb = s->backend_data;
    char path[512];
    make_path(fb, key, path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (!f) return -1;
    size_t n = fread(buf, 1, buf_size - 1, f);
    buf[n] = '\0';
    fclose(f);
    return (int)n;
}

static int file_write(Storage *s, const char *key,
                      const char *data, size_t len) {
    FileBackend *fb = s->backend_data;
    char path[512];
    make_path(fb, key, path, sizeof(path));

    FILE *f = fopen(path, "w");
    if (!f) return -1;
    size_t n = fwrite(data, 1, len, f);
    fclose(f);
    return (int)n;
}

static int file_delete(Storage *s, const char *key) {
    FileBackend *fb = s->backend_data;
    char path[512];
    make_path(fb, key, path, sizeof(path));
    return remove(path);
}

static const StorageOps file_ops = {
    .open   = file_open,
    .close  = file_close,
    .read   = file_read,
    .write  = file_write,
    .delete = file_delete,
};

Storage file_storage(void) {
    return (Storage){ .ops = &file_ops, .backend_data = NULL };
}
```

```c
/* memory_storage.c — adapter a hash map en memoria */
#include "storage.h"
#include <stdlib.h>
#include <string.h>

#define MAX_ENTRIES 256
#define MAX_KEY_LEN 128
#define MAX_VAL_LEN 4096

typedef struct {
    char   key[MAX_KEY_LEN];
    char   value[MAX_VAL_LEN];
    size_t value_len;
    int    used;
} Entry;

typedef struct {
    Entry entries[MAX_ENTRIES];
} MemBackend;

static int mem_open(Storage *s, const char *path) {
    (void)path;
    MemBackend *mb = calloc(1, sizeof(*mb));
    s->backend_data = mb;
    return 0;
}

static void mem_close(Storage *s) {
    free(s->backend_data);
    s->backend_data = NULL;
}

static Entry *find_entry(MemBackend *mb, const char *key) {
    for (int i = 0; i < MAX_ENTRIES; i++) {
        if (mb->entries[i].used &&
            strcmp(mb->entries[i].key, key) == 0) {
            return &mb->entries[i];
        }
    }
    return NULL;
}

static int mem_read(Storage *s, const char *key,
                    char *buf, size_t buf_size) {
    MemBackend *mb = s->backend_data;
    Entry *e = find_entry(mb, key);
    if (!e) return -1;
    size_t n = e->value_len < buf_size ? e->value_len : buf_size - 1;
    memcpy(buf, e->value, n);
    buf[n] = '\0';
    return (int)n;
}

static int mem_write(Storage *s, const char *key,
                     const char *data, size_t len) {
    MemBackend *mb = s->backend_data;
    Entry *e = find_entry(mb, key);
    if (!e) {
        /* Buscar slot libre */
        for (int i = 0; i < MAX_ENTRIES; i++) {
            if (!mb->entries[i].used) {
                e = &mb->entries[i];
                break;
            }
        }
        if (!e) return -1;  /* Lleno */
    }
    strncpy(e->key, key, MAX_KEY_LEN - 1);
    size_t n = len < MAX_VAL_LEN ? len : MAX_VAL_LEN;
    memcpy(e->value, data, n);
    e->value_len = n;
    e->used = 1;
    return (int)n;
}

static int mem_delete(Storage *s, const char *key) {
    MemBackend *mb = s->backend_data;
    Entry *e = find_entry(mb, key);
    if (!e) return -1;
    e->used = 0;
    return 0;
}

static const StorageOps mem_ops = {
    .open   = mem_open,
    .close  = mem_close,
    .read   = mem_read,
    .write  = mem_write,
    .delete = mem_delete,
};

Storage memory_storage(void) {
    return (Storage){ .ops = &mem_ops, .backend_data = NULL };
}
```

```c
/* main.c — elige backend en runtime */
#include "storage.h"
#include <stdio.h>
#include <string.h>

/* Declarados en los .c del adapter */
Storage file_storage(void);
Storage memory_storage(void);

void demo(Storage *s) {
    storage_open(s, "/tmp/data");

    const char *data = "hello world";
    storage_write(s, "greeting", data, strlen(data));

    char buf[256];
    int n = storage_read(s, "greeting", buf, sizeof(buf));
    if (n > 0) {
        printf("Read: %s\n", buf);
    }

    storage_delete(s, "greeting");
    storage_close(s);
}

int main(int argc, char *argv[]) {
    Storage s;

    if (argc > 1 && strcmp(argv[1], "--memory") == 0) {
        s = memory_storage();
        printf("Using memory backend\n");
    } else {
        s = file_storage();
        printf("Using file backend\n");
    }

    demo(&s);
    return 0;
}
```

```
  Estructura:
  ───────────

  main.c → storage.h (interfaz abstracta)
              │
              ├── file_storage.c    (adapter → filesystem)
              │     └─ fopen, fread, fwrite, remove
              │
              └── memory_storage.c  (adapter → hash map)
                    └─ array de entries en RAM

  demo() no sabe que backend usa.
  Solo conoce storage_read/write/delete.
```

---

## 5. Adapter para librerias legacy

Caso comun: envolver una API vieja/fea en una interfaz limpia.

### 5.1 Ejemplo: adaptar la API de sockets POSIX

```c
/* net.h — interfaz simplificada */
#ifndef NET_H
#define NET_H

#include <stddef.h>

typedef struct NetConn NetConn;

NetConn    *net_connect(const char *host, int port);
void        net_disconnect(NetConn *conn);
int         net_send(NetConn *conn, const void *data, size_t len);
int         net_recv(NetConn *conn, void *buf, size_t buf_size);
const char *net_error(const NetConn *conn);

#endif
```

```c
/* net_posix.c — adapter sobre sockets POSIX */
#include "net.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

struct NetConn {
    int  fd;
    char error[256];
};

NetConn *net_connect(const char *host, int port) {
    NetConn *conn = calloc(1, sizeof(*conn));
    if (!conn) return NULL;
    conn->fd = -1;

    /* Resolver hostname */
    struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM };
    struct addrinfo *result = NULL;
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    int err = getaddrinfo(host, port_str, &hints, &result);
    if (err != 0) {
        snprintf(conn->error, sizeof(conn->error),
                 "DNS resolution failed: %s", gai_strerror(err));
        return conn;
    }

    /* Crear socket */
    conn->fd = socket(result->ai_family, result->ai_socktype,
                      result->ai_protocol);
    if (conn->fd < 0) {
        snprintf(conn->error, sizeof(conn->error),
                 "socket: %s", strerror(errno));
        freeaddrinfo(result);
        return conn;
    }

    /* Conectar */
    if (connect(conn->fd, result->ai_addr, result->ai_addrlen) < 0) {
        snprintf(conn->error, sizeof(conn->error),
                 "connect: %s", strerror(errno));
        close(conn->fd);
        conn->fd = -1;
    }

    freeaddrinfo(result);
    return conn;
}

void net_disconnect(NetConn *conn) {
    if (!conn) return;
    if (conn->fd >= 0) {
        close(conn->fd);
    }
    free(conn);
}

int net_send(NetConn *conn, const void *data, size_t len) {
    if (conn->fd < 0) return -1;

    ssize_t sent = send(conn->fd, data, len, 0);
    if (sent < 0) {
        snprintf(conn->error, sizeof(conn->error),
                 "send: %s", strerror(errno));
        return -1;
    }
    return (int)sent;
}

int net_recv(NetConn *conn, void *buf, size_t buf_size) {
    if (conn->fd < 0) return -1;

    ssize_t received = recv(conn->fd, buf, buf_size, 0);
    if (received < 0) {
        snprintf(conn->error, sizeof(conn->error),
                 "recv: %s", strerror(errno));
        return -1;
    }
    return (int)received;
}

const char *net_error(const NetConn *conn) {
    if (conn->error[0] == '\0') return NULL;
    return conn->error;
}
```

```c
/* main.c */
#include "net.h"
#include <stdio.h>

int main(void) {
    NetConn *conn = net_connect("example.com", 80);
    if (net_error(conn)) {
        fprintf(stderr, "Error: %s\n", net_error(conn));
        net_disconnect(conn);
        return 1;
    }

    const char *req = "GET / HTTP/1.0\r\nHost: example.com\r\n\r\n";
    net_send(conn, req, strlen(req));

    char buf[4096];
    int n = net_recv(conn, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        printf("%s\n", buf);
    }

    net_disconnect(conn);
    return 0;
}
```

Lo que el adapter oculta:
- `getaddrinfo` / `freeaddrinfo` (resolucion DNS)
- `socket()` + `connect()` separados
- `struct sockaddr_in`, `AF_INET`, `SOCK_STREAM`
- errno + strerror
- ssize_t vs size_t
- Manejo de file descriptors

---

## 6. Adapter bidireccional: traducir en ambas direcciones

A veces necesitas adaptar no solo las llamadas sino tambien
los callbacks o valores de retorno:

```c
/* event.h — interfaz de eventos de nuestro sistema */
typedef struct {
    int         type;
    int         x, y;        /* Coordenadas */
    const char *source;
} Event;

typedef void (*EventHandler)(const Event *event, void *user_data);

typedef struct EventSystem EventSystem;

EventSystem *event_system_create(void);
void         event_system_destroy(EventSystem *es);
void         event_system_subscribe(EventSystem *es,
                                    int event_type,
                                    EventHandler handler,
                                    void *user_data);
void         event_system_poll(EventSystem *es);
```

```c
/* sdl_event_adapter.c — adaptar SDL_Event a nuestro Event */
#include "event.h"
#include <SDL2/SDL.h>
#include <stdlib.h>

#define MAX_HANDLERS 64

typedef struct {
    int          event_type;
    EventHandler handler;
    void        *user_data;
} Subscription;

struct EventSystem {
    Subscription subs[MAX_HANDLERS];
    int          sub_count;
};

/* Traduccion: SDL event type → nuestro event type */
static int translate_type(Uint32 sdl_type) {
    switch (sdl_type) {
        case SDL_MOUSEMOTION:    return 1;
        case SDL_MOUSEBUTTONDOWN: return 2;
        case SDL_KEYDOWN:        return 3;
        case SDL_QUIT:           return 4;
        default:                 return 0;
    }
}

/* Traduccion: SDL_Event → nuestro Event */
static Event translate_event(const SDL_Event *sdl_ev) {
    Event ev = { .type = translate_type(sdl_ev->type), .source = "sdl" };
    switch (sdl_ev->type) {
        case SDL_MOUSEMOTION:
            ev.x = sdl_ev->motion.x;
            ev.y = sdl_ev->motion.y;
            break;
        case SDL_MOUSEBUTTONDOWN:
            ev.x = sdl_ev->button.x;
            ev.y = sdl_ev->button.y;
            break;
        default:
            ev.x = 0;
            ev.y = 0;
            break;
    }
    return ev;
}

EventSystem *event_system_create(void) {
    SDL_Init(SDL_INIT_EVENTS);
    return calloc(1, sizeof(EventSystem));
}

void event_system_destroy(EventSystem *es) {
    SDL_Quit();
    free(es);
}

void event_system_subscribe(EventSystem *es, int event_type,
                            EventHandler handler, void *user_data) {
    if (es->sub_count >= MAX_HANDLERS) return;
    es->subs[es->sub_count++] = (Subscription){
        .event_type = event_type,
        .handler    = handler,
        .user_data  = user_data,
    };
}

void event_system_poll(EventSystem *es) {
    SDL_Event sdl_ev;
    while (SDL_PollEvent(&sdl_ev)) {
        Event ev = translate_event(&sdl_ev);
        if (ev.type == 0) continue;  /* Evento no mapeado */

        for (int i = 0; i < es->sub_count; i++) {
            if (es->subs[i].event_type == ev.type ||
                es->subs[i].event_type == 0) {  /* 0 = todos */
                es->subs[i].handler(&ev, es->subs[i].user_data);
            }
        }
    }
}
```

```
  Flujo bidireccional:
  ────────────────────

  SDL (externo)              Adapter              Tu codigo
  ───────────                ───────              ─────────
  SDL_PollEvent()  ──────>   translate_event()    handler(&ev)
  SDL_Event struct           SDL_Event → Event    Recibe Event
  SDL_MOUSEMOTION            type 1               type 1
  SDL_QUIT                   type 4               type 4

  La traduccion va de SDL → tu formato (input)
  Los callbacks van de tu codigo → ejecutados por el adapter
```

---

## 7. Adapter en el mundo real

### 7.1 Casos comunes en C

```
  Situacion                   Que adaptas           Ejemplo
  ─────────                   ──────────            ───────
  Libreria de terceros        API fea → API limpia  POSIX sockets → net.h
  Backend intercambiable      Interfaz comun         SQLite/Postgres → db.h
  Legacy code                 API vieja → nueva      strcpy → strncpy wrapper
  Platform abstraction        OS-specific → comun    win32/posix → thread.h
  Testing                     Real → mock            network → fake_network
  Version migration           API v1 → API v2        libfoo1 → libfoo2
```

### 7.2 Linux kernel: adapters por todas partes

```
  VFS (Virtual File System):
  ──────────────────────────
  Interfaz comun: open, read, write, close, ioctl

  Adapters (file_operations struct):
  ├── ext4_file_operations   → ext4 filesystem
  ├── nfs_file_operations    → NFS (red)
  ├── proc_file_operations   → /proc (virtual)
  └── sysfs_file_operations  → /sys (virtual)

  Userspace llama open("/proc/cpuinfo")
  VFS selecciona proc_file_operations
  proc adapter traduce a lectura de datos del kernel
```

```
  Network stack:
  ──────────────
  Interfaz comun: socket, bind, connect, send, recv

  Adapters (proto_ops struct):
  ├── inet_stream_ops    → TCP/IPv4
  ├── inet6_stream_ops   → TCP/IPv6
  ├── unix_stream_ops    → Unix domain sockets
  └── packet_ops         → Raw packets
```

---

## 8. Adapter vs otras tecnicas

```
  Tecnica          Proposito                    Estructura
  ───────          ─────────                    ──────────
  Adapter          Traducir interfaz existente  Envuelve un modulo
  Facade (C03/S04) Simplificar interfaz         Capa sobre subsistema
  Decorator (C03/S02) Agregar comportamiento    Misma interfaz + extra
  Proxy            Controlar acceso             Misma interfaz + control

  Adapter:   interfaz A → interfaz B (TRADUCE)
  Facade:    interfaz compleja → interfaz simple (SIMPLIFICA)
  Decorator: interfaz A → interfaz A + extra (EXTIENDE)
```

---

## 9. Errores comunes

| Error | Por que falla | Solucion |
|---|---|---|
| Adapter que expone detalles del backend | El caller depende de syslog, no de tu API | Ocultar todo detras del opaque type |
| Traduccion incompleta de errores | Backend retorna error desconocido | Mapear TODOS los codigos de error |
| Adapter sin tests del backend real | El adapter compila pero la traduccion es incorrecta | Integration test con backend real |
| Leaks en el adapter | Backend alloca, adapter no libera | Cada create/open necesita destroy/close |
| Adapter que modifica la semantica | Backend hace X, adapter "mejora" a Y | El adapter TRADUCE, no transforma |
| Demasiados niveles de adapter | A adapta B adapta C adapta D | Maximo 2 niveles |
| Adapter sin ownership claro | ¿Quien libera el backend_data? | Documentar en header: create → destroy |
| Traduccion de callbacks sin user_data | No puedes pasar contexto al callback | Siempre incluir `void *user_data` |

---

## 10. Ejercicios

### Ejercicio 1 — Adapter de funciones simples

Tienes una libreria de matematicas con angulos en grados:

```c
/* legacy_math.h */
double legacy_sin(double degrees);
double legacy_cos(double degrees);
double legacy_atan2(double y, double x);  /* retorna grados */
```

Tu codigo trabaja en radianes. Escribe un adapter.

**Prediccion**: ¿cuantas funciones necesita el adapter? ¿Solo
traduce la entrada, la salida, o ambas?

<details>
<summary>Respuesta</summary>

```c
/* math_adapter.h */
double my_sin(double radians);
double my_cos(double radians);
double my_atan2(double y, double x);  /* retorna radianes */
```

```c
/* math_adapter.c */
#include "math_adapter.h"
#include "legacy_math.h"

#define RAD_TO_DEG(r) ((r) * 180.0 / 3.14159265358979323846)
#define DEG_TO_RAD(d) ((d) * 3.14159265358979323846 / 180.0)

double my_sin(double radians) {
    /* Traducir ENTRADA: radianes → grados */
    return legacy_sin(RAD_TO_DEG(radians));
}

double my_cos(double radians) {
    /* Traducir ENTRADA: radianes → grados */
    return legacy_cos(RAD_TO_DEG(radians));
}

double my_atan2(double y, double x) {
    /* Traducir SALIDA: grados → radianes */
    return DEG_TO_RAD(legacy_atan2(y, x));
}
```

Tres funciones. `sin`/`cos` traducen la **entrada** (rad→deg),
`atan2` traduce la **salida** (deg→rad). El adapter debe
analizar cada funcion para saber que direccion(es) traducir.

</details>

---

### Ejercicio 2 — Adapter con opaque struct

Adapta esta API de base de datos simplista:

```c
/* raw_db.h — API fea existente */
void *raw_db_open(const char *path, int flags, int mode);
int   raw_db_exec(void *handle, const char *sql, void **result);
char *raw_db_col(void *result, int row, int col);
void  raw_db_free_result(void *result);
void  raw_db_close(void *handle);
```

A una interfaz limpia con tipos opacos.

**Prediccion**: ¿que problemas tiene la API original?

<details>
<summary>Respuesta</summary>

```c
/* db.h — interfaz limpia */
typedef struct Db       Db;
typedef struct DbResult DbResult;

Db       *db_open(const char *path);
void      db_close(Db *db);
DbResult *db_query(Db *db, const char *sql);
int       db_result_rows(const DbResult *r);
int       db_result_cols(const DbResult *r);
const char *db_result_get(const DbResult *r, int row, int col);
void      db_result_free(DbResult *r);
```

```c
/* db_adapter.c */
#include "db.h"
#include "raw_db.h"
#include <stdlib.h>

struct Db {
    void *raw_handle;
};

struct DbResult {
    void *raw_result;
    int   rows;
    int   cols;
};

Db *db_open(const char *path) {
    Db *db = calloc(1, sizeof(*db));
    if (!db) return NULL;

    db->raw_handle = raw_db_open(path, 0, 0644);
    if (!db->raw_handle) {
        free(db);
        return NULL;
    }
    return db;
}

void db_close(Db *db) {
    if (!db) return;
    raw_db_close(db->raw_handle);
    free(db);
}

DbResult *db_query(Db *db, const char *sql) {
    DbResult *r = calloc(1, sizeof(*r));
    if (!r) return NULL;

    int ret = raw_db_exec(db->raw_handle, sql, &r->raw_result);
    if (ret != 0) {
        free(r);
        return NULL;
    }
    /* Asumir que raw_db provee metadata de alguna forma */
    return r;
}

const char *db_result_get(const DbResult *r, int row, int col) {
    return raw_db_col(r->raw_result, row, col);
}

void db_result_free(DbResult *r) {
    if (!r) return;
    raw_db_free_result(r->raw_result);
    free(r);
}
```

Problemas de la API original:
1. **Todo es `void *`** — no hay type safety
2. **`flags` y `mode` expuestos** — detalles de implementacion
3. **Sin tipos opacos** — el caller maneja punteros genericos
4. **`raw_db_col` retorna `char *`** — ¿quien libera? Ambiguo
5. **`raw_db_exec` retorna `void **result`** — output param feo

El adapter envuelve todo en tipos opacos con ownership claro.

</details>

---

### Ejercicio 3 — Adapter con vtable

Implementa una interfaz `Hasher` con vtable que pueda usar
diferentes backends (CRC32, djb2, simple sum):

```c
typedef struct Hasher Hasher;
uint32_t hasher_compute(Hasher *h, const void *data, size_t len);
```

**Prediccion**: ¿donde va la vtable y donde van los backends?

<details>
<summary>Respuesta</summary>

```c
/* hasher.h */
#include <stdint.h>
#include <stddef.h>

typedef struct Hasher Hasher;

typedef struct {
    uint32_t (*compute)(const void *data, size_t len);
    const char *name;
} HasherOps;

struct Hasher {
    const HasherOps *ops;
};

static inline uint32_t hasher_compute(Hasher *h,
                                      const void *data, size_t len) {
    return h->ops->compute(data, len);
}

static inline const char *hasher_name(Hasher *h) {
    return h->ops->name;
}

/* Factory functions */
Hasher hasher_djb2(void);
Hasher hasher_sum(void);
```

```c
/* hasher_backends.c */
#include "hasher.h"

static uint32_t djb2_compute(const void *data, size_t len) {
    const unsigned char *p = data;
    uint32_t hash = 5381;
    for (size_t i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + p[i];
    }
    return hash;
}

static const HasherOps djb2_ops = {
    .compute = djb2_compute,
    .name = "djb2",
};

Hasher hasher_djb2(void) {
    return (Hasher){ .ops = &djb2_ops };
}

static uint32_t sum_compute(const void *data, size_t len) {
    const unsigned char *p = data;
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += p[i];
    }
    return sum;
}

static const HasherOps sum_ops = {
    .compute = sum_compute,
    .name = "simple_sum",
};

Hasher hasher_sum(void) {
    return (Hasher){ .ops = &sum_ops };
}
```

```c
/* main.c */
void hash_data(Hasher *h, const char *data) {
    uint32_t result = hasher_compute(h, data, strlen(data));
    printf("[%s] hash of '%s' = %u\n", hasher_name(h), data, result);
}

int main(void) {
    Hasher h1 = hasher_djb2();
    Hasher h2 = hasher_sum();

    hash_data(&h1, "hello");
    hash_data(&h2, "hello");
    return 0;
}
```

La vtable (`HasherOps`) va en el header como tipo publico.
Los backends van en .c files con sus funciones `static` y
una constante `static const HasherOps` por backend. Solo
las factory functions son visibles fuera.

</details>

---

### Ejercicio 4 — Adapter para tests (mock)

Tu codigo usa `time_now()` para obtener el timestamp actual.
Para tests necesitas controlar el tiempo.

```c
/* time_provider.h */
typedef long (*TimeFunc)(void);

typedef struct {
    TimeFunc now;
} TimeProvider;
```

Implementa un adapter real y un mock.

**Prediccion**: ¿por que function pointer y no llamar `time()`
directamente?

<details>
<summary>Respuesta</summary>

```c
/* time_provider.h */
#ifndef TIME_PROVIDER_H
#define TIME_PROVIDER_H

typedef long (*TimeFunc)(void);

typedef struct {
    TimeFunc now;
} TimeProvider;

/* Real provider */
TimeProvider time_provider_system(void);

/* Mock provider para tests */
TimeProvider time_provider_mock(long fixed_time);
void         time_mock_set(long new_time);
void         time_mock_advance(long seconds);

#endif
```

```c
/* time_provider.c */
#include "time_provider.h"
#include <time.h>

/* Real */
static long system_now(void) {
    return (long)time(NULL);
}

TimeProvider time_provider_system(void) {
    return (TimeProvider){ .now = system_now };
}

/* Mock */
static long g_mock_time = 0;

static long mock_now(void) {
    return g_mock_time;
}

TimeProvider time_provider_mock(long fixed_time) {
    g_mock_time = fixed_time;
    return (TimeProvider){ .now = mock_now };
}

void time_mock_set(long new_time) {
    g_mock_time = new_time;
}

void time_mock_advance(long seconds) {
    g_mock_time += seconds;
}
```

```c
/* cache.c — usa TimeProvider, no time() directamente */
int cache_is_expired(Cache *c, const char *key, TimeProvider *tp) {
    long now = tp->now();
    long stored_at = cache_get_timestamp(c, key);
    return (now - stored_at) > c->ttl;
}
```

```c
/* test_cache.c */
void test_cache_expiration(void) {
    TimeProvider tp = time_provider_mock(1000);
    Cache c = cache_create(60);  /* TTL = 60 sec */

    cache_put(&c, "key", "value", &tp);

    /* No expirado */
    assert(!cache_is_expired(&c, "key", &tp));

    /* Avanzar 61 segundos */
    time_mock_advance(61);
    assert(cache_is_expired(&c, "key", &tp));
}
```

Function pointer porque:
- En tests, `time()` retorna el tiempo real — no puedes
  simular que pasaron 61 segundos
- Con function pointer, inyectas el mock y controlas el tiempo
- Es dependency injection via function pointer — el adapter
  traduce "obtener tiempo" a la fuente correcta (real o mock)

</details>

---

### Ejercicio 5 — Adapter de formatos de datos

Tienes datos en formato CSV y tu sistema espera JSON strings.
Escribe un adapter que traduzca lineas CSV a JSON.

**Prediccion**: ¿el adapter deberia convertir todo el archivo
o linea por linea?

<details>
<summary>Respuesta</summary>

Linea por linea — es mas flexible y usa menos memoria:

```c
/* csv_json_adapter.h */
typedef struct CsvJsonAdapter CsvJsonAdapter;

CsvJsonAdapter *csv_json_create(const char *header_line);
void            csv_json_destroy(CsvJsonAdapter *a);

/* Convierte una linea CSV a JSON object string */
/* Retorna buffer interno — valido hasta la proxima llamada */
const char *csv_json_convert(CsvJsonAdapter *a, const char *csv_line);
```

```c
/* csv_json_adapter.c */
#include "csv_json_adapter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_FIELDS 32
#define MAX_FIELD_LEN 256

struct CsvJsonAdapter {
    char  headers[MAX_FIELDS][MAX_FIELD_LEN];
    int   num_fields;
    char  output[4096];
};

static int split_csv(const char *line, char fields[][MAX_FIELD_LEN],
                     int max_fields) {
    int count = 0;
    const char *p = line;
    while (*p && count < max_fields) {
        const char *comma = strchr(p, ',');
        size_t len;
        if (comma) {
            len = (size_t)(comma - p);
        } else {
            len = strlen(p);
            /* Quitar newline si existe */
            while (len > 0 && (p[len-1] == '\n' || p[len-1] == '\r'))
                len--;
        }
        if (len >= MAX_FIELD_LEN) len = MAX_FIELD_LEN - 1;
        memcpy(fields[count], p, len);
        fields[count][len] = '\0';
        count++;
        p = comma ? comma + 1 : p + strlen(p);
    }
    return count;
}

CsvJsonAdapter *csv_json_create(const char *header_line) {
    CsvJsonAdapter *a = calloc(1, sizeof(*a));
    a->num_fields = split_csv(header_line, a->headers, MAX_FIELDS);
    return a;
}

void csv_json_destroy(CsvJsonAdapter *a) {
    free(a);
}

const char *csv_json_convert(CsvJsonAdapter *a, const char *csv_line) {
    char values[MAX_FIELDS][MAX_FIELD_LEN];
    int n = split_csv(csv_line, values, MAX_FIELDS);

    char *out = a->output;
    int remaining = sizeof(a->output);
    int written = snprintf(out, remaining, "{");
    out += written; remaining -= written;

    int fields = n < a->num_fields ? n : a->num_fields;
    for (int i = 0; i < fields; i++) {
        written = snprintf(out, remaining, "%s\"%s\":\"%s\"",
                          i > 0 ? "," : "",
                          a->headers[i], values[i]);
        out += written; remaining -= written;
    }
    snprintf(out, remaining, "}");
    return a->output;
}
```

```c
/* main.c */
int main(void) {
    CsvJsonAdapter *a = csv_json_create("name,age,city");

    printf("%s\n", csv_json_convert(a, "Alice,30,NYC"));
    /* {"name":"Alice","age":"30","city":"NYC"} */

    printf("%s\n", csv_json_convert(a, "Bob,25,LA"));
    /* {"name":"Bob","age":"25","city":"LA"} */

    csv_json_destroy(a);
    return 0;
}
```

Linea por linea porque:
- No necesitas cargar todo el archivo en memoria
- Puedes procesar streams (stdin, socket)
- El caller decide si convertir 1 o 1 millon de lineas

</details>

---

### Ejercicio 6 — Adapter bidireccional

Tu sistema usa coordenadas cartesianas (x, y). Una libreria
de terceros usa polares (r, theta). Escribe un adapter que
traduzca en ambas direcciones.

**Prediccion**: ¿necesitas adaptar solo la entrada, solo la salida,
o ambas?

<details>
<summary>Respuesta</summary>

Ambas — envias cartesianas al adapter, el adapter convierte a
polares para la libreria, la libreria retorna polares, el adapter
convierte de vuelta a cartesianas:

```c
/* my_geometry.h — nuestra interfaz (cartesianas) */
typedef struct { double x, y; } Point;

typedef struct GeoEngine GeoEngine;

GeoEngine *geo_create(void);
void       geo_destroy(GeoEngine *g);
double     geo_distance(GeoEngine *g, Point a, Point b);
Point      geo_rotate(GeoEngine *g, Point p, double angle_rad);
```

```c
/* polar_lib.h — libreria de terceros (polares) */
typedef struct { double r, theta; } PolarPoint;
double polar_distance(PolarPoint a, PolarPoint b);
PolarPoint polar_rotate(PolarPoint p, double angle);
```

```c
/* geo_adapter.c */
#include "my_geometry.h"
#include "polar_lib.h"
#include <math.h>
#include <stdlib.h>

struct GeoEngine {
    int dummy;  /* Podria tener config */
};

/* Traduccion: cartesianas → polares */
static PolarPoint to_polar(Point p) {
    return (PolarPoint){
        .r     = sqrt(p.x * p.x + p.y * p.y),
        .theta = atan2(p.y, p.x),
    };
}

/* Traduccion: polares → cartesianas */
static Point to_cartesian(PolarPoint pp) {
    return (Point){
        .x = pp.r * cos(pp.theta),
        .y = pp.r * sin(pp.theta),
    };
}

GeoEngine *geo_create(void) {
    return calloc(1, sizeof(GeoEngine));
}

void geo_destroy(GeoEngine *g) {
    free(g);
}

double geo_distance(GeoEngine *g, Point a, Point b) {
    (void)g;
    /* ENTRADA: cartesianas → polares */
    PolarPoint pa = to_polar(a);
    PolarPoint pb = to_polar(b);
    /* Llamada a libreria */
    return polar_distance(pa, pb);
    /* SALIDA: double no necesita traduccion */
}

Point geo_rotate(GeoEngine *g, Point p, double angle_rad) {
    (void)g;
    /* ENTRADA: cartesiana → polar */
    PolarPoint pp = to_polar(p);
    /* Llamada a libreria */
    PolarPoint result = polar_rotate(pp, angle_rad);
    /* SALIDA: polar → cartesiana */
    return to_cartesian(result);
}
```

`distance` adapta entrada (2x `to_polar`), salida es `double` (no
traduce). `rotate` adapta entrada (`to_polar`) Y salida
(`to_cartesian`). Cada funcion del adapter decide que direcciones
traduce segun los tipos que entran y salen.

</details>

---

### Ejercicio 7 — Adapter con estado

Una libreria de cifrado trabaja con sesiones:

```c
/* crypto_lib.h */
void *crypto_init(int algorithm, int key_bits);
int   crypto_set_key(void *session, const unsigned char *key);
int   crypto_encrypt(void *session, const unsigned char *in,
                     size_t in_len, unsigned char *out, size_t *out_len);
int   crypto_decrypt(void *session, const unsigned char *in,
                     size_t in_len, unsigned char *out, size_t *out_len);
void  crypto_cleanup(void *session);
```

Escribe un adapter con opaque type que encapsule la sesion.

**Prediccion**: ¿que datos debe guardar el adapter struct ademas
de la sesion?

<details>
<summary>Respuesta</summary>

```c
/* cipher.h */
typedef struct Cipher Cipher;

typedef enum {
    CIPHER_AES128,
    CIPHER_AES256,
    CIPHER_CHACHA20,
} CipherAlgo;

Cipher *cipher_create(CipherAlgo algo, const unsigned char *key,
                      size_t key_len);
void    cipher_destroy(Cipher *c);

int     cipher_encrypt(Cipher *c, const unsigned char *plaintext,
                       size_t len, unsigned char *out, size_t *out_len);
int     cipher_decrypt(Cipher *c, const unsigned char *ciphertext,
                       size_t len, unsigned char *out, size_t *out_len);

CipherAlgo cipher_algorithm(const Cipher *c);
size_t     cipher_key_bits(const Cipher *c);
```

```c
/* cipher_adapter.c */
#include "cipher.h"
#include "crypto_lib.h"
#include <stdlib.h>

struct Cipher {
    void       *session;    /* Handle de la libreria */
    CipherAlgo  algo;       /* Metadata */
    size_t      key_bits;   /* Metadata */
};

static void translate_algo(CipherAlgo algo, int *lib_algo, int *bits) {
    switch (algo) {
        case CIPHER_AES128:   *lib_algo = 1; *bits = 128; break;
        case CIPHER_AES256:   *lib_algo = 1; *bits = 256; break;
        case CIPHER_CHACHA20: *lib_algo = 2; *bits = 256; break;
    }
}

Cipher *cipher_create(CipherAlgo algo, const unsigned char *key,
                      size_t key_len) {
    int lib_algo, bits;
    translate_algo(algo, &lib_algo, &bits);

    if (key_len * 8 < (size_t)bits) return NULL;

    void *session = crypto_init(lib_algo, bits);
    if (!session) return NULL;

    if (crypto_set_key(session, key) != 0) {
        crypto_cleanup(session);
        return NULL;
    }

    Cipher *c = malloc(sizeof(*c));
    c->session  = session;
    c->algo     = algo;
    c->key_bits = (size_t)bits;
    return c;
}

void cipher_destroy(Cipher *c) {
    if (!c) return;
    crypto_cleanup(c->session);
    free(c);
}

int cipher_encrypt(Cipher *c, const unsigned char *plaintext,
                   size_t len, unsigned char *out, size_t *out_len) {
    return crypto_encrypt(c->session, plaintext, len, out, out_len);
}

int cipher_decrypt(Cipher *c, const unsigned char *ciphertext,
                   size_t len, unsigned char *out, size_t *out_len) {
    return crypto_decrypt(c->session, ciphertext, len, out, out_len);
}

CipherAlgo cipher_algorithm(const Cipher *c) { return c->algo; }
size_t     cipher_key_bits(const Cipher *c)   { return c->key_bits; }
```

El adapter guarda ademas de la sesion:
- `algo`: el enum del caller (para `cipher_algorithm()`)
- `key_bits`: metadata derivada (para `cipher_key_bits()`)

La libreria usa `void *` y numeros magicos (1, 2). El adapter
traduce a enums tipados y encapsula todo en un opaque struct.
El caller nunca ve `void *session` ni los numeros de algoritmo.

</details>

---

### Ejercicio 8 — Adapter para platform abstraction

Escribe la firma de un adapter `thread.h` que abstraiga
pthreads (POSIX) y Win32 threads.

**Prediccion**: ¿que operaciones necesitas cubrir como minimo?

<details>
<summary>Respuesta</summary>

```c
/* thread.h — interfaz comun */
#ifndef THREAD_H
#define THREAD_H

typedef struct Thread Thread;
typedef struct Mutex  Mutex;

typedef void *(*ThreadFunc)(void *arg);

/* Thread */
Thread *thread_create(ThreadFunc func, void *arg);
int     thread_join(Thread *t, void **result);
void    thread_destroy(Thread *t);

/* Mutex */
Mutex  *mutex_create(void);
void    mutex_destroy(Mutex *m);
void    mutex_lock(Mutex *m);
void    mutex_unlock(Mutex *m);

#endif
```

```c
/* thread_posix.c */
#include "thread.h"
#include <pthread.h>
#include <stdlib.h>

struct Thread { pthread_t handle; };
struct Mutex  { pthread_mutex_t handle; };

Thread *thread_create(ThreadFunc func, void *arg) {
    Thread *t = malloc(sizeof(*t));
    if (pthread_create(&t->handle, NULL, func, arg) != 0) {
        free(t);
        return NULL;
    }
    return t;
}

int thread_join(Thread *t, void **result) {
    return pthread_join(t->handle, result);
}

void thread_destroy(Thread *t) { free(t); }

Mutex *mutex_create(void) {
    Mutex *m = malloc(sizeof(*m));
    pthread_mutex_init(&m->handle, NULL);
    return m;
}

void mutex_destroy(Mutex *m) {
    pthread_mutex_destroy(&m->handle);
    free(m);
}

void mutex_lock(Mutex *m)   { pthread_mutex_lock(&m->handle); }
void mutex_unlock(Mutex *m) { pthread_mutex_unlock(&m->handle); }
```

```c
/* thread_win32.c — adapter alternativo */
#include "thread.h"
#include <windows.h>
#include <stdlib.h>

struct Thread { HANDLE handle; };
struct Mutex  { CRITICAL_SECTION cs; };

Thread *thread_create(ThreadFunc func, void *arg) {
    Thread *t = malloc(sizeof(*t));
    t->handle = CreateThread(NULL, 0,
        (LPTHREAD_START_ROUTINE)func, arg, 0, NULL);
    if (!t->handle) { free(t); return NULL; }
    return t;
}

int thread_join(Thread *t, void **result) {
    WaitForSingleObject(t->handle, INFINITE);
    if (result) {
        DWORD exit_code;
        GetExitCodeThread(t->handle, &exit_code);
        *result = (void *)(uintptr_t)exit_code;
    }
    CloseHandle(t->handle);
    return 0;
}

void thread_destroy(Thread *t) { free(t); }

Mutex *mutex_create(void) {
    Mutex *m = malloc(sizeof(*m));
    InitializeCriticalSection(&m->cs);
    return m;
}

void mutex_destroy(Mutex *m) {
    DeleteCriticalSection(&m->cs);
    free(m);
}

void mutex_lock(Mutex *m)   { EnterCriticalSection(&m->cs); }
void mutex_unlock(Mutex *m) { LeaveCriticalSection(&m->cs); }
```

Operaciones minimas: create, join, destroy (thread) + create,
destroy, lock, unlock (mutex). SDL, glib, y Apache Portable
Runtime usan exactamente este patron.

</details>

---

### Ejercicio 9 — Identificar el adapter

En este codigo, ¿cual es el adapter y que esta adaptando?

```c
FILE *log_file = fopen("/var/log/app.log", "a");

void log_message(const char *level, const char *msg) {
    time_t now = time(NULL);
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S",
             localtime(&now));
    fprintf(log_file, "[%s] [%s] %s\n", timebuf, level, msg);
    fflush(log_file);
}
```

**Prediccion**: ¿es esto un adapter, una facade, o ninguno?

<details>
<summary>Respuesta</summary>

Es un **adapter** (simple, sin struct) que traduce:

```
  Interfaz del caller:         Interfaz adaptada:
  ────────────────────         ──────────────────
  log_message(level, msg)  →   time() + localtime() + strftime()
                               fprintf(file, format, ...)
                               fflush()

  El caller dice: "loggea este mensaje con este nivel"
  El adapter traduce a: obtener timestamp, formatear,
  escribir al FILE*, flush
```

Es un adapter porque:
- Traduce una **interfaz conceptual** (log message) a otra
  (file I/O con formato)
- Oculta los detalles (time formatting, file handling, flush)

No es una facade porque:
- No simplifica un subsistema complejo, solo traduce
- Solo involucra una operacion, no un conjunto coordinado

Es el adapter mas simple posible: una funcion wrapper que
traduce parametros y delega a APIs de bajo nivel.

</details>

---

### Ejercicio 10 — Disenar un adapter

Tu sistema de notificaciones tiene esta interfaz:

```c
typedef struct {
    const char *recipient;
    const char *subject;
    const char *body;
    int         priority;  /* 0=low, 1=normal, 2=high */
} Notification;

int notify_send(const Notification *n);
```

Necesitas backends: email (SMTP), SMS (API HTTP), y Slack
(webhook). Disena la estructura del adapter (headers y structs,
no la implementacion completa).

**Prediccion**: ¿usas vtable o linking separado?

<details>
<summary>Respuesta</summary>

Vtable — porque el backend puede cambiar en runtime (ej: enviar
notificaciones de alta prioridad por SMS, normales por email):

```c
/* notifier.h */
typedef struct Notifier Notifier;

typedef struct {
    int  (*send)(Notifier *n, const Notification *notif);
    void (*destroy)(Notifier *n);
    const char *name;
} NotifierOps;

struct Notifier {
    const NotifierOps *ops;
    void              *backend_data;
};

static inline int notifier_send(Notifier *n, const Notification *notif) {
    return n->ops->send(n, notif);
}

static inline void notifier_destroy(Notifier *n) {
    n->ops->destroy(n);
}

/* Factory functions */
Notifier *notifier_email(const char *smtp_host, int smtp_port);
Notifier *notifier_sms(const char *api_key, const char *api_url);
Notifier *notifier_slack(const char *webhook_url);

/* Router que despacha segun prioridad */
typedef struct {
    Notifier *backends[3];  /* Indexado por priority */
} NotifyRouter;

NotifyRouter *notify_router_create(Notifier *low,
                                   Notifier *normal,
                                   Notifier *high);
void          notify_router_destroy(NotifyRouter *r);
int           notify_router_send(NotifyRouter *r,
                                 const Notification *n);
```

```c
/* notify_router.c */
int notify_router_send(NotifyRouter *r, const Notification *n) {
    int prio = n->priority;
    if (prio < 0) prio = 0;
    if (prio > 2) prio = 2;
    return notifier_send(r->backends[prio], n);
}
```

```c
/* main.c */
int main(void) {
    Notifier *email = notifier_email("smtp.corp.com", 587);
    Notifier *sms   = notifier_sms("key123", "https://sms-api.com");
    Notifier *slack = notifier_slack("https://hooks.slack.com/...");

    NotifyRouter *router = notify_router_create(
        email,  /* low priority → email */
        slack,  /* normal → slack */
        sms     /* high priority → SMS */
    );

    Notification n = {
        .recipient = "oncall@corp.com",
        .subject   = "Server down",
        .body      = "Production DB unreachable",
        .priority  = 2,  /* high → goes to SMS */
    };
    notify_router_send(router, &n);

    notify_router_destroy(router);
    return 0;
}
```

Vtable porque:
- Multiples backends activos simultaneamente
- El router elige backend en runtime segun prioridad
- Facil agregar un backend nuevo sin modificar notify_router
- Linking separado solo permite un backend por compilacion

</details>
