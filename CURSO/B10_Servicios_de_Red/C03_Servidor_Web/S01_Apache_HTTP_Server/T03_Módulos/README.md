# Módulos de Apache

## Índice

1. [Arquitectura modular de Apache](#1-arquitectura-modular-de-apache)
2. [Gestión de módulos](#2-gestión-de-módulos)
3. [mod_rewrite — reescritura de URLs](#3-mod_rewrite--reescritura-de-urls)
4. [mod_proxy — proxy y proxy inverso](#4-mod_proxy--proxy-y-proxy-inverso)
5. [mod_ssl — HTTPS y TLS](#5-mod_ssl--https-y-tls)
6. [Otros módulos esenciales](#6-otros-módulos-esenciales)
7. [Orden de procesamiento de módulos](#7-orden-de-procesamiento-de-módulos)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. Arquitectura modular de Apache

Apache está diseñado para que casi toda su funcionalidad, más allá del core
mínimo, se implemente como módulos cargables. Esto permite activar solo lo
necesario, reduciendo uso de memoria y superficie de ataque.

```
┌──────────────────────────────────────────────────┐
│                  Apache httpd                    │
│                                                  │
│  ┌────────────────────────────────────────────┐  │
│  │              Core (httpd_core)             │  │
│  │  Parsing HTTP, ciclo de vida de requests,  │  │
│  │  contenedores <Directory>/<Location>,      │  │
│  │  configuración básica                      │  │
│  └──────────────────┬─────────────────────────┘  │
│                     │                            │
│       ┌─────────────┼─────────────┐              │
│       │             │             │              │
│  ┌────▼───┐   ┌─────▼────┐  ┌────▼────┐         │
│  │ Módulos│   │ Módulos  │  │ Módulos │         │
│  │ estát. │   │ dinámicos│  │ MPM     │         │
│  │(compil)│   │  (.so)   │  │         │         │
│  └────────┘   └──────────┘  └─────────┘         │
│                                                  │
│  Estáticos: compilados en el binario             │
│  Dinámicos: cargados con LoadModule              │
│  MPM: exactamente uno activo (event/worker/...)  │
└──────────────────────────────────────────────────┘
```

### Módulos estáticos vs dinámicos

| Tipo      | Carga                      | Ventaja                 | Inconveniente           |
|-----------|----------------------------|-------------------------|-------------------------|
| Estático  | Compilado en el binario    | Sin overhead de carga   | Requiere recompilar     |
| Dinámico  | `LoadModule` en runtime    | Activar/desactivar fácil| Ligero overhead al inicio|

En las distribuciones modernas, prácticamente todos los módulos se distribuyen
como dinámicos (archivos `.so`). Los módulos estáticos son relevantes solo cuando
se compila Apache desde fuente.

```bash
# Ver qué módulos son estáticos (compilados) vs dinámicos (shared)
apachectl -M
# Salida:
# core_module (static)
# so_module (static)           ← necesario para cargar .so
# rewrite_module (shared)      ← dinámico
# ssl_module (shared)          ← dinámico
```

---

## 2. Gestión de módulos

### 2.1 Debian/Ubuntu — a2enmod / a2dismod

Cada módulo tiene dos archivos en `/etc/apache2/mods-available/`:

```
mods-available/
├── rewrite.load          ← directiva LoadModule
├── ssl.load              ← directiva LoadModule
├── ssl.conf              ← configuración del módulo
├── proxy.load
├── proxy.conf
└── ...
```

El archivo `.load` contiene la directiva `LoadModule`:

```apache
# /etc/apache2/mods-available/rewrite.load
LoadModule rewrite_module /usr/lib/apache2/modules/mod_rewrite.so
```

El archivo `.conf` (opcional) contiene configuración por defecto del módulo.

```bash
# Habilitar módulo (crea symlink en mods-enabled/)
a2enmod rewrite
a2enmod ssl
a2enmod proxy proxy_http    # varios a la vez

# Deshabilitar módulo
a2dismod rewrite

# Ver módulos habilitados
ls /etc/apache2/mods-enabled/

# Aplicar cambios (módulos requieren restart, no solo reload)
systemctl restart apache2
```

> **Restart obligatorio**: A diferencia de cambios en VirtualHosts (que aceptan
> reload), cargar o descargar módulos **requiere restart** porque modifica la
> estructura interna del proceso.

### 2.2 RHEL/Fedora — configuración directa

Los módulos se cargan desde `/etc/httpd/conf.modules.d/`:

```
conf.modules.d/
├── 00-base.conf          ← módulos fundamentales
├── 00-mpm.conf           ← selección de MPM
├── 00-proxy.conf         ← módulos de proxy
├── 00-ssl.conf           ← mod_ssl
├── 01-cgi.conf           ← CGI
└── 00-optional.conf      ← módulos opcionales (comentados)
```

```bash
# Ver contenido de un archivo de módulos
cat /etc/httpd/conf.modules.d/00-base.conf
# LoadModule authn_core_module modules/mod_authn_core.so
# LoadModule authz_core_module modules/mod_authz_core.so
# ...

# Habilitar: descomentar la línea LoadModule
# Deshabilitar: comentar con #
# Reiniciar tras cambios
systemctl restart httpd
```

### 2.3 Instalar módulos adicionales

Algunos módulos no vienen con el paquete base:

```bash
# Debian — buscar módulos disponibles
apt search libapache2-mod
# Instalar
apt install libapache2-mod-security2    # ModSecurity WAF
apt install libapache2-mod-fcgid        # FastCGI

# RHEL — buscar módulos
dnf search mod_
# Instalar
dnf install mod_ssl                     # TLS
dnf install mod_security                # WAF
```

### 2.4 Verificar si un módulo está activo

```bash
# Listar todos los módulos cargados
apachectl -M

# Buscar uno específico
apachectl -M | grep rewrite
# rewrite_module (shared)

# Desde la configuración (condicional)
<IfModule mod_rewrite.c>
    # Solo se aplica si mod_rewrite está cargado
    RewriteEngine On
</IfModule>
```

`<IfModule>` es útil para configuraciones portables, pero **no debe usarse como
sustituto** de verificar que el módulo necesario esté cargado. Si una directiva
crítica queda dentro de un `<IfModule>` y el módulo no está cargado, el sitio
funcionará sin esa directiva silenciosamente — sin error ni advertencia.

---

## 3. mod_rewrite — reescritura de URLs

mod_rewrite permite transformar URLs internamente (sin que el cliente lo vea) o
externamente (redirigiendo al cliente). Es uno de los módulos más potentes y
complejos de Apache.

### 3.1 Conceptos fundamentales

```
Petición: GET /producto/zapatos HTTP/1.1

                    ┌─────────────────┐
                    │  RewriteEngine  │
   URL original     │      On         │
   /producto/zap.   │                 │    URL reescrita
  ──────────────────► RewriteCond     ├──► /catalogo.php?cat=zapatos
                    │ RewriteRule     │    (interna, cliente no ve)
                    │                 │
                    │       o bien    │    Redirección externa
                    │ [R=301]         ├──► Location: /nueva-url
                    └─────────────────┘    (cliente recibe 301)
```

### 3.2 Habilitar mod_rewrite

```bash
# Debian
a2enmod rewrite
systemctl restart apache2

# RHEL — generalmente ya incluido en 00-base.conf
apachectl -M | grep rewrite
```

### 3.3 RewriteRule — sintaxis

```apache
RewriteEngine On
RewriteRule PATRÓN SUSTITUCIÓN [FLAGS]
```

- **PATRÓN**: Expresión regular que se aplica sobre la ruta URL (sin el dominio
  ni query string)
- **SUSTITUCIÓN**: URL resultante. Puede incluir backreferences `$1`, `$2`...
- **FLAGS**: Modificadores entre corchetes

**Flags más usados:**

| Flag     | Nombre              | Efecto                                      |
|----------|---------------------|---------------------------------------------|
| `[L]`    | Last                | Detiene el procesamiento de reglas           |
| `[R=301]`| Redirect            | Redirección externa (301, 302, etc.)         |
| `[NC]`   | No Case             | Coincidencia sin distinguir mayúsculas       |
| `[QSA]`  | Query String Append | Añade query string original a la sustitución |
| `[PT]`   | Pass Through        | Pasa el resultado a otros módulos (Alias)    |
| `[F]`    | Forbidden           | Devuelve 403                                 |
| `[G]`    | Gone                | Devuelve 410                                 |
| `[P]`    | Proxy               | Reescritura como proxy (requiere mod_proxy)  |
| `[END]`  | End                 | Como [L] pero también detiene rondas .htaccess|

### 3.4 RewriteCond — condiciones

Las condiciones se evalúan antes de la regla que les sigue:

```apache
RewriteEngine On

# Redirigir HTTP a HTTPS
RewriteCond %{HTTPS} off
RewriteRule ^(.*)$ https://%{HTTP_HOST}$1 [R=301,L]

# Redirigir www a no-www
RewriteCond %{HTTP_HOST} ^www\.(.+)$ [NC]
RewriteRule ^(.*)$ https://%1$1 [R=301,L]
```

**Variables de servidor disponibles en RewriteCond:**

| Variable            | Contenido                           |
|---------------------|-------------------------------------|
| `%{HTTP_HOST}`      | Cabecera Host                       |
| `%{REQUEST_URI}`    | URI completa con path               |
| `%{QUERY_STRING}`   | Parámetros tras `?`                 |
| `%{HTTPS}`          | `on` u `off`                        |
| `%{REQUEST_METHOD}` | GET, POST, etc.                     |
| `%{REMOTE_ADDR}`    | IP del cliente                      |
| `%{REQUEST_FILENAME}` | Ruta del archivo en filesystem    |

**Operadores de test en RewriteCond:**

```apache
# Test de archivo
RewriteCond %{REQUEST_FILENAME} !-f    # no es un archivo existente
RewriteCond %{REQUEST_FILENAME} !-d    # no es un directorio existente

# Combinación: si no existe como archivo ni directorio, reescribir
RewriteCond %{REQUEST_FILENAME} !-f
RewriteCond %{REQUEST_FILENAME} !-d
RewriteRule ^(.*)$ /index.php?path=$1 [QSA,L]
```

### 3.5 Patrones frecuentes

**URLs limpias (front controller):**

```apache
# /producto/42/zapatos → /index.php?path=producto/42/zapatos
RewriteEngine On
RewriteCond %{REQUEST_FILENAME} !-f
RewriteCond %{REQUEST_FILENAME} !-d
RewriteRule ^(.*)$ /index.php [QSA,L]
```

**Forzar trailing slash en directorios:**

```apache
RewriteCond %{REQUEST_FILENAME} -d
RewriteCond %{REQUEST_URI} !/$
RewriteRule ^(.*)$ $1/ [R=301,L]
```

**Bloquear acceso a archivos sensibles:**

```apache
RewriteRule ^\.git(/.*)?$ - [F,L]
RewriteRule ^\.env$ - [F,L]
RewriteRule ^composer\.(json|lock)$ - [F,L]
```

### 3.6 Depuración de mod_rewrite

```apache
# En el VirtualHost o config global (NO en .htaccess)
LogLevel warn rewrite:trace3

# Niveles:
# trace1: solo reglas que coinciden
# trace2: condiciones y reglas
# trace3: procesamiento detallado paso a paso
# trace8: máximo detalle (enorme volumen)
```

Los mensajes aparecen en el `ErrorLog`:

```
[rewrite:trace3] mod_rewrite.c(477): [client 192.168.1.50:43210]
  applying pattern '^(.*)$' to uri 'producto/zapatos'
[rewrite:trace3] mod_rewrite.c(477): [client 192.168.1.50:43210]
  rewrite 'producto/zapatos' -> '/index.php'
```

---

## 4. mod_proxy — proxy y proxy inverso

mod_proxy convierte Apache en un proxy, reenviando peticiones a otros servidores
o servicios backend. Es la forma estándar de conectar Apache con aplicaciones
(Node.js, Python, Java, PHP-FPM).

### 4.1 Conceptos: forward vs reverse proxy

```
Forward proxy (proxy directo)          Reverse proxy (proxy inverso)
─────────────────────────────          ───────────────────────────────
Cliente → Proxy → Internet             Internet → Proxy → Backend

El cliente sabe que usa proxy             El cliente no sabe que hay proxy
Se configura en el navegador              Se configura en el servidor
Caso: acceso controlado a internet        Caso: servir aplicaciones backend

┌────────┐     ┌───────┐    ┌────────┐    ┌────────┐    ┌───────┐    ┌─────────┐
│Cliente │────►│ Proxy │───►│Internet│    │Cliente │───►│Apache │───►│ App     │
│        │     │forward│    │        │    │        │    │reverse│    │ :3000   │
└────────┘     └───────┘    └────────┘    └────────┘    └───────┘    └─────────┘
```

Apache como reverse proxy es el caso más frecuente: Apache recibe la petición
del cliente en el puerto 80/443 y la reenvía a un servicio backend que escucha
en un puerto interno.

### 4.2 Módulos de proxy

mod_proxy es el módulo base, pero necesita submódulos según el protocolo:

| Módulo             | Protocolo           | Uso                                |
|--------------------|---------------------|------------------------------------|
| `mod_proxy`        | —                   | Core del proxy (siempre necesario) |
| `mod_proxy_http`   | HTTP/HTTPS          | Proxy a backends HTTP              |
| `mod_proxy_fcgi`   | FastCGI             | PHP-FPM, aplicaciones FastCGI      |
| `mod_proxy_wstunnel`| WebSocket          | Proxying de WebSocket              |
| `mod_proxy_ajp`    | AJP                 | Apache Tomcat                      |
| `mod_proxy_balancer`| —                  | Balanceo de carga                  |

```bash
# Debian — habilitar proxy HTTP
a2enmod proxy proxy_http
systemctl restart apache2

# RHEL — ya incluido en conf.modules.d/00-proxy.conf
# Verificar que no esté comentado
```

### 4.3 ProxyPass y ProxyPassReverse

Las dos directivas fundamentales del reverse proxy:

```apache
<VirtualHost *:80>
    ServerName app.ejemplo.com

    # Toda petición a / se reenvía al backend en puerto 3000
    ProxyPass "/" "http://127.0.0.1:3000/"
    ProxyPassReverse "/" "http://127.0.0.1:3000/"
</VirtualHost>
```

- **ProxyPass**: Reenvía la petición del cliente al backend
- **ProxyPassReverse**: Reescribe las cabeceras de respuesta (`Location`,
  `Content-Location`, `URI`) para que el cliente vea URLs del proxy, no del
  backend

```
Sin ProxyPassReverse:
  Backend responde: Location: http://127.0.0.1:3000/login
  Cliente recibe:   Location: http://127.0.0.1:3000/login  ← ROTO

Con ProxyPassReverse:
  Backend responde: Location: http://127.0.0.1:3000/login
  Cliente recibe:   Location: http://app.ejemplo.com/login  ← CORRECTO
```

### 4.4 Proxy por ruta (subpath)

```apache
<VirtualHost *:80>
    ServerName ejemplo.com
    DocumentRoot "/var/www/ejemplo"

    # /api/* va al backend Node.js
    ProxyPass "/api/" "http://127.0.0.1:3000/api/"
    ProxyPassReverse "/api/" "http://127.0.0.1:3000/api/"

    # /app/* va al backend Python
    ProxyPass "/app/" "http://127.0.0.1:8000/"
    ProxyPassReverse "/app/" "http://127.0.0.1:8000/"

    # El resto lo sirve Apache directamente desde DocumentRoot
</VirtualHost>
```

> **Orden de ProxyPass**: Las reglas se evalúan de la más específica a la más
> general. Si usas `<Location>` en vez de ProxyPass directo, el orden es
> diferente (orden de aparición). Para evitar confusiones, listar las rutas
> más largas primero:
>
> ```apache
> ProxyPass "/api/v2/" "http://backend-v2:3000/"
> ProxyPass "/api/"    "http://backend-v1:3000/"
> ProxyPass "/"        "http://frontend:8080/"
> ```

### 4.5 Proxy con Location (alternativa)

```apache
<Location "/api/">
    ProxyPass "http://127.0.0.1:3000/"
    ProxyPassReverse "http://127.0.0.1:3000/"

    # Controlar acceso al proxy
    Require ip 10.0.0.0/8
</Location>
```

### 4.6 Cabeceras de proxy

El backend necesita saber la IP real del cliente y otros datos:

```apache
<VirtualHost *:80>
    ServerName app.ejemplo.com

    # Pasar información del cliente original
    ProxyPreserveHost On
    RequestHeader set X-Forwarded-Proto "http"
    RequestHeader set X-Real-IP "%{REMOTE_ADDR}s"

    ProxyPass "/" "http://127.0.0.1:3000/"
    ProxyPassReverse "/" "http://127.0.0.1:3000/"
</VirtualHost>
```

| Cabecera              | Contenido                              |
|-----------------------|----------------------------------------|
| `X-Forwarded-For`     | IP del cliente (Apache lo añade automát.)|
| `X-Forwarded-Proto`   | Protocolo original (http/https)        |
| `X-Forwarded-Host`    | Host original                          |
| `X-Real-IP`           | IP del cliente (formato alternativo)   |
| `ProxyPreserveHost On`| Envía al backend el Host del cliente   |

### 4.7 Proxy a PHP-FPM (FastCGI)

La arquitectura moderna para PHP separa Apache del procesamiento PHP:

```
┌─────────┐     ┌──────────────┐     ┌────────────┐
│ Cliente  │────►│    Apache    │────►│  PHP-FPM   │
│          │     │ mod_proxy_   │     │ (socket o  │
│          │     │   fcgi       │     │  TCP)      │
└─────────┘     └──────────────┘     └────────────┘
```

```apache
# Vía socket Unix (mejor rendimiento, misma máquina)
<FilesMatch "\.php$">
    SetHandler "proxy:unix:/run/php/php-fpm.sock|fcgi://localhost"
</FilesMatch>

# Vía TCP (para PHP-FPM en otra máquina)
<FilesMatch "\.php$">
    SetHandler "proxy:fcgi://192.168.1.20:9000"
</FilesMatch>
```

```bash
# Habilitar módulos necesarios (Debian)
a2enmod proxy_fcgi setenvif
systemctl restart apache2
```

### 4.8 Proxy WebSocket

```apache
# Requiere mod_proxy_wstunnel
RewriteEngine On
RewriteCond %{HTTP:Upgrade} websocket [NC]
RewriteCond %{HTTP:Connection} upgrade [NC]
RewriteRule ^/ws/(.*) "ws://127.0.0.1:3000/ws/$1" [P,L]

ProxyPass "/app/" "http://127.0.0.1:3000/app/"
ProxyPassReverse "/app/" "http://127.0.0.1:3000/app/"
```

### 4.9 Seguridad del proxy

**Evitar open proxy** — un proxy abierto permite a cualquiera usar tu servidor
para acceder a internet:

```apache
# NUNCA en producción:
ProxyRequests On          # ← habilita forward proxy

# SIEMPRE:
ProxyRequests Off         # ← solo reverse proxy (defecto)
```

---

## 5. mod_ssl — HTTPS y TLS

mod_ssl proporciona soporte TLS/SSL a Apache. Se trata en profundidad en la
sección S03 (HTTPS y TLS), pero aquí se cubre la instalación y activación básica.

### 5.1 Instalación y activación

```bash
# Debian
a2enmod ssl
systemctl restart apache2

# RHEL
dnf install mod_ssl
# Se carga automáticamente desde conf.modules.d/00-ssl.conf
systemctl restart httpd
```

### 5.2 Configuración mínima

```apache
Listen 443

<VirtualHost *:443>
    ServerName ejemplo.com
    DocumentRoot "/var/www/ejemplo/public"

    SSLEngine On
    SSLCertificateFile    /etc/ssl/certs/ejemplo.com.crt
    SSLCertificateKeyFile /etc/ssl/private/ejemplo.com.key

    # Cadena de certificación (si aplica)
    # SSLCertificateChainFile /etc/ssl/certs/chain.crt

    <Directory "/var/www/ejemplo/public">
        Require all granted
    </Directory>
</VirtualHost>
```

### 5.3 Certificado autofirmado para laboratorio

```bash
openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
    -keyout /etc/ssl/private/ejemplo.com.key \
    -out /etc/ssl/certs/ejemplo.com.crt \
    -subj "/CN=ejemplo.com"

# Permisos del archivo de clave privada
chmod 600 /etc/ssl/private/ejemplo.com.key
```

### 5.4 Redirección HTTP → HTTPS

Patrón estándar con dos VirtualHosts:

```apache
# VHost HTTP — solo redirige
<VirtualHost *:80>
    ServerName ejemplo.com
    Redirect permanent "/" "https://ejemplo.com/"
</VirtualHost>

# VHost HTTPS — sirve contenido
<VirtualHost *:443>
    ServerName ejemplo.com
    DocumentRoot "/var/www/ejemplo/public"
    SSLEngine On
    SSLCertificateFile    /etc/ssl/certs/ejemplo.com.crt
    SSLCertificateKeyFile /etc/ssl/private/ejemplo.com.key
    ...
</VirtualHost>
```

---

## 6. Otros módulos esenciales

### 6.1 mod_headers — manipulación de cabeceras

```bash
a2enmod headers    # Debian
```

```apache
# Cabeceras de seguridad
Header always set X-Content-Type-Options "nosniff"
Header always set X-Frame-Options "SAMEORIGIN"
Header always set Referrer-Policy "strict-origin-when-cross-origin"
Header always set Content-Security-Policy "default-src 'self'"

# Eliminar cabeceras que revelan información
Header always unset X-Powered-By
ServerTokens Prod              # solo muestra "Apache" sin versión
ServerSignature Off            # sin firma en páginas de error
```

### 6.2 mod_deflate — compresión

```bash
a2enmod deflate    # Debian
```

```apache
# Comprimir tipos de contenido textual
<IfModule mod_deflate.c>
    AddOutputFilterByType DEFLATE text/html text/plain text/css
    AddOutputFilterByType DEFLATE text/javascript application/javascript
    AddOutputFilterByType DEFLATE application/json application/xml
    AddOutputFilterByType DEFLATE image/svg+xml

    # No comprimir archivos ya comprimidos
    SetEnvIfNoCase Request_URI \.(?:gif|jpe?g|png|webp|zip|gz|bz2)$ \
        no-gzip dont-vary
</IfModule>
```

### 6.3 mod_expires — control de caché

```bash
a2enmod expires    # Debian
```

```apache
<IfModule mod_expires.c>
    ExpiresActive On

    # Imágenes: 1 mes
    ExpiresByType image/jpeg "access plus 1 month"
    ExpiresByType image/png "access plus 1 month"
    ExpiresByType image/webp "access plus 1 month"

    # CSS/JS: 1 semana
    ExpiresByType text/css "access plus 1 week"
    ExpiresByType application/javascript "access plus 1 week"

    # HTML: sin caché agresivo
    ExpiresByType text/html "access plus 1 hour"
</IfModule>
```

### 6.4 mod_status — monitorización en tiempo real

```bash
a2enmod status    # Debian (suele estar habilitado por defecto)
```

```apache
<Location "/server-status">
    SetHandler server-status
    Require local
    # O desde una red de monitorización:
    # Require ip 10.0.0.0/8
</Location>

# Activar ExtendedStatus para métricas detalladas
ExtendedStatus On
```

```bash
# Consultar desde CLI
curl http://localhost/server-status?auto

# Métricas disponibles:
# Total Accesses, Total kBytes, Uptime, ReqPerSec,
# BytesPerSec, BusyWorkers, IdleWorkers, Scoreboard
```

### 6.5 mod_dir — DirectoryIndex y autoindex

Siempre cargado. Controla qué archivo servir cuando se solicita un directorio:

```apache
DirectoryIndex index.html index.php default.html

# Orden de búsqueda: primero index.html, luego index.php, etc.
# Si ninguno existe y Options Indexes está activo: listado de directorio
# Si ninguno existe y Options -Indexes: 403 Forbidden
```

### 6.6 mod_alias — Alias y Redirect

```apache
# Alias: mapea URL a ruta de filesystem
Alias "/icons" "/usr/share/apache2/icons"
# /icons/image.png → /usr/share/apache2/icons/image.png

# ScriptAlias: como Alias pero marca directorio como CGI
ScriptAlias "/cgi-bin/" "/usr/lib/cgi-bin/"

# Redirect: redirección HTTP
Redirect permanent "/viejo" "https://ejemplo.com/nuevo"
Redirect 301 "/blog" "https://blog.ejemplo.com/"
```

---

## 7. Orden de procesamiento de módulos

Apache procesa cada petición en fases (hooks). Los módulos se enganchan en las
fases relevantes. El orden importa cuando varios módulos actúan sobre la misma
petición:

```
Petición entrante
       │
       ▼
  ┌─────────────────┐
  │ 1. Post-Read     │  mod_setenvif (variables de entorno)
  └────────┬─────────┘
           │
  ┌────────▼─────────┐
  │ 2. URI Translation│  mod_alias (Alias, Redirect)
  │                   │  mod_rewrite (RewriteRule en server/vhost)
  └────────┬─────────┘
           │
  ┌────────▼─────────┐
  │ 3. Access Control │  mod_authz_core (Require)
  │                   │  mod_auth_basic (autenticación)
  └────────┬─────────┘
           │
  ┌────────▼─────────┐
  │ 4. Content        │  mod_proxy (ProxyPass)
  │    (Response gen.) │  mod_cgi / mod_proxy_fcgi
  │                   │  mod_dir (DirectoryIndex)
  └────────┬─────────┘
           │
  ┌────────▼─────────┐
  │ 5. Fixups/Filters │  mod_deflate (compresión)
  │                   │  mod_headers (cabeceras de respuesta)
  │                   │  mod_expires (caché)
  └────────┬─────────┘
           │
  ┌────────▼─────────┐
  │ 6. Logging        │  mod_log_config (CustomLog)
  └──────────────────┘
```

**Implicación práctica**: mod_rewrite actúa en fase 2 (URI Translation) y puede
transformar la URL antes de que mod_proxy la vea en fase 4. Esto permite usar
RewriteRule con flag `[P]` para proxying condicional.

---

## 8. Errores comunes

### Error 1: "Invalid command 'RewriteEngine'"

```
AH00526: Syntax error: Invalid command 'RewriteEngine',
perhaps misspelled or defined by a module not included
```

**Causa**: mod_rewrite no está cargado.

**Solución**:

```bash
a2enmod rewrite && systemctl restart apache2    # Debian
# RHEL: verificar 00-base.conf
```

### Error 2: ProxyPass devuelve 503 Service Unavailable

**Causa**: El backend no está corriendo o no es alcanzable en la dirección/puerto
configurado.

**Diagnóstico**:

```bash
# ¿El backend escucha?
ss -tlnp | grep 3000
# ¿Es alcanzable desde Apache?
curl -v http://127.0.0.1:3000/
# ¿SELinux bloquea la conexión de red? (RHEL)
setsebool -P httpd_can_network_connect 1
```

El boolean `httpd_can_network_connect` de SELinux es la causa más frecuente en
RHEL. Por defecto, Apache no puede iniciar conexiones de red salientes.

### Error 3: Rewrite loops (redirección infinita)

```
ERR_TOO_MANY_REDIRECTS
```

**Causa**: Una RewriteRule redirige a una URL que coincide de nuevo con la misma
regla.

**Ejemplo problemático**:

```apache
# INCORRECTO — loop infinito
RewriteRule ^(.*)$ /index.php [L]
# /index.php coincide con ^(.*)$ → reescribe a /index.php → repite
```

**Solución**: Añadir condición para excluir el destino:

```apache
RewriteCond %{REQUEST_URI} !^/index\.php$
RewriteRule ^(.*)$ /index.php [QSA,L]
# O más general:
RewriteCond %{REQUEST_FILENAME} !-f
RewriteRule ^(.*)$ /index.php [QSA,L]
```

### Error 4: ProxyPassReverse no reescribe cookies

**Causa**: `ProxyPassReverse` solo reescribe cabeceras `Location` y
`Content-Location`, no el atributo `Domain` de las cookies `Set-Cookie`.

**Solución**: Usar `ProxyPassReverseCookieDomain` y
`ProxyPassReverseCookiePath`:

```apache
ProxyPass "/" "http://backend:8080/"
ProxyPassReverse "/" "http://backend:8080/"
ProxyPassReverseCookieDomain "backend" "ejemplo.com"
ProxyPassReverseCookiePath "/" "/"
```

### Error 5: mod_rewrite no funciona en .htaccess

**Causa**: `AllowOverride` no incluye `FileInfo` (necesario para RewriteRule en
.htaccess).

**Solución**:

```apache
<Directory "/var/www/ejemplo/public">
    AllowOverride FileInfo
    # o AllowOverride All (menos restrictivo)
</Directory>
```

---

## 9. Cheatsheet

```
┌─────────────────────────────────────────────────────────────────┐
│              MÓDULOS APACHE — REFERENCIA RÁPIDA                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  GESTIÓN DE MÓDULOS:                                            │
│  Debian: a2enmod/a2dismod NOMBRE + systemctl restart apache2    │
│  RHEL:   editar conf.modules.d/*.conf + systemctl restart httpd │
│  Ver activos: apachectl -M                                      │
│  Instalar:    apt install libapache2-mod-X / dnf install mod_X  │
│                                                                 │
│  MOD_REWRITE:                                                   │
│  RewriteEngine On                  activar motor                │
│  RewriteCond %{VAR} PATRÓN        condición previa              │
│  RewriteRule ^pat$ dest [FLAGS]    regla de reescritura          │
│  Flags: [L] last  [R=301] redirect  [NC] no-case                │
│         [QSA] query-append  [F] forbidden  [P] proxy            │
│  Debug: LogLevel warn rewrite:trace3                            │
│                                                                 │
│  MOD_PROXY:                                                     │
│  ProxyPass "/ruta/" "http://backend:port/"                      │
│  ProxyPassReverse "/ruta/" "http://backend:port/"               │
│  ProxyPreserveHost On              pasar Host original           │
│  ProxyRequests Off                 SIEMPRE (evitar open proxy)  │
│  SELinux: setsebool -P httpd_can_network_connect 1              │
│                                                                 │
│  PHP-FPM VÍA PROXY_FCGI:                                       │
│  <FilesMatch "\.php$">                                          │
│    SetHandler "proxy:unix:/run/php/php-fpm.sock|fcgi://localhost"│
│  </FilesMatch>                                                  │
│                                                                 │
│  MOD_SSL:                                                       │
│  SSLEngine On                                                   │
│  SSLCertificateFile    /path/cert.crt                           │
│  SSLCertificateKeyFile /path/key.key                            │
│                                                                 │
│  MOD_HEADERS:                                                   │
│  Header always set NAME "VALUE"    añadir/modificar cabecera    │
│  Header always unset NAME          eliminar cabecera            │
│  ServerTokens Prod                 ocultar versión              │
│  ServerSignature Off               ocultar firma en errores     │
│                                                                 │
│  MOD_DEFLATE:                                                   │
│  AddOutputFilterByType DEFLATE text/html text/css ...           │
│                                                                 │
│  MOD_EXPIRES:                                                   │
│  ExpiresActive On                                               │
│  ExpiresByType image/jpeg "access plus 1 month"                 │
│                                                                 │
│  MOD_STATUS:                                                    │
│  SetHandler server-status    en <Location "/server-status">     │
│  curl localhost/server-status?auto                              │
│                                                                 │
│  FASES DE PROCESAMIENTO:                                        │
│  PostRead → URI Translation → Access → Content → Filters → Log │
│  rewrite     alias/rewrite    authz    proxy     deflate   log  │
│              (fase 2)                  (fase 4)  headers         │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 10. Ejercicios

### Ejercicio 1: mod_rewrite — URLs limpias y redirecciones

1. Habilita mod_rewrite y verifica que está activo.
2. Crea un VirtualHost para `rewrite.local` con DocumentRoot
   `/var/www/rewrite/public`.
3. Crea `index.html` y un directorio `css/` con un archivo `style.css`.
4. Configura estas reglas en el VirtualHost (no en .htaccess):
   - Redirigir `www.rewrite.local` a `rewrite.local` (301).
   - Si el recurso solicitado no existe como archivo ni directorio, reescribir
     a `/index.html`.
   - Bloquear acceso a cualquier archivo que empiece con `.` (403).
5. Activa `LogLevel warn rewrite:trace3` y prueba cada regla con `curl`.
6. Verifica en el error log que las reglas se aplicaron correctamente.

**Pregunta de predicción**: Al solicitar `http://rewrite.local/css/style.css`,
¿se reescribirá a `/index.html`? ¿Por qué o por qué no? ¿Qué condición lo
evita?

### Ejercicio 2: mod_proxy — reverse proxy a un backend

1. Habilita mod_proxy y mod_proxy_http.
2. Inicia un servidor HTTP simple como backend:
   ```bash
   python3 -m http.server 8000 --directory /tmp/backend
   ```
3. Crea un VirtualHost para `proxy.local` que:
   - Sirva contenido estático desde `/var/www/proxy/public` en `/`.
   - Reenvíe `/api/` al backend Python en `http://127.0.0.1:8000/`.
   - Incluya `ProxyPreserveHost On`.
4. Prueba que `curl http://proxy.local/` sirve el contenido estático.
5. Prueba que `curl http://proxy.local/api/` sirve contenido del backend.
6. Detén el backend y prueba `/api/` de nuevo. ¿Qué código HTTP devuelve?
7. Si estás en RHEL, identifica si SELinux bloquea la conexión y resuélvelo.

**Pregunta de predicción**: Si el backend Python responde con un `Location:
http://127.0.0.1:8000/api/login` en una redirección, ¿qué verá el cliente sin
`ProxyPassReverse`? ¿Y con `ProxyPassReverse`?

### Ejercicio 3: Combinación de módulos

Configura un VirtualHost `full.local` que integre múltiples módulos:

1. mod_ssl con certificado autofirmado en puerto 443.
2. mod_rewrite para redirigir HTTP (80) a HTTPS (443).
3. mod_proxy para reenviar `/app/` a un backend en `localhost:3000`.
4. mod_headers para añadir `X-Content-Type-Options: nosniff` y eliminar
   `X-Powered-By`.
5. mod_deflate para comprimir respuestas HTML, CSS y JavaScript.
6. mod_expires con caché de 1 semana para imágenes.
7. Prueba cada funcionalidad de forma aislada con `curl`.
8. Ejecuta `apachectl -M` y lista solo los módulos que estás usando activamente.

**Pregunta de reflexión**: Si mod_rewrite redirige de HTTP a HTTPS y mod_proxy
reenvía `/app/` al backend, ¿en qué orden se procesan? Si un cliente solicita
`http://full.local/app/datos`, ¿cuántas peticiones HTTP se generan en total
(incluyendo la redirección y el proxy) antes de que el cliente vea la respuesta?

---

> **Siguiente tema**: T04 — .htaccess (override, performance implications,
> alternativas).
