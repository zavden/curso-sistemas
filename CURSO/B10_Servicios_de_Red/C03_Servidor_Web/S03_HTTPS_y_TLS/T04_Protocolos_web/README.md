# Protocolos web: HTTP/1.1, HTTP/2 y WebSocket

## Índice

1. [Evolución de los protocolos web](#1-evolución-de-los-protocolos-web)
2. [HTTP/1.1 — el protocolo base](#2-http11--el-protocolo-base)
3. [HTTP/2 — multiplexación y eficiencia](#3-http2--multiplexación-y-eficiencia)
4. [HTTP/3 y QUIC — visión general](#4-http3-y-quic--visión-general)
5. [WebSocket — comunicación bidireccional](#5-websocket--comunicación-bidireccional)
6. [Relevancia para WebAssembly](#6-relevancia-para-webassembly)
7. [Configuración en Apache y Nginx](#7-configuración-en-apache-y-nginx)
8. [Diagnóstico de protocolos](#8-diagnóstico-de-protocolos)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Evolución de los protocolos web

```
1991   HTTP/0.9    Solo GET, sin cabeceras, solo HTML
  │
1996   HTTP/1.0    Cabeceras, métodos, Content-Type
  │                Una conexión TCP por petición
  │
1997   HTTP/1.1    Keep-alive, chunked transfer, Host header
  │                Pipelining (en teoría), caching avanzado
  │
2011   WebSocket   Comunicación bidireccional persistente
  │    (RFC 6455)  Upgrade desde HTTP/1.1
  │
2015   HTTP/2      Multiplexación, compresión de headers,
  │    (RFC 7540)  binario, server push, una conexión TCP
  │
2022   HTTP/3      HTTP/2 sobre QUIC (UDP en vez de TCP)
       (RFC 9114)  Elimina head-of-line blocking de TCP
```

### Por qué importa la evolución

Cada versión resolvió limitaciones de la anterior. Entender estas limitaciones
explica las decisiones de diseño y las configuraciones necesarias en el
servidor:

| Problema                          | Versión que lo resuelve |
|-----------------------------------|-------------------------|
| Sin persistencia de conexión      | HTTP/1.1 (keep-alive)   |
| Head-of-line blocking (HTTP)      | HTTP/2 (multiplexación) |
| Overhead de cabeceras repetidas   | HTTP/2 (HPACK)          |
| Solo cliente→servidor             | WebSocket               |
| Head-of-line blocking (TCP)       | HTTP/3 (QUIC/UDP)       |

---

## 2. HTTP/1.1 — el protocolo base

### 2.1 Anatomía de una petición

```
GET /api/users?page=2 HTTP/1.1       ← línea de petición
Host: api.ejemplo.com                ← obligatorio en HTTP/1.1
Accept: application/json             ← tipo de contenido aceptado
Accept-Encoding: gzip, deflate       ← compresión aceptada
Connection: keep-alive               ← mantener conexión
User-Agent: Mozilla/5.0 ...          ← identificación del cliente
Authorization: Bearer eyJhbG...      ← autenticación
                                     ← línea vacía = fin de headers
```

### 2.2 Anatomía de una respuesta

```
HTTP/1.1 200 OK                      ← línea de estado
Content-Type: application/json       ← tipo del contenido
Content-Length: 1234                  ← tamaño del body
Cache-Control: max-age=3600          ← política de caché
Set-Cookie: session=abc123; HttpOnly ← cookie
Connection: keep-alive               ← mantener conexión
                                     ← línea vacía
{"users": [...]}                     ← body
```

### 2.3 Métodos HTTP

| Método    | Propósito                    | Idempotente | Body   |
|-----------|------------------------------|-------------|--------|
| `GET`     | Obtener recurso              | Sí          | No     |
| `HEAD`    | Como GET pero solo headers   | Sí          | No     |
| `POST`    | Crear recurso / enviar datos | No          | Sí     |
| `PUT`     | Reemplazar recurso completo  | Sí          | Sí     |
| `PATCH`   | Modificar parcialmente       | No*         | Sí     |
| `DELETE`  | Eliminar recurso             | Sí          | Opcional|
| `OPTIONS` | Consultar capacidades (CORS) | Sí          | No     |

*PATCH puede ser idempotente según la implementación.

### 2.4 Códigos de estado

```
1xx  Informacional
  100 Continue               ← el servidor espera el body
  101 Switching Protocols    ← upgrade a WebSocket

2xx  Éxito
  200 OK                     ← petición exitosa
  201 Created                ← recurso creado (POST)
  204 No Content             ← éxito sin body (DELETE)

3xx  Redirección
  301 Moved Permanently      ← redirección permanente (cacheable)
  302 Found                  ← redirección temporal
  304 Not Modified           ← usar versión cacheada

4xx  Error del cliente
  400 Bad Request            ← petición malformada
  401 Unauthorized           ← requiere autenticación
  403 Forbidden              ← autenticado pero sin permiso
  404 Not Found              ← recurso no existe
  405 Method Not Allowed     ← método no soportado
  429 Too Many Requests      ← rate limiting

5xx  Error del servidor
  500 Internal Server Error  ← error genérico del servidor
  502 Bad Gateway            ← backend no responde (proxy)
  503 Service Unavailable    ← servidor sobrecargado
  504 Gateway Timeout        ← backend no responde a tiempo
```

### 2.5 Keep-alive y sus limitaciones

HTTP/1.1 introduce keep-alive por defecto: la conexión TCP se reutiliza para
múltiples peticiones. Pero tiene una limitación fundamental:

```
HTTP/1.1 con keep-alive:

Conexión TCP 1:
  → GET /index.html    ← debe esperar respuesta
  ← 200 OK (index.html)
  → GET /style.css     ← ahora puede pedir el siguiente
  ← 200 OK (style.css)
  → GET /script.js
  ← 200 OK (script.js)

  Las peticiones son SECUENCIALES dentro de una conexión
  (head-of-line blocking a nivel HTTP)

Solución del navegador: abrir 6-8 conexiones TCP en paralelo
  Conexión 1: index.html → style.css → ...
  Conexión 2: script.js → font.woff2 → ...
  Conexión 3: logo.png → banner.jpg → ...
  ...
```

### 2.6 Pipelining (teoría vs práctica)

HTTP/1.1 definió pipelining: enviar múltiples peticiones sin esperar respuesta.
En la práctica, casi nunca funciona porque:

- Las respuestas deben llegar en orden (head-of-line blocking persiste)
- Muchos proxies intermedios lo rompen
- Los navegadores lo desactivaron

HTTP/2 resolvió este problema correctamente con multiplexación.

### 2.7 Chunked Transfer Encoding

Permite enviar respuestas sin conocer el tamaño total de antemano:

```
HTTP/1.1 200 OK
Transfer-Encoding: chunked

1a                          ← tamaño del chunk en hex (26 bytes)
Este es el primer trozo
12                          ← 18 bytes
y este el segundo
0                           ← chunk de tamaño 0 = fin
```

Útil para respuestas generadas dinámicamente y streaming.

---

## 3. HTTP/2 — multiplexación y eficiencia

### 3.1 Cambios fundamentales

HTTP/2 mantiene la semántica de HTTP/1.1 (mismos métodos, códigos, cabeceras)
pero cambia completamente el transporte:

```
HTTP/1.1 (texto)                     HTTP/2 (binario)
────────────────                     ───────────────
GET /index.html HTTP/1.1             Frame HEADERS (stream 1)
Host: ejemplo.com                      :method GET
                                       :path /index.html
                                       :authority ejemplo.com
                                     Frame DATA (stream 1)
                                       [contenido binario]
```

### 3.2 Streams y multiplexación

La innovación central de HTTP/2: múltiples peticiones y respuestas viajan
simultáneamente sobre **una sola conexión TCP**, sin bloquearse mutuamente:

```
HTTP/1.1: 6 conexiones TCP paralelas, secuencial dentro de cada una

TCP 1: ──[GET /a]────[resp a]──[GET /d]────[resp d]──
TCP 2: ──[GET /b]────[resp b]──[GET /e]────[resp e]──
TCP 3: ──[GET /c]────[resp c]──[GET /f]────[resp f]──

HTTP/2: 1 conexión TCP, todo multiplexado

TCP 1: ──[HDR a][HDR b][HDR c][DATA a][DATA b][DATA c]──
         stream1 str2   str3   str1    str2    str3
         (entrelazados, sin esperar)
```

```
HTTP/2 — una conexión TCP con múltiples streams:

┌──────────────────────────────────────────────┐
│              Conexión TCP                    │
│                                              │
│  Stream 1: GET /index.html ──► respuesta     │
│  Stream 3: GET /style.css  ──► respuesta     │
│  Stream 5: GET /app.js     ──► respuesta     │
│  Stream 7: GET /logo.png   ──► respuesta     │
│                                              │
│  Todos viajan simultáneamente                │
│  Sin head-of-line blocking a nivel HTTP      │
│  (persiste a nivel TCP)                      │
└──────────────────────────────────────────────┘
```

### 3.3 HPACK — compresión de cabeceras

HTTP/1.1 repite las mismas cabeceras en cada petición (Host, User-Agent,
Accept, Cookie...). HPACK las comprime:

```
Primera petición:
  :method: GET
  :path: /index.html
  :authority: ejemplo.com
  user-agent: Mozilla/5.0 ...
  accept: text/html
  cookie: session=abc123...    ← típicamente 500-800 bytes de headers

Segunda petición (HPACK):
  :path: /style.css            ← solo lo que cambió
  (todo lo demás referenciado por índice de la tabla dinámica)
  ← ~20 bytes de headers
```

Reducción típica: 85-90% en el tamaño de cabeceras tras la primera petición.

### 3.4 Server Push

El servidor puede enviar recursos antes de que el cliente los pida:

```
Cliente solicita /index.html
  │
  ▼
Servidor ve que index.html referencia style.css y app.js
  │
  ├── Envía PUSH_PROMISE para /style.css
  ├── Envía PUSH_PROMISE para /app.js
  ├── Envía respuesta de /index.html
  ├── Envía contenido de /style.css
  └── Envía contenido de /app.js

El cliente recibe todo sin haber pedido style.css ni app.js
```

> **Estado actual**: Server Push fue removido de Chrome en 2022 y su uso
> es desaconsejado. En la práctica, resultó difícil de implementar
> correctamente (problemas con caché del cliente, desperdicio de ancho de
> banda). Se prefieren alternativas como `103 Early Hints` o preload hints.

### 3.5 Priorización de streams

HTTP/2 permite asignar prioridades a los streams, indicando al servidor qué
recursos son más importantes:

```
Stream 1 (HTML):    peso 256, dependencia: ninguna  ← máxima prioridad
Stream 3 (CSS):     peso 256, dependencia: stream 1 ← tras HTML
Stream 5 (JS):      peso 220, dependencia: stream 1 ← tras HTML, algo menos
Stream 7 (imagen):  peso  32, dependencia: stream 1 ← baja prioridad
```

En la práctica, la implementación de priorización varía entre servidores y
la especificación fue revisada (RFC 9218 introduce un modelo simplificado).

### 3.6 Implicaciones para la arquitectura web

HTTP/2 cambia las buenas prácticas de optimización:

| Técnica HTTP/1.1         | Propósito                  | Con HTTP/2                    |
|--------------------------|----------------------------|-------------------------------|
| Concatenar CSS/JS        | Reducir peticiones         | Innecesario (multiplexación)  |
| CSS sprites              | Reducir peticiones img     | Innecesario                   |
| Domain sharding          | Más conexiones paralelas   | **Contraproducente** (1 conn) |
| Inline CSS/JS pequeños   | Evitar petición extra      | Menos beneficio               |
| Compresión de headers    | —                          | Automático (HPACK)            |

---

## 4. HTTP/3 y QUIC — visión general

### 4.1 El problema residual de HTTP/2

HTTP/2 resuelve el head-of-line blocking a nivel HTTP, pero no a nivel TCP.
Si se pierde un paquete TCP, **todos** los streams se bloquean hasta que se
retransmita:

```
HTTP/2 sobre TCP:

Stream A: ──[paquete 1]──[paquete 2]──[PERDIDO]──[paquete 4]──
Stream B: ──[paquete 1]──[paquete 2]──[bloqueado]──[bloqueado]──
Stream C: ──[paquete 1]──[bloqueado]──[bloqueado]──[bloqueado]──
                                        ▲
                              Todos esperan retransmisión
                              del paquete perdido de Stream A

HTTP/3 sobre QUIC (UDP):

Stream A: ──[paquete 1]──[paquete 2]──[PERDIDO]──[retransmit]──
Stream B: ──[paquete 1]──[paquete 2]──[paquete 3]──[paquete 4]──
Stream C: ──[paquete 1]──[paquete 2]──[paquete 3]──[paquete 4]──
                                        ▲
                              Solo Stream A espera
                              B y C continúan normalmente
```

### 4.2 QUIC

QUIC es un protocolo de transporte sobre UDP que integra TLS 1.3:

```
Pila HTTP/2:                  Pila HTTP/3:
┌──────────┐                  ┌──────────┐
│  HTTP/2  │                  │  HTTP/3  │
├──────────┤                  ├──────────┤
│   TLS    │                  │   QUIC   │ ← integra TLS 1.3
├──────────┤                  │          │    + control de flujo
│   TCP    │                  │          │    + multiplexación
├──────────┤                  ├──────────┤
│   IP     │                  │   UDP    │
└──────────┘                  ├──────────┤
                              │   IP     │
                              └──────────┘
```

**Ventajas de QUIC:**

- **0-RTT**: Conexión y datos en el primer paquete (reconexión)
- **Sin HOL blocking**: Pérdida de paquete solo afecta a su stream
- **Migración de conexión**: Cambiar de red (WiFi→4G) sin reconectar
- **TLS integrado**: Siempre cifrado, handshake más rápido

### 4.3 Soporte actual

HTTP/3 requiere soporte tanto en el servidor como en el cliente. Su adopción
avanza rápidamente:

- **Navegadores**: Todos los principales soportan HTTP/3
- **Nginx**: Soporte experimental desde 1.25.0 (requiere compilación con
  librería QUIC)
- **Apache**: Soporte vía mod_http2 y módulos experimentales
- **CDNs**: Cloudflare, Fastly, Akamai ya lo ofrecen

Para administradores de sistemas, HTTP/3 requiere abrir el puerto 443 en UDP
(además del TCP habitual).

---

## 5. WebSocket — comunicación bidireccional

### 5.1 El problema que resuelve

HTTP es petición-respuesta: el cliente siempre inicia. Para aplicaciones que
necesitan que el servidor envíe datos sin que el cliente pregunte (chat en
tiempo real, notificaciones, dashboards), HTTP fuerza técnicas subóptimas:

```
Polling (HTTP/1.1):
  Cliente: ¿hay datos nuevos? → Servidor: No
  Cliente: ¿hay datos nuevos? → Servidor: No
  Cliente: ¿hay datos nuevos? → Servidor: Sí, aquí están
  (desperdicio de ancho de banda y conexiones)

Long Polling:
  Cliente: ¿hay datos nuevos? → Servidor: ...(espera)...Sí, aquí están
  Cliente: ¿hay datos nuevos? → Servidor: ...(espera)...
  (mejor, pero sigue abriendo/cerrando conexiones)

Server-Sent Events (SSE):
  Cliente: quiero eventos → Servidor: dato1... dato2... dato3...
  (unidireccional servidor→cliente, sobre HTTP)

WebSocket:
  Cliente ←──────────────────→ Servidor
  (bidireccional, conexión persistente, bajo overhead)
```

### 5.2 Handshake de upgrade

WebSocket comienza como una petición HTTP/1.1 y se "upgradea" a WebSocket:

```
Cliente → Servidor:
  GET /ws/chat HTTP/1.1
  Host: ejemplo.com
  Upgrade: websocket
  Connection: Upgrade
  Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
  Sec-WebSocket-Version: 13
  Sec-WebSocket-Protocol: chat

Servidor → Cliente:
  HTTP/1.1 101 Switching Protocols
  Upgrade: websocket
  Connection: Upgrade
  Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=

(a partir de aquí, la conexión TCP lleva frames WebSocket, no HTTP)
```

### 5.3 Frames WebSocket

Tras el handshake, la comunicación usa frames binarios ligeros:

```
Frame WebSocket:
┌─────┬──────┬─────────┬──────────────┐
│ FIN │Opcode│ Length  │   Payload    │
│ 1b  │ 4b   │ 7-64b  │   N bytes    │
└─────┴──────┴─────────┴──────────────┘

Opcodes:
  0x1 = Text frame (UTF-8)
  0x2 = Binary frame
  0x8 = Close
  0x9 = Ping
  0xA = Pong

Overhead por frame: 2-14 bytes
(vs ~200-800 bytes de headers HTTP por petición)
```

### 5.4 Comparación de protocolos para tiempo real

| Aspecto            | Polling      | Long Polling | SSE          | WebSocket    |
|--------------------|--------------|--------------|--------------|--------------|
| Dirección          | Cliente→Serv | Cliente→Serv | Serv→Cliente | Bidireccional|
| Latencia           | Alta         | Media        | Baja         | Muy baja     |
| Overhead           | Alto         | Medio        | Bajo         | Muy bajo     |
| Conexiones         | Muchas       | Moderadas    | 1 por cliente| 1 por cliente|
| Complejidad        | Simple       | Media        | Simple       | Media        |
| Protocolo base     | HTTP         | HTTP         | HTTP         | TCP propio   |
| Proxy-friendly     | Sí           | Sí           | Sí           | Requiere config|
| Reconexión auto    | Inherente    | Inherente    | Sí (API)     | Manual       |

### 5.5 WebSocket sobre TLS (WSS)

```
ws://ejemplo.com/chat     ← sin cifrar (puerto 80)
wss://ejemplo.com/chat    ← cifrado con TLS (puerto 443)
```

En producción, siempre usar `wss://`. La configuración TLS del servidor web
cubre tanto HTTPS como WSS — no requiere configuración TLS adicional.

### 5.6 Server-Sent Events (SSE) — la alternativa simple

Para casos donde solo el servidor necesita enviar datos al cliente, SSE es
más simple que WebSocket:

```
# Respuesta SSE del servidor
HTTP/1.1 200 OK
Content-Type: text/event-stream
Cache-Control: no-cache
Connection: keep-alive

data: {"temperatura": 22.5}

data: {"temperatura": 22.7}

event: alerta
data: {"mensaje": "Temperatura alta"}
```

SSE funciona sobre HTTP estándar, pasa por proxies sin configuración especial,
y el navegador reconecta automáticamente si la conexión se pierde.

---

## 6. Relevancia para WebAssembly

### 6.1 WebAssembly y el servidor web

WebAssembly (Wasm) es un formato binario que se ejecuta en el navegador. Desde
la perspectiva del servidor web, un módulo `.wasm` es un archivo estático más,
pero con consideraciones específicas:

```
┌─────────┐                    ┌─────────────────┐
│ Browser │◄── HTTP/2 ────────►│  Nginx/Apache   │
│         │                    │                  │
│ ┌─────┐ │  GET /app.wasm    │  ┌────────────┐  │
│ │ JS  │─┤──────────────────►│──│ app.wasm   │  │
│ │ API │ │                    │  │ (archivo)  │  │
│ └──┬──┘ │◄─ Content-Type:   │  └────────────┘  │
│    │    │  application/wasm  │                  │
│ ┌──▼──┐ │                    │                  │
│ │Wasm │ │  (streaming       │                  │
│ │ RT  │ │   compilation)    │                  │
│ └─────┘ │                    │                  │
└─────────┘                    └─────────────────┘
```

### 6.2 Content-Type correcto

El tipo MIME `application/wasm` es esencial. Sin él, los navegadores no pueden
compilar el módulo de forma streaming:

```nginx
# Nginx — en mime.types o en la configuración
types {
    application/wasm  wasm;
}
```

```apache
# Apache — en mime.types o con AddType
AddType application/wasm .wasm
```

**Streaming compilation**: Los navegadores modernos pueden empezar a compilar
el módulo Wasm mientras se descarga, en paralelo. Esto requiere que el
Content-Type sea `application/wasm` desde el primer byte. Si el tipo es
incorrecto (ej: `application/octet-stream`), el navegador debe descargar
todo el archivo antes de compilar.

### 6.3 Compresión de módulos Wasm

Los archivos `.wasm` se comprimen bien con gzip o brotli:

```nginx
# Nginx
gzip_types application/wasm;

# Pre-compresión para módulos grandes
location ~* \.wasm$ {
    gzip_static on;           # servir .wasm.gz si existe
    add_header Cache-Control "public, max-age=31536000, immutable";
}
```

```apache
# Apache
AddOutputFilterByType DEFLATE application/wasm
```

Reducción típica: 50-70% del tamaño original.

### 6.4 Caching de módulos Wasm

Los módulos Wasm suelen ser grandes (varios MB) y cambian solo con nuevos
deploys. Un caching agresivo es apropiado:

```nginx
location ~* \.wasm$ {
    expires max;
    add_header Cache-Control "public, immutable";
    # El nombre del archivo incluye hash del contenido
    # (ej: app.a1b2c3.wasm)
}
```

### 6.5 COOP y COEP para SharedArrayBuffer

Algunas aplicaciones Wasm usan threads (vía SharedArrayBuffer), que requieren
cabeceras de aislamiento de seguridad:

```nginx
# Necesario para WebAssembly threads
add_header Cross-Origin-Opener-Policy "same-origin" always;
add_header Cross-Origin-Embedder-Policy "require-corp" always;
```

```apache
Header always set Cross-Origin-Opener-Policy "same-origin"
Header always set Cross-Origin-Embedder-Policy "require-corp"
```

> **Implicación**: Estas cabeceras restringen cómo la página puede interactuar
> con recursos de otros orígenes. Todo recurso cross-origin debe incluir
> `Cross-Origin-Resource-Policy: cross-origin` o ser servido con CORS.

### 6.6 HTTP/2 y módulos Wasm

HTTP/2 es especialmente beneficioso para aplicaciones Wasm:

- **Multiplexación**: El módulo `.wasm` (pesado) se descarga en paralelo con
  JS, CSS y otros recursos sin bloquear
- **Compresión de headers**: Reduce overhead en las peticiones de recursos
  auxiliares
- **Una sola conexión**: El handshake TLS se hace una vez, beneficiando la
  carga inicial de la aplicación

### 6.7 WebSocket y Wasm

Las aplicaciones Wasm que necesitan comunicación en tiempo real con el servidor
(juegos, editores colaborativos) usan WebSocket. El módulo Wasm no accede
directamente al WebSocket — la comunicación se gestiona vía JavaScript:

```
┌──────────────────────────────────────┐
│              Navegador               │
│                                      │
│  ┌──────────┐     ┌──────────────┐   │
│  │   Wasm   │◄───►│  JavaScript  │   │
│  │  (lógica)│     │  (WebSocket  │   │
│  │          │     │   API)       │   │
│  └──────────┘     └──────┬───────┘   │
│                          │           │
└──────────────────────────┼───────────┘
                           │ wss://
                    ┌──────▼───────┐
                    │   Servidor   │
                    └──────────────┘
```

---

## 7. Configuración en Apache y Nginx

### 7.1 HTTP/2

**Nginx:**

```nginx
server {
    listen 443 ssl http2;
    # ...
}
```

**Apache:**

```apache
# Activar módulo
# Debian: a2enmod http2
# RHEL: incluido en mod_ssl

# En el VirtualHost o globalmente
Protocols h2 http/1.1

<VirtualHost *:443>
    Protocols h2 http/1.1
    SSLEngine On
    # ...
</VirtualHost>
```

> **Apache y HTTP/2**: HTTP/2 en Apache requiere el MPM event (o worker).
> No funciona con prefork porque HTTP/2 necesita multiplexación de threads.
> Si se usa mod_php (que requiere prefork), hay incompatibilidad directa.
> Solución: usar PHP-FPM con event MPM.

### 7.2 WebSocket proxy

**Nginx:**

```nginx
map $http_upgrade $connection_upgrade {
    default upgrade;
    ''      close;
}

server {
    location /ws/ {
        proxy_pass http://backend:3000;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection $connection_upgrade;
        proxy_set_header Host $host;
        proxy_read_timeout 86400s;
    }
}
```

**Apache:**

```apache
# Activar módulos
# a2enmod proxy_wstunnel rewrite

<VirtualHost *:443>
    # WebSocket
    RewriteEngine On
    RewriteCond %{HTTP:Upgrade} websocket [NC]
    RewriteCond %{HTTP:Connection} upgrade [NC]
    RewriteRule ^/ws/(.*) "ws://127.0.0.1:3000/ws/$1" [P,L]

    # HTTP normal
    ProxyPass "/api/" "http://127.0.0.1:3000/api/"
    ProxyPassReverse "/api/" "http://127.0.0.1:3000/api/"
</VirtualHost>
```

### 7.3 Server-Sent Events

La configuración clave es desactivar el buffering para que los eventos lleguen
al cliente inmediatamente:

**Nginx:**

```nginx
location /events/ {
    proxy_pass http://backend:3000;
    proxy_buffering off;
    proxy_cache off;
    proxy_set_header Connection '';
    proxy_http_version 1.1;
    chunked_transfer_encoding off;
    proxy_read_timeout 86400s;
}
```

**Apache:**

```apache
<Location "/events/">
    ProxyPass "http://127.0.0.1:3000/events/"
    ProxyPassReverse "http://127.0.0.1:3000/events/"

    # Desactivar buffering para SSE
    SetEnv proxy-sendcl 1
    SetEnv proxy-interim-response RFC
</Location>
```

---

## 8. Diagnóstico de protocolos

### 8.1 Verificar el protocolo negociado

```bash
# HTTP/2 con curl
curl -vI --http2 https://ejemplo.com 2>&1 | grep "< HTTP"
# < HTTP/2 200

# HTTP/1.1 forzado
curl -vI --http1.1 https://ejemplo.com 2>&1 | grep "< HTTP"
# < HTTP/1.1 200 OK

# ALPN con openssl
echo | openssl s_client -connect ejemplo.com:443 \
    -servername ejemplo.com -alpn h2,http/1.1 2>/dev/null | grep ALPN
# ALPN protocol: h2
```

### 8.2 Inspeccionar cabeceras y timing

```bash
# Cabeceras de respuesta completas
curl -sI https://ejemplo.com

# Timing detallado de la conexión
curl -w "\
  DNS:        %{time_namelookup}s\n\
  TCP:        %{time_connect}s\n\
  TLS:        %{time_appconnect}s\n\
  First byte: %{time_starttransfer}s\n\
  Total:      %{time_total}s\n\
  Protocol:   %{http_version}\n" \
  -so /dev/null https://ejemplo.com
```

### 8.3 WebSocket testing

```bash
# Con websocat (instalar: cargo install websocat)
websocat wss://ejemplo.com/ws/chat

# Con curl (solo handshake, no soporta el protocolo WebSocket completo)
curl -v -H "Upgrade: websocket" -H "Connection: Upgrade" \
    -H "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==" \
    -H "Sec-WebSocket-Version: 13" \
    http://ejemplo.com/ws/
# Buscar: < HTTP/1.1 101 Switching Protocols
```

### 8.4 Herramientas de navegador

Las herramientas de desarrollo del navegador (F12) ofrecen la vista más
completa:

- **Network tab**: Muestra protocolo (h2, h3), timing, tamaño comprimido vs
  original
- **Protocol column**: Añadir columna "Protocol" para ver HTTP/1.1 vs h2
- **WebSocket tab**: Dentro de una petición WS, muestra frames enviados y
  recibidos con timestamp
- **Waterfall**: Visualiza la multiplexación de HTTP/2 (peticiones paralelas
  sobre una conexión)

---

## 9. Errores comunes

### Error 1: HTTP/2 no se activa con Apache prefork

**Causa**: HTTP/2 requiere multiplexación de threads, incompatible con el MPM
prefork (un proceso por conexión, sin threads compartidos).

**Diagnóstico**:

```bash
apachectl -V | grep MPM
# Server MPM: prefork   ← problema
```

**Solución**: Migrar a event MPM y PHP-FPM:

```bash
a2dismod mpm_prefork php8.2    # desactivar prefork y mod_php
a2enmod mpm_event proxy_fcgi   # activar event y FastCGI
systemctl restart apache2
```

### Error 2: WebSocket se desconecta tras 60 segundos de inactividad

**Causa**: `proxy_read_timeout` por defecto (60s) en Nginx cierra la conexión
si no hay actividad.

**Solución**: Aumentar timeout y/o implementar ping/pong:

```nginx
proxy_read_timeout 86400s;    # 24 horas
```

O configurar el backend para enviar pings periódicos (cada 30s).

### Error 3: Módulo .wasm devuelve Content-Type incorrecto

```
MIME type ('application/octet-stream') is not executable,
and strict MIME type checking is enabled.
```

**Causa**: El tipo MIME `application/wasm` no está registrado en el servidor.

**Solución**:

```nginx
# Nginx — añadir en http {} o en la configuración
types {
    application/wasm wasm;
}
```

```apache
# Apache
AddType application/wasm .wasm
```

### Error 4: SSE no llega en tiempo real — se acumula y llega en bloques

**Causa**: Buffering del proxy activo. Nginx acumula la respuesta antes de
enviarla al cliente.

**Solución**: Desactivar buffering para la location de SSE:

```nginx
proxy_buffering off;
```

### Error 5: Mixed protocols — HTTP/2 en el frontend, HTTP/1.1 al backend

**Esto NO es un error** — es el comportamiento esperado y correcto:

```
Cliente ──HTTP/2──► Nginx ──HTTP/1.1──► Backend
```

Nginx habla HTTP/2 con el cliente (eficiencia en la red pública) y HTTP/1.1
con el backend (simplicidad, el backend no necesita HTTP/2). La conversión
de protocolo es transparente.

---

## 10. Cheatsheet

```
┌─────────────────────────────────────────────────────────────────┐
│           PROTOCOLOS WEB — REFERENCIA RÁPIDA                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  HTTP/1.1:                                                      │
│  • Texto plano, una petición a la vez por conexión              │
│  • Keep-alive: reutiliza conexión TCP                           │
│  • Navegadores abren 6-8 conexiones paralelas                  │
│  • Head-of-line blocking a nivel HTTP                           │
│                                                                 │
│  HTTP/2:                                                        │
│  • Binario, multiplexación (muchas peticiones, 1 conexión)      │
│  • HPACK: compresión de cabeceras (~85% reducción)              │
│  • Requiere TLS en la práctica (todos los navegadores)          │
│  • HOL blocking persiste a nivel TCP                            │
│  • Invalida: domain sharding, CSS sprites, concat              │
│                                                                 │
│  HTTP/3:                                                        │
│  • HTTP/2 sobre QUIC (UDP), elimina HOL blocking de TCP         │
│  • TLS 1.3 integrado, 0-RTT en reconexión                      │
│  • Requiere puerto 443 UDP abierto                              │
│                                                                 │
│  WEBSOCKET:                                                     │
│  • Bidireccional persistente, upgrade desde HTTP/1.1            │
│  • Overhead: 2-14 bytes/frame (vs 200-800 bytes HTTP)           │
│  • wss:// para TLS (siempre en producción)                      │
│  • Requiere config proxy: Upgrade + Connection headers          │
│                                                                 │
│  SSE (Server-Sent Events):                                      │
│  • Unidireccional serv→cliente, sobre HTTP estándar             │
│  • Content-Type: text/event-stream                              │
│  • Reconexión automática del navegador                          │
│  • Proxy: desactivar buffering                                  │
│                                                                 │
│  ACTIVAR HTTP/2:                                                │
│  Nginx:  listen 443 ssl http2;                                  │
│  Apache: Protocols h2 http/1.1 (requiere event MPM)             │
│                                                                 │
│  PROXY WEBSOCKET:                                               │
│  Nginx:                                                         │
│    proxy_http_version 1.1;                                      │
│    proxy_set_header Upgrade $http_upgrade;                      │
│    proxy_set_header Connection $connection_upgrade;             │
│    proxy_read_timeout 86400s;                                   │
│  Apache:                                                        │
│    RewriteCond %{HTTP:Upgrade} websocket [NC]                   │
│    RewriteRule ^/ws/(.*) ws://backend/ws/$1 [P,L]               │
│                                                                 │
│  WEBASSEMBLY:                                                   │
│  • MIME: application/wasm (obligatorio para streaming compile)  │
│  • Compresión: gzip_types application/wasm;                     │
│  • Caché: expires max + immutable (assets con hash)             │
│  • Threads: COOP + COEP headers para SharedArrayBuffer          │
│                                                                 │
│  DIAGNÓSTICO:                                                   │
│  curl -vI --http2 https://host     protocolo negociado          │
│  openssl s_client -alpn h2         verificar ALPN               │
│  curl -w "%{http_version}" -so /dev/null URL                    │
│  websocat wss://host/ws            test WebSocket               │
│  F12 → Network → Protocol column   ver en navegador             │
│                                                                 │
│  CÓDIGOS CLAVE:                                                 │
│  101 Switching Protocols   upgrade a WebSocket                  │
│  200 OK    301 Redirect    304 Not Modified                     │
│  400 Bad   401 Unauth      403 Forbidden    404 Not Found       │
│  429 Rate  500 Internal    502 Bad Gateway   503 Unavailable    │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: HTTP/1.1 vs HTTP/2 en la práctica

1. Configura un server block Nginx con HTTPS y HTTP/2 habilitado.
2. Crea una página HTML que cargue 20 recursos (CSS, JS, imágenes pequeñas).
3. Accede con el navegador y abre las herramientas de desarrollo (Network tab).
   Añade la columna "Protocol". ¿Qué protocolo negocia?
4. Observa el waterfall: ¿las peticiones se hacen en paralelo sobre una
   conexión o secuencialmente sobre varias?
5. Ahora fuerza HTTP/1.1 desactivando HTTP/2 en el server block. Recarga.
6. Compara los waterfalls:
   - ¿Cuántas conexiones TCP se abren en cada caso?
   - ¿Cuál es el tiempo total de carga?
   - ¿Dónde se nota más la diferencia?
7. Verifica desde CLI:
   ```bash
   curl -w "Protocol: %{http_version}\n" -so /dev/null https://lab.local/
   ```

**Pregunta de predicción**: Si la página solo tiene 3 recursos pequeños (HTML +
CSS + JS), ¿la diferencia entre HTTP/1.1 y HTTP/2 será significativa? ¿A partir
de cuántos recursos se empieza a notar la ventaja de la multiplexación?

### Ejercicio 2: WebSocket a través de proxy

1. Crea un servidor WebSocket echo simple:
   ```bash
   # Python (instalar: pip install websockets)
   python3 -c "
   import asyncio, websockets
   async def echo(ws):
       async for msg in ws:
           await ws.send(f'Echo: {msg}')
   async def main():
       async with websockets.serve(echo, '127.0.0.1', 9000):
           await asyncio.Future()
   asyncio.run(main())
   "
   ```
2. Configura Nginx como proxy WebSocket en `/ws/` con las cabeceras `Upgrade`
   y `Connection` correctas.
3. Prueba la conexión WebSocket con `websocat` o un script.
4. Establece `proxy_read_timeout 10s` y espera 15 segundos sin enviar nada.
   ¿Qué sucede?
5. Cambia a `86400s` y verifica que la conexión persiste.
6. Configura el mismo proxy en Apache con `mod_proxy_wstunnel` y compara
   la configuración necesaria.

**Pregunta de predicción**: El handshake WebSocket usa HTTP/1.1 con el header
`Upgrade`. ¿Es posible hacer un upgrade a WebSocket desde una conexión HTTP/2?
¿O WebSocket siempre requiere HTTP/1.1 para el handshake?

### Ejercicio 3: Servir WebAssembly correctamente

1. Descarga o genera un archivo `.wasm` de ejemplo (puedes compilar un
   "hello world" en Rust con `wasm-pack`, o descargar uno de demostración).
2. Configura Nginx para servirlo con:
   - Content-Type `application/wasm`
   - Compresión gzip
   - Cache `immutable` con `expires max`
3. Verifica el Content-Type:
   ```bash
   curl -sI https://lab.local/app.wasm | grep Content-Type
   ```
4. Verifica la compresión:
   ```bash
   curl -sI -H "Accept-Encoding: gzip" https://lab.local/app.wasm | \
       grep Content-Encoding
   ```
5. Añade las cabeceras COOP y COEP para habilitar SharedArrayBuffer.
6. Verifica las cabeceras:
   ```bash
   curl -sI https://lab.local/app.wasm | grep -i cross-origin
   ```
7. Repite los pasos 2-4 con Apache y compara la configuración necesaria.

**Pregunta de reflexión**: El streaming compilation de WebAssembly permite
que el navegador compile el módulo mientras se descarga. ¿Cómo interactúa
esto con HTTP/2? Si el módulo `.wasm` de 5 MB se descarga en paralelo con el
JavaScript de 500 KB que lo instancia, ¿qué debe terminar primero para que la
aplicación funcione? ¿La multiplexación de HTTP/2 es ventajosa o indiferente
en este caso?

---

> **Fin de la Sección 3 — HTTPS y TLS**, y del **Capítulo 3 — Servidor Web**.
> Siguiente capítulo: C04 — Compartición de Archivos (Sección 1: NFS,
> T01 — Conceptos y arquitectura).
