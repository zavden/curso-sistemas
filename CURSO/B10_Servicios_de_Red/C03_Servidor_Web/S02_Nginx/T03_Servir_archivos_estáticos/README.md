# Servir archivos estáticos con Nginx

## Índice

1. [Nginx y archivos estáticos](#1-nginx-y-archivos-estáticos)
2. [root vs alias](#2-root-vs-alias)
3. [try_files — control de fallback](#3-try_files--control-de-fallback)
4. [Tipos MIME y detección de contenido](#4-tipos-mime-y-detección-de-contenido)
5. [Caching y cabeceras de expiración](#5-caching-y-cabeceras-de-expiración)
6. [Compresión](#6-compresión)
7. [Optimizaciones de I/O](#7-optimizaciones-de-io)
8. [Patrones de configuración completos](#8-patrones-de-configuración-completos)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Nginx y archivos estáticos

Servir archivos estáticos es donde Nginx demuestra su mayor ventaja sobre
otros servidores. Su arquitectura event-driven y el uso de `sendfile()` le
permiten transferir archivos directamente desde el kernel al socket de red,
sin copiar datos al espacio de usuario.

```
Apache (tradicional):                  Nginx (sendfile):

Disco → kernel → userspace → kernel    Disco → kernel ────────► socket
        buffer    (Apache)    socket          (sin copia a userspace)

4 copias de datos                      2 copias de datos (zero-copy)
2 cambios de contexto extra            Sin cambios de contexto extra
```

### Separación de responsabilidades

El patrón más eficiente en producción es que Nginx sirva estáticos directamente
y delegue contenido dinámico al backend:

```
                        Nginx
                          │
               ┌──────────┼──────────┐
               │                     │
        /static/*                 /api/*
        /images/*                 /app/*
        /css/*                       │
        /js/*                        ▼
           │                    Backend
           ▼                   (Node/PHP/Python)
     Sistema de archivos
     (servido directo)
```

---

## 2. root vs alias

Las dos directivas para mapear URLs a rutas del filesystem. Su diferencia es
sutil pero causa errores frecuentes.

### 2.1 root — concatena la URI completa

`root` define el directorio base. Nginx **concatena** la URI completa
(incluyendo la parte que coincide con la location) al path:

```nginx
location /images/ {
    root /var/www/ejemplo;
}

# /images/logo.png → /var/www/ejemplo + /images/logo.png
#                   = /var/www/ejemplo/images/logo.png
```

La estructura en disco necesita reflejar la URL:

```
/var/www/ejemplo/
└── images/           ← directorio debe existir
    ├── logo.png
    └── banner.jpg
```

### 2.2 alias — reemplaza la location

`alias` sustituye la parte que coincide con la location por el path indicado:

```nginx
location /images/ {
    alias /var/www/ejemplo/static/;
}

# /images/logo.png → /var/www/ejemplo/static/ + logo.png
#                   = /var/www/ejemplo/static/logo.png
#
# La parte "/images/" se DESCARTA y se reemplaza por el alias
```

La estructura en disco puede ser diferente de la URL:

```
/var/www/ejemplo/
└── static/           ← no necesita llamarse "images"
    ├── logo.png
    └── banner.jpg
```

### 2.3 Comparación visual

```
URL solicitada: /images/photo.jpg

Con root /var/www/site:
  /var/www/site  +  /images/photo.jpg  =  /var/www/site/images/photo.jpg
  ─────────────     ─────────────────
  root              URI completa

Con alias /var/www/site/static/:
  /var/www/site/static/  +  photo.jpg  =  /var/www/site/static/photo.jpg
  ─────────────────────     ─────────
  alias                     URI sin location
```

### 2.4 Reglas de uso

| Aspecto              | root                              | alias                              |
|----------------------|-----------------------------------|------------------------------------|
| Comportamiento       | Concatena URI completa            | Reemplaza la parte de la location  |
| Trailing slash       | No importa                        | **Obligatorio** si location lo tiene|
| Con regex location   | Funciona                          | Funciona, pero cuidado con captura |
| Dónde usarlo         | Cuando la estructura del disco refleja las URLs | Cuando la ruta en disco difiere de la URL |
| Dentro de server     | Sí (hereda a todas las locations) | No — solo en location              |

### 2.5 La trampa del trailing slash con alias

```nginx
# INCORRECTO — falta trailing slash en alias
location /images/ {
    alias /var/www/static;      # ← sin /
}
# /images/logo.png → /var/www/staticlogo.png  ← ROTO

# CORRECTO
location /images/ {
    alias /var/www/static/;     # ← con /
}
# /images/logo.png → /var/www/static/logo.png
```

**Regla**: Si la location termina en `/`, el alias **debe** terminar en `/`.

### 2.6 Cuándo usar cada uno

```nginx
# root: cuando las URLs reflejan la estructura de archivos
# (la mayoría de los casos)
server {
    root /var/www/mi-sitio/public;

    location / {
        try_files $uri $uri/ =404;
    }

    # No necesita root ni alias — hereda del server
    location /css/ { }
    location /js/ { }
    location /images/ { }
}

# alias: cuando necesitas mapear una URL a una ruta diferente
server {
    root /var/www/mi-sitio/public;

    # Los favicon del sistema están en otro lugar
    location /favicon.ico {
        alias /usr/share/icons/favicon.ico;
    }

    # Media de usuarios en un volumen separado
    location /media/ {
        alias /mnt/storage/user-uploads/;
    }

    # Documentación generada fuera del proyecto
    location /docs/ {
        alias /opt/generated-docs/html/;
    }
}
```

---

## 3. try_files — control de fallback

`try_files` intenta servir archivos en orden y ejecuta un fallback si ninguno
existe. Es la directiva más importante para servir estáticos.

### 3.1 Sintaxis

```nginx
try_files FILE1 FILE2 ... FALLBACK;
```

- **FILE1, FILE2...**: Rutas a probar (relativas a root)
- **FALLBACK** (último argumento): Puede ser una URI interna (`/index.html`),
  un código de error (`=404`), o un named location (`@backend`)

### 3.2 Patrones fundamentales

**Estáticos simples con 404:**

```nginx
location / {
    try_files $uri $uri/ =404;
}
# 1. ¿Existe como archivo? → servirlo
# 2. ¿Existe como directorio? → servir su index
# 3. Devolver 404
```

**Front controller PHP:**

```nginx
location / {
    try_files $uri $uri/ /index.php$is_args$args;
}
# 1. ¿Archivo estático? → servirlo directo (CSS, JS, imágenes)
# 2. ¿Directorio? → su index
# 3. Redirigir internamente a index.php (framework se encarga)
```

**SPA (Single Page Application):**

```nginx
location / {
    try_files $uri /index.html;
}
# 1. ¿Archivo existe? → servirlo (bundle.js, style.css)
# 2. Todo lo demás → index.html (router JS maneja la ruta)
```

**Named location como fallback:**

```nginx
location / {
    try_files $uri $uri/ @backend;
}

location @backend {
    proxy_pass http://127.0.0.1:3000;
    include proxy_params;
}
# 1. Intentar servir archivo estático
# 2. Si no existe, reenviar al backend
```

### 3.3 Cómo funciona internamente

```
Petición: GET /css/style.css
root: /var/www/app/public

try_files $uri $uri/ =404;

Paso 1: $uri = /css/style.css
  → stat("/var/www/app/public/css/style.css")
  → ¿Existe y es archivo? SÍ → servir con sendfile()

Petición: GET /about
root: /var/www/app/public

try_files $uri $uri/ /index.html;

Paso 1: $uri = /about
  → stat("/var/www/app/public/about")
  → No existe

Paso 2: $uri/ = /about/
  → stat("/var/www/app/public/about/")
  → No existe como directorio

Paso 3: fallback /index.html
  → Redirección interna a /index.html
  → stat("/var/www/app/public/index.html")
  → Existe → servir
```

### 3.4 try_files con location anidados

```nginx
# Servir estáticos, PHP al backend, 404 para todo lo demás
location / {
    try_files $uri $uri/ @php;
}

location @php {
    include fastcgi_params;
    fastcgi_param SCRIPT_FILENAME $document_root/index.php;
    fastcgi_param PATH_INFO $uri;
    fastcgi_pass unix:/run/php/php-fpm.sock;
}

location ~ \.php$ {
    # Acceso directo a .php: verificar que existe
    try_files $uri =404;
    include fastcgi_params;
    fastcgi_param SCRIPT_FILENAME $document_root$fastcgi_script_name;
    fastcgi_pass unix:/run/php/php-fpm.sock;
}
```

---

## 4. Tipos MIME y detección de contenido

### 4.1 mime.types

Nginx usa el archivo `/etc/nginx/mime.types` para mapear extensiones a
Content-Type. Sin este mapeo, los archivos se sirven con el tipo por defecto:

```nginx
http {
    include /etc/nginx/mime.types;
    default_type application/octet-stream;
    # default_type: se usa cuando la extensión no está en mime.types
    # application/octet-stream → el navegador ofrece descarga
}
```

### 4.2 Añadir tipos personalizados

```nginx
http {
    include mime.types;

    # Tipos adicionales
    types {
        application/wasm  wasm;
        text/markdown     md;
        application/json  map;       # source maps
    }
}
```

### 4.3 Forzar tipo por location

```nginx
# Forzar descarga (no mostrar en el navegador)
location /downloads/ {
    types { }                           # limpiar tipos
    default_type application/octet-stream;
    add_header Content-Disposition "attachment";
}

# Forzar texto plano para logs
location /logs/ {
    default_type text/plain;
    charset utf-8;
}
```

---

## 5. Caching y cabeceras de expiración

### 5.1 Directiva expires

La forma más directa de controlar el caché del navegador:

```nginx
# Por location
location /static/ {
    expires 30d;    # Cache-Control: max-age=2592000
}

location /images/ {
    expires 1y;     # Cache-Control: max-age=31536000
}

location /api/ {
    expires -1;     # Cache-Control: no-cache
    # Fuerza revalidación en cada petición
}

location = /index.html {
    expires epoch;  # Cache-Control: no-cache (Expires: 1 Jan 1970)
    # Nunca cachear la página principal (SPA)
}
```

**Valores de expires:**

| Valor       | Cache-Control generado          | Uso                        |
|-------------|---------------------------------|----------------------------|
| `30d`       | `max-age=2592000`               | Estáticos versionados      |
| `1h`        | `max-age=3600`                  | Contenido semi-estático    |
| `epoch`     | `no-cache` (Expires: epoch)     | Forzar revalidación        |
| `-1`        | `no-cache`                      | No cachear                 |
| `off`       | No envía headers de caché       | Control manual             |
| `max`       | `max-age=315360000` (~10 años)  | Assets con hash en nombre  |

### 5.2 Cache-Control avanzado con add_header

Para control granular, añadir `Cache-Control` directamente:

```nginx
# Assets con hash en el nombre (bundle.a1b2c3.js)
# Cachear "para siempre" — el hash cambia al actualizar
location ~* \.[a-f0-9]{8,}\.(js|css)$ {
    expires max;
    add_header Cache-Control "public, immutable";
}

# Fuentes: cacheable pero revalidar periódicamente
location ~* \.(woff2?|ttf|eot)$ {
    expires 30d;
    add_header Cache-Control "public";
}

# HTML: revalidar siempre (para que el navegador pida nuevos assets)
location ~* \.html$ {
    expires epoch;
    add_header Cache-Control "no-cache, must-revalidate";
}
```

### 5.3 ETag y Last-Modified

Nginx envía automáticamente `ETag` y `Last-Modified` para archivos estáticos.
Estos permiten **revalidación condicional**:

```
Primera petición:
  Cliente → GET /style.css
  Nginx   → 200 OK
             ETag: "65f2a1b3-1a4f"
             Last-Modified: Sun, 14 Mar 2026 10:00:00 GMT
             (envía el archivo completo)

Peticiones siguientes (con caché expirado):
  Cliente → GET /style.css
             If-None-Match: "65f2a1b3-1a4f"
             If-Modified-Since: Sun, 14 Mar 2026 10:00:00 GMT
  Nginx   → 304 Not Modified
             (NO envía el archivo, ahorra ancho de banda)
```

```nginx
# Controlar ETag y Last-Modified
etag on;                # defecto: on
if_modified_since exact; # defecto: exact
# Valores: exact, before (acepta fechas anteriores), off
```

### 5.4 Patrón por tipo de contenido

```nginx
# Agrupar por política de caché
location ~* \.(jpg|jpeg|png|gif|webp|svg|ico)$ {
    expires 30d;
    add_header Cache-Control "public";
    access_log off;         # no registrar cada imagen en access.log
}

location ~* \.(css|js)$ {
    expires 7d;
    add_header Cache-Control "public";
}

location ~* \.(woff2?|ttf|otf|eot)$ {
    expires 30d;
    add_header Cache-Control "public";
    add_header Access-Control-Allow-Origin "*";    # CORS para fuentes
}

location ~* \.(pdf|doc|docx|xls|xlsx)$ {
    expires 1d;
}
```

---

## 6. Compresión

### 6.1 gzip

```nginx
http {
    # Activar compresión
    gzip on;

    # Nivel de compresión (1-9, mayor = más compresión pero más CPU)
    gzip_comp_level 5;        # buen balance CPU/tamaño

    # Tamaño mínimo para comprimir (no comprimir archivos pequeños)
    gzip_min_length 256;

    # Comprimir también respuestas proxied
    gzip_proxied any;

    # Añadir Vary: Accept-Encoding (importante para CDN/proxies)
    gzip_vary on;

    # Tipos a comprimir (nunca imágenes/video — ya están comprimidos)
    gzip_types
        text/plain
        text/css
        text/javascript
        text/xml
        application/json
        application/javascript
        application/xml
        application/xml+rss
        application/atom+xml
        application/wasm
        image/svg+xml;

    # No comprimir para IE6 (legacy, ya innecesario)
    # gzip_disable "msie6";
}
```

### 6.2 gzip_static — archivos pre-comprimidos

Si el sistema de build genera archivos `.gz` pre-comprimidos, Nginx puede
servirlos directamente sin gastar CPU en compresión en tiempo real:

```nginx
location /static/ {
    gzip_static on;
    # Si existe /static/bundle.js.gz, lo sirve en lugar de /static/bundle.js
    # cuando el cliente acepta gzip (Accept-Encoding: gzip)
}
```

```
/var/www/app/public/static/
├── bundle.js             ← original (para clientes sin gzip)
├── bundle.js.gz          ← pre-comprimido (servido con gzip_static)
├── style.css
└── style.css.gz
```

> **Requisito**: Compilar Nginx con `--with-http_gzip_static_module` (incluido
> en la mayoría de paquetes de distribución).

### 6.3 Verificar compresión

```bash
# Petición con Accept-Encoding: gzip
curl -H "Accept-Encoding: gzip" -I http://localhost/style.css
# Buscar: Content-Encoding: gzip

# Ver tamaño comprimido vs original
curl -s -H "Accept-Encoding: gzip" http://localhost/style.css | wc -c
curl -s http://localhost/style.css | wc -c
```

---

## 7. Optimizaciones de I/O

### 7.1 sendfile

Transfiere archivos del disco al socket sin pasar por el espacio de usuario:

```nginx
http {
    sendfile on;         # defecto: off (pero casi siempre se activa)
}
```

**Sin sendfile**: `read()` del disco a buffer en userspace → `write()` del
buffer al socket.

**Con sendfile**: `sendfile()` syscall hace la copia directamente en el kernel.

### 7.2 tcp_nopush y tcp_nodelay

Estas dos directivas trabajan juntas para optimizar el envío de datos:

```nginx
http {
    sendfile on;
    tcp_nopush on;      # envía headers + comienzo de archivo en un paquete
    tcp_nodelay on;     # desactiva Nagle para keep-alive (baja latencia)
}
```

```
Sin tcp_nopush:                   Con tcp_nopush:
Paquete 1: [HTTP headers]        Paquete 1: [HTTP headers + datos]
Paquete 2: [datos...]            Paquete 2: [datos...]
Paquete 3: [datos...]            (un paquete menos)
```

`tcp_nopush` y `tcp_nodelay` parecen contradictorios (nopush acumula, nodelay
envía inmediatamente), pero Nginx los combina: usa nopush para el envío
inicial (headers + datos juntos), luego cambia a nodelay para los datos
restantes.

### 7.3 open_file_cache

Cachea metadata de archivos frecuentemente accedidos (descriptores, tamaño,
fecha de modificación):

```nginx
http {
    # Cachear hasta 1000 descriptores, expirar inactivos en 60s
    open_file_cache max=1000 inactive=60s;

    # Revalidar cada 30s que los archivos no cambiaron
    open_file_cache_valid 30s;

    # Mínimo de accesos antes de cachear
    open_file_cache_min_uses 2;

    # Cachear también errores (archivo no encontrado)
    open_file_cache_errors on;
}
```

**Beneficio**: Evita repetir llamadas `stat()` y `open()` para archivos
servidos frecuentemente. Especialmente útil en servidores con miles de archivos
estáticos.

> **Advertencia**: Si los archivos cambian frecuentemente (deployments),
> `open_file_cache_valid` debe ser corto o se servirán versiones stale durante
> ese periodo.

### 7.4 directio

Para archivos muy grandes (videos, ISOs), `directio` bypasea el caché del
sistema de archivos del kernel, evitando que desplace archivos más útiles:

```nginx
location /videos/ {
    directio 10m;        # usar direct I/O para archivos > 10 MB
    aio on;              # I/O asíncrono (recomendado con directio)
    sendfile off;        # sendfile no es compatible con directio
    output_buffers 1 1m; # buffer de 1 MB para output
}
```

---

## 8. Patrones de configuración completos

### 8.1 Sitio estático optimizado

```nginx
server {
    listen 80;
    server_name static.ejemplo.com;
    root /var/www/static-site;

    # Seguridad
    server_tokens off;
    add_header X-Content-Type-Options "nosniff";
    add_header X-Frame-Options "SAMEORIGIN";

    # Página por defecto y fallback
    index index.html;
    error_page 404 /404.html;

    location / {
        try_files $uri $uri/ =404;
    }

    # Assets con hash (cache agresivo)
    location ~* \.[a-f0-9]{8,}\.(js|css|png|jpg|webp|woff2)$ {
        expires max;
        add_header Cache-Control "public, immutable";
        access_log off;
    }

    # Assets sin hash
    location ~* \.(js|css)$ {
        expires 7d;
        add_header Cache-Control "public";
    }

    # Imágenes y fuentes
    location ~* \.(jpg|jpeg|png|gif|webp|svg|ico|woff2?|ttf)$ {
        expires 30d;
        add_header Cache-Control "public";
        access_log off;
    }

    # Archivos ocultos: denegar
    location ~ /\. {
        deny all;
        access_log off;
        log_not_found off;
    }
}
```

### 8.2 SPA con backend API

```nginx
server {
    listen 80;
    server_name app.ejemplo.com;
    root /var/www/app/dist;

    # HTML: nunca cachear (para que nuevos deploys se apliquen)
    location = /index.html {
        expires epoch;
        add_header Cache-Control "no-cache, must-revalidate";
    }

    # Assets con hash del build
    location /assets/ {
        expires max;
        add_header Cache-Control "public, immutable";
        access_log off;
    }

    # SPA fallback: todo lo que no sea archivo → index.html
    location / {
        try_files $uri /index.html;
    }

    # API → backend
    location /api/ {
        proxy_pass http://127.0.0.1:3000;
        proxy_http_version 1.1;
        include proxy_params;
        proxy_read_timeout 30s;
    }

    # WebSocket
    location /ws/ {
        proxy_pass http://127.0.0.1:3000;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_read_timeout 86400s;
    }
}
```

### 8.3 File server con listado de directorio

```nginx
server {
    listen 80;
    server_name files.ejemplo.com;
    root /srv/shared;

    # Habilitar listado de directorio
    autoindex on;
    autoindex_exact_size off;       # mostrar KB/MB en vez de bytes
    autoindex_localtime on;         # hora local en vez de UTC
    autoindex_format html;          # html, xml, json, jsonp

    # Limitar acceso a red interna
    allow 10.0.0.0/8;
    allow 192.168.0.0/16;
    deny all;

    # Archivos grandes: optimizar
    location ~* \.(iso|zip|tar\.gz|tar\.bz2)$ {
        directio 10m;
        aio on;
        sendfile off;
    }

    # Prevenir ejecución accidental
    location ~ \.(php|py|sh|pl)$ {
        default_type text/plain;     # mostrar como texto, no ejecutar
    }
}
```

---

## 9. Errores comunes

### Error 1: alias sin trailing slash

```nginx
# INCORRECTO
location /media/ {
    alias /var/www/uploads;
}
# /media/photo.jpg → /var/www/uploadsphoto.jpg  ← ruta corrupta

# CORRECTO
location /media/ {
    alias /var/www/uploads/;
}
# /media/photo.jpg → /var/www/uploads/photo.jpg
```

**Diagnóstico**: Error 404 para archivos que sí existen, o errores extraños en
el error log con rutas concatenadas incorrectamente.

### Error 2: try_files con proxy_pass sin named location

```nginx
# INCORRECTO — proxy_pass no puede ser argumento directo de try_files
location / {
    try_files $uri $uri/ proxy_pass http://backend;    # ← ERROR SINTAXIS
}

# CORRECTO — usar named location
location / {
    try_files $uri $uri/ @backend;
}
location @backend {
    proxy_pass http://127.0.0.1:3000;
    include proxy_params;
}
```

### Error 3: Caché agresivo rompe actualizaciones

**Causa**: Se configuró `expires max` para archivos CSS/JS sin hash en el
nombre. Tras un deploy, los usuarios siguen viendo la versión antigua.

```nginx
# PROBLEMÁTICO
location ~* \.(css|js)$ {
    expires max;    # navegador no pide el archivo por meses
}

# SEGURO — cachear solo archivos con hash
location ~* \.[a-f0-9]{8,}\.(css|js)$ {
    expires max;
    add_header Cache-Control "public, immutable";
}

# Archivos sin hash: caché corto
location ~* \.(css|js)$ {
    expires 1h;
}
```

### Error 4: gzip en archivos ya comprimidos

**Causa**: Incluir formatos como JPEG, PNG, WOFF2 en `gzip_types`. Estos ya
están comprimidos internamente; gzip no los reduce y gasta CPU.

**Solución**: Solo comprimir formatos de texto. Nunca incluir:
`image/jpeg`, `image/png`, `image/gif`, `image/webp`, `font/woff2`,
`application/zip`, `video/*`, `audio/*`.

### Error 5: 403 Forbidden por permisos en la cadena de directorios

```bash
# El archivo existe y es legible, pero un directorio padre no lo es
namei -l /var/www/app/public/index.html
# f: /var/www/app/public/index.html
# drwxr-xr-x root root /
# drwxr-xr-x root root var
# drwxr-xr-x root root www
# drwx------ deploy deploy app    ← Nginx no puede atravesar (falta +x)
# drwxr-xr-x deploy deploy public
# -rw-r--r-- deploy deploy index.html
```

**Solución**: Cada directorio en la cadena necesita permiso de ejecución (`+x`)
para el usuario de Nginx:

```bash
chmod o+x /var/www/app
```

---

## 10. Cheatsheet

```
┌─────────────────────────────────────────────────────────────────┐
│           NGINX ARCHIVOS ESTÁTICOS — REFERENCIA RÁPIDA          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ROOT VS ALIAS:                                                 │
│  root /path;    → /path + URI completa                          │
│  alias /path/;  → /path/ + URI sin location (¡trailing slash!)  │
│  root: defecto, heredable, URLs = estructura disco              │
│  alias: solo en location, URLs ≠ estructura disco               │
│                                                                 │
│  TRY_FILES:                                                     │
│  try_files $uri $uri/ =404;              estáticos              │
│  try_files $uri /index.html;             SPA                    │
│  try_files $uri $uri/ /index.php...;     PHP front controller   │
│  try_files $uri $uri/ @backend;          fallback a proxy       │
│                                                                 │
│  CACHING:                                                       │
│  expires 30d;                 Cache-Control: max-age=2592000    │
│  expires max;                 ~10 años (assets con hash)        │
│  expires epoch;               no-cache (HTML dinámico)          │
│  expires -1;                  no-cache                          │
│  add_header Cache-Control "public, immutable";                  │
│                                                                 │
│  COMPRESIÓN:                                                    │
│  gzip on;                                                       │
│  gzip_comp_level 5;           balance CPU/tamaño                │
│  gzip_min_length 256;         no comprimir pequeños             │
│  gzip_types text/plain text/css application/json ...;           │
│  gzip_static on;              servir .gz pre-comprimidos        │
│  NUNCA comprimir: JPEG, PNG, WOFF2, ZIP, video, audio          │
│                                                                 │
│  OPTIMIZACIÓN I/O:                                              │
│  sendfile on;                 zero-copy kernel→socket           │
│  tcp_nopush on;               headers+datos en un paquete       │
│  tcp_nodelay on;              baja latencia en keep-alive       │
│  open_file_cache max=1000 inactive=60s;  cachear metadata       │
│  directio 10m;                para archivos >10MB               │
│                                                                 │
│  TIPOS MIME:                                                    │
│  include mime.types;          mapeo estándar                    │
│  default_type application/octet-stream;  tipo por defecto       │
│  types { application/wasm wasm; }  tipos adicionales            │
│                                                                 │
│  LISTADO DE DIRECTORIO:                                         │
│  autoindex on;                habilitar                         │
│  autoindex_exact_size off;    mostrar KB/MB                     │
│  autoindex_localtime on;      hora local                        │
│                                                                 │
│  SEGURIDAD:                                                     │
│  location ~ /\. { deny all; }   ocultar archivos .hidden       │
│  access_log off;                no loguear assets               │
│  log_not_found off;             no loguear 404 de assets        │
│                                                                 │
│  PATRONES COMUNES POR TIPO:                                     │
│  Imágenes/fuentes .... expires 30d, access_log off              │
│  CSS/JS sin hash ..... expires 7d                               │
│  CSS/JS con hash ..... expires max, immutable                   │
│  HTML ................. expires epoch, no-cache                  │
│  Archivos grandes .... directio + aio, sendfile off             │
│                                                                 │
│  VERIFICAR:                                                     │
│  curl -I URL                  ver headers de caché              │
│  curl -H "Accept-Encoding: gzip" -I URL   ver compresión       │
│  namei -l /path/to/file       verificar cadena de permisos      │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: root vs alias

1. Crea un server block para `static-lab.local` con `root /var/www/static-lab`.
2. Crea la estructura:
   ```
   /var/www/static-lab/
   ├── index.html
   ├── images/
   │   └── logo.png
   └── css/
       └── style.css
   ```
3. Verifica que `http://static-lab.local/images/logo.png` funciona con `root`.
4. Ahora crea un directorio `/opt/media/` con un archivo `video.mp4`.
5. Configura `location /media/` con `alias /opt/media/;` para servir ese archivo.
6. Verifica que `http://static-lab.local/media/video.mp4` funciona.
7. Elimina el trailing slash del alias y prueba de nuevo. ¿Qué error obtienes?
8. Verifica el error log para ver la ruta que Nginx intentó acceder.

**Pregunta de predicción**: Si configuras `location /docs/` con
`root /opt/documentation;`, ¿dónde busca Nginx el archivo cuando solicitas
`/docs/manual.pdf`? ¿En `/opt/documentation/manual.pdf` o en
`/opt/documentation/docs/manual.pdf`?

### Ejercicio 2: try_files y caching para una SPA

1. Simula una SPA creando esta estructura:
   ```
   /var/www/spa/
   ├── index.html          (contiene <script src="/assets/app.js">)
   ├── assets/
   │   ├── app.a1b2c3d4.js
   │   ├── app.a1b2c3d4.css
   │   └── logo.png
   └── favicon.ico
   ```
2. Configura el server block con:
   - `try_files $uri /index.html;` en `location /`
   - `expires epoch` para `index.html` (exacto con `= /index.html`)
   - `expires max` + `immutable` para assets con hash
   - `expires 7d` para assets sin hash (logo.png, favicon.ico)
3. Prueba con `curl -I` y verifica los headers `Cache-Control` de cada recurso.
4. Prueba que `http://spa.local/ruta/inexistente` devuelve `index.html` (SPA
   routing).
5. Prueba que `http://spa.local/assets/app.a1b2c3d4.js` devuelve el archivo
   directamente (no index.html).

**Pregunta de predicción**: Si un usuario tiene `app.a1b2c3d4.js` en su caché
del navegador con `immutable` y haces un deploy que genera `app.e5f6g7h8.js`,
¿el usuario verá la versión nueva o la antigua? ¿Qué mecanismo garantiza la
actualización?

### Ejercicio 3: Optimización y compresión

1. Crea un archivo CSS grande (> 10 KB) repitiendo contenido.
2. Configura gzip con `gzip_comp_level 5` y `gzip_min_length 256`.
3. Solicita el archivo con y sin `Accept-Encoding: gzip`:
   ```bash
   curl -s http://lab.local/big.css | wc -c
   curl -s -H "Accept-Encoding: gzip" http://lab.local/big.css | wc -c
   ```
4. Calcula el ratio de compresión.
5. Genera la versión `.gz` pre-comprimida:
   ```bash
   gzip -k -9 /var/www/lab/big.css
   ```
6. Activa `gzip_static on` y verifica que Nginx sirve el `.gz` directamente
   (el timestamp de la respuesta debe coincidir con el archivo `.gz`).
7. Activa `open_file_cache` y `sendfile on`. Ejecuta un benchmark simple con
   `ab -n 5000 -c 20` antes y después de activar las optimizaciones.

**Pregunta de reflexión**: `gzip_static` permite pre-comprimir con nivel 9
(máxima compresión) sin coste de CPU en cada petición, mientras que `gzip`
en tiempo real usa un nivel menor (5) para no saturar la CPU. ¿En qué tipo de
despliegue es más práctico cada enfoque? ¿Qué pasa si el archivo original
cambia pero el `.gz` no se regenera?

---

> **Siguiente tema**: T04 — Apache vs Nginx (diferencias de arquitectura,
> prefork/worker vs event-driven).
