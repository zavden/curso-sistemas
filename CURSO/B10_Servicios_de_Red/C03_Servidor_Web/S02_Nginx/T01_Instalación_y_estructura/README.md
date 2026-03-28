# Instalación y estructura de Nginx

## Índice

1. [Nginx en el ecosistema web](#1-nginx-en-el-ecosistema-web)
2. [Instalación](#2-instalación)
3. [Arquitectura de procesos](#3-arquitectura-de-procesos)
4. [Estructura de archivos de configuración](#4-estructura-de-archivos-de-configuración)
5. [Anatomía de nginx.conf](#5-anatomía-de-nginxconf)
6. [Server blocks (Virtual Hosts)](#6-server-blocks-virtual-hosts)
7. [Location blocks — enrutamiento de URLs](#7-location-blocks--enrutamiento-de-urls)
8. [Herencia y fusión de directivas](#8-herencia-y-fusión-de-directivas)
9. [Gestión del servicio](#9-gestión-del-servicio)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Nginx en el ecosistema web

Nginx (pronunciado "engine-x") fue creado en 2004 por Igor Sysoev para resolver
el problema C10K: servir 10,000 conexiones concurrentes con recursos limitados.
Su arquitectura event-driven lo hace fundamentalmente diferente a Apache.

```
                    Cliente (navegador)
                          │
                          ▼
              ┌───────────────────────┐
              │        Nginx          │
              │                       │
              │  ┌─────────────────┐  │
              │  │  master process │  │  ← lee configuración, gestiona workers
              │  └────────┬────────┘  │
              │           │           │
              │    ┌──────┼──────┐    │
              │    │      │      │    │
              │  ┌─▼─┐  ┌─▼─┐  ┌─▼─┐ │
              │  │ W1 │  │ W2 │  │ W3 │ │  ← worker processes (event loop)
              │  └────┘  └────┘  └────┘ │
              │                       │
              │  Cada worker maneja   │
              │  miles de conexiones  │
              │  con un solo thread   │
              └───────────────────────┘
```

### Nginx vs Apache — diferencia fundamental

| Aspecto                | Apache (prefork/worker)        | Nginx                          |
|------------------------|--------------------------------|--------------------------------|
| Modelo                 | Proceso/thread por conexión    | Event loop, async no-bloqueante|
| Conexiones concurrentes| Limitadas por procesos/threads | Miles por worker               |
| Memoria por conexión   | ~10 MB (con módulos)           | ~2.5 KB                        |
| Contenido dinámico     | Módulos internos (mod_php)     | Delegado a backends (FastCGI)  |
| Configuración dinámica | .htaccess (por directorio)     | No existe equivalente          |
| Recarga de config      | graceful reload                | reload (sin cortar conexiones) |

Nginx no procesa contenido dinámico internamente. Siempre delega a un
backend (PHP-FPM, Gunicorn, Node.js, etc.) vía proxy inverso o FastCGI. Esto
separa responsabilidades: Nginx sirve estáticos y gestiona conexiones, el
backend procesa lógica.

---

## 2. Instalación

### 2.1 Debian/Ubuntu

```bash
# Instalar desde repositorios de la distribución
apt update
apt install nginx

# Verificar
nginx -v                     # versión
systemctl status nginx

# Archivos principales instalados
# /etc/nginx/                configuración
# /var/www/html/             DocumentRoot por defecto
# /var/log/nginx/            logs (access.log, error.log)
# /usr/sbin/nginx            binario
# /usr/share/nginx/          archivos por defecto
```

### 2.2 RHEL/Fedora

```bash
# Instalar
dnf install nginx

# Verificar
nginx -v
systemctl status nginx

# Archivos principales instalados
# /etc/nginx/                configuración
# /usr/share/nginx/html/     DocumentRoot por defecto
# /var/log/nginx/            logs
# /usr/sbin/nginx            binario
```

### 2.3 Repositorio oficial de Nginx

Los repositorios de las distribuciones suelen tener versiones algo antiguas.
Para la versión más reciente, usar el repositorio oficial:

```bash
# Debian/Ubuntu
# Añadir clave GPG y repositorio (ver nginx.org/en/linux_packages.html)
apt install curl gnupg2 ca-certificates lsb-release
curl -fsSL https://nginx.org/keys/nginx_signing.key | \
    gpg --dearmor -o /usr/share/keyrings/nginx-archive-keyring.gpg

echo "deb [signed-by=/usr/share/keyrings/nginx-archive-keyring.gpg] \
    http://nginx.org/packages/ubuntu $(lsb_release -cs) nginx" \
    > /etc/apt/sources.list.d/nginx.list

apt update
apt install nginx

# RHEL/Fedora
cat > /etc/yum.repos.d/nginx.repo << 'EOF'
[nginx-stable]
name=nginx stable repo
baseurl=http://nginx.org/packages/centos/$releasever/$basearch/
gpgcheck=1
enabled=1
gpgkey=https://nginx.org/keys/nginx_signing.key
module_hotfixes=true
EOF

dnf install nginx
```

### 2.4 Habilitar e iniciar

```bash
systemctl enable --now nginx

# Firewall (RHEL/Fedora)
firewall-cmd --permanent --add-service=http
firewall-cmd --permanent --add-service=https
firewall-cmd --reload

# SELinux (RHEL) — contextos por defecto
# /usr/share/nginx/html(/.*)? → httpd_sys_content_t
# Nginx usa los mismos contextos SELinux que Apache (httpd_*)

# Verificar respuesta
curl -I http://localhost
```

---

## 3. Arquitectura de procesos

### 3.1 Master y workers

Nginx opera con exactamente dos tipos de proceso:

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│  master process (PID 1 o PID del servicio)                 │
│  ─────────────────────────────────────────                 │
│  • Ejecuta como root                                       │
│  • Lee y valida configuración                              │
│  • Crea/gestiona worker processes                          │
│  • Abre puertos privilegiados (80, 443)                    │
│  • Gestiona señales (reload, reopen, stop)                 │
│  • NO procesa peticiones                                   │
│                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │ worker 1     │  │ worker 2     │  │ worker N     │     │
│  │ (nginx user) │  │ (nginx user) │  │ (nginx user) │     │
│  │              │  │              │  │              │     │
│  │ event loop:  │  │ event loop:  │  │ event loop:  │     │
│  │  epoll/kqueue│  │  epoll/kqueue│  │  epoll/kqueue│     │
│  │  accept()    │  │  accept()    │  │  accept()    │     │
│  │  read/write  │  │  read/write  │  │  read/write  │     │
│  │              │  │              │  │              │     │
│  │ ~1000+ conn  │  │ ~1000+ conn  │  │ ~1000+ conn  │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 Configuración de workers

```nginx
# En nginx.conf (contexto main)

# Número de worker processes
worker_processes auto;       # auto = 1 por núcleo de CPU (recomendado)
# worker_processes 4;        # fijo

# Máximo de conexiones simultáneas por worker
events {
    worker_connections 1024;  # defecto: 512 o 1024

    # Mecanismo de notificación de eventos
    # Linux: epoll (por defecto, el más eficiente)
    # FreeBSD/macOS: kqueue
    # use epoll;

    # Aceptar múltiples conexiones a la vez
    multi_accept on;
}
```

**Cálculo de conexiones máximas:**

```
Conexiones máximas = worker_processes × worker_connections

Ejemplo: 4 workers × 1024 = 4,096 conexiones simultáneas

Para proxy inverso (cada petición = 2 conexiones: cliente + backend):
  Clientes máximos = (worker_processes × worker_connections) / 2
```

### 3.3 Verificar procesos

```bash
ps aux | grep nginx
# root      1234  ... nginx: master process /usr/sbin/nginx
# nginx     1235  ... nginx: worker process
# nginx     1236  ... nginx: worker process
# nginx     1237  ... nginx: worker process
# nginx     1238  ... nginx: worker process
```

---

## 4. Estructura de archivos de configuración

### 4.1 Debian/Ubuntu

```
/etc/nginx/
├── nginx.conf                 ← archivo principal
├── mime.types                 ← mapeo extensión → Content-Type
├── fastcgi_params             ← parámetros para backends FastCGI
├── proxy_params               ← parámetros para proxying
├── scgi_params                ← parámetros SCGI
├── uwsgi_params               ← parámetros uWSGI
│
├── sites-available/           ← server blocks disponibles
│   └── default                ← sitio por defecto
├── sites-enabled/             ← symlinks a sites-available/ (activos)
│   └── default -> ../sites-available/default
│
├── conf.d/                    ← fragmentos de configuración
│   └── (vacío por defecto)
│
├── snippets/                  ← fragmentos reutilizables
│   ├── fastcgi-php.conf       ← config estándar para PHP-FPM
│   └── snakeoil.conf          ← certificado autofirmado para pruebas
│
└── modules-enabled/           ← módulos dinámicos (symlinks)
    └── ...
```

Modelo similar a Apache en Debian: `sites-available` + `sites-enabled` con
symlinks.

### 4.2 RHEL/Fedora

```
/etc/nginx/
├── nginx.conf                 ← archivo principal (incluye server block default)
├── mime.types
├── fastcgi_params
├── scgi_params
├── uwsgi_params
│
├── conf.d/                    ← server blocks van aquí
│   └── (vacío o con default.conf)
│
└── default.d/                 ← fragmentos incluidos en el server block default
```

RHEL no usa `sites-available`/`sites-enabled`. Los server blocks se crean
directamente en `conf.d/` como archivos `.conf`.

### 4.3 Comparación

```
DEBIAN/UBUNTU                          RHEL/FEDORA
─────────────                          ───────────
nginx.conf                             nginx.conf
    │                                      │
    ├── include modules-enabled/*.conf     ├── (módulos en nginx.conf)
    │                                      │
    ├── include conf.d/*.conf              ├── include conf.d/*.conf
    │                                      │   (server blocks aquí)
    └── include sites-enabled/*            │
        (server blocks aquí)               └── include default.d/*.conf
                                               (fragmentos del server default)
```

---

## 5. Anatomía de nginx.conf

La configuración de Nginx es jerárquica, organizada en **contextos** anidados
con llaves:

```nginx
# ─── Contexto main (global) ───
user nginx;                          # Debian: www-data
worker_processes auto;
error_log /var/log/nginx/error.log warn;
pid /run/nginx.pid;

# ─── Contexto events ───
events {
    worker_connections 1024;
}

# ─── Contexto http ───
http {
    # Configuración HTTP global
    include /etc/nginx/mime.types;
    default_type application/octet-stream;

    # Formato de log
    log_format main '$remote_addr - $remote_user [$time_local] '
                    '"$request" $status $body_bytes_sent '
                    '"$http_referer" "$http_user_agent"';
    access_log /var/log/nginx/access.log main;

    # Optimizaciones
    sendfile on;
    tcp_nopush on;
    tcp_nodelay on;
    keepalive_timeout 65;
    types_hash_max_size 2048;

    # Compresión
    gzip on;
    gzip_types text/plain text/css application/json application/javascript;

    # Incluir server blocks
    include /etc/nginx/conf.d/*.conf;
    include /etc/nginx/sites-enabled/*;    # solo Debian
}
```

### 5.1 Jerarquía de contextos

```
main                           ← proceso global
├── events { }                 ← modelo de conexiones
└── http { }                   ← configuración HTTP global
    ├── upstream { }           ← grupos de backends
    └── server { }             ← server block (≈ VirtualHost)
        └── location { }      ← enrutamiento por URL
            └── location { }  ← location anidado
```

Las directivas definidas en un contexto superior se heredan en los inferiores,
salvo que se redefinan:

```nginx
http {
    gzip on;                   # aplica a todos los server blocks

    server {
        # hereda gzip on del http { }

        location /api/ {
            gzip off;          # sobreescribe para esta location
        }
    }
}
```

### 5.2 Directivas principales del contexto http

| Directiva             | Propósito                                            |
|-----------------------|------------------------------------------------------|
| `sendfile on`         | Transferencia directa kernel→socket (evita userspace)|
| `tcp_nopush on`       | Envía headers y comienzo de archivo en un solo paquete|
| `tcp_nodelay on`      | Desactiva Nagle para keep-alive (baja latencia)      |
| `keepalive_timeout N` | Segundos que una conexión keep-alive permanece abierta|
| `client_max_body_size`| Tamaño máximo de body (uploads). Defecto: 1 MB       |
| `server_tokens off`   | Oculta versión de Nginx en headers y páginas de error|
| `gzip on`             | Activa compresión de respuestas                      |
| `include`             | Incluye otros archivos de configuración               |

---

## 6. Server blocks (Virtual Hosts)

En Nginx, el equivalente a un VirtualHost de Apache es un **server block**.

### 6.1 Estructura básica

```nginx
server {
    listen 80;
    server_name ejemplo.com www.ejemplo.com;

    root /var/www/ejemplo/public;
    index index.html index.htm;

    access_log /var/log/nginx/ejemplo-access.log;
    error_log /var/log/nginx/ejemplo-error.log;

    location / {
        try_files $uri $uri/ =404;
    }
}
```

### 6.2 Directiva listen

```nginx
listen 80;                       # IPv4, puerto 80
listen [::]:80;                  # IPv6, puerto 80
listen 443 ssl;                  # HTTPS
listen 80 default_server;        # server block por defecto
listen 127.0.0.1:8080;           # solo localhost
listen unix:/var/run/nginx.sock; # socket Unix
```

### 6.3 Directiva server_name

Determina qué server block procesa la petición según la cabecera `Host`:

```nginx
# Nombre exacto
server_name ejemplo.com;

# Múltiples nombres
server_name ejemplo.com www.ejemplo.com;

# Wildcard al inicio
server_name *.ejemplo.com;

# Wildcard al final
server_name ejemplo.*;

# Expresión regular (prefijo ~)
server_name ~^(?<subdomain>.+)\.ejemplo\.com$;
# $subdomain queda disponible como variable

# Catch-all (cadena vacía)
server_name "";

# Nombre especial: sin Host header
server_name _;     # convención para "no coincide con nada"
```

### 6.4 Selección del server block

Nginx selecciona el server block en este orden:

```
1. Coincidencia exacta de server_name
   │  server_name ejemplo.com;
   │
2. Wildcard más largo al inicio
   │  server_name *.ejemplo.com;
   │
3. Wildcard más largo al final
   │  server_name ejemplo.*;
   │
4. Primera regex que coincida (orden de aparición)
   │  server_name ~^(.+)\.ejemplo\.com$;
   │
5. default_server para ese listen
   │  listen 80 default_server;
   │
6. Primer server block definido (si no hay default_server)
```

### 6.5 default_server

```nginx
# Catch-all: rechazar peticiones sin Host válido
server {
    listen 80 default_server;
    server_name _;
    return 444;              # código especial Nginx: cierra conexión
}

# O redirigir al sitio principal
server {
    listen 80 default_server;
    server_name _;
    return 301 https://ejemplo.com$request_uri;
}
```

> **Código 444**: Es específico de Nginx (no es un código HTTP estándar). Cierra
> la conexión TCP sin enviar respuesta. Útil para descartar escaneos y bots.

### 6.6 Gestión de server blocks

**Debian/Ubuntu:**

```bash
# Crear el archivo
vim /etc/nginx/sites-available/ejemplo.conf

# Habilitar (crear symlink)
ln -s /etc/nginx/sites-available/ejemplo.conf \
      /etc/nginx/sites-enabled/ejemplo.conf

# Deshabilitar (eliminar symlink)
rm /etc/nginx/sites-enabled/ejemplo.conf

# Verificar y recargar
nginx -t && systemctl reload nginx
```

**RHEL/Fedora:**

```bash
# Crear directamente en conf.d/
vim /etc/nginx/conf.d/ejemplo.conf

# Deshabilitar: renombrar
mv /etc/nginx/conf.d/ejemplo.conf /etc/nginx/conf.d/ejemplo.conf.disabled

# Verificar y recargar
nginx -t && systemctl reload nginx
```

---

## 7. Location blocks — enrutamiento de URLs

Los `location` blocks son el mecanismo central de enrutamiento en Nginx.
Determinan cómo se procesan las peticiones según la URI solicitada.

### 7.1 Tipos de coincidencia

```nginx
# 1. Prefijo exacto (prioridad máxima)
location = /favicon.ico {
    # Solo coincide con /favicon.ico exactamente
}

# 2. Prefijo preferente (detiene búsqueda de regex)
location ^~ /images/ {
    # Coincide con URIs que empiezan con /images/
    # Si coincide, NO se evalúan regex posteriores
}

# 3. Expresión regular case-sensitive
location ~ \.php$ {
    # Coincide con URIs que terminan en .php
}

# 4. Expresión regular case-insensitive
location ~* \.(jpg|jpeg|png|gif|webp)$ {
    # Coincide con extensiones de imagen (mayúsculas o minúsculas)
}

# 5. Prefijo normal (prioridad base)
location /api/ {
    # Coincide con URIs que empiezan con /api/
}

# 6. Prefijo raíz (catch-all)
location / {
    # Coincide con todo (todas las URIs empiezan con /)
}
```

### 7.2 Algoritmo de selección

Nginx evalúa las locations en un orden preciso, no en el orden de aparición:

```
URI solicitada: /images/photo.jpg

Paso 1: Buscar coincidencia exacta (=)
  ├── = /images/photo.jpg  → ¿existe? Si sí, USAR. Si no, continuar.
  │
Paso 2: Buscar todos los prefijos que coincidan
  ├── /                    → coincide (longitud 1)
  ├── /images/             → coincide (longitud 8) ← más largo
  ├── /api/                → no coincide
  │
  └── ¿El prefijo más largo tiene ^~ ?
      ├── SÍ → USAR ese location, NO buscar regex
      └── NO → recordar este prefijo, continuar con regex
  │
Paso 3: Evaluar regex en orden de aparición
  ├── ~ \.php$             → no coincide
  ├── ~* \.(jpg|jpeg)$     → COINCIDE → USAR este location
  │
Paso 4: Si ninguna regex coincide → usar el prefijo recordado
```

Resumido como diagrama:

```
                        ┌─ = exacto ──────────► USAR (fin)
                        │
  URI ──► Evaluar ──────┤
                        │                 ┌─ SÍ ──► USAR ^~ (fin)
                        ├─ Prefijos ──────┤
                        │  (más largo)    └─ NO ──► Recordar
                        │
                        │                 ┌─ SÍ ──► USAR regex (fin)
                        └─ Regex ─────────┤
                           (orden)        └─ NO ──► USAR prefijo recordado
```

### 7.3 Ejemplos prácticos de prioridad

```nginx
server {
    # A: catch-all
    location / {
        return 200 "A: catch-all\n";
    }

    # B: prefijo /images/
    location /images/ {
        return 200 "B: prefijo /images/\n";
    }

    # C: prefijo preferente /images/
    location ^~ /images/icons/ {
        return 200 "C: ^~ /images/icons/\n";
    }

    # D: regex imágenes
    location ~* \.(jpg|png|gif)$ {
        return 200 "D: regex imagen\n";
    }

    # E: exacto
    location = /images/logo.png {
        return 200 "E: exacto logo\n";
    }
}
```

| URI solicitada           | Location que responde | Razón                          |
|--------------------------|----------------------|--------------------------------|
| `/`                      | A                    | Único prefijo que coincide     |
| `/about`                 | A                    | Solo `/` coincide como prefijo |
| `/images/photo.jpg`      | D                    | Regex `.jpg$` gana sobre prefijo|
| `/images/icons/menu.png` | C                    | `^~` detiene búsqueda de regex |
| `/images/logo.png`       | E                    | Coincidencia exacta = prioridad máxima|
| `/images/data.json`      | B                    | Sin regex que coincida, usa prefijo|

### 7.4 try_files — el patrón más importante

`try_files` intenta servir archivos en orden, con un fallback final:

```nginx
location / {
    try_files $uri $uri/ =404;
}
# 1. ¿Existe el archivo $uri? → servirlo
# 2. ¿Existe el directorio $uri/? → servir su index
# 3. Devolver 404
```

**Front controller (PHP frameworks):**

```nginx
location / {
    try_files $uri $uri/ /index.php$is_args$args;
}
# 1. ¿Archivo existe? → servirlo (CSS, JS, imágenes)
# 2. ¿Directorio existe? → su index
# 3. Pasar a /index.php con query string original
```

**Aplicación SPA (Single Page Application):**

```nginx
location / {
    try_files $uri /index.html;
}
# Si el archivo no existe, siempre servir index.html
# (el enrutamiento lo hace JavaScript en el cliente)
```

### 7.5 Locations anidados

```nginx
location /api/ {
    # Configuración general para /api/

    location /api/public/ {
        # Sin autenticación
    }

    location /api/admin/ {
        # Con autenticación
        auth_basic "Admin API";
        auth_basic_user_file /etc/nginx/.htpasswd;
    }
}
```

---

## 8. Herencia y fusión de directivas

### 8.1 Tipos de directivas según herencia

Nginx tiene dos tipos de directivas en relación a la herencia:

**Directivas simples** (valor único): Se heredan al contexto hijo si no están
redefinidas. Al redefinir, reemplazan completamente:

```nginx
http {
    gzip on;
    root /var/www/default;

    server {
        # hereda gzip on y root /var/www/default

        location /app/ {
            root /var/www/app;     # reemplaza root para esta location
            # hereda gzip on
        }
    }
}
```

**Directivas de array** (add_header, proxy_set_header, etc.): **No se heredan**
si se define alguna en el contexto hijo. Si el hijo define una, pierde todas las
del padre:

```nginx
http {
    add_header X-Frame-Options "SAMEORIGIN";
    add_header X-Content-Type-Options "nosniff";

    server {
        # hereda ambos headers

        location /api/ {
            add_header X-API-Version "2.0";
            # PIERDE X-Frame-Options y X-Content-Type-Options
            # Solo tiene X-API-Version
        }
    }
}
```

### 8.2 La trampa de add_header

Este es uno de los comportamientos más sorprendentes de Nginx:

```nginx
# INCORRECTO — la location pierde los headers de seguridad
server {
    add_header X-Frame-Options "DENY";
    add_header X-Content-Type-Options "nosniff";
    add_header Referrer-Policy "strict-origin";

    location /api/ {
        add_header X-API-Version "1.0";
        # ← solo envía X-API-Version, PIERDE los tres de arriba
    }
}

# CORRECTO — repetir todos los headers
server {
    add_header X-Frame-Options "DENY";
    add_header X-Content-Type-Options "nosniff";
    add_header Referrer-Policy "strict-origin";

    location /api/ {
        add_header X-Frame-Options "DENY";
        add_header X-Content-Type-Options "nosniff";
        add_header Referrer-Policy "strict-origin";
        add_header X-API-Version "1.0";
    }
}

# MEJOR — usar include para no repetir
# /etc/nginx/snippets/security-headers.conf
# add_header X-Frame-Options "DENY";
# add_header X-Content-Type-Options "nosniff";
# add_header Referrer-Policy "strict-origin";

server {
    include snippets/security-headers.conf;

    location /api/ {
        include snippets/security-headers.conf;
        add_header X-API-Version "1.0";
    }
}
```

### 8.3 root vs alias

Dos directivas para mapear URLs a rutas del filesystem, con comportamiento
diferente:

```nginx
# root: CONCATENA la location al path
location /images/ {
    root /var/www/ejemplo;
    # /images/logo.png → /var/www/ejemplo/images/logo.png
    #                     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    #                     root + URI completa
}

# alias: REEMPLAZA la location por el path
location /images/ {
    alias /var/www/ejemplo/static/;
    # /images/logo.png → /var/www/ejemplo/static/logo.png
    #                     ^^^^^^^^^^^^^^^^^^^^^^^^^^^
    #                     alias + parte DESPUÉS de /images/
}
```

> **Nota sobre trailing slash en alias**: Si la location termina en `/`, el alias
> también debe terminar en `/`. La omisión causa errores sutiles.

---

## 9. Gestión del servicio

### 9.1 Comandos de control

```bash
# Verificar sintaxis (SIEMPRE antes de reload)
nginx -t
# nginx: the configuration file /etc/nginx/nginx.conf syntax is ok
# nginx: configuration file /etc/nginx/nginx.conf test is successful

# Verificar con dump de configuración efectiva
nginx -T                     # -T = test + print config completa

# Recargar configuración (graceful — sin cortar conexiones)
systemctl reload nginx
# o directamente:
nginx -s reload

# Señales disponibles
nginx -s stop                # terminar inmediatamente (SIGTERM)
nginx -s quit                # terminar tras completar peticiones (SIGQUIT)
nginx -s reload              # recargar configuración (SIGHUP)
nginx -s reopen              # reabrir archivos de log (SIGUSR1)
```

### 9.2 Proceso de reload

El reload de Nginx es especialmente elegante:

```
1. Master recibe SIGHUP (reload)
2. Master valida la nueva configuración
   ├── Si es inválida: mantiene la anterior, log error
   └── Si es válida: continua
3. Master crea nuevos worker processes con la nueva config
4. Master señala a los workers antiguos que dejen de aceptar conexiones
5. Workers antiguos terminan las peticiones en curso
6. Workers antiguos se terminan
7. Solo quedan los nuevos workers

Resultado: zero downtime, sin conexiones interrumpidas
```

### 9.3 Verificar configuración efectiva

```bash
# Mostrar configuración completa (tras resolver includes)
nginx -T

# Mostrar solo server blocks con sus server_names
nginx -T 2>/dev/null | grep -E "server_name|listen" | head -20

# Ver opciones de compilación
nginx -V 2>&1 | tr ' ' '\n' | grep --
# --with-http_ssl_module
# --with-http_v2_module
# --with-http_gzip_static_module
# ...
```

---

## 10. Errores comunes

### Error 1: "conflicting server name" warning

```
nginx: [warn] conflicting server name "ejemplo.com" on 0.0.0.0:80,
ignored: /etc/nginx/sites-enabled/ejemplo.conf
```

**Causa**: Dos server blocks tienen el mismo `server_name` para el mismo
`listen`. Suele pasar cuando el sitio default y el nuevo comparten nombre, o
cuando se tiene un symlink duplicado.

**Solución**: Verificar con `nginx -T | grep server_name` y eliminar duplicados.
Eliminar el server block default si no se necesita:

```bash
rm /etc/nginx/sites-enabled/default
```

### Error 2: 403 Forbidden al acceder al sitio

**Causas ordenadas por frecuencia**:

1. **Sin archivo index**: El directorio existe pero no tiene archivo index
   y `autoindex` está desactivado.
2. **Permisos**: Nginx (usuario `nginx`/`www-data`) no puede leer los archivos.
3. **SELinux** (RHEL): Contexto incorrecto en archivos del sitio.

```bash
# Verificar permisos en toda la cadena
namei -l /var/www/ejemplo/public/index.html

# SELinux
ls -Z /var/www/ejemplo/
# Si no es httpd_sys_content_t:
semanage fcontext -a -t httpd_sys_content_t "/var/www/ejemplo(/.*)?"
restorecon -Rv /var/www/ejemplo
```

### Error 3: "upstream timed out" o 502 Bad Gateway

**Causa**: El backend (PHP-FPM, Node.js, etc.) no responde a tiempo o no está
corriendo.

```bash
# ¿El backend corre?
systemctl status php-fpm
ss -tlnp | grep 9000    # o el socket correspondiente

# Verificar socket Unix
ls -la /run/php/php-fpm.sock
```

### Error 4: Cambios no surten efecto tras editar configuración

**Causa**: No se ejecutó `reload` o el reload falló silenciosamente.

```bash
# Verificar que la config es válida
nginx -t

# Verificar que reload se completó
systemctl status nginx
journalctl -u nginx --since "1 min ago"
```

### Error 5: location regex sobreescribe prefijo inesperadamente

```nginx
location /images/ {
    root /var/www/static;
}

location ~* \.(jpg|png)$ {
    root /var/www/media;      # ← captura /images/photo.jpg
}
```

**Causa**: Las regex tienen prioridad sobre los prefijos normales. Solo `= ` y
`^~` tienen mayor prioridad que las regex.

**Solución**: Usar `^~` si el prefijo debe ganar:

```nginx
location ^~ /images/ {
    root /var/www/static;     # ahora NO será sobreescrito por regex
}
```

---

## 11. Cheatsheet

```
┌─────────────────────────────────────────────────────────────────┐
│                   NGINX — REFERENCIA RÁPIDA                     │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  INSTALACIÓN:                                                   │
│  Debian: apt install nginx    RHEL: dnf install nginx           │
│                                                                 │
│  SERVICIO:                                                      │
│  nginx -t                    validar sintaxis                   │
│  nginx -T                    validar + dump config completa     │
│  systemctl reload nginx      recargar (zero downtime)           │
│  nginx -s stop|quit|reload|reopen   señales directas            │
│                                                                 │
│  ESTRUCTURA DE ARCHIVOS:                                        │
│  Debian: /etc/nginx/sites-available/ + sites-enabled/           │
│  RHEL:   /etc/nginx/conf.d/*.conf                               │
│  Ambos:  /etc/nginx/nginx.conf (principal)                      │
│                                                                 │
│  JERARQUÍA DE CONTEXTOS:                                        │
│  main → events { }                                              │
│       → http { } → server { } → location { }                   │
│                                                                 │
│  SERVER BLOCK MÍNIMO:                                           │
│  server {                                                       │
│      listen 80;                                                 │
│      server_name ejemplo.com;                                   │
│      root /var/www/ejemplo;                                     │
│      location / { try_files $uri $uri/ =404; }                  │
│  }                                                              │
│                                                                 │
│  PRIORIDAD DE LOCATION:                                         │
│  1.  = /exacto           coincidencia exacta (máxima)           │
│  2. ^~ /prefijo          prefijo preferente (detiene regex)     │
│  3.  ~ regex             regex case-sensitive                   │
│  4. ~* regex             regex case-insensitive                 │
│  5.    /prefijo          prefijo normal                         │
│  6.    /                 catch-all                              │
│                                                                 │
│  SELECCIÓN DE SERVER_NAME:                                      │
│  1. Nombre exacto                                               │
│  2. Wildcard inicio (*.ejemplo.com)                             │
│  3. Wildcard final (ejemplo.*)                                  │
│  4. Primera regex (~^...)                                       │
│  5. default_server                                              │
│                                                                 │
│  TRY_FILES:                                                     │
│  try_files $uri $uri/ =404;           estáticos con 404         │
│  try_files $uri $uri/ /index.php...;  front controller PHP      │
│  try_files $uri /index.html;          SPA                       │
│                                                                 │
│  ROOT VS ALIAS:                                                 │
│  root /path;    → /path + URI completa                          │
│  alias /path/;  → /path + parte después de location             │
│                                                                 │
│  HERENCIA:                                                      │
│  Directivas simples: se heredan, redefinir reemplaza            │
│  Directivas array (add_header): NO se heredan si se             │
│    define alguna en el hijo → usar include snippets             │
│                                                                 │
│  WORKERS:                                                       │
│  worker_processes auto;        1 por CPU core                   │
│  worker_connections 1024;      conex. por worker                │
│  max = workers × connections                                    │
│                                                                 │
│  RETURN ESPECIAL:                                               │
│  return 444;    cierra conexión sin respuesta (anti-bot)         │
│  return 301 URL;  redirección permanente                        │
│                                                                 │
│  LOGS:                                                          │
│  /var/log/nginx/access.log   /var/log/nginx/error.log           │
│  Personalizar: access_log /path formato;                        │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: Instalación y server blocks básicos

1. Instala Nginx y verifica que responde en `http://localhost`.
2. Comprueba cuántos worker processes se crearon y correlaciona con el número
   de CPUs (`nproc`).
3. Crea dos server blocks: `sitio-a.local` y `sitio-b.local`, cada uno con su
   propio `root` y un `index.html` identificable.
4. Añade entradas en `/etc/hosts` para ambos dominios.
5. Habilita los sitios (symlink en Debian o `.conf` en RHEL).
6. Verifica la sintaxis con `nginx -t` y recarga.
7. Prueba con `curl -H "Host: sitio-a.local" http://localhost/` y con
   `sitio-b.local`.
8. Accede con un Host desconocido. ¿Cuál de los dos sitios responde? ¿Por qué?

**Pregunta de predicción**: Si añades `listen 80 default_server;` al server block
de `sitio-b.local` y el default actual también tiene `default_server`, ¿qué
sucederá al hacer `nginx -t`?

### Ejercicio 2: Location blocks y prioridad

Crea un server block con estas locations y predice qué responderá cada URI
**antes** de probar:

```nginx
server {
    listen 80;
    server_name loc-test.local;
    root /var/www/loc-test;

    location / {
        return 200 "CATCH-ALL\n";
    }

    location /images/ {
        return 200 "PREFIX /images/\n";
    }

    location ^~ /images/icons/ {
        return 200 "PREFERENT /images/icons/\n";
    }

    location ~ \.png$ {
        return 200 "REGEX .png\n";
    }

    location = /test.html {
        return 200 "EXACT /test.html\n";
    }
}
```

Predice y luego verifica con `curl`:

| URI                        | Tu predicción | Resultado real |
|----------------------------|---------------|----------------|
| `/`                        |               |                |
| `/about`                   |               |                |
| `/images/photo.png`        |               |                |
| `/images/icons/menu.png`   |               |                |
| `/images/data.json`        |               |                |
| `/test.html`               |               |                |
| `/docs/test.html`          |               |                |
| `/styles/main.png`         |               |                |

**Pregunta de predicción**: Si eliminas el `^~` de `/images/icons/` y dejas
solo `location /images/icons/`, ¿qué responderá `/images/icons/menu.png`?
¿Cambiará la respuesta? ¿Por qué?

### Ejercicio 3: Herencia de directivas y try_files

1. Configura un server block con `add_header X-Global "yes"` en el contexto
   `server`.
2. Crea una `location /api/` que añada `add_header X-API "yes"`.
3. Accede a `/api/` con `curl -v` y examina los headers de respuesta.
   ¿Aparece `X-Global`? ¿Por qué?
4. Corrige el problema usando un snippet incluido en ambos contextos.
5. Ahora configura `try_files` para que:
   - Los archivos estáticos se sirvan directamente si existen.
   - Las rutas que no existen se pasen a `/index.html` (patrón SPA).
6. Crea `/var/www/test/index.html` y `/var/www/test/css/style.css`.
7. Prueba: `curl http://test.local/css/style.css` (debe servir el CSS) y
   `curl http://test.local/ruta/inexistente` (debe servir index.html).

**Pregunta de reflexión**: El comportamiento de `add_header` que no se hereda
cuando el hijo define alguno es contraintuitivo. ¿Por qué crees que Nginx lo
diseñó así en vez de acumular headers como haría Apache? Piensa en qué pasaría
si una location necesitara explícitamente **no** tener un header del padre.

---

> **Siguiente tema**: T02 — Proxy inverso (proxy_pass, headers, WebSocket
> proxying).
