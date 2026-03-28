# Squid básico

## Índice

1. [¿Qué es Squid?](#1-qué-es-squid)
2. [Instalación y archivos](#2-instalación-y-archivos)
3. [squid.conf: estructura general](#3-squidconf-estructura-general)
4. [ACLs: el sistema de control de acceso](#4-acls-el-sistema-de-control-de-acceso)
5. [http_access: aplicar las ACLs](#5-http_access-aplicar-las-acls)
6. [Caché](#6-caché)
7. [Autenticación](#7-autenticación)
8. [Logging](#8-logging)
9. [Gestión del servicio](#9-gestión-del-servicio)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. ¿Qué es Squid?

Squid es el proxy forward más utilizado en Linux. Lleva décadas en producción y es capaz de manejar miles de conexiones concurrentes con caché eficiente.

```
┌─────────────────────────────────────────────────┐
│                    Squid                        │
│                                                 │
│  ┌───────────┐  ┌────────────┐  ┌────────────┐ │
│  │   ACLs    │  │   Caché    │  │    Auth    │ │
│  │ (quién    │  │ (disco +   │  │ (LDAP,     │ │
│  │  puede    │  │  memoria)  │  │  NCSA,     │ │
│  │  qué)     │  │            │  │  Kerberos) │ │
│  └───────────┘  └────────────┘  └────────────┘ │
│                                                 │
│  Puerto por defecto: 3128                       │
│  Protocolos: HTTP, HTTPS (CONNECT), FTP         │
└─────────────────────────────────────────────────┘
```

### Qué puede hacer Squid

| Función | Descripción |
|---|---|
| **Forward proxy** | Intermediario para tráfico saliente |
| **Caché HTTP** | Almacena respuestas para reutilizarlas |
| **Filtrado de acceso** | ACLs por IP, dominio, horario, usuario |
| **Autenticación** | LDAP, NCSA (htpasswd), Kerberos, RADIUS |
| **Logging** | Registro detallado de toda la navegación |
| **HTTPS tunneling** | Método CONNECT para tráfico cifrado |
| **Proxy transparente** | Intercepta tráfico sin config en el cliente |
| **Delay pools** | Control de ancho de banda |

---

## 2. Instalación y archivos

### Instalación

```bash
# RHEL/Fedora
dnf install squid

# Debian/Ubuntu
apt install squid
```

### Archivos y directorios

```
/etc/squid/squid.conf              ← configuración principal
/etc/squid/conf.d/                 ← drop-ins (incluidos por squid.conf)
/etc/squid/errorpage.css           ← estilos de páginas de error

/var/spool/squid/                  ← caché en disco
/var/log/squid/access.log          ← log de accesos
/var/log/squid/cache.log           ← log del proceso squid (errores, arranque)
/var/run/squid.pid                 ← PID del proceso

/usr/lib64/squid/                  ← helpers (autenticación, ACLs externas)
/usr/share/squid/errors/           ← plantillas de páginas de error
```

### Primer arranque

```bash
# Habilitar y arrancar
systemctl enable --now squid

# Verificar
systemctl status squid
ss -tlnp | grep 3128
# LISTEN  0  256  *:3128  *:*  users:(("squid",pid=...))

# Probar desde el mismo servidor
curl -x http://localhost:3128 http://example.com
# Si devuelve HTML → Squid funciona

# Probar desde un cliente
curl -x http://10.0.0.1:3128 http://example.com
# Probablemente denegado → la config por defecto solo permite localhost
```

---

## 3. squid.conf: estructura general

El archivo de configuración por defecto de Squid es largo (miles de líneas con comentarios). La configuración efectiva suele ser de 20-50 líneas.

```bash
# Ver solo las líneas activas (sin comentarios ni vacías)
grep -E '^[^#]' /etc/squid/squid.conf
```

### Estructura conceptual

```bash
# /etc/squid/squid.conf — estructura lógica

# === 1. ACL definitions ===
# Definir "nombres" para condiciones
acl localnet src 10.0.0.0/8
acl blocked_sites dstdomain .facebook.com .tiktok.com
acl work_hours time MTWHF 08:00-18:00

# === 2. Access rules ===
# Aplicar las ACLs (orden importa: primera coincidencia gana)
http_access allow localnet
http_access deny blocked_sites
http_access deny all

# === 3. Network settings ===
http_port 3128
# visible_hostname proxy.example.com

# === 4. Cache settings ===
cache_dir ufs /var/spool/squid 100 16 256
maximum_object_size 100 MB

# === 5. Logging ===
access_log /var/log/squid/access.log squid

# === 6. Other ===
coredump_dir /var/spool/squid
```

> **Regla fundamental de Squid**: En las reglas `http_access`, la **primera coincidencia gana**. Si una petición coincide con una regla `allow`, se permite inmediatamente sin evaluar las siguientes. Lo mismo con `deny`. Es como un firewall: el orden es crítico.

---

## 4. ACLs: el sistema de control de acceso

Las ACLs de Squid se definen en dos pasos:

1. **Definir** la ACL: darle un nombre a una condición
2. **Aplicar** la ACL: usar el nombre en reglas `http_access`

### Sintaxis de definición

```
acl NOMBRE TIPO VALORES...
```

### Tipos de ACL

#### Por origen (quién hace la petición)

```bash
# Por IP del cliente
acl localnet src 10.0.0.0/8
acl localnet src 172.16.0.0/12
acl localnet src 192.168.0.0/16
# Múltiples líneas con el mismo nombre = OR (cualquiera de ellas)

# Por subred específica
acl finance_dept src 10.0.10.0/24
acl dev_dept src 10.0.20.0/24

# Por IP individual
acl boss src 10.0.0.100
```

#### Por destino (a dónde va la petición)

```bash
# Por dominio de destino
acl blocked_sites dstdomain .facebook.com .instagram.com .tiktok.com
# El punto inicial (.facebook.com) incluye subdominios:
#   facebook.com ✓, www.facebook.com ✓, m.facebook.com ✓

# Sin punto: solo el dominio exacto
acl exact_site dstdomain example.com
#   example.com ✓, www.example.com ✗

# Por dominio desde archivo (listas grandes)
acl blocked_sites dstdomain "/etc/squid/blocked_domains.txt"
# El archivo tiene un dominio por línea:
#   .facebook.com
#   .tiktok.com
#   .instagram.com

# Por IP de destino
acl internal_servers dst 10.0.1.0/24

# Por expresión regular en la URL
acl blocked_urls url_regex -i \.exe$ \.torrent$
# -i = case insensitive
# Bloquea cualquier URL que termine en .exe o .torrent

# Por regex en el dominio
acl social_media dstdom_regex (facebook|instagram|tiktok)\.com$
```

#### Por puerto y protocolo

```bash
# Puertos seguros para CONNECT (HTTPS tunneling)
acl SSL_ports port 443
acl Safe_ports port 80          # HTTP
acl Safe_ports port 443         # HTTPS
acl Safe_ports port 21          # FTP
acl Safe_ports port 1025-65535  # Puertos altos

# Método HTTP
acl CONNECT method CONNECT
acl purge method PURGE
```

#### Por tiempo

```bash
# Horario laboral: lunes a viernes, 8:00-18:00
acl work_hours time MTWHF 08:00-18:00

# Fin de semana
acl weekend time AS 00:00-23:59

# Días individuales:
# S=Sunday M=Monday T=Tuesday W=Wednesday
# H=Thursday F=Friday A=Saturday
```

#### Por tipo MIME y tamaño

```bash
# Tipo MIME de la respuesta
acl video_content rep_mime_type video/

# Tamaño máximo de la petición
acl large_request req_mime_type -i ^application/octet-stream$

# Tamaño de respuesta (para delay pools)
acl large_download rep_header Content-Length >10485760
```

#### Por autenticación

```bash
# Usuarios autenticados
acl authenticated proxy_auth REQUIRED
# REQUIRED = cualquier usuario autenticado

# Usuarios específicos
acl admins proxy_auth admin jefe
acl devs proxy_auth alice bob carol

# Grupo externo (vía helper)
acl ldap_admins external ldap_group sysadmins
```

---

## 5. http_access: aplicar las ACLs

### Sintaxis

```
http_access allow|deny [!]ACL [[!]ACL ...]
```

Múltiples ACLs en la misma línea = AND (todas deben cumplirse).
`!` = negación.

### Orden de evaluación

```
http_access allow localnet work_hours
http_access deny blocked_sites
http_access deny all

Petición de 10.0.0.50 a facebook.com a las 10:00:

  Regla 1: allow localnet work_hours
    ¿localnet? 10.0.0.50 está en 10.0.0.0/8 → SÍ
    ¿work_hours? 10:00 MTWHF → SÍ
    AND: SÍ AND SÍ = SÍ → ALLOW → PARA (primera coincidencia)

  ¡facebook.com fue permitido porque la regla allow
   coincidió ANTES que la regla deny!
```

**Orden correcto**:

```bash
# Las reglas deny (restricciones) deben ir ANTES que las allow (permisos)

# 1. Denegar lo peligroso primero
http_access deny !Safe_ports
http_access deny CONNECT !SSL_ports

# 2. Denegar sitios bloqueados
http_access deny blocked_sites

# 3. Permitir lo que está OK
http_access allow localnet

# 4. Denegar todo lo demás (red de seguridad)
http_access deny all
```

### Ejemplos de políticas

```bash
# === Política básica: red local con sitios bloqueados ===

acl localnet src 10.0.0.0/8
acl Safe_ports port 80 443 21 1025-65535
acl SSL_ports port 443
acl CONNECT method CONNECT
acl blocked dstdomain "/etc/squid/blocked.txt"

http_access deny !Safe_ports
http_access deny CONNECT !SSL_ports
http_access deny blocked
http_access allow localnet
http_access deny all
```

```bash
# === Política por departamentos y horarios ===

acl finance src 10.0.10.0/24
acl devs src 10.0.20.0/24
acl streaming dstdomain .youtube.com .netflix.com .spotify.com
acl social dstdomain .facebook.com .instagram.com .tiktok.com
acl work_hours time MTWHF 08:00-18:00

# Finanzas: solo sitios de trabajo
http_access deny finance social
http_access deny finance streaming

# Desarrollo: libre excepto streaming en horario laboral
http_access deny devs streaming work_hours

# Permitir ambas redes
http_access allow finance
http_access allow devs

http_access deny all
```

```bash
# === Política con autenticación ===

auth_param basic program /usr/lib64/squid/basic_ldap_auth ...
acl authenticated proxy_auth REQUIRED
acl admins proxy_auth admin root

# Los admins pueden todo
http_access allow admins

# El resto debe autenticarse y no puede ver sitios bloqueados
http_access deny !authenticated
http_access deny blocked_sites
http_access allow authenticated

http_access deny all
```

### Lógica AND vs OR

```bash
# AND — múltiples ACLs en la misma línea
http_access deny streaming work_hours
# Bloquea streaming SOLO durante horario laboral
# Fuera de horario → work_hours no coincide → regla no aplica

# OR — múltiples líneas para la misma ACL
acl localnet src 10.0.0.0/8
acl localnet src 172.16.0.0/12
# Valores dentro de la misma ACL son OR:
# 10.0.0.0/8 OR 172.16.0.0/12

# NOT — con !
http_access deny !Safe_ports
# Deniega si el puerto NO está en Safe_ports
```

---

## 6. Caché

### Configuración de caché en disco

```bash
# cache_dir TYPE DIRECTORY SIZE_MB L1 L2
cache_dir ufs /var/spool/squid 1000 16 256
#         ^^^                  ^^^^  ^^ ^^^
#         │                    │     │  └── subdirectorios nivel 2 (256)
#         │                    │     └── subdirectorios nivel 1 (16)
#         │                    └── tamaño máximo en MB (1 GB)
#         └── tipo de almacenamiento
```

Tipos de almacenamiento:

| Tipo | Descripción | Uso |
|---|---|---|
| `ufs` | Unix File System, síncrono | Por defecto, simple |
| `aufs` | Asynchronous UFS, usa threads | Mejor rendimiento |
| `rock` | Base de datos optimizada | Alto throughput, SSD |
| `diskd` | Daemon externo para I/O | Servidores dedicados |

### Caché en memoria

```bash
# Memoria RAM para objetos hot (accedidos frecuentemente)
cache_mem 256 MB
# NOTA: esto NO es toda la memoria que usa Squid
# Squid usa más RAM para índices, buffers, conexiones
# Regla: cache_mem + ~100 MB overhead ≈ RAM total para Squid

# Tamaño máximo de objeto en memoria
maximum_object_size_in_memory 1 MB
```

### Control de qué se cachea

```bash
# Tamaño máximo de objeto en disco
maximum_object_size 100 MB

# Tamaño mínimo (no cachear objetos pequeños, el overhead no vale)
minimum_object_size 0 KB

# Patrones de refresco: cuánto tiempo mantener objetos
# refresh_pattern REGEX MIN_MINUTES PERCENT MAX_MINUTES
refresh_pattern ^ftp:           1440    20%     10080
refresh_pattern ^gopher:        1440    0%      1440
refresh_pattern -i (/cgi-bin/|\?) 0     0%      0
refresh_pattern .               0       20%     4320

# Ejemplo: cachear updates de Linux agresivamente
refresh_pattern -i \.rpm$       10080   100%    43200
refresh_pattern -i \.deb$       10080   100%    43200
refresh_pattern -i \.pkg\.tar   10080   100%    43200
```

### Cómo funciona refresh_pattern

```
refresh_pattern REGEX MIN% MAX

  Objeto en caché con edad = T minutos
  Tiempo desde que el servidor lo marcó como "fresco" = LM_AGE

  Si T < MIN → FRESCO (servir desde caché)
  Si T > MAX → OBSOLETO (pedir al servidor)
  Si MIN < T < MAX → usar PERCENT:
    Si T < (LM_AGE × PERCENT/100) → FRESCO
    Si no → OBSOLETO
```

### Inicializar la caché

```bash
# Crear los directorios de caché (necesario la primera vez)
squid -z
# O:
squid -z --foreground
# Crea la estructura de directorios en /var/spool/squid/

# Después:
systemctl start squid
```

### Estadísticas de caché

```bash
# Desde la línea de comandos
squidclient -h localhost mgr:info
# Muestra: hit ratio, memoria, conexiones activas

squidclient -h localhost mgr:utilization
# Uso de caché: disco, memoria

# Caché manager desde el navegador (si está habilitado)
# http://proxy:3128/squid-internal-mgr/info
```

---

## 7. Autenticación

Squid puede exigir que los usuarios se identifiquen antes de navegar. Soporta múltiples backends de autenticación.

### Esquemas de autenticación

```
┌──────────┐    ┌─────────┐    ┌──────────────────────┐
│ Navegador│───►│  Squid  │───►│ Helper de autenticación│
│          │◄───│         │◄───│ (programa externo)     │
└──────────┘    └─────────┘    └──────────────────────┘
                                 │
     HTTP 407                    ├── basic_ncsa_auth (archivo)
     Proxy-Authenticate         ├── basic_ldap_auth (LDAP)
                                 ├── negotiate_kerberos (Kerberos/AD)
                                 └── basic_radius_auth (RADIUS)
```

### Autenticación NCSA (archivo local)

La más simple: usuario y contraseña en un archivo tipo htpasswd.

```bash
# Crear archivo de contraseñas
htpasswd -c /etc/squid/passwd alice
# Pide contraseña, crea el archivo

# Añadir más usuarios
htpasswd /etc/squid/passwd bob
htpasswd /etc/squid/passwd carol

# Permisos
chown root:squid /etc/squid/passwd
chmod 640 /etc/squid/passwd
```

```bash
# squid.conf — autenticación NCSA
auth_param basic program /usr/lib64/squid/basic_ncsa_auth /etc/squid/passwd
auth_param basic children 5          # procesos helper simultáneos
auth_param basic realm Proxy Corp    # texto que muestra el navegador
auth_param basic credentialsttl 2 hours  # tiempo antes de re-pedir credenciales

acl authenticated proxy_auth REQUIRED

http_access deny !authenticated
http_access allow authenticated
http_access deny all
```

```
Flujo:
  1. Cliente pide http://example.com vía proxy
  2. Squid responde: 407 Proxy Authentication Required
     Proxy-Authenticate: Basic realm="Proxy Corp"
  3. Navegador muestra diálogo de usuario/contraseña
  4. Cliente reenvía con: Proxy-Authorization: Basic base64(user:pass)
  5. Squid pasa user:pass al helper basic_ncsa_auth
  6. Helper verifica contra /etc/squid/passwd
  7. Helper responde OK o ERR
  8. Squid permite o deniega
```

### Autenticación LDAP

```bash
# squid.conf — autenticación contra LDAP
auth_param basic program /usr/lib64/squid/basic_ldap_auth \
    -v 3 \
    -H ldaps://ldap.example.com \
    -b "ou=People,dc=example,dc=com" \
    -D "cn=readonly,dc=example,dc=com" \
    -W /etc/squid/ldap-pass \
    -f "(uid=%s)"
#   ^^^^^^^^
#   %s se reemplaza por el nombre de usuario

auth_param basic children 10
auth_param basic realm Corporate Proxy
auth_param basic credentialsttl 1 hour

acl authenticated proxy_auth REQUIRED
```

Opciones del helper LDAP:

| Opción | Descripción |
|---|---|
| `-v 3` | Versión LDAP |
| `-H URI` | Servidor LDAP (ldaps:// o ldap:// con -ZZ para StartTLS) |
| `-b base` | Base DN de búsqueda |
| `-D bind_dn` | DN para bind de búsqueda |
| `-W file` | Archivo con contraseña de bind |
| `-w pass` | Contraseña de bind (inseguro, visible en `ps`) |
| `-f filter` | Filtro LDAP (`%s` = username) |
| `-ZZ` | Exigir StartTLS |

### Autenticación LDAP con grupos

Squid puede verificar membresía en grupos LDAP usando un helper externo:

```bash
# Helper para verificar grupos LDAP
external_acl_type ldap_group %LOGIN \
    /usr/lib64/squid/ext_ldap_group_acl \
    -v 3 \
    -H ldaps://ldap.example.com \
    -b "ou=Groups,dc=example,dc=com" \
    -D "cn=readonly,dc=example,dc=com" \
    -W /etc/squid/ldap-pass \
    -f "(&(objectClass=posixGroup)(cn=%g)(memberUid=%u))"
#                                    ^^              ^^
#                                    grupo           usuario

acl ldap_admins external ldap_group sysadmins
acl ldap_devs external ldap_group developers

# Admins: acceso total
http_access allow ldap_admins

# Developers: sin streaming
http_access deny ldap_devs streaming
http_access allow ldap_devs

http_access deny all
```

### Kerberos/Negotiate (entorno AD)

```bash
# Para entornos con Active Directory y SSO
auth_param negotiate program /usr/lib64/squid/negotiate_kerberos_auth \
    -k /etc/squid/squid.keytab \
    -s HTTP/proxy.corp.example.com@CORP.EXAMPLE.COM

auth_param negotiate children 10
auth_param negotiate keep_alive on

# El navegador envía ticket Kerberos automáticamente (SSO)
# No aparece diálogo de usuario/contraseña
```

---

## 8. Logging

### access.log: registro de accesos

```bash
# Formato por defecto (squid native):
# timestamp elapsed client action/code size method URL user hierarchy content_type

1711008000.123    150 10.0.0.50 TCP_MISS/200 15234 GET http://example.com/ - DIRECT/93.184.216.34 text/html
│                 │   │         │        │   │     │   │                    │ │                    │
│                 │   │         │        │   │     │   │                    │ │                    └── tipo MIME
│                 │   │         │        │   │     │   │                    │ └── servidor contactado
│                 │   │         │        │   │     │   │                    └── usuario (- = no auth)
│                 │   │         │        │   │     │   └── URL solicitada
│                 │   │         │        │   │     └── método HTTP
│                 │   │         │        │   └── tamaño en bytes
│                 │   │         │        └── código HTTP
│                 │   │         └── resultado de caché
│                 │   └── IP del cliente
│                 └── tiempo de respuesta (ms)
└── timestamp Unix
```

### Resultados de caché

| Código | Descripción |
|---|---|
| `TCP_HIT` | Servido desde caché (no contactó al servidor) |
| `TCP_MISS` | No estaba en caché, pidió al servidor |
| `TCP_REFRESH_HIT` | Estaba en caché, verificó con servidor, no cambió |
| `TCP_REFRESH_MISS` | Estaba en caché, verificó, el servidor envió nuevo |
| `TCP_DENIED` | Petición denegada por ACL |
| `TCP_TUNNEL` | Conexión CONNECT (HTTPS tunnel) |
| `NONE/407` | Requiere autenticación |

### Formato de log personalizado

```bash
# Formato combinado (compatible con analizadores web estándar)
access_log /var/log/squid/access.log combined

# Formato personalizado
logformat detailed %ts.%03tu %6tr %>a %Ss/%03>Hs %<st %rm %ru %un %Sh/%<a %mt "%{User-Agent}>h"
access_log /var/log/squid/access.log detailed
```

### cache.log: log del servicio

```bash
# /var/log/squid/cache.log — mensajes del proceso Squid
# Arranque, errores de configuración, warnings, shutdown

# Ver errores recientes
grep -i "error\|warning\|fatal" /var/log/squid/cache.log | tail -20

# Rotar logs manualmente
squid -k rotate
# Rota access.log y cache.log (crea .1, .2, etc.)
```

### Análisis de logs

```bash
# Top 10 dominios más visitados
awk '{print $7}' /var/log/squid/access.log | \
    sed 's|https\?://||;s|/.*||' | sort | uniq -c | sort -rn | head -10

# Top 10 clientes por volumen de tráfico
awk '{sum[$3]+=$5} END {for(ip in sum) print sum[ip], ip}' \
    /var/log/squid/access.log | sort -rn | head -10

# Hit ratio (porcentaje de caché)
grep -c "TCP_HIT\|TCP_MEM_HIT\|TCP_REFRESH_HIT" /var/log/squid/access.log
grep -c "." /var/log/squid/access.log
# (hits / total) × 100

# Peticiones denegadas
grep "TCP_DENIED" /var/log/squid/access.log | tail -20

# Herramientas dedicadas
# calamaris — generador de reportes para Squid
# SARG (Squid Analysis Report Generator) — reportes HTML
# Lightsquid — reportes web ligeros
```

---

## 9. Gestión del servicio

### Comandos de control

```bash
# Arrancar/parar
systemctl start squid
systemctl stop squid
systemctl restart squid

# Recargar configuración sin reiniciar (no corta conexiones)
systemctl reload squid
# Equivalente a:
squid -k reconfigure

# Verificar configuración ANTES de recargar
squid -k parse
# Si hay errores los muestra sin afectar al servicio
# SIEMPRE ejecutar antes de reload/restart
```

### squid -k: señales de control

| Comando | Descripción |
|---|---|
| `squid -k parse` | Verificar sintaxis sin aplicar |
| `squid -k reconfigure` | Recargar config (= systemctl reload) |
| `squid -k rotate` | Rotar logs |
| `squid -k shutdown` | Parada limpia (espera conexiones activas) |
| `squid -k interrupt` | Parada inmediata |
| `squid -k check` | Verificar si Squid está corriendo |

### Firewall

```bash
# RHEL — permitir acceso al proxy
firewall-cmd --add-port=3128/tcp --permanent
firewall-cmd --reload

# Si Squid es proxy transparente, redirigir HTTP
firewall-cmd --add-forward-port=port=80:proto=tcp:toport=3128 --permanent
firewall-cmd --reload
```

### SELinux

```bash
# Squid tiene política SELinux propia
# Si necesitas que Squid escuche en un puerto no estándar:
semanage port -a -t squid_port_t -p tcp 8080

# Si la caché está en una ubicación no estándar:
semanage fcontext -a -t squid_cache_t "/data/squid(/.*)?"
restorecon -Rv /data/squid

# Si Squid necesita conectar a LDAP para autenticación:
setsebool -P squid_connect_any on
```

### Configuración de ejemplo completa

```bash
# /etc/squid/squid.conf — configuración corporativa básica

# === Red y puertos ===
http_port 3128
visible_hostname proxy.empresa.local

# === ACL definitions ===
acl localnet src 10.0.0.0/8
acl Safe_ports port 80 443 21 70 210 1025-65535
acl SSL_ports port 443
acl CONNECT method CONNECT

acl blocked_sites dstdomain "/etc/squid/blocked_domains.txt"
acl blocked_urls url_regex -i "/etc/squid/blocked_urls.txt"
acl work_hours time MTWHF 08:00-18:00
acl streaming dstdomain .youtube.com .netflix.com .twitch.tv

# === Autenticación LDAP ===
auth_param basic program /usr/lib64/squid/basic_ldap_auth \
    -v 3 -H ldaps://ldap.empresa.local \
    -b "ou=People,dc=empresa,dc=local" \
    -D "cn=squid-bind,dc=empresa,dc=local" \
    -W /etc/squid/ldap-pass \
    -f "(uid=%s)"
auth_param basic children 10
auth_param basic realm Proxy Corporativo
auth_param basic credentialsttl 1 hour

acl authenticated proxy_auth REQUIRED

# === Reglas de acceso (ORDEN IMPORTA) ===
# 1. Seguridad básica
http_access deny !Safe_ports
http_access deny CONNECT !SSL_ports

# 2. Requerir autenticación
http_access deny !authenticated

# 3. Restricciones de contenido
http_access deny blocked_sites
http_access deny blocked_urls
http_access deny streaming work_hours

# 4. Permitir red local autenticada
http_access allow localnet

# 5. Denegar todo lo demás
http_access deny all

# === Caché ===
cache_dir aufs /var/spool/squid 2000 16 256
cache_mem 512 MB
maximum_object_size 200 MB
minimum_object_size 0 KB
maximum_object_size_in_memory 2 MB

refresh_pattern ^ftp:           1440    20%     10080
refresh_pattern -i \.rpm$       10080   100%    43200
refresh_pattern -i \.deb$       10080   100%    43200
refresh_pattern -i (/cgi-bin/|\?) 0     0%      0
refresh_pattern .               0       20%     4320

# === Logging ===
access_log /var/log/squid/access.log squid
cache_log /var/log/squid/cache.log
logfile_rotate 10

# === Misc ===
coredump_dir /var/spool/squid
shutdown_lifetime 10 seconds

# Cabeceras de privacidad
forwarded_for off
# ^^^ No enviar X-Forwarded-For (oculta IP del cliente)
# Alternativa: forwarded_for on (el servidor ve la IP real)

# Página de error personalizada
# err_page_name /etc/squid/errors/ERR_ACCESS_DENIED
```

---

## 10. Errores comunes

### Error 1: Orden incorrecto de reglas http_access

```bash
# MAL — allow antes de deny
http_access allow localnet
http_access deny blocked_sites
http_access deny all

# Resultado: un usuario de localnet accede a facebook.com
#   Regla 1: allow localnet → 10.0.0.50 coincide → ALLOW → PARA
#   ¡La regla deny blocked_sites NUNCA se evalúa!

# BIEN — deny primero, allow después
http_access deny blocked_sites
http_access allow localnet
http_access deny all

# Resultado:
#   Regla 1: deny blocked_sites → facebook.com coincide → DENY → PARA
```

### Error 2: Olvidar http_access deny all al final

```bash
# MAL — sin deny all
http_access allow localnet

# ¿Qué pasa con una IP que no está en localnet?
# Comportamiento: por defecto se INVIERTE la última regla
# Si la última era allow → el default es deny (correcto por suerte)
# Pero si la última era deny:
http_access deny blocked
# ¡El default sería allow! Todo lo que no está blocked se permite

# BIEN — siempre terminar con deny all explícito
http_access allow localnet
http_access deny all
```

### Error 3: No inicializar la caché

```bash
# Síntoma: Squid no arranca, cache.log dice:
# "Failed to verify one of the swap directories"

# Causa: los directorios de caché no existen

# Solución: inicializar antes del primer arranque
squid -z
systemctl start squid
```

### Error 4: No ejecutar squid -k parse antes de reload

```bash
# MAL — aplicar config sin verificar
vim /etc/squid/squid.conf    # hacer cambios
systemctl reload squid        # si hay un typo, squid puede caer

# BIEN — verificar primero
vim /etc/squid/squid.conf
squid -k parse
# Si hay errores:
# FATAL: Bungled /etc/squid/squid.conf line 42: aclc localnet src ...
#                                                ^^^^  typo

# Solo aplicar si parse no muestra errores
systemctl reload squid
```

### Error 5: Autenticación LDAP con contraseña en línea de comandos

```bash
# MAL — contraseña visible en ps aux
auth_param basic program /usr/lib64/squid/basic_ldap_auth \
    -w "MiContraseñaSecreta" ...
# Cualquier usuario del sistema puede ver la contraseña con ps

# BIEN — archivo de contraseña
echo -n "MiContraseñaSecreta" > /etc/squid/ldap-pass
chmod 640 /etc/squid/ldap-pass
chown root:squid /etc/squid/ldap-pass

auth_param basic program /usr/lib64/squid/basic_ldap_auth \
    -W /etc/squid/ldap-pass ...
```

---

## 11. Cheatsheet

```
=== INSTALACIÓN ===
dnf install squid                     RHEL
apt install squid                     Debian
squid -z                              inicializar directorios de caché
systemctl enable --now squid          arrancar y habilitar

=== ARCHIVOS ===
/etc/squid/squid.conf                 configuración principal
/var/spool/squid/                     caché en disco
/var/log/squid/access.log             log de accesos
/var/log/squid/cache.log              log del servicio

=== CONTROL DEL SERVICIO ===
squid -k parse                        verificar config (SIEMPRE antes de reload)
squid -k reconfigure                  recargar sin reiniciar
squid -k rotate                       rotar logs
squid -k shutdown                     parada limpia
systemctl reload squid                = squid -k reconfigure

=== ACL TYPES ===
src IP/MASK                           IP del cliente
dstdomain .domain.com                 dominio destino (con subdominios)
dstdomain "/file"                     dominios desde archivo
dst IP/MASK                           IP del destino
url_regex -i REGEX                    regex en la URL
dstdom_regex REGEX                    regex en el dominio
port N                                puerto destino
method CONNECT                        método HTTP
time MTWHF HH:MM-HH:MM               día y hora
proxy_auth REQUIRED                   usuario autenticado
proxy_auth user1 user2                usuarios específicos
external TYPE group                   grupo externo (LDAP)

=== HTTP_ACCESS ===
http_access deny ACL1 ACL2            denegar si ACL1 AND ACL2
http_access allow ACL1                permitir si ACL1
http_access deny !ACL1                denegar si NOT ACL1
Primera coincidencia gana             orden = prioridad
Siempre terminar con deny all

=== CACHÉ ===
cache_dir TYPE DIR SIZE L1 L2         caché en disco
cache_mem N MB                        caché en memoria (objetos hot)
maximum_object_size N MB              máximo por objeto en disco
refresh_pattern REGEX MIN% MAX        política de frescura

=== AUTENTICACIÓN ===
auth_param basic program PATH ARGS    helper de autenticación
  basic_ncsa_auth /path/passwd        archivo htpasswd
  basic_ldap_auth -H URI -b BASE ...  LDAP
  negotiate_kerberos_auth -k keytab   Kerberos/AD

=== LOGGING ===
TCP_HIT       servido desde caché
TCP_MISS      pedido al servidor
TCP_DENIED    bloqueado por ACL
NONE/407      requiere autenticación

=== TESTING ===
curl -x http://proxy:3128 http://example.com        probar HTTP
curl -x http://proxy:3128 https://example.com        probar HTTPS
curl -x http://user:pass@proxy:3128 http://example.com   con auth
squidclient -h localhost mgr:info                    estadísticas
```

---

## 12. Ejercicios

### Ejercicio 1: Escribir ACLs y reglas

Escribe la configuración de `squid.conf` (solo las secciones ACL y http_access) para esta política:

- Red de la empresa: `172.16.0.0/16`
- Departamento de RRHH: `172.16.10.0/24`
- Departamento de Ingeniería: `172.16.20.0/24`
- Sitios bloqueados para todos: Facebook, Instagram, TikTok
- Streaming (YouTube, Netflix, Twitch): bloqueado para RRHH siempre, bloqueado para Ingeniería solo en horario laboral (L-V 8:00-18:00)
- Descargas `.exe` y `.torrent`: bloqueadas para todos
- Solo puertos HTTP (80), HTTPS (443) y SSH (22) están permitidos
- Todo lo que no sea de la red de la empresa está denegado

Después de escribirlo, verifica mentalmente estos escenarios:
1. Un ingeniero accede a YouTube un sábado a las 15:00
2. RRHH intenta descargar un archivo `.exe` de un sitio permitido
3. Un visitante con IP `192.168.1.50` intenta usar el proxy

> **Pregunta de predicción**: Si intercambias el orden de las reglas `deny blocked_sites` y `allow localnet`, ¿qué efecto tiene? ¿Para qué escenarios cambia el resultado?

### Ejercicio 2: Diagnosticar access.log

Analiza estas líneas de access.log y responde las preguntas:

```
1711008000.100    45 172.16.20.15 TCP_DENIED/407 4050 GET http://github.com/ - NONE/- text/html
1711008000.200   200 172.16.20.15 TCP_MISS/200 52340 GET http://github.com/ alice DIRECT/140.82.121.3 text/html
1711008000.300     5 172.16.20.15 TCP_DENIED/403 3800 GET http://www.facebook.com/ alice NONE/- text/html
1711008000.400  1500 172.16.20.15 TCP_MISS/200 1048576 GET http://mirror.centos.org/7/updates/x86_64/kernel-5.4.rpm alice DIRECT/85.236.55.6 application/x-rpm
1711008000.500     2 172.16.20.15 TCP_HIT/200 1048576 GET http://mirror.centos.org/7/updates/x86_64/kernel-5.4.rpm bob NONE/- application/x-rpm
```

1. ¿Qué ocurrió en la línea 1? ¿Por qué código 407?
2. ¿Cuánto tardó la petición de la línea 2 vs la línea 5? ¿Por qué la diferencia?
3. ¿Por qué la línea 3 fue denegada? ¿Qué código HTTP recibió el usuario?
4. En las líneas 4 y 5, el mismo archivo fue descargado por alice y bob. ¿Cuánto ancho de banda ahorró la caché?
5. ¿Qué significan `DIRECT/140.82.121.3` y `NONE/-`?

> **Pregunta de predicción**: Si la línea 4 hubiera sido una descarga por HTTPS (`https://mirror.centos.org/...`), ¿habría aparecido `TCP_HIT` en la línea 5? ¿Por qué?

### Ejercicio 3: Configuración completa

Configura Squid para un laboratorio educativo con estos requisitos:

1. 30 PCs en la red `10.10.0.0/24`
2. Autenticación NCSA con archivo `/etc/squid/lab-passwd`
3. Solo se permiten los puertos 80 y 443
4. Caché de 5 GB en disco, 256 MB en memoria
5. Los archivos `.iso` y `.rpm` se cachean agresivamente (7 días mínimo)
6. Log en formato squid nativo
7. El proxy escucha en el puerto 8080 (no el 3128 por defecto)

Escribe el `squid.conf` completo y los comandos necesarios para:
- Crear el archivo de usuarios con 3 usuarios de prueba
- Inicializar la caché
- Verificar la configuración
- Arrancar el servicio
- Probar con curl desde un cliente

> **Pregunta de predicción**: Si un alumno configura su navegador para usar el proxy en el puerto 8080, pero otro alumno no lo configura e intenta navegar directamente, ¿qué ocurre con el segundo alumno? ¿Puede navegar? ¿Depende del firewall de la red?

---

> **Siguiente tema**: T03 — Configuración de clientes (variables http_proxy/https_proxy, proxy en navegador, proxy transparente).
