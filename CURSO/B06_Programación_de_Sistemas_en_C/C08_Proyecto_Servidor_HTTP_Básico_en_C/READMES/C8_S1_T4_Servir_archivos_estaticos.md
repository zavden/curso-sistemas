# Servir archivos estáticos

## Índice
1. [Mapear URI a filesystem](#1-mapear-uri-a-filesystem)
2. [Document root y resolución de path](#2-document-root-y-resolución-de-path)
3. [Seguridad: path traversal](#3-seguridad-path-traversal)
4. [Leer y servir un archivo](#4-leer-y-servir-un-archivo)
5. [Index por defecto](#5-index-por-defecto)
6. [Servir archivos grandes con sendfile](#6-servir-archivos-grandes-con-sendfile)
7. [Cache con Last-Modified / If-Modified-Since](#7-cache-con-last-modified--if-modified-since)
8. [handle_client completo](#8-handle_client-completo)
9. [Servidor completo integrado](#9-servidor-completo-integrado)
10. [Probar el servidor](#10-probar-el-servidor)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. Mapear URI a filesystem

El servidor de archivos estáticos traduce la URI del request a
una ruta en el filesystem:

```
URI del cliente          Document root        Archivo en disco
─────────────────       ────────────────      ──────────────────
GET /                   ./www               → ./www/index.html
GET /index.html         ./www               → ./www/index.html
GET /css/style.css      ./www               → ./www/css/style.css
GET /img/logo.png       ./www               → ./www/img/logo.png
GET /about              ./www               → ./www/about/index.html
```

### Concepto de document root

El document root es el directorio base del servidor. Toda URI
se resuelve relativa a este directorio:

```
Document root: /srv/www

Filesystem:
  /srv/www/
  ├── index.html
  ├── css/
  │   └── style.css
  ├── js/
  │   └── app.js
  └── img/
      ├── logo.png
      └── banner.jpg

URI             → Path resuelto
/               → /srv/www/index.html
/css/style.css  → /srv/www/css/style.css
/img/logo.png   → /srv/www/img/logo.png
/../../etc/passwd → RECHAZADO (path traversal)
```

---

## 2. Document root y resolución de path

### Construir el filepath

```c
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

#define DOCROOT "./www"

// Construir el path del archivo a partir de la URI
// Retorna 0 en éxito, -1 si el path es inválido
int resolve_path(const char *uri, char *filepath, size_t filepath_size) {
    // Concatenar docroot + uri
    int n = snprintf(filepath, filepath_size, "%s%s", DOCROOT, uri);
    if (n < 0 || (size_t)n >= filepath_size) {
        return -1;  // path demasiado largo
    }
    return 0;
}
```

### ¿Por qué no basta con concatenar?

La concatenación simple es **peligrosa**:

```c
// DOCROOT = "./www"
// uri     = "/../../../etc/passwd"
// filepath = "./www/../../../etc/passwd"
// Resuelto: /etc/passwd  ← FUERA del document root
```

Necesitamos validar que el path resuelto **está dentro** del
document root. Esto se cubre en la siguiente sección.

---

## 3. Seguridad: path traversal

Path traversal (directory traversal) es un ataque donde el
cliente usa `..` para escapar del document root y leer archivos
arbitrarios del sistema:

```
GET /../../../etc/passwd HTTP/1.1
GET /..%2f..%2f..%2fetc/passwd HTTP/1.1    (con percent-encoding)
GET /css/../../etc/shadow HTTP/1.1
```

### La solución: realpath

`realpath()` resuelve un path a su forma **canónica** absoluta,
eliminando `.`, `..`, y symlinks. Después verificamos que el
resultado empieza con el document root:

```c
#include <stdlib.h>
#include <string.h>

// Verificar que filepath está dentro del document root
// Retorna 0 si es seguro, -1 si es path traversal
int validate_path(const char *filepath, char *resolved, size_t resolved_size) {
    // Resolver el document root a path absoluto
    char docroot_resolved[PATH_MAX];
    if (realpath(DOCROOT, docroot_resolved) == NULL) {
        return -1;
    }
    size_t docroot_len = strlen(docroot_resolved);

    // Resolver el filepath
    // realpath requiere que el archivo exista (o el directorio padre)
    if (realpath(filepath, resolved) == NULL) {
        // El archivo no existe — podría ser un 404 legítimo
        // Verificar al menos el directorio padre
        char dir[PATH_MAX];
        strncpy(dir, filepath, sizeof(dir) - 1);
        dir[sizeof(dir) - 1] = '\0';

        // Extraer directorio padre
        char *last_slash = strrchr(dir, '/');
        if (last_slash) *last_slash = '\0';

        char dir_resolved[PATH_MAX];
        if (realpath(dir, dir_resolved) == NULL) {
            return -1;  // ni el directorio existe
        }

        // Verificar que el directorio padre está en docroot
        if (strncmp(dir_resolved, docroot_resolved, docroot_len) != 0) {
            return -1;  // path traversal
        }

        // Reconstruir el path con el directorio resuelto
        if (last_slash) {
            snprintf(resolved, resolved_size, "%s/%s",
                     dir_resolved, last_slash + 1);
        }
        return 0;  // seguro, pero el archivo puede no existir (404)
    }

    // El archivo existe — verificar que está dentro del docroot
    if (strncmp(resolved, docroot_resolved, docroot_len) != 0) {
        return -1;  // path traversal
    }

    return 0;
}
```

### ¿Por qué realpath y no solo buscar ".."?

Buscar `..` en la string no es suficiente:

```
Filtro simple (buscar ".."):
  "/../etc/passwd"          ← detectado ✓
  "/%2e%2e/etc/passwd"      ← NO detectado (después de url_decode: "/../etc/passwd")
  "/css/../../etc/passwd"   ← detectado ✓
  "/symlink_to_etc/passwd"  ← NO detectado (symlink apunta fuera del docroot)

realpath():
  Resuelve TODOS los casos: .., symlinks, paths relativos
  Si el resultado no empieza con docroot → rechazar
```

### Diagrama de validación

```
URI: "/css/../../etc/passwd"

1. url_decode()     → "/css/../../etc/passwd"   (sin cambio)
2. snprintf()       → "./www/css/../../etc/passwd"
3. realpath()       → "/etc/passwd"
4. strncmp con      → "/etc/passwd" no empieza con "/home/.../www"
   docroot_resolved → RECHAZADO → 403 Forbidden

URI: "/css/style.css"

1. url_decode()     → "/css/style.css"
2. snprintf()       → "./www/css/style.css"
3. realpath()       → "/home/user/project/www/css/style.css"
4. strncmp con      → empieza con "/home/user/project/www"
   docroot_resolved → ACEPTADO → servir archivo
```

---

## 4. Leer y servir un archivo

Una vez validado el path, leemos el archivo y lo enviamos como
respuesta HTTP.

### Con read en buffer

Para archivos pequeños-medianos (< unos pocos MB):

```c
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

// Servir un archivo al cliente
// Retorna 0 en éxito, -1 en error
int serve_file(int client_fd, const char *filepath,
               const http_request_t *req)
{
    // Obtener info del archivo
    struct stat st;
    if (stat(filepath, &st) == -1) {
        send_error(client_fd, 404);
        return -1;
    }

    // ¿Es un archivo regular?
    if (!S_ISREG(st.st_mode)) {
        send_error(client_fd, 403);
        return -1;
    }

    // ¿Permiso de lectura?
    if (access(filepath, R_OK) == -1) {
        send_error(client_fd, 403);
        return -1;
    }

    // Abrir el archivo
    int file_fd = open(filepath, O_RDONLY);
    if (file_fd == -1) {
        send_error(client_fd, 500);
        return -1;
    }

    size_t file_size = st.st_size;
    int is_head = (strcmp(req->method, "HEAD") == 0);

    // Determinar Content-Type
    const char *mime = get_mime_type(filepath);

    // Enviar headers
    http_response_t resp = {
        .status_code    = 200,
        .content_type   = mime,
        .body           = NULL,
        .body_len       = file_size,
        .is_head        = is_head,
        .last_modified  = st.st_mtime,
    };

    char headers[4096];
    int header_len = build_full_headers(headers, sizeof(headers), &resp);
    write_all(client_fd, headers, header_len);

    // Enviar body (solo si no es HEAD)
    if (!is_head) {
        char buf[8192];
        ssize_t n;
        while ((n = read(file_fd, buf, sizeof(buf))) > 0) {
            if (write_all(client_fd, buf, n) == -1)
                break;
        }
    }

    close(file_fd);
    return 0;
}
```

### Diagrama del flujo de datos

```
Disco                 Kernel               User space              Socket
┌──────┐         ┌──────────────┐      ┌───────────────┐      ┌──────────┐
│archivo│──read──▶│  page cache  │─────▶│  buf[8192]    │─write▶│ client_fd│
│       │         │              │      │               │      │          │
└──────┘         └──────────────┘      └───────────────┘      └──────────┘
                                             ▲    │
                                    read()   │    │  write()
                                    copia al │    │  copia al
                                    buffer   │    │  kernel
                                    del user │    │
                                             │    ▼
                                        2 copias de datos
```

Con `read()` + `write()`, los datos se copian **dos veces**:
disco → buffer de usuario → socket del kernel. Para archivos
grandes, esto es ineficiente. `sendfile()` lo resuelve (sección 6).

---

## 5. Index por defecto

Cuando la URI apunta a un directorio, servimos `index.html`
automáticamente:

```
GET /              → ./www/index.html
GET /about/        → ./www/about/index.html
GET /about         → redirigir a /about/ (con trailing slash)
```

### Implementación

```c
#include <sys/stat.h>

// Resolver la ruta final, manejando directorios e index.html
// filepath: buffer de entrada/salida con el path a resolver
// Retorna: 0 si resolvió un archivo, 1 si necesita redirect, -1 si error
int resolve_index(char *filepath, size_t filepath_size,
                  const char *uri)
{
    struct stat st;
    if (stat(filepath, &st) == -1) {
        return -1;  // no existe → 404
    }

    if (S_ISDIR(st.st_mode)) {
        // Es un directorio

        // ¿La URI termina en '/'?
        size_t uri_len = strlen(uri);
        if (uri_len == 0 || uri[uri_len - 1] != '/') {
            return 1;  // necesita redirect: /about → /about/
        }

        // Intentar index.html
        size_t len = strlen(filepath);
        // Asegurar trailing slash en filepath
        if (filepath[len - 1] != '/') {
            if (len + 1 >= filepath_size) return -1;
            filepath[len] = '/';
            filepath[len + 1] = '\0';
            len++;
        }

        if (len + 10 >= filepath_size) return -1;  // "index.html" = 10 chars
        strcat(filepath, "index.html");

        if (stat(filepath, &st) == -1) {
            return -1;  // index.html no existe → 404 (o directory listing)
        }

        return 0;  // index.html encontrado
    }

    return 0;  // es un archivo regular
}
```

### Redirect de directorio sin trailing slash

Los servidores HTTP redirigen `/about` a `/about/` con un 301.
¿Por qué? Sin el trailing slash, las URLs relativas en el HTML
se resuelven incorrectamente:

```
URI: /about    (sin slash)
HTML contiene: <link href="style.css">
Navegador resuelve: /style.css  ← busca en la raíz, INCORRECTO

URI: /about/   (con slash)
HTML contiene: <link href="style.css">
Navegador resuelve: /about/style.css  ← correcto
```

```c
// En handle_client, después de resolve_index:
int result = resolve_index(filepath, sizeof(filepath), req.path);
if (result == 1) {
    // Redirect: añadir trailing slash
    char location[MAX_PATH + 2];
    snprintf(location, sizeof(location), "%s/", req.path);
    send_redirect(client_fd, 301, location);
    return;
}
```

---

## 6. Servir archivos grandes con sendfile

`sendfile()` es una syscall de Linux que transfiere datos
directamente del file descriptor al socket **sin pasar por user
space**:

```
read() + write():
  Disco → kernel buffer → user buffer → kernel socket buffer → red
          ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
          2 copias, 2 context switches extra

sendfile():
  Disco → kernel buffer → kernel socket buffer → red
                         ^^^^^^^^^^^^^^^^^^^^^
                         1 copia, todo en kernel
```

### Implementación

```c
#include <sys/sendfile.h>

int serve_file_sendfile(int client_fd, const char *filepath,
                        const http_request_t *req)
{
    struct stat st;
    if (stat(filepath, &st) == -1) {
        send_error(client_fd, 404);
        return -1;
    }

    if (!S_ISREG(st.st_mode)) {
        send_error(client_fd, 403);
        return -1;
    }

    int file_fd = open(filepath, O_RDONLY);
    if (file_fd == -1) {
        send_error(client_fd, 500);
        return -1;
    }

    size_t file_size = st.st_size;
    int is_head = (strcmp(req->method, "HEAD") == 0);
    const char *mime = get_mime_type(filepath);

    // Enviar headers
    http_response_t resp = {
        .status_code   = 200,
        .content_type  = mime,
        .body          = NULL,
        .body_len      = file_size,
        .is_head       = is_head,
        .last_modified = st.st_mtime,
    };
    char headers[4096];
    int header_len = build_full_headers(headers, sizeof(headers), &resp);
    write_all(client_fd, headers, header_len);

    // Enviar body con sendfile (solo si no es HEAD)
    if (!is_head) {
        off_t offset = 0;
        size_t remaining = file_size;

        while (remaining > 0) {
            ssize_t sent = sendfile(client_fd, file_fd,
                                    &offset, remaining);
            if (sent == -1) {
                if (errno == EINTR) continue;
                if (errno == EAGAIN) continue;
                break;  // error real (cliente desconectó, etc.)
            }
            remaining -= sent;
        }
    }

    close(file_fd);
    return 0;
}
```

### ¿Cuándo usar sendfile vs read+write?

```
sendfile():
  ✓ Archivos estáticos (HTML, CSS, imágenes, PDFs)
  ✓ Archivos grandes (eficiencia de copia)
  ✓ Linux-specific (no POSIX, pero existe en BSDs con otra API)
  ✗ No puedes modificar los datos antes de enviar

read() + write():
  ✓ Cuando necesitas procesar los datos (templates, compresión)
  ✓ Portable (POSIX)
  ✓ Cuando necesitas respuestas generadas dinámicamente
  ✗ Dos copias de datos → más lento para archivos grandes
```

Para nuestro servidor de archivos estáticos, `sendfile()` es
ideal. Para respuestas generadas (error pages, directory listing),
usamos `write()`.

---

## 7. Cache con Last-Modified / If-Modified-Since

El caching evita transferir archivos que el cliente ya tiene:

```
Primera petición:
  Cliente → GET /style.css
  Servidor → 200 OK
             Last-Modified: Wed, 15 Mar 2026 08:30:00 GMT
             [body: 10KB de CSS]

Segunda petición (el navegador envía la fecha de la última vez):
  Cliente → GET /style.css
            If-Modified-Since: Wed, 15 Mar 2026 08:30:00 GMT
  Servidor → (archivo no cambió desde esa fecha)
           → 304 Not Modified
             [sin body — el navegador usa su cache]
```

### Implementación

```c
#include <time.h>

// Parsear una fecha HTTP → time_t
// Formato: "Wed, 15 Mar 2026 08:30:00 GMT"
time_t parse_http_date(const char *date_str) {
    struct tm tm;
    memset(&tm, 0, sizeof(tm));

    if (strptime(date_str, "%a, %d %b %Y %H:%M:%S GMT", &tm) == NULL) {
        return -1;
    }

    return timegm(&tm);  // convertir struct tm (UTC) a time_t
}

// Verificar si el archivo fue modificado desde la fecha dada
// Retorna 1 si fue modificado, 0 si no
int is_modified_since(time_t file_mtime, const char *ims_header) {
    if (ims_header == NULL) {
        return 1;  // sin header → siempre enviar
    }

    time_t ims_time = parse_http_date(ims_header);
    if (ims_time == -1) {
        return 1;  // fecha inválida → enviar
    }

    // HTTP dates tienen resolución de 1 segundo
    // Si el archivo fue modificado DESPUÉS de ims_time → modificado
    return file_mtime > ims_time;
}
```

### Integración en serve_file

```c
int serve_file_cached(int client_fd, const char *filepath,
                      const http_request_t *req)
{
    struct stat st;
    if (stat(filepath, &st) == -1) {
        send_error(client_fd, 404);
        return -1;
    }

    // Verificar If-Modified-Since
    const char *ims = get_header(req, "If-Modified-Since");
    if (!is_modified_since(st.st_mtime, ims)) {
        // No modificado → 304 sin body
        http_response_t resp = {
            .status_code   = 304,
            .content_type  = get_mime_type(filepath),
            .body          = NULL,
            .body_len      = 0,
            .last_modified = st.st_mtime,
        };
        char headers[4096];
        int hlen = build_full_headers(headers, sizeof(headers), &resp);
        write_all(client_fd, headers, hlen);
        return 0;
    }

    // Modificado (o sin cache) → servir normalmente
    return serve_file_sendfile(client_fd, filepath, req);
}
```

### Diagrama del flujo de cache

```
                    ┌──────────────────────┐
                    │ If-Modified-Since     │
                    │ header presente?      │
                    └──────────┬───────────┘
                          ┌────┴────┐
                          │         │
                         Sí        No
                          │         │
                          ▼         │
              ┌─────────────────┐   │
              │ archivo.mtime   │   │
              │ > ims_time ?    │   │
              └────────┬────────┘   │
                  ┌────┴────┐       │
                  │         │       │
                 Sí        No       │
                  │         │       │
                  │         ▼       │
                  │    304 Not      │
                  │    Modified     │
                  │    (sin body)   │
                  │                 │
                  ▼                 ▼
            200 OK + body completo
```

---

## 8. handle_client completo

Unimos todos los tópicos: parser, response, archivos, seguridad,
cache:

```c
void handle_client(int client_fd, const char *client_ip) {
    char buf[8192];

    // 1. Leer petición
    ssize_t n = read_request(client_fd, buf, sizeof(buf));
    if (n <= 0) return;

    // 2. Parsear
    http_request_t req;
    if (parse_http_request(buf, &req) == -1) {
        send_error(client_fd, 400);
        return;
    }

    // 3. Decodificar URL
    url_decode(req.path);

    // 4. Verificar método
    if (strcmp(req.method, "GET") != 0 &&
        strcmp(req.method, "HEAD") != 0) {
        send_error_with_header(client_fd, 405, "Allow: GET, HEAD\r\n");
        return;
    }

    // 5. Construir filepath
    char filepath[PATH_MAX];
    snprintf(filepath, sizeof(filepath), "%s%s", DOCROOT, req.path);

    // 6. Validar path (seguridad)
    char resolved[PATH_MAX];
    if (validate_path(filepath, resolved, sizeof(resolved)) == -1) {
        send_error(client_fd, 403);
        return;
    }
    strncpy(filepath, resolved, sizeof(filepath) - 1);

    // 7. Resolver directorio / index.html
    int idx_result = resolve_index(filepath, sizeof(filepath), req.path);
    if (idx_result == 1) {
        // Redirect: directorio sin trailing slash
        char location[MAX_PATH + 2];
        snprintf(location, sizeof(location), "%s/", req.path);
        send_redirect(client_fd, 301, location);
        return;
    }
    if (idx_result == -1) {
        send_error(client_fd, 404);
        return;
    }

    // 8. Log
    printf("[%s] %s %s → %s\n",
           client_ip, req.method, req.path, filepath);

    // 9. Servir archivo (con cache)
    serve_file_cached(client_fd, filepath, &req);
}
```

### Flujo visual completo

```
Request: "GET /css/../img/logo.png HTTP/1.1\r\n..."

  ┌─────────────┐
  │ read_request │ → buf = "GET /css/../img/logo.png HTTP/1.1\r\n..."
  └──────┬──────┘
         ▼
  ┌─────────────┐
  │ parse_http_  │ → method="GET", path="/css/../img/logo.png"
  │ request      │
  └──────┬──────┘
         ▼
  ┌─────────────┐
  │ url_decode   │ → path="/css/../img/logo.png" (sin cambio)
  └──────┬──────┘
         ▼
  ┌─────────────┐
  │ método OK?   │ → GET ✓
  └──────┬──────┘
         ▼
  ┌─────────────┐
  │ snprintf     │ → filepath="./www/css/../img/logo.png"
  └──────┬──────┘
         ▼
  ┌─────────────┐
  │validate_path │ → realpath → "/home/user/www/img/logo.png"
  │              │   strncmp con docroot → OK ✓
  └──────┬──────┘
         ▼
  ┌──────────────┐
  │resolve_index  │ → es archivo regular → OK
  └──────┬───────┘
         ▼
  ┌──────────────┐
  │serve_file_   │ → stat → MIME → headers → sendfile → close
  │cached        │
  └──────────────┘
```

---

## 9. Servidor completo integrado

El servidor completo que une T01 (listener), T02 (parser),
T03 (response) y T04 (archivos):

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits.h>

// ─── Configuración ──────────────────────────────────
#define PORT     8080
#define BACKLOG  128
#define DOCROOT  "./www"
#define BUFSIZE  8192

// ─── Estructuras (de T02 y T03) ─────────────────────
// ... http_request_t, http_header_t, mime_entry_t ...
// ... (definidos en tópicos anteriores) ...

// ─── Funciones de utilidad ──────────────────────────
// write_all(), format_http_date(), status_reason(),
// get_mime_type(), url_decode(), get_header()
// ... (definidos en tópicos anteriores) ...

// ─── Parser (T02) ───────────────────────────────────
// read_request(), parse_request_line(), parse_headers(),
// parse_http_request()
// ... (definidos en T02) ...

// ─── Response (T03) ─────────────────────────────────
// build_full_headers(), send_error(), send_redirect()
// ... (definidos en T03) ...

// ─── Archivos (T04) ─────────────────────────────────
// validate_path(), resolve_index(), serve_file_cached()
// ... (definidos arriba) ...

// ─── Handler principal ──────────────────────────────
// handle_client()  (sección 8)

// ─── Main ───────────────────────────────────────────
static volatile sig_atomic_t running = 1;

void shutdown_handler(int sig) {
    (void)sig;
    running = 0;
}

int main(int argc, char *argv[]) {
    int port = PORT;
    const char *docroot = DOCROOT;

    // Argumentos opcionales
    if (argc >= 2) port    = atoi(argv[1]);
    if (argc >= 3) docroot = argv[2];

    // Verificar que docroot existe
    struct stat st;
    if (stat(docroot, &st) == -1 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Document root '%s' does not exist\n", docroot);
        return 1;
    }

    // Crear listener
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == -1) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(port),
        .sin_addr.s_addr = INADDR_ANY,
    };

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(listener);
        return 1;
    }

    if (listen(listener, BACKLOG) == -1) {
        perror("listen");
        close(listener);
        return 1;
    }

    // Señales
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags   = SA_NOCLDWAIT;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, NULL);

    struct sigaction sa_shut;
    sa_shut.sa_handler = shutdown_handler;
    sa_shut.sa_flags   = 0;
    sigemptyset(&sa_shut.sa_mask);
    sigaction(SIGINT,  &sa_shut, NULL);
    sigaction(SIGTERM, &sa_shut, NULL);

    char docroot_abs[PATH_MAX];
    realpath(docroot, docroot_abs);
    printf("minihttpd listening on 0.0.0.0:%d\n", port);
    printf("Document root: %s\n", docroot_abs);
    printf("Press Ctrl+C to stop.\n\n");

    // Accept loop
    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(listener,
                               (struct sockaddr *)&client_addr,
                               &client_len);
        if (client_fd == -1) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr,
                  client_ip, sizeof(client_ip));

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            close(client_fd);
            continue;
        }

        if (pid == 0) {
            close(listener);
            handle_client(client_fd, client_ip);
            close(client_fd);
            _exit(0);
        }

        close(client_fd);
    }

    printf("\nShutting down.\n");
    close(listener);
    return 0;
}
```

### Crear el document root de prueba

```bash
mkdir -p www/css www/js www/img

cat > www/index.html << 'HTML'
<!DOCTYPE html>
<html>
<head>
    <title>minihttpd</title>
    <link rel="stylesheet" href="css/style.css">
</head>
<body>
    <h1>It works!</h1>
    <p>minihttpd is serving files.</p>
    <img src="img/test.svg" alt="test">
</body>
</html>
HTML

cat > www/css/style.css << 'CSS'
body {
    font-family: sans-serif;
    max-width: 600px;
    margin: 50px auto;
    padding: 0 20px;
}
h1 { color: #333; }
CSS

cat > www/img/test.svg << 'SVG'
<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100">
  <circle cx="50" cy="50" r="40" fill="#4CAF50"/>
</svg>
SVG
```

---

## 10. Probar el servidor

### Compilar y ejecutar

```bash
gcc -Wall -Wextra -o minihttpd minihttpd.c
./minihttpd
# minihttpd listening on 0.0.0.0:8080
# Document root: /home/user/project/www

# Puerto y docroot opcionales:
./minihttpd 3000 /srv/public
```

### Pruebas con curl

```bash
# Página principal
curl http://localhost:8080/
# → HTML de index.html

# CSS
curl http://localhost:8080/css/style.css
# → contenido CSS

# Headers de respuesta
curl -v http://localhost:8080/
# → HTTP/1.1 200 OK
# → Content-Type: text/html; charset=utf-8
# → Content-Length: ...
# → Last-Modified: ...

# HEAD request
curl -I http://localhost:8080/css/style.css
# → headers sin body

# 404
curl -v http://localhost:8080/noexiste.html
# → HTTP/1.1 404 Not Found

# Path traversal (debe ser rechazado)
curl http://localhost:8080/../../../etc/passwd
# → 403 Forbidden

# Método no soportado
curl -X DELETE http://localhost:8080/
# → 405 Method Not Allowed

# Redirect de directorio
curl -v http://localhost:8080/css
# → 301 Moved Permanently
# → Location: /css/

# Cache (segunda petición)
curl -v -H "If-Modified-Since: Thu, 01 Jan 2099 00:00:00 GMT" \
     http://localhost:8080/index.html
# → 304 Not Modified (fecha en el futuro → no modificado)
```

### Probar con navegador

Abrir `http://localhost:8080/` en el navegador. Verificar:
- La página HTML se renderiza
- El CSS se aplica (verificar con DevTools → Network)
- La imagen SVG se muestra
- DevTools muestra Content-Type correcto para cada recurso

---

## 11. Errores comunes

### Error 1: no validar path traversal

```c
// MAL: concatenar directamente sin validar
snprintf(filepath, size, "%s%s", docroot, req.path);
open(filepath, O_RDONLY);
// req.path = "/../../etc/passwd"
// filepath = "./www/../../etc/passwd"
// → lee /etc/passwd del sistema
```

**Solución**: usar `realpath()` y verificar que el resultado
empieza con el document root resuelto.

### Error 2: no redirigir directorios sin trailing slash

```c
// MAL: servir index.html de /about sin redirect
// URI: /about → sirve ./www/about/index.html
// El HTML contiene <link href="style.css">
// Navegador resuelve: /style.css ← INCORRECTO
// Debería ser: /about/style.css
```

**Solución**: detectar directorio sin trailing slash y enviar
301 redirect a `/about/`.

### Error 3: usar strlen para Content-Length de archivos binarios

```c
// MAL: strlen en archivo leído en buffer
char *content = read_entire_file(filepath);
size_t len = strlen(content);
// Archivo PNG tiene byte 0x00 en posición 10
// → strlen retorna 10, Content-Length: 10
// → imagen truncada
```

**Solución**: usar `stat().st_size` para el tamaño del archivo:

```c
struct stat st;
stat(filepath, &st);
size_t len = st.st_size;
```

### Error 4: no cerrar file_fd después de servir

```c
// MAL: abrir sin cerrar
int fd = open(filepath, O_RDONLY);
// ... sendfile o read+write ...
// olvidar close(fd)
// → fd leak → después de ~1000 peticiones: Too many open files
```

**Solución**: cerrar el file descriptor en todos los paths
(éxito y error):

```c
int fd = open(filepath, O_RDONLY);
if (fd == -1) { send_error(client_fd, 500); return; }
// ... servir ...
close(fd);  // siempre cerrar
```

### Error 5: sendfile con offset no inicializado

```c
// MAL: offset sin inicializar
off_t offset;  // basura
sendfile(client_fd, file_fd, &offset, file_size);
// offset podría ser 999999 → sendfile empieza a leer desde
// posición 999999 del archivo → datos incorrectos o error
```

**Solución**: siempre inicializar offset a 0:

```c
off_t offset = 0;
sendfile(client_fd, file_fd, &offset, file_size);
```

---

## 12. Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│          Servir Archivos Estáticos                            │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  FLUJO COMPLETO:                                             │
│    parse → url_decode → método? → filepath → validate_path  │
│    → resolve_index → cache? → stat → open → sendfile/write  │
│    → close                                                   │
│                                                              │
│  DOCUMENT ROOT:                                              │
│    snprintf(filepath, size, "%s%s", DOCROOT, uri)            │
│    Nunca confiar en la URI directamente                      │
│                                                              │
│  PATH TRAVERSAL:                                             │
│    realpath(filepath, resolved)                              │
│    strncmp(resolved, docroot_resolved, docroot_len)          │
│    Si no empieza con docroot → 403 Forbidden                 │
│                                                              │
│  INDEX POR DEFECTO:                                          │
│    URI es directorio?                                        │
│      Sin trailing / → 301 redirect a URI/                    │
│      Con trailing / → concatenar "index.html"                │
│      index.html no existe → 404 (o directory listing)        │
│                                                              │
│  SERVIR ARCHIVO:                                             │
│    stat(filepath)  →  tamaño, mtime, permisos               │
│    S_ISREG()       →  verificar que es archivo regular       │
│    access(R_OK)    →  verificar permiso de lectura           │
│    get_mime_type()  →  Content-Type por extensión            │
│    open(O_RDONLY)   →  abrir archivo                         │
│    sendfile()       →  enviar sin copia a user space         │
│    close()          →  siempre cerrar                        │
│                                                              │
│  SENDFILE:                                                   │
│    #include <sys/sendfile.h>                                 │
│    off_t offset = 0;                                         │
│    sendfile(socket_fd, file_fd, &offset, size)               │
│    Loop con remaining (short sends posibles)                 │
│                                                              │
│  CACHE:                                                      │
│    Servidor envía:    Last-Modified: <fecha mtime>           │
│    Cliente envía:     If-Modified-Since: <fecha>             │
│    No modificado →    304 Not Modified (sin body)            │
│    Modificado →       200 OK + body completo                 │
│    parse_http_date(): strptime + timegm                      │
│                                                              │
│  DECISIONES:                                                 │
│    método ≠ GET/HEAD     → 405 + Allow                      │
│    parse falla           → 400                               │
│    path traversal        → 403                               │
│    archivo no existe     → 404                               │
│    sin permiso lectura   → 403                               │
│    no es archivo regular → 403                               │
│    stat/open falla       → 500                               │
│    todo OK               → 200 (o 304 con cache)            │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 13. Ejercicios

### Ejercicio 1: servidor de archivos con seguridad

Implementa el servidor de archivos completo con protección
contra path traversal.

**Pasos**:
1. Crea un document root `./www` con al menos index.html,
   un CSS, y una imagen
2. Implementa `resolve_path()` con snprintf de docroot + URI
3. Implementa `validate_path()` con realpath + strncmp
4. Implementa `serve_file()` con stat, open, read+write loop
5. Prueba path traversal: `curl localhost:8080/../../etc/passwd`
6. Prueba archivo normal: `curl localhost:8080/index.html`

**Predicción antes de ejecutar**: ¿qué retorna realpath si el
archivo no existe? ¿Y si el directorio padre tampoco existe?
¿Qué respuesta HTTP envías en cada caso (403 vs 404)?

> **Pregunta de reflexión**: realpath resuelve symlinks. Si
> creas un symlink dentro de www que apunta a /etc/passwd
> (`ln -s /etc/passwd www/secret`), ¿tu validación lo detecta?
> ¿Por qué sí (o por qué no)? ¿Es esto deseable? Nginx tiene
> la directiva `disable_symlinks` — ¿cuándo la usarías?

### Ejercicio 2: sendfile + cache 304

Implementa la transferencia con sendfile y soporte de cache
con If-Modified-Since.

**Pasos**:
1. Reemplaza read+write por sendfile en un loop (manejar short
   sends y EINTR)
2. Implementa `format_http_date()` y `parse_http_date()`
3. Añade `Last-Modified` al header de respuesta
4. Implementa el chequeo de `If-Modified-Since`: si el archivo
   no cambió → 304 sin body
5. Prueba: primera petición → 200, segunda con
   `-H "If-Modified-Since: ..."` → 304

**Predicción antes de ejecutar**: si haces `touch www/index.html`
(actualizar mtime sin cambiar contenido) y luego envías una
petición con If-Modified-Since anterior al touch, ¿recibes 200
o 304? ¿El contenido es el mismo en ambos casos?

> **Pregunta de reflexión**: Last-Modified tiene resolución de
> 1 segundo. Si un archivo cambia dos veces en el mismo segundo,
> el cliente puede tener una versión stale. ETags (hash del
> contenido) resuelven esto. ¿Cómo implementarías un ETag
> simple? ¿Calcular un hash MD5 de cada archivo en cada petición
> sería eficiente? ¿Qué alternativa más ligera usarías?

### Ejercicio 3: servidor con múltiples funcionalidades

Integra todos los tópicos (T01-T04) en un servidor funcional:

**Pasos**:
1. Socket listener con SO_REUSEADDR y fork por conexión (T01)
2. Parser HTTP completo con validación (T02)
3. Respuestas con headers correctos, Date, Server, MIME types (T03)
4. Servir archivos del document root con path traversal protection
5. Index.html por defecto en directorios
6. Redirect 301 para directorios sin trailing slash
7. Páginas de error HTML para 400, 403, 404, 405
8. Shutdown limpio con Ctrl+C

**Predicción antes de ejecutar**: abre el servidor y navega a
`http://localhost:8080/` en un navegador. ¿Cuántas peticiones
HTTP ves en los logs? (Pista: el HTML referencia CSS e imágenes.)
¿En qué orden llegan las peticiones?

> **Pregunta de reflexión**: con fork por conexión, cada petición
> crea un proceso nuevo. Si una página HTML referencia 10 recursos
> (CSS, JS, imágenes), el navegador abre hasta 6 conexiones
> paralelas (límite HTTP/1.1). ¿Cuántos procesos fork se crean
> para cargar una sola página? ¿Qué mejoras (thread pool, epoll,
> keep-alive) reducirían este overhead? Las cubriremos en S02.
