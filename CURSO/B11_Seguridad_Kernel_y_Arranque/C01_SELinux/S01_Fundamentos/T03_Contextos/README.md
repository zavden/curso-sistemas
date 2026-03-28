# Contextos SELinux — user:role:type:level

## Índice

1. [¿Qué es un contexto SELinux?](#1-qué-es-un-contexto-selinux)
2. [Los cuatro campos del contexto](#2-los-cuatro-campos-del-contexto)
3. [Ver contextos: las opciones -Z](#3-ver-contextos-las-opciones--z)
4. [Contextos de archivos](#4-contextos-de-archivos)
5. [Contextos de procesos (dominios)](#5-contextos-de-procesos-dominios)
6. [Contextos de usuarios](#6-contextos-de-usuarios)
7. [Contextos de puertos](#7-contextos-de-puertos)
8. [Transiciones de dominio](#8-transiciones-de-dominio)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. ¿Qué es un contexto SELinux?

Un contexto SELinux es una **etiqueta** que se asigna a todo objeto del sistema: archivos, procesos, puertos, sockets, dispositivos. La política de seguridad se basa en estas etiquetas para decidir qué puede acceder a qué.

```
                  Contexto SELinux
                  ────────────────
        system_u:system_r:httpd_t:s0
        ────────  ────────  ──────  ──
            │        │        │      │
          user     role     type   level
```

Mientras que DAC pregunta "¿el usuario `apache` puede leer este archivo?", SELinux pregunta "¿el tipo `httpd_t` puede leer un objeto de tipo `httpd_sys_content_t`?".

```
DAC:      UID/GID del proceso  vs  UID/GID del archivo  →  rwx
SELinux:  tipo del proceso     vs  tipo del objeto       →  política
```

---

## 2. Los cuatro campos del contexto

### Formato

```
user_u:role_r:type_t:level

Convención de sufijos:
  _u  →  SELinux user
  _r  →  SELinux role
  _t  →  SELinux type
  s0   →  sensitivity level (MLS)
```

### Campo 1: SELinux user (user_u)

El usuario SELinux es **diferente** del usuario Unix. Mapea usuarios de login a identidades SELinux.

```
Usuarios Unix     →    Usuario SELinux
──────────────          ──────────────
root              →    unconfined_u      (o sysadm_u en MLS)
alice             →    unconfined_u      (usuario normal, no confinado)
bob               →    unconfined_u
apache (servicio) →    system_u          (servicios del sistema)
sshd (servicio)   →    system_u
```

| Usuario SELinux | Descripción | Encontrado en |
|---|---|---|
| `system_u` | Procesos y archivos del sistema | Servicios, archivos de config |
| `unconfined_u` | Usuarios no confinados | Login de usuarios regulares |
| `user_u` | Usuarios confinados | Entornos estrictos |
| `sysadm_u` | Administradores confinados | Política MLS |
| `staff_u` | Staff que puede transicionar a sysadm | Política MLS |

> En la política `targeted`, casi todos los usuarios son `unconfined_u`. El usuario SELinux importa más en políticas `mls` o `strict`.

### Campo 2: SELinux role (role_r)

El rol determina a qué **dominios** (tipos de proceso) puede transicionar un usuario. Es el puente entre usuarios y tipos.

```
Rol              Puede ejecutar dominios de tipo...
────             ─────────────────────────────────
system_r         httpd_t, sshd_t, named_t, ... (servicios)
unconfined_r     unconfined_t (sin restricciones)
object_r         no es un proceso (archivos, puertos)
sysadm_r         sysadm_t (administración)
staff_r          staff_t (transicionar a sysadm_r con newrole)
user_r           user_t (usuario confinado básico)
```

| Rol | Descripción |
|---|---|
| `system_r` | Servicios y daemons del sistema |
| `unconfined_r` | Procesos sin confinar |
| `object_r` | Objetos no-proceso (archivos, puertos) — siempre `object_r` |

> En la política `targeted`, los archivos siempre tienen rol `object_r` y los servicios `system_r`. El rol se vuelve crucial en políticas MLS donde controla qué administradores pueden hacer qué.

### Campo 3: SELinux type (type_t) — EL MÁS IMPORTANTE

El tipo es el campo que realmente importa en la política `targeted`. Toda la lógica de seguridad se basa en la relación entre tipos.

```
Para procesos, el tipo se llama "dominio":
  httpd_t      → dominio de Apache
  sshd_t       → dominio de SSH
  named_t      → dominio de BIND
  mysqld_t     → dominio de MySQL
  unconfined_t → dominio sin restricciones

Para archivos, el tipo indica la categoría:
  httpd_sys_content_t    → archivos web (lectura)
  httpd_sys_rw_content_t → archivos web (lectura/escritura)
  httpd_log_t            → logs de Apache
  httpd_config_t         → configuración de Apache
  shadow_t               → /etc/shadow
  etc_t                  → /etc/* genérico
  user_home_t            → archivos en /home
  tmp_t                  → /tmp
  var_log_t              → /var/log genérico

Para puertos:
  http_port_t            → 80, 443, 8080...
  ssh_port_t             → 22
  smtp_port_t            → 25, 587
```

La política dice: **dominio X puede hacer operación Y sobre tipo Z**.

```
allow httpd_t httpd_sys_content_t : file { read open getattr };
      ───────  ───────────────────         ───────────────────
      dominio        tipo                    operaciones

"El proceso Apache puede leer archivos web"
```

### Campo 4: Level (s0, s0-s0:c0.c1023)

El nivel de seguridad. En la política `targeted`, casi siempre es `s0` (no se usa activamente). Es relevante en MLS y en contenedores:

```
Targeted:     s0                          (siempre, no importa)
MLS:          s0 (unclassified) → s3 (top secret)
Contenedores: s0:c123,c456               (categorías para aislar)
```

En la política `targeted` puedes ignorar este campo para efectos prácticos.

### Resumen de importancia

```
En política targeted:

  user_u : role_r : type_t : s0
  ──────   ──────   ──────   ──
  casi      casi     ████    siempre
  siempre   siempre  ████    s0
  el mismo  el mismo ████
                     ████ ← ESTO es lo que importa

El 95% del trabajo con SELinux en targeted
consiste en gestionar TIPOS.
```

---

## 3. Ver contextos: las opciones -Z

La opción `-Z` es universal en herramientas de sistema para mostrar contextos SELinux.

### Archivos: ls -Z

```bash
$ ls -Z /var/www/html/
system_u:object_r:httpd_sys_content_t:s0 index.html
system_u:object_r:httpd_sys_content_t:s0 style.css
system_u:object_r:httpd_sys_rw_content_t:s0 uploads/

$ ls -Zd /var/www/html/
system_u:object_r:httpd_sys_content_t:s0 /var/www/html/
# -d: contexto del directorio mismo, no de su contenido

$ ls -Z /etc/shadow
system_u:object_r:shadow_t:s0 /etc/shadow

$ ls -Z /tmp/
unconfined_u:object_r:user_tmp_t:s0 test.txt
system_u:object_r:tmp_t:s0 systemd-private-...
```

### Procesos: ps -Z

```bash
$ ps -eZ | grep httpd
system_u:system_r:httpd_t:s0      1234 ?    00:00:05 httpd
system_u:system_r:httpd_t:s0      1235 ?    00:00:02 httpd
system_u:system_r:httpd_t:s0      1236 ?    00:00:01 httpd

$ ps -eZ | grep sshd
system_u:system_r:sshd_t:s0-s0:c0.c1023  890 ?  00:00:00 sshd

$ ps -eZ | grep bash
unconfined_u:unconfined_r:unconfined_t:s0-s0:c0.c1023 5678 pts/0 00:00:00 bash
# ^^^ shell de un usuario de login → unconfined (no confinado)
```

### Usuario actual: id -Z

```bash
$ id -Z
unconfined_u:unconfined_r:unconfined_t:s0-s0:c0.c1023
# Contexto de tu sesión de login actual

# Como root:
# id -Z
# unconfined_u:unconfined_r:unconfined_t:s0-s0:c0.c1023
# Root también es unconfined en política targeted
```

### Puertos: semanage port -l

```bash
$ semanage port -l | head -20
SELinux Port Type              Proto    Port Number
afs3_callback_port_t           tcp      7001
...
http_port_t                    tcp      80, 81, 443, 488, 8008, 8009, 8443, 9000
...
ssh_port_t                     tcp      22
smtp_port_t                    tcp      25, 465, 587
...

$ semanage port -l | grep http
http_cache_port_t              tcp      8080, 8118, 8123, 10001-10010
http_port_t                    tcp      80, 81, 443, 488, 8008, 8009, 8443, 9000
```

### Otras herramientas con -Z

```bash
# Sockets de red
ss -Ztlnp
# Muestra contexto SELinux de cada socket abierto

# Conexiones netstat
netstat -Ztlnp

# Copiar archivos preservando contexto
cp --preserve=context source dest
cp -Z source dest    # equivalente

# Crear archivos con contexto específico
install -Z system_u:object_r:httpd_sys_content_t:s0 source dest
```

---

## 4. Contextos de archivos

### De dónde viene el contexto de un archivo

El contexto de un archivo se determina por **la ruta donde reside**, definido en la base de datos `file_contexts`:

```bash
# Ver las reglas de contexto por ruta
semanage fcontext -l | head -20
# /.*                   all files    system_u:object_r:default_t:s0
# /bin/.*               all files    system_u:object_r:bin_t:s0
# /etc/.*               all files    system_u:object_r:etc_t:s0
# /var/www(/.*)?        all files    system_u:object_r:httpd_sys_content_t:s0
# /var/log(/.*)?        all files    system_u:object_r:var_log_t:s0

# Buscar contexto esperado de una ruta
semanage fcontext -l | grep '/var/www'
# /var/www(/.*)?        all files    system_u:object_r:httpd_sys_content_t:s0
# /var/www/html(/.*)?/wp-content(/.*)?  all files  ...httpd_sys_rw_content_t:s0

# O con matchpathcon:
matchpathcon /var/www/html/index.html
# /var/www/html/index.html    system_u:object_r:httpd_sys_content_t:s0

matchpathcon /etc/shadow
# /etc/shadow    system_u:object_r:shadow_t:s0
```

### Contextos comunes de archivos

```
Ruta                        Tipo                          Propósito
──────────────              ──────────────────────        ────────────────
/var/www/html/*             httpd_sys_content_t           Web (solo lectura)
/var/www/html/*/uploads     httpd_sys_rw_content_t        Web (lectura+escritura)
/var/www/cgi-bin/*          httpd_sys_script_exec_t       CGI ejecutable
/etc/httpd/*                httpd_config_t                Config Apache
/var/log/httpd/*            httpd_log_t                   Logs Apache
/etc/ssh/*                  sshd_key_t / etc_t            Config SSH
/etc/shadow                 shadow_t                      Contraseñas
/etc/passwd                 passwd_file_t                 Usuarios
/home/*                     user_home_t                   Homes
/home/*/.ssh                ssh_home_t                    Claves SSH
/tmp/*                      tmp_t                         Temporal
/var/tmp/*                  tmp_t                         Temporal persistente
/usr/bin/*                  bin_t                         Binarios
/usr/sbin/*                 bin_t                         Binarios admin
/usr/lib/systemd/system/*   systemd_unit_file_t           Unit files
/var/lib/mysql/*            mysqld_db_t                   Datos MySQL
/var/lib/pgsql/*            postgresql_db_t               Datos PostgreSQL
```

### Cuándo el contexto se pierde o es incorrecto

```
1. Archivo CREADO en una ubicación → hereda del directorio padre
   touch /var/www/html/new.html → httpd_sys_content_t ✓

2. Archivo MOVIDO (mv) → CONSERVA el contexto original
   mv /tmp/page.html /var/www/html/
   → page.html sigue con tmp_t ✗ (incorrecto)

3. Archivo COPIADO (cp) → hereda del directorio destino
   cp /tmp/page.html /var/www/html/
   → page.html obtiene httpd_sys_content_t ✓

4. Archivo creado con SELinux disabled → sin contexto
   → default_t o unlabeled_t ✗
```

```
  mv vs cp — la distinción más importante:

  mv:  mueve el inodo → el contexto viaja con el archivo
       ├── /tmp/file (tmp_t) ──mv──► /var/www/html/file (tmp_t) ✗ MAL
       └── Solución: restorecon -v /var/www/html/file

  cp:  crea nuevo inodo → el contexto se asigna por la ruta
       └── /tmp/file (tmp_t) ──cp──► /var/www/html/file (httpd_sys_content_t) ✓ OK
```

> **Regla de oro**: Después de mover (`mv`) archivos a su ubicación final, siempre ejecutar `restorecon -Rv /ruta/destino/`. Copiar (`cp`) no necesita restorecon.

---

## 5. Contextos de procesos (dominios)

El contexto de un proceso se llama **dominio**. Determina qué puede hacer el proceso.

### Dominios comunes

```bash
$ ps -eZ | grep -E 'httpd|sshd|named|mysql|postfix' | awk '{print $1, $NF}'
system_u:system_r:httpd_t:s0          httpd
system_u:system_r:sshd_t:s0           sshd
system_u:system_r:named_t:s0          named
system_u:system_r:mysqld_t:s0         mysqld
system_u:system_r:postfix_master_t:s0 master
```

| Dominio | Servicio | Qué puede acceder |
|---|---|---|
| `httpd_t` | Apache/Nginx | `httpd_sys_content_t`, `httpd_log_t`, `http_port_t` |
| `sshd_t` | SSH daemon | `sshd_key_t`, `ssh_port_t`, `/etc/shadow` |
| `named_t` | BIND DNS | `named_zone_t`, `named_conf_t`, `dns_port_t` |
| `mysqld_t` | MySQL/MariaDB | `mysqld_db_t`, `mysqld_log_t`, `mysqld_port_t` |
| `postfix_master_t` | Postfix | `postfix_spool_t`, `smtp_port_t` |
| `unconfined_t` | Usuarios de login | Todo (no confinado) |
| `initrc_t` | Init scripts sin política | Transicional |
| `kernel_t` | Kernel | Todo |

### unconfined_t: el dominio libre

En la política `targeted`, los procesos lanzados por usuarios de login corren como `unconfined_t`:

```bash
# Tu sesión de shell
$ id -Z
unconfined_u:unconfined_r:unconfined_t:s0

# Un script que ejecutas manualmente
$ ps -eZ | grep my_script
unconfined_u:unconfined_r:unconfined_t:s0  ... my_script.sh

# unconfined_t puede hacer CASI todo
# Es como si SELinux no existiera para ese proceso
# PERO: sigue sin poder acceder a ciertos tipos protegidos
```

`unconfined_t` no es completamente libre — hay reglas que ni siquiera `unconfined_t` puede romper (como acceder directamente a `kernel_t`). Pero para efectos prácticos, los usuarios no confinados operan sin restricciones de SELinux.

---

## 6. Contextos de usuarios

### Mapeo de usuarios Unix a SELinux

```bash
# Ver el mapeo
semanage login -l
# Login Name    SELinux User     MLS/MCS Range       Service
# __default__   unconfined_u     s0-s0:c0.c1023      *
# root          unconfined_u     s0-s0:c0.c1023      *

# __default__ = todos los usuarios Unix que no tienen mapeo explícito
# Resultado: todos los usuarios → unconfined_u
```

### Confinar usuarios (opcional, avanzado)

Se puede mapear usuarios Unix a usuarios SELinux confinados:

```bash
# Confinar al usuario "intern" a user_u (muy restrictivo)
semanage login -a -s user_u intern
# user_u solo puede:
#   - Leer/escribir sus archivos
#   - Ejecutar programas de usuario
#   - NO puede usar su, sudo, montar dispositivos, etc.

# Confinar a "developer" a staff_u (puede escalar a sysadm con newrole)
semanage login -a -s staff_u developer

# Ver el mapeo actualizado
semanage login -l

# Eliminar un mapeo
semanage login -d intern
```

| Usuario SELinux | Restricciones | Caso de uso |
|---|---|---|
| `unconfined_u` | Ninguna (libertad completa) | Default en targeted |
| `user_u` | Sin su, sudo, red de servidor | Usuarios muy restringidos |
| `staff_u` | Puede sudo, puede transicionar a sysadm_r | Staff de TI |
| `sysadm_u` | Administración completa pero auditada | Admins |
| `guest_u` | Sin red, sin ejecución desde home | Invitados |
| `xguest_u` | Solo navegador web, sin archivos | Kioscos |

---

## 7. Contextos de puertos

SELinux controla qué dominios pueden usar qué puertos de red.

```bash
# Listar todos los puertos etiquetados
semanage port -l

# Buscar puertos para un servicio
semanage port -l | grep http
# http_cache_port_t     tcp   8080, 8118, 8123, 10001-10010
# http_port_t           tcp   80, 81, 443, 488, 8008, 8009, 8443, 9000

semanage port -l | grep ssh
# ssh_port_t            tcp   22

semanage port -l | grep mysql
# mysqld_port_t         tcp   1186, 3306, 63132-63164
```

### Relación dominio ↔ puerto

```
La política define:

  httpd_t puede bind/listen en http_port_t (80, 443, 8080...)
  httpd_t NO puede bind/listen en ssh_port_t (22)
  httpd_t NO puede bind/listen en un puerto sin tipo

Si Apache intenta escuchar en un puerto no registrado:

  Listen 9090   ← en httpd.conf

  $ systemctl restart httpd
  Job for httpd.service failed.

  $ ausearch -m AVC -ts recent
  type=AVC denied { name_bind } ... scontext=httpd_t tcontext=unreserved_port_t

  → SELinux deniega porque 9090 no está en http_port_t
```

La solución (se verá en detalle en S02) es registrar el puerto:

```bash
semanage port -a -t http_port_t -p tcp 9090
```

---

## 8. Transiciones de dominio

Un concepto crucial: cuando un proceso lanza otro, el proceso hijo puede **cambiar de dominio** automáticamente. Esto se llama **transición de dominio**.

### Por qué son necesarias

```
Sin transiciones:
  init (kernel_t) → fork → httpd → ¿sigue siendo kernel_t?
  ¡No! Eso daría a Apache todos los permisos del kernel

Con transiciones:
  init (init_t) → exec httpd → transiciona a httpd_t
  El dominio cambia automáticamente al ejecutar el binario
```

### Cómo funciona

```
Tres condiciones necesarias para una transición de dominio:

1. El dominio origen tiene permiso de EJECUTAR el binario
   allow init_t httpd_exec_t : file { execute };

2. El dominio origen tiene permiso de TRANSICIONAR al destino
   allow init_t httpd_t : process { transition };

3. Existe una regla de TYPE TRANSITION
   type_transition init_t httpd_exec_t : process httpd_t;
   "Cuando init_t ejecuta un archivo de tipo httpd_exec_t,
    el nuevo proceso obtiene dominio httpd_t"
```

```
Flujo de una transición:

  ┌────────┐     exec      ┌────────────────┐     nuevo proceso    ┌─────────┐
  │ init_t │ ─────────────►│ httpd_exec_t   │ ───────────────────► │ httpd_t │
  │        │               │ (/usr/sbin/    │                      │         │
  │(proceso│               │   httpd)       │                      │(proceso │
  │ padre) │               │ (binario)      │                      │  hijo)  │
  └────────┘               └────────────────┘                      └─────────┘
   dominio                  tipo del archivo                        nuevo dominio
   origen                   ejecutable
```

### Ver el tipo de un ejecutable

```bash
$ ls -Z /usr/sbin/httpd
system_u:object_r:httpd_exec_t:s0 /usr/sbin/httpd
#                 ^^^^^^^^^^^^^
#                 tipo exec → trigger de transición

$ ls -Z /usr/sbin/sshd
system_u:object_r:sshd_exec_t:s0 /usr/sbin/sshd

$ ls -Z /usr/sbin/named
system_u:object_r:named_exec_t:s0 /usr/sbin/named

# Patrón: servicio_exec_t → al ejecutar, transiciona a servicio_t
#   httpd_exec_t → httpd_t
#   sshd_exec_t  → sshd_t
#   named_exec_t → named_t
```

### Transición de usuario

Cuando un usuario hace login via SSH:

```
sshd (sshd_t)
  │
  └── usuario hace login
      │
      ├── exec /bin/bash
      │     tipo: shell_exec_t
      │
      └── transiciona a:
            unconfined_u:unconfined_r:unconfined_t
            (para usuarios no confinados)
            o
            user_u:user_r:user_t
            (para usuarios confinados)
```

### Verificar qué transiciones están permitidas

```bash
# ¿A qué dominios puede transicionar init_t?
sesearch --type_trans -s init_t | head -10
# type_transition init_t httpd_exec_t : process httpd_t;
# type_transition init_t sshd_exec_t : process sshd_t;
# type_transition init_t named_exec_t : process named_t;
# ...

# ¿Qué dominio obtiene un proceso al ejecutar httpd_exec_t?
sesearch --type_trans -t httpd_exec_t
# type_transition init_t httpd_exec_t : process httpd_t;
```

> `sesearch` es parte del paquete `setools-console` (RHEL) o `setools` (Debian).

---

## 9. Errores comunes

### Error 1: No entender por qué mv causa problemas y cp no

```bash
# Escenario: desplegar una web nueva
tar xzf website.tar.gz -C /tmp/website/
# Archivos en /tmp tienen tipo tmp_t

# MAL — mover
mv /tmp/website/* /var/www/html/
ls -Z /var/www/html/index.html
# ...tmp_t...    ← tipo INCORRECTO, Apache no puede leer

# BIEN opción A — copiar (no mover)
cp -a /tmp/website/* /var/www/html/
ls -Z /var/www/html/index.html
# ...httpd_sys_content_t...    ← tipo correcto ✓

# BIEN opción B — mover + restorecon
mv /tmp/website/* /var/www/html/
restorecon -Rv /var/www/html/
# Relabeled /var/www/html/index.html from ...tmp_t to ...httpd_sys_content_t
```

### Error 2: Mirar solo el tipo y olvidar que DAC también aplica

```bash
# Archivo con contexto correcto pero permisos Unix incorrectos
ls -laZ /var/www/html/secret.html
# -rw------- root root system_u:object_r:httpd_sys_content_t:s0 secret.html
#  ^^^^^^^
#  Solo root puede leer (DAC)

# SELinux permite (httpd_t → httpd_sys_content_t ✓)
# DAC deniega (apache no es root, no tiene permiso ✗)
# Resultado: 403 Forbidden

# Necesitas AMBOS correctos:
chmod 644 /var/www/html/secret.html    # arreglar DAC
# Y el contexto ya estaba bien          # SELinux OK
```

### Error 3: Confundir el tipo de un ejecutable con el dominio del proceso

```bash
# El ARCHIVO /usr/sbin/httpd tiene tipo httpd_exec_t
ls -Z /usr/sbin/httpd
# ...httpd_exec_t...

# El PROCESO httpd tiene dominio httpd_t
ps -eZ | grep httpd
# ...httpd_t...

# httpd_exec_t ≠ httpd_t
# httpd_exec_t es el trigger: "cuando alguien ejecuta este archivo"
# httpd_t es el dominio resultante: "el proceso resultante tiene estos permisos"
```

### Error 4: Asumir que root no está sujeto a contextos

```bash
# root lanza un script manualmente
$ id -Z
unconfined_u:unconfined_r:unconfined_t:s0

# El script corre como unconfined_t (no confinado) ✓
# root PARECE libre, pero:

# Si root ejecuta directamente httpd:
# El binario httpd_exec_t causa una transición → httpd_t
# Ahora el proceso está confinado como httpd_t
# incluso siendo root quien lo lanzó

# root no puede evitar la transición de dominio
# (a menos que use runcon o cambie el tipo del binario)
```

### Error 5: No saber dónde buscar el contexto de un puerto

```bash
# Síntoma: servicio no arranca con "Permission denied" o "bind failed"
# Primer instinto: revisar permisos de archivos
# Realidad: puede ser un problema de PUERTOS

# ¿El puerto tiene tipo asignado?
semanage port -l | grep 8443
# http_port_t    tcp    ..., 8443, ...    ← sí, OK

semanage port -l | grep 9090
# (vacío)    ← no tiene tipo asignado, SELinux bloqueará el bind
```

---

## 10. Cheatsheet

```
=== FORMATO DEL CONTEXTO ===
user_u : role_r : type_t : level
  │        │        │        │
  │        │        │        └── MLS level (s0 en targeted)
  │        │        └── TIPO ← lo más importante en targeted
  │        └── Rol (system_r, object_r, unconfined_r)
  └── Usuario SELinux (system_u, unconfined_u)

=== VER CONTEXTOS ===
ls -Z              archivos (contexto SELinux)
ls -Zd             directorio mismo (no contenido)
ps -eZ             procesos (dominio)
id -Z              sesión actual del usuario
ss -Ztlnp          sockets de red
semanage port -l   puertos etiquetados

=== TIPOS COMUNES DE ARCHIVOS ===
httpd_sys_content_t        /var/www/html (lectura)
httpd_sys_rw_content_t     uploads web (lectura/escritura)
httpd_log_t                /var/log/httpd
httpd_config_t             /etc/httpd
shadow_t                   /etc/shadow
etc_t                      /etc/* genérico
user_home_t                /home/*
tmp_t                      /tmp
var_log_t                  /var/log genérico
bin_t                      /usr/bin, /usr/sbin
mysqld_db_t                /var/lib/mysql
default_t                  sin tipo asignado (problema)

=== DOMINIOS COMUNES DE PROCESOS ===
httpd_t         Apache/Nginx
sshd_t          SSH
named_t         BIND
mysqld_t        MySQL/MariaDB
postfix_master_t  Postfix
unconfined_t    Usuarios de login (no confinados)
init_t          PID 1 (systemd)
kernel_t        Kernel

=== CONTEXTOS DE ARCHIVOS ===
matchpathcon /ruta              qué contexto DEBERÍA tener
semanage fcontext -l | grep X   buscar reglas de contexto
mv conserva contexto original   → necesita restorecon
cp hereda contexto del destino  → correcto automáticamente

=== TRANSICIONES DE DOMINIO ===
httpd_exec_t → httpd_t          ejecutar binario → nuevo dominio
sshd_exec_t → sshd_t
sesearch --type_trans -s DOM    ver transiciones desde un dominio

=== USUARIOS SELINUX ===
semanage login -l               ver mapeo Unix → SELinux
unconfined_u                    usuario libre (default en targeted)
system_u                        servicios del sistema
user_u                          usuario confinado (restrictivo)
staff_u                         staff (puede escalar a sysadm)

=== PUERTOS ===
semanage port -l                listar todos los puertos etiquetados
semanage port -l | grep TYPE    buscar puertos de un tipo
http_port_t    80,443,8080...   puertos web
ssh_port_t     22               puertos SSH
```

---

## 11. Ejercicios

### Ejercicio 1: Leer contextos

Ejecuta los siguientes comandos (o interprétalos si no tienes acceso a una máquina con SELinux) y responde:

```bash
$ ls -Z /var/www/html/index.html
system_u:object_r:httpd_sys_content_t:s0 /var/www/html/index.html

$ ls -Z /home/alice/backup.html
unconfined_u:object_r:user_home_t:s0 /home/alice/backup.html

$ ps -eZ | grep httpd
system_u:system_r:httpd_t:s0     1234 ?  00:00:05 httpd

$ ls -Z /usr/sbin/httpd
system_u:object_r:httpd_exec_t:s0 /usr/sbin/httpd
```

1. ¿`httpd` (proceso) puede leer `/var/www/html/index.html`? ¿Por qué?
2. ¿`httpd` puede leer `/home/alice/backup.html`? ¿Por qué?
3. ¿Cuál es la diferencia entre `httpd_t`, `httpd_exec_t` y `httpd_sys_content_t`?
4. ¿Por qué `/usr/sbin/httpd` (archivo) tiene tipo `httpd_exec_t` y no `httpd_t`?
5. Si un usuario hace `cp /home/alice/backup.html /var/www/html/`, ¿qué contexto tendrá el archivo destino?
6. Si en vez de `cp` usa `mv`, ¿qué contexto tendrá? ¿Qué consecuencia tiene?

> **Pregunta de predicción**: Si alice ejecuta `python3 -m http.server 8080` desde `/home/alice/`, ¿el proceso correrá como `httpd_t` o como `unconfined_t`? ¿Por qué? ¿Puede servir archivos de `/home/alice/`?

### Ejercicio 2: Predecir violaciones

Dado este estado del sistema:

```
Proceso:  httpd_t (Apache)
Política: httpd_t puede leer httpd_sys_content_t
          httpd_t puede escribir httpd_log_t
          httpd_t puede escuchar en http_port_t (80, 443)
```

Predice el resultado de cada operación (permitida o denegada por SELinux):

| Operación de httpd_t | Tipo del objetivo | ¿Resultado? |
|---|---|---|
| Leer `/var/www/html/index.html` | `httpd_sys_content_t` | |
| Leer `/etc/shadow` | `shadow_t` | |
| Escribir `/var/log/httpd/access_log` | `httpd_log_t` | |
| Escribir `/var/www/html/hack.php` | `httpd_sys_content_t` | |
| Escuchar en puerto 80 | `http_port_t` | |
| Escuchar en puerto 22 | `ssh_port_t` | |
| Escuchar en puerto 9090 | (sin tipo) | |
| Ejecutar `/bin/bash` | `shell_exec_t` | |

> **Pregunta de predicción**: Para la última fila (ejecutar `/bin/bash`), si SELinux permite la ejecución, ¿el shell resultante correrá como `httpd_t` o como `unconfined_t`? ¿Existe una transición de dominio `httpd_t → shell_exec_t → ???`?

### Ejercicio 3: Diagnosticar un despliegue web

Un administrador despliega un sitio web nuevo con estos pasos:

```bash
tar xzf website.tar.gz -C /tmp/
mv /tmp/website/* /var/www/html/nueva-app/
chown -R apache:apache /var/www/html/nueva-app/
chmod -R 755 /var/www/html/nueva-app/
systemctl restart httpd
```

Al acceder a `http://server/nueva-app/`, Apache devuelve 403 Forbidden.

1. ¿Cuál es la causa más probable?
2. ¿Qué comando usarías para confirmar tu hipótesis? (pista: `ls -Z`)
3. ¿Qué tipo tienen los archivos ahora? ¿Qué tipo deberían tener?
4. ¿Qué comando arregla el problema?
5. ¿Qué paso del procedimiento original debería cambiar para evitar este problema en el futuro?

> **Pregunta de predicción**: Si el administrador hubiera usado `tar xzf website.tar.gz -C /var/www/html/nueva-app/` directamente (sin pasar por /tmp), ¿se habría evitado el problema? ¿Por qué?

---

> **Siguiente tema**: T04 — Política targeted (qué procesos están confinados, unconfined_t).
