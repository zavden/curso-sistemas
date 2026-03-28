# Headers HTTP

## Índice
1. [Qué son las cabeceras HTTP](#qué-son-las-cabeceras-http)
2. [Anatomía de una cabecera](#anatomía-de-una-cabecera)
3. [Clasificación de cabeceras](#clasificación-de-cabeceras)
4. [Cabeceras de petición](#cabeceras-de-petición)
5. [Cabeceras de respuesta](#cabeceras-de-respuesta)
6. [Cabeceras de entidad](#cabeceras-de-entidad)
7. [MIME types](#mime-types)
8. [Cabeceras de seguridad](#cabeceras-de-seguridad)
9. [Cabeceras personalizadas](#cabeceras-personalizadas)
10. [Inspeccionar cabeceras en la práctica](#inspeccionar-cabeceras-en-la-práctica)
11. [Errores comunes](#errores-comunes)
12. [Cheatsheet](#cheatsheet)
13. [Ejercicios](#ejercicios)

---

## Qué son las cabeceras HTTP

Las cabeceras HTTP son pares clave-valor que acompañan tanto a peticiones como a respuestas. Proporcionan **metadatos** sobre la comunicación: qué tipo de contenido se envía, quién lo pide, cómo cachearlo, qué codificación aceptar, etc.

```
┌──────────────────────────────────────────────────────────────┐
│                  Petición HTTP/1.1                            │
│                                                              │
│  POST /api/users HTTP/1.1          ← request line            │
│  ┌────────────────────────────────────────────────────┐      │
│  │  Host: api.example.com          ← obligatoria     │      │
│  │  Content-Type: application/json ← tipo del body   │      │
│  │  Content-Length: 42             ← tamaño del body  │      │
│  │  User-Agent: curl/8.5.0        ← quién envía     │      │
│  │  Accept: application/json       ← qué acepta     │      │
│  │  Authorization: Bearer eyJ...   ← credenciales   │      │
│  │  Cache-Control: no-cache        ← política caché │      │
│  └────────────────────────────────────────────────────┘      │
│                                       ↑ CABECERAS            │
│  \r\n                                 ← línea vacía          │
│  {"name": "Ana", "role": "admin"}     ← body                │
└──────────────────────────────────────────────────────────────┘
```

Sin cabeceras, el servidor no sabría cómo interpretar el body, a qué dominio va dirigida la petición, ni qué formato de respuesta enviar.

---

## Anatomía de una cabecera

```
Content-Type: application/json; charset=utf-8
───────────── ─────────────────────────────────
     │                    │
   Nombre              Valor (puede incluir parámetros separados por ;)
```

Reglas de formato:

- **Case-insensitive en el nombre**: `Content-Type`, `content-type` y `CONTENT-TYPE` son equivalentes.
- **Un CRLF por cabecera**: cada cabecera ocupa una línea terminada en `\r\n`.
- **Parámetros con `;`**: algunos valores tienen parámetros adicionales (ej. `charset=utf-8`).
- **Múltiples valores con `,`**: algunas cabeceras aceptan listas (ej. `Accept: text/html, application/json`).
- **Cabeceras repetidas**: enviar la misma cabecera dos veces equivale a unirlas con coma (excepciones: `Set-Cookie`).

```bash
# Estas dos formas son equivalentes:
Accept-Encoding: gzip, deflate, br

# Es lo mismo que:
Accept-Encoding: gzip
Accept-Encoding: deflate
Accept-Encoding: br
```

---

## Clasificación de cabeceras

```
┌──────────────────────────────────────────────────────────────────┐
│               Clasificación por contexto                         │
│                                                                  │
│  ┌─────────────────┐   Solo en requests                         │
│  │ Request headers │   Host, User-Agent, Accept, Authorization  │
│  └─────────────────┘                                             │
│                                                                  │
│  ┌──────────────────┐  Solo en responses                        │
│  │ Response headers │  Server, Set-Cookie, Location, WWW-Auth   │
│  └──────────────────┘                                            │
│                                                                  │
│  ┌──────────────────┐  Describen el body                        │
│  │ Entity headers   │  Content-Type, Content-Length,             │
│  │ (Representation) │  Content-Encoding, Content-Language       │
│  └──────────────────┘  (en request o response)                  │
│                                                                  │
│  ┌──────────────────┐  Controlan transporte, caché, conexión    │
│  │ General headers  │  Cache-Control, Connection, Date,         │
│  └──────────────────┘  Transfer-Encoding                        │
└──────────────────────────────────────────────────────────────────┘
```

> **Nota**: la RFC 7231 eliminó la categoría formal "Entity headers" y la renombró a "Representation headers", pero los nombres anteriores siguen siendo ampliamente usados en documentación.

---

## Cabeceras de petición

### Host (obligatoria)

```
Host: www.example.com
Host: api.example.com:8080    ← incluir puerto si no es el default
```

Es la **única cabecera obligatoria** en HTTP/1.1. Permite al servidor distinguir entre múltiples sitios web alojados en la misma IP (virtual hosting):

```
┌──────────────────────────────────────────────────────────────┐
│                    Virtual Hosting                            │
│                                                              │
│  IP 93.184.216.34                                            │
│  ┌─────────────────────────────────────────────┐             │
│  │              nginx                          │             │
│  │                                             │             │
│  │  Host: shop.example.com  →  /var/www/shop   │             │
│  │  Host: blog.example.com  →  /var/www/blog   │             │
│  │  Host: api.example.com   →  proxy a :3000   │             │
│  │                                             │             │
│  │  Sin Host → 400 Bad Request                 │             │
│  └─────────────────────────────────────────────┘             │
└──────────────────────────────────────────────────────────────┘
```

### User-Agent

Identifica al cliente que realiza la petición:

```
User-Agent: curl/8.5.0
User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:128.0) Gecko/20100101 Firefox/128.0
User-Agent: python-requests/2.31.0
```

Usos legítimos del servidor: adaptar el contenido (ej. servir versión móvil), estadísticas, bloquear bots conocidos. El cliente puede enviar cualquier valor — no es un mecanismo de seguridad.

### Accept

Indica qué tipos de contenido acepta el cliente en la respuesta:

```bash
# Acepto JSON, si no hay JSON acepto XML, y en último caso lo que sea
Accept: application/json, application/xml;q=0.9, */*;q=0.1
```

El factor `q` (quality, de 0.0 a 1.0) indica prioridad. Si se omite, se asume `q=1.0`:

```
┌─────────────────────────────────────────────────────────────┐
│                Negociación con q-values                      │
│                                                              │
│  Accept: text/html;q=1.0, application/json;q=0.9, */*;q=0.1│
│                                                              │
│  Prioridad:                                                  │
│    1. text/html        (q=1.0)   ← preferido                │
│    2. application/json (q=0.9)   ← aceptable                │
│    3. cualquier otro   (q=0.1)   ← último recurso           │
│                                                              │
│  Si el servidor no puede satisfacer ninguno → 406 Not Accept.│
└─────────────────────────────────────────────────────────────┘
```

### Accept-Encoding

Compresiones que el cliente puede descomprimir:

```
Accept-Encoding: gzip, deflate, br
```

| Algoritmo | Descripción |
|-----------|-------------|
| `gzip` | El más universal, soportado por todos los navegadores |
| `deflate` | Más antiguo, menos usado |
| `br` | Brotli — mejor ratio que gzip, requiere HTTPS |
| `zstd` | Zstandard — muy rápido, soporte creciente |

El servidor responde con `Content-Encoding` indicando cuál usó:

```
Content-Encoding: gzip
```

> **Dato práctico**: la compresión típicamente reduce el tamaño de texto (HTML, JSON, CSS, JS) entre un 60-80%. Las imágenes JPEG/PNG ya están comprimidas y no se benefician.

### Accept-Language

```
Accept-Language: es-ES, es;q=0.9, en;q=0.5
```

El servidor puede usarla para seleccionar el idioma del contenido. Devuelve `Content-Language` con lo elegido.

### Authorization

Envía credenciales al servidor:

```bash
# Basic auth (base64 de user:password — NO es cifrado)
Authorization: Basic dXNlcjpwYXNzd29yZA==

# Bearer token (JWT, OAuth2)
Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...

# API Key (convención, no estándar)
Authorization: ApiKey sk-abc123...
```

> **Seguridad**: Basic auth envía credenciales en base64, que es **reversible trivialmente** — SIEMPRE usar HTTPS. Bearer tokens también se deben enviar exclusivamente por HTTPS.

### If-None-Match / If-Modified-Since

Cabeceras condicionales para validación de caché (vistas en T01):

```
If-None-Match: "etag-value"                   ← ¿cambió el hash?
If-Modified-Since: Wed, 18 Mar 2026 12:00:00 GMT  ← ¿cambió desde esta fecha?
```

---

## Cabeceras de respuesta

### Server

Identifica el software del servidor:

```
Server: nginx/1.24.0
Server: Apache/2.4.58
Server: cloudflare
```

> **Seguridad**: muchos administradores ocultan la versión (`server_tokens off` en nginx) para no revelar información que facilite ataques dirigidos.

### Set-Cookie

Instruye al cliente para almacenar una cookie que se reenviará en futuras peticiones:

```
Set-Cookie: session_id=abc123; Path=/; HttpOnly; Secure; SameSite=Strict; Max-Age=3600
```

| Atributo | Propósito |
|----------|-----------|
| `Path=/` | La cookie se envía para todas las rutas |
| `HttpOnly` | No accesible desde JavaScript (protege contra XSS) |
| `Secure` | Solo se envía por HTTPS |
| `SameSite=Strict` | No se envía en peticiones cross-site (protege contra CSRF) |
| `Max-Age=3600` | Expira en 1 hora (en segundos) |
| `Domain=.example.com` | Válida para todos los subdominios |

El cliente reenvía las cookies automáticamente:

```
Cookie: session_id=abc123; theme=dark
```

### Location

Indica la URL de un recurso creado o al que redireccionar:

```bash
# Después de un POST exitoso (201 Created)
Location: /api/users/42

# En una redirección (301, 302, 307, 308)
Location: https://www.example.com/new-path
```

### WWW-Authenticate

Acompaña a un 401 para indicar qué esquema de autenticación usar:

```
HTTP/1.1 401 Unauthorized
WWW-Authenticate: Bearer realm="api", error="invalid_token"
```

### Retry-After

Acompaña a un 429 o 503 para indicar cuándo reintentar:

```
HTTP/1.1 429 Too Many Requests
Retry-After: 60                    ← espera 60 segundos

HTTP/1.1 503 Service Unavailable
Retry-After: Wed, 21 Mar 2026 11:00:00 GMT   ← o una fecha absoluta
```

### ETag

Identificador único del estado actual de un recurso:

```
ETag: "33a64df551425fcc55e4d42a148795d9f25f89d4"   ← strong (byte-for-byte)
ETag: W/"0815"                                       ← weak (semánticamente equivalente)
```

- **Strong ETag**: dos representaciones son idénticas byte por byte.
- **Weak ETag** (`W/`): dos representaciones son semánticamente equivalentes (pueden diferir en formato, whitespace, etc.).

---

## Cabeceras de entidad

Estas cabeceras describen el body del mensaje, y pueden aparecer tanto en requests como en responses.

### Content-Type

Define el tipo MIME del body:

```
Content-Type: text/html; charset=utf-8
Content-Type: application/json
Content-Type: multipart/form-data; boundary=----WebKitFormBoundary
Content-Type: application/x-www-form-urlencoded
```

Es la cabecera más importante para interpretar el body correctamente. Sin ella, el receptor no sabe si los bytes son HTML, JSON, una imagen o un binario.

### Content-Length

Tamaño del body en bytes (no caracteres):

```
Content-Length: 1256
```

Reglas:

- Debe ser exacto — si es incorrecto, el cliente puede truncar o esperar datos extra.
- Mutuamente excluyente con `Transfer-Encoding: chunked`.
- En UTF-8, un carácter puede ocupar 1-4 bytes: `"ñ"` son 2 bytes, no 1.

```
┌───────────────────────────────────────────────────────┐
│  "Hola" → 4 caracteres, 4 bytes   (ASCII puro)      │
│  "Año"  → 3 caracteres, 4 bytes   (ñ = 2 bytes)     │
│  "日本" → 2 caracteres, 6 bytes   (3 bytes cada uno) │
│                                                       │
│  Content-Length cuenta BYTES, no caracteres            │
└───────────────────────────────────────────────────────┘
```

### Content-Encoding

Compresión aplicada al body:

```
Content-Encoding: gzip
```

El flujo completo de compresión:

```
┌───────────────────────────────────────────────────────────────┐
│                    Flujo de compresión                        │
│                                                               │
│  Cliente:                                                     │
│    Accept-Encoding: gzip, br     "puedo descomprimir estos"   │
│                                                               │
│  Servidor:                                                    │
│    Content-Encoding: gzip        "usé gzip"                   │
│    Content-Length: 420           tamaño COMPRIMIDO             │
│                                                               │
│  Body original: 2048 bytes (HTML)                             │
│  Body comprimido: 420 bytes (gzip)                            │
│  Ahorro: 79%                                                  │
│                                                               │
│  IMPORTANTE: Content-Length = tamaño del body TAL COMO        │
│  se transmite (comprimido), NO el tamaño original.            │
└───────────────────────────────────────────────────────────────┘
```

### Content-Language

```
Content-Language: es
Content-Language: en-US
```

### Transfer-Encoding

```
Transfer-Encoding: chunked
```

No es lo mismo que `Content-Encoding`. `Content-Encoding` es una propiedad del recurso (compresión). `Transfer-Encoding` es una propiedad de la transferencia (cómo se fragmenta el envío):

```
┌──────────────────────────────────────────────────────────────┐
│  Content-Encoding vs Transfer-Encoding                       │
│                                                              │
│  Content-Encoding: gzip                                      │
│    → El recurso ESTÁ comprimido                              │
│    → Un proxy puede cachearlo comprimido                     │
│                                                              │
│  Transfer-Encoding: chunked                                  │
│    → La TRANSFERENCIA se hace en fragmentos                  │
│    → Un proxy puede re-ensamblar y reenviar con              │
│      Content-Length en vez de chunked                         │
│                                                              │
│  Pueden coexistir: el recurso está comprimido (gzip)         │
│  y además se envía en chunks.                                │
└──────────────────────────────────────────────────────────────┘
```

---

## MIME types

MIME (Multipurpose Internet Mail Extensions) types identifican el formato del contenido. Se usan en `Content-Type`, `Accept`, y en la configuración de servidores web.

### Estructura

```
tipo/subtipo
────  ───────
  │      │
  │      └── Formato específico
  └── Categoría general
```

### Tipos principales

| MIME Type | Descripción |
|-----------|-------------|
| **Texto** | |
| `text/html` | Documento HTML |
| `text/plain` | Texto sin formato |
| `text/css` | Hoja de estilos |
| `text/javascript` | JavaScript (oficial desde RFC 9239) |
| **Aplicación** | |
| `application/json` | JSON |
| `application/xml` | XML |
| `application/pdf` | PDF |
| `application/octet-stream` | Binario genérico (descarga forzada) |
| `application/x-www-form-urlencoded` | Datos de formulario HTML |
| `application/gzip` | Archivo gzip |
| **Multipart** | |
| `multipart/form-data` | Formulario con archivos |
| `multipart/mixed` | Partes heterogéneas |
| **Imagen** | |
| `image/png` | PNG |
| `image/jpeg` | JPEG |
| `image/svg+xml` | SVG |
| `image/webp` | WebP |
| **Audio/Video** | |
| `audio/mpeg` | MP3 |
| `video/mp4` | MP4 |

### Formularios: urlencoded vs multipart

```
┌──────────────────────────────────────────────────────────────┐
│  application/x-www-form-urlencoded                           │
│  ─────────────────────────────────                           │
│  Datos codificados como query string:                        │
│  name=Ana+García&email=ana%40example.com                     │
│                                                              │
│  + Compacto para datos de texto simples                      │
│  - Ineficiente para datos binarios (cada byte → %XX)         │
│  Uso: login forms, búsquedas, datos simples                  │
├──────────────────────────────────────────────────────────────┤
│  multipart/form-data                                         │
│  ─────────────────                                           │
│  Cada campo es una "parte" separada por un boundary:         │
│                                                              │
│  ------WebKitFormBoundary7MA4YWxkTrZu0gW                     │
│  Content-Disposition: form-data; name="name"                 │
│                                                              │
│  Ana García                                                  │
│  ------WebKitFormBoundary7MA4YWxkTrZu0gW                     │
│  Content-Disposition: form-data; name="photo";               │
│    filename="photo.jpg"                                      │
│  Content-Type: image/jpeg                                    │
│                                                              │
│  [datos binarios del JPEG]                                   │
│  ------WebKitFormBoundary7MA4YWxkTrZu0gW--                   │
│                                                              │
│  + Eficiente para archivos binarios                          │
│  - Overhead por boundaries y cabeceras por parte             │
│  Uso: upload de archivos                                     │
└──────────────────────────────────────────────────────────────┘
```

```bash
# curl con urlencoded (default para -d)
curl -X POST http://example.com/login \
  -d "user=ana&password=secret"

# curl con multipart (upload de archivo)
curl -X POST http://example.com/upload \
  -F "name=Ana" \
  -F "photo=@/home/user/photo.jpg"
```

### MIME type en el servidor

Los servidores web determinan el Content-Type usando el archivo `/etc/mime.types` o configuración interna:

```bash
# Ver asociaciones MIME del sistema
cat /etc/mime.types | grep -E "^(text/html|application/json|image/png)"
```

```
# Extracto de /etc/mime.types
text/html                           html htm
application/json                    json
application/javascript              js mjs
image/png                           png
```

Nginx y Apache usan estas asociaciones para servir archivos estáticos con el Content-Type correcto.

---

## Cabeceras de seguridad

Aunque se verán en profundidad en temas posteriores, estas cabeceras de respuesta son fundamentales para la seguridad web:

| Cabecera | Valor típico | Protege contra |
|----------|-------------|---------------|
| `Strict-Transport-Security` | `max-age=31536000; includeSubDomains` | Downgrade a HTTP (HSTS) |
| `X-Content-Type-Options` | `nosniff` | MIME sniffing (interpretar JS como otro tipo) |
| `X-Frame-Options` | `DENY` o `SAMEORIGIN` | Clickjacking (embeber en iframe) |
| `Content-Security-Policy` | `default-src 'self'` | XSS, inyección de recursos |
| `Referrer-Policy` | `strict-origin-when-cross-origin` | Filtrado de información en el Referer |

```bash
# Verificar cabeceras de seguridad de un sitio
curl -sI https://example.com | grep -iE "^(strict|x-content|x-frame|content-security|referrer)"
```

### HSTS en detalle

```
Strict-Transport-Security: max-age=31536000; includeSubDomains; preload
```

Le dice al navegador: "durante los próximos 31536000 segundos (1 año), **siempre** usa HTTPS para este dominio, incluso si el usuario escribe `http://`". Esto previene ataques de downgrade donde un intermediario fuerza la conexión a HTTP.

---

## Cabeceras personalizadas

Las aplicaciones pueden definir cabeceras propias. Históricamente se usaba el prefijo `X-`, pero la RFC 6648 (2012) lo deprecó:

```bash
# Convención antigua (deprecated pero aún común)
X-Request-ID: 550e8400-e29b-41d4-a716-446655440000
X-Forwarded-For: 203.0.113.50

# Convención moderna: usar un nombre descriptivo sin prefijo
Request-ID: 550e8400-e29b-41d4-a716-446655440000
```

### Cabeceras de proxy comunes

Cuando un reverse proxy (nginx, HAProxy) está delante de la aplicación, necesita pasar la información del cliente original:

```
┌────────────────────────────────────────────────────────────┐
│  Cliente           Proxy (nginx)          Backend          │
│  203.0.113.50  →   192.168.1.10     →    192.168.1.20     │
│                                                            │
│  Sin cabeceras de proxy, el backend ve:                    │
│    Remote IP: 192.168.1.10  ← IP del proxy, no del cliente│
│                                                            │
│  Con cabeceras:                                            │
│    X-Forwarded-For: 203.0.113.50      ← IP real           │
│    X-Forwarded-Proto: https            ← protocolo real   │
│    X-Forwarded-Host: www.example.com   ← Host original    │
│                                                            │
│  Estándar moderno (RFC 7239):                              │
│    Forwarded: for=203.0.113.50; proto=https;               │
│               host=www.example.com                         │
└────────────────────────────────────────────────────────────┘
```

> **Seguridad**: nunca confiar en `X-Forwarded-For` de fuentes externas — el cliente puede falsificarlo. Solo confiar en él si lo establece tu propio proxy y el backend no es accesible directamente desde internet.

---

## Inspeccionar cabeceras en la práctica

### Con curl

```bash
# Solo cabeceras de respuesta (HEAD)
curl -I https://example.com

# Cabeceras + body
curl -i https://example.com

# Comunicación completa (request + response)
curl -v https://example.com 2>&1

# Filtrar cabeceras específicas
curl -sI https://example.com | grep -i "content-type"
```

### Con herramientas de red

```bash
# Ver cabeceras de un servicio local con ss + curl
ss -tlnp | grep :80          # ¿hay algo escuchando en 80?
curl -I http://localhost      # ¿qué cabeceras devuelve?
```

### Cabeceras que revela tu servidor (auditoría)

```bash
# ¿Tu servidor revela demasiada información?
curl -sI https://tu-servidor.com | grep -iE "^(server|x-powered-by|x-aspnet)"

# Bueno: cabeceras genéricas o ausentes
Server: nginx

# Malo: versiones exactas
Server: Apache/2.4.58 (Ubuntu)
X-Powered-By: PHP/8.2.3
```

---

## Errores comunes

### 1. Enviar JSON sin Content-Type

```bash
# INCORRECTO: el servidor no sabe que es JSON
curl -X POST http://api.example.com/users \
  -d '{"name": "Ana"}'
# → El servidor asume application/x-www-form-urlencoded
# → Parsea mal el body o devuelve 415 Unsupported Media Type

# CORRECTO
curl -X POST http://api.example.com/users \
  -H "Content-Type: application/json" \
  -d '{"name": "Ana"}'
```

### 2. Confundir Content-Length en bytes con caracteres

```bash
# "Año" tiene 3 caracteres pero 4 bytes en UTF-8
echo -n "Año" | wc -c    # → 4
echo -n "Año" | wc -m    # → 3

# Content-Length: 4  ← correcto (bytes)
# Content-Length: 3  ← INCORRECTO (caracteres)
```

curl calcula `Content-Length` automáticamente, pero si construyes peticiones manualmente (ej. con `nc` o programáticamente), un valor incorrecto causa truncamiento o timeouts.

### 3. Confiar en X-Forwarded-For sin validarlo

```
# Un atacante puede enviar:
X-Forwarded-For: 127.0.0.1

# Si tu aplicación usa este valor para control de acceso,
# el atacante simula ser localhost.
#
# Solución: solo aceptar X-Forwarded-For de proxies confiables
# y tomar el ÚLTIMO valor añadido por tu proxy, no el primero.
```

### 4. No distinguir Content-Encoding de Transfer-Encoding

```
# Content-Encoding: gzip
#   → El RECURSO está comprimido
#   → Se debe cachear comprimido
#   → El cliente lo descomprime para usarlo

# Transfer-Encoding: chunked
#   → Solo afecta a CÓMO se transmite
#   → Un proxy intermedio puede re-ensamblar y usar Content-Length
#   → No es una propiedad del recurso
```

### 5. Omitir Vary en respuestas con negociación de contenido

```bash
# Si el servidor devuelve JSON o HTML según Accept,
# los caches intermedios necesitan saber esto:

# INCORRECTO: un CDN podría cachear la respuesta JSON y
# servirla a un navegador que pidió HTML
Content-Type: application/json

# CORRECTO: indicar que la respuesta varía según Accept
Content-Type: application/json
Vary: Accept

# Vary le dice a los caches: "esta respuesta depende del valor
# de Accept — no asumas que es la misma para todos los clientes"
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│                   HTTP Headers Cheatsheet                     │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  REQUEST HEADERS:                                            │
│    Host: dominio:puerto        única obligatoria en HTTP/1.1 │
│    User-Agent: cliente/ver     identifica al cliente         │
│    Accept: tipo/subtipo        qué formatos acepta           │
│    Accept-Encoding: gzip, br   qué compresión soporta       │
│    Accept-Language: es, en     qué idiomas prefiere          │
│    Authorization: Bearer tok   credenciales                  │
│    Cookie: key=val             cookies del cliente           │
│    If-None-Match: "etag"       validación de caché           │
│    If-Modified-Since: date     validación de caché           │
│                                                              │
│  RESPONSE HEADERS:                                           │
│    Server: nginx               software del servidor         │
│    Set-Cookie: key=val; ...    establecer cookie             │
│    Location: /new/path         recurso creado o redirección  │
│    WWW-Authenticate: scheme    cómo autenticarse (con 401)   │
│    Retry-After: 60             cuándo reintentar (429/503)   │
│    ETag: "hash"                hash del recurso              │
│    Vary: Accept                de qué depende la respuesta   │
│                                                              │
│  ENTITY/REPRESENTATION:                                      │
│    Content-Type: tipo/sub      tipo MIME del body            │
│    Content-Length: bytes        tamaño en BYTES (no chars)   │
│    Content-Encoding: gzip      compresión del recurso        │
│    Content-Language: es        idioma del contenido          │
│    Transfer-Encoding: chunked  fragmentación del envío       │
│                                                              │
│  CACHÉ:                                                      │
│    Cache-Control: max-age=N    expiración                    │
│    Cache-Control: no-store     prohibir almacenamiento       │
│    Cache-Control: no-cache     almacenar pero revalidar      │
│                                                              │
│  SEGURIDAD:                                                  │
│    Strict-Transport-Security   forzar HTTPS (HSTS)           │
│    X-Content-Type-Options      prevenir MIME sniffing        │
│    X-Frame-Options             prevenir clickjacking         │
│    Content-Security-Policy     controlar fuentes de recursos │
│                                                              │
│  PROXY:                                                      │
│    X-Forwarded-For: IP         IP real del cliente           │
│    X-Forwarded-Proto: https    protocolo original            │
│    Forwarded: for=IP           estándar RFC 7239             │
│                                                              │
│  MIME TYPES COMUNES:                                         │
│    text/html          application/json      image/png        │
│    text/css           application/xml       image/jpeg       │
│    text/javascript    application/pdf       image/svg+xml    │
│    text/plain         application/octet-stream               │
│    multipart/form-data    application/x-www-form-urlencoded  │
│                                                              │
│  CURL:                                                       │
│    curl -I URL             solo cabeceras (HEAD)             │
│    curl -i URL             cabeceras + body                  │
│    curl -v URL             request + response completo       │
│    curl -H "Key: Val"     enviar cabecera custom             │
│    curl -d '...'           x-www-form-urlencoded (default)   │
│    curl -F "file=@path"   multipart/form-data               │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Identificar y corregir cabeceras

Analiza la siguiente petición HTTP y encuentra **todos** los problemas:

```
POST /api/users HTTP/1.1
User-Agent: MyApp/1.0
Content-Length: 15

{"name": "José María"}
```

**Responde**:
1. ¿Qué cabecera obligatoria falta?
2. ¿Es correcto el valor de `Content-Length`? Calcula el tamaño real en bytes (considerando UTF-8).
3. ¿Qué cabecera falta para que el servidor sepa interpretar el body como JSON?
4. Si el servidor tiene tres virtual hosts, ¿qué ocurre sin la cabecera faltante del punto 1?
5. Reescribe la petición corregida.

**Pregunta de reflexión**: ¿Por qué HTTP/1.1 decidió hacer `Host` obligatoria cuando HTTP/1.0 no la requería? ¿Qué problema del mundo real motivó este cambio?

---

### Ejercicio 2: Negociación de contenido y caché

Un servidor API soporta JSON y XML. Analiza estos intercambios:

**Intercambio A**:
```
> GET /api/data HTTP/1.1
> Accept: application/xml

< HTTP/1.1 200 OK
< Content-Type: application/json
< Cache-Control: max-age=300
```

**Intercambio B**:
```
> GET /api/data HTTP/1.1
> Accept: application/json
> Accept-Encoding: br, gzip
> If-None-Match: "v42"

< HTTP/1.1 304 Not Modified
< ETag: "v42"
< Cache-Control: max-age=300
```

**Responde**:
1. En el intercambio A, ¿el servidor está actuando correctamente? ¿Qué debería haber hecho?
2. ¿Falta alguna cabecera en la respuesta de A para que la caché funcione correctamente si el servidor soporta ambos formatos?
3. En el intercambio B, ¿hay body en la respuesta? ¿Por qué?
4. En B, ¿se usó la compresión Brotli? ¿Cómo lo sabes?
5. Después de recibir la respuesta B, ¿por cuánto tiempo más puede el cliente usar su copia cacheada sin revalidar?

**Pregunta de reflexión**: Si un CDN está entre el cliente y el servidor, ¿qué ocurre si el servidor olvida incluir `Vary: Accept` y el primer cliente pidió XML? ¿Qué recibiría el segundo cliente que pide JSON?

---

### Ejercicio 3: Auditoría de cabeceras de un servidor

Ejecuta estos comandos contra un servidor público (puedes usar `http://example.com` o `https://httpbin.org`):

```bash
# 1. Obtener todas las cabeceras de respuesta
curl -sI https://example.com

# 2. Ver la comunicación completa
curl -sv https://example.com 2>&1 | head -30

# 3. Verificar cabeceras de seguridad
curl -sI https://example.com | grep -iE "^(strict|x-content|x-frame|content-security)"
```

**Responde**:
1. ¿Qué `Content-Type` devuelve? ¿Incluye `charset`?
2. ¿Hay `Content-Length` o `Transfer-Encoding: chunked`?
3. ¿El servidor revela su nombre y versión? ¿Es esto bueno o malo?
4. ¿Qué cabeceras de seguridad están presentes? ¿Cuáles faltan?
5. ¿Hay `Cache-Control`? ¿Qué política usa?

Ahora compara con un segundo servidor diferente (ej. `https://httpbin.org`). ¿Qué diferencias notas en las cabeceras?

**Pregunta de reflexión**: Un compañero dice "las cabeceras de seguridad las pone el framework de la aplicación, no es responsabilidad del sysadmin". ¿Estás de acuerdo? ¿Dónde es más eficiente configurar cabeceras como HSTS — en la aplicación o en el reverse proxy?

---

> **Siguiente tema**: T03 — CORS (Same-Origin Policy, preflight requests, Access-Control-Allow-*, relevancia para WASM en navegador)
