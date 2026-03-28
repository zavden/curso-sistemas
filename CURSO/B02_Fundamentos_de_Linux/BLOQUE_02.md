# Bloque 2: Fundamentos de Linux

**Objetivo**: Dominar la base del sistema operativo Linux a nivel profesional.
Cada tema se practica en ambas familias de distribuciones.

## Capítulo 1: Jerarquía del Sistema de Archivos (FHS) [A]

### Sección 1: Estructura de Directorios
- **T01 - Directorios raíz**: /, /bin, /sbin, /usr, /etc, /var, /tmp, /home, /root, /opt
- **T02 - /usr merge**: qué distribuciones lo hacen, por qué, implicaciones
- **T03 - /var en profundidad**: log, spool, cache, lib, tmp — qué va en cada uno
- **T04 - /etc en profundidad**: configuraciones del sistema, convención de archivos

### Sección 2: Sistemas de Archivos Virtuales
- **T01 - /proc**: información de procesos, /proc/self, /proc/sys
- **T02 - /sys**: modelo de dispositivos del kernel, sysfs
- **T03 - /dev**: dispositivos de bloque y carácter, udev, /dev/null, /dev/zero, /dev/urandom
- **T04 - Diferencias entre distribuciones**: rutas que varían entre Debian y RHEL

## Capítulo 2: Usuarios, Grupos y Permisos [A]

### Sección 1: Gestión de Usuarios
- **T01 - Archivos fundamentales**: /etc/passwd, /etc/shadow, /etc/group, /etc/gshadow
- **T02 - useradd/usermod/userdel**: flags importantes, diferencias con adduser (Debian)
- **T03 - Contraseñas**: passwd, chage, políticas de expiración, algoritmos de hash
- **T04 - Usuarios de sistema vs regulares**: UID ranges, nologin, convenciones

### Sección 2: Gestión de Grupos
- **T01 - Grupos primarios y suplementarios**: newgrp, id, groups
- **T02 - groupadd/groupmod/groupdel**: gestión completa
- **T03 - Grupo primario efectivo**: implicaciones en creación de archivos

### Sección 3: Permisos
- **T01 - rwx y notación octal**: lectura, escritura, ejecución en archivos vs directorios
- **T02 - chmod y chown**: sintaxis simbólica vs octal, -R, peculiaridades
- **T03 - SUID, SGID, Sticky bit**: funcionamiento, riesgos de seguridad, casos de uso
- **T04 - umask**: cálculo, configuración por usuario/sistema, herencia
- **T05 - ACLs**: getfacl, setfacl, máscara efectiva, herencia en directorios, ACLs default
- **T06 - Primer contacto con SELinux/AppArmor**: getenforce, sestatus (RHEL), aa-status (Debian), ls -Z, por qué archivos tienen contextos de seguridad — referencia a B11 para estudio completo

## Capítulo 3: Gestión de Procesos [A]

### Sección 1: Inspección de Procesos
- **T01 - ps**: formatos de salida, selectores, árbol de procesos (ps auxf)
- **T02 - top/htop**: columnas clave, ordenamiento, tree view, filtros
- **T03 - /proc/[pid]**: status, maps, fd, cmdline, environ — leer estado real del proceso

### Sección 2: Control de Procesos
- **T01 - Señales**: SIGTERM, SIGKILL, SIGHUP, SIGSTOP, SIGCONT — diferencias y cuándo usar cada una
- **T02 - kill, killall, pkill**: envío de señales, patterns
- **T03 - nice y renice**: prioridades, rango -20 a 19, quién puede hacer qué
- **T04 - Jobs**: bg, fg, Ctrl+Z, nohup, disown — mantener procesos vivos al cerrar la terminal

### Sección 3: Procesos Especiales
- **T01 - Procesos zombie y huérfanos**: qué son, cómo se crean, cómo prevenirlos
- **T02 - Init/PID 1**: responsabilidades, reaping de huérfanos
- **T03 - Daemons**: características, doble fork (clásico), systemd (moderno)

## Capítulo 4: Gestión de Paquetes [A]

### Sección 1: APT (Debian/Ubuntu)
- **T01 - apt vs apt-get**: diferencias prácticas, cuándo usar cada uno
- **T02 - Repositorios**: sources.list, GPG keys, PPAs, prioridades (pinning)
- **T03 - Operaciones**: install, remove, purge, autoremove, update, upgrade, dist-upgrade
- **T04 - dpkg**: instalación directa de .deb, consultar paquetes, extraer archivos

### Sección 2: DNF/YUM (RHEL/Alma/Rocky/Fedora)
- **T01 - dnf vs yum**: evolución, compatibilidad, diferencias internas
- **T02 - Repositorios**: .repo files, GPG keys, EPEL, prioridades, habilitación condicional
- **T03 - Operaciones**: install, remove, update, group install, module streams
- **T04 - rpm**: instalación directa, consultas (-q, -qa, -ql, -qf), verificación (-V)

### Sección 3: Compilación desde Fuente
- **T01 - Patrón clásico**: ./configure, make, make install
- **T02 - checkinstall**: crear paquete .deb/.rpm desde make install
- **T03 - Dependencias de compilación**: build-essential (Debian), Development Tools (RHEL)

## Capítulo 5: Shell Scripting Avanzado [A]

