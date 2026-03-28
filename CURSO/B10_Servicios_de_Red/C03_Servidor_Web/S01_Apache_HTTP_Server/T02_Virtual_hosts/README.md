# Virtual Hosts en Apache

## Índice

1. [Concepto de Virtual Host](#1-concepto-de-virtual-host)
2. [Tipos de Virtual Host](#2-tipos-de-virtual-host)
3. [Configuración de Virtual Hosts name-based](#3-configuración-de-virtual-hosts-name-based)
4. [DocumentRoot y estructura de directorios](#4-documentroot-y-estructura-de-directorios)
5. [Gestión en Debian vs RHEL](#5-gestión-en-debian-vs-rhel)
6. [Orden de evaluación y default VirtualHost](#6-orden-de-evaluación-y-default-virtualhost)
7. [Logging por VirtualHost](#7-logging-por-virtualhost)
8. [Directivas específicas dentro de VirtualHost](#8-directivas-específicas-dentro-de-virtualhost)
9. [Depuración de VirtualHosts](#9-depuración-de-virtualhosts)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Concepto de Virtual Host

Un Virtual Host permite servir múltiples sitios web desde una única instancia
de Apache. Sin esta capacidad, cada dominio necesitaría su propia instancia de
Apache con su propia IP, lo cual sería ineficiente y costoso.

```
                    Internet
                       │
          ┌────────────┼────────────┐
          │            │            │
     app.ejemplo.com   │   docs.ejemplo.com
          │       blog.ejemplo.com  │
          │            │            │
          ▼            ▼            ▼
     ┌─────────────────────────────────┐
     │         Apache httpd            │
     │         IP: 203.0.113.10        │
     │                                 │
     │  ┌─────────┐ ┌───────┐ ┌─────┐ │
     │  │ VHost 1 │ │VHost 2│ │VH 3 │ │
     │  │ app.*   │ │blog.* │ │docs.│ │
     │  └────┬────┘ └───┬───┘ └──┬──┘ │
     │       │          │        │     │
     └───────┼──────────┼────────┼─────┘
             │          │        │
             ▼          ▼        ▼
        /srv/app   /srv/blog  /srv/docs
```

### Cómo funciona la selección

Cuando Apache recibe una petición HTTP, el navegador incluye la cabecera `Host`:

```
GET /index.html HTTP/1.1
Host: blog.ejemplo.com
```

Apache compara este valor con los `ServerName` y `ServerAlias` de cada
VirtualHost configurado para determinar cuál sirve la respuesta.

---

## 2. Tipos de Virtual Host

### 2.1 Name-based (por nombre)

Múltiples dominios comparten la misma IP. Apache distingue por la cabecera
`Host`. Es el método estándar y el más usado:

```apache
# Dos sitios en la misma IP
<VirtualHost *:80>
    ServerName app.ejemplo.com
    DocumentRoot "/srv/app"
</VirtualHost>

<VirtualHost *:80>
    ServerName blog.ejemplo.com
    DocumentRoot "/srv/blog"
</VirtualHost>
```

### 2.2 IP-based (por dirección IP)

Cada sitio tiene una IP dedicada. Ya no es necesario para HTTPS desde la
introducción de SNI (Server Name Indication), pero se sigue usando en casos
específicos:

```apache
<VirtualHost 203.0.113.10:80>
    ServerName app.ejemplo.com
    DocumentRoot "/srv/app"
</VirtualHost>

<VirtualHost 203.0.113.11:80>
    ServerName blog.ejemplo.com
    DocumentRoot "/srv/blog"
</VirtualHost>
```

### 2.3 Port-based (por puerto)

Cada sitio escucha en un puerto diferente. Útil para entornos de desarrollo o
servicios internos:

```apache
Listen 8080
Listen 8081

<VirtualHost *:8080>
    ServerName app.ejemplo.com
    DocumentRoot "/srv/app"
</VirtualHost>

<VirtualHost *:8081>
    ServerName blog.ejemplo.com
    DocumentRoot "/srv/blog"
</VirtualHost>
```

### 2.4 Comparación

```
Name-based (estándar)          IP-based                  Port-based
─────────────────────          ────────                  ──────────
1 IP → N sitios                N IPs → N sitios          1 IP → N puertos
Requiere cabecera Host         No requiere Host          No requiere Host
Compatible con SNI/TLS         TLS sin SNI               Inusual para público
99% de los despliegues         Legacy / compliance       Dev / servicios internos
```

> **SNI (Server Name Indication)**: Extensión TLS que envía el nombre del
> servidor durante el handshake, permitiendo que Apache seleccione el certificado
> correcto antes de descifrar. Todos los navegadores modernos lo soportan. Esto
> eliminó la necesidad de IP-based para HTTPS.

---

## 3. Configuración de Virtual Hosts name-based

### 3.1 Estructura mínima

```apache
<VirtualHost *:80>
    ServerName www.ejemplo.com
    DocumentRoot "/var/www/ejemplo"
</VirtualHost>
```

El `*` en `<VirtualHost *:80>` significa "cualquier dirección IP en la que Apache
escuche en el puerto 80". Es equivalente a `<VirtualHost _default_:80>` cuando
hay name-based VHosts.

### 3.2 Configuración completa

```apache
<VirtualHost *:80>
    ServerName www.ejemplo.com
    ServerAlias ejemplo.com *.ejemplo.com

    DocumentRoot "/var/www/ejemplo/public"

    <Directory "/var/www/ejemplo/public">
        Options -Indexes +FollowSymLinks
        AllowOverride None
        Require all granted
    </Directory>

    # Logs separados por sitio
    ErrorLog ${APACHE_LOG_DIR}/ejemplo-error.log
    CustomLog ${APACHE_LOG_DIR}/ejemplo-access.log combined

    # Directivas específicas del sitio
    DirectoryIndex index.html index.php
    ServerAdmin webmaster@ejemplo.com
</VirtualHost>
```

### 3.3 ServerAlias — nombres alternativos

`ServerAlias` permite que un VirtualHost responda a múltiples nombres sin
duplicar la configuración:

```apache
<VirtualHost *:80>
    ServerName ejemplo.com
    ServerAlias www.ejemplo.com
    ServerAlias staging.ejemplo.com
    # O en una línea:
    # ServerAlias www.ejemplo.com staging.ejemplo.com *.ejemplo.com
    DocumentRoot "/var/www/ejemplo"
</VirtualHost>
```

Soporta wildcards: `*.ejemplo.com` coincide con cualquier subdominio. El
wildcard solo funciona en `ServerAlias`, no en `ServerName`.

### 3.4 Redirección de www a no-www (o viceversa)

Patrón habitual — VirtualHost dedicado para redireccionar:

```apache
# VHost que redirige www → dominio raíz
<VirtualHost *:80>
    ServerName www.ejemplo.com
    Redirect permanent "/" "http://ejemplo.com/"
</VirtualHost>

# VHost principal
<VirtualHost *:80>
    ServerName ejemplo.com
    DocumentRoot "/var/www/ejemplo"
</VirtualHost>
```

Alternativa con `ServerAlias` + mod_rewrite (todo en un VHost):

```apache
<VirtualHost *:80>
    ServerName ejemplo.com
    ServerAlias www.ejemplo.com
    DocumentRoot "/var/www/ejemplo"

    RewriteEngine On
    RewriteCond %{HTTP_HOST} ^www\.ejemplo\.com$ [NC]
    RewriteRule ^(.*)$ http://ejemplo.com$1 [R=301,L]
</VirtualHost>
```

---

## 4. DocumentRoot y estructura de directorios

### 4.1 Convenciones de ubicación

No existe un estándar obligatorio, pero hay convenciones establecidas:

```
/var/www/                          ← FHS estándar
├── html/                          ← DocumentRoot por defecto
├── ejemplo.com/
│   └── public/                    ← DocumentRoot del sitio
├── blog.ejemplo.com/
│   └── public/
└── app.ejemplo.com/
    └── public/

/srv/www/                          ← alternativa (FHS permite /srv para servicios)
├── ejemplo.com/
│   ├── public/                    ← DocumentRoot
│   ├── logs/                      ← logs del sitio (alternativa a /var/log)
│   └── config/                    ← configuración de la app
└── blog.ejemplo.com/
    └── public/
```

**Por qué un subdirectorio `public/`**: El DocumentRoot no debe ser la raíz del
proyecto. Archivos como `.env`, `config.php` o `.git/` que están un nivel arriba
del DocumentRoot no son accesibles vía web, lo cual es una protección por diseño:

```
/var/www/ejemplo.com/
├── .env                  ← FUERA del DocumentRoot = inaccesible vía web
├── config/
│   └── database.yml      ← inaccesible vía web
└── public/               ← DocumentRoot apunta aquí
    ├── index.html         ← accesible: http://ejemplo.com/
    ├── css/
    └── images/
```

### 4.2 Permisos recomendados

```bash
# Propietario: usuario del desarrollador o deploy
# Grupo: grupo de Apache
chown -R deploy:www-data /var/www/ejemplo.com    # Debian
chown -R deploy:apache /var/www/ejemplo.com      # RHEL

# Directorios: 750 (rwxr-x---)
find /var/www/ejemplo.com -type d -exec chmod 750 {} \;

# Archivos: 640 (rw-r-----)
find /var/www/ejemplo.com -type f -exec chmod 640 {} \;
```

### 4.3 SELinux y DocumentRoot personalizado (RHEL)

Si se mueve el DocumentRoot fuera de `/var/www/`, SELinux bloqueará el acceso:

```bash
# Error típico en /var/log/audit/audit.log:
# type=AVC msg=audit(...): avc:  denied  { read } ... scontext=...httpd_t...

# Solución: asignar contexto correcto
semanage fcontext -a -t httpd_sys_content_t "/srv/www(/.*)?"
restorecon -Rv /srv/www

# Si la app necesita escribir (uploads):
semanage fcontext -a -t httpd_sys_rw_content_t "/srv/www/ejemplo.com/public/uploads(/.*)?"
restorecon -Rv /srv/www/ejemplo.com/public/uploads
```

---

## 5. Gestión en Debian vs RHEL

### 5.1 Workflow en Debian/Ubuntu

```bash
# 1. Crear el archivo de configuración
cat > /etc/apache2/sites-available/ejemplo.conf << 'VHOST'
<VirtualHost *:80>
    ServerName ejemplo.com
    ServerAlias www.ejemplo.com
    DocumentRoot /var/www/ejemplo.com/public

    <Directory /var/www/ejemplo.com/public>
        Options -Indexes +FollowSymLinks
        AllowOverride None
        Require all granted
    </Directory>

    ErrorLog ${APACHE_LOG_DIR}/ejemplo-error.log
    CustomLog ${APACHE_LOG_DIR}/ejemplo-access.log combined
</VirtualHost>
VHOST

# 2. Crear estructura de directorios
mkdir -p /var/www/ejemplo.com/public
echo "<h1>ejemplo.com</h1>" > /var/www/ejemplo.com/public/index.html
chown -R www-data:www-data /var/www/ejemplo.com

# 3. Habilitar el sitio (crea symlink en sites-enabled/)
a2ensite ejemplo

# 4. Opcionalmente deshabilitar el sitio por defecto
a2dissite 000-default

# 5. Verificar y recargar
apachectl configtest && systemctl reload apache2
```

**¿Qué hace `a2ensite` internamente?**

```bash
# Equivale a:
ln -s /etc/apache2/sites-available/ejemplo.conf \
      /etc/apache2/sites-enabled/ejemplo.conf
```

### 5.2 Workflow en RHEL/Fedora

```bash
# 1. Crear directamente en conf.d/ (se incluye automáticamente)
cat > /etc/httpd/conf.d/ejemplo.conf << 'VHOST'
<VirtualHost *:80>
    ServerName ejemplo.com
    ServerAlias www.ejemplo.com
    DocumentRoot /var/www/ejemplo.com/public

    <Directory /var/www/ejemplo.com/public>
        Options -Indexes +FollowSymLinks
        AllowOverride None
        Require all granted
    </Directory>

    ErrorLog /var/log/httpd/ejemplo-error.log
    CustomLog /var/log/httpd/ejemplo-access.log combined
</VirtualHost>
VHOST

# 2. Crear estructura
mkdir -p /var/www/ejemplo.com/public
echo "<h1>ejemplo.com</h1>" > /var/www/ejemplo.com/public/index.html
chown -R apache:apache /var/www/ejemplo.com

# 3. SELinux (si DocumentRoot está en /var/www/, ya tiene contexto)
# Solo necesario si se usa otra ruta

# 4. Verificar y recargar
httpd -t && systemctl reload httpd
```

**Deshabilitar un sitio en RHEL** (sin a2dissite):

```bash
# Opción 1: renombrar para que no coincida con *.conf
mv /etc/httpd/conf.d/ejemplo.conf /etc/httpd/conf.d/ejemplo.conf.disabled

# Opción 2: mover fuera del directorio
mv /etc/httpd/conf.d/ejemplo.conf /etc/httpd/conf.d.disabled/
```

---

## 6. Orden de evaluación y default VirtualHost

### 6.1 Algoritmo de selección

Cuando Apache recibe una petición, sigue este proceso:

```
Petición entrante: Host: blog.ejemplo.com, IP: 203.0.113.10:80
    │
    ▼
1. Buscar VirtualHosts que coincidan con IP:puerto
   (si es *:80, todos los *:80 coinciden)
    │
    ▼
2. Del conjunto que coincide, buscar ServerName/ServerAlias
   que coincida con la cabecera Host
    │
    ├── Encontrado → servir con ese VirtualHost
    │
    └── No encontrado → usar el PRIMER VirtualHost
        del conjunto (default)
```

### 6.2 El primer VirtualHost como catch-all

El primer VirtualHost definido para un IP:puerto actúa como **default** cuando
ningún `ServerName`/`ServerAlias` coincide. Esto tiene implicaciones de
seguridad:

```apache
# Este VirtualHost captura todo lo que no coincida con nada
# Debe ser el PRIMERO en el orden de carga
<VirtualHost *:80>
    ServerName default.ejemplo.com

    # Opción 1: rechazar peticiones desconocidas
    <Location "/">
        Require all denied
    </Location>

    # Opción 2: redirigir al sitio principal
    # Redirect permanent "/" "http://ejemplo.com/"

    ErrorLog ${APACHE_LOG_DIR}/default-error.log
    CustomLog ${APACHE_LOG_DIR}/default-access.log combined
</VirtualHost>

# Los sitios reales vienen después
<VirtualHost *:80>
    ServerName ejemplo.com
    DocumentRoot "/var/www/ejemplo"
    ...
</VirtualHost>
```

### 6.3 Control del orden de carga

El orden importa porque determina el default. Apache carga en este orden:

**Debian**: Los archivos en `sites-enabled/` se incluyen alfabéticamente. Por eso
el sitio por defecto se llama `000-default.conf` — el prefijo `000` garantiza que
se carga primero:

```bash
ls /etc/apache2/sites-enabled/
# 000-default.conf -> ../sites-available/000-default.conf
# ejemplo.conf -> ../sites-available/ejemplo.conf
```

**RHEL**: Los archivos en `conf.d/` también se cargan alfabéticamente. Usar
prefijos numéricos para controlar el orden:

```bash
ls /etc/httpd/conf.d/
# 00-default.conf
# 10-ejemplo.conf
# 20-blog.conf
```

### 6.4 Ver el orden efectivo

```bash
apachectl -S
```

Salida típica:

```
VirtualHost configuration:
*:80                   is a NameVirtualHost
         default server default.ejemplo.com (/etc/apache2/sites-enabled/000-default.conf:1)
         port 80 namevhost ejemplo.com (/etc/apache2/sites-enabled/ejemplo.conf:1)
                 alias www.ejemplo.com
         port 80 namevhost blog.ejemplo.com (/etc/apache2/sites-enabled/blog.conf:1)
```

La primera línea bajo cada IP:puerto muestra el `default server`.

---

## 7. Logging por VirtualHost

### 7.1 Logs separados vs centralizados

Cada VirtualHost puede tener sus propios archivos de log. Esto facilita el
análisis por sitio pero multiplica los archivos abiertos:

```apache
# Logs separados (recomendado para pocos sitios)
<VirtualHost *:80>
    ServerName ejemplo.com
    ErrorLog ${APACHE_LOG_DIR}/ejemplo-error.log
    CustomLog ${APACHE_LOG_DIR}/ejemplo-access.log combined
</VirtualHost>

<VirtualHost *:80>
    ServerName blog.ejemplo.com
    ErrorLog ${APACHE_LOG_DIR}/blog-error.log
    CustomLog ${APACHE_LOG_DIR}/blog-access.log combined
</VirtualHost>
```

### 7.2 Log centralizado con identificación de VHost

Para muchos sitios, un solo archivo con el nombre del VHost en cada línea:

```apache
# Definir formato que incluya el ServerName
LogFormat "%v %h %l %u %t \"%r\" %>s %b \"%{Referer}i\" \"%{User-Agent}i\"" vhost_combined

# En el config global (fuera de VirtualHosts)
CustomLog ${APACHE_LOG_DIR}/access.log vhost_combined
```

El `%v` añade el `ServerName` del VirtualHost que procesó la petición:

```
ejemplo.com 192.168.1.50 - - [21/Mar/2026:10:15:33 +0100] "GET / HTTP/1.1" 200 ...
blog.ejemplo.com 192.168.1.51 - - [21/Mar/2026:10:15:34 +0100] "GET / HTTP/1.1" 200 ...
```

### 7.3 Piped logs para procesamiento en tiempo real

```apache
# Rotar automáticamente con rotatelogs
CustomLog "|/usr/bin/rotatelogs /var/log/apache2/ejemplo-access.%Y%m%d.log 86400" combined

# Filtrar con un script externo
CustomLog "|/usr/local/bin/log-processor.sh" combined
```

> **Nota**: Si un VirtualHost no define `ErrorLog`, hereda el del servidor
> principal. Si no define `CustomLog`, el acceso se registra en el `CustomLog`
> global (si existe) y además en el del VirtualHost si se define. Un CustomLog
> dentro de VirtualHost **no reemplaza** al global — ambos registran.

---

## 8. Directivas específicas dentro de VirtualHost

### 8.1 Directivas que cambian por sitio

```apache
<VirtualHost *:80>
    ServerName app.ejemplo.com
    DocumentRoot "/var/www/app/public"

    # Alias: mapear una URL a otra ruta del filesystem
    Alias "/static" "/var/www/app/assets"
    <Directory "/var/www/app/assets">
        Require all granted
        Options -Indexes
    </Directory>

    # Redirección
    Redirect permanent "/old-page" "/new-page"
    RedirectMatch 301 "^/blog/(.*)$" "http://blog.ejemplo.com/$1"

    # Headers personalizados
    Header always set X-Content-Type-Options "nosniff"
    Header always set X-Frame-Options "SAMEORIGIN"

    # Configuración de PHP (si aplica)
    <FilesMatch "\.php$">
        SetHandler "proxy:unix:/run/php/php-fpm.sock|fcgi://localhost"
    </FilesMatch>

    # Limitar tamaño de uploads
    LimitRequestBody 10485760    # 10 MB

    # Timeout personalizado
    TimeOut 60
</VirtualHost>
```

### 8.2 Alias vs Redirect vs Rewrite

| Directiva      | Opera en        | Efecto                                    |
|----------------|-----------------|-------------------------------------------|
| `Alias`        | Servidor        | Mapea URL a ruta filesystem diferente      |
| `Redirect`     | Cliente         | Envía 301/302 al navegador (cambia URL)    |
| `RedirectMatch`| Cliente         | Como Redirect pero con regex               |
| `RewriteRule`  | Ambos           | Reescritura flexible (requiere mod_rewrite)|

```apache
# Alias: /downloads/file.zip sirve /opt/storage/file.zip
# El navegador sigue viendo /downloads/file.zip
Alias "/downloads" "/opt/storage"

# Redirect: /viejo redirige al navegador a /nuevo
# El navegador cambia la URL visible
Redirect permanent "/viejo" "/nuevo"
```

### 8.3 Directory dentro de VirtualHost

Los bloques `<Directory>` dentro de un `<VirtualHost>` aplican solo a ese
VirtualHost. Esto permite reglas diferentes por sitio para el mismo directorio
padre:

```apache
<VirtualHost *:80>
    ServerName app.ejemplo.com
    DocumentRoot "/var/www/app/public"

    <Directory "/var/www/app/public">
        AllowOverride All       # permite .htaccess
        Require all granted
    </Directory>
</VirtualHost>

<VirtualHost *:80>
    ServerName docs.ejemplo.com
    DocumentRoot "/var/www/docs/public"

    <Directory "/var/www/docs/public">
        AllowOverride None      # sin .htaccess (mejor rendimiento)
        Require all granted
    </Directory>
</VirtualHost>
```

---

## 9. Depuración de VirtualHosts

### 9.1 Herramientas de diagnóstico

```bash
# 1. Ver configuración efectiva de VirtualHosts
apachectl -S

# 2. Validar sintaxis
apachectl configtest

# 3. Probar con curl especificando el Host header
curl -H "Host: ejemplo.com" http://localhost/
curl -H "Host: blog.ejemplo.com" http://localhost/

# 4. Ver respuesta completa con headers
curl -v -H "Host: ejemplo.com" http://localhost/ 2>&1 | head -20

# 5. Verificar resolución DNS (o /etc/hosts)
getent hosts ejemplo.com
```

### 9.2 Simular dominios con /etc/hosts

Para pruebas locales sin DNS:

```bash
# /etc/hosts
127.0.0.1   ejemplo.com www.ejemplo.com
127.0.0.1   blog.ejemplo.com
127.0.0.1   app.ejemplo.com
```

### 9.3 Diagnóstico paso a paso cuando un VHost no responde

```
1. ¿Apache está corriendo?
   └── systemctl status apache2/httpd

2. ¿Escucha en el puerto correcto?
   └── ss -tlnp | grep apache2/httpd

3. ¿La sintaxis es correcta?
   └── apachectl configtest

4. ¿El VHost está habilitado?
   └── apachectl -S  (debe aparecer en la lista)

5. ¿El Host header llega correctamente?
   └── curl -v -H "Host: ejemplo.com" http://localhost/

6. ¿El DocumentRoot existe y tiene permisos?
   └── ls -la /var/www/ejemplo.com/public/
   └── namei -l /var/www/ejemplo.com/public/  (verifica toda la cadena)

7. ¿SELinux bloquea? (RHEL)
   └── ausearch -m avc -ts recent

8. ¿Qué dice el error log?
   └── tail /var/log/apache2/ejemplo-error.log
```

---

## 10. Errores comunes

### Error 1: Todos los dominios muestran el mismo sitio

**Causa**: Los VirtualHosts comparten el mismo `ServerName` o falta por completo,
haciendo que todos caigan al default.

**Diagnóstico**:

```bash
apachectl -S | grep "namevhost"
```

**Solución**: Verificar que cada VirtualHost tenga un `ServerName` único y
distinto. Un typo en el nombre es suficiente para que no coincida.

### Error 2: VirtualHost ignorado en Debian

**Causa**: El archivo existe en `sites-available/` pero no se ejecutó `a2ensite`:

```bash
ls -la /etc/apache2/sites-enabled/
# No aparece el symlink del sitio
```

**Solución**: `a2ensite nombre && systemctl reload apache2`.

### Error 3: 403 Forbidden en el DocumentRoot

**Causa**: Falta el bloque `<Directory>` con `Require all granted` para el
DocumentRoot del VirtualHost.

**Predicción**: Por defecto, Apache 2.4 deniega acceso a todo directorio que
no tenga una directiva `Require` explícita. El archivo principal incluye
`Require all denied` para `/` (raíz del filesystem), y cada DocumentRoot
necesita su propio `Require all granted` para sobreescribirlo.

**Solución**: Añadir el bloque `<Directory>` dentro del VirtualHost.

### Error 4: El default VirtualHost sirve contenido incorrecto

**Causa**: El orden de carga hace que un sitio inesperado sea el default.

**Diagnóstico**:

```bash
apachectl -S
# default server muestra cuál es
```

**Solución**: En Debian, usar prefijo numérico (`000-catch-all.conf`). En RHEL,
nombrar el archivo default con prefijo bajo (`00-default.conf`).

### Error 5: Wildcard ServerAlias captura tráfico de otro VHost

```apache
# VHost A
<VirtualHost *:80>
    ServerName ejemplo.com
    ServerAlias *.ejemplo.com      # ← captura TODO subdominio
    DocumentRoot "/var/www/ejemplo"
</VirtualHost>

# VHost B — nunca recibe tráfico
<VirtualHost *:80>
    ServerName blog.ejemplo.com
    DocumentRoot "/var/www/blog"
</VirtualHost>
```

**Causa**: Apache evalúa `ServerName` primero, después `ServerAlias`. Pero si
VHost A aparece antes y su `ServerAlias *.ejemplo.com` coincide, depende de la
implementación interna.

**Solución**: Apache verifica primero `ServerName` de todos los VHosts, luego
`ServerAlias`. Esto significa que `blog.ejemplo.com` como `ServerName` en VHost B
tiene prioridad sobre el wildcard `ServerAlias` de VHost A. Si no funciona,
verificar con `apachectl -S` y reordenar si es necesario.

---

## 11. Cheatsheet

```
┌─────────────────────────────────────────────────────────────────┐
│               VIRTUAL HOSTS — REFERENCIA RÁPIDA                │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ESTRUCTURA MÍNIMA:                                             │
│  <VirtualHost *:80>                                             │
│      ServerName ejemplo.com                                     │
│      ServerAlias www.ejemplo.com                                │
│      DocumentRoot "/var/www/ejemplo/public"                     │
│      <Directory "/var/www/ejemplo/public">                      │
│          Require all granted                                    │
│      </Directory>                                               │
│      ErrorLog ${APACHE_LOG_DIR}/ejemplo-error.log               │
│      CustomLog ${APACHE_LOG_DIR}/ejemplo-access.log combined    │
│  </VirtualHost>                                                 │
│                                                                 │
│  TIPOS:                                                         │
│  *:80 .............. name-based (estándar, por Host header)     │
│  IP:80 ............. IP-based (legacy)                          │
│  *:8080 ............ port-based (dev/interno)                   │
│                                                                 │
│  DEBIAN WORKFLOW:                                               │
│  Crear:    /etc/apache2/sites-available/ejemplo.conf            │
│  Activar:  a2ensite ejemplo                                     │
│  Desact.:  a2dissite ejemplo                                    │
│  Recargar: systemctl reload apache2                             │
│                                                                 │
│  RHEL WORKFLOW:                                                 │
│  Crear:    /etc/httpd/conf.d/ejemplo.conf                       │
│  Desact.:  renombrar a .disabled                                │
│  Recargar: systemctl reload httpd                               │
│                                                                 │
│  ORDEN DE CARGA: alfabético por nombre de archivo               │
│  Default VHost: el primero cargado para ese IP:puerto           │
│  Usar prefijos: 000-default.conf, 010-ejemplo.conf              │
│                                                                 │
│  EVALUACIÓN DE PETICIONES:                                      │
│  1. Filtrar VHosts por IP:puerto                                │
│  2. Buscar coincidencia por ServerName (todos los VHosts)       │
│  3. Buscar por ServerAlias (todos los VHosts)                   │
│  4. Si nada coincide → primer VHost (default)                   │
│                                                                 │
│  DEPURACIÓN:                                                    │
│  apachectl -S            ver VHosts y default                   │
│  apachectl configtest    validar sintaxis                       │
│  curl -H "Host: X" URL  probar VHost específico                │
│  namei -l /path          verificar cadena de permisos           │
│                                                                 │
│  LOG CENTRALIZADO CON VHOST:                                    │
│  LogFormat "%v %h ..." vhost_combined                           │
│  CustomLog log vhost_combined   (global, fuera de VHost)        │
│                                                                 │
│  DOCUMENTROOT SEGURO:                                           │
│  /var/www/sitio/                                                │
│  ├── .env          ← fuera del DocumentRoot                    │
│  └── public/       ← DocumentRoot aquí                         │
│      └── index.html                                             │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: Configurar dos sitios name-based

1. Crea dos VirtualHosts: `sitio-a.local` y `sitio-b.local`.
2. Cada uno con su propio DocumentRoot (`/var/www/sitio-a/public` y
   `/var/www/sitio-b/public`).
3. Cada uno con un `index.html` que identifique el sitio.
4. Logs de error y acceso separados por sitio.
5. Añade entradas en `/etc/hosts` para ambos dominios.
6. Verifica con `apachectl -S` que ambos aparecen.
7. Prueba con `curl` que cada dominio devuelve su contenido correcto.
8. Prueba accediendo con un dominio que no existe (ej: `curl -H "Host: inexistente.local" http://localhost/`). ¿Qué sitio responde?

**Pregunta de predicción**: Si `sitio-b.conf` se carga antes alfabéticamente que
`sitio-a.conf`, ¿cuál de los dos será el default VirtualHost? ¿Cómo lo
verificarías?

### Ejercicio 2: Catch-all y redirecciones

1. Crea un VirtualHost catch-all que responda con 403 a cualquier dominio no
   reconocido. Asegúrate de que se carga primero (prefijo `000-`).
2. Configura `ejemplo.local` como sitio principal con contenido.
3. Configura que `www.ejemplo.local` redirija permanentemente (301) a
   `ejemplo.local`.
4. Verifica las tres situaciones con `curl -v`:
   - `curl -H "Host: ejemplo.local" http://localhost/` → 200
   - `curl -H "Host: www.ejemplo.local" http://localhost/` → 301
   - `curl -H "Host: atacante.com" http://localhost/` → 403
5. Revisa los logs de cada VirtualHost para confirmar que las peticiones se
   registraron en el archivo correcto.

**Pregunta de predicción**: Si olvidas el catch-all y un atacante envía una
petición con `Host: atacante.com`, ¿qué contenido verá? ¿Qué implicaciones de
seguridad tiene esto?

### Ejercicio 3: Diagnóstico de un VirtualHost roto

Configura deliberadamente estos problemas y diagnostícalos:

1. Crea un VirtualHost para `roto.local` con DocumentRoot
   `/var/www/roto/public` pero **no** crees el directorio. ¿Qué error
   devuelve Apache? ¿Qué dice el error log?
2. Crea el directorio pero **sin** el bloque `<Directory>`. ¿Qué código HTTP
   devuelve?
3. Añade el bloque `<Directory>` con `Require all granted`. En RHEL, asigna
   el DocumentRoot a `/srv/roto/public/` sin configurar SELinux. ¿Qué error
   aparece en audit.log?
4. Para cada caso, describe el flujo de diagnóstico que seguiste y qué
   herramienta te dio la pista definitiva.

**Pregunta de reflexión**: Cuando Apache devuelve 403 Forbidden, hay al menos
tres causas posibles (permisos POSIX, falta de `Require`, SELinux). ¿En qué
orden investigarías y por qué? ¿Qué herramienta descarta cada causa más
rápidamente?

---

> **Siguiente tema**: T03 — Módulos (mod_rewrite, mod_proxy, mod_ssl,
> habilitación/deshabilitación).
