# Proxy inverso con Nginx

## Índice

1. [Nginx como proxy inverso](#1-nginx-como-proxy-inverso)
2. [proxy_pass — la directiva fundamental](#2-proxy_pass--la-directiva-fundamental)
3. [Cabeceras de proxy](#3-cabeceras-de-proxy)
4. [Upstream — grupos de backends](#4-upstream--grupos-de-backends)
5. [Buffering y timeouts](#5-buffering-y-timeouts)
6. [WebSocket proxying](#6-websocket-proxying)
7. [FastCGI proxying (PHP-FPM)](#7-fastcgi-proxying-php-fpm)
8. [Patrones de configuración completos](#8-patrones-de-configuración-completos)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Nginx como proxy inverso

El caso de uso más habitual de Nginx en producción no es servir archivos
estáticos directamente, sino actuar como **proxy inverso**: recibir peticiones
de clientes y reenviarlas a aplicaciones backend.

```
                    Internet
                       │
                       ▼
              ┌─────────────────┐
              │     Nginx       │
              │  (puerto 80/443)│
              │                 │
              │  • TLS termination
              │  • Compresión   │
              │  • Caché        │
              │  • Rate limiting│
              │  • Estáticos    │
              └───┬─────┬───┬──┘
                  │     │   │
           ┌──────┘     │   └──────┐
           ▼            ▼          ▼
     ┌──────────┐ ┌──────────┐ ┌──────────┐
     │ Node.js  │ │ PHP-FPM  │ │ Python   │
     │ :3000    │ │ socket   │ │ :8000    │
     └──────────┘ └──────────┘ └──────────┘
```

### Ventajas del proxy inverso

| Ventaja               | Descripción                                          |
|-----------------------|------------------------------------------------------|
| Terminación TLS       | Nginx maneja HTTPS; backends reciben HTTP simple     |
| Servir estáticos      | Archivos estáticos directo desde disco, sin backend  |
| Balanceo de carga     | Distribuir peticiones entre múltiples instancias      |
| Buffering             | Absorbe respuestas lentas del backend                |
| Protección            | El backend nunca se expone directamente a internet   |
| Compresión            | gzip/brotli en Nginx, no en el backend               |

---

## 2. proxy_pass — la directiva fundamental

### 2.1 Sintaxis básica

```nginx
location /api/ {
    proxy_pass http://127.0.0.1:3000;
}
```

### 2.2 El trailing slash importa

La presencia o ausencia de URI en `proxy_pass` cambia el comportamiento:

```nginx
# SIN URI en proxy_pass (sin trailing slash ni path)
location /api/ {
    proxy_pass http://127.0.0.1:3000;
    # /api/users → http://127.0.0.1:3000/api/users
    # La URI completa se reenvía tal cual
}

# CON URI en proxy_pass (tiene trailing slash = tiene path "/")
location /api/ {
    proxy_pass http://127.0.0.1:3000/;
    # /api/users → http://127.0.0.1:3000/users
    # /api/ se REEMPLAZA por /
}

# CON path en proxy_pass
location /api/ {
    proxy_pass http://127.0.0.1:3000/v2/;
    # /api/users → http://127.0.0.1:3000/v2/users
    # /api/ se REEMPLAZA por /v2/
}
```

Resumido:

```
                         proxy_pass SIN URI        proxy_pass CON URI
                         (sin / al final)          (con / al final)
                         ───────────────────       ─────────────────
location /api/           /api/users →              /api/users →
                         backend/api/users         backend/users

Regla:                   Reenvía URI completa      Reemplaza la parte
                                                   que coincide con location
```

> **Regla mnemotécnica**: Si `proxy_pass` termina en solo el host:puerto (sin
> `/`), la URI pasa intacta. Si tiene cualquier path (incluso solo `/`), Nginx
> sustituye la parte de la location.

### 2.3 proxy_pass con variables

Cuando `proxy_pass` contiene variables, Nginx **no** puede hacer la sustitución
automática de la location. La URI se pasa tal cual:

```nginx
# Con variable: la sustitución automática NO ocurre
set $backend "http://127.0.0.1:3000";
location /api/ {
    proxy_pass $backend;
    # /api/users → http://127.0.0.1:3000/api/users (siempre URI completa)
}
```

Esto también significa que se necesita un `resolver` configurado si el backend
se especifica por nombre DNS:

```nginx
resolver 127.0.0.1;    # DNS para resolver nombres en variables
set $backend "http://mi-backend.local:3000";
location /api/ {
    proxy_pass $backend;
}
```

### 2.4 proxy_pass con regex location

Cuando la location usa regex, `proxy_pass` **no puede** incluir URI:

```nginx
# INCORRECTO — error de configuración
location ~ ^/api/(.*)$ {
    proxy_pass http://127.0.0.1:3000/;    # ERROR: URI en proxy_pass + regex
}

# CORRECTO — sin URI, reescribir con rewrite si es necesario
location ~ ^/api/(.*)$ {
    proxy_pass http://127.0.0.1:3000;     # sin URI
    # /api/users → http://127.0.0.1:3000/api/users
}

# CORRECTO — usar rewrite para transformar
location ~ ^/api/(.*)$ {
    rewrite ^/api/(.*)$ /$1 break;
    proxy_pass http://127.0.0.1:3000;
    # /api/users → rewrite a /users → http://127.0.0.1:3000/users
}
```

---

## 3. Cabeceras de proxy

### 3.1 El problema

Por defecto, Nginx modifica dos cabeceras al hacer proxy:

```
Host: → cambia al valor de proxy_pass (ej: 127.0.0.1:3000)
Connection: → establece "close"
```

El backend pierde información crucial: no sabe el dominio original del cliente,
su IP real, ni si la conexión era HTTPS.

### 3.2 Cabeceras estándar de proxy

```nginx
location /api/ {
    proxy_pass http://127.0.0.1:3000;

    # Preservar el Host original del cliente
    proxy_set_header Host $host;

    # IP real del cliente (Nginx añade a X-Forwarded-For existente)
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;

    # Protocolo original (http o https)
    proxy_set_header X-Forwarded-Proto $scheme;

    # IP del cliente (formato alternativo)
    proxy_set_header X-Real-IP $remote_addr;

    # Mantener keep-alive con el backend
    proxy_set_header Connection "";
    proxy_http_version 1.1;
}
```

### 3.3 proxy_params / proxy.conf de Debian

Debian incluye un archivo con las cabeceras estándar listo para usar:

```nginx
# /etc/nginx/proxy_params (incluido con la instalación)
proxy_set_header Host $http_host;
proxy_set_header X-Real-IP $remote_addr;
proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
proxy_set_header X-Forwarded-Proto $scheme;

# Uso en la configuración
location /api/ {
    proxy_pass http://127.0.0.1:3000;
    include proxy_params;
}
```

### 3.4 $host vs $http_host

| Variable       | Contenido                            | Ejemplo               |
|----------------|--------------------------------------|-----------------------|
| `$host`        | Nombre del server_name que coincidió o cabecera Host (sin puerto) | `ejemplo.com` |
| `$http_host`   | Cabecera Host tal cual la envió el cliente (con puerto si lo tiene) | `ejemplo.com:8080` |
| `$server_name` | Valor de la directiva server_name    | `ejemplo.com`         |

**Recomendación**: Usar `$host` en la mayoría de casos. Usar `$http_host` cuando
el backend necesita saber el puerto original.

### 3.5 proxy_set_header y herencia

`proxy_set_header` es una directiva de tipo array — sufre el mismo problema de
herencia que `add_header`:

```nginx
# PROBLEMA: location pierde los headers del server
server {
    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;

    location /api/ {
        proxy_set_header X-API-Key "secret";
        proxy_pass http://backend:3000;
        # ← PIERDE Host, X-Real-IP y X-Forwarded-For
    }
}

# SOLUCIÓN: include o repetir
server {
    location /api/ {
        include proxy_params;
        proxy_set_header X-API-Key "secret";
        proxy_pass http://backend:3000;
    }
}
```

### 3.6 Ocultar cabeceras del backend

Para evitar que el backend exponga información al cliente:

```nginx
location /api/ {
    proxy_pass http://backend:3000;
    include proxy_params;

    # Ocultar cabeceras del backend que no deben llegar al cliente
    proxy_hide_header X-Powered-By;
    proxy_hide_header Server;
    proxy_hide_header X-AspNet-Version;
}
```

---

## 4. Upstream — grupos de backends

### 4.1 Definición básica

Un bloque `upstream` define un grupo de servidores backend. Permite balanceo de
carga y failover:

```nginx
upstream app_backend {
    server 10.0.0.10:3000;
    server 10.0.0.11:3000;
    server 10.0.0.12:3000;
}

server {
    listen 80;
    server_name app.ejemplo.com;

    location / {
        proxy_pass http://app_backend;
        include proxy_params;
    }
}
```

### 4.2 Algoritmos de balanceo

```nginx
# Round-robin (defecto): distribuye secuencialmente
upstream backend {
    server 10.0.0.10:3000;
    server 10.0.0.11:3000;
}

# Weighted round-robin: más peso = más peticiones
upstream backend {
    server 10.0.0.10:3000 weight=3;    # 3 de cada 4 peticiones
    server 10.0.0.11:3000 weight=1;    # 1 de cada 4 peticiones
}

# Least connections: envía al que tiene menos conexiones activas
upstream backend {
    least_conn;
    server 10.0.0.10:3000;
    server 10.0.0.11:3000;
}

# IP hash: mismo cliente siempre al mismo backend (sticky sessions)
upstream backend {
    ip_hash;
    server 10.0.0.10:3000;
    server 10.0.0.11:3000;
}

# Hash genérico: por cualquier variable
upstream backend {
    hash $request_uri consistent;    # por URI, con consistencia
    server 10.0.0.10:3000;
    server 10.0.0.11:3000;
}
```

### 4.3 Parámetros de servidor

```nginx
upstream backend {
    server 10.0.0.10:3000 weight=3 max_fails=3 fail_timeout=30s;
    server 10.0.0.11:3000 weight=2;
    server 10.0.0.12:3000 backup;         # solo si los demás fallan
    server 10.0.0.13:3000 down;           # marcado como fuera de servicio
}
```

| Parámetro       | Defecto   | Descripción                                      |
|-----------------|-----------|--------------------------------------------------|
| `weight=N`      | 1         | Peso relativo para round-robin                   |
| `max_fails=N`   | 1         | Fallos antes de marcar como no disponible         |
| `fail_timeout=T`| 10s       | Tiempo que se considera no disponible tras fallos |
| `backup`        | —         | Solo recibe tráfico si los normales fallan        |
| `down`          | —         | Marcado permanentemente como no disponible        |
| `max_conns=N`   | 0 (ilim.) | Máximo de conexiones concurrentes a este server   |

### 4.4 Health checks pasivos

Nginx Open Source usa health checks pasivos: marca un backend como caído
cuando las peticiones reales fallan:

```
Petición → backend:3000 → timeout/error (fallo 1)
Petición → backend:3000 → timeout/error (fallo 2)
Petición → backend:3000 → timeout/error (fallo 3 = max_fails)
   ↓
Backend marcado como "unavailable" por fail_timeout segundos
   ↓
Tras fail_timeout: Nginx lo prueba de nuevo con peticiones reales
   ↓
Si responde → restaurado al pool
```

### 4.5 Manejo de errores de backend

```nginx
location / {
    proxy_pass http://backend;

    # Si un backend falla, intentar con el siguiente
    proxy_next_upstream error timeout http_502 http_503 http_504;

    # Máximo de reintentos
    proxy_next_upstream_tries 3;

    # Tiempo máximo para reintentos
    proxy_next_upstream_timeout 10s;
}
```

---

## 5. Buffering y timeouts

### 5.1 Proxy buffering

Por defecto, Nginx bufferiza la respuesta del backend: la recibe completa en
memoria (o disco) antes de enviarla al cliente. Esto libera al backend rápidamente.

```
Con buffering ON (defecto):
  Cliente ──slow──► Nginx ◄──fast── Backend
                    [buffer]
  Nginx absorbe la respuesta rápida del backend,
  luego la envía al cliente a su ritmo.
  El backend queda libre para otra petición.

Con buffering OFF:
  Cliente ──slow──► Nginx ◄──slow── Backend
  Nginx reenvía byte a byte.
  El backend queda bloqueado hasta que el
  cliente reciba todo.
```

```nginx
location /api/ {
    proxy_pass http://backend;

    # Buffering (defecto: on)
    proxy_buffering on;

    # Tamaño del buffer para la primera parte de la respuesta (headers)
    proxy_buffer_size 4k;

    # Número y tamaño de buffers para el body
    proxy_buffers 8 4k;

    # Si la respuesta excede los buffers, escribir a disco temporal
    proxy_max_temp_file_size 1024m;

    # Desactivar para streaming/SSE (Server-Sent Events)
    # proxy_buffering off;
}
```

### 5.2 Cuándo desactivar buffering

```nginx
# Server-Sent Events (SSE)
location /events/ {
    proxy_pass http://backend;
    proxy_buffering off;           # no acumular, enviar inmediatamente
    proxy_cache off;
    proxy_read_timeout 3600s;      # mantener conexión abierta
}

# Streaming de archivos grandes
location /download/ {
    proxy_pass http://storage;
    proxy_buffering off;
}
```

### 5.3 Timeouts

```nginx
location /api/ {
    proxy_pass http://backend;

    # Tiempo para establecer conexión con el backend
    proxy_connect_timeout 5s;      # defecto: 60s

    # Tiempo de espera entre dos read() del backend
    proxy_read_timeout 60s;        # defecto: 60s

    # Tiempo de espera entre dos write() al backend
    proxy_send_timeout 60s;        # defecto: 60s
}
```

**Diagrama de timeouts:**

```
Cliente        Nginx           Backend
  │              │                │
  │──request────►│                │
  │              │──connect──────►│
  │              │ ◄─────────────►│ proxy_connect_timeout
  │              │                │
  │              │──send request─►│
  │              │ ◄─────────────►│ proxy_send_timeout (entre writes)
  │              │                │
  │              │    processing  │
  │              │                │
  │              │◄──response─────│
  │              │ ◄─────────────►│ proxy_read_timeout (entre reads)
  │              │                │
  │◄──response───│                │
  │              │                │
```

### 5.4 Timeouts para aplicaciones lentas

Para backends que procesan operaciones largas (reportes, cálculos):

```nginx
location /api/reports/ {
    proxy_pass http://backend;

    # Valores agresivos para operaciones largas
    proxy_read_timeout 300s;       # 5 minutos
    proxy_connect_timeout 10s;     # pero conectar rápido
    proxy_send_timeout 60s;
}

# Mantener los defaults cortos para el resto
location /api/ {
    proxy_pass http://backend;
    proxy_connect_timeout 5s;
    proxy_read_timeout 30s;
}
```

---

## 6. WebSocket proxying

WebSocket requiere configuración especial porque usa un upgrade de protocolo
de HTTP a WebSocket (cambio de HTTP/1.1 a un protocolo bidireccional
persistente).

### 6.1 Handshake WebSocket

```
Cliente                    Nginx                     Backend
  │                          │                          │
  │── GET /ws HTTP/1.1 ─────►│                          │
  │   Upgrade: websocket     │── GET /ws HTTP/1.1 ─────►│
  │   Connection: Upgrade    │   Upgrade: websocket     │
  │                          │   Connection: Upgrade    │
  │                          │                          │
  │                          │◄─ 101 Switching ─────────│
  │◄─ 101 Switching ─────────│                          │
  │                          │                          │
  │◄════ WebSocket frames ═══►◄════ WebSocket frames ═══►│
  │        (bidireccional)   │       (bidireccional)     │
```

### 6.2 Configuración

```nginx
# Mapeo para la cabecera Connection
# Si el cliente envía Upgrade → Connection = upgrade
# Si no → Connection = close (o vacío)
map $http_upgrade $connection_upgrade {
    default upgrade;
    ''      close;
}

server {
    listen 80;
    server_name app.ejemplo.com;

    # Rutas WebSocket
    location /ws/ {
        proxy_pass http://127.0.0.1:3000;
        proxy_http_version 1.1;

        # Headers necesarios para el upgrade
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection $connection_upgrade;

        # Headers de proxy estándar
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;

        # Timeout largo — conexiones WebSocket son persistentes
        proxy_read_timeout 86400s;    # 24 horas
        proxy_send_timeout 86400s;
    }

    # Rutas HTTP normales
    location / {
        proxy_pass http://127.0.0.1:3000;
        include proxy_params;
    }
}
```

### 6.3 Por qué proxy_http_version 1.1

El proxy de Nginx usa HTTP/1.0 por defecto hacia el backend. WebSocket requiere
HTTP/1.1 porque el mecanismo de upgrade (`Upgrade: websocket`) no existe en
HTTP/1.0. Además, HTTP/1.1 permite keep-alive con el backend.

```nginx
# SIEMPRE para WebSocket (y recomendado en general)
proxy_http_version 1.1;
```

### 6.4 Detección automática HTTP/WebSocket

Si la aplicación usa la misma ruta para HTTP y WebSocket:

```nginx
map $http_upgrade $connection_upgrade {
    default upgrade;
    ''      close;
}

server {
    location / {
        proxy_pass http://127.0.0.1:3000;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection $connection_upgrade;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        # Timeout largo solo cuando hay WebSocket
        # Para HTTP normal, el timeout por defecto aplica
        proxy_read_timeout 86400s;
    }
}
```

---

## 7. FastCGI proxying (PHP-FPM)

### 7.1 Arquitectura Nginx + PHP-FPM

A diferencia de Apache con mod_php, Nginx **nunca** ejecuta PHP internamente.
Delega a PHP-FPM vía el protocolo FastCGI:

```
┌─────────┐     FastCGI      ┌────────────┐
│  Nginx   │────────────────►│  PHP-FPM   │
│          │  (socket Unix   │            │
│ .php →   │   o TCP :9000)  │ pool de    │
│ fastcgi  │                 │ workers    │
│          │◄────────────────│            │
│ response │   respuesta     │            │
└─────────┘                  └────────────┘
```

### 7.2 Configuración estándar

```nginx
server {
    listen 80;
    server_name php-app.ejemplo.com;
    root /var/www/php-app/public;
    index index.php index.html;

    # Archivos estáticos: servir directamente
    location / {
        try_files $uri $uri/ /index.php$is_args$args;
    }

    # Archivos PHP: pasar a PHP-FPM
    location ~ \.php$ {
        # Verificar que el archivo existe (prevenir CGI path info attacks)
        try_files $uri =404;

        # Parámetros FastCGI
        include fastcgi_params;
        fastcgi_param SCRIPT_FILENAME $document_root$fastcgi_script_name;

        # Socket Unix (misma máquina — más rápido)
        fastcgi_pass unix:/run/php/php-fpm.sock;

        # O TCP (para PHP-FPM remoto)
        # fastcgi_pass 127.0.0.1:9000;

        fastcgi_index index.php;
    }

    # Denegar acceso a archivos ocultos
    location ~ /\. {
        deny all;
    }
}
```

### 7.3 fastcgi_params esenciales

El archivo `/etc/nginx/fastcgi_params` incluye parámetros estándar. El más
importante que suele añadirse manualmente:

```nginx
# SCRIPT_FILENAME le dice a PHP-FPM qué archivo ejecutar
fastcgi_param SCRIPT_FILENAME $document_root$fastcgi_script_name;
```

Sin esta línea, PHP-FPM no sabe qué archivo procesar y devuelve una página
en blanco o "No input file specified".

### 7.4 Seguridad: prevenir ejecución de uploads

```nginx
# Servir uploads como archivos estáticos, NUNCA procesar PHP
location /uploads/ {
    location ~ \.php$ {
        deny all;     # bloquear .php en /uploads/
    }
}
```

---

## 8. Patrones de configuración completos

### 8.1 Aplicación Node.js con estáticos

```nginx
upstream node_app {
    server 127.0.0.1:3000;
    keepalive 32;               # pool de conexiones persistentes
}

server {
    listen 80;
    server_name app.ejemplo.com;

    # Estáticos directos desde disco
    root /var/www/app/public;

    location /static/ {
        expires 30d;
        add_header Cache-Control "public, immutable";
        try_files $uri =404;
    }

    # Todo lo demás al backend
    location / {
        proxy_pass http://node_app;
        proxy_http_version 1.1;
        proxy_set_header Connection "";     # keepalive con backend
        include proxy_params;
    }
}
```

### 8.2 Aplicación Python (Gunicorn/uWSGI)

```nginx
upstream gunicorn {
    server unix:/run/gunicorn/app.sock;
}

server {
    listen 80;
    server_name api.ejemplo.com;
    root /var/www/api/static;

    # Estáticos
    location /static/ {
        expires 7d;
        try_files $uri =404;
    }

    # Media (uploads)
    location /media/ {
        alias /var/www/api/media/;
        expires 30d;
    }

    # Aplicación
    location / {
        proxy_pass http://gunicorn;
        include proxy_params;
        proxy_read_timeout 120s;
    }

    # Tamaño de uploads
    client_max_body_size 10m;
}
```

### 8.3 Múltiples backends por ruta

```nginx
upstream api_service {
    server 127.0.0.1:3000;
    server 127.0.0.1:3001;
}

upstream admin_service {
    server 127.0.0.1:4000;
}

server {
    listen 80;
    server_name ejemplo.com;
    root /var/www/frontend/dist;

    # Frontend SPA (archivos estáticos)
    location / {
        try_files $uri /index.html;
    }

    # API
    location /api/ {
        proxy_pass http://api_service;
        proxy_http_version 1.1;
        include proxy_params;
    }

    # Panel de administración
    location /admin/ {
        proxy_pass http://admin_service;
        include proxy_params;

        # Solo red interna
        allow 10.0.0.0/8;
        deny all;
    }
}
```

### 8.4 Proxy con caché

```nginx
# Definir zona de caché (en el contexto http, fuera de server)
proxy_cache_path /var/cache/nginx/api
    levels=1:2
    keys_zone=api_cache:10m       # 10 MB para claves en memoria
    max_size=1g                   # 1 GB máximo en disco
    inactive=60m                  # eliminar tras 60 min sin acceso
    use_temp_path=off;

server {
    location /api/ {
        proxy_pass http://api_service;
        include proxy_params;

        # Activar caché
        proxy_cache api_cache;
        proxy_cache_valid 200 10m;          # cachear 200 por 10 min
        proxy_cache_valid 404 1m;           # cachear 404 por 1 min
        proxy_cache_use_stale error timeout updating;

        # Header para debugging
        add_header X-Cache-Status $upstream_cache_status;
        # Valores: MISS, HIT, EXPIRED, STALE, UPDATING, BYPASS
    }
}
```

---

## 9. Errores comunes

### Error 1: 502 Bad Gateway

**Causa**: El backend no está corriendo o Nginx no puede conectar:

```bash
# ¿Backend corriendo?
ss -tlnp | grep 3000

# ¿Socket existe y tiene permisos?
ls -la /run/php/php-fpm.sock

# ¿SELinux bloquea la conexión? (RHEL)
setsebool -P httpd_can_network_connect 1       # para TCP
# Para sockets Unix, verificar contexto SELinux del socket
```

**Log de Nginx**:

```
connect() to 127.0.0.1:3000 failed (111: Connection refused)
connect() to unix:/run/php/php-fpm.sock failed (13: Permission denied)
```

### Error 2: Trailing slash causa redirección inesperada

```nginx
# Backend en puerto 3000, accedido internamente
location /app {
    proxy_pass http://127.0.0.1:3000;
}
# El cliente accede a /app (sin slash)
# El backend responde con redirect a /app/ (con slash)
# Pero el redirect incluye puerto 3000 → URL rota para el cliente
```

**Solución**: Usar trailing slash consistente en location y proxy_pass, o
añadir `proxy_redirect`:

```nginx
location /app/ {
    proxy_pass http://127.0.0.1:3000/;
    proxy_redirect default;
}
```

### Error 3: WebSocket se desconecta tras 60 segundos

**Causa**: `proxy_read_timeout` por defecto es 60s. Si no hay actividad en la
conexión WebSocket por 60 segundos, Nginx la cierra.

**Solución**:

```nginx
proxy_read_timeout 86400s;     # 24 horas
proxy_send_timeout 86400s;
```

O implementar ping/pong a nivel de aplicación para mantener actividad.

### Error 4: PHP muestra "No input file specified"

**Causa**: Falta `SCRIPT_FILENAME` en los parámetros FastCGI:

```nginx
# INCORRECTO
location ~ \.php$ {
    include fastcgi_params;
    fastcgi_pass unix:/run/php/php-fpm.sock;
    # ← falta SCRIPT_FILENAME
}

# CORRECTO
location ~ \.php$ {
    include fastcgi_params;
    fastcgi_param SCRIPT_FILENAME $document_root$fastcgi_script_name;
    fastcgi_pass unix:/run/php/php-fpm.sock;
}
```

### Error 5: proxy_set_header perdidos en location anidada

```nginx
server {
    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;

    location /api/ {
        proxy_set_header X-API-Version "2";
        proxy_pass http://backend;
        # ← Host, X-Real-IP y X-Forwarded-For PERDIDOS
    }
}
```

**Solución**: Usar `include proxy_params` dentro de cada location que haga
proxy, junto con cualquier header adicional.

---

## 10. Cheatsheet

```
┌─────────────────────────────────────────────────────────────────┐
│             NGINX PROXY INVERSO — REFERENCIA RÁPIDA             │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  PROXY BÁSICO:                                                  │
│  location /api/ {                                               │
│      proxy_pass http://127.0.0.1:3000;  ← SIN / = URI intacta  │
│      proxy_pass http://127.0.0.1:3000/; ← CON / = reemplaza    │
│      include proxy_params;                                      │
│  }                                                              │
│                                                                 │
│  CABECERAS ESENCIALES:                                          │
│  proxy_set_header Host $host;                                   │
│  proxy_set_header X-Real-IP $remote_addr;                       │
│  proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;   │
│  proxy_set_header X-Forwarded-Proto $scheme;                    │
│  proxy_http_version 1.1;                                        │
│  O simplemente: include proxy_params; (Debian)                  │
│                                                                 │
│  UPSTREAM (BALANCEO):                                           │
│  upstream backend {                                             │
│      [least_conn | ip_hash | hash $var];  algoritmo             │
│      server IP:PORT weight=N max_fails=N fail_timeout=Ns;       │
│      server IP:PORT backup;               solo si otros fallan  │
│  }                                                              │
│  proxy_pass http://backend;                                     │
│                                                                 │
│  TIMEOUTS:                                                      │
│  proxy_connect_timeout 5s;    conectar al backend               │
│  proxy_read_timeout 60s;      esperar respuesta                 │
│  proxy_send_timeout 60s;      enviar petición                   │
│                                                                 │
│  BUFFERING:                                                     │
│  proxy_buffering on;          defecto, libera backend rápido    │
│  proxy_buffering off;         para SSE/streaming                │
│  proxy_buffer_size 4k;        buffer para headers               │
│  proxy_buffers 8 4k;          buffers para body                 │
│                                                                 │
│  WEBSOCKET:                                                     │
│  map $http_upgrade $connection_upgrade {                        │
│      default upgrade; '' close;                                 │
│  }                                                              │
│  proxy_http_version 1.1;                                        │
│  proxy_set_header Upgrade $http_upgrade;                        │
│  proxy_set_header Connection $connection_upgrade;               │
│  proxy_read_timeout 86400s;                                     │
│                                                                 │
│  FASTCGI (PHP-FPM):                                             │
│  include fastcgi_params;                                        │
│  fastcgi_param SCRIPT_FILENAME $document_root$fastcgi_script_name│
│  fastcgi_pass unix:/run/php/php-fpm.sock;                       │
│                                                                 │
│  MANEJO DE FALLOS:                                              │
│  proxy_next_upstream error timeout http_502 http_503;           │
│  proxy_next_upstream_tries 3;                                   │
│                                                                 │
│  SELINUX (RHEL):                                                │
│  setsebool -P httpd_can_network_connect 1    (TCP a backend)    │
│                                                                 │
│  DEBUGGING:                                                     │
│  curl -v -H "Host: X" http://localhost/     probar headers      │
│  tail -f /var/log/nginx/error.log           ver errores proxy   │
│  add_header X-Cache-Status $upstream_cache_status;              │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: Proxy inverso básico con trailing slash

1. Inicia un servidor HTTP simple como backend:
   ```bash
   mkdir -p /tmp/backend && echo "Backend OK" > /tmp/backend/index.html
   python3 -m http.server 8000 --directory /tmp/backend
   ```
2. Configura un server block `proxy-lab.local` que reenvíe `/app/` al backend.
3. Prueba con **dos** configuraciones y anota la diferencia:
   - `proxy_pass http://127.0.0.1:8000;` (sin trailing slash)
   - `proxy_pass http://127.0.0.1:8000/;` (con trailing slash)
4. Para cada caso, accede a `http://proxy-lab.local/app/index.html` y observa
   qué URI llega al backend (revisa el log del servidor Python).
5. Configura `include proxy_params` y verifica con `curl -v` que las cabeceras
   `X-Real-IP` y `X-Forwarded-For` llegan al backend.

**Pregunta de predicción**: Si configuras `proxy_pass http://127.0.0.1:8000/v2/;`
y el cliente solicita `/app/users`, ¿qué URI recibe el backend? ¿Y si el
proxy_pass fuera `http://127.0.0.1:8000/v2` (sin trailing slash en la URI)?

### Ejercicio 2: Upstream con balanceo y failover

1. Inicia tres backends en puertos diferentes (8001, 8002, 8003), cada uno con
   un archivo `index.html` que identifique su puerto.
2. Configura un bloque `upstream` con los tres servidores y round-robin.
3. Haz 10 peticiones con un bucle `for i in $(seq 10); do curl ...; done` y
   verifica que se distribuyen entre los tres backends.
4. Detén uno de los backends y repite las peticiones. ¿Qué sucede con las
   peticiones que iban a ese backend?
5. Cambia a `least_conn` y repite el test.
6. Añade `weight=5` a uno de los backends y verifica que recibe más tráfico.
7. Marca uno como `backup` y verifica que solo recibe tráfico cuando los otros
   fallan.

**Pregunta de predicción**: Con `max_fails=2` y `fail_timeout=10s`, si un
backend falla 2 veces seguidas, ¿cuánto tiempo tardará Nginx en volver a
enviarle peticiones? ¿Qué pasa con las peticiones durante ese tiempo?

### Ejercicio 3: WebSocket y múltiples protocolos

1. Crea un servidor WebSocket simple con Node.js o Python:
   ```bash
   # Python con websockets
   pip install websockets
   python3 -c "
   import asyncio, websockets
   async def echo(ws):
       async for msg in ws:
           await ws.send(f'Echo: {msg}')
   asyncio.run(websockets.serve(echo, '127.0.0.1', 9000))
   "
   ```
2. Configura Nginx para que `/ws/` proxifique al servidor WebSocket con las
   cabeceras `Upgrade` y `Connection` correctas.
3. Configura que `/` sirva archivos estáticos y `/api/` proxifique a un
   backend HTTP (puede ser otro `python3 -m http.server`).
4. Prueba el WebSocket con `websocat` o un script simple.
5. Verifica que `/api/` y `/` funcionan con HTTP normal simultáneamente.
6. Establece `proxy_read_timeout 10s` para WebSocket y observa qué pasa cuando
   la conexión está inactiva más de 10 segundos. Luego súbelo a `86400s`.

**Pregunta de reflexión**: Nginx necesita `proxy_http_version 1.1` para
WebSocket porque HTTP/1.0 no soporta el mecanismo de Upgrade. ¿Por qué Nginx
usa HTTP/1.0 por defecto hacia backends en vez de HTTP/1.1? Piensa en
keep-alive y en cuándo fue diseñado el módulo de proxy.

---

> **Siguiente tema**: T03 — Servir archivos estáticos (root vs alias, try_files,
> caching).