### Sección 1: Fundamentos de Scripting
- **T01 - Shebang y ejecución**: #!/bin/bash vs #!/usr/bin/env bash, permisos, PATH
- **T02 - Variables y expansiones**: ${var:-default}, ${var:+alt}, ${var%pattern}, ${var#pattern}, ${#var}
- **T03 - Exit codes**: $?, set -e, set -o pipefail, convenciones (0 = éxito, 1-125 = error, 126-255 = especiales)
- **T04 - Quoting**: comillas simples vs dobles vs backticks vs $(), word splitting, globbing accidental

### Sección 2: Control de Flujo y Funciones
- **T01 - Condicionales**: test / [ ] / [[ ]], diferencias, -f, -d, -z, -n, comparaciones numéricas vs string
- **T02 - Loops avanzados**: for con rangos y arrays, while read line, process substitution <()
- **T03 - Funciones**: local variables, return vs exit, paso de argumentos, funciones recursivas
- **T04 - Traps**: trap 'cmd' SIGNAL, cleanup al salir, manejo de SIGINT/SIGTERM en scripts

### Sección 3: Herramientas de Texto
- **T01 - sed**: sustitución, direccionamiento por línea/regex, in-place (-i), hold space (avanzado)
- **T02 - awk**: campos ($1, $2...), patrones, BEGIN/END, variables built-in (NR, NF, FS)
- **T03 - cut, sort, uniq, tr, tee, xargs**: pipeline idiomático, combinaciones comunes
- **T04 - grep avanzado**: -E (ERE), -P (PCRE), -o, -c, -l, -r, expresiones regulares

### Sección 4: Scripting Profesional
- **T01 - Getopts**: parsing de flags y argumentos, shift, validación de input
- **T02 - Here documents y here strings**: <<EOF, <<'EOF' (sin expansión), <<<
- **T03 - Arrays**: indexed y associativos (declare -A), iteración, ${arr[@]} vs ${arr[*]}
- **T04 - Debugging**: set -x, PS4, bash -n (syntax check), shellcheck (linter estático)

## Capítulo 6: Systemd [A]

### Sección 1: Conceptos Fundamentales
- **T01 - Unidades y tipos**: service, socket, timer, mount, target, slice, scope, path
- **T02 - Archivos de unidad**: ubicaciones (/usr/lib/systemd, /etc/systemd, /run/systemd), precedencia
- **T03 - Estados de unidad**: loaded, active, inactive, failed, masked

### Sección 2: Gestión de Servicios
- **T01 - systemctl**: start, stop, restart, reload, enable, disable, mask, status
- **T02 - Crear una unidad de servicio custom**: [Unit], [Service], [Install] — campos clave
- **T03 - Dependencias**: Requires, Wants, After, Before, Conflicts
- **T04 - Restart policies**: on-failure, always, on-abort, intervalos, límites

### Sección 3: Targets
- **T01 - Targets vs runlevels**: mapeo, multi-user.target, graphical.target, rescue.target
- **T02 - Cambiar target**: set-default, isolate, emergency.target
- **T03 - Target de arranque**: cómo systemd decide qué target arrancar

### Sección 4: Journald
- **T01 - journalctl**: filtros por unidad, prioridad, tiempo, boot, PID
- **T02 - Almacenamiento**: volatile vs persistent, tamaño máximo, rotación
- **T03 - Integración con rsyslog**: forwarding, coexistencia

## Capítulo 7: Tareas Programadas [A]

### Sección 1: Cron
- **T01 - Sintaxis crontab**: los 5 campos, caracteres especiales (*, /, -, ,)
- **T02 - crontab vs /etc/cron.d**: scope usuario vs sistema, /etc/cron.{hourly,daily,weekly,monthly}
- **T03 - Variables de entorno en cron**: PATH, MAILTO, SHELL — errores comunes
- **T04 - anacron**: ejecución garantizada, /etc/anacrontab, para equipos que no están siempre encendidos

### Sección 2: At y Batch
- **T01 - at**: ejecución única programada, sintaxis de tiempo, atq, atrm
- **T02 - batch**: ejecución cuando la carga es baja, load average threshold

### Sección 3: Systemd Timers
- **T01 - Timers vs cron**: ventajas, OnCalendar, OnBootSec, AccuracySec
- **T02 - Crear un timer**: relación timer↔service, persistent timers
- **T03 - Monotonic vs realtime timers**: cuándo usar cada uno

## Capítulo 8: Logging y Auditoría del Sistema [A]

### Sección 1: rsyslog
- **T01 - Configuración**: /etc/rsyslog.conf, facilidad.prioridad, acciones
- **T02 - Filtros y templates**: property-based, RainerScript
- **T03 - Envío remoto**: TCP vs UDP, configuración cliente-servidor

### Sección 2: journalctl (profundización)
- **T01 - Consultas avanzadas**: combinación de filtros, formatos de salida (json, verbose)
- **T02 - Campos personalizados**: MESSAGE_ID, SYSLOG_IDENTIFIER
- **T03 - Forward a syslog**: configuración de coexistencia

### Sección 3: logrotate
- **T01 - Configuración**: /etc/logrotate.conf, /etc/logrotate.d/
- **T02 - Directivas**: rotate, daily/weekly, compress, delaycompress, copytruncate, postrotate

## Capítulo 9: Monitoreo y Capacidad [A]

### Sección 1: Herramientas de Monitoreo
- **T01 - vmstat**: memoria virtual, swap, I/O, CPU — interpretación de columnas
- **T02 - iostat**: rendimiento de disco, await, %util, lectura vs escritura
- **T03 - sar**: recopilación histórica, configuración de sysstat, informes
- **T04 - free, df, du**: memoria, disco, uso — flags útiles (-h, -i, --max-depth)

### Sección 2: Tiempo y Localización
- **T01 - NTP/Chrony**: configuración, timedatectl, sincronización, drift
- **T02 - Zonas horarias**: timedatectl, /etc/localtime, TZ, diferencias entre distros
- **T03 - Locale**: LANG, LC_*, /etc/locale.conf, locale-gen, implicaciones en scripts
