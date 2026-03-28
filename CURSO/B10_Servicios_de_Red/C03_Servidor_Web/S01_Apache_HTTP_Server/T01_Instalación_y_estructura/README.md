# Instalación y estructura de Apache HTTP Server

## Índice

1. [Apache en el ecosistema web](#1-apache-en-el-ecosistema-web)
2. [Instalación](#2-instalación)
3. [Arquitectura de procesos (MPMs)](#3-arquitectura-de-procesos-mpms)
4. [Estructura de archivos de configuración](#4-estructura-de-archivos-de-configuración)
5. [Anatomía de httpd.conf / apache2.conf](#5-anatomía-de-httpdconf--apache2conf)
6. [Directivas fundamentales](#6-directivas-fundamentales)
7. [Contenedores de contexto](#7-contenedores-de-contexto)
8. [Gestión del servicio](#8-gestión-del-servicio)
9. [Diferencias Debian vs RHEL — resumen comparativo](#9-diferencias-debian-vs-rhel--resumen-comparativo)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Apache en el ecosistema web

Apache HTTP Server (httpd) lleva operando desde 1995 y sigue siendo uno de los
servidores web más desplegados. Su arquitectura modular permite que un mismo
binario actúe como servidor de archivos estáticos, proxy inverso, balanceador de
carga o terminador TLS, según los módulos cargados.

```
                    Cliente (navegador)
                          │
                          ▼
              ┌───────────────────────┐
              │    Apache httpd       │
              │                       │
              │  ┌─────────────────┐  │
              │  │   MPM           │  │  ← modelo de concurrencia
              │  │  (event/worker/ │  │
              │  │   prefork)      │  │
              │  └────────┬────────┘  │
              │           │           │
              │  ┌────────▼────────┐  │
              │  │   Core          │  │  ← procesamiento de requests
              │  └────────┬────────┘  │
              │           │           │
              │  ┌────────▼────────┐  │
              │  │   Módulos       │  │  ← funcionalidad extensible
              │  │  mod_ssl        │  │
              │  │  mod_rewrite    │  │
              │  │  mod_proxy      │  │
              │  │  mod_php (si)   │  │
              │  └─────────────────┘  │
              └───────────────────────┘
                          │
                          ▼
              Sistema de archivos / Backend
```

### Versiones relevantes

La versión actual es Apache 2.4.x (rama estable desde 2012). Las diferencias
principales con 2.2 que aún aparecen en documentación antigua:

| Aspecto              | Apache 2.2                  | Apache 2.4                    |
|----------------------|-----------------------------|-------------------------------|
| Control de acceso    | `Order`, `Allow`, `Deny`    | `Require` (mod_authz)         |
| MPM por defecto      | prefork                     | event (mayoría de distros)    |
| Directiva Include    | Solo rutas absolutas        | Soporta wildcards y relativas |
| Expresiones          | No disponible               | `<If>` con expresiones        |

> **Importante**: Las directivas `Order`/`Allow`/`Deny` de 2.2 siguen
> funcionando si `mod_access_compat` está cargado, pero **no deben usarse** en
> configuraciones nuevas. Usar siempre `Require`.

---

## 2. Instalación

### 2.1 Debian/Ubuntu

```bash
# Instalar
apt update
apt install apache2

# Verificar
apache2 -v            # versión
systemctl status apache2

# Archivos principales instalados
# /etc/apache2/           configuración
# /var/www/html/          DocumentRoot por defecto
# /var/log/apache2/       logs (access.log, error.log)
# /usr/lib/apache2/       módulos compilados (.so)
# /usr/sbin/apache2       binario
```

### 2.2 RHEL/Fedora

```bash
# Instalar
dnf install httpd

# Verificar
httpd -v
systemctl status httpd

# Archivos principales instalados
# /etc/httpd/             configuración
# /var/www/html/          DocumentRoot por defecto
# /var/log/httpd/         logs
# /usr/lib64/httpd/       módulos compilados (.so)
# /usr/sbin/httpd         binario
```

### 2.3 Habilitar e iniciar

```bash
# Debian/Ubuntu
systemctl enable --now apache2

# RHEL/Fedora
systemctl enable --now httpd

# Firewall (RHEL/Fedora)
firewall-cmd --permanent --add-service=http
firewall-cmd --permanent --add-service=https
firewall-cmd --reload

# SELinux (RHEL) — contextos por defecto
# /var/www(/.*)?  → httpd_sys_content_t (lectura)
# /var/www/cgi-bin(/.*)? → httpd_sys_script_exec_t
```

### 2.4 Verificación post-instalación

```bash
# Comprobar que escucha en puerto 80
ss -tlnp | grep ':80'

# Comprobar respuesta
curl -I http://localhost

# Salida esperada:
# HTTP/1.1 200 OK
# Server: Apache/2.4.x ...
```

---

## 3. Arquitectura de procesos (MPMs)

MPM (Multi-Processing Module) determina cómo Apache maneja la concurrencia.
Solo un MPM puede estar activo a la vez.

### 3.1 Comparación de MPMs

```
prefork                    worker                     event
┌──────────┐              ┌──────────┐              ┌──────────┐
│  master   │              │  master   │              │  master   │
│  process  │              │  process  │              │  process  │
└─────┬─────┘              └─────┬─────┘              └─────┬─────┘
      │                          │                          │
 ┌────┼────┐              ┌──────┼──────┐            ┌──────┼──────┐
 │    │    │              │      │      │            │      │      │
 ▼    ▼    ▼              ▼      ▼      ▼            ▼      ▼      ▼
┌─┐  ┌─┐  ┌─┐          ┌───┐  ┌───┐  ┌───┐       ┌───┐  ┌───┐  ┌───┐
│P│  │P│  │P│          │P  │  │P  │  │P  │       │P  │  │P  │  │P  │
│ │  │ │  │ │          │T T│  │T T│  │T T│       │T T│  │T T│  │T T│
│1│  │1│  │1│          │1 2│  │1 2│  │1 2│       │1 2│  │1 2│  │1 2│
│c│  │c│  │c│          │c c│  │c c│  │c c│       │   │  │   │  │   │
└─┘  └─┘  └─┘          └───┘  └───┘  └───┘       └─┬─┘  └─┬─┘  └─┬─┘
                                                    │      │      │
1 proceso =             1 proceso =               listener thread
1 conexión              N threads/conex.          gestiona keep-alive
                                                  libera threads
```

| Característica     | prefork            | worker             | event (recomendado) |
|--------------------|--------------------|--------------------|---------------------|
| Modelo             | 1 proceso/conexión | Threads por proceso| Threads + listener  |
| Memoria            | Alta               | Moderada           | Baja                |
| Thread-safety req. | No                 | Sí                 | Sí                  |
| Keep-alive         | Bloquea proceso    | Bloquea thread     | No bloquea          |
| Caso de uso        | mod_php (legacy)   | General            | **Producción moderna** |

### 3.2 Verificar y cambiar MPM

```bash
# Ver MPM activo
apachectl -V | grep MPM
# o
httpd -V | grep MPM

# Debian: el MPM es un módulo conmutable
a2dismod mpm_prefork
a2enmod mpm_event
systemctl restart apache2

# RHEL: editar /etc/httpd/conf.modules.d/00-mpm.conf
# Comentar el MPM actual, descomentar el deseado:
# LoadModule mpm_event_module modules/mod_mpm_event.so
```

### 3.3 Tuning del MPM event

Los valores por defecto son conservadores. Ajustar según la capacidad del
servidor:

```apache
# En mpm_event.conf o httpd.conf
<IfModule mpm_event_module>
    StartServers             3
    MinSpareThreads         75
    MaxSpareThreads        250
    ThreadsPerChild         25
    MaxRequestWorkers      400     # máximo de conexiones simultáneas
    MaxConnectionsPerChild   0     # 0 = sin límite (procesos no se reciclan)
    AsyncRequestWorkerFactor 2
</IfModule>
```

**Cálculo de `MaxRequestWorkers`**: Determina el máximo absoluto de conexiones
concurrentes. Debe ser múltiplo de `ThreadsPerChild`. Fórmula orientativa:

```
MaxRequestWorkers = (RAM disponible para Apache) / (memoria media por thread)
```

Monitorear con `apachectl status` o `server-status` tras configurar.

---

## 4. Estructura de archivos de configuración

La diferencia más visible entre familias es la organización de archivos de
configuración.

### 4.1 Debian/Ubuntu — estructura modular

```
/etc/apache2/
├── apache2.conf              ← archivo principal
├── envvars                   ← variables de entorno (APACHE_RUN_USER, etc.)
├── ports.conf                ← Listen 80, Listen 443
├── magic                     ← detección MIME por contenido
│
├── mods-available/           ← módulos disponibles (.load + .conf)
│   ├── rewrite.load
│   ├── ssl.load
│   ├── ssl.conf
│   └── ...
├── mods-enabled/             ← symlinks a mods-available/ (activos)
│   ├── rewrite.load -> ../mods-available/rewrite.load
│   └── ...
│
├── sites-available/          ← VirtualHosts definidos
│   ├── 000-default.conf
│   └── mi-sitio.conf
├── sites-enabled/            ← symlinks a sites-available/ (activos)
│   └── 000-default.conf -> ../sites-available/000-default.conf
│
└── conf-available/           ← fragmentos de configuración
    ├── security.conf
    └── ...
└── conf-enabled/             ← symlinks activos
```

**Herramientas de gestión (exclusivas de Debian):**

```bash
a2ensite mi-sitio         # habilita VirtualHost (crea symlink)
a2dissite mi-sitio        # deshabilita VirtualHost
a2enmod rewrite           # habilita módulo
a2dismod rewrite          # deshabilita módulo
a2enconf security         # habilita fragmento de configuración
a2disconf security        # deshabilita fragmento
```

### 4.2 RHEL/Fedora — estructura monolítica

```
/etc/httpd/
├── conf/
│   └── httpd.conf            ← archivo principal (todo aquí o incluido)
│
├── conf.d/                   ← fragmentos incluidos automáticamente
│   ├── ssl.conf              ← configuración TLS (si mod_ssl instalado)
│   ├── welcome.conf          ← página por defecto
│   └── mi-sitio.conf         ← VirtualHosts van aquí
│
├── conf.modules.d/           ← carga de módulos
│   ├── 00-base.conf
│   ├── 00-mpm.conf
│   ├── 00-ssl.conf
│   └── ...
│
├── logs -> /var/log/httpd    ← symlink de conveniencia
├── modules -> /usr/lib64/httpd/modules  ← symlink
└── run -> /run/httpd         ← PID y sockets
```

En RHEL no existen `a2ensite`/`a2dissite`. Los VirtualHosts se crean
directamente como archivos `.conf` en `/etc/httpd/conf.d/` y se incluyen
automáticamente por la línea en `httpd.conf`:

```apache
IncludeOptional conf.d/*.conf
```

### 4.3 Diagrama comparativo del flujo de inclusión

```
DEBIAN/UBUNTU                          RHEL/FEDORA
─────────────                          ───────────
apache2.conf                           httpd.conf
    │                                      │
    ├── Include ports.conf                 ├── Include conf.modules.d/*.conf
    │                                      │   (carga de módulos)
    ├── Include mods-enabled/*.load        │
    ├── Include mods-enabled/*.conf        ├── IncludeOptional conf.d/*.conf
    │   (módulos + su configuración)       │   (VirtualHosts + extras)
    │                                      │
    ├── Include conf-enabled/*.conf        └── (todo en un flujo lineal)
    │   (fragmentos extra)
    │
    └── Include sites-enabled/*.conf
        (VirtualHosts)
```

---

## 5. Anatomía de httpd.conf / apache2.conf

El archivo principal se procesa linealmente de arriba a abajo. Las directivas
se aplican según el contexto donde aparecen.

### 5.1 Secciones del archivo principal

```apache
# ─── 1. Configuración global del servidor ───
ServerRoot "/etc/httpd"          # raíz de rutas relativas
Listen 80                        # puerto(s) de escucha
ServerAdmin admin@ejemplo.com    # email del administrador
ServerName www.ejemplo.com:80    # nombre canónico del servidor

# ─── 2. Módulos ───
LoadModule rewrite_module modules/mod_rewrite.so
LoadModule ssl_module modules/mod_ssl.so

# ─── 3. Configuración del proceso ───
User apache                      # Debian: www-data
Group apache                     # Debian: www-data

# ─── 4. Servidor principal ───
DocumentRoot "/var/www/html"
DirectoryIndex index.html index.htm

# ─── 5. Permisos de directorios ───
<Directory "/var/www/html">
    Options Indexes FollowSymLinks
    AllowOverride None
    Require all granted
</Directory>

# ─── 6. Logging ───
ErrorLog "logs/error_log"
LogLevel warn
CustomLog "logs/access_log" combined

# ─── 7. Inclusiones ───
IncludeOptional conf.d/*.conf
```

### 5.2 La directiva ServerName

Si no se configura `ServerName`, Apache intenta una resolución inversa del IP al
arrancar y genera una advertencia:

```
AH00558: Could not reliably determine the server's fully qualified domain name
```

**Solución**: Siempre configurar `ServerName`, incluso en labs:

```apache
# Para un servidor que no tiene DNS
ServerName localhost
```

---

## 6. Directivas fundamentales

### 6.1 Listen

Define en qué IP y puerto escucha Apache. Se puede especificar múltiples veces:

```apache
Listen 80                    # todas las interfaces, puerto 80
Listen 443                   # todas las interfaces, puerto 443
Listen 10.0.0.5:8080         # solo una IP específica
Listen [::]:80               # IPv6 todas las interfaces
```

> En Debian esto está en `/etc/apache2/ports.conf`, separado del archivo
> principal.

### 6.2 DocumentRoot

Define el directorio raíz desde donde se sirven archivos:

```apache
DocumentRoot "/var/www/html"

# La URL http://servidor/pagina.html busca:
# /var/www/html/pagina.html
```

Toda solicitud se mapea relativa a DocumentRoot. Un `DocumentRoot "/srv/web"`
convierte `http://host/img/logo.png` en `/srv/web/img/logo.png`.

### 6.3 Control de acceso con Require (2.4)

```apache
# Acceso público
Require all granted

# Sin acceso
Require all denied

# Solo red local
Require ip 192.168.1.0/24

# Solo usuarios autenticados
Require valid-user

# Usuario específico
Require user admin

# Combinaciones con RequireAll / RequireAny / RequireNone
<RequireAll>
    Require ip 10.0.0.0/8
    Require valid-user
</RequireAll>
```

### 6.4 Options

Controla qué funcionalidades están disponibles por directorio:

| Opción           | Efecto                                              |
|------------------|-----------------------------------------------------|
| `Indexes`        | Lista el contenido si no hay `DirectoryIndex`       |
| `FollowSymLinks` | Permite seguir symlinks                             |
| `SymLinksIfOwnerMatch` | Symlinks solo si mismo propietario             |
| `ExecCGI`        | Permite ejecución CGI                               |
| `MultiViews`     | Negociación de contenido                            |
| `None`           | Desactiva todas                                     |
| `All`            | Activa todas excepto `MultiViews`                   |

**Operadores `+` y `-`**: Permiten añadir o quitar opciones relativas al padre:

```apache
# Hereda del padre y solo añade/quita
<Directory "/var/www/html/uploads">
    Options +Indexes -FollowSymLinks
</Directory>
```

> **Seguridad**: `Indexes` nunca debería estar activo en producción. Si un
> directorio no tiene `index.html`, Apache mostraría el listado completo de
> archivos.

### 6.5 LogLevel

Controla la verbosidad del log de errores:

```apache
# Global
LogLevel warn

# Por módulo (Apache 2.4)
LogLevel warn ssl:info rewrite:trace3

# Niveles: emerg, alert, crit, error, warn, notice, info, debug, trace1-8
```

La capacidad de establecer nivel por módulo es muy útil para depurar mod_rewrite
o mod_proxy sin inundar el log con debug de otros módulos.

### 6.6 Formatos de log de acceso

```apache
# Formatos predefinidos
LogFormat "%h %l %u %t \"%r\" %>s %b" common
LogFormat "%h %l %u %t \"%r\" %>s %b \"%{Referer}i\" \"%{User-Agent}i\"" combined

# Uso
CustomLog "/var/log/httpd/access_log" combined
```

Desglose del formato `combined`:

```
192.168.1.50 - frank [21/Mar/2026:10:15:33 +0100] "GET /index.html HTTP/1.1" 200 1234 "http://ejemplo.com/" "Mozilla/5.0..."
│            │ │     │                               │                        │   │    │                      │
%h           %l %u   %t                              %r                       %>s %b   Referer                User-Agent
IP cliente   -  user timestamp                       request line             code size
```

---

## 7. Contenedores de contexto

Apache utiliza contenedores (secciones XML-like) para aplicar directivas solo a
contextos específicos. Se procesan en un orden de fusión determinista.

### 7.1 Tipos de contenedores

```apache
# Por ruta del sistema de archivos
<Directory "/var/www/html">
    # Aplica a este directorio y subdirectorios
</Directory>

<DirectoryMatch "^/var/www/.*/logs">
    # Expresión regular sobre rutas
</DirectoryMatch>

# Por ruta URL
<Location "/admin">
    # Aplica a URLs que comienzan con /admin
</Location>

<LocationMatch "^/api/v[0-9]+/">
    # Expresión regular sobre URLs
</LocationMatch>

# Por nombre de archivo
<Files ".htaccess">
    # Aplica a archivos con ese nombre
</Files>

<FilesMatch "\.(log|bak|old)$">
    # Expresión regular sobre nombres de archivo
</FilesMatch>
```

### 7.2 Orden de fusión (merge order)

Cuando múltiples contenedores aplican al mismo recurso, Apache los fusiona en
este orden (el último gana):

```
1. <Directory>         (de más corto a más largo, sin regex)
2. <DirectoryMatch>    (y <Directory ~ "regex">, en orden de aparición)
3. <Files>             (en orden de aparición)
4. <FilesMatch>        (en orden de aparición)
5. <Location>          (en orden de aparición)
6. <LocationMatch>     (en orden de aparición)
```

Esto significa que `<Location>` siempre prevalece sobre `<Directory>`:

```apache
# El Require de Location GANA
<Directory "/var/www/html/private">
    Require all denied
</Directory>

<Location "/private">
    Require all granted     # ← esto prevalece (¡cuidado!)
</Location>
```

### 7.3 Directory vs Location — cuándo usar cada uno

| Criterio         | `<Directory>`                  | `<Location>`                 |
|------------------|-------------------------------|------------------------------|
| Opera sobre      | Ruta del sistema de archivos  | Ruta URL                      |
| Útil para        | Permisos de archivos reales   | URLs virtuales, proxying     |
| AllowOverride    | Sí                            | No                           |
| Recomendado para | Contenido estático            | Endpoints de aplicación      |

**Regla práctica**: Usar `<Directory>` para proteger archivos. Usar `<Location>`
para URLs que no mapean directamente a archivos (proxy, status, redirecciones).

### 7.4 Condicionales con If (Apache 2.4)

```apache
<If "%{HTTP_HOST} == 'www.ejemplo.com'">
    Redirect permanent "/" "https://ejemplo.com/"
</If>

<If "-R '10.0.0.0/8'">
    # Solo si el cliente está en la red 10.x.x.x
    Header set X-Internal "true"
</If>
```

---

## 8. Gestión del servicio

### 8.1 Comandos de control

```bash
# Verificar sintaxis ANTES de aplicar cambios
apachectl configtest        # o httpd -t
# Syntax OK

# Verificar sintaxis con dump de VirtualHosts
apachectl -S                # muestra mapeo de VirtualHosts

# Recargar configuración (graceful — sin cortar conexiones activas)
systemctl reload apache2    # Debian
systemctl reload httpd      # RHEL

# Reiniciar (corta conexiones activas)
systemctl restart apache2

# Ver módulos cargados
apachectl -M                # o httpd -M

# Ver directivas compiladas
apachectl -V                # opciones de compilación
apachectl -L                # lista todas las directivas disponibles
```

### 8.2 Graceful vs Restart

```
             reload (graceful)                restart
             ─────────────────                ───────
Proceso      El padre señala a los            El padre mata todos
padre        hijos que terminen las           los hijos y arranca
             conexiones actuales              nuevos inmediatamente
             y luego se reciclen

Conexiones   No se interrumpen               Se cortan
activas

Config       Se relee completa               Se relee completa
nueva

Cuándo       Cambios de configuración         Cambios de módulos,
usar         en producción                    MPM, o si reload falla
```

### 8.3 Módulos de información (útiles para diagnóstico)

```apache
# Habilitar server-status (solo acceso local)
<Location "/server-status">
    SetHandler server-status
    Require local
</Location>

# Habilitar server-info (detalle de módulos y configuración)
<Location "/server-info">
    SetHandler server-info
    Require local
</Location>
```

```bash
# Consultar
curl http://localhost/server-status?auto
curl http://localhost/server-info
```

> **Seguridad**: Nunca exponer `/server-status` o `/server-info` públicamente.
> Revelan información sensible del servidor.

---

## 9. Diferencias Debian vs RHEL — resumen comparativo

| Aspecto                    | Debian/Ubuntu              | RHEL/Fedora               |
|----------------------------|----------------------------|---------------------------|
| Paquete                    | `apache2`                  | `httpd`                   |
| Servicio systemd           | `apache2`                  | `httpd`                   |
| Binario                    | `/usr/sbin/apache2`        | `/usr/sbin/httpd`         |
| Config principal           | `/etc/apache2/apache2.conf`| `/etc/httpd/conf/httpd.conf` |
| Config de puertos          | `ports.conf` (separado)    | Dentro de `httpd.conf`    |
| Variables de entorno       | `/etc/apache2/envvars`     | `/etc/sysconfig/httpd`    |
| Usuario                    | `www-data`                 | `apache`                  |
| VirtualHosts               | `sites-available/` + `sites-enabled/` | `conf.d/`      |
| Módulos                    | `mods-available/` + `mods-enabled/`   | `conf.modules.d/` |
| Herramientas               | `a2ensite`, `a2enmod`...   | Edición manual            |
| MPM por defecto            | event                      | event                     |
| Log de errores             | `/var/log/apache2/error.log` | `/var/log/httpd/error_log` |
| Log de acceso              | `/var/log/apache2/access.log` | `/var/log/httpd/access_log` |
| SELinux                    | No (AppArmor disponible)   | Sí, contextos `httpd_*`  |
| Página por defecto         | `index.html` en `/var/www/html` | `welcome.conf` en `conf.d/` |

---

## 10. Errores comunes

### Error 1: AH00558 — Could not reliably determine the server's FQDN

```
AH00558: apache2: Could not reliably determine the server's
fully qualified domain name, using 127.0.1.1
```

**Causa**: No se configuró `ServerName`.

**Solución**:

```bash
# Debian: crear fragmento dedicado
echo "ServerName localhost" > /etc/apache2/conf-available/servername.conf
a2enconf servername
systemctl reload apache2

# RHEL: añadir al httpd.conf
echo "ServerName localhost" >> /etc/httpd/conf/httpd.conf
systemctl reload httpd
```

### Error 2: AH00072 — Address already in use

```
AH00072: make_sock: could not bind to address [::]:80
```

**Causa**: Otro proceso (nginx, otro Apache, contenedor) ya usa el puerto 80.

**Solución**:

```bash
ss -tlnp | grep ':80'     # identificar el proceso
# Detener el otro servicio o cambiar el puerto en Listen
```

### Error 3: 403 Forbidden tras mover DocumentRoot

**Causa**: El nuevo directorio no tiene el `<Directory>` correspondiente con
`Require all granted`, o en RHEL, SELinux bloquea el acceso.

**Solución**:

```apache
# 1. Añadir bloque Directory
<Directory "/srv/mi-web">
    Require all granted
</Directory>
```

```bash
# 2. SELinux (RHEL)
semanage fcontext -a -t httpd_sys_content_t "/srv/mi-web(/.*)?"
restorecon -Rv /srv/mi-web
```

### Error 4: Cambios en configuración no surten efecto

**Causa habitual en Debian**: Se editó el archivo en `sites-available/` pero no
se habilitó con `a2ensite`, o se olvidó el reload.

**Causa habitual en RHEL**: El archivo en `conf.d/` no termina en `.conf` (la
directiva `IncludeOptional conf.d/*.conf` solo incluye archivos con esa
extensión).

### Error 5: Mezclar directivas 2.2 y 2.4

```apache
# INCORRECTO — sintaxis 2.2
<Directory "/var/www/html">
    Order allow,deny
    Allow from all
</Directory>

# CORRECTO — sintaxis 2.4
<Directory "/var/www/html">
    Require all granted
</Directory>
```

**Predicción**: Si se usan directivas 2.2 sin `mod_access_compat`, Apache
rechazará la configuración con error de sintaxis. Si el módulo está cargado,
funcionará pero generará inconsistencias si se mezclan ambas sintaxis en el mismo
bloque.

---

## 11. Cheatsheet

```
┌─────────────────────────────────────────────────────────────────┐
│              APACHE HTTP SERVER — REFERENCIA RÁPIDA             │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  INSTALACIÓN:                                                   │
│  Debian: apt install apache2    RHEL: dnf install httpd         │
│                                                                 │
│  SERVICIO:                                                      │
│  Debian: systemctl {start|stop|reload|restart} apache2          │
│  RHEL:   systemctl {start|stop|reload|restart} httpd            │
│                                                                 │
│  VERIFICACIONES:                                                │
│  apachectl configtest          validar sintaxis                 │
│  apachectl -S                  dump de VirtualHosts             │
│  apachectl -M                  módulos cargados                 │
│  apachectl -V                  opciones de compilación          │
│  apachectl -V | grep MPM       MPM activo                      │
│                                                                 │
│  HERRAMIENTAS DEBIAN:                                           │
│  a2ensite/a2dissite NOMBRE     habilitar/deshabilitar sitio     │
│  a2enmod/a2dismod NOMBRE       habilitar/deshabilitar módulo    │
│  a2enconf/a2disconf NOMBRE     habilitar/deshabilitar config    │
│                                                                 │
│  ARCHIVOS CLAVE:                                                │
│  Debian: /etc/apache2/apache2.conf                              │
│          /etc/apache2/sites-available/*.conf                    │
│          /etc/apache2/mods-available/*.{load,conf}              │
│  RHEL:   /etc/httpd/conf/httpd.conf                             │
│          /etc/httpd/conf.d/*.conf                               │
│          /etc/httpd/conf.modules.d/*.conf                       │
│                                                                 │
│  DIRECTIVAS ESENCIALES:                                         │
│  Listen 80                     puerto de escucha                │
│  ServerName host:port          nombre del servidor              │
│  DocumentRoot "/path"          raíz de documentos               │
│  DirectoryIndex index.html     archivo por defecto              │
│  ErrorLog "path"               log de errores                   │
│  CustomLog "path" combined     log de acceso                    │
│  LogLevel warn                 verbosidad (warn/info/debug)     │
│  LogLevel warn modulo:debug    debug por módulo                 │
│                                                                 │
│  CONTROL DE ACCESO (2.4):                                       │
│  Require all granted           acceso público                   │
│  Require all denied            sin acceso                       │
│  Require ip 10.0.0.0/8        solo red específica              │
│  Require valid-user            requiere autenticación           │
│                                                                 │
│  CONTENEDORES:                                                  │
│  <Directory "/path">           por ruta de filesystem           │
│  <Location "/url">             por ruta URL                     │
│  <Files "name">                por nombre de archivo            │
│  *Match variantes aceptan regex                                 │
│                                                                 │
│  ORDEN DE FUSIÓN:                                               │
│  Directory < DirectoryMatch < Files < FilesMatch                │
│           < Location < LocationMatch                            │
│  (el último gana en caso de conflicto)                          │
│                                                                 │
│  MPMs:                                                          │
│  prefork: 1 proceso/conexión (legacy, mod_php)                  │
│  worker:  threads por proceso                                   │
│  event:   threads + listener async (recomendado)                │
│                                                                 │
│  LOGS:                                                          │
│  Debian: /var/log/apache2/{access,error}.log                    │
│  RHEL:   /var/log/httpd/{access_log,error_log}                  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: Instalación y configuración inicial

1. Instala Apache en tu distribución.
2. Verifica qué MPM está activo. Si no es `event`, cámbialo.
3. Configura `ServerName localhost` para eliminar la advertencia AH00558.
4. Crea el archivo `/var/www/html/test.html` con contenido básico.
5. Verifica la sintaxis con `apachectl configtest`.
6. Accede a `http://localhost/test.html` con `curl` y verifica el código 200.
7. Revisa el log de acceso y confirma que la petición quedó registrada.
8. Lista los módulos cargados y cuenta cuántos hay activos.

**Pregunta de predicción**: Si ejecutas `apachectl -S` y no has configurado
ningún VirtualHost, ¿qué mostrará en la sección de VirtualHosts? ¿Se negará a
funcionar o usará una configuración implícita?

### Ejercicio 2: Estructura de archivos y diferencias

1. Identifica tu distribución y localiza:
   - El archivo principal de configuración.
   - El directorio donde irían los VirtualHosts.
   - El archivo que define en qué puertos escucha Apache.
   - El directorio de logs.
2. Ejecuta `apachectl -V` y anota: `Server MPM`, `Server Root`, `HTTPD_ROOT`,
   `SERVER_CONFIG_FILE`.
3. Sigue el flujo de inclusión: partiendo del archivo principal, identifica todas
   las directivas `Include`/`IncludeOptional` y dibuja el árbol de archivos que
   se cargan.
4. Cambia el `DocumentRoot` a `/srv/mi-web/`, crea un `index.html` en ese
   directorio, añade el bloque `<Directory>` correspondiente, y verifica.
5. Si estás en RHEL/Fedora, resuelve el bloqueo de SELinux que aparecerá.

**Pregunta de predicción**: En Debian, si creas un archivo
`/etc/apache2/sites-available/nuevo.conf` pero no ejecutas `a2ensite nuevo`,
¿Apache cargará esa configuración? ¿Qué sucede internamente?

### Ejercicio 3: Contenedores y orden de fusión

Crea la siguiente configuración y predice qué sucederá antes de probar:

1. Configura un directorio `/var/www/html/test/` con un archivo `index.html` y un
   archivo `secret.txt`.
2. Aplica estas directivas:

```apache
<Directory "/var/www/html/test">
    Options Indexes
    Require all granted
</Directory>

<Files "secret.txt">
    Require all denied
</Files>

<Location "/test">
    Options -Indexes
</Location>
```

3. Predice: ¿Podrás acceder a `http://localhost/test/`? ¿Se mostrará el listado
   de archivos o un 403? ¿Y `http://localhost/test/secret.txt`?
4. Prueba con `curl` y verifica tus predicciones.
5. Invierte el `Require` del `<Directory>` a `denied` y el de `<Files>` a
   `granted`. ¿`secret.txt` será accesible? Razona según el orden de fusión.

**Pregunta de reflexión**: El orden de fusión dice que `<Location>` prevalece
sobre `<Directory>`. Pero `<Location>` en este ejemplo solo cambia `Options`,
no `Require`. ¿La directiva `Require all granted` del `<Directory>` sigue
aplicando? ¿Por qué la fusión es por directiva individual y no reemplaza el
bloque completo?

---

> **Siguiente tema**: T02 — Virtual Hosts (name-based, configuración, DocumentRoot).
