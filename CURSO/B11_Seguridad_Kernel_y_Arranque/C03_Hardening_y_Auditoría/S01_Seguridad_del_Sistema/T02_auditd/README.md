# auditd: reglas de auditoría, ausearch, aureport

## Índice

1. [Qué es auditd y por qué existe](#1-qué-es-auditd-y-por-qué-existe)
2. [Arquitectura del sistema de auditoría](#2-arquitectura-del-sistema-de-auditoría)
3. [Configuración: auditd.conf](#3-configuración-auditdconf)
4. [Reglas de auditoría](#4-reglas-de-auditoría)
5. [auditctl: gestión de reglas en vivo](#5-auditctl-gestión-de-reglas-en-vivo)
6. [Reglas persistentes: audit.rules](#6-reglas-persistentes-auditrules)
7. [ausearch: buscar eventos](#7-ausearch-buscar-eventos)
8. [aureport: reportes resumidos](#8-aureport-reportes-resumidos)
9. [Escenarios prácticos](#9-escenarios-prácticos)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué es auditd y por qué existe

El sistema de auditoría de Linux registra eventos de seguridad a nivel del
kernel. No es un sistema de monitorización general — es un sistema forense
diseñado para responder la pregunta:

> **¿Quién hizo qué, cuándo, y con qué resultado?**

```
Niveles de logging en Linux
════════════════════════════

  Nivel           Herramienta       Registra
  ─────           ───────────       ────────
  Aplicación      syslog/journal    Mensajes del servicio
                                    "nginx: client connected"

  Sistema         systemd-journald  Eventos del sistema
                                    "service started/stopped"

  Seguridad       auditd            Syscalls, acceso a archivos,
                                    cambios de usuarios, intentos
                                    de acceso, ejecución de programas
                                    → Nivel KERNEL, no evasible
```

### Por qué no basta con syslog

```
syslog/journald vs auditd
═══════════════════════════

  syslog/journald:
    → El programa DECIDE qué registrar
    → Un atacante puede modificar el programa para no loguear
    → No registra syscalls ni accesos a archivos
    → Formato inconsistente entre aplicaciones

  auditd:
    → El KERNEL registra los eventos
    → El programa no puede evitar ser auditado
    → Registra syscalls (open, execve, connect, etc.)
    → Formato estructurado y consistente
    → Requerido por estándares: PCI-DSS, HIPAA, SOC2, ISO 27001
```

### Qué puede auditar

```
Capacidades de auditd
══════════════════════

  ✓ Acceso a archivos     ¿Quién leyó/modificó /etc/shadow?
  ✓ Ejecución de programas ¿Quién ejecutó /usr/bin/passwd?
  ✓ Llamadas al sistema   ¿Qué syscall hizo el proceso 1234?
  ✓ Cambios de usuario    ¿Quién hizo su o sudo?
  ✓ Conexiones de red     ¿Quién conectó al puerto 22?
  ✓ Cambios de permisos   ¿Quién cambió permisos de /etc/?
  ✓ Módulos del kernel    ¿Quién cargó un módulo?
  ✓ Eventos de SELinux     AVCs y cambios de política
  ✓ Login/logout          ¿Quién entró y salió del sistema?
```

---

## 2. Arquitectura del sistema de auditoría

```
Arquitectura del audit framework
══════════════════════════════════

  ┌─────────────────────────────────────────────┐
  │                 KERNEL                       │
  │                                              │
  │  Syscall ──► Audit hook ──► ¿Coincide       │
  │                               con regla?     │
  │                               │              │
  │                    NO ◄───────┤              │
  │                    (ignorar)  │              │
  │                               SÍ             │
  │                               │              │
  │                               ▼              │
  │                         Generar evento       │
  │                               │              │
  └───────────────────────────────┼──────────────┘
                                  │
                           netlink socket
                                  │
                                  ▼
                          ┌───────────────┐
                          │    auditd     │
                          │   (demonio)   │
                          └───────┬───────┘
                                  │
                    ┌─────────────┼─────────────┐
                    │             │             │
                    ▼             ▼             ▼
            /var/log/audit/   audisp         syslog
            audit.log         (dispatcher)   (opcional)
                              │
                              ▼
                         Plugins:
                         - af_unix
                         - syslog
                         - au-remote
                         - sedispatch (SELinux)
```

### Componentes

```
Componentes del audit framework
═════════════════════════════════

  Kernel:
    audit hooks       → Interceptan syscalls según las reglas
    kauditd           → Thread del kernel que envía eventos

  Espacio de usuario:
    auditd            → Demonio que recibe y escribe eventos
    auditctl          → Gestionar reglas en vivo (temporales)
    augenrules        → Compilar reglas persistentes
    ausearch          → Buscar eventos en los logs
    aureport          → Generar reportes resumidos
    autrace           → Rastrear un proceso (como strace pero audit)
    aulast            → Como last pero usando audit logs
    audisp-plugins    → Plugins para enviar eventos a otros sistemas
```

### Paquetes necesarios

```bash
# RHEL / CentOS / Rocky / Fedora
sudo dnf install audit          # auditd + herramientas

# Ubuntu / Debian
sudo apt install auditd audispd-plugins

# Verificar que está activo
systemctl status auditd

# Estado del subsistema en el kernel
sudo auditctl -s
# enabled 1
# failure 1
# pid 1234
# ...
```

---

## 3. Configuración: auditd.conf

```bash
# Archivo principal: /etc/audit/auditd.conf
```

### Parámetros clave

```bash
# /etc/audit/auditd.conf (extracto con explicaciones)

log_file = /var/log/audit/audit.log
# Ruta del archivo de log principal

log_format = ENRICHED
# RAW: IDs numéricos (uid=1000)
# ENRICHED: nombres resueltos (uid=1000, auid=alice)

log_group = root
# Grupo que puede leer el log (cambiar a "adm" si necesitas
# que usuarios no-root lean los logs)

num_logs = 5
# Número de archivos de log a mantener en rotación

max_log_file = 8
# Tamaño máximo por archivo en MB

max_log_file_action = ROTATE
# Qué hacer cuando el log alcanza max_log_file:
#   ROTATE    → Rotar al siguiente archivo
#   SYSLOG    → Enviar a syslog y continuar
#   SUSPEND   → Dejar de loguear
#   KEEP_LOGS → Rotar sin límite de num_logs
#   IGNORE    → No hacer nada (peligroso)

space_left = 75
space_left_action = SYSLOG
# Cuando queda poco espacio: avisar por syslog

admin_space_left = 50
admin_space_left_action = SUSPEND
# Cuando queda MUY poco espacio: dejar de loguear

disk_full_action = SUSPEND
# Disco lleno: suspender auditoría
# En entornos de alta seguridad: HALT (apagar el sistema)

disk_error_action = SUSPEND
# Error de disco: suspender

flush = INCREMENTAL_ASYNC
# Cómo escribir al disco:
#   NONE               → Buffer del kernel decide
#   INCREMENTAL        → Cada freq registros
#   INCREMENTAL_ASYNC  → Asíncrono con flushes periódicos
#   DATA               → Cada registro (más lento, más seguro)
#   SYNC               → Sincrónico total

freq = 50
# Con INCREMENTAL: flush cada 50 registros
```

### Configuración para cumplimiento normativo

```
Según el estándar de seguridad:
════════════════════════════════

  PCI-DSS / HIPAA / SOC2:
    max_log_file_action = KEEP_LOGS    (no perder datos)
    space_left_action = email          (alertar al admin)
    admin_space_left_action = HALT     (detener si no hay espacio)
    disk_full_action = HALT            (integridad > disponibilidad)
    flush = SYNC                       (cada evento al disco)

  Entorno general:
    max_log_file_action = ROTATE       (balance rendimiento/retención)
    space_left_action = SYSLOG         (aviso suficiente)
    flush = INCREMENTAL_ASYNC          (buen rendimiento)
```

---

## 4. Reglas de auditoría

Hay tres tipos de reglas:

```
Tipos de reglas de auditoría
═════════════════════════════

  1. Reglas de control
     → Configuran el comportamiento del subsistema
     → Ejemplo: -b 8192 (tamaño del buffer)

  2. Reglas de sistema de archivos (watches)
     → Monitorean acceso a archivos/directorios
     → Ejemplo: vigilar /etc/passwd

  3. Reglas de syscall
     → Monitorean llamadas al sistema específicas
     → Ejemplo: vigilar execve (ejecución de programas)
```

### Reglas de archivo (file watches)

```bash
# Formato:
# -w ruta -p permisos -k etiqueta
#    │        │            │
#    │        │            └── Key: etiqueta para buscar después
#    │        └── Permisos a vigilar
#    └── Ruta del archivo o directorio

# Permisos que se pueden vigilar:
#   r → read    (lectura)
#   w → write   (escritura)
#   x → execute (ejecución)
#   a → attribute change (cambio de atributos: chmod, chown)
```

```bash
# Ejemplos de file watches:

# Vigilar cambios en /etc/passwd
-w /etc/passwd -p wa -k identity_changes
# wa = write + attribute → cualquier modificación

# Vigilar acceso a /etc/shadow (lectura incluida)
-w /etc/shadow -p rwa -k shadow_access

# Vigilar cambios en sudoers
-w /etc/sudoers -p wa -k sudoers_changes
-w /etc/sudoers.d/ -p wa -k sudoers_changes

# Vigilar cambios en la configuración de red
-w /etc/sysconfig/network-scripts/ -p wa -k network_changes

# Vigilar acceso a claves SSH
-w /etc/ssh/sshd_config -p wa -k sshd_config
-w /home/ -p wa -k home_changes

# Vigilar cambios en cron
-w /etc/crontab -p wa -k cron_changes
-w /etc/cron.d/ -p wa -k cron_changes
-w /var/spool/cron/ -p wa -k cron_changes

# Vigilar módulos del kernel
-w /sbin/insmod -p x -k kernel_modules
-w /sbin/modprobe -p x -k kernel_modules
-w /sbin/rmmod -p x -k kernel_modules
```

### Reglas de syscall

```bash
# Formato:
# -a acción,lista -F campo=valor -S syscall -k etiqueta
#    │       │       │              │          │
#    │       │       │              │          └── Key para buscar
#    │       │       │              └── Syscall a vigilar
#    │       │       └── Filtro (uid, pid, path, etc.)
#    │       └── Lista de filtros (exit, task, user, exclude)
#    └── Acción (always = registrar, never = ignorar)

# Acciones:
#   -a always,exit  → Registrar cuando el syscall retorna
#   -a never,exit   → Nunca registrar (exclusión)

# Filtros comunes (-F):
#   arch=b64           → Solo para 64-bit
#   -F auid>=1000      → Solo usuarios reales (no servicios)
#   -F auid!=4294967295 → Excluir procesos sin login UID (unset)
#   -F euid=0          → Solo cuando corre como root
#   -F success=1       → Solo exitosos
#   -F success=0       → Solo fallidos
#   -F key=etiqueta    → Asignar key
```

```bash
# Ejemplos de reglas de syscall:

# Auditar toda ejecución de programas
-a always,exit -F arch=b64 -S execve -k program_execution

# Auditar cambios de hora del sistema
-a always,exit -F arch=b64 -S adjtimex -S settimeofday -S clock_settime \
   -k time_changes

# Auditar cambios de usuario/grupo
-a always,exit -F arch=b64 -S setuid -S setgid -S setreuid -S setregid \
   -F auid>=1000 -F auid!=4294967295 -k privilege_escalation

# Auditar creación/eliminación de archivos
-a always,exit -F arch=b64 -S unlink -S unlinkat -S rename -S renameat \
   -F auid>=1000 -F auid!=4294967295 -k file_deletion

# Auditar cambios de permisos
-a always,exit -F arch=b64 -S chmod -S fchmod -S fchmodat \
   -F auid>=1000 -F auid!=4294967295 -k permission_changes

# Auditar montaje de filesystems
-a always,exit -F arch=b64 -S mount -S umount2 \
   -F auid>=1000 -F auid!=4294967295 -k filesystem_mounts

# Auditar conexiones de red (connect)
-a always,exit -F arch=b64 -S connect -F auid>=1000 \
   -F auid!=4294967295 -k network_connect
```

### El campo auid (audit UID)

```
auid — el UID original del usuario
════════════════════════════════════

  auid = "audit user ID" = el UID con el que el usuario hizo LOGIN

  Flujo:
    alice (uid=1000) hace SSH login
      → auid = 1000
    alice ejecuta: sudo su - bob
      → uid cambia a bob (uid=1001)
      → auid sigue siendo 1000 (alice)
    bob ejecuta: sudo systemctl restart nginx
      → euid = 0 (root)
      → auid sigue siendo 1000 (alice)

  El auid NUNCA cambia después del login.
  Siempre sabes quién fue el humano original.

  auid = 4294967295 (-1 como unsigned)
    → Proceso que nunca pasó por un login
    → Servicios del sistema (systemd, NetworkManager, etc.)
    → Filtrar con: -F auid!=4294967295
```

---

## 5. auditctl: gestión de reglas en vivo

`auditctl` manipula las reglas en el kernel en tiempo real. Los cambios son
**temporales** (se pierden al reiniciar):

### Operaciones básicas

```bash
# Ver todas las reglas activas
sudo auditctl -l

# Ver estado del subsistema
sudo auditctl -s
# enabled 1         → Auditoría activa
# failure 1         → Acción ante error (1=printk, 2=panic)
# pid 1234          → PID de auditd
# backlog_limit 8192

# Agregar una regla de archivo
sudo auditctl -w /etc/passwd -p wa -k identity

# Agregar una regla de syscall
sudo auditctl -a always,exit -F arch=b64 -S execve -k exec

# Eliminar una regla específica
sudo auditctl -W /etc/passwd -p wa -k identity
#              ─┬─
#               └── -W (mayúscula) = eliminar file watch

# Eliminar una regla de syscall
sudo auditctl -d always,exit -F arch=b64 -S execve -k exec
#              ─┬─
#               └── -d = delete

# Eliminar TODAS las reglas
sudo auditctl -D

# Cambiar tamaño del buffer
sudo auditctl -b 16384
```

### Probar reglas antes de hacerlas persistentes

```bash
# 1. Agregar regla temporal
sudo auditctl -w /etc/shadow -p rwa -k test_shadow

# 2. Verificar que está activa
sudo auditctl -l
# -w /etc/shadow -p rwa -k test_shadow

# 3. Generar evento de prueba
sudo cat /etc/shadow > /dev/null

# 4. Buscar el evento
sudo ausearch -k test_shadow -ts recent

# 5. Si funciona como esperas → agregar a audit.rules
# 6. Si genera demasiado ruido → ajustar o eliminar
sudo auditctl -W /etc/shadow -p rwa -k test_shadow
```

---

## 6. Reglas persistentes: audit.rules

Las reglas persistentes se almacenan en archivos que se cargan al arrancar:

### Ubicación de los archivos

```
Archivos de reglas
═══════════════════

  RHEL 7+ / Ubuntu 18.04+:
  ─────────────────────────
  /etc/audit/rules.d/          ← Directorio de reglas
  ├── 10-base-config.rules     ← Buffer, backlog
  ├── 20-identity.rules        ← Archivos de identidad
  ├── 30-sysadmin.rules        ← Comandos de admin
  ├── 40-network.rules         ← Eventos de red
  ├── 50-custom.rules          ← Tus reglas custom
  └── 99-finalize.rules        ← Bloquear cambios

  augenrules --load
    → Lee todos los archivos en orden (10-, 20-, ...)
    → Genera /etc/audit/audit.rules
    → Carga las reglas en el kernel

  Nota: NO editar /etc/audit/audit.rules directamente
        (se sobrescribe con augenrules --load)
```

### Estructura recomendada

```bash
# ═══════ /etc/audit/rules.d/10-base-config.rules ═══════

# Eliminar reglas existentes
-D

# Tamaño del buffer (aumentar si hay backlog overflow)
-b 8192

# Qué hacer ante error (0=silent, 1=printk, 2=panic)
-f 1
```

```bash
# ═══════ /etc/audit/rules.d/20-identity.rules ═══════

# Archivos de identidad de usuarios
-w /etc/passwd -p wa -k identity
-w /etc/group -p wa -k identity
-w /etc/shadow -p wa -k identity
-w /etc/gshadow -p wa -k identity
-w /etc/security/opasswd -p wa -k identity

# Sudoers
-w /etc/sudoers -p wa -k sudoers
-w /etc/sudoers.d/ -p wa -k sudoers
```

```bash
# ═══════ /etc/audit/rules.d/30-sysadmin.rules ═══════

# Ejecución de programas
-a always,exit -F arch=b64 -S execve -F euid=0 -k root_commands

# Cambios de hora
-a always,exit -F arch=b64 -S adjtimex -S settimeofday \
   -S clock_settime -k time_change
-w /etc/localtime -p wa -k time_change

# Carga de módulos del kernel
-w /sbin/insmod -p x -k kernel_modules
-w /sbin/modprobe -p x -k kernel_modules
-w /sbin/rmmod -p x -k kernel_modules
-a always,exit -F arch=b64 -S init_module -S finit_module \
   -S delete_module -k kernel_modules

# Cambios en configuración de SSH
-w /etc/ssh/sshd_config -p wa -k sshd_config

# Acciones de login/logout
-w /var/log/lastlog -p wa -k login_events
-w /var/log/faillog -p wa -k login_events
-w /var/run/utmp -p wa -k session_events
-w /var/log/wtmp -p wa -k session_events
-w /var/log/btmp -p wa -k session_events
```

```bash
# ═══════ /etc/audit/rules.d/40-network.rules ═══════

# Cambios en configuración de red
-w /etc/hosts -p wa -k network_config
-w /etc/sysconfig/network -p wa -k network_config
-w /etc/resolv.conf -p wa -k network_config

# Reglas de firewall
-w /etc/firewalld/ -p wa -k firewall_changes
-w /etc/nftables/ -p wa -k firewall_changes
```

```bash
# ═══════ /etc/audit/rules.d/99-finalize.rules ═══════

# Hacer las reglas inmutables (requiere reboot para cambiar)
-e 2
# -e 0 = deshabilitado
# -e 1 = habilitado (default)
# -e 2 = inmutable (no se pueden cambiar sin reboot)
#         ¡IMPORTANTE! Poner siempre al FINAL
```

### Cargar reglas

```bash
# Generar y cargar reglas desde rules.d/
sudo augenrules --load

# Verificar que se cargaron
sudo auditctl -l

# Si no usas augenrules, cargar directamente:
sudo auditctl -R /etc/audit/audit.rules
```

---

## 7. ausearch: buscar eventos

`ausearch` es la herramienta principal para buscar eventos en los logs:

### Búsquedas comunes

```bash
# Por key (la etiqueta -k que pusiste en las reglas)
sudo ausearch -k identity
sudo ausearch -k sudoers

# Por tipo de mensaje
sudo ausearch -m USER_LOGIN           # Logins de usuario
sudo ausearch -m USER_CMD             # Comandos sudo
sudo ausearch -m AVC                  # Denegaciones SELinux
sudo ausearch -m APPARMOR_DENIED      # Denegaciones AppArmor
sudo ausearch -m EXECVE               # Ejecuciones
sudo ausearch -m USER_AUTH            # Autenticaciones

# Por proceso
sudo ausearch -c httpd                # Por nombre del comando
sudo ausearch -p 1234                 # Por PID

# Por usuario
sudo ausearch -ua 1000                # Por auid (audit UID)
sudo ausearch -ui 1000                # Por uid
sudo ausearch -ue 0                   # Por euid (effective)

# Por tiempo
sudo ausearch -ts recent              # Últimos 10 minutos
sudo ausearch -ts today               # Desde medianoche
sudo ausearch -ts "03/15/2026" "10:00:00"  # Fecha específica
sudo ausearch -ts "03/15/2026" -te "03/15/2026" "12:00:00"
#              ─┬─                  ─┬─
#               └── time start       └── time end

# Por resultado
sudo ausearch -sv yes                 # Solo exitosos (success=yes)
sudo ausearch -sv no                  # Solo fallidos

# Formato interpretado (resuelve UIDs a nombres)
sudo ausearch -k identity -i
#                          ─┬─
#                           └── interpreted: uid=1000 → alice

# Formato raw (para piping a audit2why, etc.)
sudo ausearch -k identity --raw
```

### Interpretar la salida

```bash
sudo ausearch -k identity -i
```

```
Anatomía de un evento de auditoría
════════════════════════════════════

  ----
  type=SYSCALL msg=audit(1710000000.123:4567):
    arch=x86_64 syscall=openat success=yes exit=3
    a0=ffffff9c a1=7fff... a2=241 a3=1b6
    items=1 ppid=1200 pid=1234 auid=alice uid=root
    gid=root euid=root ...
    comm="useradd" exe="/usr/sbin/useradd"
    key="identity"
  type=PATH msg=audit(1710000000.123:4567):
    item=0 name="/etc/passwd" inode=123456
    dev=fd:00 mode=file,644 ouid=root ogid=root
    obj=system_u:object_r:passwd_file_t:s0
  ----

  Campos clave:
  ─────────────
  type=SYSCALL    → Tipo de evento (syscall, path, user_cmd, etc.)
  msg=audit(...)  → Timestamp y serial (identifica el evento)
  syscall=openat  → Syscall ejecutada
  success=yes     → ¿Tuvo éxito?
  auid=alice      → Quién es el humano original (audit UID)
  uid=root        → UID al momento del evento (tras sudo)
  comm="useradd"  → Nombre del comando
  exe="/.../useradd" → Ruta completa del ejecutable
  key="identity"  → Key de la regla que capturó el evento

  type=PATH       → Información del archivo accedido
  name="/etc/passwd" → Ruta del archivo
  mode=file,644   → Permisos del archivo
```

### Combinar filtros

```bash
# Quién modificó /etc/passwd hoy como root
sudo ausearch -k identity -ts today -ue 0 -sv yes -i

# Qué comandos ejecutó alice en las últimas 2 horas
sudo ausearch -k root_commands -ua alice -ts "2 hours ago" -i

# Intentos fallidos de login hoy
sudo ausearch -m USER_LOGIN -sv no -ts today -i
```

---

## 8. aureport: reportes resumidos

`aureport` genera reportes estadísticos a partir de los logs:

### Reportes comunes

```bash
# Resumen general
sudo aureport --summary

# Reporte de autenticaciones
sudo aureport --auth
# Muestra: fecha, hora, usuario, terminal, host, resultado

# Reporte de logins
sudo aureport --login
# Muestra: logins exitosos y fallidos

# Reporte de ejecuciones
sudo aureport -x
# Muestra: programas ejecutados, quién, cuándo

# Reporte de archivos
sudo aureport -f
# Muestra: archivos accedidos

# Reporte de cambios de configuración
sudo aureport -c
# Muestra: cambios en la configuración de auditoría

# Reporte de eventos por key
sudo aureport -k
# Muestra: eventos agrupados por key

# Reporte de intentos fallidos
sudo aureport --failed
# Muestra: solo eventos con resultado fallido

# Reporte de eventos de un período
sudo aureport --login -ts today -te now
sudo aureport -x -ts "03/01/2026" -te "03/31/2026"
```

### Combinar aureport con ausearch

```bash
# Pipeline: filtrar con ausearch → resumir con aureport

# Reporte de ejecuciones de alice
sudo ausearch -ua alice --raw | aureport -x --summary

# Reporte de archivos accedidos por root hoy
sudo ausearch -ue 0 -ts today --raw | aureport -f --summary

# Reporte de logins fallidos esta semana
sudo ausearch -m USER_LOGIN -sv no -ts this-week --raw | aureport --login
```

### Ejemplo de salida

```bash
sudo aureport --login --summary -ts today

# Login Summary Report
# =====================
# total  auid
# =====================
# 15     alice
# 8      bob
# 3      deploy
# 2      (unset)
#
# Totals: 28 logins
# Failed logins: 2

sudo aureport -k --summary

# Key Summary Report
# ===================
# total  key
# ===================
# 145    root_commands
# 23     identity
# 12     sudoers
# 8      time_change
# 3      kernel_modules
```

---

## 9. Escenarios prácticos

### Escenario 1: ¿Quién modificó /etc/passwd?

```bash
# 1. Asegurarte de que existe la regla
sudo auditctl -l | grep passwd
# Si no existe:
sudo auditctl -w /etc/passwd -p wa -k identity

# 2. Buscar eventos
sudo ausearch -k identity -f /etc/passwd -ts today -i

# 3. Salida (interpretada):
# type=SYSCALL ... auid=alice uid=root comm="useradd"
#   exe="/usr/sbin/useradd" key="identity"
# type=PATH ... name="/etc/passwd"

# Respuesta: alice ejecutó useradd (como root vía sudo)
# que modificó /etc/passwd
```

### Escenario 2: intentos de acceso fallidos a SSH

```bash
# 1. Regla (ya incluida por defecto en muchas distribuciones)
# type=USER_LOGIN se genera automáticamente

# 2. Buscar
sudo ausearch -m USER_LOGIN -sv no -ts today -i

# 3. Reporte resumido
sudo aureport --login --failed -ts today

# 4. Detalle: desde qué IPs
sudo ausearch -m USER_LOGIN -sv no -ts today -i | grep addr=
```

### Escenario 3: auditar comandos ejecutados como root

```bash
# 1. Regla
sudo auditctl -a always,exit -F arch=b64 -S execve -F euid=0 \
   -F auid>=1000 -F auid!=4294967295 -k root_commands

# 2. Ejecutar algo como root
sudo systemctl restart nginx

# 3. Buscar
sudo ausearch -k root_commands -ts recent -i

# 4. Reporte: qué ejecutó cada usuario como root
sudo ausearch -k root_commands -ts today --raw | aureport -x --summary
```

### Escenario 4: detectar carga de módulos del kernel

```bash
# 1. Reglas
sudo auditctl -w /sbin/insmod -p x -k kernel_modules
sudo auditctl -w /sbin/modprobe -p x -k kernel_modules
sudo auditctl -a always,exit -F arch=b64 -S init_module \
   -S finit_module -S delete_module -k kernel_modules

# 2. Cargar un módulo (prueba)
sudo modprobe dummy

# 3. Buscar
sudo ausearch -k kernel_modules -ts recent -i

# Esto es crítico para detectar rootkits que cargan módulos
# maliciosos en el kernel
```

### Escenario 5: monitorear archivos sensibles específicos

```bash
# Vigilar quién lee claves privadas TLS
sudo auditctl -w /etc/pki/tls/private/ -p r -k tls_key_access
sudo auditctl -w /etc/ssl/private/ -p r -k tls_key_access

# Vigilar cambios en la configuración de auditd (meta-auditoría)
sudo auditctl -w /etc/audit/ -p wa -k audit_config

# Vigilar intentos de acceder a archivos de otros usuarios
sudo auditctl -a always,exit -F arch=b64 -S openat \
   -F dir=/home -F auid>=1000 -F auid!=4294967295 \
   -F success=0 -k home_access_denied
# success=0 → solo fallos (intentos denegados por DAC)
```

---

## 10. Errores comunes

### Error 1: demasiadas reglas sin filtros

```bash
# MAL — auditar TODA ejecución de programas por TODOS
-a always,exit -F arch=b64 -S execve -k all_exec
# Genera MILES de eventos por minuto
# El log se llena en horas, backlog overflow

# BIEN — filtrar por usuarios reales que actúan como root
-a always,exit -F arch=b64 -S execve -F euid=0 \
   -F auid>=1000 -F auid!=4294967295 -k root_commands
# Solo registra: humanos que ejecutan cosas como root
# Ignora: servicios del sistema haciendo su trabajo normal
```

### Error 2: olvidar auid!=4294967295

```bash
# MAL — captura eventos de servicios del sistema
-a always,exit -F arch=b64 -S execve -F auid>=1000 -k exec
# Un servicio que arrancó antes del login tiene auid=4294967295
# (unsigned -1), que es >= 1000, ¡así que coincide!

# BIEN — excluir procesos sin login
-a always,exit -F arch=b64 -S execve -F auid>=1000 \
   -F auid!=4294967295 -k exec
# 4294967295 = (unsigned)-1 = "unset" = nunca hubo login
```

### Error 3: no hacer las reglas inmutables

```bash
# Sin -e 2, un atacante con root puede:
sudo auditctl -D           # Eliminar todas las reglas
sudo auditctl -e 0         # Deshabilitar auditoría
# → Sus acciones ya no se registran

# Con -e 2 al final de las reglas:
-e 2
# No se pueden cambiar reglas sin reiniciar el sistema
# Un atacante necesitaría reiniciar → más evidente
```

### Error 4: no monitorear el propio auditd

```bash
# Si no auditas la configuración de auditd, un atacante puede
# modificarla sin ser detectado

# Agregar siempre:
-w /etc/audit/ -p wa -k audit_config
-w /etc/audit/auditd.conf -p wa -k audit_config
-w /etc/audit/rules.d/ -p wa -k audit_config
```

### Error 5: confundir augenrules con auditctl -R

```bash
# augenrules --load
#   → Lee /etc/audit/rules.d/*.rules en orden
#   → Genera /etc/audit/audit.rules
#   → Carga las reglas en el kernel
#   → Método recomendado

# auditctl -R /etc/audit/audit.rules
#   → Lee y carga un archivo directamente
#   → No procesa rules.d/
#   → Puede sobrescribir reglas de augenrules

# No mezcles ambos métodos
# Elige uno y úsalo consistentemente
```

---

## 11. Cheatsheet

```
╔══════════════════════════════════════════════════════════════════╗
║                     auditd — Cheatsheet                        ║
╠══════════════════════════════════════════════════════════════════╣
║                                                                  ║
║  SERVICIO                                                        ║
║  systemctl status auditd        Estado del demonio               ║
║  sudo auditctl -s               Estado del subsistema kernel     ║
║  sudo auditctl -l               Listar reglas activas            ║
║                                                                  ║
║  REGLAS DE ARCHIVO (file watch)                                  ║
║  -w /ruta -p rwxa -k etiqueta                                    ║
║    r=read  w=write  x=execute  a=attribute                       ║
║                                                                  ║
║  REGLAS DE SYSCALL                                               ║
║  -a always,exit -F arch=b64 -S syscall -F filtro -k etiqueta    ║
║    -F auid>=1000              Solo usuarios reales               ║
║    -F auid!=4294967295        Excluir servicios sin login        ║
║    -F euid=0                  Solo cuando actúa como root        ║
║    -F success=0               Solo fallos                        ║
║                                                                  ║
║  AUDITCTL (temporales)                                           ║
║  sudo auditctl -w /ruta -p wa -k key   Agregar file watch       ║
║  sudo auditctl -W /ruta -p wa -k key   Eliminar file watch      ║
║  sudo auditctl -a always,exit ...      Agregar syscall rule     ║
║  sudo auditctl -d always,exit ...      Eliminar syscall rule    ║
║  sudo auditctl -D                      Eliminar TODAS           ║
║                                                                  ║
║  PERSISTENTES                                                    ║
║  /etc/audit/rules.d/*.rules   Archivos de reglas                ║
║  sudo augenrules --load       Compilar y cargar                  ║
║  -e 2                         Inmutable (siempre al final)       ║
║                                                                  ║
║  BUSCAR (ausearch)                                               ║
║  sudo ausearch -k etiqueta          Por key                      ║
║  sudo ausearch -m USER_LOGIN        Por tipo de mensaje          ║
║  sudo ausearch -ua 1000             Por audit UID                ║
║  sudo ausearch -c httpd             Por nombre de comando        ║
║  sudo ausearch -ts today            Desde hoy                    ║
║  sudo ausearch -ts recent           Últimos 10 minutos          ║
║  sudo ausearch -sv no               Solo fallos                  ║
║  sudo ausearch -i                   Formato interpretado         ║
║                                                                  ║
║  REPORTES (aureport)                                             ║
║  sudo aureport --summary           Resumen general               ║
║  sudo aureport --login             Logins                        ║
║  sudo aureport --auth              Autenticaciones               ║
║  sudo aureport -x                  Ejecuciones                   ║
║  sudo aureport -f                  Archivos                      ║
║  sudo aureport -k                  Por key                       ║
║  sudo aureport --failed            Solo fallos                   ║
║                                                                  ║
║  ARCHIVOS ESENCIALES A AUDITAR                                   ║
║  /etc/passwd, /etc/shadow, /etc/group        Identidad           ║
║  /etc/sudoers, /etc/sudoers.d/               Privilegios         ║
║  /etc/ssh/sshd_config                        SSH                 ║
║  /etc/audit/, /etc/audit/rules.d/            Meta-auditoría      ║
║  /var/log/lastlog, wtmp, btmp                Sesiones            ║
║                                                                  ║
╚══════════════════════════════════════════════════════════════════╝
```

---

## 12. Ejercicios

### Ejercicio 1: Crear una regla y verificar

```bash
# Paso 1: Agregar regla para vigilar /etc/hosts
sudo auditctl -w /etc/hosts -p wa -k hosts_modified

# Paso 2: Generar un evento
echo "# test" | sudo tee -a /etc/hosts

# Paso 3: Buscar el evento
sudo ausearch -k hosts_modified -ts recent -i
```

> **Pregunta de predicción**: en la salida de ausearch, ¿qué valor tendrá
> el campo `comm`? ¿Y el campo `auid`? ¿Serán iguales `uid` y `auid`?
>
> **Respuesta**: `comm="tee"` (el comando que escribió el archivo).
> `auid` será tu usuario de login (con el que hiciste SSH). `uid` será
> `root` (porque usaste sudo). Son **diferentes**: `auid` es quién eres
> realmente, `uid` es quién eres en ese momento (root tras sudo). Esta
> distinción es exactamente lo que hace útil a `auid` para auditoría.

### Ejercicio 2: Investigar quién ejecutó comandos como root

```bash
# Paso 1: Agregar regla
sudo auditctl -a always,exit -F arch=b64 -S execve -F euid=0 \
   -F auid>=1000 -F auid!=4294967295 -k root_commands

# Paso 2: Ejecutar varios comandos con sudo
sudo ls /root
sudo cat /etc/shadow
sudo systemctl status sshd
```

> **Pregunta de predicción**: ¿cuántos eventos generará el paso 2? ¿Solo
> 3 (uno por cada sudo), o más?
>
> **Respuesta**: **más de 3**. Cada `sudo` genera al menos un evento para
> el propio `sudo` y otro para el comando que ejecuta. Además, `systemctl
> status sshd` puede invocar subprocesos (como `pager`). `ls` y `cat` son
> sencillos, pero `sudo` en sí también es un execve como euid=0. Usa
> `sudo ausearch -k root_commands -ts recent -i | grep comm=` para ver
> todos los comandos registrados.

```bash
# Paso 3: Ver reporte
sudo ausearch -k root_commands -ts recent --raw | aureport -x --summary
```

### Ejercicio 3: Configurar auditoría persistente

```bash
# Paso 1: Crear archivo de reglas
sudo nano /etc/audit/rules.d/50-custom.rules
```

> **Pregunta de predicción**: si agregas `-e 2` en el archivo
> `50-custom.rules` en vez de en `99-finalize.rules`, ¿qué problema
> causará?
>
> **Respuesta**: las reglas de los archivos `60-*`, `70-*`, etc. NO se
> cargarán. El flag `-e 2` hace las reglas **inmutables** desde ese
> punto — cualquier regla posterior será rechazada por el kernel.
> Por eso `-e 2` debe ir siempre en el **último** archivo
> (`99-finalize.rules`). Los números en los nombres de archivo
> determinan el orden de carga.

```bash
# Contenido recomendado para 50-custom.rules:
# -w /etc/passwd -p wa -k identity
# -w /etc/shadow -p wa -k identity
# -w /etc/sudoers -p wa -k sudoers
# -w /etc/sudoers.d/ -p wa -k sudoers
# -w /etc/ssh/sshd_config -p wa -k sshd_config

# Paso 2: Cargar
sudo augenrules --load

# Paso 3: Verificar
sudo auditctl -l
```

---

> **Siguiente tema**: T03 — Hardening básico (desactivar servicios
> innecesarios, kernel parameters de seguridad).
