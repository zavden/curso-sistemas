# Mejora 3: Logging de requests

## Índice
1. [¿Por qué logging?](#1-por-qué-logging)
2. [Formato de log estándar](#2-formato-de-log-estándar)
3. [Generar el timestamp](#3-generar-el-timestamp)
4. [Implementar la función de log](#4-implementar-la-función-de-log)
5. [Logging thread-safe](#5-logging-thread-safe)
6. [Escribir a archivo](#6-escribir-a-archivo)
7. [Rotación de logs](#7-rotación-de-logs)
8. [Integrar con el servidor](#8-integrar-con-el-servidor)
9. [Niveles de log](#9-niveles-de-log)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. ¿Por qué logging?

Sin logs, un servidor HTTP es una caja negra. Cuando algo falla
— una página no carga, un cliente reporta un error, el servidor
se vuelve lento — no hay forma de diagnosticar el problema.

### Qué queremos registrar

```
Cada petición:
  [2026-03-20 14:30:05] 127.0.0.1 "GET /index.html HTTP/1.1" 200 1234 3ms

  Cuándo:    2026-03-20 14:30:05
  Quién:     127.0.0.1
  Qué pidió: GET /index.html HTTP/1.1
  Resultado: 200 (OK)
  Tamaño:    1234 bytes enviados
  Duración:  3ms
```

### Usos del log

```
Diagnóstico:
  "¿Por qué esta página da 404?"
  → grep 404 access.log → ver qué path se pidió

Seguridad:
  "¿Alguien intentó path traversal?"
  → grep "\.\./" access.log → ver IPs sospechosas

Rendimiento:
  "¿Qué páginas son lentas?"
  → sort por duración → encontrar bottlenecks

Estadísticas:
  "¿Cuántas visitas tuvo /about hoy?"
  → grep + wc → contar requests por path
```

---

## 2. Formato de log estándar

### Common Log Format (CLF)

El formato más extendido, usado por Apache desde los años 90.
Herramientas como GoAccess, AWStats y grep lo entienden:

```
127.0.0.1 - - [20/Mar/2026:14:30:05 +0000] "GET /index.html HTTP/1.1" 200 1234
^^^^^^^^^       ^^^^^^^^^^^^^^^^^^^^^^^^^^   ^^^^^^^^^^^^^^^^^^^^^^^^^^  ^^^ ^^^^
IP         ident user     timestamp           request line              code size
           (-)   (-)
```

- `ident`: campo de identd (obsoleto, siempre `-`)
- `user`: usuario HTTP auth (siempre `-` sin autenticación)
- Timestamp entre corchetes, formato específico
- Request line entre comillas dobles
- Status code y tamaño del body

### Combined Log Format

Extiende CLF con Referer y User-Agent:

```
127.0.0.1 - - [20/Mar/2026:14:30:05 +0000] "GET / HTTP/1.1" 200 1234 "http://example.com" "curl/7.88.1"
                                                                        ^^^^^^^^^^^^^^^^^^^^  ^^^^^^^^^^^^
                                                                        Referer               User-Agent
```

### Nuestro formato

Usaremos un formato basado en CLF pero con duración:

```
127.0.0.1 [2026-03-20 14:30:05] "GET /index.html HTTP/1.1" 200 1234 3ms
```

Más legible que CLF estricto, y la duración es útil para
diagnóstico de rendimiento.

---

## 3. Generar el timestamp

### Formato para el log

```c
#include <time.h>
#include <sys/time.h>
#include <stdio.h>

// Generar timestamp para el log
// Formato: "2026-03-20 14:30:05"
void log_timestamp(char *buf, size_t bufsize) {
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(buf, bufsize, "%Y-%m-%d %H:%M:%S", &tm);
}
```

### Con milisegundos

`time()` tiene resolución de 1 segundo. Para la duración de
requests, necesitamos mayor precisión. `gettimeofday()` da
microsegundos:

```c
#include <sys/time.h>

// Timestamp con milisegundos: "2026-03-20 14:30:05.123"
void log_timestamp_ms(char *buf, size_t bufsize) {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    struct tm tm;
    localtime_r(&tv.tv_sec, &tm);

    int n = strftime(buf, bufsize, "%Y-%m-%d %H:%M:%S", &tm);
    snprintf(buf + n, bufsize - n, ".%03ld", tv.tv_usec / 1000);
}
```

### Medir duración de un request

```c
#include <sys/time.h>

// Calcular diferencia en milisegundos
long elapsed_ms(struct timeval *start, struct timeval *end) {
    return (end->tv_sec - start->tv_sec) * 1000L +
           (end->tv_usec - start->tv_usec) / 1000;
}

// Uso en handle_client:
void handle_client(int client_fd, const char *client_ip) {
    struct timeval start, end;
    gettimeofday(&start, NULL);

    // ... parsear, servir archivo ...

    gettimeofday(&end, NULL);
    long duration = elapsed_ms(&start, &end);

    // Log con duración
    log_request(client_ip, &req, status_code, bytes_sent, duration);
}
```

### clock_gettime (alternativa moderna)

`gettimeofday` está marcado como obsoleto en POSIX.
`clock_gettime` es la alternativa moderna con resolución de
nanosegundos:

```c
#include <time.h>

long elapsed_ms_clock(struct timespec *start, struct timespec *end) {
    return (end->tv_sec - start->tv_sec) * 1000L +
           (end->tv_nsec - start->tv_nsec) / 1000000;
}

// Uso:
struct timespec start, end;
clock_gettime(CLOCK_MONOTONIC, &start);
// ... procesar ...
clock_gettime(CLOCK_MONOTONIC, &end);
long ms = elapsed_ms_clock(&start, &end);
```

`CLOCK_MONOTONIC` no se ve afectado por cambios de hora del
sistema (NTP, DST), a diferencia de `CLOCK_REALTIME` y
`gettimeofday`. Para medir duración, siempre `CLOCK_MONOTONIC`.

---

## 4. Implementar la función de log

### Estructura para capturar datos del request

```c
typedef struct {
    const char *client_ip;
    const char *method;
    const char *path;
    const char *version;
    int         status_code;
    size_t      bytes_sent;      // tamaño del body enviado
    long        duration_ms;     // duración en milisegundos
} log_entry_t;
```

### Función de log a stderr

```c
#include <stdio.h>

void log_request(const log_entry_t *entry) {
    char timestamp[64];
    log_timestamp(timestamp, sizeof(timestamp));

    fprintf(stderr,
            "%s [%s] \"%s %s %s\" %d %zu %ldms\n",
            entry->client_ip,
            timestamp,
            entry->method,
            entry->path,
            entry->version,
            entry->status_code,
            entry->bytes_sent,
            entry->duration_ms);
}
```

### Salida resultante

```
127.0.0.1 [2026-03-20 14:30:05] "GET /index.html HTTP/1.1" 200 1234 3ms
127.0.0.1 [2026-03-20 14:30:05] "GET /css/style.css HTTP/1.1" 200 456 1ms
192.168.1.50 [2026-03-20 14:30:06] "GET /noexiste HTTP/1.1" 404 123 0ms
10.0.0.3 [2026-03-20 14:30:07] "POST / HTTP/1.1" 405 18 0ms
```

### ¿Por qué stderr?

- `stdout` puede estar redirigido o buffered (line-buffered en
  terminal, fully-buffered en pipe)
- `stderr` es unbuffered por defecto — cada fprintf se escribe
  inmediatamente
- Separar: `stdout` para mensajes del servidor (startup, shutdown),
  `stderr` para el log de acceso

```bash
# stdout para mensajes, stderr para logs
./minihttpd 2>access.log
# Terminal ve: "minihttpd listening on 0.0.0.0:8080"
# access.log recibe: las líneas de log

# O separar ambos
./minihttpd >server.log 2>access.log
```

---

## 5. Logging thread-safe

Con thread pool, múltiples workers escriben al log
simultáneamente. Sin sincronización, las líneas se pueden
entrelazar:

```
Sin protección (2 workers escriben al mismo tiempo):
  127.0.0.1 [2026-03-20 14192.168.1.50 [2026-03-20 1
  4:30:05] "GET / HTTP/1.1" 20:30:05] "GET /style.css
  0 1234 3ms                  HTTP/1.1" 200 456 1ms

  → líneas corruptas, ilegibles
```

### Opción 1: mutex global

```c
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void log_request_safe(const log_entry_t *entry) {
    char timestamp[64];
    log_timestamp(timestamp, sizeof(timestamp));

    char line[1024];
    int n = snprintf(line, sizeof(line),
            "%s [%s] \"%s %s %s\" %d %zu %ldms\n",
            entry->client_ip,
            timestamp,
            entry->method,
            entry->path,
            entry->version,
            entry->status_code,
            entry->bytes_sent,
            entry->duration_ms);

    pthread_mutex_lock(&log_mutex);
    fwrite(line, 1, n, stderr);
    pthread_mutex_unlock(&log_mutex);
}
```

Construimos la línea completa en un buffer local **antes** del
lock, minimizando el tiempo que el mutex está held:

```
Sin preparar:
  lock → timestamp → snprintf → fwrite → unlock
  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  mutex held durante TODO (timestamp es lento)

Con preparar:
  timestamp → snprintf → lock → fwrite → unlock
                         ^^^^^^^^^^^^^^^^^^
                         mutex held solo para write
```

### Opción 2: write atómico

En Linux, `write()` a un fd con un buffer menor a `PIPE_BUF`
(4096 bytes) es **atómico** — el kernel no entrelaza con writes
de otros threads/procesos:

```c
void log_request_atomic(const log_entry_t *entry) {
    char timestamp[64];
    log_timestamp(timestamp, sizeof(timestamp));

    char line[1024];
    int n = snprintf(line, sizeof(line),
            "%s [%s] \"%s %s %s\" %d %zu %ldms\n",
            entry->client_ip,
            timestamp,
            entry->method,
            entry->path,
            entry->version,
            entry->status_code,
            entry->bytes_sent,
            entry->duration_ms);

    // write() atómico para n < PIPE_BUF (4096)
    write(STDERR_FILENO, line, n);
}
```

Esta opción no necesita mutex. Cada `write()` produce una línea
completa que no se entrelaza con otras. Es más simple y evita
contención entre workers.

```
Opción 1 (mutex):
  ✓ Funciona con fprintf/fwrite (buffered I/O)
  ✗ Contención del mutex bajo alta carga

Opción 2 (write atómico):
  ✓ Sin contención (no necesita mutex)
  ✓ Atómico si línea < 4096 bytes (siempre para nuestro formato)
  ✗ Solo funciona con write() (unbuffered), no fprintf
  ✗ No aplica si el fd es un socket o pipe > PIPE_BUF
```

Para nuestro servidor, la opción 2 es preferible.

---

## 6. Escribir a archivo

### Abrir el archivo de log

```c
#include <fcntl.h>

static int log_fd = -1;  // fd del archivo de log (o STDERR_FILENO)

int log_init(const char *log_path) {
    if (log_path == NULL) {
        // Sin archivo: log a stderr
        log_fd = STDERR_FILENO;
        return 0;
    }

    log_fd = open(log_path,
                  O_WRONLY | O_CREAT | O_APPEND,
                  0644);
    // O_APPEND: cada write va al final del archivo
    //           (atómico en POSIX — múltiples procesos/threads OK)
    // O_CREAT:  crear si no existe
    // 0644:     rw-r--r--

    if (log_fd == -1) {
        perror("log open");
        log_fd = STDERR_FILENO;  // fallback a stderr
        return -1;
    }

    return 0;
}

void log_close(void) {
    if (log_fd != -1 && log_fd != STDERR_FILENO) {
        close(log_fd);
        log_fd = -1;
    }
}
```

### Función de log unificada

```c
void log_request_unified(const log_entry_t *entry) {
    char timestamp[64];
    log_timestamp(timestamp, sizeof(timestamp));

    char line[1024];
    int n = snprintf(line, sizeof(line),
            "%s [%s] \"%s %s %s\" %d %zu %ldms\n",
            entry->client_ip,
            timestamp,
            entry->method,
            entry->path,
            entry->version,
            entry->status_code,
            entry->bytes_sent,
            entry->duration_ms);

    if (n > 0 && (size_t)n < sizeof(line)) {
        write(log_fd, line, n);
    }
}
```

### ¿Por qué O_APPEND?

Sin `O_APPEND`, dos threads que escriben al mismo archivo
pueden sobreescribirse mutuamente:

```
Sin O_APPEND:
  Thread A: lseek(fd, 0, SEEK_END)  → posición 1000
  Thread B: lseek(fd, 0, SEEK_END)  → posición 1000
  Thread A: write(fd, lineA, 50)    → escribe en 1000-1049
  Thread B: write(fd, lineB, 60)    → escribe en 1000-1059
  → lineB sobreescribe parte de lineA

Con O_APPEND:
  Thread A: write(fd, lineA, 50)    → kernel: seek_end + write atómico → 1000-1049
  Thread B: write(fd, lineB, 60)    → kernel: seek_end + write atómico → 1050-1109
  → sin sobreescritura
```

`O_APPEND` hace que el seek al final y el write sean una
**operación atómica** en el kernel.

---

## 7. Rotación de logs

Un archivo de log crece indefinidamente. La rotación divide el
log en archivos por fecha o tamaño.

### Rotación con señal (SIGHUP)

La convención Unix es que SIGHUP indica al daemon que reabra
sus archivos de log. `logrotate` envía SIGHUP después de rotar:

```c
static volatile sig_atomic_t reopen_log = 0;
static const char *log_path_global = NULL;

void sighup_handler(int sig) {
    (void)sig;
    reopen_log = 1;
}

void log_reopen_if_needed(void) {
    if (!reopen_log)
        return;
    reopen_log = 0;

    if (log_path_global == NULL || log_fd == STDERR_FILENO)
        return;

    int new_fd = open(log_path_global,
                      O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (new_fd == -1) {
        // No se pudo reabrir — mantener el fd actual
        return;
    }

    int old_fd = log_fd;
    log_fd = new_fd;
    close(old_fd);
}

// Instalar el handler:
void setup_log_signals(void) {
    struct sigaction sa;
    sa.sa_handler = sighup_handler;
    sa.sa_flags   = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGHUP, &sa, NULL);
}
```

### Flujo con logrotate

```bash
# /etc/logrotate.d/minihttpd
/var/log/minihttpd/access.log {
    daily
    rotate 7
    compress
    missingok
    notifempty
    postrotate
        kill -HUP $(cat /var/run/minihttpd.pid)
    endscript
}
```

```
Día 1:
  access.log (escribiendo)

logrotate ejecuta:
  1. mv access.log access.log.1
  2. kill -HUP minihttpd
     → servidor reabre access.log (nuevo archivo)
  3. gzip access.log.1

Día 2:
  access.log (nuevo, escribiendo)
  access.log.1.gz (ayer, comprimido)
```

### ¿Por qué reabrir y no simplemente seguir escribiendo?

Después de `mv access.log access.log.1`, el servidor todavía
tiene el fd abierto al **mismo inode** (ahora access.log.1).
Si no reabre, sigue escribiendo al archivo renombrado:

```
Sin reabrir:
  fd → inode 12345 (antes access.log, ahora access.log.1)
  → nuevos logs van a access.log.1, no al nuevo access.log
  → el nuevo access.log está vacío → confusión

Con reabrir (SIGHUP):
  close(old_fd)
  fd = open("access.log", ...) → nuevo inode
  → nuevos logs van al archivo correcto
```

---

## 8. Integrar con el servidor

### Modificar handle_client

Necesitamos capturar el status code y bytes enviados, que antes
no guardábamos:

```c
void handle_client(int client_fd, const char *client_ip) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int status_code = 200;
    size_t bytes_sent = 0;

    char buf[8192];
    ssize_t n = read_request(client_fd, buf, sizeof(buf));
    if (n <= 0) return;

    http_request_t req;
    if (parse_http_request(buf, &req) == -1) {
        bytes_sent = send_error(client_fd, 400);
        status_code = 400;
        goto log;
    }

    url_decode(req.path);

    if (strcmp(req.method, "GET") != 0 &&
        strcmp(req.method, "HEAD") != 0) {
        bytes_sent = send_error_with_header(client_fd, 405,
                                            "Allow: GET, HEAD\r\n");
        status_code = 405;
        goto log;
    }

    char filepath[PATH_MAX];
    snprintf(filepath, sizeof(filepath), "%s%s", DOCROOT, req.path);

    char resolved[PATH_MAX];
    if (validate_path(filepath, resolved, sizeof(resolved)) == -1) {
        bytes_sent = send_error(client_fd, 403);
        status_code = 403;
        goto log;
    }
    strncpy(filepath, resolved, sizeof(filepath) - 1);

    int idx_result = resolve_index(filepath, sizeof(filepath), req.path);

    switch (idx_result) {
        case 0:
            status_code = serve_file_cached(client_fd, filepath,
                                            &req, &bytes_sent);
            break;
        case 1: {
            char location[2048];
            snprintf(location, sizeof(location), "%s/", req.path);
            bytes_sent = send_redirect(client_fd, 301, location);
            status_code = 301;
            break;
        }
        case 2: {
            char html[65536];
            int html_len = generate_listing(req.path, filepath,
                                            html, sizeof(html));
            if (html_len == -1) {
                bytes_sent = send_error(client_fd, 500);
                status_code = 500;
            } else {
                int is_head = (strcmp(req.method, "HEAD") == 0);
                http_response_t resp = {
                    .status_code  = 200,
                    .content_type = "text/html; charset=utf-8",
                    .body         = html,
                    .body_len     = html_len,
                    .is_head      = is_head,
                };
                send_http_response(client_fd, &resp);
                bytes_sent = is_head ? 0 : html_len;
                status_code = 200;
            }
            break;
        }
        default:
            bytes_sent = send_error(client_fd, 404);
            status_code = 404;
            break;
    }

log:
    clock_gettime(CLOCK_MONOTONIC, &end);
    long duration = (end.tv_sec - start.tv_sec) * 1000L +
                    (end.tv_nsec - start.tv_nsec) / 1000000;

    log_entry_t entry = {
        .client_ip   = client_ip,
        .method      = req.method[0] ? req.method : "-",
        .path        = req.path[0] ? req.path : "-",
        .version     = req.version[0] ? req.version : "-",
        .status_code = status_code,
        .bytes_sent  = bytes_sent,
        .duration_ms = duration,
    };
    log_request_unified(&entry);
}
```

### Modificar send_error para retornar bytes

```c
// Antes:
void send_error(int fd, int code);

// Después: retornar bytes del body enviado
size_t send_error(int fd, int code) {
    const char *reason = status_reason(code);
    char body[512];
    int body_len = snprintf(body, sizeof(body),
        "<!DOCTYPE html>\n"
        "<html><head><title>%d %s</title></head>\n"
        "<body><h1>%d %s</h1></body></html>\n",
        code, reason, code, reason);

    send_response(fd, code, "text/html; charset=utf-8",
                  body, body_len);
    return body_len;
}
```

### Modificar main

```c
int main(int argc, char *argv[]) {
    int port = 8080;
    const char *docroot  = "./www";
    const char *log_path = NULL;  // NULL = stderr
    int num_workers = 8;

    // Parsear argumentos
    if (argc >= 2) port        = atoi(argv[1]);
    if (argc >= 3) docroot     = argv[2];
    if (argc >= 4) num_workers = atoi(argv[3]);
    if (argc >= 5) log_path    = argv[4];

    // Inicializar log
    log_init(log_path);
    setup_log_signals();  // SIGHUP para rotación

    // ... socket, pool, accept loop (sin cambios) ...

    // Shutdown
    pool_destroy(&pool);
    close(listener);
    log_close();

    return 0;
}
```

### Uso

```bash
# Log a stderr (terminal)
./minihttpd 8080 ./www 8

# Log a archivo
./minihttpd 8080 ./www 8 access.log

# Log a archivo, stderr para errores del servidor
./minihttpd 8080 ./www 8 access.log 2>error.log
```

---

## 9. Niveles de log

Además del log de acceso (un registro por petición), es útil
tener un log de errores y debug con niveles de severidad:

### Definición de niveles

```c
typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
} log_level_t;

static log_level_t current_level = LOG_INFO;

static const char *level_names[] = {
    "DEBUG", "INFO", "WARN", "ERROR"
};

void set_log_level(log_level_t level) {
    current_level = level;
}
```

### Función de log con nivel

```c
#include <stdarg.h>

void server_log(log_level_t level, const char *fmt, ...) {
    if (level < current_level)
        return;  // filtrar por nivel

    char timestamp[64];
    log_timestamp(timestamp, sizeof(timestamp));

    char message[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    char line[1200];
    int n = snprintf(line, sizeof(line),
                     "[%s] [%s] %s\n",
                     timestamp, level_names[level], message);

    write(STDERR_FILENO, line, n);
}
```

### Uso

```c
server_log(LOG_INFO, "Listening on 0.0.0.0:%d", port);
server_log(LOG_INFO, "Document root: %s", docroot);
server_log(LOG_INFO, "Workers: %d", num_workers);

server_log(LOG_WARN, "Queue full, rejecting connection");

server_log(LOG_ERROR, "Failed to open %s: %s",
           filepath, strerror(errno));

server_log(LOG_DEBUG, "Parsed request: %s %s %s",
           req.method, req.path, req.version);
```

### Salida

```
[2026-03-20 14:30:00] [INFO] Listening on 0.0.0.0:8080
[2026-03-20 14:30:00] [INFO] Document root: /home/user/www
[2026-03-20 14:30:00] [INFO] Workers: 8
[2026-03-20 14:30:05] [DEBUG] Parsed request: GET /index.html HTTP/1.1
[2026-03-20 14:31:00] [WARN] Queue full, rejecting connection
[2026-03-20 14:31:05] [ERROR] Failed to open ./www/broken: No such file or directory
```

### Separar access log y error log

```
access.log  ← log_request_unified()  (un registro por petición HTTP)
error.log   ← server_log()           (mensajes del servidor con niveles)

Es la misma distinción que nginx:
  /var/log/nginx/access.log
  /var/log/nginx/error.log
```

---

## 10. Errores comunes

### Error 1: fprintf sin lock en multihilo

```c
// MAL: fprintf no es atómico entre threads
fprintf(stderr, "%s %s %d\n", ip, path, code);
// Thread A escribe: "127.0.0.1 /index"
// Thread B escribe: ".html 200\n192.168.1.1 /style.css 200\n"
// → líneas entrelazadas
```

**Solución**: construir la línea completa en buffer local, luego
un solo `write()`:

```c
char line[1024];
int n = snprintf(line, sizeof(line), "%s %s %d\n", ip, path, code);
write(STDERR_FILENO, line, n);
```

### Error 2: usar CLOCK_REALTIME para duración

```c
// MAL: CLOCK_REALTIME puede saltar con NTP
clock_gettime(CLOCK_REALTIME, &start);
// ... NTP ajusta el reloj -2 segundos ...
clock_gettime(CLOCK_REALTIME, &end);
// → duración negativa o incorrecta
```

**Solución**: `CLOCK_MONOTONIC` para medir intervalos:

```c
clock_gettime(CLOCK_MONOTONIC, &start);
// ... procesar ...
clock_gettime(CLOCK_MONOTONIC, &end);
// → siempre avanza, nunca retrocede
```

### Error 3: no manejar SIGHUP para rotación

```c
// MAL: el fd apunta al inode viejo después de logrotate
int log_fd = open("access.log", O_WRONLY | O_APPEND | O_CREAT, 0644);
// logrotate hace: mv access.log access.log.1
// el servidor sigue escribiendo al inode de access.log.1
// el nuevo access.log está vacío → logs perdidos
```

**Solución**: handler de SIGHUP que reabra el archivo:

```c
// SIGHUP → reopen_log = 1
// En el REPL/loop: log_reopen_if_needed()
```

### Error 4: log sin timestamp

```c
// MAL: log sin contexto temporal
fprintf(stderr, "%s %s %d\n", ip, path, code);
// "127.0.0.1 /index.html 200"
// ¿Cuándo ocurrió? Imposible correlacionar con otros eventos
```

**Solución**: siempre incluir timestamp:

```c
char ts[64];
log_timestamp(ts, sizeof(ts));
snprintf(line, size, "%s [%s] \"%s %s\" %d\n", ip, ts, method, path, code);
```

### Error 5: no abrir con O_APPEND

```c
// MAL: abrir sin O_APPEND
int fd = open("access.log", O_WRONLY | O_CREAT, 0644);
// Con fork: el padre y el hijo comparten el offset
//   → sobreescritura si ambos escriben
// Con threads: el offset se comparte
//   → un thread puede sobreescribir lo que otro escribió
```

**Solución**: siempre `O_APPEND` para archivos de log:

```c
int fd = open("access.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
// seek + write es atómico con O_APPEND
```

---

## 11. Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│                 Logging de Requests                          │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  FORMATO DE LOG:                                             │
│    IP [timestamp] "METHOD PATH VERSION" CODE BYTES DURATIONms│
│    127.0.0.1 [2026-03-20 14:30:05] "GET / HTTP/1.1" 200 ... │
│                                                              │
│  TIMESTAMP:                                                  │
│    localtime_r + strftime("%Y-%m-%d %H:%M:%S")              │
│    (thread-safe, hora local para el usuario)                 │
│                                                              │
│  MEDIR DURACIÓN:                                             │
│    clock_gettime(CLOCK_MONOTONIC, &start)                    │
│    ... procesar ...                                          │
│    clock_gettime(CLOCK_MONOTONIC, &end)                      │
│    (MONOTONIC: no afectado por NTP/DST)                      │
│                                                              │
│  THREAD SAFETY:                                              │
│    Opción A: snprintf en buffer local → mutex → fwrite       │
│    Opción B: snprintf en buffer local → write() atómico      │
│    (write < PIPE_BUF=4096 es atómico en Linux)               │
│                                                              │
│  ARCHIVO DE LOG:                                             │
│    open(path, O_WRONLY | O_CREAT | O_APPEND, 0644)          │
│    O_APPEND: seek+write atómico, sin sobreescritura          │
│                                                              │
│  ROTACIÓN (SIGHUP):                                          │
│    logrotate: mv access.log access.log.1                     │
│    kill -HUP pid → handler pone flag                         │
│    loop principal: close(old_fd) → open(nuevo access.log)    │
│                                                              │
│  NIVELES:                                                    │
│    LOG_DEBUG  detalles internos (deshabilitado en prod)       │
│    LOG_INFO   eventos normales (startup, shutdown)           │
│    LOG_WARN   situaciones anómalas (cola llena)              │
│    LOG_ERROR  fallos (open, read, permisos)                  │
│                                                              │
│  DOS LOGS SEPARADOS:                                         │
│    access.log  un registro por petición HTTP                 │
│    error.log   mensajes del servidor con niveles             │
│                                                              │
│  STDERR VS ARCHIVO:                                          │
│    stderr: sin buffering, bueno para terminal/desarrollo     │
│    archivo: persistente, rotable, bueno para producción      │
│    Fallback: si open falla → usar stderr                     │
│                                                              │
│  DATOS A CAPTURAR:                                           │
│    client_ip, method, path, version (del parser)             │
│    status_code (de la lógica de respuesta)                   │
│    bytes_sent (retorno de send_response)                     │
│    duration_ms (clock_gettime antes/después)                 │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: access log con duración

Implementa el logging completo de peticiones con medición de
duración.

**Pasos**:
1. Implementa `log_timestamp()` con localtime_r + strftime
2. Implementa `elapsed_ms()` con clock_gettime(CLOCK_MONOTONIC)
3. Implementa `log_request()` que escriba una línea formateada
   a stderr con write() (atómico)
4. Modifica `handle_client()` para capturar status_code,
   bytes_sent, y medir start/end
5. Prueba: `curl localhost:8080/` y verifica la línea de log

**Predicción antes de ejecutar**: si sirves un archivo de 1MB,
¿la duración será mayor que para un archivo de 1KB? ¿La
diferencia será en milisegundos o microsegundos en localhost?
¿Qué componente domina: leer el archivo o escribir al socket?

> **Pregunta de reflexión**: `clock_gettime(CLOCK_MONOTONIC)`
> mide el tiempo transcurrido incluyendo el tiempo que el thread
> estuvo dormido (esperando I/O). Si quisieras medir solo el
> tiempo de CPU usado por el thread, ¿qué clock usarías?
> (Pista: CLOCK_THREAD_CPUTIME_ID.) ¿Cuándo sería útil cada
> medida?

### Ejercicio 2: log a archivo con rotación

Implementa logging a archivo con soporte de rotación via SIGHUP.

**Pasos**:
1. Implementa `log_init(path)` con open(O_WRONLY|O_CREAT|O_APPEND)
2. Implementa `log_close()` que cierre el fd
3. Implementa el handler de SIGHUP que ponga un flag
4. Implementa `log_reopen_if_needed()` que reabra el archivo
5. Prueba: lanzar servidor con log a archivo, escribir datos,
   `mv access.log access.log.1`, `kill -HUP $(pidof minihttpd)`,
   verificar que nuevos logs van al nuevo access.log

**Predicción antes de ejecutar**: después de `mv access.log
access.log.1` pero **antes** del SIGHUP, ¿a qué archivo van
los logs? ¿Al nuevo access.log (vacío) o a access.log.1?
¿Por qué?

> **Pregunta de reflexión**: la rotación con SIGHUP tiene
> una ventana entre el `mv` y el `kill -HUP` donde los logs
> van al archivo viejo. ¿Cuántas líneas de log se podrían
> perder? ¿Existe una forma de rotación sin esta ventana?
> (Pista: `copytruncate` en logrotate copia y trunca sin
> renombrar — ¿cuáles son sus desventajas?)

### Ejercicio 3: log con niveles y filtro

Implementa un sistema de logging con niveles para mensajes
del servidor.

**Pasos**:
1. Define el enum `log_level_t` (DEBUG, INFO, WARN, ERROR)
2. Implementa `server_log(level, fmt, ...)` con va_list
3. Implementa `set_log_level()` para filtrar por nivel
4. Añade logs en puntos clave: startup (INFO), cada request
   parseado (DEBUG), errores de open/stat (ERROR), cola llena (WARN)
5. Prueba con nivel DEBUG (todo visible) vs INFO (sin debug)

**Predicción antes de ejecutar**: con nivel INFO, ¿las líneas
DEBUG se omiten o se escriben pero no se muestran? ¿La
comprobación de nivel ocurre antes o después de formatear
el mensaje con vsnprintf? ¿Importa para el rendimiento?

> **Pregunta de reflexión**: nuestro `server_log` usa
> `vsnprintf` para formatear el mensaje incluso si el nivel
> está filtrado (lo descartamos después). ¿Cómo optimizarías
> esto para no hacer el formato si el mensaje no se va a
> escribir? En C no hay evaluación lazy de argumentos — ¿cómo
> lo resuelven frameworks de logging como syslog o log4j?
> (Pista: macros con `do { if (level >= ...) ... } while(0)`.)
