# Response HTTP

## Índice
1. [Anatomía de una respuesta HTTP](#1-anatomía-de-una-respuesta-http)
2. [Status line y códigos de estado](#2-status-line-y-códigos-de-estado)
3. [Headers de respuesta](#3-headers-de-respuesta)
4. [Content-Type y MIME types](#4-content-type-y-mime-types)
5. [Construir la respuesta](#5-construir-la-respuesta)
6. [Enviar la respuesta completa](#6-enviar-la-respuesta-completa)
7. [Respuestas de error](#7-respuestas-de-error)
8. [HEAD: respuesta sin body](#8-head-respuesta-sin-body)
9. [Date y headers estándar](#9-date-y-headers-estándar)
10. [Función de respuesta unificada](#10-función-de-respuesta-unificada)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. Anatomía de una respuesta HTTP

La respuesta tiene la misma estructura general que la petición:
status line, headers, línea vacía, body.

```
HTTP/1.1 200 OK\r\n                    ← status line
Content-Type: text/html\r\n            ← header
Content-Length: 45\r\n                  ← header
Connection: close\r\n                  ← header
\r\n                                   ← fin de headers
<html><body>Hello, World!</body></html> ← body
```

### Estructura visual

```
┌─────────────────────────────────────────────────┐
│  Status Line                                     │
│  ┌──────────┐ ┌──────┐ ┌────────────────┐       │
│  │ Versión  │ │Código│ │  Reason Phrase  │       │
│  │ HTTP/1.1 │ │ 200  │ │      OK        │       │
│  └──────────┘ └──────┘ └────────────────┘       │
├─────────────────────────────────────────────────┤
│  Headers (clave: valor)                          │
│    Content-Type: text/html                       │
│    Content-Length: 45                             │
│    Connection: close                             │
│    Date: Thu, 20 Mar 2026 12:00:00 GMT           │
│    Server: minihttpd/1.0                         │
├─────────────────────────────────────────────────┤
│  \r\n  (línea vacía = fin de headers)            │
├─────────────────────────────────────────────────┤
│  Body                                            │
│    <html><body>Hello, World!</body></html>       │
└─────────────────────────────────────────────────┘
```

### Comparación request vs response

```
Request:                          Response:
  GET /path HTTP/1.1\r\n            HTTP/1.1 200 OK\r\n
  Host: localhost\r\n               Content-Type: text/html\r\n
  Accept: */*\r\n                   Content-Length: 45\r\n
  \r\n                              \r\n
  [body]                            <html>...</html>

  ^^^^^^^^^^^^^^                    ^^^^^^^^^^^^^^^^^
  método path versión               versión código frase
```

---

## 2. Status line y códigos de estado

La status line tiene tres campos separados por espacio:

```
HTTP/1.1 200 OK\r\n
^^^^^^^^ ^^^ ^^
versión  │   reason phrase (texto descriptivo)
         │
    status code (número de 3 dígitos)
```

### Códigos de estado organizados por clase

```
1xx ─ Informacional (raro en HTTP/1.1 simple)
    100 Continue          el cliente puede enviar el body

2xx ─ Éxito
    200 OK                petición exitosa
    201 Created           recurso creado (POST)
    204 No Content        éxito sin body

3xx ─ Redirección
    301 Moved Permanently redirigir permanentemente
    302 Found             redirigir temporalmente
    304 Not Modified      cache válido, no enviar body

4xx ─ Error del cliente
    400 Bad Request       petición malformada
    403 Forbidden         acceso denegado
    404 Not Found         recurso no existe
    405 Method Not Allowed método no soportado
    408 Request Timeout   timeout esperando petición
    413 Payload Too Large body demasiado grande
    414 URI Too Long      URI demasiado larga

5xx ─ Error del servidor
    500 Internal Server Error  error genérico del servidor
    501 Not Implemented        funcionalidad no implementada
    503 Service Unavailable    servidor sobrecargado
    505 HTTP Version Not Supported
```

### Tabla de reason phrases

Para nuestro servidor, un lookup sencillo:

```c
const char *status_reason(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 503: return "Service Unavailable";
        case 505: return "HTTP Version Not Supported";
        default:  return "Unknown";
    }
}
```

### La reason phrase es ignorable

El cliente (curl, navegador) usa el **código numérico** para
tomar decisiones, no el texto. Se podría enviar
`HTTP/1.1 404 Nope\r\n` y el cliente lo trata como un 404.
Pero usar las frases estándar es buena práctica.

---

## 3. Headers de respuesta

### Headers que nuestro servidor debe enviar

| Header | Propósito | Ejemplo |
|--------|-----------|---------|
| `Content-Type` | Tipo MIME del body | `text/html; charset=utf-8` |
| `Content-Length` | Tamaño del body en bytes | `1234` |
| `Connection` | Mantener o cerrar conexión | `close` |
| `Date` | Fecha/hora de la respuesta | `Thu, 20 Mar 2026 12:00:00 GMT` |
| `Server` | Identificación del servidor | `minihttpd/1.0` |

### Headers opcionales útiles

| Header | Propósito | Ejemplo |
|--------|-----------|---------|
| `Allow` | Métodos permitidos (con 405) | `GET, HEAD` |
| `Location` | URL de redirección (con 3xx) | `/new-path` |
| `Last-Modified` | Última modificación del archivo | fecha HTTP |
| `Cache-Control` | Directivas de cache | `no-cache` |

### ¿Por qué Connection: close?

HTTP/1.1 usa keep-alive por defecto — el cliente espera que la
conexión se mantenga abierta para enviar más peticiones. Para
simplificar nuestro servidor, siempre cerramos:

```
Con Connection: close:
  Request 1 → Response 1 → close → nueva conexión
  Request 2 → Response 2 → close → nueva conexión
  (simple, una petición por conexión)

Con keep-alive (default HTTP/1.1):
  Request 1 → Response 1 → Request 2 → Response 2 → ... → close
  (complejo: necesitas Content-Length exacto, parsear múltiples
   requests del mismo socket, manejar timeouts)
```

---

## 4. Content-Type y MIME types

`Content-Type` dice al cliente cómo interpretar el body. Sin
este header, el navegador no sabe si el body es HTML, una imagen,
o texto plano.

### Tabla de MIME types comunes

```c
typedef struct {
    const char *extension;
    const char *mime_type;
} mime_entry_t;

static const mime_entry_t MIME_TYPES[] = {
    // Texto
    {".html", "text/html; charset=utf-8"},
    {".htm",  "text/html; charset=utf-8"},
    {".css",  "text/css"},
    {".js",   "application/javascript"},
    {".json", "application/json"},
    {".xml",  "application/xml"},
    {".txt",  "text/plain; charset=utf-8"},
    {".csv",  "text/csv"},

    // Imágenes
    {".png",  "image/png"},
    {".jpg",  "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif",  "image/gif"},
    {".svg",  "image/svg+xml"},
    {".ico",  "image/x-icon"},
    {".webp", "image/webp"},

    // Fuentes
    {".woff",  "font/woff"},
    {".woff2", "font/woff2"},
    {".ttf",   "font/ttf"},

    // Otros
    {".pdf",  "application/pdf"},
    {".zip",  "application/zip"},
    {".gz",   "application/gzip"},
    {".tar",  "application/x-tar"},
    {".mp4",  "video/mp4"},
    {".mp3",  "audio/mpeg"},
    {".wasm", "application/wasm"},

    {NULL, NULL}
};
```

### Lookup por extensión

```c
#include <string.h>
#include <strings.h>

const char *get_mime_type(const char *path) {
    // Encontrar la última extensión
    const char *dot = strrchr(path, '.');
    if (dot == NULL)
        return "application/octet-stream";  // binario genérico

    // Buscar en la tabla
    for (int i = 0; MIME_TYPES[i].extension != NULL; i++) {
        if (strcasecmp(dot, MIME_TYPES[i].extension) == 0) {
            return MIME_TYPES[i].mime_type;
        }
    }

    return "application/octet-stream";  // extensión desconocida
}
```

### ¿Por qué charset=utf-8?

Para archivos de texto, el charset indica la codificación de
caracteres. Sin especificarlo, el navegador puede adivinar mal
y mostrar caracteres rotos:

```
Content-Type: text/html
→ navegador adivina charset (puede ser ISO-8859-1)
→ caracteres como á, ñ, ü pueden verse mal

Content-Type: text/html; charset=utf-8
→ navegador sabe que es UTF-8
→ caracteres se muestran correctamente
```

### strrchr: la última extensión

```c
const char *path = "/archive.tar.gz";
const char *dot = strrchr(path, '.');
// dot → ".gz"  (la ÚLTIMA extensión, no ".tar")

// Esto es correcto para MIME: .tar.gz → application/gzip
```

---

## 5. Construir la respuesta

La respuesta se construye en dos partes: headers y body. Los
headers son texto, el body puede ser binario (imágenes, PDFs).

### Construir headers

```c
#include <stdio.h>
#include <time.h>

#define MAX_RESPONSE_HEADERS 4096

// Construir los headers de respuesta en un buffer
// Retorna el número de bytes escritos
int build_response_headers(char *buf, size_t bufsize,
                           int status_code,
                           const char *content_type,
                           size_t content_length)
{
    // Fecha HTTP (RFC 7231)
    char date_buf[128];
    time_t now = time(NULL);
    struct tm *gmt = gmtime(&now);
    strftime(date_buf, sizeof(date_buf),
             "%a, %d %b %Y %H:%M:%S GMT", gmt);

    int n = snprintf(buf, bufsize,
        "HTTP/1.1 %d %s\r\n"
        "Server: minihttpd/1.0\r\n"
        "Date: %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_code, status_reason(status_code),
        date_buf,
        content_type,
        content_length);

    return n;
}
```

### ¿Por qué separar headers y body?

El body puede ser binario (una imagen PNG tiene bytes `\0` en
su contenido). No podemos mezclar headers y body en un solo
`snprintf` porque `%s` se detiene en `\0`:

```c
// MAL: body binario cortado en \0
snprintf(buf, size, "%s%s", headers, binary_body);
// binary_body tiene un \0 en posición 10
// → solo se copian 10 bytes del body

// BIEN: enviar por separado
write(fd, headers, header_len);   // headers son texto
write(fd, body, body_len);        // body puede ser binario
```

### Flujo de envío

```
┌──────────────────────┐
│  build_response_headers()
│  → "HTTP/1.1 200 OK\r\n..."
└──────────┬───────────┘
           │ write(fd, headers, header_len)
           ▼
┌──────────────────────┐
│  body (archivo leído │
│  o generado)         │
└──────────┬───────────┘
           │ write(fd, body, body_len)
           ▼
     close(fd)
```

---

## 6. Enviar la respuesta completa

### Función write_all para escritura completa

`write()` puede enviar menos bytes de los pedidos (short write).
Necesitamos un loop:

```c
#include <unistd.h>
#include <errno.h>

// Enviar exactamente len bytes al fd
// Retorna 0 en éxito, -1 en error
int write_all(int fd, const void *buf, size_t len) {
    const char *ptr = buf;
    size_t remaining = len;

    while (remaining > 0) {
        ssize_t n = write(fd, ptr, remaining);
        if (n == -1) {
            if (errno == EINTR)
                continue;      // señal interrumpió, reintentar
            return -1;         // error real
        }
        ptr       += n;
        remaining -= n;
    }
    return 0;
}
```

### Enviar una respuesta con body de texto

```c
void send_response(int fd, int status_code,
                   const char *content_type,
                   const char *body, size_t body_len)
{
    char headers[MAX_RESPONSE_HEADERS];
    int header_len = build_response_headers(
        headers, sizeof(headers),
        status_code, content_type, body_len);

    write_all(fd, headers, header_len);
    if (body_len > 0) {
        write_all(fd, body, body_len);
    }
}
```

### Ejemplo de uso

```c
// Respuesta HTML
const char *html = "<html><body><h1>Hello!</h1></body></html>";
send_response(client_fd, 200, "text/html; charset=utf-8",
              html, strlen(html));

// Respuesta JSON
const char *json = "{\"status\": \"ok\"}";
send_response(client_fd, 200, "application/json",
              json, strlen(json));

// Respuesta de texto
send_response(client_fd, 200, "text/plain; charset=utf-8",
              "Hello, World!\n", 14);
```

---

## 7. Respuestas de error

Para cada tipo de error, enviamos una respuesta HTTP con el
status code correspondiente y un body descriptivo en HTML:

```c
void send_error(int fd, int status_code) {
    const char *reason = status_reason(status_code);

    // Body HTML simple
    char body[512];
    int body_len = snprintf(body, sizeof(body),
        "<!DOCTYPE html>\n"
        "<html><head><title>%d %s</title></head>\n"
        "<body><h1>%d %s</h1></body></html>\n",
        status_code, reason,
        status_code, reason);

    send_response(fd, status_code,
                  "text/html; charset=utf-8",
                  body, body_len);
}
```

### Errores con headers adicionales

Algunos status codes requieren headers extra:

```c
void send_error_405(int fd) {
    const char *body =
        "<!DOCTYPE html>\n"
        "<html><head><title>405 Method Not Allowed</title></head>\n"
        "<body><h1>405 Method Not Allowed</h1></body></html>\n";
    size_t body_len = strlen(body);

    char headers[MAX_RESPONSE_HEADERS];
    char date_buf[128];
    time_t now = time(NULL);
    struct tm *gmt = gmtime(&now);
    strftime(date_buf, sizeof(date_buf),
             "%a, %d %b %Y %H:%M:%S GMT", gmt);

    int header_len = snprintf(headers, sizeof(headers),
        "HTTP/1.1 405 Method Not Allowed\r\n"
        "Server: minihttpd/1.0\r\n"
        "Date: %s\r\n"
        "Allow: GET, HEAD\r\n"           // ← header extra obligatorio
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        date_buf, body_len);

    write_all(fd, headers, header_len);
    write_all(fd, body, body_len);
}

void send_redirect(int fd, int code, const char *location) {
    char body[512];
    int body_len = snprintf(body, sizeof(body),
        "<!DOCTYPE html>\n"
        "<html><head><title>%d %s</title></head>\n"
        "<body><h1>%d %s</h1>\n"
        "<p>Redirecting to <a href=\"%s\">%s</a></p></body></html>\n",
        code, status_reason(code),
        code, status_reason(code),
        location, location);

    char headers[MAX_RESPONSE_HEADERS];
    char date_buf[128];
    time_t now = time(NULL);
    struct tm *gmt = gmtime(&now);
    strftime(date_buf, sizeof(date_buf),
             "%a, %d %b %Y %H:%M:%S GMT", gmt);

    int header_len = snprintf(headers, sizeof(headers),
        "HTTP/1.1 %d %s\r\n"
        "Server: minihttpd/1.0\r\n"
        "Date: %s\r\n"
        "Location: %s\r\n"              // ← header de redirección
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        code, status_reason(code),
        date_buf,
        location,
        body_len);

    write_all(fd, headers, header_len);
    write_all(fd, body, body_len);
}
```

### Mapa de decisión

```
parse_http_request() → error?
  │                      │
  │                      └─→ send_error(fd, 400)
  ▼
method == GET || HEAD?
  │              │
  │              └─→ send_error_405(fd)
  ▼
path existe en filesystem?
  │              │
  │              └─→ send_error(fd, 404)
  ▼
permiso de lectura?
  │              │
  │              └─→ send_error(fd, 403)
  ▼
send_response(fd, 200, mime, body, len)
```

---

## 8. HEAD: respuesta sin body

`HEAD` es idéntico a `GET` pero el servidor **no** envía el body.
El cliente lo usa para verificar si un recurso existe, su tamaño,
o su tipo, sin descargarlo:

```
GET /large-file.zip HTTP/1.1        HEAD /large-file.zip HTTP/1.1

HTTP/1.1 200 OK                     HTTP/1.1 200 OK
Content-Length: 104857600           Content-Length: 104857600
Content-Type: application/zip      Content-Type: application/zip

[100 MB de datos]                   [nada — solo headers]
```

### Implementación

La lógica es idéntica a GET (mismos headers, mismo Content-Length),
pero se omite el write del body:

```c
void handle_request(int fd, http_request_t *req) {
    // ... resolver path, obtener mime type, tamaño ...

    int is_head = (strcmp(req->method, "HEAD") == 0);

    // Enviar headers (idénticos para GET y HEAD)
    char headers[MAX_RESPONSE_HEADERS];
    int header_len = build_response_headers(
        headers, sizeof(headers),
        200, mime_type, file_size);

    write_all(fd, headers, header_len);

    // Enviar body solo si es GET
    if (!is_head) {
        write_all(fd, file_content, file_size);
    }
}
```

### ¿Por qué Content-Length en HEAD?

El RFC requiere que HEAD retorne los **mismos headers** que GET
retornaría. Esto incluye `Content-Length`. El cliente puede usar
HEAD para saber el tamaño del archivo antes de descargarlo:

```bash
# ¿Cuánto pesa este archivo? (sin descargarlo)
curl -I http://localhost:8080/large-file.zip
# Content-Length: 104857600
# → 100 MB, ¿quiero descargarlo?
```

---

## 9. Date y headers estándar

### Header Date

El RFC 7231 dice que un servidor **debe** enviar `Date` en
cada respuesta (excepto 1xx y 5xx). El formato es específico:

```
Date: Thu, 20 Mar 2026 12:00:00 GMT
      ^^^  ^^  ^^^  ^^^^  ^^^^^^^^ ^^^
      día  DD  mes  año   hora     zona (siempre GMT)
```

```c
#include <time.h>

void format_http_date(char *buf, size_t bufsize, time_t t) {
    struct tm *gmt = gmtime(&t);
    strftime(buf, bufsize, "%a, %d %b %Y %H:%M:%S GMT", gmt);
}

// Uso:
char date[128];
format_http_date(date, sizeof(date), time(NULL));
// → "Thu, 20 Mar 2026 12:00:00 GMT"
```

### Header Server

Identifica el software del servidor. Es opcional pero
convencional:

```
Server: minihttpd/1.0
Server: Apache/2.4.52 (Ubuntu)
Server: nginx/1.24.0
```

En producción, algunos administradores lo ocultan por seguridad
(para no revelar versiones vulnerables). Para nuestro proyecto
educativo, lo incluimos.

### Header Last-Modified

Indica cuándo fue la última vez que el archivo fue modificado.
Es importante para caching:

```c
#include <sys/stat.h>

struct stat st;
stat(filepath, &st);

char last_modified[128];
format_http_date(last_modified, sizeof(last_modified), st.st_mtime);
// → "Wed, 15 Mar 2026 08:30:00 GMT"
```

El navegador puede enviar `If-Modified-Since` con esta fecha.
Si el archivo no cambió, respondemos `304 Not Modified` sin body,
ahorrando ancho de banda.

---

## 10. Función de respuesta unificada

Combinamos todo en una función flexible que maneja los headers
comunes y casos especiales:

```c
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

typedef struct {
    int         status_code;
    const char *content_type;
    const void *body;
    size_t      body_len;
    int         is_head;           // no enviar body
    const char *extra_headers;     // headers adicionales (o NULL)
    time_t      last_modified;     // 0 si no aplica
} http_response_t;

int send_http_response(int fd, const http_response_t *resp) {
    char headers[MAX_RESPONSE_HEADERS];
    char date_buf[128];
    time_t now = time(NULL);
    format_http_date(date_buf, sizeof(date_buf), now);

    // Headers obligatorios
    int n = snprintf(headers, sizeof(headers),
        "HTTP/1.1 %d %s\r\n"
        "Server: minihttpd/1.0\r\n"
        "Date: %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n",
        resp->status_code, status_reason(resp->status_code),
        date_buf,
        resp->content_type,
        resp->body_len);

    // Last-Modified (si aplica)
    if (resp->last_modified > 0) {
        char lm_buf[128];
        format_http_date(lm_buf, sizeof(lm_buf), resp->last_modified);
        n += snprintf(headers + n, sizeof(headers) - n,
                      "Last-Modified: %s\r\n", lm_buf);
    }

    // Headers extra (Allow, Location, etc.)
    if (resp->extra_headers != NULL) {
        n += snprintf(headers + n, sizeof(headers) - n,
                      "%s", resp->extra_headers);
    }

    // Línea vacía (fin de headers)
    n += snprintf(headers + n, sizeof(headers) - n, "\r\n");

    // Enviar headers
    if (write_all(fd, headers, n) == -1)
        return -1;

    // Enviar body (solo si no es HEAD y hay body)
    if (!resp->is_head && resp->body_len > 0) {
        if (write_all(fd, resp->body, resp->body_len) == -1)
            return -1;
    }

    return 0;
}
```

### Uso simplificado

```c
// 200 OK con HTML
const char *html = "<html><body><h1>Welcome</h1></body></html>";
http_response_t resp = {
    .status_code   = 200,
    .content_type  = "text/html; charset=utf-8",
    .body          = html,
    .body_len      = strlen(html),
    .is_head       = (strcmp(req->method, "HEAD") == 0),
};
send_http_response(client_fd, &resp);

// 404 Not Found
char body[256];
int len = snprintf(body, sizeof(body),
    "<!DOCTYPE html>\n<html><body>"
    "<h1>404 Not Found</h1>"
    "<p>%s not found on this server.</p>"
    "</body></html>\n", req->path);
http_response_t resp = {
    .status_code  = 404,
    .content_type = "text/html; charset=utf-8",
    .body         = body,
    .body_len     = len,
    .is_head      = (strcmp(req->method, "HEAD") == 0),
};
send_http_response(client_fd, &resp);

// 301 Redirect
http_response_t resp = {
    .status_code    = 301,
    .content_type   = "text/html; charset=utf-8",
    .body           = redirect_body,
    .body_len       = strlen(redirect_body),
    .extra_headers  = "Location: /new-path\r\n",
};
send_http_response(client_fd, &resp);

// 405 Method Not Allowed
http_response_t resp = {
    .status_code    = 405,
    .content_type   = "text/html; charset=utf-8",
    .body           = error_body,
    .body_len       = strlen(error_body),
    .extra_headers  = "Allow: GET, HEAD\r\n",
};
send_http_response(client_fd, &resp);
```

### Flujo completo integrado con el parser

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

    int is_head = (strcmp(req.method, "HEAD") == 0);

    printf("[%s] %s %s\n", client_ip, req.method, req.path);

    // 3. Verificar método
    if (strcmp(req.method, "GET") != 0 &&
        strcmp(req.method, "HEAD") != 0) {
        http_response_t resp = {
            .status_code   = 405,
            .content_type  = "text/html; charset=utf-8",
            .body          = "Method Not Allowed",
            .body_len      = 18,
            .is_head       = is_head,
            .extra_headers = "Allow: GET, HEAD\r\n",
        };
        send_http_response(client_fd, &resp);
        return;
    }

    // 4. Servir recurso (T04 — por ahora, placeholder)
    const char *html =
        "<!DOCTYPE html>\n"
        "<html><body><h1>minihttpd works!</h1></body></html>\n";
    http_response_t resp = {
        .status_code  = 200,
        .content_type = "text/html; charset=utf-8",
        .body         = html,
        .body_len     = strlen(html),
        .is_head      = is_head,
    };
    send_http_response(client_fd, &resp);
}
```

---

## 11. Errores comunes

### Error 1: Content-Length incorrecto

```c
// MAL: calcular Content-Length en headers + body juntos
const char *html = "<html>Hello</html>";
snprintf(response, sizeof(response),
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: %zu\r\n"   // ← calcula antes de construir
    "\r\n"
    "%s", strlen(html), html);  // ← pero html podría tener \0
```

```c
// BIEN: calcular body_len por separado
size_t body_len = strlen(html);
int header_len = snprintf(headers, sizeof(headers),
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: %zu\r\n"
    "\r\n", body_len);
write_all(fd, headers, header_len);
write_all(fd, html, body_len);
```

Si `Content-Length` es menor que el body real, el cliente ignora
los bytes sobrantes. Si es mayor, el cliente espera más datos y
la conexión se cuelga (timeout).

### Error 2: olvidar \r\n al final de los headers

```c
// MAL: sin línea vacía final
snprintf(headers, size,
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Content-Length: 5\r\n");
// Falta \r\n → el cliente piensa que siguen más headers
// → el body "Hello" es interpretado como un header malformado
```

**Solución**: siempre terminar con `\r\n\r\n` (la última línea
de header + la línea vacía):

```c
snprintf(headers, size,
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Content-Length: 5\r\n"
    "\r\n");                        // ← línea vacía obligatoria
```

### Error 3: usar strlen para body binario

```c
// MAL: strlen en contenido binario (imágenes, PDFs)
size_t len = strlen(file_content);
// Un archivo PNG tiene bytes \0 → strlen retorna un valor menor
// → Content-Length incorrecto → imagen cortada
```

**Solución**: usar `stat()` o `fread()` retorno para el tamaño:

```c
struct stat st;
stat(filepath, &st);
size_t len = st.st_size;  // tamaño real del archivo
```

### Error 4: formato de Date incorrecto

```c
// MAL: fecha en formato local
time_t now = time(NULL);
char *date = ctime(&now);
// → "Thu Mar 20 12:00:00 2026\n"  (formato ctime, no HTTP)
```

**Solución**: usar `strftime` con formato RFC 7231 y GMT:

```c
struct tm *gmt = gmtime(&now);
strftime(date_buf, sizeof(date_buf),
         "%a, %d %b %Y %H:%M:%S GMT", gmt);
// → "Thu, 20 Mar 2026 12:00:00 GMT"
```

### Error 5: enviar body con status 204 o 304

```c
// MAL: 204 No Content con body
send_response(fd, 204, "text/plain", "OK", 2);
// RFC dice: 204 y 304 NO DEBEN tener body
// Algunos proxies descartan el body, otros se confunden
```

**Solución**: para 204 y 304, Content-Length: 0 y sin body:

```c
send_response(fd, 204, "text/plain", NULL, 0);
// → Content-Length: 0, sin body
```

---

## 12. Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│               Response HTTP                                  │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  FORMATO:                                                    │
│    HTTP/1.1 CODE REASON\r\n                                  │
│    Header: Value\r\n                                         │
│    \r\n                                                      │
│    [body]                                                    │
│                                                              │
│  CÓDIGOS FRECUENTES:                                         │
│    200 OK                    éxito                            │
│    301 Moved Permanently     redirect + Location header      │
│    304 Not Modified          cache válido, sin body          │
│    400 Bad Request           petición malformada             │
│    403 Forbidden             sin permiso                     │
│    404 Not Found             recurso no existe               │
│    405 Method Not Allowed    + Allow header obligatorio      │
│    500 Internal Server Error error del servidor              │
│                                                              │
│  HEADERS OBLIGATORIOS:                                       │
│    Content-Type      tipo MIME del body                      │
│    Content-Length     tamaño exacto del body (bytes)          │
│    Date              fecha GMT formato RFC 7231              │
│    Connection: close simplifica nuestro servidor             │
│                                                              │
│  CONTENT-TYPE COMUNES:                                       │
│    text/html; charset=utf-8     .html                        │
│    text/css                     .css                          │
│    application/javascript       .js                           │
│    application/json             .json                        │
│    image/png                    .png                          │
│    image/jpeg                   .jpg                          │
│    application/octet-stream     binario genérico             │
│                                                              │
│  MIME LOOKUP:                                                │
│    strrchr(path, '.')  encontrar extensión                   │
│    strcasecmp(ext, ".html")  comparar case-insensitive       │
│                                                              │
│  HEAD vs GET:                                                │
│    Mismos headers (incluyendo Content-Length)                 │
│    HEAD: NO enviar body                                      │
│    GET: enviar body                                          │
│                                                              │
│  DATE FORMAT:                                                │
│    gmtime() + strftime("%a, %d %b %Y %H:%M:%S GMT")         │
│    Siempre GMT, nunca hora local                             │
│                                                              │
│  ENVÍO:                                                      │
│    write_all(fd, headers, hdr_len)  headers (texto)          │
│    write_all(fd, body, body_len)    body (puede ser binario) │
│    Loop de write con EINTR retry                             │
│                                                              │
│  REGLAS:                                                     │
│    Content-Length debe ser EXACTO                             │
│    204 y 304: sin body                                       │
│    405: requiere Allow header                                │
│    3xx: requiere Location header                             │
│    Body binario: no usar strlen, usar stat().st_size         │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 13. Ejercicios

### Ejercicio 1: servidor con páginas de error HTML

Implementa un servidor que responda con páginas de error
formateadas en HTML.

**Pasos**:
1. Implementa `status_reason()` para al menos 200, 400, 403,
   404, 405, 500
2. Implementa `send_error()` que genere un body HTML con
   `<h1>CODE REASON</h1>` y envíe con Content-Type text/html
3. Implementa `send_error_405()` con el header `Allow: GET, HEAD`
4. En handle_client: parsear request → verificar método →
   responder 200 con "Hello" o error apropiado
5. Prueba cada error: petición malformada (400), método POST (405)

**Predicción antes de ejecutar**: al hacer `curl -X DELETE
localhost:8080/`, ¿qué status code recibes? Si haces
`curl -v -X DELETE ...`, ¿ves el header `Allow` en la respuesta?
¿El body HTML se muestra en curl o solo en el navegador?

> **Pregunta de reflexión**: un servidor real como nginx tiene
> páginas de error personalizables (error_page 404 /404.html).
> ¿Cómo implementarías esto? ¿Deberías servir la página de
> error como un archivo del filesystem, o generarla en código?
> ¿Qué Content-Length usarías si la página de error es un
> template con el path del recurso embebido?

### Ejercicio 2: MIME types y HEAD

Implementa detección de MIME type y soporte para HEAD requests.

**Pasos**:
1. Crea la tabla de MIME types con al menos .html, .css, .js,
   .png, .jpg, .txt, .json
2. Implementa `get_mime_type()` usando strrchr + strcasecmp
3. Modifica handle_client para detectar HEAD y no enviar body
4. Verifica que HEAD retorna los mismos headers que GET
   (incluyendo Content-Length)
5. Prueba: `curl -I localhost:8080/style.css` vs
   `curl localhost:8080/style.css`

**Predicción antes de ejecutar**: `curl -I` envía una petición
HEAD. ¿Cuántos bytes transfiere el servidor si el archivo
style.css pesa 10KB? ¿Content-Length dice 10240 aunque no se
envíe body? ¿Qué Content-Type retorna para un archivo sin
extensión?

> **Pregunta de reflexión**: la detección de MIME type por
> extensión es simple pero falible — un archivo .html podría
> contener texto plano, o un archivo sin extensión podría ser
> una imagen. El comando `file` de Linux detecta el tipo por
> contenido (magic bytes). ¿Cuáles son las ventajas y
> desventajas de cada enfoque? ¿Por qué los servidores HTTP
> reales usan extensión y no magic bytes?

### Ejercicio 3: respuesta con headers dinámicos

Implementa `send_http_response()` con la estructura
`http_response_t` y genera respuestas con headers dinámicos.

**Pasos**:
1. Define `http_response_t` con status_code, content_type,
   body, body_len, is_head, extra_headers, last_modified
2. Implementa `format_http_date()` con gmtime + strftime
3. Implementa `send_http_response()` que construya los headers
   (incluyendo Date, Server, Last-Modified si aplica, extra_headers)
4. Prueba: `GET /` → 200 con Date y Server, `GET /old-page` →
   301 con Location, `POST /` → 405 con Allow
5. Verifica con `curl -v` que todos los headers están presentes

**Predicción antes de ejecutar**: ¿el header Date cambia entre
dos peticiones separadas por un segundo? ¿Y entre dos peticiones
en el mismo segundo? ¿Qué pasa si el servidor envía
`Last-Modified` con una fecha futura — cómo afecta el caching?

> **Pregunta de reflexión**: construimos la respuesta en un
> buffer estático con snprintf. ¿Qué pasa si el buffer es
> demasiado pequeño para los headers? snprintf retorna cuántos
> bytes **habría** escrito, no cuántos escribió. ¿Cómo usarías
> el valor de retorno de snprintf para detectar truncamiento y
> manejarlo?
