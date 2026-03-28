# Booleans — Interruptores de la política SELinux

## Índice

1. [¿Qué son los booleans?](#1-qué-son-los-booleans)
2. [Consultar booleans: getsebool](#2-consultar-booleans-getsebool)
3. [Modificar booleans: setsebool](#3-modificar-booleans-setsebool)
4. [semanage boolean](#4-semanage-boolean)
5. [Booleans de httpd](#5-booleans-de-httpd)
6. [Booleans de otros servicios](#6-booleans-de-otros-servicios)
7. [Encontrar el boolean correcto](#7-encontrar-el-boolean-correcto)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. ¿Qué son los booleans?

Los booleans de SELinux son **interruptores on/off** que activan o desactivan partes opcionales de la política sin necesidad de recompilarla. Permiten personalizar el comportamiento de SELinux de forma sencilla y segura.

```
Política SELinux (simplificada):

  httpd_t puede leer httpd_sys_content_t         ← siempre activo
  httpd_t puede escribir httpd_log_t              ← siempre activo
  httpd_t puede escuchar en http_port_t           ← siempre activo

  httpd_t puede conectar a la red               ← BOOLEAN: httpd_can_network_connect
  httpd_t puede enviar email                    ← BOOLEAN: httpd_can_sendmail
  httpd_t puede ejecutar CGI                    ← BOOLEAN: httpd_enable_cgi
  httpd_t puede leer contenido de /home         ← BOOLEAN: httpd_enable_homedirs
```

```
                              off ○──── on
                              ─────────────
  httpd_can_network_connect   off ○         Apache NO puede conectar a BD remotas
  httpd_can_network_connect        ──── on  Apache SÍ puede conectar a BD remotas

  Cada boolean controla una "funcionalidad opcional"
  de un servicio confinado
```

### Por qué existen

Sin booleans, la política sería binaria: todo o nada. Los booleans permiten ajustar la política a las necesidades de cada servidor sin desactivar SELinux:

```
Servidor web estático:
  httpd_can_network_connect = off   (no necesita BD)
  httpd_enable_cgi = off            (no tiene CGI)
  httpd_can_sendmail = off          (no envía email)
  → Mínimo privilegio: Apache solo sirve archivos ✓

Servidor con WordPress:
  httpd_can_network_connect = off
  httpd_can_network_connect_db = on  (necesita MySQL)
  httpd_enable_cgi = off
  httpd_can_sendmail = on            (envía notificaciones)
  httpd_unified = on                 (PHP-FPM comparte tipos)
  → Permisos ajustados a WordPress ✓

Servidor con proxy reverso:
  httpd_can_network_connect = on     (conecta a backends)
  httpd_can_network_relay = on       (reenvía conexiones)
  → Permisos ajustados a reverse proxy ✓
```

---

## 2. Consultar booleans: getsebool

### Consultar un boolean específico

```bash
getsebool httpd_can_network_connect
# httpd_can_network_connect --> off

getsebool httpd_enable_cgi
# httpd_enable_cgi --> on
```

### Listar todos los booleans

```bash
# Todos los booleans del sistema
getsebool -a
# abrt_anon_write --> off
# abrt_handle_event --> off
# ...
# httpd_can_network_connect --> off
# httpd_can_network_connect_db --> off
# httpd_can_sendmail --> off
# ...
# (cientos de booleans)

# Contar booleans
getsebool -a | wc -l
# ~350 en una instalación RHEL típica
```

### Filtrar booleans por servicio

```bash
# Todos los booleans de httpd
getsebool -a | grep httpd
# httpd_anon_write --> off
# httpd_builtin_scripting --> on
# httpd_can_check_spam --> off
# httpd_can_connect_ftp --> off
# httpd_can_connect_ldap --> off
# httpd_can_connect_mythtv --> off
# httpd_can_connect_zabbix --> off
# httpd_can_network_connect --> off
# httpd_can_network_connect_cobbler --> off
# httpd_can_network_connect_db --> off
# httpd_can_network_memcache --> off
# httpd_can_network_relay --> off
# httpd_can_sendmail --> off
# httpd_dbus_avahi --> off
# httpd_dbus_sssd --> off
# httpd_enable_cgi --> on
# httpd_enable_ftp_server --> off
# httpd_enable_homedirs --> off
# httpd_execmem --> off
# httpd_graceful_shutdown --> on
# httpd_manage_ipa --> off
# httpd_mod_auth_ntlm_winbind --> off
# httpd_mod_auth_pam --> off
# httpd_read_user_content --> off
# httpd_run_ipa --> off
# httpd_run_preupgrade --> off
# httpd_run_stickshift --> off
# httpd_serve_cobbler_files --> off
# httpd_setrlimit --> off
# httpd_ssi_exec --> off
# httpd_sys_script_anon_write --> off
# httpd_tmp_exec --> off
# httpd_tty_comm --> off
# httpd_unified --> off
# httpd_use_cifs --> off
# httpd_use_fusefs --> off
# httpd_use_gpg --> off
# httpd_use_nfs --> off
# httpd_use_openstack --> off
# httpd_use_sasl --> off
# httpd_verify_dns --> off

# Booleans de samba
getsebool -a | grep samba
# samba_create_home_dirs --> off
# samba_domain_controller --> off
# samba_enable_home_dirs --> off
# samba_export_all_ro --> off
# samba_export_all_rw --> off
# samba_load_libgfapi --> off
# samba_portmapper --> off
# samba_run_unconfined --> off
# samba_share_fusefs --> off
# samba_share_nfs --> off
```

---

## 3. Modificar booleans: setsebool

### Cambio temporal (runtime, no sobrevive reboot)

```bash
setsebool httpd_can_network_connect on
# Efecto inmediato, se pierde al reiniciar

getsebool httpd_can_network_connect
# httpd_can_network_connect --> on

# Desactivar
setsebool httpd_can_network_connect off
```

### Cambio persistente (sobrevive reboot)

```bash
setsebool -P httpd_can_network_connect on
#          ^^
#          Persistente (escribe en la política en disco)
#          Tarda un poco más que sin -P

# Verificar
getsebool httpd_can_network_connect
# httpd_can_network_connect --> on
```

> **Siempre usar `-P`** a menos que estés haciendo una prueba rápida. Sin `-P`, el boolean vuelve a su valor anterior después de un reboot, lo que puede causar problemas difíciles de diagnosticar ("funcionaba antes de reiniciar").

### Cambiar múltiples booleans

```bash
# Varios a la vez
setsebool -P httpd_can_network_connect_db on \
             httpd_can_sendmail on \
             httpd_enable_homedirs on
```

---

## 4. semanage boolean

`semanage boolean` ofrece información adicional que `getsebool` no muestra.

### Listar con descripción

```bash
# Ver estado actual y si fue modificado del default
semanage boolean -l | grep httpd_can_network
# SELinux boolean               State  Default  Description
# httpd_can_network_connect     (off  ,  off)   Allow httpd to can network connect
# httpd_can_network_connect_db  (off  ,  off)   Allow httpd to can network connect db
# httpd_can_network_relay       (off  ,  off)   Allow httpd to can network relay
```

```
semanage boolean -l muestra:

  State:    valor actual (lo que está activo AHORA)
  Default:  valor por defecto de la política

  (on  ,  on)   → activado, es el default
  (off ,  off)  → desactivado, es el default
  (on  ,  off)  → activado por el admin (customizado)
  (off ,  on)   → desactivado por el admin (customizado)
```

### Ver solo booleans modificados

```bash
# Booleans que difieren del default (los que el admin cambió)
semanage boolean -l -C
# SELinux boolean               State  Default  Description
# httpd_can_network_connect_db  (on   ,  off)   Allow httpd to can network connect db
# httpd_can_sendmail            (on   ,  off)   Allow httpd to can sendmail

# Esto es MUY útil para documentar qué se ha personalizado
# y para replicar la configuración en otro servidor
```

### Modificar con semanage

```bash
# Equivalente a setsebool -P
semanage boolean -m --on httpd_can_network_connect
semanage boolean -m --off httpd_can_network_connect

# No ofrece ventaja práctica sobre setsebool -P
# Pero es consistente con el resto de herramientas semanage
```

---

## 5. Booleans de httpd

Apache/Nginx es el servicio con más booleans. Los más importantes:

### Conectividad de red

```bash
# Apache puede iniciar conexiones TCP a cualquier puerto
httpd_can_network_connect = off (default)
# Caso: reverse proxy, conectar a APIs externas
# Si off: Apache solo puede servir contenido estático/local

# Apache puede conectar a puertos de base de datos
httpd_can_network_connect_db = off (default)
# Caso: WordPress, Django, cualquier app con BD
# Puertos: 3306 (MySQL), 5432 (PostgreSQL), 1521 (Oracle)

# Apache puede reenviar conexiones (relay)
httpd_can_network_relay = off (default)
# Caso: reverse proxy hacia backends

# Apache puede conectar a LDAP
httpd_can_connect_ldap = off (default)
# Caso: autenticación LDAP en Apache

# Apache puede conectar a memcached
httpd_can_network_memcache = off (default)
# Caso: apps con sesiones en memcached
```

### Contenido

```bash
# Apache puede leer contenido en /home
httpd_enable_homedirs = off (default)
# Caso: UserDir (~user/) o apps en /home

# Apache puede leer contenido de usuarios
httpd_read_user_content = off (default)
# Caso: leer archivos con tipo user_content_t

# Apache puede usar NFS
httpd_use_nfs = off (default)
# Caso: contenido web en mount NFS

# Apache puede usar CIFS/SMB
httpd_use_cifs = off (default)
# Caso: contenido web en mount CIFS

# Apache puede usar FUSE
httpd_use_fusefs = off (default)
# Caso: contenido web en mount FUSE (S3, GlusterFS)
```

### Scripting y ejecución

```bash
# Apache puede ejecutar scripts CGI
httpd_enable_cgi = on (default)
# Activado por defecto; desactivar si no usas CGI

# Apache puede ejecutar código con execmem
httpd_execmem = off (default)
# Caso: módulos PHP que necesitan JIT o mmap con exec
# Riesgo: permite ejecución de código en memoria

# Apache puede ejecutar scripts en /tmp
httpd_tmp_exec = off (default)
# Riesgo alto: un atacante que suba un script a /tmp podría ejecutarlo

# Apache puede enviar correo
httpd_can_sendmail = off (default)
# Caso: PHP mail(), formularios de contacto
```

### Ejemplo: configurar booleans para WordPress

```bash
# WordPress necesita:
# 1. Conectar a MySQL
setsebool -P httpd_can_network_connect_db on

# 2. Enviar emails (notificaciones, registro)
setsebool -P httpd_can_sendmail on

# 3. Si el contenido está en NFS:
# setsebool -P httpd_use_nfs on

# Verificar
getsebool -a | grep httpd | grep ' on$'
# httpd_builtin_scripting --> on
# httpd_can_network_connect_db --> on
# httpd_can_sendmail --> on
# httpd_enable_cgi --> on
# httpd_graceful_shutdown --> on
```

---

## 6. Booleans de otros servicios

### Samba

```bash
# Samba puede compartir directorios home
samba_enable_home_dirs = off (default)
# Caso: [homes] share

# Samba puede exportar TODOS los archivos como solo lectura
samba_export_all_ro = off (default)
# Peligroso: da acceso a todo el sistema de archivos

# Samba puede exportar TODOS los archivos como lectura/escritura
samba_export_all_rw = off (default)
# MUY peligroso: escritura a todo el filesystem

# Samba puede compartir NFS
samba_share_nfs = off (default)
# Caso: re-exportar NFS vía Samba
```

### NFS

```bash
# NFS puede exportar todo con lectura/escritura
nfs_export_all_rw = on (default)
# Activado por defecto; el control fino se hace en /etc/exports

# NFS puede exportar todo como solo lectura
nfs_export_all_ro = on (default)

# Uso de NFS home directories
use_nfs_home_dirs = off (default)
# Caso: homes montados por NFS (necesario para login si /home es NFS)
```

### SSH

```bash
# SSH puede reenviar puertos (port forwarding)
ssh_sysadm_login = off (default)
# Permite login como sysadm_u (solo relevante en política MLS)
```

### FTP (vsftpd)

```bash
# FTP puede acceder a homes
ftp_home_dir = off (default)

# FTP permite subir archivos
ftpd_anon_write = off (default)

# FTP permite login completo (no solo anon)
ftpd_full_access = off (default)
```

### Bases de datos

```bash
# MySQL puede conectar a la red
mysql_connect_any = off (default)
# Caso: replicación, conexiones a otros servidores

# PostgreSQL puede conectar a la red
postgresql_can_rsync = off (default)
```

### General

```bash
# Permitir que los usuarios ejecuten programas desde /tmp
user_exec_content = on (default)

# Permitir que servicios con dominio domain_can_mmap_files lean mmaps
domain_can_mmap_files = on (default)

# Daemons pueden usar syslog vía /dev/log
daemons_use_tty = off (default)
```

---

## 7. Encontrar el boolean correcto

Cuando SELinux bloquea algo, necesitas encontrar qué boolean activar. Hay varios métodos:

### Método 1: audit2why (el mejor)

```bash
# Reproducir el error, luego:
ausearch -m AVC -ts recent | audit2why

# Ejemplo de salida:
# type=AVC msg=audit(1711008000.123:456): avc: denied { name_connect }
#   for pid=1234 comm="httpd" dest=3306
#   scontext=system_u:system_r:httpd_t:s0
#   tcontext=system_u:object_r:mysqld_port_t:s0
#
# Was caused by:
#   The boolean httpd_can_network_connect_db was set incorrectly.
#   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#   audit2why te dice EXACTAMENTE qué boolean activar
#
#   Allow httpd to can network connect db
#
#   setsebool -P httpd_can_network_connect_db 1

# audit2why analiza el AVC denial y busca si un boolean lo resolvería
```

### Método 2: Buscar por nombre del servicio

```bash
# Si sabes qué servicio tiene el problema
getsebool -a | grep httpd
# Lista todos los booleans de httpd
# Lee las descripciones y encuentra el que parece relevante

# Con descripciones:
semanage boolean -l | grep httpd
# Más verboso, incluye descripción de cada uno
```

### Método 3: Buscar por palabra clave

```bash
# ¿Necesitas que algo conecte a la red?
semanage boolean -l | grep -i network
# httpd_can_network_connect      (off  ,  off)  Allow httpd to can network connect
# httpd_can_network_connect_db   (off  ,  off)  Allow httpd to can network connect db
# mysql_connect_any              (off  ,  off)  Allow mysql to connect any
# ...

# ¿Necesitas acceso a home directories?
semanage boolean -l | grep -i home
# httpd_enable_homedirs          (off  ,  off)  Allow httpd to enable homedirs
# samba_enable_home_dirs         (off  ,  off)  Allow samba to enable home dirs
# use_nfs_home_dirs              (off  ,  off)  Allow use to nfs home dirs
# ftp_home_dir                   (off  ,  off)  Allow ftp to home dir

# ¿Necesitas enviar email?
semanage boolean -l | grep -i mail
# httpd_can_sendmail             (off  ,  off)  Allow httpd to can sendmail
```

### Método 4: Documentación del paquete

```bash
# man pages de SELinux por servicio
man httpd_selinux
# Documenta todos los booleans, file contexts y ports de httpd

man samba_selinux
man nfs_selinux
man mysqld_selinux

# Instalar si no están disponibles:
dnf install selinux-policy-doc
```

### Flujo de diagnóstico completo

```
Servicio falla → ¿es SELinux?
│
├── setenforce 0 → ¿funciona ahora? → No → no es SELinux
│                                    → Sí → es SELinux
│   setenforce 1 (volver)
│
├── ausearch -m AVC -ts recent
│   │
│   └── audit2why
│       │
│       ├── "The boolean XXX was set incorrectly"
│       │   → setsebool -P XXX on
│       │   → RESUELTO ✓
│       │
│       ├── "Missing type enforcement rule"
│       │   → No hay boolean, necesitas file context
│       │     o política personalizada (ver T02, T03)
│       │
│       └── (sin AVC)
│           → El problema no es SELinux
│             o el AVC fue suprimido (dontaudit)
│             → semodule -DB (deshabilitar dontaudit)
│             → reproducir → ausearch
│             → semodule -B (restaurar)
```

---

## 8. Errores comunes

### Error 1: Activar httpd_can_network_connect cuando solo necesitas BD

```bash
# MAL — demasiado permisivo
setsebool -P httpd_can_network_connect on
# Permite Apache conectar a CUALQUIER puerto TCP
# Incluye: MySQL, pero también SSH, SMTP, lo que sea

# BIEN — mínimo privilegio
setsebool -P httpd_can_network_connect_db on
# Solo permite conectar a puertos de bases de datos
# (3306, 5432, 1521, etc.)
```

```
httpd_can_network_connect      → conectar a CUALQUIER puerto
httpd_can_network_connect_db   → solo puertos de BD
httpd_can_connect_ldap         → solo LDAP
httpd_can_network_memcache     → solo memcached
httpd_can_network_relay        → solo relay/proxy

Usar el más específico posible.
```

### Error 2: Olvidar -P y perder el cambio al reiniciar

```bash
# MAL — sin -P
setsebool httpd_can_network_connect_db on
# Funciona ahora...
# Reboot...
# Ya no funciona → "¿qué cambió?"

# Verificar si hay booleans customizados sin persistir:
# Comparar State vs Default
semanage boolean -l | grep "(on  ,  off)"
# Si State=on pero Default=off y no se usó -P → se perderá

# BIEN — siempre -P
setsebool -P httpd_can_network_connect_db on
```

### Error 3: No documentar qué booleans se cambiaron

```bash
# Después de meses, nadie recuerda por qué ciertos booleans están on
# Solución: semanage boolean -l -C muestra los cambios

semanage boolean -l -C
# httpd_can_network_connect_db  (on   ,  off)  Allow httpd to can network connect db
# httpd_can_sendmail            (on   ,  off)  Allow httpd to can sendmail
# use_nfs_home_dirs             (on   ,  off)  Allow use to nfs home dirs

# Esto documenta EXACTAMENTE qué se ha personalizado
# Útil para: replicar en otro servidor, auditoría, troubleshooting
```

### Error 4: Activar booleans peligrosos sin entender las consecuencias

```bash
# Booleans peligrosos que se activan "por si acaso":

samba_export_all_rw = on
# ¡Samba puede leer/escribir TODO el filesystem vía SMB!

httpd_execmem = on
# Apache puede ejecutar código en memoria
# (necesario para algunos módulos, pero abre vector de ataque)

httpd_tmp_exec = on
# Apache puede ejecutar scripts en /tmp
# (un atacante que suba un script a /tmp lo puede ejecutar vía Apache)

# Regla: activar SOLO lo que necesitas, verificar con audit2why
```

### Error 5: Buscar el boolean manualmente en vez de usar audit2why

```bash
# MAL — adivinar
# "Apache no conecta a MySQL... ¿qué boolean será?"
getsebool -a | grep httpd | grep connect
# Hay 7 resultados, ¿cuál es?

# BIEN — dejar que audit2why lo diga
ausearch -m AVC -ts recent | audit2why
# "The boolean httpd_can_network_connect_db was set incorrectly"
# Sin adivinar, sin error
```

---

## 9. Cheatsheet

```
=== CONSULTAR BOOLEANS ===
getsebool BOOLEAN                    valor de un boolean
getsebool -a                         todos los booleans
getsebool -a | grep SERVICIO         filtrar por servicio
semanage boolean -l                  todos con descripción
semanage boolean -l | grep KEYWORD   buscar por palabra
semanage boolean -l -C               solo los MODIFICADOS

=== MODIFICAR BOOLEANS ===
setsebool BOOLEAN on|off             cambiar (temporal)
setsebool -P BOOLEAN on|off          cambiar (PERSISTENTE) ← usar siempre
setsebool -P BOOL1 on BOOL2 on      cambiar varios a la vez

=== ENCONTRAR EL BOOLEAN CORRECTO ===
ausearch -m AVC -ts recent | audit2why   el MEJOR método
getsebool -a | grep SERVICIO             buscar por servicio
semanage boolean -l | grep KEYWORD       buscar por palabra
man SERVICIO_selinux                     documentación completa

=== BOOLEANS HTTPD MÁS IMPORTANTES ===
httpd_can_network_connect         conectar a cualquier puerto
httpd_can_network_connect_db      conectar a puertos BD (MySQL, PG)
httpd_can_network_relay           reenviar conexiones (reverse proxy)
httpd_can_connect_ldap            conectar a LDAP
httpd_can_sendmail                enviar email (PHP mail())
httpd_enable_cgi                  ejecutar CGI (on por defecto)
httpd_enable_homedirs             leer /home (UserDir)
httpd_use_nfs                     leer contenido en NFS
httpd_use_cifs                    leer contenido en CIFS/SMB
httpd_execmem                     exec en memoria (JIT, mmap)
httpd_read_user_content           leer archivos de usuarios
httpd_unified                     PHP-FPM comparte tipos con httpd

=== BOOLEANS OTROS SERVICIOS ===
samba_enable_home_dirs            Samba exporta homes
samba_export_all_rw               Samba exporta TODO (¡peligroso!)
use_nfs_home_dirs                 homes en NFS
ftp_home_dir                      FTP accede a homes
mysql_connect_any                 MySQL conecta a red
```

---

## 10. Ejercicios

### Ejercicio 1: Diagnosticar con booleans

Un servidor RHEL con SELinux enforcing ejecuta Apache con una aplicación PHP que:
- Conecta a MySQL en `10.0.0.5:3306`
- Envía emails de notificación
- Lee imágenes de un share NFS montado en `/mnt/media`

Todo funcionaba en el servidor de desarrollo (SELinux permissive). En producción (enforcing), la aplicación falla en tres puntos:

1. "Error: Could not connect to database"
2. "Warning: mail(): Unable to send"
3. Imágenes de `/mnt/media` devuelven 403

Para cada fallo:
1. ¿Qué AVC denial verías en el log?
2. ¿Qué boolean resuelve el problema?
3. Escribe el comando exacto para activarlo persistentemente
4. ¿Había un boolean más permisivo que también funcionaría? ¿Por qué no usarlo?

> **Pregunta de predicción**: Si activas `httpd_can_network_connect` (genérico) en vez de `httpd_can_network_connect_db` (específico), ¿la conexión a MySQL funciona? ¿Qué riesgo adicional introduces?

### Ejercicio 2: Auditar booleans de un servidor

Ejecutas `semanage boolean -l -C` en un servidor y obtienes:

```
SELinux boolean                     State  Default  Description
httpd_can_network_connect           (on  ,  off)    Allow httpd to can network connect
httpd_can_sendmail                  (on  ,  off)    Allow httpd to can sendmail
httpd_execmem                       (on  ,  off)    Allow httpd to execmem
httpd_tmp_exec                      (on  ,  off)    Allow httpd to tmp exec
samba_export_all_rw                 (on  ,  off)    Allow samba to export all rw
use_nfs_home_dirs                   (on  ,  off)    Allow use to nfs home dirs
```

1. ¿Cuáles de estos booleans son potencialmente peligrosos? ¿Por qué?
2. `httpd_can_network_connect` está activado. ¿Podría reemplazarse por un boolean más específico? ¿Cómo averiguarías cuál?
3. `httpd_execmem` y `httpd_tmp_exec` están activados. ¿Qué tipo de ataques facilitan?
4. `samba_export_all_rw` está activado. ¿Qué implica exactamente?
5. Escribe un plan para reducir estos permisos al mínimo necesario sin romper los servicios

> **Pregunta de predicción**: Si desactivas `httpd_can_network_connect` y activas `httpd_can_network_connect_db` en su lugar, ¿podrían romperse funcionalidades que no son de base de datos (como un reverse proxy a un backend en puerto 8080)?

### Ejercicio 3: Replicar configuración SELinux

Necesitas configurar un segundo servidor de producción idéntico al primero. El primer servidor tiene varios booleans personalizados.

1. ¿Qué comando ejecutas en el servidor original para listar SOLO los booleans modificados?
2. Escribe un script bash que exporte los booleans modificados del servidor original y los aplique en el nuevo servidor
3. ¿`setsebool -P` es suficiente o también necesitas replicar file contexts y ports? (tema que se verá en T02/T04)
4. Si un boolean fue cambiado con `setsebool` (sin `-P`), ¿aparece en la lista de modificados de `semanage boolean -l -C`?

> **Pregunta de predicción**: Si ejecutas el script de replicación en un servidor con una versión diferente de `selinux-policy` (por ejemplo, RHEL 8 vs RHEL 9), ¿podrían fallar algunos `setsebool`? ¿Qué pasaría si un boolean no existe en la nueva versión?

---

> **Siguiente tema**: T02 — File contexts (semanage fcontext, restorecon, chcon — temporal vs permanente).
