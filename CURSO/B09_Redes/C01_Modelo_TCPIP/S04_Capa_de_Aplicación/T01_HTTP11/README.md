# HTTP/1.1

## Índice
1. [Qué es HTTP](#qué-es-http)
2. [Evolución del protocolo](#evolución-del-protocolo)
3. [Estructura de una petición](#estructura-de-una-petición)
4. [Estructura de una respuesta](#estructura-de-una-respuesta)
5. [Métodos HTTP](#métodos-http)
6. [Códigos de estado](#códigos-de-estado)
7. [Conexiones persistentes](#conexiones-persistentes)
8. [Negociación de contenido](#negociación-de-contenido)
9. [Caché básica](#caché-básica)
10. [curl como herramienta de diagnóstico](#curl-como-herramienta-de-diagnóstico)
11. [Errores comunes](#errores-comunes)
12. [Cheatsheet](#cheatsheet)
13. [Ejercicios](#ejercicios)

---

## Qué es HTTP

HTTP (HyperText Transfer Protocol) es un protocolo de capa de aplicación diseñado para la transferencia de documentos hipermedia. Opera sobre TCP (puerto 80 por defecto, 443 con TLS) y sigue un modelo **request/response**: el cliente envía una petición, el servidor devuelve una respuesta.

```
┌─────────────────────────────────────────────────────────┐
│               Modelo request/response                   │
│                                                         │
│   Cliente                              Servidor         │
│   ┌──────┐    request (GET /index)     ┌──────┐        │
│   │      │ ──────────────────────────► │      │        │
│   │ curl │                             │ nginx│        │
│   │      │ ◄────────────────────────── │      │        │
│   └──────┘    response (200 OK)        └──────┘        │
│                                                         │
│   Puerto efímero ──────► Puerto 80/443                  │
│   (ej. 52431)            (well-known)                   │
└─────────────────────────────────────────────────────────┘
```

Características fundamentales de HTTP:

- **Stateless**: cada petición es independiente; el servidor no recuerda peticiones anteriores (las cookies y sesiones son mecanismos adicionales, no parte del protocolo base).
- **Basado en texto** (en HTTP/1.1): las cabeceras son legibles en ASCII.
- **Extensible**: las cabeceras permiten añadir funcionalidad sin cambiar el protocolo.

---

## Evolución del protocolo

```
┌──────────────────────────────────────────────────────────────────┐
│                    Línea temporal de HTTP                         │
│                                                                  │
│  1991      1996       1997        1999        2015       2022    │
│   │         │          │           │           │          │      │
│   ▼         ▼          ▼           ▼           ▼          ▼      │
│  0.9       1.0        1.1       1.1bis        2.0        3.0    │
│  (GET      (headers,  (persist.  (RFC 2616)  (binary,   (QUIC, │
│  only)     POST)      conn,      reescrito   mux)       UDP)   │
│                       Host,      RFC 7230-                      │
│                       chunked)   7235)                          │
└──────────────────────────────────────────────────────────────────┘
```

| Versión | RFC | Cambio clave |
|---------|-----|-------------|
| HTTP/0.9 | — | Solo `GET`, sin cabeceras, solo HTML |
| HTTP/1.0 | 1945 | Cabeceras, `POST`, `Content-Type`, una conexión por request |
| HTTP/1.1 | 2616 → 7230-7235 | Conexiones persistentes, `Host` obligatorio, chunked transfer, pipelining |
| HTTP/2 | 7540 → 9113 | Binary framing, multiplexing, server push (tema T04) |
| HTTP/3 | 9114 | QUIC sobre UDP, 0-RTT (tema T04) |

> **Nota**: HTTP/1.1 sigue siendo el protocolo más desplegado. Incluso sitios que soportan HTTP/2 y HTTP/3 mantienen compatibilidad con 1.1. Este tema se centra exclusivamente en HTTP/1.1.

---

## Estructura de una petición

Una petición HTTP/1.1 tiene tres partes: **línea de petición**, **cabeceras** y **cuerpo** (opcional).

```
┌──────────────────────────────────────────────────────────┐
│                   HTTP Request                           │
├──────────────────────────────────────────────────────────┤
│  Request line:                                           │
│  GET /api/users?page=2 HTTP/1.1\r\n                     │
│  ─────────────────────────────────                       │
│  │    │                   │                              │
│  │    │                   └── Versión del protocolo      │
│  │    └── Request-URI (ruta + query string)              │
│  └── Método                                              │
├──────────────────────────────────────────────────────────┤
│  Headers:                                                │
│  Host: api.example.com\r\n          ← obligatorio en 1.1│
│  User-Agent: curl/8.5.0\r\n                             │
│  Accept: application/json\r\n                            │
│  Authorization: Bearer eyJhb...\r\n                      │
│  \r\n                               ← línea vacía = fin │
├──────────────────────────────────────────────────────────┤
│  Body (opcional):                                        │
│  (vacío en GET)                                          │
└──────────────────────────────────────────────────────────┘
```

Observaciones importantes:

- Cada línea termina con `\r\n` (CRLF), no solo `\n`.
- La cabecera `Host` es **obligatoria** en HTTP/1.1 — sin ella, el servidor no puede distinguir entre virtual hosts que comparten la misma IP.
- La línea vacía (`\r\n\r\n`) marca el fin de las cabeceras y el inicio del cuerpo.

### Ejemplo real con curl

```bash
# -v muestra la comunicación completa (> = enviado, < = recibido)
curl -v http://example.com/
```

Salida relevante (lo que envía el cliente):

```
> GET / HTTP/1.1
> Host: example.com
> User-Agent: curl/8.5.0
> Accept: */*
>
```

---

## Estructura de una respuesta

```
┌──────────────────────────────────────────────────────────┐
│                   HTTP Response                          │
├──────────────────────────────────────────────────────────┤
│  Status line:                                            │
│  HTTP/1.1 200 OK\r\n                                    │
│  ──────────────────                                      │
│  │        │   │                                          │
│  │        │   └── Reason phrase (informativa, ignorable) │
│  │        └── Status code                                │
│  └── Versión                                             │
├──────────────────────────────────────────────────────────┤
│  Headers:                                                │
│  Content-Type: text/html; charset=UTF-8\r\n              │
│  Content-Length: 1256\r\n                                 │
│  Date: Fri, 21 Mar 2026 10:30:00 GMT\r\n                │
│  Server: nginx/1.24.0\r\n                                │
│  Connection: keep-alive\r\n                              │
│  \r\n                                                    │
├──────────────────────────────────────────────────────────┤
│  Body:                                                   │
│  <!doctype html>                                         │
│  <html>                                                  │
│  <head><title>Example</title></head>                     │
│  ...                                                     │
│  </html>                                                 │
└──────────────────────────────────────────────────────────┘
```

La **reason phrase** (ej. "OK", "Not Found") es meramente informativa. En HTTP/2 y posteriores fue eliminada. Los clientes deben basarse únicamente en el código numérico.

### ¿Cómo sabe el cliente cuántos bytes leer del body?

Hay dos mecanismos principales:

1. **Content-Length**: indica el tamaño exacto en bytes.
2. **Transfer-Encoding: chunked**: el cuerpo se envía en fragmentos, cada uno precedido por su tamaño en hexadecimal; un chunk de tamaño `0` marca el final.

```
Transfer-Encoding: chunked

┌─────────────────────────────────────────┐
│  7\r\n              ← tamaño: 7 bytes   │
│  Mozilla\r\n        ← datos             │
│  9\r\n              ← tamaño: 9 bytes   │
│  Developer\r\n      ← datos             │
│  0\r\n              ← fin               │
│  \r\n                                    │
└─────────────────────────────────────────┘
```

Chunked transfer es esencial cuando el servidor no conoce el tamaño total de antemano (ej. respuestas generadas dinámicamente, streaming).

---

## Métodos HTTP

### Métodos principales

| Método | Semántica | Body en request | Idempotente | Safe |
|--------|-----------|:---------------:|:-----------:|:----:|
| `GET` | Obtener recurso | No | Sí | Sí |
| `HEAD` | Igual que GET pero sin body en respuesta | No | Sí | Sí |
| `POST` | Crear recurso / ejecutar acción | Sí | **No** | No |
| `PUT` | Reemplazar recurso completo | Sí | Sí | No |
| `PATCH` | Modificar parcialmente un recurso | Sí | **No** | No |
| `DELETE` | Eliminar recurso | Opcional | Sí | No |
| `OPTIONS` | Consultar capacidades del servidor | No | Sí | Sí |

**Definiciones clave**:

- **Safe** (seguro): no modifica el estado del servidor. `GET`, `HEAD`, `OPTIONS` son safe.
- **Idempotente**: ejecutar N veces produce el mismo resultado que ejecutar 1 vez. `PUT` es idempotente (reemplaza el recurso con el mismo contenido), `POST` no lo es (cada llamada puede crear un recurso nuevo).

### POST vs PUT vs PATCH

```
┌─────────────────────────────────────────────────────────────┐
│                 POST vs PUT vs PATCH                        │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  POST /api/users                                            │
│  {"name": "Ana", "role": "admin"}                           │
│  → Crea un NUEVO usuario. El servidor asigna el ID.         │
│  → Response: 201 Created, Location: /api/users/42           │
│                                                             │
│  PUT /api/users/42                                          │
│  {"name": "Ana", "role": "editor"}                          │
│  → REEMPLAZA el usuario 42 completo.                        │
│  → Si falta un campo, se borra (es un reemplazo total).     │
│                                                             │
│  PATCH /api/users/42                                        │
│  {"role": "editor"}                                         │
│  → Modifica SOLO el campo "role".                           │
│  → Los demás campos permanecen intactos.                    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

> **En la práctica**: muchas APIs usan `PUT` como si fuera `PATCH` (aceptando actualizaciones parciales). Esto viola la especificación, pero es extremadamente común.

### Métodos menos habituales

| Método | Uso |
|--------|-----|
| `TRACE` | Eco de la petición — para diagnóstico de proxies. Deshabilitado en la mayoría de servidores por seguridad (Cross-Site Tracing). |
| `CONNECT` | Establece un túnel TCP a través de un proxy (usado por HTTPS a través de proxy HTTP). |

---

## Códigos de estado

Los códigos de estado son números de tres dígitos agrupados en cinco familias:

```
┌─────────────────────────────────────────────────────────────┐
│              Familias de códigos de estado                   │
├──────┬──────────────────────────────────────────────────────┤
│ 1xx  │  Informational — la petición se está procesando      │
│ 2xx  │  Success — la petición se completó correctamente     │
│ 3xx  │  Redirection — se necesita acción adicional          │
│ 4xx  │  Client Error — el cliente envió algo incorrecto     │
│ 5xx  │  Server Error — el servidor falló al procesar        │
└──────┴──────────────────────────────────────────────────────┘
```

### Códigos que debes conocer de memoria

#### 2xx — Éxito

| Código | Nombre | Significado |
|--------|--------|-------------|
| 200 | OK | Petición exitosa. El significado depende del método. |
| 201 | Created | Se creó un nuevo recurso (típico en `POST`). |
| 204 | No Content | Éxito pero sin body en respuesta (típico en `DELETE`). |

#### 3xx — Redirección

| Código | Nombre | Significado |
|--------|--------|-------------|
| 301 | Moved Permanently | El recurso se movió permanentemente. Los clientes y buscadores actualizan la URL. |
| 302 | Found | Redirección temporal (en la práctica, se usa mal — ver nota). |
| 304 | Not Modified | El recurso no cambió desde la última vez (caché válida). |
| 307 | Temporary Redirect | Como 302, pero **garantiza** que no cambia el método (POST sigue siendo POST). |
| 308 | Permanent Redirect | Como 301, pero **garantiza** que no cambia el método. |

> **La confusión 301/302**: históricamente, los navegadores cambiaban `POST` a `GET` al seguir un 301 o 302, violando la especificación. Por eso se crearon 307 y 308, que prohíben explícitamente el cambio de método.

#### 4xx — Error del cliente

| Código | Nombre | Significado |
|--------|--------|-------------|
| 400 | Bad Request | Petición malformada o inválida. |
| 401 | Unauthorized | Falta autenticación (el nombre es engañoso: debería ser "Unauthenticated"). |
| 403 | Forbidden | Autenticado pero sin permisos (este es el verdadero "unauthorized"). |
| 404 | Not Found | El recurso no existe. |
| 405 | Method Not Allowed | El método no está soportado para esta URI. |
| 409 | Conflict | Conflicto con el estado actual del recurso (ej. versión desactualizada). |
| 429 | Too Many Requests | Rate limiting — demasiadas peticiones en un periodo. |

#### 5xx — Error del servidor

| Código | Nombre | Significado |
|--------|--------|-------------|
| 500 | Internal Server Error | Error genérico del servidor. |
| 502 | Bad Gateway | Un proxy/reverse proxy recibió una respuesta inválida del upstream. |
| 503 | Service Unavailable | Servidor sobrecargado o en mantenimiento. |
| 504 | Gateway Timeout | El proxy no recibió respuesta del upstream a tiempo. |

### 401 vs 403: la distinción crítica

```
┌──────────────────────────────────────────────────┐
│  401 Unauthorized (mal nombre → "Unauthenticated")│
│  ─────────────────────────────────────────────    │
│  "¿Quién eres? No te conozco."                   │
│  → El cliente debe enviar credenciales válidas.   │
│  → El servidor incluye WWW-Authenticate header.   │
│                                                   │
│  403 Forbidden (el verdadero "Unauthorized")      │
│  ─────────────────────────────────────────────    │
│  "Sé quién eres, pero NO tienes permiso."         │
│  → No importa cuántas veces lo intentes.          │
│  → Reautenticarse no ayuda.                       │
└──────────────────────────────────────────────────┘
```

### 502 vs 504: diagnóstico en reverse proxy

```
                        ┌─────────┐
    Client ──────────►  │  nginx  │ ──────────► Backend (app)
                        │ (proxy) │
                        └─────────┘

    502 Bad Gateway:   nginx contactó al backend pero la respuesta
                       fue inválida (ej. backend crasheó a medio envío)

    504 Gateway Timeout: nginx NO recibió respuesta a tiempo
                        (ej. backend congelado, query lenta a BD)
```

---

## Conexiones persistentes

En HTTP/1.0, cada request requería una nueva conexión TCP (3-way handshake + transferencia + cierre). Con múltiples recursos por página (HTML + CSS + JS + imágenes), esto era enormemente ineficiente.

HTTP/1.1 introdujo **conexiones persistentes** como comportamiento por defecto:

```
┌────────────────────────────────────────────────────────────────┐
│            HTTP/1.0                    HTTP/1.1                │
│                                                                │
│  TCP handshake  ──┐               TCP handshake  ──┐          │
│  GET /index.html  │               GET /index.html  │          │
│  Response         │               Response         │          │
│  TCP close      ──┘               GET /style.css   │ misma    │
│                                    Response         │ conexión │
│  TCP handshake  ──┐               GET /app.js      │ TCP      │
│  GET /style.css   │               Response         │          │
│  Response         │               GET /logo.png    │          │
│  TCP close      ──┘               Response         │          │
│                                    TCP close      ──┘          │
│  TCP handshake  ──┐                                            │
│  GET /app.js      │            Ahorro: 3 handshakes           │
│  Response         │                                            │
│  TCP close      ──┘                                            │
└────────────────────────────────────────────────────────────────┘
```

### Cabecera Connection

```bash
# HTTP/1.1: keep-alive es el default (no hace falta especificarlo)
Connection: keep-alive

# Para cerrar la conexión después de esta respuesta:
Connection: close
```

El servidor puede cerrar la conexión en cualquier momento (tras un timeout de inactividad, por límite de requests, etc.). El cliente debe estar preparado para reenviar la petición en una nueva conexión.

### Pipelining vs Head-of-Line Blocking

HTTP/1.1 permite **pipelining**: enviar múltiples peticiones sin esperar la respuesta de cada una. Sin embargo, las respuestas **deben** llegar en el mismo orden que las peticiones:

```
┌──────────────────────────────────────────────────────┐
│                    Pipelining                        │
│                                                      │
│  Cliente envía:    GET /a   GET /b   GET /c          │
│                    ──────►  ──────►  ──────►         │
│                                                      │
│  Servidor responde: Resp /a   Resp /b   Resp /c      │
│                     ◄──────  ◄──────  ◄──────        │
│                     (orden estricto)                  │
│                                                      │
│  Problema (Head-of-Line Blocking):                   │
│  Si /a es lento (ej. query pesada a BD),             │
│  /b y /c quedan bloqueados esperando.                │
│                                                      │
│  GET /a ─── esperando ─── Resp /a  Resp /b  Resp /c  │
│              3 seg ↑                                  │
│              /b y /c listas pero bloqueadas           │
└──────────────────────────────────────────────────────┘
```

> **En la práctica**: pipelining está deshabilitado en la mayoría de navegadores modernos por problemas de compatibilidad. Los navegadores usan **múltiples conexiones TCP paralelas** (típicamente 6 por dominio) como workaround. HTTP/2 resuelve este problema con multiplexing real.

---

## Negociación de contenido

El cliente puede indicar qué formatos acepta, y el servidor elige el más apropiado:

```bash
# El cliente acepta JSON preferentemente, pero también XML
Accept: application/json, application/xml;q=0.9, */*;q=0.1
```

El parámetro `q` (quality) indica preferencia de 0 a 1 (1 es el default si no se especifica):

| Cabecera | Negocia |
|----------|---------|
| `Accept` | Tipo de contenido (MIME type) |
| `Accept-Language` | Idioma (es, en, fr) |
| `Accept-Encoding` | Compresión (gzip, br, deflate) |
| `Accept-Charset` | Codificación de caracteres (obsoleta, UTF-8 es universal) |

```bash
# Ejemplo: cliente que prefiere español, acepta inglés
Accept-Language: es;q=1.0, en;q=0.8

# El servidor responde con Content-Language indicando qué eligió
Content-Language: es
```

---

## Caché básica

HTTP incluye mecanismos de caché integrados que reducen la carga del servidor y mejoran los tiempos de respuesta.

### Modelo de expiración

El servidor indica por cuánto tiempo la respuesta es válida:

```
Cache-Control: max-age=3600     ← válida por 1 hora
Cache-Control: no-cache         ← se puede almacenar, pero revalidar siempre
Cache-Control: no-store         ← NO almacenar en absoluto (datos sensibles)
```

> **Cuidado**: `no-cache` **no** significa "no cachear". Significa "cachea si quieres, pero pregúntame antes de usarlo". El que prohíbe el almacenamiento es `no-store`.

### Modelo de validación

Cuando la caché expira, el cliente puede preguntar al servidor si el recurso cambió sin descargar todo de nuevo:

```
┌─────────────────────────────────────────────────────────────────┐
│                Validación condicional                            │
│                                                                  │
│  1. Primera petición:                                            │
│     GET /data.json                                               │
│     →  200 OK                                                    │
│        ETag: "a1b2c3"                                            │
│        Last-Modified: Wed, 18 Mar 2026 12:00:00 GMT              │
│                                                                  │
│  2. Petición condicional (tras expiración):                      │
│     GET /data.json                                               │
│     If-None-Match: "a1b2c3"                                      │
│     If-Modified-Since: Wed, 18 Mar 2026 12:00:00 GMT             │
│                                                                  │
│  3a. Si NO cambió:                                               │
│      →  304 Not Modified  (sin body, ahorra ancho de banda)      │
│                                                                  │
│  3b. Si cambió:                                                  │
│      →  200 OK  (body completo con nuevo ETag)                   │
└─────────────────────────────────────────────────────────────────┘
```

| Mecanismo | Cabecera del servidor | Cabecera condicional del cliente |
|-----------|----------------------|--------------------------------|
| ETag | `ETag: "hash"` | `If-None-Match: "hash"` |
| Fecha | `Last-Modified: date` | `If-Modified-Since: date` |

ETag es más preciso que Last-Modified (puede detectar cambios dentro del mismo segundo).

---

## curl como herramienta de diagnóstico

`curl` es la herramienta fundamental para interactuar con HTTP desde la terminal:

### Peticiones básicas

```bash
# GET simple (muestra solo el body)
curl http://example.com

# Ver cabeceras de respuesta (-I = solo HEAD, -i = headers + body)
curl -I http://example.com       # Solo cabeceras (método HEAD)
curl -i http://example.com       # Cabeceras + body

# Comunicación completa con -v (verbose)
curl -v http://example.com       # Muestra request Y response headers

# Guardar en archivo
curl -o page.html http://example.com
```

### Enviar datos

```bash
# POST con JSON
curl -X POST http://api.example.com/users \
  -H "Content-Type: application/json" \
  -d '{"name": "Ana", "role": "admin"}'

# PUT
curl -X PUT http://api.example.com/users/42 \
  -H "Content-Type: application/json" \
  -d '{"name": "Ana", "role": "editor"}'

# DELETE
curl -X DELETE http://api.example.com/users/42

# POST con datos de formulario (application/x-www-form-urlencoded)
curl -X POST http://example.com/login \
  -d "user=ana&password=secret"
```

### Opciones avanzadas

```bash
# Seguir redirecciones (-L)
curl -L http://example.com/old-page

# Limitar tiempo de conexión
curl --connect-timeout 5 --max-time 30 http://example.com

# Ver solo el código de estado
curl -o /dev/null -s -w "%{http_code}\n" http://example.com

# Enviar cabecera personalizada
curl -H "Authorization: Bearer TOKEN123" http://api.example.com/me

# Ver tiempos detallados (DNS, connect, TLS, transfer)
curl -o /dev/null -s -w "\
  DNS:       %{time_namelookup}s\n\
  Connect:   %{time_connect}s\n\
  TLS:       %{time_appconnect}s\n\
  Start:     %{time_starttransfer}s\n\
  Total:     %{time_total}s\n" \
  https://example.com
```

### Interpretar la salida de curl -v

```
*   Trying 93.184.216.34:80...         ← Conexión TCP
* Connected to example.com             ← Handshake completado
> GET / HTTP/1.1                       ← Request line (enviada)
> Host: example.com                    ← Headers enviados
> User-Agent: curl/8.5.0              ← (> = lo que envía curl)
> Accept: */*
>
< HTTP/1.1 200 OK                      ← Status line (recibida)
< Content-Type: text/html              ← Headers recibidos
< Content-Length: 1256                  ← (< = lo que recibe curl)
< Date: Fri, 21 Mar 2026 10:30:00 GMT
<
<!doctype html>                        ← Body
...
```

Prefijos de la salida verbose:
- `*` — información de curl (conexión, TLS, etc.)
- `>` — datos enviados por curl (request)
- `<` — datos recibidos del servidor (response)

---

## Errores comunes

### 1. Confundir `no-cache` con `no-store`

```
# INCORRECTO: pensar que no-cache prohíbe el caché
Cache-Control: no-cache    ← SÍ cachea, pero revalida siempre

# CORRECTO: para prohibir almacenamiento
Cache-Control: no-store    ← NO almacena en absoluto
```

`no-cache` + revalidación puede seguir devolviendo 304, ahorrando ancho de banda. `no-store` fuerza descarga completa siempre.

### 2. Usar `PUT` para actualizaciones parciales

```bash
# INCORRECTO: enviar solo un campo con PUT
curl -X PUT http://api.example.com/users/42 \
  -H "Content-Type: application/json" \
  -d '{"role": "editor"}'
# → El usuario 42 PIERDE los campos no enviados (name, email, etc.)

# CORRECTO: usar PATCH para cambios parciales
curl -X PATCH http://api.example.com/users/42 \
  -H "Content-Type: application/json" \
  -d '{"role": "editor"}'
```

### 3. Confundir 401 y 403

```
401 → "No sé quién eres"  → Solución: enviar credenciales válidas
403 → "Sé quién eres pero no tienes permiso" → Solución: obtener el permiso adecuado
```

Muchas APIs devuelven 403 para todo, pero la distinción afecta al comportamiento del cliente: ante un 401, el cliente puede pedir login; ante un 403, no tiene sentido re-autenticarse.

### 4. No enviar `Host` al hacer peticiones manuales

```bash
# INCORRECTO: conectar por IP sin Host
# El servidor no sabe qué virtual host servir
curl http://93.184.216.34/    # ← ¿cuál de los 50 sitios en esa IP?

# CORRECTO
curl -H "Host: example.com" http://93.184.216.34/
```

En HTTP/1.1, `Host` es la **única cabecera obligatoria**. Sin ella, el servidor devuelve 400 Bad Request.

### 5. Asumir que el body siempre usa Content-Length

```
# El body puede enviarse chunked cuando el servidor no conoce el tamaño:
Transfer-Encoding: chunked

# Regla: si ves Transfer-Encoding: chunked, NO habrá Content-Length
# (son mutuamente excluyentes en una respuesta válida)
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│                      HTTP/1.1 Cheatsheet                     │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  ESTRUCTURA REQUEST:                                         │
│    MÉTODO /ruta HTTP/1.1\r\n                                │
│    Host: dominio\r\n                                         │
│    [Headers]\r\n                                             │
│    \r\n                                                      │
│    [Body]                                                    │
│                                                              │
│  ESTRUCTURA RESPONSE:                                        │
│    HTTP/1.1 CÓDIGO Reason\r\n                                │
│    [Headers]\r\n                                             │
│    \r\n                                                      │
│    [Body]                                                    │
│                                                              │
│  MÉTODOS:                                                    │
│    GET      → obtener         POST    → crear                │
│    PUT      → reemplazar      PATCH   → modificar parcial    │
│    DELETE   → eliminar        HEAD    → GET sin body         │
│    OPTIONS  → capacidades                                    │
│                                                              │
│  CÓDIGOS CLAVE:                                              │
│    200 OK          201 Created       204 No Content          │
│    301 Moved Perm  304 Not Modified  307 Temp Redirect       │
│    400 Bad Request 401 Unauthn       403 Forbidden           │
│    404 Not Found   429 Too Many      405 Method Not Allowed  │
│    500 Internal    502 Bad Gateway   503 Unavailable         │
│    504 Gateway Timeout                                       │
│                                                              │
│  SAFE:        GET, HEAD, OPTIONS                             │
│  IDEMPOTENT:  GET, HEAD, OPTIONS, PUT, DELETE                │
│  NEITHER:     POST, PATCH                                    │
│                                                              │
│  CURL RÁPIDO:                                                │
│    curl -v URL              verbose (debug completo)         │
│    curl -I URL              solo cabeceras (HEAD)            │
│    curl -i URL              cabeceras + body                 │
│    curl -L URL              seguir redirecciones             │
│    curl -X POST -d 'data'   enviar datos                     │
│    curl -H "Key: Val"       cabecera personalizada           │
│    curl -o /dev/null -s -w "%{http_code}" URL   solo código  │
│                                                              │
│  CACHÉ:                                                      │
│    no-store  = no almacenar nunca                            │
│    no-cache  = almacenar pero revalidar siempre              │
│    max-age=N = válido por N segundos                         │
│    ETag + If-None-Match → validación por hash                │
│    Last-Modified + If-Modified-Since → validación por fecha  │
│    304 = no cambió, usa tu caché                             │
│                                                              │
│  KEEP-ALIVE:                                                 │
│    HTTP/1.1 = persistente por defecto                        │
│    Connection: close → cierra tras esta respuesta            │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Interpretar una transacción HTTP completa

Dada esta salida de `curl -v`:

```
> POST /api/orders HTTP/1.1
> Host: shop.example.com
> Content-Type: application/json
> Content-Length: 45
>
> {"product_id": 101, "quantity": 2}

< HTTP/1.1 201 Created
< Location: /api/orders/5873
< Content-Type: application/json
< Content-Length: 78
<
< {"id": 5873, "product_id": 101, "quantity": 2, "status": "pending"}
```

**Responde**:
1. ¿Qué método HTTP se usó y por qué es apropiado para esta operación?
2. ¿Qué significa el código de estado 201?
3. ¿Para qué sirve la cabecera `Location` en la respuesta?
4. Si repitieras esta misma petición POST 3 veces, ¿cuántas órdenes se crearían? ¿Sería diferente si usaras PUT?
5. ¿Es esta petición safe? ¿Idempotente?

**Pregunta de reflexión**: ¿Por qué HTTP eligió separar los conceptos de "safe" e "idempotente" en lugar de tener un solo concepto de "operación sin efectos secundarios"?

---

### Ejercicio 2: Diagnóstico con códigos de estado

Un administrador de sistemas opera un reverse proxy nginx que redirige tráfico a tres backends. Analiza estos escenarios y di qué código de estado recibiría el cliente y por qué:

1. El usuario accede a `/admin` sin un token de autenticación.
2. El usuario tiene un token válido pero su rol es "viewer" y `/admin` requiere rol "admin".
3. El backend está procesando una query SQL que tarda 45 segundos y nginx tiene `proxy_read_timeout 30s`.
4. El backend crasheó y nginx recibe un TCP RST al intentar conectar.
5. El usuario accede a `/old-docs` que fue movido permanentemente a `/docs`.
6. El usuario envía un JSON malformado: `{"name": "Ana",}` (coma extra).
7. El usuario hace 200 requests en 10 segundos y el rate limiter se activa.

**Pregunta de reflexión**: ¿Por qué es importante para un sysadmin distinguir entre 502 y 504? ¿Qué pasos de diagnóstico diferentes seguirías en cada caso?

---

### Ejercicio 3: Diseñar requests con curl

Usando `curl`, escribe los comandos para las siguientes operaciones contra una API REST en `http://localhost:8080`:

1. Obtener la lista de todos los usuarios en formato JSON.
2. Crear un nuevo usuario con nombre "Carlos" y email "carlos@example.com".
3. Actualizar **solo** el email del usuario 15 a "carlos_nuevo@example.com" (sin modificar otros campos).
4. Eliminar el usuario 15.
5. Verificar que el usuario 15 ya no existe (esperas un 404), mostrando **solo** el código de estado.
6. Medir el tiempo total de respuesta del endpoint `/api/health`.

Después de escribir los comandos, responde:
- ¿Cuál de los comandos anteriores podrías ejecutar repetidamente sin que cambie el resultado? (idempotencia)
- ¿Cuál sería peligroso ejecutar repetidamente y por qué?

**Pregunta de reflexión**: Si el servidor responde con `Connection: close` tras tu petición GET, ¿afecta al contenido de la respuesta que recibes? ¿Qué impacto tiene en el rendimiento si planeas hacer múltiples peticiones seguidas?

---

> **Siguiente tema**: T02 — Headers HTTP (Content-Type, Content-Length, Host, User-Agent, Accept, Cache-Control, MIME types)
