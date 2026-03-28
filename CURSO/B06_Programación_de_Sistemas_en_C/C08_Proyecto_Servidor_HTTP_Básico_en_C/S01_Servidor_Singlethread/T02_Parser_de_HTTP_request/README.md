# Parser de HTTP request

## Índice
1. [Anatomía de una petición HTTP](#1-anatomía-de-una-petición-http)
2. [Request line](#2-request-line)
3. [Headers](#3-headers)
4. [Body](#4-body)
5. [Estructura de datos del request](#5-estructura-de-datos-del-request)
6. [Leer la petición completa del socket](#6-leer-la-petición-completa-del-socket)
7. [Parsear la request line](#7-parsear-la-request-line)
8. [Parsear los headers](#8-parsear-los-headers)
9. [Parser completo](#9-parser-completo)
10. [Decodificar URL (percent-encoding)](#10-decodificar-url-percent-encoding)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. Anatomía de una petición HTTP

Cuando un cliente (curl, navegador) se conecta a nuestro servidor,
envía una petición HTTP como texto plano sobre TCP. Este es el
formato:

```
GET /index.html HTTP/1.1\r\n          ← request line
Host: localhost:8080\r\n              ← header
User-Agent: curl/7.88.1\r\n          ← header
Accept: */*\r\n                       ← header
\r\n                                  ← línea vacía (fin de headers)
[body opcional]                       ← solo en POST/PUT
```

### Estructura visual

```
┌─────────────────────────────────────────────────┐
│  Request Line                                    │
│  ┌────────┐ ┌──────────────┐ ┌──────────┐       │
│  │ Método │ │    Path      │ │ Versión  │       │
│  │  GET   │ │ /index.html  │ │ HTTP/1.1 │       │
│  └────────┘ └──────────────┘ └──────────┘       │
├─────────────────────────────────────────────────┤
│  Headers (clave: valor)                          │
│    Host: localhost:8080                           │
│    User-Agent: curl/7.88.1                       │
│    Accept: */*                                   │
│    Content-Length: 13    (si hay body)            │
│    Content-Type: application/x-www-form-urlencoded│
├─────────────────────────────────────────────────┤
│  \r\n  (línea vacía = fin de headers)            │
├─────────────────────────────────────────────────┤
│  Body (opcional)                                 │
│    name=John&age=30                              │
└─────────────────────────────────────────────────┘
```

### Separadores clave

HTTP usa `\r\n` (CRLF) como separador de línea, **no** `\n`:

```
\r = carriage return (0x0D)
\n = line feed       (0x0A)

Cada línea termina en \r\n
Fin de headers = \r\n\r\n (línea vacía)
```

Esto es un estándar del protocolo (RFC 7230). Algunos servidores
toleran solo `\n`, pero el estándar requiere `\r\n`.

---

## 2. Request line

La primera línea contiene tres campos separados por espacio:

```
GET /path/to/resource HTTP/1.1\r\n
^^^  ^^^^^^^^^^^^^^^^^  ^^^^^^^^
 │          │               │
método     URI           versión
```

### Métodos HTTP

| Método | Uso | ¿Tiene body? |
|--------|-----|-------------|
| `GET` | Obtener un recurso | No |
| `HEAD` | Como GET pero sin body en la respuesta | No |
| `POST` | Enviar datos al servidor | Sí |
| `PUT` | Reemplazar un recurso | Sí |
| `DELETE` | Eliminar un recurso | Opcional |
| `OPTIONS` | Consultar capacidades | No |

Para nuestro servidor de archivos estáticos, solo necesitamos
`GET` y `HEAD`. Pero parseamos el método genéricamente para
poder responder 405 (Method Not Allowed) a los demás.

### URI (path)

El URI es la ruta del recurso solicitado. Puede contener:

```
/                          raíz
/index.html                archivo
/css/style.css             subdirectorio
/search?q=hello&lang=es    query string
/path%20with%20spaces      percent-encoding
```

Para nuestro servidor, el path se mapea al filesystem:

```
URI:        /css/style.css
Document root: ./www
Filesystem: ./www/css/style.css
```

### Versión HTTP

```
HTTP/1.0    conexión cerrada después de cada respuesta (por defecto)
HTTP/1.1    keep-alive por defecto, requiere header Host
HTTP/2      binario, multiplexado (no lo cubrimos)
```

Nuestro servidor soporta HTTP/1.0 y HTTP/1.1 pero siempre
cierra la conexión (`Connection: close`).

---

## 3. Headers

Después de la request line, cada línea es un header con formato
`Nombre: Valor\r\n`. Los headers terminan con una línea vacía
(`\r\n\r\n`):

```
Host: localhost:8080\r\n
User-Agent: curl/7.88.1\r\n
Accept: text/html, application/json\r\n
Accept-Encoding: gzip, deflate\r\n
Connection: keep-alive\r\n
\r\n
```

### Headers relevantes para nuestro servidor

| Header | Significado | Ejemplo |
|--------|-------------|---------|
| `Host` | Nombre del servidor (obligatorio en HTTP/1.1) | `localhost:8080` |
| `Content-Length` | Tamaño del body en bytes | `42` |
| `Content-Type` | Tipo MIME del body | `application/json` |
| `Connection` | `keep-alive` o `close` | `close` |
| `User-Agent` | Identificación del cliente | `curl/7.88.1` |
| `Accept` | Tipos MIME que el cliente acepta | `text/html` |
| `If-Modified-Since` | Cache condicional | fecha HTTP |

### Reglas de los headers

- Los nombres de header **no** son case-sensitive:
  `Content-Type` == `content-type` == `CONTENT-TYPE`
- El valor empieza después de `: ` (dos puntos + espacio)
- Un header puede aparecer múltiples veces (se concatenan con `,`)
- Los headers pueden tener continuation lines (empezar con
  espacio/tab), pero esto fue deprecado en HTTP/1.1

---

## 4. Body

El body es opcional y aparece después de la línea vacía
(`\r\n\r\n`). Solo los métodos que lo requieren (POST, PUT)
lo envían.

### ¿Cómo saber el tamaño del body?

```
Método 1: Content-Length header
  Content-Length: 27\r\n
  \r\n
  name=John&age=30&city=Lima    ← exactamente 27 bytes

Método 2: Transfer-Encoding: chunked
  Transfer-Encoding: chunked\r\n
  \r\n
  1b\r\n                         ← tamaño del chunk en hex (27)
  name=John&age=30&city=Lima\r\n
  0\r\n                          ← chunk de tamaño 0 = fin
  \r\n

Método 3: Connection: close (HTTP/1.0)
  Leer hasta EOF
```

Para nuestro servidor de archivos estáticos, los clientes envían
GET (sin body). Pero parseamos `Content-Length` por si llega un
POST, para poder leer (y descartar) el body correctamente.

---

## 5. Estructura de datos del request

```c
#include <stddef.h>

#define MAX_METHOD    16
#define MAX_PATH      2048
#define MAX_VERSION   16
#define MAX_HEADERS   64
#define MAX_HEADER_NAME  128
#define MAX_HEADER_VALUE 4096

typedef struct {
    char name[MAX_HEADER_NAME];
    char value[MAX_HEADER_VALUE];
} http_header_t;

typedef struct {
    // Request line
    char method[MAX_METHOD];       // "GET", "POST", etc.
    char path[MAX_PATH];           // "/index.html"
    char query[MAX_PATH];          // "q=hello&lang=es" (después del ?)
    char version[MAX_VERSION];     // "HTTP/1.1"

    // Headers
    http_header_t headers[MAX_HEADERS];
    int num_headers;

    // Body
    char *body;                    // puntero al inicio del body (si existe)
    size_t content_length;         // valor de Content-Length (0 si no hay)
} http_request_t;
```

### ¿Por qué separar path y query?

La URI puede contener un query string:

```
/search?q=hello&lang=es
^^^^^^^^ ^^^^^^^^^^^^^^^^^
  path      query string

Al parsear, separamos en el '?':
  path:  "/search"
  query: "q=hello&lang=es"
```

El path se usa para encontrar el archivo. El query string se
usa para parámetros (no relevante para archivos estáticos, pero
bueno parsearlo igualmente).

---

## 6. Leer la petición completa del socket

TCP es un flujo de bytes — un `read()` puede retornar una
petición parcial. Necesitamos leer hasta encontrar `\r\n\r\n`
(fin de headers):

```c
#define REQUEST_BUFSIZE 8192

// Leer del socket hasta encontrar \r\n\r\n o llenar el buffer
// Retorna bytes leídos, o -1 en error
ssize_t read_request(int fd, char *buf, size_t bufsize) {
    size_t total = 0;

    while (total < bufsize - 1) {
        ssize_t n = read(fd, buf + total, bufsize - 1 - total);

        if (n == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) {
            // Cliente cerró la conexión
            return total > 0 ? (ssize_t)total : -1;
        }

        total += n;
        buf[total] = '\0';

        // ¿Ya tenemos los headers completos?
        if (strstr(buf, "\r\n\r\n") != NULL) {
            return total;
        }
    }

    // Buffer lleno sin encontrar \r\n\r\n
    return -1;  // request demasiado grande
}
```

### ¿Por qué no un solo read?

```
Caso ideal (LAN rápida):
  read() → "GET / HTTP/1.1\r\nHost: ...\r\n\r\n"  (todo de una vez)

Caso real (red lenta, request grande):
  read() → "GET / HTTP/1.1\r\nHost"     (parcial)
  read() → ": localhost\r\nUser-Agen"   (parcial)
  read() → "t: curl\r\n\r\n"            (fin de headers)
```

TCP no preserva los límites de los mensajes. Un `read()` puede
retornar cualquier cantidad de bytes, desde 1 hasta el tamaño
del buffer. El loop garantiza que leemos hasta el `\r\n\r\n`.

### Diagrama del buffer

```
Buffer después de leer headers completos:

buf: "GET / HTTP/1.1\r\nHost: localhost\r\nAccept: */*\r\n\r\nbody..."
      ├─────────────── headers ──────────────────────┤├── body ──┤
                                                    ▲
                                              \r\n\r\n
                                         (fin de headers)
```

Si el request tiene body (POST), los primeros bytes del body
pueden estar ya en el buffer. Por eso guardamos el puntero
al body para procesarlo después.

---

## 7. Parsear la request line

La request line es la primera línea del request, terminada en
`\r\n`:

```c
#include <string.h>
#include <stdio.h>

// Parsear "GET /path?query HTTP/1.1\r\n"
// Retorna 0 en éxito, -1 en error
int parse_request_line(const char *line, http_request_t *req) {
    // Método: leer hasta el primer espacio
    const char *p = line;
    const char *space = strchr(p, ' ');
    if (space == NULL) return -1;

    size_t method_len = space - p;
    if (method_len >= MAX_METHOD) return -1;
    memcpy(req->method, p, method_len);
    req->method[method_len] = '\0';

    // URI: leer hasta el siguiente espacio
    p = space + 1;
    space = strchr(p, ' ');
    if (space == NULL) return -1;

    size_t uri_len = space - p;
    if (uri_len >= MAX_PATH) return -1;

    // Separar path y query en el '?'
    const char *question = memchr(p, '?', uri_len);
    if (question != NULL) {
        size_t path_len = question - p;
        memcpy(req->path, p, path_len);
        req->path[path_len] = '\0';

        size_t query_len = (space - question) - 1;
        memcpy(req->query, question + 1, query_len);
        req->query[query_len] = '\0';
    } else {
        memcpy(req->path, p, uri_len);
        req->path[uri_len] = '\0';
        req->query[0] = '\0';
    }

    // Versión: leer hasta \r\n
    p = space + 1;
    const char *crlf = strstr(p, "\r\n");
    if (crlf == NULL) return -1;

    size_t version_len = crlf - p;
    if (version_len >= MAX_VERSION) return -1;
    memcpy(req->version, p, version_len);
    req->version[version_len] = '\0';

    // Validar versión
    if (strcmp(req->version, "HTTP/1.0") != 0 &&
        strcmp(req->version, "HTTP/1.1") != 0) {
        return -1;
    }

    return 0;
}
```

### Paso a paso visual

```
Entrada: "GET /search?q=hello HTTP/1.1\r\n"

Paso 1: encontrar primer espacio
  "GET /search?q=hello HTTP/1.1\r\n"
   ^^^
   method = "GET"

Paso 2: encontrar segundo espacio
  "GET /search?q=hello HTTP/1.1\r\n"
       ^^^^^^^^^^^^^^^^
       URI completa

Paso 3: separar en '?'
  "/search?q=hello"
   ^^^^^^^  ^^^^^^^
   path     query
   "/search" "q=hello"

Paso 4: versión hasta \r\n
  "HTTP/1.1\r\n"
   ^^^^^^^^
   version = "HTTP/1.1"
```

---

## 8. Parsear los headers

Después de la request line, cada header tiene formato
`Nombre: Valor\r\n`. Parseamos hasta la línea vacía:

```c
// Parsear headers desde la posición actual
// p apunta al primer byte después de la request line (\r\n)
// Retorna puntero al inicio del body (después de \r\n\r\n), o NULL
const char *parse_headers(const char *p, http_request_t *req) {
    req->num_headers = 0;

    while (*p != '\0') {
        // ¿Línea vacía? → fin de headers
        if (p[0] == '\r' && p[1] == '\n') {
            return p + 2;  // puntero al body
        }

        // Encontrar ':'
        const char *colon = strchr(p, ':');
        const char *crlf  = strstr(p, "\r\n");

        if (colon == NULL || crlf == NULL || colon > crlf) {
            return NULL;  // header malformado
        }

        if (req->num_headers >= MAX_HEADERS) {
            // Demasiados headers, ignorar el resto
            const char *end = strstr(p, "\r\n\r\n");
            return end ? end + 4 : NULL;
        }

        // Nombre del header
        http_header_t *h = &req->headers[req->num_headers];
        size_t name_len = colon - p;
        if (name_len >= MAX_HEADER_NAME) {
            p = crlf + 2;
            continue;  // header con nombre demasiado largo, saltar
        }
        memcpy(h->name, p, name_len);
        h->name[name_len] = '\0';

        // Valor: saltar ": " (colon + espacio opcional)
        const char *val = colon + 1;
        while (*val == ' ' || *val == '\t')
            val++;  // skip OWS (optional whitespace)

        size_t val_len = crlf - val;
        if (val_len >= MAX_HEADER_VALUE)
            val_len = MAX_HEADER_VALUE - 1;
        memcpy(h->value, val, val_len);
        h->value[val_len] = '\0';

        req->num_headers++;
        p = crlf + 2;  // avanzar al siguiente header
    }

    return NULL;
}
```

### Buscar un header por nombre

Como los nombres de header no son case-sensitive, necesitamos
comparación case-insensitive:

```c
#include <strings.h>  // para strcasecmp

// Buscar un header por nombre (case-insensitive)
const char *get_header(const http_request_t *req, const char *name) {
    for (int i = 0; i < req->num_headers; i++) {
        if (strcasecmp(req->headers[i].name, name) == 0) {
            return req->headers[i].value;
        }
    }
    return NULL;
}
```

### Ejemplo de uso

```c
const char *host = get_header(&req, "Host");
// "localhost:8080"

const char *content_len = get_header(&req, "Content-Length");
if (content_len != NULL) {
    req.content_length = strtoul(content_len, NULL, 10);
}

const char *conn = get_header(&req, "Connection");
// "keep-alive" o "close"
```

---

## 9. Parser completo

Unimos las piezas: leer del socket, parsear request line, parsear
headers:

```c
// Parsear una petición HTTP completa
// buf: buffer con los datos leídos del socket
// req: estructura donde guardar el resultado
// Retorna 0 en éxito, -1 en error de parseo
int parse_http_request(const char *buf, http_request_t *req) {
    memset(req, 0, sizeof(*req));

    // 1. Parsear request line (primera línea)
    if (parse_request_line(buf, req) == -1) {
        return -1;
    }

    // 2. Avanzar al primer header (después del primer \r\n)
    const char *header_start = strstr(buf, "\r\n");
    if (header_start == NULL) return -1;
    header_start += 2;

    // 3. Parsear headers
    const char *body_start = parse_headers(header_start, req);

    // 4. Content-Length
    const char *cl = get_header(req, "Content-Length");
    if (cl != NULL) {
        req->content_length = strtoul(cl, NULL, 10);
    }

    // 5. Body (puede estar parcialmente en el buffer)
    req->body = (char *)body_start;

    return 0;
}
```

### Integración con handle_client

```c
void handle_client(int client_fd, const char *client_ip) {
    char buf[REQUEST_BUFSIZE];

    // 1. Leer la petición
    ssize_t n = read_request(client_fd, buf, sizeof(buf));
    if (n <= 0) return;

    // 2. Parsear
    http_request_t req;
    if (parse_http_request(buf, &req) == -1) {
        // Petición malformada → 400 Bad Request
        const char *resp =
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Length: 11\r\n"
            "Connection: close\r\n"
            "\r\n"
            "Bad Request";
        write(client_fd, resp, strlen(resp));
        return;
    }

    // 3. Log
    printf("[%s] %s %s %s\n",
           client_ip, req.method, req.path, req.version);

    // 4. Procesar (próximos tópicos: response + archivos)
    if (strcmp(req.method, "GET") == 0 ||
        strcmp(req.method, "HEAD") == 0) {
        // serve_file(client_fd, &req);  ← T03 y T04
        send_placeholder_response(client_fd, &req);
    } else {
        // 405 Method Not Allowed
        const char *resp =
            "HTTP/1.1 405 Method Not Allowed\r\n"
            "Allow: GET, HEAD\r\n"
            "Content-Length: 18\r\n"
            "Connection: close\r\n"
            "\r\n"
            "Method Not Allowed";
        write(client_fd, resp, strlen(resp));
    }
}

// Respuesta temporal hasta que implementemos T03
void send_placeholder_response(int client_fd, const http_request_t *req) {
    char body[512];
    int body_len = snprintf(body, sizeof(body),
        "Method: %s\nPath: %s\nQuery: %s\nVersion: %s\n"
        "Headers: %d\n",
        req->method, req->path, req->query, req->version,
        req->num_headers);

    char response[1024];
    int resp_len = snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        body_len, body);

    write(client_fd, response, resp_len);
}
```

### Probar el parser

```bash
gcc -Wall -Wextra -o httpd httpd.c
./httpd &

# Petición normal
curl -v http://localhost:8080/index.html

# Con query string
curl http://localhost:8080/search?q=hello

# POST (el servidor responde 405)
curl -X POST -d "data=test" http://localhost:8080/

# Headers personalizados
curl -H "X-Custom: value" http://localhost:8080/

# Petición malformada (solo texto sin formato HTTP)
echo "not http" | nc localhost 8080
# → 400 Bad Request
```

---

## 10. Decodificar URL (percent-encoding)

Las URLs no pueden contener ciertos caracteres directamente.
Se codifican como `%XX` donde XX es el valor hexadecimal del byte:

```
Espacio     → %20
/           → %2F  (cuando es parte del nombre, no del path)
?           → %3F
&           → %26
#           → %23
á           → %C3%A1  (UTF-8, dos bytes)
```

### Decodificador

```c
#include <ctype.h>

// Convertir un carácter hex a su valor numérico
static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Decodificar percent-encoding in-place
// "/hello%20world" → "/hello world"
void url_decode(char *str) {
    char *src = str;
    char *dst = str;

    while (*src) {
        if (*src == '%' && isxdigit(src[1]) && isxdigit(src[2])) {
            int hi = hex_val(src[1]);
            int lo = hex_val(src[2]);
            *dst = (char)((hi << 4) | lo);
            src += 3;
        } else if (*src == '+') {
            // '+' es espacio en query strings (application/x-www-form-urlencoded)
            *dst = ' ';
            src++;
        } else {
            *dst = *src;
            src++;
        }
        dst++;
    }
    *dst = '\0';
}
```

### ¿Cuándo decodificar?

Decodificar **después** de parsear la request line, pero **antes**
de usar el path para buscar archivos:

```c
parse_request_line(buf, &req);
// req.path = "/hello%20world"

url_decode(req.path);
// req.path = "/hello world"

url_decode(req.query);
// req.query decodificado
```

### Seguridad: path traversal

Después de decodificar, el path podría contener `..` para escapar
del document root:

```
GET /../../etc/passwd HTTP/1.1
GET /%2e%2e/%2e%2e/etc/passwd HTTP/1.1   (codificado)

Decodificado: /../../etc/passwd
Resuelto:     /etc/passwd  ← fuera del document root
```

Verificar esto es crítico — lo cubriremos en T04 (servir archivos).

---

## 11. Errores comunes

### Error 1: buscar \n en vez de \r\n

```c
// MAL: buscar solo \n
char *end_of_line = strchr(buf, '\n');
// Funciona con la mayoría de clientes pero viola el estándar
// Algunos proxies envían \r\n estricto y esto se rompe
```

**Solución**: siempre buscar `\r\n`:

```c
char *end_of_line = strstr(buf, "\r\n");
```

En la práctica, ser tolerante al parsear (aceptar `\n` solo)
pero estricto al generar (siempre enviar `\r\n`) es razonable.
Para nuestro servidor educativo, ser estricto con `\r\n` es
mejor para entender el protocolo.

### Error 2: un solo read sin loop

```c
// MAL: asumir que read() retorna la petición completa
char buf[4096];
ssize_t n = read(client_fd, buf, sizeof(buf));
// Si la red es lenta, puede retornar solo "GET / HTT"
// → parseo falla
```

**Solución**: loop de read hasta encontrar `\r\n\r\n`:

```c
while (total < bufsize - 1) {
    ssize_t n = read(fd, buf + total, bufsize - 1 - total);
    total += n;
    buf[total] = '\0';
    if (strstr(buf, "\r\n\r\n"))
        break;
}
```

### Error 3: no terminar el buffer con \0

```c
// MAL: usar strstr en buffer sin null terminator
ssize_t n = read(fd, buf, sizeof(buf));
char *crlf = strstr(buf, "\r\n");  // comportamiento indefinido
// strstr lee hasta encontrar \0 — si no hay, lee memoria basura
```

**Solución**: siempre null-terminate después de read:

```c
ssize_t n = read(fd, buf + total, sizeof(buf) - 1 - total);
total += n;
buf[total] = '\0';  // ahora strstr es seguro
```

### Error 4: comparación case-sensitive de headers

```c
// MAL: strcmp es case-sensitive
if (strcmp(header->name, "Content-Length") == 0)
// No detecta "content-length" ni "CONTENT-LENGTH"
// RFC dice que los nombres de header son case-insensitive
```

**Solución**: usar `strcasecmp`:

```c
if (strcasecmp(header->name, "Content-Length") == 0)
// Detecta "Content-Length", "content-length", "CONTENT-LENGTH"
```

### Error 5: no validar Content-Length antes de leer body

```c
// MAL: confiar ciegamente en Content-Length
size_t cl = strtoul(get_header(&req, "Content-Length"), NULL, 10);
char *body = malloc(cl);
read(client_fd, body, cl);
// Si cl = 4294967295 (MAX_UINT), malloc(4GB) → crash o OOM
```

**Solución**: validar contra un límite razonable:

```c
#define MAX_BODY_SIZE (1 * 1024 * 1024)  // 1 MB

size_t cl = strtoul(content_length_str, NULL, 10);
if (cl > MAX_BODY_SIZE) {
    // 413 Payload Too Large
    send_error(client_fd, 413);
    return;
}
```

---

## 12. Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│            Parser de HTTP Request                            │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  FORMATO HTTP REQUEST:                                       │
│    MÉTODO ESPACIO PATH ESPACIO VERSIÓN \r\n                  │
│    Header-Name: Header-Value\r\n                             │
│    Header-Name: Header-Value\r\n                             │
│    \r\n                       ← fin de headers               │
│    [body]                     ← solo si Content-Length > 0   │
│                                                              │
│  PARSEO EN ORDEN:                                            │
│    1. read_request()     leer hasta \r\n\r\n                 │
│    2. parse_request_line()  método + path + versión          │
│    3. parse_headers()       clave: valor por línea           │
│    4. get_header("Content-Length")  tamaño del body          │
│    5. url_decode(path)      decodificar %XX                  │
│                                                              │
│  REQUEST LINE:                                               │
│    "GET /path?query HTTP/1.1\r\n"                            │
│    Separar por espacios, dividir path/query en '?'           │
│                                                              │
│  HEADERS:                                                    │
│    Nombre: Valor\r\n                                         │
│    Nombres case-insensitive → strcasecmp()                   │
│    Fin de headers: \r\n\r\n                                  │
│                                                              │
│  URL DECODE:                                                 │
│    %20 → espacio                                             │
│    %2F → /                                                   │
│    + → espacio (en query strings)                            │
│                                                              │
│  RESPUESTAS DE ERROR:                                        │
│    400 Bad Request          petición malformada              │
│    405 Method Not Allowed   método no soportado              │
│    413 Payload Too Large    body excede límite               │
│                                                              │
│  SEGURIDAD:                                                  │
│    Validar Content-Length contra MAX_BODY_SIZE               │
│    Limitar tamaño de buffer de request                       │
│    url_decode DESPUÉS de parsear, ANTES de usar path        │
│    Verificar path traversal (../) ← en T04                  │
│                                                              │
│  READ DEL SOCKET:                                            │
│    Loop hasta \r\n\r\n (no un solo read)                     │
│    Siempre null-terminate: buf[total] = '\0'                 │
│    Manejar EINTR en read                                     │
│    Límite de buffer para evitar DoS                          │
│                                                              │
│  DIFERENCIA HTTP/1.0 vs 1.1:                                 │
│    1.0: Connection: close por defecto                        │
│    1.1: Connection: keep-alive por defecto, Host obligatorio │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 13. Ejercicios

### Ejercicio 1: parser con dump de headers

Implementa el parser completo y muestra toda la petición
parseada.

**Pasos**:
1. Define `http_request_t` con method, path, query, version,
   headers[], num_headers
2. Implementa `read_request()` con loop hasta `\r\n\r\n`
3. Implementa `parse_request_line()` y `parse_headers()`
4. En `handle_client()`, parsea la petición y responde con un
   dump en texto plano de todos los campos parseados
5. Prueba con `curl -v -H "X-Test: hello" localhost:8080/page?id=42`

**Predicción antes de ejecutar**: ¿cuántos headers envía curl
por defecto (sin `-H` adicionales)? ¿En qué orden aparecen
Host, User-Agent, Accept? ¿El path incluye el query string
o está separado?

> **Pregunta de reflexión**: el parser guarda los headers en un
> array de tamaño fijo (MAX_HEADERS = 64). Un atacante podría
> enviar miles de headers para desbordar el array o consumir
> recursos. ¿Cómo protegerías tu parser contra este ataque?
> ¿Es suficiente con ignorar los headers extra, o deberías
> cerrar la conexión?

### Ejercicio 2: router por path

Extiende el servidor para responder diferente según el path:

**Pasos**:
1. `GET /` → responder "Welcome to minihttpd"
2. `GET /time` → responder con la hora actual (`time()` + `ctime()`)
3. `GET /headers` → responder con un dump de los headers del request
4. `GET /echo?msg=hello` → parsear query string y responder con el
   valor de `msg`
5. Cualquier otro path → responder 404 Not Found

**Predicción antes de ejecutar**: si haces `curl localhost:8080/echo?msg=hello%20world`, ¿el valor de msg será `hello%20world`
o `hello world`? ¿En qué paso del parseo se decodifica el `%20`?

> **Pregunta de reflexión**: el query string `msg=hello&name=John`
> tiene dos pares clave=valor. ¿Cómo implementarías un parser de
> query strings que separe los pares en `&` y luego cada par en
> `=`? ¿Qué pasa si el valor contiene un `=` literal
> (como `data=a=b`)?

### Ejercicio 3: validación y respuestas de error

Implementa validación robusta del request con respuestas HTTP
correctas para cada error:

**Pasos**:
1. Request mayor que 8KB → 413 Payload Too Large
2. Request line sin tres campos → 400 Bad Request
3. Versión diferente de HTTP/1.0 y HTTP/1.1 → 505 HTTP Version
   Not Supported
4. Método diferente de GET y HEAD → 405 Method Not Allowed
   (incluir header `Allow: GET, HEAD`)
5. Timeout de 10 segundos sin datos → cerrar conexión

**Predicción antes de ejecutar**: si envías `curl -X DELETE
localhost:8080/`, ¿qué status code recibe? ¿Qué header especial
debe incluir la respuesta 405? ¿Qué pasa si envías
`INVALID /path HTTP/1.1` — es un 400 o un 405?

> **Pregunta de reflexión**: el RFC dice que un servidor DEBE
> responder 501 Not Implemented para métodos desconocidos y 405
> Method Not Allowed para métodos conocidos pero no permitidos.
> ¿Cuál es la diferencia semántica? Si tu servidor solo soporta
> GET y HEAD, ¿deberías responder 405 o 501 a un `PATCH` request?
> ¿Y a un `FROBNICATE` request (método inventado)?
