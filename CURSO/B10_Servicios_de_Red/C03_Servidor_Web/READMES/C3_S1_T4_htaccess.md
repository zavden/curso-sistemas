# .htaccess en Apache

## Índice

1. [Qué es .htaccess](#1-qué-es-htaccess)
2. [Cómo funciona internamente](#2-cómo-funciona-internamente)
3. [AllowOverride — control del administrador](#3-allowoverride--control-del-administrador)
4. [Directivas comunes en .htaccess](#4-directivas-comunes-en-htaccess)
5. [Impacto en rendimiento](#5-impacto-en-rendimiento)
6. [Alternativas a .htaccess](#6-alternativas-a-htaccess)
7. [Migración de .htaccess a configuración del servidor](#7-migración-de-htaccess-a-configuración-del-servidor)
8. [Seguridad de .htaccess](#8-seguridad-de-htaccess)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Qué es .htaccess

`.htaccess` (hypertext access) es un archivo de configuración descentralizada
que permite modificar el comportamiento de Apache **por directorio**, sin acceso
al archivo principal del servidor ni necesidad de reiniciar el servicio.

```
Configuración centralizada              Configuración descentralizada
(httpd.conf / VirtualHost)              (.htaccess)
──────────────────────────              ─────────────────────────────
Editada por el sysadmin                 Editada por el usuario/desarrollador
Requiere reload/restart                 Efecto inmediato (sin reinicio)
Se lee una vez al iniciar               Se lee en CADA petición
Contexto server/vhost                   Contexto directory
Rendimiento óptimo                      Penalización por I/O
```

### Caso de uso original

.htaccess nació para hosting compartido, donde múltiples usuarios comparten un
Apache y ninguno tiene acceso root ni al `httpd.conf`. Cada usuario controla su
propio espacio web colocando `.htaccess` dentro de su directorio.

### ¿Cuándo tiene sentido usar .htaccess?

| Escenario                              | ¿.htaccess?  | Razón                        |
|----------------------------------------|--------------|------------------------------|
| Hosting compartido sin acceso a config | **Sí**       | Única opción disponible      |
| Aplicación que distribuye reglas       | **Sí**       | WordPress, Laravel, etc.     |
| Servidor propio con acceso root        | **No**       | Usar `<Directory>` directo   |
| Entorno de desarrollo rápido           | Aceptable    | Conveniencia temporal        |
| Producción con control total           | **No**       | Rendimiento y mantenibilidad |

---

## 2. Cómo funciona internamente

### 2.1 Búsqueda recursiva por directorio

Cuando Apache recibe una petición, **antes de servir el archivo**, busca
`.htaccess` en cada directorio de la ruta, desde la raíz del filesystem hasta el
directorio del archivo solicitado:

```
Petición: GET /blog/2026/articulo.html
DocumentRoot: /var/www/ejemplo/public

Apache busca .htaccess en:
   /.htaccess                               ← generalmente no existe
   /var/.htaccess                           ← generalmente no existe
   /var/www/.htaccess                       ← generalmente no existe
   /var/www/ejemplo/.htaccess              ← podría existir
   /var/www/ejemplo/public/.htaccess       ← podría existir
   /var/www/ejemplo/public/blog/.htaccess  ← podría existir
   /var/www/ejemplo/public/blog/2026/.htaccess  ← podría existir
```

Cada `.htaccess` encontrado se fusiona con el anterior, con los más profundos
prevaleciendo sobre los superiores.

> **Punto clave**: Esta búsqueda ocurre en **cada petición HTTP**, incluyendo
> CSS, imágenes, JavaScript — no solo páginas HTML. Una página con 30 recursos
> dispara 30 búsquedas recursivas de `.htaccess`.

### 2.2 Fusión de directivas

Las directivas de `.htaccess` se fusionan como si fueran bloques `<Directory>`
correspondientes a cada nivel del path:

```
/var/www/ejemplo/public/.htaccess:
    Options -Indexes
    ErrorDocument 404 /not-found.html

/var/www/ejemplo/public/blog/.htaccess:
    Options +Indexes              ← sobreescribe Options del padre
                                   ← ErrorDocument 404 se hereda
```

### 2.3 AccessFileName

El nombre `.htaccess` es configurable (aunque rara vez se cambia):

```apache
# En httpd.conf
AccessFileName .htaccess

# Cambiar a otro nombre (ofuscación, no seguridad real)
AccessFileName .config
```

---

## 3. AllowOverride — control del administrador

`AllowOverride` es la directiva que el administrador del servidor usa dentro de
`<Directory>` para controlar **qué puede hacer** un `.htaccess`. Es el mecanismo
de seguridad que previene que un usuario en hosting compartido modifique
configuración crítica.

### 3.1 Valores de AllowOverride

```apache
<Directory "/var/www/html">
    AllowOverride VALOR
</Directory>
```

| Valor          | Permite en .htaccess                                     |
|----------------|----------------------------------------------------------|
| `None`         | .htaccess es ignorado completamente                      |
| `All`          | Todas las directivas permitidas                          |
| `AuthConfig`   | Autenticación: AuthType, AuthName, Require, etc.         |
| `FileInfo`     | Control de tipos: ErrorDocument, RewriteRule, Header, etc.|
| `Indexes`      | Listados de directorio: DirectoryIndex, Options Indexes  |
| `Limit`        | Control de acceso por host (legacy 2.2)                  |
| `Options`      | Directiva Options (se puede limitar con `=`)             |

Se pueden combinar:

```apache
# Solo permitir autenticación y reescritura
AllowOverride AuthConfig FileInfo

# Permitir Options pero solo FollowSymLinks y Indexes
AllowOverride Options=FollowSymLinks,Indexes
```

### 3.2 AllowOverrideList (Apache 2.4.7+)

Permite especificar directivas individuales en vez de categorías enteras:

```apache
<Directory "/var/www/html">
    AllowOverride None
    AllowOverrideList Redirect RedirectMatch RewriteEngine RewriteRule RewriteCond
</Directory>
```

Esto es más seguro que `AllowOverride FileInfo` porque solo permite las
directivas de reescritura específicas, no todas las de la categoría FileInfo.

### 3.3 Interacción con directivas del servidor

```
Prioridad (de menor a mayor):

1. httpd.conf (global)
2. <VirtualHost>
3. <Directory> (de más corto a más largo)
4. .htaccess (del directorio padre al actual)
5. <Location> / <Files> (siempre ganan)
```

Un `.htaccess` puede sobreescribir lo que `<Directory>` establece, pero nunca
lo que `<Location>` o `<Files>` impone:

```apache
# En httpd.conf — esta restricción NO puede ser anulada por .htaccess
<Files ".env">
    Require all denied
</Files>
```

---

## 4. Directivas comunes en .htaccess

### 4.1 Reescritura de URLs

El uso más frecuente de `.htaccess`. Requiere `AllowOverride FileInfo`:

```apache
# .htaccess
RewriteEngine On

# Front controller (WordPress, Laravel, Symfony)
RewriteCond %{REQUEST_FILENAME} !-f
RewriteCond %{REQUEST_FILENAME} !-d
RewriteRule ^(.*)$ index.php [QSA,L]
```

> **Diferencia de contexto**: En `.htaccess`, el patrón de RewriteRule no
> incluye el slash inicial. `/blog/post` se evalúa como `blog/post` (relativo
> al directorio del .htaccess). En `<VirtualHost>` o `<Directory>`, el patrón
> incluye el slash: `^/blog/post`.

### 4.2 Control de acceso

```apache
# Denegar acceso a archivos sensibles
<Files ".env">
    Require all denied
</Files>

<FilesMatch "\.(sql|log|bak)$">
    Require all denied
</FilesMatch>

# Restringir por IP
Require ip 192.168.1.0/24
```

### 4.3 Páginas de error personalizadas

```apache
ErrorDocument 404 /errors/not-found.html
ErrorDocument 403 /errors/forbidden.html
ErrorDocument 500 /errors/server-error.html

# Con mensaje inline (no recomendado para producción)
ErrorDocument 503 "Mantenimiento en curso"
```

### 4.4 Cabeceras de seguridad

```apache
Header always set X-Content-Type-Options "nosniff"
Header always set X-Frame-Options "DENY"
Header always set X-XSS-Protection "0"
Header always set Referrer-Policy "strict-origin-when-cross-origin"
```

### 4.5 Control de caché

```apache
<IfModule mod_expires.c>
    ExpiresActive On
    ExpiresByType image/jpeg "access plus 1 month"
    ExpiresByType text/css "access plus 1 week"
    ExpiresByType application/javascript "access plus 1 week"
</IfModule>
```

### 4.6 Compresión

```apache
<IfModule mod_deflate.c>
    AddOutputFilterByType DEFLATE text/html text/plain text/css
    AddOutputFilterByType DEFLATE application/javascript application/json
</IfModule>
```

### 4.7 Autenticación básica

```apache
AuthType Basic
AuthName "Area Restringida"
AuthUserFile /etc/apache2/.htpasswd
Require valid-user
```

```bash
# Crear archivo de contraseñas
htpasswd -c /etc/apache2/.htpasswd usuario1
htpasswd /etc/apache2/.htpasswd usuario2    # sin -c para añadir
```

> **Seguridad**: `AuthUserFile` debe estar **fuera** del DocumentRoot. Si se
> coloca dentro, podría ser descargado por un cliente.

### 4.8 Directorio index y opciones

```apache
# Cambiar archivo por defecto
DirectoryIndex home.html index.php

# Desactivar listado de directorio
Options -Indexes

# Permitir seguir symlinks
Options +FollowSymLinks
```

---

## 5. Impacto en rendimiento

### 5.1 El coste de la búsqueda recursiva

El impacto principal es el I/O de disco. Por cada petición, Apache hace
múltiples llamadas `stat()` al sistema de archivos:

```
Petición: GET /images/logo.png
DocumentRoot: /var/www/html

Llamadas stat() generadas:
  stat("/.htaccess")                          → ENOENT
  stat("/var/.htaccess")                      → ENOENT
  stat("/var/www/.htaccess")                  → ENOENT
  stat("/var/www/html/.htaccess")             → encontrado, open() + read()
  stat("/var/www/html/images/.htaccess")      → ENOENT
  stat("/var/www/html/images/logo.png")       → encontrado, servir

= 6 llamadas al sistema solo para verificar .htaccess
```

Con `AllowOverride None`, Apache sabe que no necesita buscar `.htaccess` y
elimina todas esas llamadas `stat()`:

```
Con AllowOverride None:
  stat("/var/www/html/images/logo.png")       → encontrado, servir

= 1 llamada al sistema
```

### 5.2 Cuantificación del impacto

```
Escenario: Página web con 50 recursos (HTML + CSS + JS + imágenes)
Profundidad media: 5 niveles de directorio
AllowOverride All: 50 × 5 = 250 llamadas stat() adicionales por página

En un servidor con 100 req/s:
  250 × 100 = 25,000 stat() adicionales por segundo

Con AllowOverride None:
  0 stat() adicionales
```

En servidores con almacenamiento lento (NFS, spinning disks) o bajo carga alta,
esta diferencia es medible. En SSD con poco tráfico, el impacto es menor pero
sigue existiendo.

### 5.3 Otros costes de rendimiento

Además del I/O, `.htaccess` tiene costes indirectos:

| Coste                    | Descripción                                    |
|--------------------------|------------------------------------------------|
| Parsing repetido         | El archivo se parsea en cada petición           |
| Sin caché de contenido   | Apache no cachea el contenido de .htaccess      |
| Herencia acumulativa     | Cada nivel fusiona, aumentando procesamiento    |
| Dificultad de diagnóstico| Configuración dispersa, difícil de rastrear     |

### 5.4 Benchmark comparativo

```bash
# Medir con AllowOverride All (y .htaccess con RewriteRules)
ab -n 10000 -c 50 http://localhost/

# Mover las reglas al VirtualHost y poner AllowOverride None
ab -n 10000 -c 50 http://localhost/

# Comparar: Requests per second, Time per request
```

---

## 6. Alternativas a .htaccess

### 6.1 Directiva <Directory> en el VirtualHost

La alternativa directa. Todo lo que `.htaccess` puede hacer, `<Directory>`
también puede:

```apache
# En vez de /var/www/ejemplo/public/.htaccess con:
#   RewriteEngine On
#   RewriteCond %{REQUEST_FILENAME} !-f
#   RewriteRule ^(.*)$ index.php [QSA,L]

# Usar en el VirtualHost:
<VirtualHost *:80>
    ServerName ejemplo.com
    DocumentRoot "/var/www/ejemplo/public"

    <Directory "/var/www/ejemplo/public">
        AllowOverride None

        RewriteEngine On
        RewriteCond %{REQUEST_FILENAME} !-f
        RewriteCond %{REQUEST_FILENAME} !-d
        RewriteRule ^(.*)$ /index.php [QSA,L]
    </Directory>
</VirtualHost>
```

> **Diferencia de sintaxis**: En `<Directory>`, el patrón de RewriteRule incluye
> el slash inicial (`^/(.*)$`). Pero si se usa `RewriteBase /`, el
> comportamiento se iguala al de `.htaccess`. En la práctica, `^(.*)$` sin slash
> suele funcionar en ambos contextos en Apache 2.4.

### 6.2 Include para organización modular

Si la configuración se hace compleja, fragmentar con Include:

```apache
# /etc/apache2/conf-available/php-rewrite.conf
RewriteEngine On
RewriteCond %{REQUEST_FILENAME} !-f
RewriteCond %{REQUEST_FILENAME} !-d
RewriteRule ^(.*)$ /index.php [QSA,L]

# En el VirtualHost
<Directory "/var/www/ejemplo/public">
    AllowOverride None
    Include /etc/apache2/conf-available/php-rewrite.conf
</Directory>
```

### 6.3 Tabla comparativa

| Aspecto              | .htaccess                     | <Directory> en VHost          |
|----------------------|-------------------------------|-------------------------------|
| Acceso root          | No necesario                  | Requiere root/sudo            |
| Efecto               | Inmediato                     | Requiere reload               |
| Rendimiento          | Penalización por petición     | Sin overhead adicional        |
| Visibilidad          | Disperso, difícil de auditar  | Centralizado                  |
| Versionado           | Se puede versionar con la app | Se versiona con el servidor   |
| Errores de sintaxis  | 500 Internal Server Error     | Apache no arranca (detectable)|
| Debugging            | Difícil (¿cuál .htaccess?)    | Localización clara            |

### 6.4 ¿Cuándo mantener .htaccess?

Hay dos casos legítimos para mantener `.htaccess`:

**1. Frameworks que lo requieren**: WordPress, Laravel, Symfony y otros
frameworks PHP distribuyen `.htaccess` como parte de la aplicación. Eliminarlos
requiere replicar sus reglas en el VirtualHost, lo cual funciona pero complica
las actualizaciones del framework.

**2. Delegación intencional**: En entornos educativos o de desarrollo, donde
múltiples personas administran subdirectorios sin acceso al servidor:

```apache
# Administrador permite .htaccess solo donde sea necesario
<Directory "/var/www/html">
    AllowOverride None              # por defecto: sin .htaccess
</Directory>

<Directory "/var/www/html/users">
    AllowOverride AuthConfig FileInfo   # solo para directorios de usuarios
</Directory>
```

---

## 7. Migración de .htaccess a configuración del servidor

### 7.1 Proceso paso a paso

```
1. Identificar todos los .htaccess activos
2. Para cada uno, copiar las directivas al <Directory> correspondiente
3. Ajustar la sintaxis (slash en RewriteRule)
4. Establecer AllowOverride None
5. Verificar con apachectl configtest
6. Aplicar con reload
7. Probar funcionalidad
8. Eliminar los archivos .htaccess
```

### 7.2 Encontrar todos los .htaccess

```bash
find /var/www -name ".htaccess" -type f
# /var/www/ejemplo/public/.htaccess
# /var/www/ejemplo/public/admin/.htaccess
# /var/www/blog/public/.htaccess
```

### 7.3 Ejemplo de migración completa

**Antes** — tres archivos `.htaccess`:

```apache
# /var/www/ejemplo/public/.htaccess
RewriteEngine On
RewriteCond %{REQUEST_FILENAME} !-f
RewriteCond %{REQUEST_FILENAME} !-d
RewriteRule ^(.*)$ index.php [QSA,L]

Options -Indexes
ErrorDocument 404 /not-found.html
```

```apache
# /var/www/ejemplo/public/uploads/.htaccess
# Deshabilitar ejecución de scripts en uploads
<FilesMatch "\.(php|phtml|php3|php4|php5)$">
    Require all denied
</FilesMatch>
```

```apache
# /var/www/ejemplo/public/admin/.htaccess
AuthType Basic
AuthName "Admin"
AuthUserFile /etc/apache2/.htpasswd
Require valid-user
```

**Después** — todo en el VirtualHost:

```apache
<VirtualHost *:80>
    ServerName ejemplo.com
    DocumentRoot "/var/www/ejemplo/public"

    <Directory "/var/www/ejemplo/public">
        AllowOverride None
        Require all granted
        Options -Indexes

        RewriteEngine On
        RewriteCond %{REQUEST_FILENAME} !-f
        RewriteCond %{REQUEST_FILENAME} !-d
        RewriteRule ^(.*)$ /index.php [QSA,L]

        ErrorDocument 404 /not-found.html
    </Directory>

    <Directory "/var/www/ejemplo/public/uploads">
        <FilesMatch "\.(php|phtml|php3|php4|php5)$">
            Require all denied
        </FilesMatch>
    </Directory>

    <Directory "/var/www/ejemplo/public/admin">
        AuthType Basic
        AuthName "Admin"
        AuthUserFile /etc/apache2/.htpasswd
        Require valid-user
    </Directory>

    ErrorLog ${APACHE_LOG_DIR}/ejemplo-error.log
    CustomLog ${APACHE_LOG_DIR}/ejemplo-access.log combined
</VirtualHost>
```

### 7.4 Verificar la migración

```bash
# 1. Validar sintaxis
apachectl configtest

# 2. Verificar con -S que el VHost sigue correcto
apachectl -S

# 3. Recargar
systemctl reload apache2

# 4. Probar cada funcionalidad que tenía .htaccess
curl -v http://ejemplo.com/                    # front controller
curl -v http://ejemplo.com/no-existe           # ErrorDocument 404
curl -v http://ejemplo.com/uploads/malware.php # debe ser 403
curl -v http://ejemplo.com/admin/              # debe pedir autenticación
```

---

## 8. Seguridad de .htaccess

### 8.1 Proteger el archivo .htaccess

Apache incluye por defecto una regla que impide descargar `.htaccess`:

```apache
# Ya incluido en la configuración base de Apache
<Files ".ht*">
    Require all denied
</Files>
```

Esto protege `.htaccess`, `.htpasswd` y cualquier archivo que empiece con `.ht`.
**Nunca eliminar esta regla.**

### 8.2 Riesgos de AllowOverride All

Con `AllowOverride All`, un atacante que logre escribir un `.htaccess` (por
ejemplo, a través de una vulnerabilidad de upload) puede:

| Acción maliciosa                         | Directiva usada                    |
|------------------------------------------|------------------------------------|
| Ejecutar PHP en directorios de uploads   | `AddHandler application/x-httpd-php .jpg` |
| Redirigir tráfico a sitio malicioso      | `RewriteRule` con `[R]`            |
| Exponer archivos del servidor            | `Options +Indexes`                 |
| Crear backdoor de autenticación          | `Satisfy Any` (2.2) / `Require`    |
| Listar directorios sensibles             | `Options +Indexes +FollowSymLinks` |

**Mitigación**: Usar `AllowOverride None` siempre que sea posible. Si es
necesario, usar `AllowOverrideList` para permitir solo directivas específicas.

### 8.3 .htaccess en directorios de upload

Los directorios donde los usuarios suben archivos son el vector de ataque más
común. La protección debe estar en la configuración del servidor, no en un
`.htaccess` que el atacante podría sobreescribir:

```apache
# En el VirtualHost (no en .htaccess)
<Directory "/var/www/ejemplo/public/uploads">
    AllowOverride None              # ignorar cualquier .htaccess

    # Desactivar ejecución de scripts
    php_admin_flag engine off       # si usa mod_php
    RemoveHandler .php .phtml       # eliminar handler PHP
    RemoveType .php .phtml

    <FilesMatch "\.ph(p[0-9]?|tml)$">
        Require all denied
    </FilesMatch>

    # Solo permitir tipos de archivo seguros
    # ForceType application/octet-stream
</Directory>
```

---

## 9. Errores comunes

### Error 1: 500 Internal Server Error por sintaxis en .htaccess

**Causa**: Cualquier error de sintaxis en `.htaccess` produce un error 500 para
todo el directorio y subdirectorios. A diferencia del `httpd.conf` (que impide
arrancar Apache), un `.htaccess` roto no genera advertencia — solo errores 500.

**Diagnóstico**:

```bash
tail /var/log/apache2/error.log
# .htaccess: Invalid command 'RewritEngne', ...
```

**Prevención**: No hay `configtest` para `.htaccess`. La única verificación
es probar tras cada cambio.

### Error 2: RewriteRule no funciona en .htaccess

**Causa más frecuente**: `AllowOverride` no incluye `FileInfo`:

```apache
# Si AllowOverride es None o no incluye FileInfo,
# las directivas Rewrite en .htaccess se ignoran silenciosamente
```

**Causa secundaria**: Falta `RewriteEngine On` en el `.htaccess`. A diferencia
de la configuración del servidor (donde puede heredarse), en `.htaccess` debe
declararse explícitamente en cada archivo que use reglas de reescritura.

### Error 3: .htaccess ignorado completamente

**Causa**: `AllowOverride None` en el `<Directory>` que contiene el archivo.

**Diagnóstico**:

```bash
# Ver la configuración efectiva
apachectl -S

# Buscar AllowOverride en los archivos de configuración
grep -rn "AllowOverride" /etc/apache2/    # Debian
grep -rn "AllowOverride" /etc/httpd/      # RHEL
```

**Trampa habitual en Debian**: `apache2.conf` incluye:

```apache
<Directory /var/www/>
    AllowOverride None    # ← bloquea .htaccess globalmente
</Directory>
```

Hay que cambiarlo o sobreescribirlo en el `<Directory>` del VirtualHost.

### Error 4: .htaccess de subdirectorio no hereda del padre

**Causa**: El `.htaccess` del subdirectorio redefine `Options` o `RewriteEngine`
y sobreescribe la configuración del padre.

```apache
# /var/www/html/.htaccess
RewriteEngine On
RewriteRule ^(.*)$ /index.php [QSA,L]

# /var/www/html/api/.htaccess
# ← No incluye RewriteEngine On
# Las reglas del padre NO se heredan automáticamente
# RewriteEngine vuelve a Off en este nivel
```

**Solución**: Cada `.htaccess` que use RewriteRule debe incluir `RewriteEngine On`
explícitamente.

### Error 5: AllowOverride All con aplicación vulnerable

**Escenario**: Aplicación PHP con vulnerabilidad de upload. El atacante sube un
`.htaccess` que habilita ejecución PHP en el directorio de uploads:

```apache
# .htaccess malicioso subido por el atacante
AddHandler application/x-httpd-php .jpg
```

Ahora cualquier archivo `.jpg` se ejecuta como PHP, permitiendo webshells.

**Solución**: `AllowOverride None` en directorios de upload, configurado en el
VirtualHost (donde el atacante no puede modificarlo).

---

## 10. Cheatsheet

```
┌─────────────────────────────────────────────────────────────────┐
│                .htaccess — REFERENCIA RÁPIDA                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  QUÉ ES:                                                        │
│  Configuración por directorio, leída en cada petición           │
│  Controlada por AllowOverride en <Directory>                    │
│                                                                 │
│  ALLOWOVERRIDE:                                                 │
│  None ........... .htaccess ignorado (RECOMENDADO)              │
│  All ............ todo permitido (riesgo de seguridad)          │
│  AuthConfig ..... autenticación (AuthType, Require)             │
│  FileInfo ....... reescritura, errores (Rewrite, ErrorDoc)      │
│  Indexes ........ DirectoryIndex, Options Indexes               │
│  Options[=X,Y] .. Options solo con las listadas                 │
│  AllowOverrideList Dir1 Dir2 .. directivas individuales         │
│                                                                 │
│  DIRECTIVAS FRECUENTES:                                         │
│  RewriteEngine On                 activar reescritura           │
│  RewriteCond %{REQUEST_FILENAME} !-f  si no es archivo          │
│  RewriteRule ^(.*)$ index.php [QSA,L] front controller          │
│  Options -Indexes                 sin listado de directorio     │
│  ErrorDocument 404 /error.html    página de error               │
│  Header set X-Frame-Options DENY  cabecera de seguridad         │
│  AuthType Basic                   autenticación HTTP            │
│  Require valid-user               requiere login                │
│                                                                 │
│  CONTEXTO VS SERVIDOR:                                          │
│  .htaccess: patrón SIN slash → ^blog/post$                     │
│  <Directory>: patrón CON slash → ^/blog/post$                  │
│  (en Apache 2.4 ambos suelen funcionar sin slash)               │
│                                                                 │
│  IMPACTO EN RENDIMIENTO:                                        │
│  Con AllowOverride All:                                         │
│    stat() por cada directorio en la ruta, cada petición         │
│    50 recursos × 5 niveles = 250 stat() extra por pageview      │
│  Con AllowOverride None:                                        │
│    0 stat() extra                                               │
│                                                                 │
│  CUÁNDO USAR:                                                   │
│  ✓ Hosting compartido sin acceso a httpd.conf                   │
│  ✓ Frameworks que lo distribuyen (WordPress, Laravel)           │
│  ✗ Servidor propio → usar <Directory> en VirtualHost            │
│  ✗ Producción con acceso root → migrar a config del servidor    │
│                                                                 │
│  MIGRACIÓN A <DIRECTORY>:                                       │
│  1. find /var/www -name ".htaccess"                             │
│  2. Copiar directivas al <Directory> correspondiente            │
│  3. AllowOverride None                                          │
│  4. apachectl configtest && systemctl reload apache2            │
│  5. Probar cada funcionalidad                                   │
│  6. Eliminar archivos .htaccess                                 │
│                                                                 │
│  SEGURIDAD:                                                     │
│  Protección por defecto: <Files ".ht*"> Require all denied      │
│  Directorios de upload: AllowOverride None en <Directory>       │
│  Nunca AllowOverride All en producción si puedes evitarlo       │
│                                                                 │
│  DIAGNÓSTICO:                                                   │
│  Error 500 → revisar error.log para errores de sintaxis         │
│  .htaccess ignorado → buscar AllowOverride None                 │
│  Rewrite no funciona → AllowOverride FileInfo necesario         │
│  No hay configtest → probar cada cambio inmediatamente          │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: .htaccess y AllowOverride

1. Crea un VirtualHost para `htaccess-lab.local` con `AllowOverride None`.
2. Crea un `.htaccess` en el DocumentRoot con `Options +Indexes`.
3. Accede a un directorio sin `index.html` — ¿se muestra el listado o 403?
4. Cambia `AllowOverride` a `Indexes`. Recarga Apache y prueba de nuevo.
5. Cambia a `AllowOverride FileInfo` y añade `RewriteEngine On` con una regla
   al `.htaccess`. ¿Funciona la regla? ¿Y `Options +Indexes`?
6. Cambia a `AllowOverrideList Options RewriteEngine RewriteRule RewriteCond`.
   ¿Funcionan las reglas de reescritura? ¿Y `ErrorDocument 404`?

**Pregunta de predicción**: Con `AllowOverride FileInfo`, ¿qué sucede si el
`.htaccess` incluye una directiva `AuthType Basic` (categoría AuthConfig)?
¿Apache la ignora silenciosamente o devuelve un error 500?

### Ejercicio 2: Impacto en rendimiento

1. Configura un VirtualHost con `AllowOverride All` y un `.htaccess` con
   `RewriteEngine On` y una regla simple de front controller.
2. Crea 10 archivos estáticos pequeños (CSS, imágenes) en el DocumentRoot.
3. Ejecuta un benchmark: `ab -n 5000 -c 20 http://htaccess-lab.local/img1.png`
4. Anota los resultados: Requests/sec, Time per request.
5. Migra las reglas del `.htaccess` al `<Directory>` del VirtualHost.
6. Establece `AllowOverride None`.
7. Repite el benchmark con los mismos parámetros.
8. Compara los resultados. ¿Cuál es la diferencia porcentual?

**Pregunta de predicción**: Si el archivo solicitado está a 6 niveles de
profundidad (`/a/b/c/d/e/f/file.png`) y `AllowOverride All`, ¿cuántas llamadas
`stat()` adicionales genera cada petición solo para buscar `.htaccess`?

### Ejercicio 3: Migración y seguridad

Escenario: Tienes una aplicación PHP con esta estructura:

```
/var/www/app/public/
├── .htaccess              (front controller + opciones)
├── index.php
├── uploads/
│   └── .htaccess          (bloqueo de PHP)
└── admin/
    └── .htaccess          (autenticación básica)
```

1. Lee el contenido de cada `.htaccess` (inventa directivas razonables).
2. Escribe la configuración equivalente como bloques `<Directory>` dentro del
   VirtualHost.
3. Establece `AllowOverride None` para todo el árbol.
4. Verifica con `apachectl configtest`.
5. Prueba que la funcionalidad es idéntica.
6. Ahora simula un ataque: intenta crear un `.htaccess` en `uploads/` con
   `AddHandler application/x-httpd-php .jpg`. ¿Funciona con `AllowOverride None`?

**Pregunta de reflexión**: Un framework como WordPress distribuye su `.htaccess`
con las reglas de reescritura necesarias. Si migras esas reglas al VirtualHost
y eliminas el `.htaccess`, ¿qué sucede cuando WordPress se actualiza y regenera
su `.htaccess`? ¿Se producirá un conflicto o convivencia? ¿Qué efecto tiene
`AllowOverride None` en este escenario?

---

> **Fin de la Sección 1 — Apache HTTP Server.** Siguiente sección: S02 — Nginx
> (T01 — Instalación y estructura: nginx.conf, server blocks, location).
