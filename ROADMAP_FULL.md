# Roadmap Completo — Linux · C · Rust

> Leyenda: `[A]` autónomo · `[M]` mixto · `[P]` proyecto
> Marcar con `[x]` conforme se completa cada tópico.

---

## B01 — Docker y Entorno de Laboratorio

### C01 — Arquitectura de Docker `[A]`

**S01 — Modelo de Imágenes**
- [ ] T01 — Imágenes y capas (union filesystem, copy-on-write, OverlayFS)
- [ ] T02 — Registros (Docker Hub, pull/push, tags vs digests, imágenes oficiales)
- [ ] T03 — Inspección (docker image inspect, docker history, tamaño real vs virtual)

**S02 — Contenedores**
- [ ] T01 — Ciclo de vida (created → running → paused → stopped → removed)
- [ ] T02 — Ejecución (docker run flags: -it, -d, --rm, --name, -e, -p, -v)
- [ ] T03 — Gestión (docker ps, exec, logs, stats, attach vs exec)
- [ ] T04 — Diferencias entre stop, kill y rm (señales, timeouts, datos)

**S03 — Almacenamiento**
- [ ] T01 — Volúmenes (named volumes, bind mounts, tmpfs mounts)
- [ ] T02 — Persistencia (qué se pierde al destruir, estrategias)
- [ ] T03 — Permisos en bind mounts (UID/GID mapping, problemas host↔contenedor)

**S04 — Redes**
- [ ] T01 — Drivers de red (bridge, host, none, overlay)
- [ ] T02 — Comunicación entre contenedores (DNS interno, redes custom)
- [ ] T03 — Exposición de puertos (-p vs -P, binding a interfaces)

**S05 — Seguridad Básica**
- [ ] T01 — Root en contenedor (USER instruction, por qué es peligroso)
- [ ] T02 — Rootless containers (modo rootless de Docker, Podman rootless)
- [ ] T03 — Capabilities y seccomp (--cap-drop, --cap-add, perfiles seccomp)
- [ ] T04 — Imágenes de confianza (docker scout, DCT, firmado)

### C02 — Dockerfile `[A]`

**S01 — Sintaxis y Directivas**
- [ ] T01 — Instrucciones básicas (FROM, RUN, COPY, ADD, WORKDIR, CMD, ENTRYPOINT)
- [ ] T02 — CMD vs ENTRYPOINT (forma exec vs shell, combinaciones)
- [ ] T03 — ARG vs ENV (scope build vs runtime, precedencia)
- [ ] T04 — HEALTHCHECK (sintaxis, intervalos, estados de salud)

**S02 — Optimización**
- [ ] T01 — Orden de instrucciones y caché (invalidación, impacto en build time)
- [ ] T02 — Multi-stage builds (reducir tamaño, separar build de runtime)
- [ ] T03 — .dockerignore (sintaxis, impacto en contexto de build)

### C03 — Docker Compose `[A]`

**S01 — Fundamentos**
- [ ] T01 — Sintaxis YAML (servicios, redes, volúmenes, versiones)
- [ ] T02 — Ciclo de vida (up, down, start, stop, restart, ps, logs)
- [ ] T03 — Variables de entorno (.env files, interpolación, precedencia)

**S02 — Orquestación**
- [ ] T01 — Dependencias entre servicios (depends_on, condiciones de salud)
- [ ] T02 — Redes multi-servicio (comunicación por nombre de servicio)
- [ ] T03 — Volúmenes compartidos (datos persistentes entre servicios)

### C04 — Podman `[A]`

**S01 — Podman como Alternativa a Docker**
- [ ] T01 — Qué es Podman (daemonless, rootless by default, compatibilidad OCI)
- [ ] T02 — Equivalencia de comandos (drop-in replacement, alias)
- [ ] T03 — Diferencias clave (no daemon, fork/exec, integración con systemd, pods)
- [ ] T04 — Pods (agrupación de contenedores, relación con Kubernetes)

**S02 — Podman Compose y Migración**
- [ ] T01 — podman-compose (instalación, diferencias, limitaciones)
- [ ] T02 — Migrar de Docker a Podman (Dockerfile compatible, SELinux)
- [ ] T03 — Cuándo usar Docker vs Podman (RHCSA requiere Podman, industria usa Docker)

### C05 — Laboratorio del Curso `[M]`

**S01 — Construcción del Entorno**
- [ ] T01 — Dockerfile Debian dev (gcc, make, cmake, gdb, valgrind, rustup)
- [ ] T02 — Dockerfile Alma dev (dnf, EPEL, gcc-toolset, equivalentes)
- [ ] T03 — docker-compose.yml del curso (servicios, volúmenes, red del lab)
- [ ] T04 — Verificación (script de validación del entorno completo)

**S02 — Mantenimiento y Limpieza**
- [ ] T01 — Monitoreo de espacio (docker system df, du en /var/lib/docker)
- [ ] T02 — Limpieza selectiva (image prune, container prune, volume prune)
- [ ] T03 — Limpieza agresiva (docker system prune -a, qué se pierde)
- [ ] T04 — Estrategia del curso (rutina periódica, reutilización con volúmenes)

---

## B02 — Fundamentos de Linux

### C01 — Jerarquía del Sistema de Archivos `[A]`

**S01 — Estructura de Directorios**
- [ ] T01 — Directorios raíz (/, /bin, /sbin, /usr, /etc, /var, /tmp, /home, /opt)
- [ ] T02 — /usr merge (qué distros lo hacen, por qué, implicaciones)
- [ ] T03 — /var en profundidad (log, spool, cache, lib, tmp)
- [ ] T04 — /etc en profundidad (convención de archivos de configuración)

**S02 — Sistemas de Archivos Virtuales**
- [ ] T01 — /proc (información de procesos, /proc/self, /proc/sys)
- [ ] T02 — /sys (modelo de dispositivos del kernel, sysfs)
- [ ] T03 — /dev (dispositivos, udev, /dev/null, /dev/zero, /dev/urandom)
- [ ] T04 — Diferencias entre distribuciones (rutas que varían entre Debian y RHEL)

### C02 — Usuarios, Grupos y Permisos `[A]`

**S01 — Gestión de Usuarios**
- [ ] T01 — Archivos fundamentales (/etc/passwd, /etc/shadow, /etc/group)
- [ ] T02 — useradd/usermod/userdel (flags, diferencias con adduser en Debian)
- [ ] T03 — Contraseñas (passwd, chage, expiración, algoritmos de hash)
- [ ] T04 — Usuarios de sistema vs regulares (UID ranges, nologin)

**S02 — Gestión de Grupos**
- [ ] T01 — Grupos primarios y suplementarios (newgrp, id, groups)
- [ ] T02 — groupadd/groupmod/groupdel
- [ ] T03 — Grupo primario efectivo (implicaciones en creación de archivos)

**S03 — Permisos**
- [ ] T01 — rwx y notación octal (en archivos vs directorios)
- [ ] T02 — chmod y chown (sintaxis simbólica vs octal, -R)
- [ ] T03 — SUID, SGID, Sticky bit (funcionamiento, riesgos, casos de uso)
- [ ] T04 — umask (cálculo, configuración, herencia)
- [ ] T05 — ACLs (getfacl, setfacl, máscara efectiva, herencia, ACLs default)
- [ ] T06 — Primer contacto SELinux/AppArmor (getenforce, aa-status, ls -Z)

### C03 — Gestión de Procesos `[A]`

**S01 — Inspección de Procesos**
- [ ] T01 — ps (formatos, selectores, árbol de procesos con ps auxf)
- [ ] T02 — top/htop (columnas clave, ordenamiento, tree view, filtros)
- [ ] T03 — /proc/[pid] (status, maps, fd, cmdline, environ)

**S02 — Control de Procesos**
- [ ] T01 — Señales (SIGTERM, SIGKILL, SIGHUP, SIGSTOP, SIGCONT)
- [ ] T02 — kill, killall, pkill (envío de señales, patterns)
- [ ] T03 — nice y renice (prioridades, rango -20 a 19)
- [ ] T04 — Jobs (bg, fg, Ctrl+Z, nohup, disown)

**S03 — Procesos Especiales**
- [ ] T01 — Procesos zombie y huérfanos (creación, prevención)
- [ ] T02 — Init/PID 1 (responsabilidades, reaping de huérfanos)
- [ ] T03 — Daemons (características, doble fork clásico, systemd moderno)

### C04 — Gestión de Paquetes `[A]`

**S01 — APT (Debian/Ubuntu)**
- [ ] T01 — apt vs apt-get (diferencias prácticas)
- [ ] T02 — Repositorios (sources.list, GPG keys, PPAs, pinning)
- [ ] T03 — Operaciones (install, remove, purge, autoremove, update, upgrade)
- [ ] T04 — dpkg (instalación de .deb, consultar paquetes, extraer archivos)

**S02 — DNF/YUM (RHEL/Alma/Rocky/Fedora)**
- [ ] T01 — dnf vs yum (evolución, compatibilidad, diferencias internas)
- [ ] T02 — Repositorios (.repo files, GPG keys, EPEL, module streams)
- [ ] T03 — Operaciones (install, remove, update, group install, module streams)
- [ ] T04 — rpm (instalación directa, consultas -q/-qa/-ql/-qf, verificación -V)

**S03 — Compilación desde Fuente**
- [ ] T01 — Patrón clásico (./configure, make, make install)
- [ ] T02 — checkinstall (crear .deb/.rpm desde make install)
- [ ] T03 — Dependencias de compilación (build-essential, Development Tools)

### C05 — Shell Scripting Avanzado `[A]`

**S01 — Fundamentos de Scripting**
- [ ] T01 — Shebang y ejecución (#!/bin/bash, permisos, PATH)
- [ ] T02 — Variables y expansiones (${var:-default}, ${var%pattern}, ${#var})
- [ ] T03 — Exit codes ($?, set -e, set -o pipefail, convenciones)
- [ ] T04 — Quoting (simples vs dobles vs $(), word splitting, globbing)

**S02 — Control de Flujo y Funciones**
- [ ] T01 — Condicionales (test / [ ] / [[ ]], comparaciones numéricas vs string)
- [ ] T02 — Loops avanzados (for con arrays, while read line, process substitution)
- [ ] T03 — Funciones (local variables, return vs exit, recursión)
- [ ] T04 — Traps (cleanup al salir, SIGINT/SIGTERM en scripts)

**S03 — Herramientas de Texto**
- [ ] T01 — sed (sustitución, direccionamiento, in-place, hold space)
- [ ] T02 — awk (campos, patrones, BEGIN/END, NR/NF/FS)
- [ ] T03 — cut, sort, uniq, tr, tee, xargs (pipeline idiomático)
- [ ] T04 — grep avanzado (-E, -P, -o, -c, -l, -r, expresiones regulares)

**S04 — Scripting Profesional**
- [ ] T01 — Getopts (parsing de flags, shift, validación)
- [ ] T02 — Here documents y here strings (<<EOF, <<'EOF', <<<)
- [ ] T03 — Arrays (indexed y asociativos, iteración)
- [ ] T04 — Debugging (set -x, PS4, bash -n, shellcheck)

### C06 — Systemd `[A]`

**S01 — Conceptos Fundamentales**
- [ ] T01 — Unidades y tipos (service, socket, timer, mount, target, slice)
- [ ] T02 — Archivos de unidad (ubicaciones, precedencia)
- [ ] T03 — Estados de unidad (loaded, active, inactive, failed, masked)

**S02 — Gestión de Servicios**
- [ ] T01 — systemctl (start, stop, restart, reload, enable, disable, mask)
- [ ] T02 — Crear unidad custom ([Unit], [Service], [Install])
- [ ] T03 — Dependencias (Requires, Wants, After, Before, Conflicts)
- [ ] T04 — Restart policies (on-failure, always, intervalos, límites)

**S03 — Targets**
- [ ] T01 — Targets vs runlevels (multi-user.target, graphical.target, rescue)
- [ ] T02 — Cambiar target (set-default, isolate, emergency.target)
- [ ] T03 — Target de arranque (cómo systemd decide qué arrancar)

**S04 — Journald**
- [ ] T01 — journalctl (filtros por unidad, prioridad, tiempo, boot, PID)
- [ ] T02 — Almacenamiento (volatile vs persistent, tamaño, rotación)
- [ ] T03 — Integración con rsyslog (forwarding, coexistencia)

### C07 — Tareas Programadas `[A]`

**S01 — Cron**
- [ ] T01 — Sintaxis crontab (5 campos, *, /, -, ,)
- [ ] T02 — crontab vs /etc/cron.d (scope usuario vs sistema)
- [ ] T03 — Variables de entorno en cron (PATH, MAILTO, SHELL)
- [ ] T04 — anacron (ejecución garantizada, /etc/anacrontab)

**S02 — At y Batch**
- [ ] T01 — at (ejecución única programada, atq, atrm)
- [ ] T02 — batch (ejecución cuando la carga es baja)

**S03 — Systemd Timers**
- [ ] T01 — Timers vs cron (OnCalendar, OnBootSec, AccuracySec)
- [ ] T02 — Crear un timer (relación timer↔service, persistent timers)
- [ ] T03 — Monotonic vs realtime timers

### C08 — Logging y Auditoría del Sistema `[A]`

**S01 — rsyslog**
- [ ] T01 — Configuración (/etc/rsyslog.conf, facilidad.prioridad, acciones)
- [ ] T02 — Filtros y templates (property-based, RainerScript)
- [ ] T03 — Envío remoto (TCP vs UDP, cliente-servidor)

**S02 — journalctl (profundización)**
- [ ] T01 — Consultas avanzadas (combinación de filtros, formatos json/verbose)
- [ ] T02 — Campos personalizados (MESSAGE_ID, SYSLOG_IDENTIFIER)
- [ ] T03 — Forward a syslog (configuración de coexistencia)

**S03 — logrotate**
- [ ] T01 — Configuración (/etc/logrotate.conf, /etc/logrotate.d/)
- [ ] T02 — Directivas (rotate, daily, compress, delaycompress, postrotate)

### C09 — Monitoreo y Capacidad `[A]`

**S01 — Herramientas de Monitoreo**
- [ ] T01 — vmstat (memoria virtual, swap, I/O, CPU)
- [ ] T02 — iostat (rendimiento de disco, await, %util)
- [ ] T03 — sar (recopilación histórica, sysstat, informes)
- [ ] T04 — free, df, du (memoria, disco, uso — flags útiles)

**S02 — Tiempo y Localización**
- [ ] T01 — NTP/Chrony (configuración, timedatectl, sincronización)
- [ ] T02 — Zonas horarias (timedatectl, /etc/localtime, TZ)
- [ ] T03 — Locale (LANG, LC_*, locale-gen, implicaciones en scripts)

### C10 — Redes Básicas para Virtualización `[A]`

**S01 — Direcciones IP**
- [ ] T01 — Direcciones IPv4 (octetos, unicast, broadcast, loopback)
- [ ] T02 — Máscara de red y CIDR (notación, cálculo rápido)
- [ ] T03 — Direcciones privadas (10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16, NAT)

**S02 — NAT y DHCP**
- [ ] T01 — NAT (MASQUERADE, acceso a internet de VMs via host)
- [ ] T02 — DHCP (proceso DORA, dhclient, libvirt DHCP server)
- [ ] T03 — DNS básico (resolv.conf, dig, nslookup, /etc/hosts)

**S03 — Bridge Networking**
- [ ] T01 — Concepto de bridge (software switch, capa 2)
- [ ] T02 — Bridge en Linux (ip link, VMs en el mismo switch)

**S04 — Redes Virtuales en libvirt**
- [ ] T01 — Tipos (NAT default, isolated, bridge)
- [ ] T02 — Gestión (virsh net-list, virsh net-dhcp-leases)

### C11 — Discos y Particiones para Virtualización `[A]`

**S01 — Dispositivos de Bloque y Particiones**
- [ ] T01 — Dispositivos de bloque (nomenclatura /dev/sd*, /dev/vd*, /dev/nvme*)
- [ ] T02 — Tablas de particiones (MBR vs GPT, fdisk, gdisk, parted)

**S02 — Sistemas de Archivos y Montaje**
- [ ] T01 — Sistemas de archivos (ext4, XFS, Btrfs, journaling, mkfs, fsck)
- [ ] T02 — Montaje y fstab (mount, umount, opciones, UUID, findmnt)

**S03 — Imágenes de Disco Virtuales**
- [ ] T01 — Formatos (raw, qcow2, vmdk, vdi, vhd)
- [ ] T02 — Discos virtuales en VMs (virtio-blk, discos extra, snapshots)

---

## B03 — Fundamentos de C

### C01 — El Compilador y el Proceso de Compilación `[A]`

**S01 — GCC**
- [ ] T01 — Fases de compilación (-E, -S, -c, enlace)
- [ ] T02 — Flags esenciales (-Wall, -Wextra, -Werror, -std=c11, -g, -O2)
- [ ] T03 — Archivos objeto y enlace (.o, estático vs dinámico, -l -L -I)
- [ ] T04 — Comportamiento indefinido (UB: qué es, por qué es peligroso)

**S02 — Herramientas de Diagnóstico**
- [ ] T01 — GDB básico (breakpoints, step, next, print, backtrace, watch)
- [ ] T02 — Valgrind (memcheck, leaks, accesos inválidos, uso con GDB)
- [ ] T03 — AddressSanitizer/UBSan (-fsanitize=address, cuándo preferir)

**S03 — Estándar del Lenguaje**
- [ ] T01 — Evolución de C (C89 → C99 → C11 → C17 → C23)
- [ ] T02 — Estándar del curso C11 (static_assert, anonymous structs, _Generic)
- [ ] T03 — Comportamiento por compilador (GCC default, -std=gnu11 vs -std=c11)
- [ ] T04 — Features de C23 mencionadas (__VA_OPT__, constexpr, typeof)

### C02 — Tipos de Datos y Variables `[A]`

**S01 — Tipos Primitivos**
- [ ] T01 — Enteros (char, short, int, long, long long, stdint.h)
- [ ] T02 — Punto flotante (float, double, long double, IEEE 754)
- [ ] T03 — sizeof y alineación (padding en structs, alignof C11)
- [ ] T04 — Signed vs unsigned (overflow UB vs wrapping, conversiones)
- [ ] T05 — Endianness (big vs little-endian, htonl/htons, impacto en protocolos)

**S02 — Variables y Almacenamiento**
- [ ] T01 — Clases de almacenamiento (auto, static, extern, _Thread_local)
- [ ] T02 — Scope y lifetime (block scope, file scope, linkage)
- [ ] T03 — Calificadores (const, volatile, restrict)
- [ ] T04 — Inicialización (valores por defecto, designated initializers C99)

### C03 — Operadores y Control de Flujo `[A]`

**S01 — Operadores**
- [ ] T01 — Aritméticos y de asignación (precedencia, asociatividad, promoción)
- [ ] T02 — Bitwise (&, |, ^, ~, <<, >> — máscaras, flags)
- [ ] T03 — Lógicos y relacionales (short-circuit, diferencia & vs &&)
- [ ] T04 — Operador coma y ternario

**S02 — Control de Flujo**
- [ ] T01 — if/else, switch (fall-through, Duff's device)
- [ ] T02 — Loops (for, while, do-while, loops infinitos idiomáticos)
- [ ] T03 — goto (cuándo es legítimo: cleanup en C)

### C04 — Funciones `[A]`

**S01 — Declaración y Definición**
- [ ] T01 — Prototipos (forward declaration, f() vs f(void))
- [ ] T02 — Paso por valor (simulando paso por referencia con punteros)
- [ ] T03 — Variadic functions (stdarg.h, va_list, cómo funciona printf)

**S02 — Funciones Avanzadas**
- [ ] T01 — Punteros a función (sintaxis, typedef, callbacks, dispatch tables)
- [ ] T02 — Inline (keyword, comportamiento real, ODR)
- [ ] T03 — _Noreturn y _Static_assert (C11 features útiles)

### C05 — Arrays y Cadenas `[A]`

**S01 — Arrays**
- [ ] T01 — Declaración y acceso (estáticos, VLAs C99, inicialización)
- [ ] T02 — Arrays y punteros (la relación array→puntero, por qué array ≠ puntero)
- [ ] T03 — Arrays multidimensionales (row-major, array de punteros vs 2D real)

**S02 — Cadenas**
- [ ] T01 — El modelo de strings en C (null-terminated, por qué es peligroso)
- [ ] T02 — string.h (strlen, strcpy, strncpy, strcmp, strcat, memcpy, memmove)
- [ ] T03 — Buffer overflows (stack smashing, exploits clásicos, mitigaciones)

### C06 — Punteros `[A]`

**S01 — Fundamentos**
- [ ] T01 — Qué es un puntero (dirección de memoria, & y *, NULL)
- [ ] T02 — Aritmética de punteros (incremento, diferencia, relación con arrays)
- [ ] T03 — Punteros a punteros (doble indirección, parámetros de salida)
- [ ] T04 — void* (genericidad, casting, reglas de aliasing)

**S02 — Punteros Avanzados**
- [ ] T01 — const y punteros (const int*, int* const, const int* const)
- [ ] T02 — Punteros a arrays vs arrays de punteros (regla de decodificación)
- [ ] T03 — restrict (qué promete al compilador, cuándo usarlo)
- [ ] T04 — Punteros colgantes y wild pointers (causas, detección, prevención)

### C07 — Memoria Dinámica `[A]`

**S01 — Asignación y Liberación**
- [ ] T01 — malloc, calloc, realloc, free (diferencias, patrones seguros)
- [ ] T02 — Fragmentación de heap (qué es, allocators alternativos)
- [ ] T03 — Memory leaks (detección con Valgrind, patrones de ownership)

**S02 — Patrones de Gestión**
- [ ] T01 — Ownership manual (convenciones para quién libera qué)
- [ ] T02 — Arena allocators (concepto, implementación simple)
- [ ] T03 — RAII en C (cleanup con goto, __attribute__((cleanup)))

### C08 — Estructuras, Uniones y Enumeraciones `[A]`

**S01 — Structs**
- [ ] T01 — Declaración y uso (inicialización, designated initializers, . y ->)
- [ ] T02 — Padding y alineación (#pragma pack, offsetof)
- [ ] T03 — Structs anidados y self-referential (listas, árboles)
- [ ] T04 — Bit fields (sintaxis, portabilidad, uso en protocolos)

**S02 — Uniones y Enums**
- [ ] T01 — Unions (overlapping memory, tagged unions)
- [ ] T02 — Enums (valores, conversión implícita a int)
- [ ] T03 — typedef (convenciones, opaque types, forward declarations)

### C09 — Entrada/Salida de Archivos `[A]`

**S01 — stdio.h (Buffered I/O)**
- [ ] T01 — FILE*, fopen, fclose (modos de apertura, errores)
- [ ] T02 — fread, fwrite, fgets, fputs (I/O binario vs texto)
- [ ] T03 — fprintf, fscanf, sprintf, sscanf (format strings, vulnerabilidades)
- [ ] T04 — Buffering (full, line, unbuffered — setvbuf, fflush)

**S02 — POSIX I/O (Unbuffered)**
- [ ] T01 — open, close, read, write (file descriptors, flags O_*)
- [ ] T02 — lseek (posicionamiento, sparse files)
- [ ] T03 — Diferencia stdio vs POSIX I/O (cuándo usar cada uno, mixing)

### C10 — El Preprocesador `[A]`

**S01 — Directivas**
- [ ] T01 — #include y guardas (guardas de inclusión, #pragma once)
- [ ] T02 — #define (macros con/sin parámetros, operadores # y ##)
- [ ] T03 — Macros peligrosas (side effects, do { } while(0) idiom)
- [ ] T04 — Compilación condicional (#if, #ifdef, #elif)

**S02 — Macros Avanzadas**
- [ ] T01 — Variadic macros (__VA_ARGS__, __VA_OPT__ C23)
- [ ] T02 — _Generic (despacho por tipo en C11)
- [ ] T03 — Macros predefinidas (__FILE__, __LINE__, __func__, __DATE__)

### C11 — Bibliotecas `[A]`

**S01 — Bibliotecas Estáticas**
- [ ] T01 — Creación (ar, ranlib, convención lib*.a)
- [ ] T02 — Enlace estático (-l, -L, orden de argumentos)
- [ ] T03 — Ventajas y desventajas (tamaño, independencia, actualización)

**S02 — Bibliotecas Dinámicas**
- [ ] T01 — Creación (-fPIC, -shared, convención lib*.so.X.Y.Z)
- [ ] T02 — Enlace dinámico (ld-linux.so, LD_LIBRARY_PATH, ldconfig, rpath)
- [ ] T03 — Versionado de soname (soname, real name, linker name)
- [ ] T04 — dlopen/dlsym/dlclose (carga dinámica, plugins)

---

## B04 — Sistemas de Compilación

### C01 — Make y Makefiles `[A]`

**S01 — Fundamentos**
- [ ] T01 — Reglas (target, prerequisites, recipe, tabulación obligatoria)
- [ ] T02 — Variables (=, :=, ?=, += — lazy vs immediate)
- [ ] T03 — Reglas implícitas y patrones (%.o: %.c, $@, $<, $^, $*)
- [ ] T04 — Phony targets (.PHONY, clean, all, install)

**S02 — Make Avanzado**
- [ ] T01 — Funciones ($(wildcard), $(patsubst), $(foreach), $(shell))
- [ ] T02 — Dependencias automáticas (-MMD, -MP, archivos .d)
- [ ] T03 — Makefiles recursivos vs no-recursivos (pros, contras, patrón)
- [ ] T04 — Paralelismo (make -j, order-only prerequisites)

### C02 — CMake `[A]`

**S01 — Fundamentos**
- [ ] T01 — CMakeLists.txt básico (cmake_minimum_required, project, add_executable)
- [ ] T02 — Targets y propiedades (add_library, target_include_directories)
- [ ] T03 — Generadores (Makefiles, Ninja, selección)
- [ ] T04 — Build types (Debug, Release, RelWithDebInfo, MinSizeRel)

**S02 — CMake Intermedio**
- [ ] T01 — Variables y caché (set, option, CMakeCache.txt, -D flags)
- [ ] T02 — find_package y módulos (Config mode vs Module mode)
- [ ] T03 — Install y export (destinos, CMake package config)
- [ ] T04 — Subdirectorios y estructura (add_subdirectory, visibilidad de targets)

### C03 — pkg-config `[A]`

**S01 — Uso y Configuración**
- [ ] T01 — Qué es pkg-config (.pc files, --cflags, --libs, PKG_CONFIG_PATH)
- [ ] T02 — Crear un .pc file (para bibliotecas propias)
- [ ] T03 — Integración con Make y CMake (patrones idiomáticos)

---

## B05 — Fundamentos de Rust

### C01 — Toolchain y Ecosistema `[A]`

**S01 — Herramientas**
- [ ] T01 — rustup (toolchains stable/nightly/beta, targets, components)
- [ ] T02 — Cargo (new, build, run, test, check, doc, estructura)
- [ ] T03 — Cargo.toml ([package], [dependencies], features, [profile])
- [ ] T04 — Herramientas auxiliares (rustfmt, clippy, cargo-expand, rust-analyzer)

**S02 — El Ecosistema**
- [ ] T01 — crates.io (publicación, versionado semántico, yanking)
- [ ] T02 — Documentación (cargo doc, ///, //!, ejemplos compilables)

### C02 — Tipos de Datos y Variables `[A]`

**S01 — Tipos Primitivos**
- [ ] T01 — Enteros (i8-i128, u8-u128, isize/usize, overflow en debug/release)
- [ ] T02 — Flotantes (f32, f64, NaN, Infinity, por qué f64 no implementa Eq)
- [ ] T03 — Booleanos y char (bool, char 4 bytes Unicode, ≠ u8)
- [ ] T04 — Inferencia de tipos (turbofish ::<>, ambigüedad)

**S02 — Variables y Mutabilidad**
- [ ] T01 — let vs let mut (inmutabilidad por defecto, re-binding vs mutación)
- [ ] T02 — Shadowing (re-declarar con mismo nombre, cambio de tipo)
- [ ] T03 — Constantes (const vs static, evaluación en compilación, static mut)
- [ ] T04 — Tipo unit () (qué es, cuándo aparece)

**S03 — Tipos Compuestos**
- [ ] T01 — Tuplas (acceso por índice, destructuring, tupla vacía)
- [ ] T02 — Arrays ([T; N], inicialización, bounds checking)
- [ ] T03 — Slices (&[T], &str, fat pointers, relación con arrays)

### C03 — Ownership, Borrowing y Lifetimes `[A]`

**S01 — Ownership**
- [ ] T01 — Las 3 reglas (un dueño, uno a la vez, drop al salir de scope)
- [ ] T02 — Move semántica (String se mueve, i32 se copia, Copy vs Clone)
- [ ] T03 — Drop (trait Drop, orden de destrucción, std::mem::drop)
- [ ] T04 — Copy y Clone (cuándo derivar, tipos que no pueden ser Copy)

**S02 — Borrowing**
- [ ] T01 — Referencias compartidas (&T — múltiples lectores)
- [ ] T02 — Referencias mutables (&mut T — exclusividad, no aliasing)
- [ ] T03 — Reglas del borrow checker (por qué &mut excluye &)
- [ ] T04 — Reborrowing (por qué funciona pasar &mut sin mover)

**S03 — Lifetimes**
- [ ] T01 — Lifetime elision (las 3 reglas del compilador)
- [ ] T02 — Anotaciones explícitas ('a, múltiples lifetimes)
- [ ] T03 — Lifetime en structs (structs que contienen referencias)
- [ ] T04 — 'static (qué significa realmente, T: 'static)

### C04 — Control de Flujo y Pattern Matching `[A]`

**S01 — Control de Flujo**
- [ ] T01 — if/else como expresión (retornar valores, if let)
- [ ] T02 — Loops (loop, while, for, break con valor, labels)
- [ ] T03 — Iteradores (.iter(), .into_iter(), .iter_mut())

**S02 — Pattern Matching**
- [ ] T01 — match (exhaustividad, destructuring, guards)
- [ ] T02 — Patrones (literales, variables, wildcard, rangos, ref, bindings @)
- [ ] T03 — if let y while let (match de una sola rama)
- [ ] T04 — let-else (Rust 1.65+, early return con pattern)

### C05 — Structs, Enums y Traits `[A]`

**S01 — Structs**
- [ ] T01 — Struct con campos nombrados (instanciación, field init shorthand)
- [ ] T02 — Tuple structs y unit structs (newtype pattern, marker types)
- [ ] T03 — Métodos e impl blocks (&self, &mut self, self, Self)
- [ ] T04 — Visibilidad (pub en campos, módulo como boundary)

**S02 — Enums**
- [ ] T01 — Enums con datos (variantes con campos nombrados, tuplas, unit)
- [ ] T02 — Option<T> (Some, None, unwrap, map, and_then)
- [ ] T03 — Result<T,E> (Ok, Err, propagación con ?, From)

**S03 — Traits**
- [ ] T01 — Definición e implementación (trait, impl Trait for Type, defaults)
- [ ] T02 — Traits estándar clave (Display, Debug, Clone, Copy, Ord, Hash)
- [ ] T03 — Derive macro (#[derive(...)], cuándo implementar manualmente)
- [ ] T04 — Trait bounds (fn foo<T: Trait>, where clauses, múltiples +)
- [ ] T05 — Trait objects (dyn Trait, vtable, &dyn vs Box<dyn>, object safety)

### C06 — Manejo de Errores `[A]`

**S01 — Estrategias**
- [ ] T01 — panic! vs Result (cuándo usar, unwinding vs abort)
- [ ] T02 — Propagación con ? (conversión From, retorno temprano)
- [ ] T03 — Tipos de error custom (struct vs enum, std::error::Error)
- [ ] T04 — thiserror y anyhow (ecosistema, cuándo usar cada uno)

### C07 — Colecciones `[A]`

**S01 — Colecciones Estándar**
- [ ] T01 — Vec<T> (push, pop, capacidad vs longitud, layout en memoria)
- [ ] T02 — String y &str (UTF-8, por qué no indexar por posición, chars())
- [ ] T03 — HashMap<K,V> (inserción, entry API, por qué K: Hash+Eq)
- [ ] T04 — HashSet, BTreeMap, BTreeSet, VecDeque (cuándo preferir cada uno)

### C08 — Genéricos e Iteradores `[A]`

**S01 — Genéricos**
- [ ] T01 — Funciones genéricas (monomorphization, costo en binario)
- [ ] T02 — Structs y enums genéricos (Option<T>, Result<T,E>)
- [ ] T03 — impl para tipos genéricos (impl<T>, blanket impls)

**S02 — Closures**
- [ ] T01 — Sintaxis y captura (|args|, por referencia vs por valor)
- [ ] T02 — Fn, FnMut, FnOnce (los 3 traits, jerarquía)
- [ ] T03 — move closures (transferencia de ownership, threads y async)
- [ ] T04 — Closures como parámetros/retorno (impl Fn vs dyn Fn, boxing)

**S03 — Iteradores**
- [ ] T01 — El trait Iterator (Item, next(), lazy evaluation)
- [ ] T02 — Adaptadores (map, filter, enumerate, zip, take, skip, chain)
- [ ] T03 — Consumidores (collect, sum, count, any, all, find, fold)
- [ ] T04 — Iteradores custom (impl Iterator, IntoIterator)

### C09 — Smart Pointers `[A]`

**S01 — Punteros al Heap**
- [ ] T01 — Box<T> (heap allocation, tipos recursivos)
- [ ] T02 — Deref y DerefMut (coerción automática, deref coercion chain)
- [ ] T03 — Drop revisitado (ManuallyDrop, orden en structs)

**S02 — Conteo de Referencias**
- [ ] T01 — Rc<T> (reference counting single-thread, Rc::clone, strong_count)
- [ ] T02 — Arc<T> (atómico para multi-thread, overhead vs Rc)
- [ ] T03 — Weak<T> (referencias débiles, romper ciclos, upgrade/downgrade)

**S03 — Interior Mutability**
- [ ] T01 — Cell<T> (get/set sin &mut, solo Copy types, zero-cost)
- [ ] T02 — RefCell<T> (borrowing dinámico, panic en runtime)
- [ ] T03 — Rc<RefCell<T>> (patrón para datos compartidos mutables)
- [ ] T04 — OnceCell y LazyCell (inicialización perezosa, OnceLock)

**S04 — Cow y Otros**
- [ ] T01 — Cow<T> (clone-on-write, optimización de strings)
- [ ] T02 — Pin<T> (self-referential structs, relación con async/Future)
- [ ] T03 — Árbol de decisión (cuándo usar cada smart pointer)

### C10 — Módulos y Crates `[A]`

**S01 — Sistema de Módulos**
- [ ] T01 — mod y archivos (mod.rs vs nombre_modulo.rs, edición 2018+)
- [ ] T02 — Visibilidad (pub, pub(crate), pub(super), pub(in path))
- [ ] T03 — use y re-exports (pub use, as, glob imports)
- [ ] T04 — El preludio (qué incluye, crear uno propio)

**S02 — Workspace y Crates Múltiples**
- [ ] T01 — Workspaces (Cargo.toml raíz, members, dependencias compartidas)
- [ ] T02 — Librería vs binario (lib.rs vs main.rs, ambos en un crate)
- [ ] T03 — Features (opcionales, compilación condicional con cfg)

**S03 — Macros Declarativas**
- [ ] T01 — macro_rules! básico ($x:expr, $x:ident, repetición)
- [ ] T02 — Macros del ecosistema (vec![], println![], cargo-expand)
- [ ] T03 — Macro vs función genérica (tradeoffs, debugging)

### C11 — Testing `[A]`

**S01 — Tests en Rust**
- [ ] T01 — Unit tests (#[test], #[cfg(test)], assert!, assert_eq!)
- [ ] T02 — Tests de integración (carpeta tests/, scope, #[ignore])
- [ ] T03 — Tests que esperan panic (#[should_panic], expected message)
- [ ] T04 — Doc tests (ejemplos en documentación, no_run, ignore)

---

## B06 — Programación de Sistemas en C

### C01 — Llamadas al Sistema `[A]`

**S01 — Interfaz Syscall**
- [ ] T01 — Qué es una syscall (user space vs kernel, wrapper glibc)
- [ ] T02 — Manejo de errores (errno, perror, strerror)
- [ ] T03 — strace (trazar syscalls, filtros, timing, conteo)

**S02 — Operaciones de Archivo (POSIX)**
- [ ] T01 — open/close/read/write avanzado (O_CLOEXEC, O_NONBLOCK, EINTR)
- [ ] T02 — stat, fstat, lstat (struct stat, permisos, tipo, timestamps)
- [ ] T03 — Directorios (opendir, readdir, recorrido recursivo)
- [ ] T04 — File locking (flock, fcntl, advisory vs mandatory)

**S03 — mmap**
- [ ] T01 — Mapeo de archivos (PROT_READ/WRITE, MAP_SHARED/PRIVATE, munmap)
- [ ] T02 — Mapeo anónimo (MAP_ANONYMOUS, lo que malloc hace internamente)
- [ ] T03 — Memoria compartida con mmap (shm_open vs archivo, sincronización)
- [ ] T04 — Ventajas (zero-copy, lazy loading, page cache, archivos grandes)

**S04 — inotify**
- [ ] T01 — API de inotify (init, add_watch, eventos IN_CREATE/MODIFY/DELETE)
- [ ] T02 — Casos de uso (hot reload, sincronización, build watchers)
- [ ] T03 — Limitaciones (no recursivo, max_user_watches, fanotify)

### C02 — Procesos `[A]`

**S01 — Creación y Gestión**
- [ ] T01 — fork() (duplicación, PID, valor de retorno en padre e hijo)
- [ ] T02 — exec family (execvp, execle, variantes)
- [ ] T03 — wait y waitpid (estado de salida, WIFEXITED, zombies)
- [ ] T04 — Patrón fork+exec (el idioma estándar de Unix)

**S02 — Entorno del Proceso**
- [ ] T01 — Variables de entorno (getenv, setenv, environ)
- [ ] T02 — Directorio de trabajo (getcwd, chdir)
- [ ] T03 — Límites de recursos (getrlimit, setrlimit, ulimit)

### C03 — Señales `[A]`

**S01 — Manejo de Señales**
- [ ] T01 — signal() vs sigaction() (por qué preferir sigaction)
- [ ] T02 — Señales comunes (SIGINT, SIGTERM, SIGCHLD, SIGPIPE, SIGALRM)
- [ ] T03 — Signal handlers seguros (async-signal-safe, sig_atomic_t)
- [ ] T04 — Máscara de señales (sigprocmask, sigpending)

**S02 — Señales Avanzadas**
- [ ] T01 — Señales en tiempo real (SIGRTMIN-SIGRTMAX, sigqueue)
- [ ] T02 — signalfd (señales como file descriptors, Linux-específico)
- [ ] T03 — Self-pipe trick (despertar poll/select desde signal handler)

### C04 — IPC — Comunicación entre Procesos `[A]`

**S01 — Pipes**
- [ ] T01 — Pipes anónimos (pipe(), comunicación padre↔hijo)
- [ ] T02 — Named pipes (FIFOs, mkfifo, procesos no relacionados)
- [ ] T03 — Redirección con dup2 (implementar tuberías de shell)

**S02 — IPC System V y POSIX**
- [ ] T01 — Memoria compartida (shmget/shmat SysV, shm_open/mmap POSIX)
- [ ] T02 — Semáforos (semget SysV, sem_open/sem_post/sem_wait POSIX)
- [ ] T03 — Colas de mensajes (msgget SysV, mq_open POSIX)
- [ ] T04 — SysV vs POSIX IPC (diferencias, cuál preferir, portabilidad)

### C05 — Hilos (pthreads) `[A]`

**S01 — Fundamentos**
- [ ] T01 — pthread_create y pthread_join (creación, espera, argumentos)
- [ ] T02 — Datos compartidos (race conditions, por qué son peligrosas)
- [ ] T03 — Mutex (lock/unlock, deadlocks, PTHREAD_MUTEX_ERRORCHECK)

**S02 — Sincronización**
- [ ] T01 — Variables de condición (cond_wait, cond_signal, spurious wakeups)
- [ ] T02 — Read-write locks (pthread_rwlock, cuándo son ventajosos)
- [ ] T03 — Thread-local storage (pthread_key_create, __thread)
- [ ] T04 — Barriers y spinlocks (pthread_barrier, pthread_spin)

### C06 — Sockets `[A]`

**S01 — API de Sockets**
- [ ] T01 — socket, bind, listen, accept, connect (flujo completo TCP)
- [ ] T02 — UDP (sendto, recvfrom, diferencias con TCP)
- [ ] T03 — Unix domain sockets (AF_UNIX, paso de file descriptors)
- [ ] T04 — Network byte order (htonl/htons, sockaddr_in, inet_pton)

**S02 — I/O Multiplexado**
- [ ] T01 — select (API, FD_SETSIZE, timeout)
- [ ] T02 — poll (POLLIN, POLLOUT, POLLERR)
- [ ] T03 — epoll (epoll_create, epoll_ctl, edge vs level triggered)
- [ ] T04 — Servidor concurrente (fork, thread pool, event loop con epoll)

### C07 — Proyecto: Mini-shell `[P]`

- [ ] T01 — Parser de comandos (tokenización, pipes, redirección)
- [ ] T02 — Ejecución (fork+exec, pipes encadenados)
- [ ] T03 — Built-ins (cd, exit, señales, Ctrl+C no mata la shell)
- [ ] T04 — Job control básico (background &, fg, lista de jobs)

### C08 — Proyecto: Servidor HTTP Básico en C `[P]`

**S01 — Servidor Single-thread**
- [ ] T01 — Socket TCP listener (bind, accept en loop, fork por conexión)
- [ ] T02 — Parser de HTTP request (método, path, headers)
- [ ] T03 — Response HTTP (status line, headers, body)
- [ ] T04 — Servir archivos estáticos (MIME types, 404)

**S02 — Mejoras**
- [ ] T01 — Thread pool (reutilizar hilos en lugar de fork)
- [ ] T02 — Directory listing (índice HTML autogenerado)
- [ ] T03 — Logging de requests (IP, método, path, status, timestamp)

### C09 — Proyecto: Mini Runtime de Contenedor `[P]`

> Capítulo pendiente de definición — directorio creado, contenido aún no especificado.

- [ ] T01 — Namespaces de Linux (PID, NET, MNT, UTS, IPC — clone flags)
- [ ] T02 — cgroups (limitar CPU/memoria, jerarquía, cgroup v2)
- [ ] T03 — chroot y pivot_root (cambiar la raíz del filesystem)
- [ ] T04 — Construcción del runtime (exec dentro del namespace, imagen mínima)

---

## B07 — Programación de Sistemas en Rust

### C01 — Concurrencia con Hilos `[A]`

**S01 — Threads**
- [ ] T01 — std::thread::spawn (closures, JoinHandle, move semántica)
- [ ] T02 — Datos compartidos (Arc<T>, Mutex<T>, RwLock<T>)
- [ ] T03 — Channels (mpsc::channel, sync_channel, mensajes)
- [ ] T04 — Poisoning (qué pasa si un thread paniquea con Mutex locked)

**S02 — Concurrencia sin Data Races**
- [ ] T01 — Send y Sync (qué significan, por qué Rc no es Send)
- [ ] T02 — Atomics (AtomicBool, AtomicUsize, Ordering)
- [ ] T03 — Scoped threads (std::thread::scope Rust 1.63+)
- [ ] T04 — Rayon (par_iter, join, paralelismo de datos)

### C02 — Async/Await `[A]`

**S01 — Fundamentos Async**
- [ ] T01 — Futures y el trait Future (Poll, Pending, Ready, lazy)
- [ ] T02 — async fn y .await (máquina de estados del compilador)
- [ ] T03 — Runtime Tokio (tokio::main, spawn, select!, JoinHandle)
- [ ] T04 — Pin y Unpin (self-referential structs, cuándo preocuparse)

**S02 — I/O Asíncrono**
- [ ] T01 — tokio::fs y tokio::net (TcpListener, TcpStream, archivos)
- [ ] T02 — Channels async (mpsc, broadcast, oneshot, watch)
- [ ] T03 — Timeouts y cancelación (tokio::time::timeout, select!, drop)
- [ ] T04 — Async vs threads (cuándo usar cada uno, tradeoffs)

### C03 — Sistema de Archivos `[A]`

**S01 — Operaciones de FS**
- [ ] T01 — std::fs (read, write, create_dir_all, remove_file, rename)
- [ ] T02 — std::path (Path, PathBuf, join, canonicalize, diferencias OS)
- [ ] T03 — Recorrido de directorios (read_dir, walkdir crate, symlinks)
- [ ] T04 — Permisos y metadata (permissions, set_permissions)

### C04 — Networking en Rust `[A]`

**S01 — std::net**
- [ ] T01 — TcpListener y TcpStream (bind, accept, connect, read/write)
- [ ] T02 — UdpSocket (send_to, recv_from, broadcast)
- [ ] T03 — Servidor multi-cliente (un thread por conexión, thread pool)

**S02 — Networking Async**
- [ ] T01 — Servidor TCP con Tokio (TcpListener, spawn por conexión)
- [ ] T02 — Protocolo custom (framing, serde, length-prefixed messages)
- [ ] T03 — HTTP client (reqwest, GET/POST, JSON, manejo de errores)

### C05 — FFI (Foreign Function Interface) `[M]`

**S01 — Llamar C desde Rust**
- [ ] T01 — extern "C" y #[link] (declarar funciones C, linking)
- [ ] T02 — Tipos FFI (std::ffi: CStr, CString, c_int, c_char)
- [ ] T03 — bindgen (bindings automáticos desde headers C)
- [ ] T04 — Wrappers seguros (envolver APIs C en interfaces idiomáticas)

**S02 — Llamar Rust desde C**
- [ ] T01 — #[no_mangle] y extern "C" (exportar como ABI C)
- [ ] T02 — cbindgen (generar headers .h desde Rust)
- [ ] T03 — Tipos opacos (Box<T> como puntero, free functions, ownership)

### C06 — unsafe Rust `[A]`

**S01 — Los 5 Superpoderes**
- [ ] T01 — Raw pointers (*const T, *mut T, creación segura, deref unsafe)
- [ ] T02 — unsafe fn y bloques (safety invariants, // SAFETY: comments)
- [ ] T03 — Traits unsafe (Send/Sync manualmente, cuándo necesario)
- [ ] T04 — Union access y static mut
- [ ] T05 — extern functions (siempre unsafe)

**S02 — Buen Uso de unsafe**
- [ ] T01 — Minimizar scope (bloques lo más pequeños posible)
- [ ] T02 — Documentar invariantes (por qué es correcto)
- [ ] T03 — Miri (detección de UB, cargo miri test)

### C07 — Debugging de Programas de Sistema `[M]`

**S01 — Debugging en C (Avanzado)**
- [ ] T01 — GDB avanzado (conditional breakpoints, watchpoints, core dumps)
- [ ] T02 — strace avanzado (-T, -c, -f follow forks)
- [ ] T03 — Core dumps (ulimit -c, coredumpctl, analizar con GDB)
- [ ] T04 — ltrace (llamadas a bibliotecas dinámicas vs strace)

**S02 — Debugging en Rust**
- [ ] T01 — RUST_BACKTRACE (niveles 0/1/full, interpretar)
- [ ] T02 — cargo-expand (código generado por macros, derive)
- [ ] T03 — Miri (detección de UB, limitaciones)
- [ ] T04 — ThreadSanitizer (-Zsanitizer=thread, data races)

**S03 — Metodología**
- [ ] T01 — Investigar un crash (reproducir → aislar → diagnosticar → corregir)
- [ ] T02 — Bisección (git bisect, automatización con scripts)
- [ ] T03 — Profiling básico (perf stat, flame graphs, CPU vs I/O)

### C08 — Proyecto: Utilidad CLI Tipo grep `[P]`

- [ ] T01 — Argument parsing (clap crate, subcomandos, flags)
- [ ] T02 — Búsqueda en archivos (regex crate, multi-archivo, recursivo)
- [ ] T03 — Salida coloreada (colored crate, detección de TTY)
- [ ] T04 — Rendimiento (buffered I/O, memmap, rayon)

### C09 — Proyecto: Chat TCP Multi-cliente `[P]`

**S01 — Servidor**
- [ ] T01 — Servidor con Tokio (TcpListener, spawn, broadcast)
- [ ] T02 — Protocolo de chat (framing, nicknames, comandos /nick /quit)
- [ ] T03 — Estado compartido (Arc<Mutex<HashMap>> de clientes)

**S02 — Cliente**
- [ ] T01 — Cliente TUI (stdin + socket con tokio::select!)
- [ ] T02 — Mejora 1: historial con scroll
- [ ] T03 — Mejora 2: salas/canales (/join, /msg)
- [ ] T04 — Mejora 3: persistir historial al disco

---

## B08 — Almacenamiento y Sistemas de Archivos

### C00 — QEMU/KVM y Entorno de Virtualización `[A]`

**S01 — Fundamentos de Virtualización**
- [ ] T01 — Emulación vs virtualización (Intel VT-x / AMD-V, hardware-assisted)
- [ ] T02 — Hipervisores tipo 1 vs 2 (bare-metal, hosted, KVM)
- [ ] T03 — La pila QEMU/KVM (emulator, kernel module, aceleración)

**S02 — Instalación y Verificación**
- [ ] T01 — Paquetes (qemu-kvm, libvirt, virt-install, virt-manager)
- [ ] T02 — Configuración de libvirtd (systemd enable, grupo libvirt)
- [ ] T03 — Verificación completa (virt-host-validate)

**S03 — Imágenes de Disco y Storage Pools**
- [ ] T01 — Formatos (raw, qcow2, thin provisioning, rendimiento)
- [ ] T02 — Imágenes backing (overlays, copy-on-write, linked clones)
- [ ] T03 — Storage pools de libvirt (pool-define-as, vol-list)

**S04 — Creación de VMs con virt-install**
- [ ] T01 — Instalación desde ISO (--name, --ram, --vcpus, --disk, --cdrom)
- [ ] T02 — Instalación desatendida (kickstart / preseed)
- [ ] T03 — Imágenes cloud-init (Debian/Alma cloud images, seed.iso)
- [ ] T04 — Plantillas de VMs para el curso

**S05 — Gestión de VMs con virsh**
- [ ] T01 — Ciclo de vida (start, shutdown, destroy, suspend, resume, undefine)
- [ ] T02 — Modificar VMs (virsh edit, attach-disk, attach-interface, autostart)
- [ ] T03 — Snapshots (snapshot-create-as, snapshot-revert, snapshot-delete)
- [ ] T04 — Acceso a la consola (virt-viewer, virsh console, SSH)

**S06 — Networking Virtual**
- [ ] T01 — Red NAT por defecto (192.168.122.0/24, DHCP)
- [ ] T02 — Red aislada (isolated, sin internet)
- [ ] T03 — Red bridge (VM aparece como máquina física)
- [ ] T04 — Redes internas multi-subnet (simulación de routing)

**S07 — Almacenamiento Virtual — Discos Extra**
- [ ] T01 — Agregar discos (qemu-img create, virsh attach-disk)
- [ ] T02 — Múltiples discos para labs (RAID, LVM)
- [ ] T03 — Passthrough de dispositivos

**S08 — Automatización de Entornos de Lab**
- [ ] T01 — Scripts de provisioning (crear VMs, adjuntar discos, redes)
- [ ] T02 — Vagrant con libvirt (opcional)
- [ ] T03 — Recetas por bloque (B08, B09, B10, B11 environments)

### C01 — Dispositivos de Bloque y Particiones `[A]`

**S01 — Dispositivos**
- [ ] T01 — Nomenclatura (/dev/sd*, /dev/nvme*, /dev/vd*)
- [ ] T02 — Información del dispositivo (lsblk, blkid, fdisk -l, udevadm)
- [ ] T03 — udev (reglas, persistent naming, /dev/disk/by-{id,uuid,label})

**S02 — Particionamiento**
- [ ] T01 — MBR vs GPT (limitaciones, estructura, cuándo usar cada uno)
- [ ] T02 — fdisk y gdisk (creación, tipos, alineación)
- [ ] T03 — parted (avanzado, redimensionar)
- [ ] T04 — Particiones en Docker (loop devices para simular discos)

### C02 — Sistemas de Archivos `[A]`

**S01 — Creación y Montaje**
- [ ] T01 — ext4 (mkfs.ext4, tune2fs, características, journal)
- [ ] T02 — XFS (mkfs.xfs, xfs_admin, xfs_repair, diferencias con ext4)
- [ ] T03 — Btrfs (subvolúmenes, snapshots, RAID integrado, compresión)
- [ ] T04 — mount y umount (opciones, flags, lazy unmount)

**S02 — Configuración Persistente**
- [ ] T01 — /etc/fstab (campos, opciones, UUID vs device path)
- [ ] T02 — systemd mount units (.mount y .automount)
- [ ] T03 — Filesystem check (fsck, e2fsck, xfs_repair)

### C03 — LVM `[A]`

**S01 — Volúmenes Lógicos**
- [ ] T01 — Conceptos (PV, VG, LV, PE — la cadena completa)
- [ ] T02 — Crear y gestionar (pvcreate, vgcreate, lvcreate, lvextend)
- [ ] T03 — Redimensionar en caliente (extender LV + filesystem sin desmontar)
- [ ] T04 — Snapshots LVM (crear, montar, revertir, limitaciones)

### C04 — RAID `[A]`

**S01 — RAID por Software**
- [ ] T01 — Niveles RAID (0, 1, 5, 6, 10 — capacidad y tolerancia)
- [ ] T02 — mdadm (crear, ensamblar, monitorear, reemplazar discos)
- [ ] T03 — /proc/mdstat y mdadm --detail (monitoreo, diagnóstico)
- [ ] T04 — RAID + LVM (combinación, orden de capas)

### C05 — Swap y Cuotas `[A]`

**S01 — Swap**
- [ ] T01 — Partición vs archivo swap (mkswap, swapon, prioridad)
- [ ] T02 — Swappiness (vm.swappiness, cuándo ajustar)

**S02 — Cuotas de Disco**
- [ ] T01 — Cuotas de usuario y grupo (quota, repquota, edquota)
- [ ] T02 — Configuración (usrquota/grpquota, quotacheck, quotaon)
- [ ] T03 — Soft vs hard limits (grace period, comportamiento)

**S03 — Almacenamiento Moderno en RHEL**
- [ ] T01 — Stratis (pool-based, stratis pool create, snapshots)
- [ ] T02 — VDO (deduplicación/compresión, vdo create, estadísticas)
- [ ] T03 — Comparación (LVM+FS manual vs Stratis vs Btrfs)

---

## B09 — Redes

### C01 — Modelo TCP/IP `[A]`

**S01 — Capas y Protocolos**
- [ ] T01 — Las 4 capas (enlace, red, transporte, aplicación vs OSI)
- [ ] T02 — Encapsulación (headers, payload, MTU, fragmentación)
- [ ] T03 — Ethernet (frames, MAC, ARP, ARP cache)

**S02 — Capa de Red**
- [ ] T01 — IPv4 (formato, CIDR)
- [ ] T02 — Subnetting (máscara, cálculo, broadcast, hosts)
- [ ] T03 — IPv6 (formato, tipos, link-local, autoconfiguración)
- [ ] T04 — ICMP (ping, traceroute, Path MTU Discovery)

**S03 — Capa de Transporte**
- [ ] T01 — TCP (three-way handshake, estados, ventana, retransmisión)
- [ ] T02 — UDP (sin conexión, cuándo preferir)
- [ ] T03 — Puertos (well-known, registered, ephemeral, /etc/services)

**S04 — Capa de Aplicación (HTTP)**
- [ ] T01 — HTTP/1.1 (request/response, métodos, status codes 1xx-5xx)
- [ ] T02 — Headers HTTP (Content-Type, Content-Length, MIME types)
- [ ] T03 — CORS (Same-Origin Policy, preflight, Access-Control-Allow-*)
- [ ] T04 — HTTP/2 y HTTP/3 (multiplexing, QUIC, binary framing)
- [ ] T05 — WebSocket (upgrade, bidireccional, frames)

### C02 — DNS `[M]`

**S01 — Funcionamiento**
- [ ] T01 — Jerarquía DNS (root servers, TLDs, FQDN)
- [ ] T02 — Tipos de registro (A, AAAA, CNAME, MX, NS, PTR, SOA, TXT)
- [ ] T03 — Resolución (recursiva vs iterativa, caching, TTL)
- [ ] T04 — /etc/resolv.conf y /etc/hosts (nsswitch.conf)

**S02 — Herramientas**
- [ ] T01 — dig y nslookup (consultas, interpretar salida)
- [ ] T02 — host (consultas simples)
- [ ] T03 — systemd-resolved (resolvectl)

### C03 — DHCP `[A]`

**S01 — Protocolo y Configuración**
- [ ] T01 — Funcionamiento DHCP (DORA: Discover, Offer, Request, Acknowledge)
- [ ] T02 — Leases (tiempo de alquiler, renovación, rebinding)
- [ ] T03 — Configuración de cliente (dhclient, NetworkManager, systemd-networkd)

### C04 — Configuración de Red en Linux `[M]`

**S01 — Herramientas Modernas**
- [ ] T01 — ip command (ip addr, ip route, ip link, ip neigh)
- [ ] T02 — NetworkManager (nmcli, nmtui, connection profiles)
- [ ] T03 — systemd-networkd (.network files)
- [ ] T04 — /etc/network/interfaces (configuración clásica Debian)

**S02 — Configuración Avanzada**
- [ ] T01 — Hostname (hostnamectl, /etc/hostname)
- [ ] T02 — Bonding y teaming (alta disponibilidad, modos)
- [ ] T03 — VLANs (802.1Q, ip link, NetworkManager)
- [ ] T04 — Bridge networking (bridges, virtualización, Docker)
- [ ] T05 — VPN tunnels (WireGuard kernel-integrado, comparación OpenVPN)

### C05 — Firewall `[M]`

**S01 — Netfilter e iptables**
- [ ] T01 — Arquitectura Netfilter (tablas filter/nat/mangle, chains)
- [ ] T02 — iptables (reglas, targets ACCEPT/DROP/REJECT/LOG)
- [ ] T03 — NAT con iptables (SNAT, DNAT, MASQUERADE, port forwarding)
- [ ] T04 — nftables (sucesor, sintaxis, migración)

**S02 — firewalld**
- [ ] T01 — Zonas (public, internal, trusted, modelo de confianza)
- [ ] T02 — firewall-cmd (servicios, puertos, permanent vs runtime)
- [ ] T03 — Direct rules (integración con iptables, cuándo necesarias)

### C06 — Diagnóstico de Red `[M]`

**S01 — Herramientas**
- [ ] T01 — ss (reemplazo de netstat, filtros, estados)
- [ ] T02 — tcpdump (captura, filtros BPF, análisis básico)
- [ ] T03 — traceroute y mtr (diagnóstico de rutas)
- [ ] T04 — nmap (escaneo de puertos, detección de servicios)
- [ ] T05 — curl y wget (pruebas HTTP, flags de diagnóstico)

### C07 — Redes en Docker `[M]`

**S01 — Networking de Contenedores**
- [ ] T01 — Bridge networking avanzado (subnets custom, gateways)
- [ ] T02 — Host networking (cuándo usar, seguridad)
- [ ] T03 — Lab de red multi-contenedor (topologías con Docker Compose)

---

## B10 — Servicios de Red

### C01 — SSH `[A]`

**S01 — Configuración Avanzada**
- [ ] T01 — sshd_config (PermitRootLogin, PasswordAuthentication, AllowUsers)
- [ ] T02 — Autenticación por clave (ssh-keygen, authorized_keys, ssh-agent)
- [ ] T03 — SSH tunneling (local -L, remote -R, dynamic -D)
- [ ] T04 — SSH hardening (fail2ban, rate limiting, cipher suites)
- [ ] T05 — ~/.ssh/config (Host aliases, ProxyJump, IdentityFile)

### C02 — Servidor DNS (BIND) `[A]`

**S01 — Instalación y Configuración**
- [ ] T01 — Instalación (paquetes Debian vs RHEL, named.conf)
- [ ] T02 — Zonas (forward, reversa, SOA, archivos de zona)
- [ ] T03 — Tipos de servidor (caching-only, forwarder, authoritative)
- [ ] T04 — DNSSEC (conceptos, validación, firma de zonas)

**S02 — Diagnóstico**
- [ ] T01 — named-checkconf y named-checkzone
- [ ] T02 — rndc (control remoto)
- [ ] T03 — Logs de BIND (canales, debugging)

### C03 — Servidor Web `[A]`

**S01 — Apache HTTP Server**
- [ ] T01 — Instalación y estructura (httpd.conf, sites-available, conf.d)
- [ ] T02 — Virtual hosts (name-based, DocumentRoot)
- [ ] T03 — Módulos (mod_rewrite, mod_proxy, mod_ssl)
- [ ] T04 — .htaccess (override, performance, alternativas)

**S02 — Nginx**
- [ ] T01 — Instalación y estructura (nginx.conf, server blocks, location)
- [ ] T02 — Proxy inverso (proxy_pass, headers, WebSocket proxying)
- [ ] T03 — Servir archivos estáticos (root vs alias, try_files, caching)
- [ ] T04 — Apache vs Nginx (prefork/worker vs event-driven)

**S03 — HTTPS y TLS**
- [ ] T01 — Certificados (CSR, self-signed, Let's Encrypt concepto)
- [ ] T02 — TLS en Apache (mod_ssl, SSLCertificateFile, cipher suites)
- [ ] T03 — TLS en Nginx (ssl_certificate, ssl_protocols, HSTS)
- [ ] T04 — Protocolos web (HTTP/1.1, HTTP/2, WebSocket para WASM)

### C04 — Compartición de Archivos `[A]`

**S01 — NFS**
- [ ] T01 — Servidor NFS (/etc/exports, exportfs, opciones rw/sync)
- [ ] T02 — Cliente NFS (montaje manual, fstab, autofs)
- [ ] T03 — NFSv4 vs NFSv3 (Kerberos, id mapping)

**S02 — Samba**
- [ ] T01 — Configuración (smb.conf, shares, usuarios)
- [ ] T02 — Cliente SMB/CIFS (smbclient, mount.cifs)
- [ ] T03 — Integración con usuarios Linux (smbpasswd, permisos)

### C05 — Otros Servicios `[A]`

**S01 — DHCP Server**
- [ ] T01 — ISC DHCP server (dhcpd.conf, rangos, reservaciones)
- [ ] T02 — Diagnóstico (logs, tcpdump para ver DORA)

**S02 — Email (Fundamentos)**
- [ ] T01 — Conceptos (MTA, MDA, MUA, SMTP, POP3, IMAP)
- [ ] T02 — Postfix básico (main.cf, configuración mínima, relay)
- [ ] T03 — mailq y logs (diagnóstico de entrega)

**S03 — PAM**
- [ ] T01 — Arquitectura PAM (auth, account, password, session)
- [ ] T02 — Archivos (/etc/pam.d/, sintaxis, orden de módulos)
- [ ] T03 — Módulos comunes (pam_unix, pam_limits, pam_wheel, pam_faillock)

**S04 — LDAP Client**
- [ ] T01 — Conceptos (DN, base DN, LDIF, esquema, operaciones)
- [ ] T02 — sssd (instalación, sssd.conf, dominios LDAP/AD)
- [ ] T03 — Integración NSS y PAM (nsswitch.conf, pam_sss)
- [ ] T04 — Diagnóstico (ldapsearch, sssctl, logs)

**S05 — Proxy Forward**
- [ ] T01 — Proxy forward vs reverse (casos de uso corporativos)
- [ ] T02 — Squid básico (squid.conf, ACLs, caché, autenticación)
- [ ] T03 — Configuración de clientes (http_proxy, proxy transparente)

---

## B11 — Seguridad, Kernel y Arranque

### C01 — SELinux `[A]`

**S01 — Fundamentos**
- [ ] T01 — MAC vs DAC (por qué SELinux, qué protege)
- [ ] T02 — Modos (Enforcing, Permissive, Disabled — getenforce, setenforce)
- [ ] T03 — Contextos (user:role:type:level, ls -Z, ps -Z)
- [ ] T04 — Política targeted (procesos confinados, unconfined_t)

**S02 — Gestión**
- [ ] T01 — Booleans (getsebool, setsebool -P, semanage boolean)
- [ ] T02 — File contexts (semanage fcontext, restorecon, chcon)
- [ ] T03 — Troubleshooting (audit2why, audit2allow, sealert)
- [ ] T04 — Ports (semanage port, servicios en puertos no estándar)

### C02 — AppArmor `[A]`

**S01 — Fundamentos**
- [ ] T01 — Perfiles (enforce vs complain, aa-status)
- [ ] T02 — Gestión (aa-enforce, aa-complain, aa-disable)
- [ ] T03 — Crear perfiles (aa-genprof, aa-logprof, sintaxis)
- [ ] T04 — SELinux vs AppArmor (comparación práctica)

### C03 — Hardening y Auditoría `[A]`

**S01 — Seguridad del Sistema**
- [ ] T01 — sudo avanzado (/etc/sudoers, visudo, NOPASSWD, restricción)
- [ ] T02 — auditd (reglas, ausearch, aureport, monitoreo de archivos)
- [ ] T03 — Hardening básico (desactivar servicios, kernel parameters)
- [ ] T04 — GPG (generación de claves, cifrado, firma, keyservers)

**S02 — Criptografía y PKI**
- [ ] T01 — OpenSSL (generación de certificados, CSR, s_client)
- [ ] T02 — LUKS (cifrado de disco, cryptsetup, desbloqueo al arranque)
- [ ] T03 — dm-crypt (capa inferior de LUKS, configuración manual)

### C04 — Proceso de Arranque `[A]`

**S01 — Boot**
- [ ] T01 — BIOS vs UEFI (Secure Boot, EFI System Partition)
- [ ] T02 — GRUB2 (grub.cfg, grub-mkconfig, parámetros del kernel)
- [ ] T03 — Edición GRUB en boot (recovery, init=/bin/bash, password GRUB)
- [ ] T04 — Proceso completo (firmware → bootloader → kernel → initramfs → target)

**S02 — Initramfs**
- [ ] T01 — Qué es initramfs (para qué, cuándo regenerar)
- [ ] T02 — dracut (RHEL) vs update-initramfs (Debian)

### C05 — Kernel `[A]`

**S01 — Módulos del Kernel**
- [ ] T01 — lsmod, modprobe, modinfo (listar, cargar, descargar)
- [ ] T02 — /etc/modprobe.d/ (blacklisting, opciones, aliases)
- [ ] T03 — depmod (dependencias, modules.dep)

**S02 — Configuración en Runtime**
- [ ] T01 — sysctl (/proc/sys, /etc/sysctl.conf, parámetros comunes)
- [ ] T02 — /proc y /sys en profundidad (tuning de red, scheduler)
- [ ] T03 — Compilación del kernel (.config, make menuconfig, install)
- [ ] T04 — dkms (módulos de terceros, reconstrucción automática)

### C06 — Mantenimiento del Sistema `[A]`

**S01 — Backup y Recuperación**
- [ ] T01 — tar (backups, compresión, incrementales --listed-incremental)
- [ ] T02 — rsync (--delete, -a, exclusiones, backups remotos SSH)
- [ ] T03 — dd (clonar discos, crear imágenes, precauciones)
- [ ] T04 — Estrategias (completo, incremental, diferencial, 3-2-1)

**S02 — Automatización**
- [ ] T01 — Scripts de mantenimiento (logs, espacio, alertas)
- [ ] T02 — Compilación desde fuente avanzada (configure options, DESTDIR)

### C07 — Proyecto: System Health Check `[P]`

**S01 — Herramienta de Diagnóstico**
- [ ] T01 — Recolección de datos (df, free, load average, systemctl --failed)
- [ ] T02 — Formateo y reporte (salida coloreada, umbrales, resumen)
- [ ] T03 — Mejora 1: exportar como JSON o HTML
- [ ] T04 — Mejora 2: TUI con ratatui (paneles de disco, memoria, servicios)

---

## B12 — GUI con Rust (egui)

### C01 — Introducción a egui y eframe `[A]`

**S01 — Fundamentos**
- [ ] T01 — Immediate mode GUI (diferencia con retained mode, tradeoffs)
- [ ] T02 — eframe (App trait, setup, update loop, egui::Context)
- [ ] T03 — Proyecto mínimo (ventana con texto, Cargo.toml, compilar)
- [ ] T04 — El render loop (RequestRepaint, cuándo se redibuja)
- [ ] T05 — egui vs alternativas (iced, gtk-rs, Slint, Tauri)

**S02 — Widgets Básicos**
- [ ] T01 — Labels y botones (ui.label(), ui.button(), clicks)
- [ ] T02 — Text input (TextEdit, singleline vs multiline)
- [ ] T03 — Sliders y checkboxes (binding a estado)
- [ ] T04 — ComboBox y selección (dropdown, enum como opciones)

**S03 — Layout**
- [ ] T01 — Horizontal y vertical (ui.horizontal(), anidamiento)
- [ ] T02 — Panels (SidePanel, TopBottomPanel, CentralPanel)
- [ ] T03 — ScrollArea (contenido largo, scroll vertical/horizontal)
- [ ] T04 — Grid y columns (distribución tabular, alineación)

**S04 — Temas y Accesibilidad**
- [ ] T01 — Dark/light mode (Visuals, detección de preferencia del OS)
- [ ] T02 — Escalado y DPI (ctx.set_pixels_per_point(), HiDPI)
- [ ] T03 — Limitaciones de accesibilidad (screen readers, AccessKit)

### C02 — Proyecto: Calculadoras `[P]`

- [ ] T01 — Calculadora simple (dos campos, operación, resultado)
- [ ] T02 — Calculadora regular (teclado en pantalla, display)
- [ ] T03 — Calculadora científica (sin/cos/tan/√/log, shunting-yard)

### C03 — Proyecto: Reloj y Temporizadores `[P]`

- [ ] T01 — Reloj digital (hora actual, actualización por segundo)
- [ ] T02 — Cronómetro (Start/Stop/Reset, Lap)
- [ ] T03 — Cuenta regresiva (input de tiempo, alerta visual)

### C04 — Estado, Persistencia y Archivos `[A]`

**S01 — Gestión de Estado**
- [ ] T01 — Estado en struct App (campos mutables, modelo-vista)
- [ ] T02 — IDs en egui (por qué importan, conflictos, push_id)
- [ ] T03 — Diálogos de archivo (rfd crate, abrir/guardar)

**S02 — Serialización**
- [ ] T01 — serde (Serialize/Deserialize, #[derive], JSON)
- [ ] T02 — Persistencia (eframe::Storage, guardar al cerrar)
- [ ] T03 — Formatos (JSON, TOML, RON)

**S03 — Atajos de Teclado**
- [ ] T01 — ctx.input() (Key enum, Modifiers)
- [ ] T02 — Combinaciones (Ctrl+S, Ctrl+Z, Ctrl+Shift+S)
- [ ] T03 — Conflictos con TextEdit
- [ ] T04 — Mapa de atajos configurable (HashMap<KeyCombo, Action>)

### C05 — Proyecto: Editores de Texto `[P]`

- [ ] T01 — Editor básico (TextEdit multiline, guardar/abrir)
- [ ] T02 — Editor Markdown (pulldown-cmark, panel split, preview)

### C06 — Proyecto: Lista de Tareas `[P]`

- [ ] T01 — Todo list (checkbox, texto, delete, filtros)
- [ ] T02 — Drag and drop, persistencia JSON, múltiples listas

### C07 — Proyecto: Gestión Tabular `[P]`

- [ ] T01 — Tabla editable (filas/columnas, ordenar, CSV)
- [ ] T02 — Editor tipo Excel (fórmulas, referencias de celda)

### C08 — Painter y Gráficos 2D `[A]`

**S01 — API de Painter**
- [ ] T01 — Painter (líneas, rectángulos, círculos, texto)
- [ ] T02 — Colores (Color32, Rgba, RGB↔HSL)
- [ ] T03 — Shapes y Stroke (grosor, relleno, anti-aliasing)
- [ ] T04 — Transformaciones (translate, rotate manual con trigonometría)

**S02 — Interacción con Canvas**
- [ ] T01 — Sense y Response (hover, click, drag en regiones)
- [ ] T02 — Coordenadas (screen space vs canvas space)
- [ ] T03 — Texturas (cargar imágenes, TextureHandle, performance)

### C09 — Proyecto: Dibujo Libre `[P]`

- [ ] T01 — Lienzo (dibujar con mouse, strokes, color, pincel, PNG)
- [ ] T02 — Herramientas geométricas (línea, rectángulo, círculo, Ctrl+Z)

### C10 — Proyecto: Apps de Imágenes `[P]`

- [ ] T01 — Visor (cargar imagen, zoom, pan, selección de región)
- [ ] T02 — Navegador de imágenes (carpeta, miniaturas, búsqueda)
- [ ] T03 — Color picker (RGB/HSL/HEX, portapapeles, paleta)

---

## B13 — Multimedia con GStreamer

### C01 — Introducción a GStreamer `[A]`

**S01 — Instalación y Verificación**
- [ ] T01 — Paquetes Debian (gstreamer1.0-tools, plugins base/good/bad/ugly)
- [ ] T02 — Paquetes RHEL/Fedora (gstreamer1-devel, RPM Fusion para codecs)
- [ ] T03 — Verificación (gst-inspect-1.0, gst-launch-1.0)
- [ ] T04 — Bindings Rust (gstreamer-rs, pkg-config, errores comunes)

**S02 — Fundamentos**
- [ ] T01 — Arquitectura (pipelines, elements, pads, caps, bus)
- [ ] T02 — Bindings de Rust (inicialización, pipeline textual)
- [ ] T03 — Estados (Null → Ready → Paused → Playing)
- [ ] T04 — El bus de mensajes (EOS, Error, StateChanged, tags)

**S03 — Pipelines**
- [ ] T01 — Elements comunes (filesrc, decodebin, autoaudiosink)
- [ ] T02 — playbin (URI playback, propiedades volume/uri)
- [ ] T03 — Pipeline custom (enlazar elements, pad linking)

### C02 — Pipeline de Audio `[A]`

**S01 — Audio en GStreamer**
- [ ] T01 — Decodificación (MP3, WAV, FLAC, OGG, decodebin)
- [ ] T02 — AppSink para audio (muestras crudas, formato, canales)
- [ ] T03 — Metadata (GStreamer Discoverer, tags)
- [ ] T04 — Posición y duración (query, seek preciso)

### C03 — Proyecto: Reproductor de Audio `[P]`

- [ ] T01 — Básico (play/pause, barra de progreso, volumen, metadatos)
- [ ] T02 — Con playlist (anterior/siguiente, shuffle, forma de onda)

### C04 — Pipeline de Video `[A]`

**S01 — Video en GStreamer**
- [ ] T01 — Decodificación (MP4, MKV, WebM, codecs, contenedores)
- [ ] T02 — AppSink para video (frames RGBA, SampleRef, MapInfo)
- [ ] T03 — new_preroll vs new_sample (primer frame vs sucesivos)
- [ ] T04 — Sincronización A/V (timestamps, pipeline clock)

**S02 — Renderizado**
- [ ] T01 — Frames a textura egui (TextureHandle, update por frame)
- [ ] T02 — Aspect ratio y escalado (letter/pillarboxing)
- [ ] T03 — Performance (tasa de frames, buffering, threading)

### C05 — Proyecto: Reproductores de Video `[P]`

- [ ] T01 — Básico (play/pause, drag & drop, volumen)
- [ ] T02 — Con timeline (barra interactiva, frame stepping, velocidad)
- [ ] T03 — Con marcadores (crear, saltar, exportar timestamps, subtítulos)

---

## B14 — Interoperabilidad C/Rust y WebAssembly

### C01 — FFI Avanzado `[M]`

**S01 — Patrones Complejos**
- [ ] T01 — Callbacks (funciones Rust como callbacks a C, trampolines)
- [ ] T02 — Tipos opacos bidireccionales (C→Rust y Rust→C, lifetimes)
- [ ] T03 — Error handling cruzado (propagar errores entre lenguajes)
- [ ] T04 — Datos complejos (structs con punteros, ownership cruzado)

**S02 — Tooling**
- [ ] T01 — bindgen avanzado (allowlist, blocklist, build.rs)
- [ ] T02 — cbindgen avanzado (renaming, includes, orden)
- [ ] T03 — Integración CMake+Cargo (linking cruzado)

### C02 — Bibliotecas Compartidas C/Rust `[M]`

**S01 — Biblioteca Dinámica**
- [ ] T01 — Rust cdylib (crate-type = ["cdylib"], exportar funciones)
- [ ] T02 — Consumir desde C (linking, headers, ldconfig)
- [ ] T03 — C .so consumida desde Rust (build.rs, pkg-config)

**S02 — Plugin System**
- [ ] T01 — Plugins con dlopen (sistema en C con carga dinámica)
- [ ] T02 — Plugin Rust cargado por C (ABI compatible, versionado)
- [ ] T03 — Plugin C cargado por Rust (libloading crate, safety wrappers)

### C03 — Introducción a WebAssembly `[A]`

**S01 — Conceptos**
- [ ] T01 — Qué es WebAssembly (.wasm, .wat, sandboxing)
- [ ] T02 — WASI (filesystem, networking, preview1 vs preview2)
- [ ] T03 — Casos de uso (navegador, serverless, edge, plugins)
- [ ] T04 — WASM fuera del navegador (Wasmtime, Wasmer, WasmEdge)
- [ ] T05 — WASM como plugin (aislamiento sin contenedores, shared-nothing)

**S02 — Rust → WASM**
- [ ] T01 — wasm-pack (instalación, build, targets web/bundler/nodejs)
- [ ] T02 — wasm-bindgen (#[wasm_bindgen], JsValue)
- [ ] T03 — Ejemplo práctico (función Rust → WASM → navegador)
- [ ] T04 — Limitaciones (threads con SharedArrayBuffer, sin DOM directo)
- [ ] T05 — trunk (bundling simplificado, hot reload)

**S03 — C → WASM**
- [ ] T01 — Emscripten (emcc, compilar C a WASM)
- [ ] T02 — WASI SDK (sin Emscripten, wasmtime/wasmer)
- [ ] T03 — Comparación Rust vs C para WASM

### C04 — Proyecto Final: App C/Rust con Módulo WASM `[P]`

- [ ] T01 — Biblioteca compartida C/Rust (lógica en Rust, interfaz C)
- [ ] T02 — Compilación dual (nativa .so y WASM .wasm desde el mismo código)
- [ ] T03 — Programa C consumidor (usa biblioteca nativa via FFI)
- [ ] T04 — Página web consumidora (usa módulo WASM, misma lógica)

---

## B15 — Estructuras de Datos en C y Rust

### C01 — Fundamentos transversales `[A]`

**S01 — TAD**
- [ ] T01 — Definición y contrato (interfaz, invariantes, pre/postcondiciones)
- [ ] T02 — TADs en C (headers .h, structs opacos, patrón create/destroy)
- [ ] T03 — TADs en Rust (traits como contrato, visibilidad pub/pub(crate))
- [ ] T04 — Genericidad (void* + callbacks vs <T> + trait bounds)

**S02 — Complejidad algorítmica**
- [ ] T01 — Notación O, Ω, Θ ▸ *README_EXTRA pendiente*
- [ ] T02 — Análisis mejor, peor, caso promedio
- [ ] T03 — Análisis amortizado (método del banquero, del potencial) ▸ *README_EXTRA pendiente*
- [ ] T04 — Complejidad espacial (in-place O(1), tradeoffs)
- [ ] T05 — Clases comunes (O(1) hasta O(n!), tabla y gráfica mental)

**S03 — Herramientas para estructuras dinámicas**
- [ ] T01 — Valgrind para DS (patrones de leak, use-after-free)
- [ ] T02 — GDB para punteros (inspeccionar nodos, watchpoints)
- [ ] T03 — ASan y Miri (-fsanitize=address, cargo miri test)
- [ ] T04 — Testing de estructuras (invariantes, stress tests, proptest)

**S04 — unsafe Rust**
- [ ] T01 — Raw pointers (*const T, *mut T, null_mut, aritmética)
- [ ] T02 — Bloques unsafe (// SAFETY: comments, minimizar scope)
- [ ] T03 — NonNull y Box raw (Box::into_raw, Box::from_raw, patrón listas)
- [ ] T04 — Cuándo usar unsafe (árbol de decisión, benchmark de overhead)

### C02 — La pila `[A]`

**S01 — Concepto y operaciones**
- [ ] T01 — TAD pila (LIFO, push, pop, peek, is_empty, size)
- [ ] T02 — Implementación con array en C
- [ ] T03 — Implementación con lista en C
- [ ] T04 — Implementación en Rust

**S02 — Aplicaciones**
- [ ] T01 — Paréntesis balanceados
- [ ] T02 — Notación posfija (RPN)
- [ ] T03 — Conversión infija a posfija (Shunting-Yard)
- [ ] T04 — Evaluación de expresiones
- [ ] T05 — Otras (navegación, undo/redo, palíndromos)

### C03 — Recursión `[A]`

**S01 — Fundamentos**
- [ ] T01 — Definiciones recursivas (caso base, call stack) ▸ *README_EXTRA pendiente*
- [ ] T02 — Pila de llamadas (GDB backtrace, stack overflow, ulimit)
- [ ] T03 — Recursión vs iteración (pila explícita)
- [ ] T04 — Recursión de cola (TCO, loop en Rust)

**S02 — Recursión aplicada**
- [ ] T01 — Búsqueda binaria recursiva
- [ ] T02 — Torres de Hanoi (2ⁿ − 1 movimientos)
- [ ] T03 — Recursión mutua (is_even/is_odd, forward declarations)
- [ ] T04 — Backtracking (N-reinas, laberintos)
- [ ] T05 — Divide y vencerás (preview de merge sort y quicksort)

### C04 — Colas `[A]`

**S01 — La cola y variantes**
- [ ] T01 — TAD cola (FIFO, enqueue, dequeue, front)
- [ ] T02 — Array circular en C (front, rear, módulo wrap)
- [ ] T03 — Lista enlazada en C
- [ ] T04 — Implementación en Rust (VecDeque<T>)

**S02 — Colas especiales**
- [ ] T01 — Cola de prioridad (concepto, implementación ingenua)
- [ ] T02 — Deque
- [ ] T03 — Cola de prioridad con lista
- [ ] T04 — Simulación (sistema de atención)

### C05 — Listas enlazadas `[A]`

**S01 — Lista simplemente enlazada**
- [ ] T01 — Nodos y punteros (Node struct, NULL)
- [ ] T02 — Operaciones en C (insertar, buscar, eliminar, puntero a puntero)
- [ ] T03 — Recorrido e impresión
- [ ] T04 — Implementación Rust safe (Option<Box>, enum List)
- [ ] T05 — Implementación Rust unsafe (raw pointers)

**S02 — Lista doblemente enlazada**
- [ ] T01 — Nodos con doble enlace (prev, next)
- [ ] T02 — Operaciones en C
- [ ] T03 — Implementación en Rust (Rc<RefCell>, raw pointers)
- [ ] T04 — C vs Rust (ergonomía)

**S03 — Listas circulares y variantes**
- [ ] T01 — Lista circular simple
- [ ] T02 — Problema de Josephus
- [ ] T03 — Lista circular doble
- [ ] T04 — Listas no homogéneas
- [ ] T05 — Comparación de variantes

### C06 — Árboles `[A]`

**S01 — Árboles binarios**
- [ ] T01 — Definición y terminología (raíz, hojas, altura, propiedades)
- [ ] T02 — Representación con punteros C (struct TreeNode)
- [ ] T03 — Representación con array (hijo izquierdo 2i+1, derecho 2i+2)
- [ ] T04 — Representación en Rust (Option<Box<TreeNode<T>>>)

**S02 — Recorridos**
- [ ] T01 — Recursivos (inorden, preorden, postorden)
- [ ] T02 — Por niveles (BFS con cola)
- [ ] T03 — Iterativos (con pila explícita, Morris traversal)
- [ ] T04 — Aplicaciones (serialización, copia, igualdad, liberación)

**S03 — BST**
- [ ] T01 — Definición y propiedad (izq < nodo < der)
- [ ] T02 — Inserción ▸ *README_EXTRA pendiente*
- [ ] T03 — Búsqueda (mín, máx, sucesor, predecesor)
- [ ] T04 — Eliminación (hoja, un hijo, dos hijos)
- [ ] T05 — BST en Rust (Option<Box<Node<T>>>, match)
- [ ] T06 — Degeneración y balanceo (motivación para AVL/RB)

**S04 — Árboles balanceados**
- [ ] T01 — Rotaciones (simple izquierda/derecha, doble)
- [ ] T02 — Árboles AVL (factor de balance ∈ {-1,0,1}) ▸ *README_EXTRA pendiente*
- [ ] T03 — Árboles rojinegros (5 propiedades, coloreo) ▸ *README_EXTRA pendiente*
- [ ] T04 — AVL vs rojinegro (tabla comparativa)
- [ ] T05 — Implementación en Rust (BTreeMap como alternativa práctica)

**S05 — Árboles generales**
- [ ] T01 — Árboles n-arios (hijo-izquierdo/hermano-derecho)
- [ ] T02 — Árboles de expresión (infija/posfija, evaluación)
- [ ] T03 — Minimax (estados de juego, poda alfa-beta, tres en raya)
- [ ] T04 — Tries (búsqueda por prefijo, autocompletado)

### C07 — Montículos y colas de prioridad `[A]`

**S01 — Montículo binario**
- [ ] T01 — Definición (árbol binario completo, min/max heap, array implícito)
- [ ] T02 — Inserción (sift-up, O(log n))
- [ ] T03 — Extracción (sift-down, O(log n))
- [ ] T04 — Construcción heapify (bottom-up O(n)) ▸ *README_EXTRA pendiente*
- [ ] T05 — Implementación en C (array dinámico, comparador genérico)
- [ ] T06 — Implementación en Rust (BinaryHeap<T>, Reverse<T>)

**S02 — Aplicaciones**
- [ ] T01 — Heapsort ▸ *README_EXTRA pendiente*
- [ ] T02 — Cola de prioridad como TAD (insert, extract_min, decrease_key)
- [ ] T03 — Heap en grafos (Dijkstra, Prim — preview de C11)

### C08 — Ordenamiento `[A]`

**S01 — Fundamentos**
- [ ] T01 — Terminología (estabilidad, in-place, adaptativo, interno/externo)
- [ ] T02 — Límite inferior Ω(n log n) ▸ *README_EXTRA pendiente*
- [ ] T03 — Medir rendimiento (comparaciones, swaps, cache)

**S02 — Cuadráticos O(n²)**
- [ ] T01 — Bubble sort
- [ ] T02 — Selection sort
- [ ] T03 — Insertion sort
- [ ] T04 — Cuándo usar O(n²)

**S03 — Eficientes O(n log n)**
- [ ] T01 — Merge sort ▸ *README_EXTRA pendiente*
- [ ] T02 — Quicksort ▸ *README_EXTRA pendiente*
- [ ] T03 — Quicksort optimizaciones (cutoff, 3-way) ▸ *README_EXTRA pendiente*
- [ ] T04 — Heapsort (referencia C07)
- [ ] T05 — Shell sort (secuencias de gaps)
- [ ] T06 — Comparación (tabla: estabilidad, espacio, peor caso, cache)

**S04 — Lineales y funciones de biblioteca**
- [ ] T01 — Counting sort (O(n+k), estable)
- [ ] T02 — Radix sort (LSD/MSD, O(d·(n+k)))
- [ ] T03 — qsort en C (firma, comparador, void*)
- [ ] T04 — Ordenamiento en Rust (sort, sort_unstable, closures)
- [ ] T05 — C vs Rust (void* indirección vs monomorphization)

### C09 — Búsqueda `[A]`

**S01 — Búsqueda en secuencias**
- [ ] T01 — Búsqueda secuencial (O(n), sentinel, move-to-front)
- [ ] T02 — Búsqueda binaria (O(log n), lower/upper_bound) ▸ *README_EXTRA pendiente*
- [ ] T03 — Búsqueda por interpolación (O(log log n) uniformes)
- [ ] T04 — Búsqueda en Rust (binary_search, Result<usize,usize>)

**S02 — Búsqueda en árboles**
- [ ] T01 — BST (O(log n) promedio, O(n) peor)
- [ ] T02 — Balanceados AVL/RB (O(log n) garantizado)
- [ ] T03 — B-trees (nodos con múltiples claves, I/O optimizado)
- [ ] T04 — B+ trees (datos en hojas, enlazadas para recorrido secuencial)
- [ ] T05 — Tries revisitado (O(m) independiente de n, Patricia tries)

### C10 — Tablas hash `[A]`

**S01 — Fundamentos**
- [ ] T01 — Concepto (O(1) promedio, función hash, colisiones)
- [ ] T02 — Funciones hash (división, multiplicación, djb2, FNV-1a)
- [ ] T03 — Factor de carga (α = n/m, cuándo redimensionar) ▸ *README_EXTRA pendiente*
- [ ] T04 — Colisiones (inevitables: principio del palomar)

**S02 — Resolución de colisiones**
- [ ] T01 — Encadenamiento separado ▸ *README_EXTRA pendiente*
- [ ] T02 — Sonda lineal (clustering primario) ▸ *README_EXTRA pendiente*
- [ ] T03 — Sonda cuadrática y doble hashing
- [ ] T04 — Eliminación en open addressing (tombstones)
- [ ] T05 — Redimensionamiento ▸ *README_EXTRA pendiente*

**S03 — Hash tables en la práctica**
- [ ] T01 — HashMap en Rust (SwissTable/hashbrown, entry API)
- [ ] T02 — HashSet en Rust (unión, intersección, diferencia)
- [ ] T03 — Hash tables en C (GLib GHashTable, uthash)
- [ ] T04 — Hashing perfecto y universal (familias universales, DoS)
- [ ] T05 — Cuándo NO usar hash tables

### C11 — Grafos `[A]`

**S01 — Definición y representación**
- [ ] T01 — Terminología (vértices, aristas, dirigido, ponderado, ciclo)
- [ ] T02 — Matriz de adyacencia (O(1) arista, O(V²) espacio)
- [ ] T03 — Lista de adyacencia (O(V+E) espacio)
- [ ] T04 — Representación en Rust (Vec<Vec<usize>>, petgraph)
- [ ] T05 — Comparación (tabla: espacio, verificar arista, listar vecinos)

**S02 — Recorridos**
- [ ] T01 — BFS (cola, árbol BFS, distancias no ponderadas)
- [ ] T02 — DFS (pila/recursión, tiempos, clasificación de aristas)
- [ ] T03 — Aplicaciones (ciclos, componentes conexos, topológico)
- [ ] T04 — Componentes fuertemente conexos (Kosaraju o Tarjan)

**S03 — Caminos más cortos**
- [ ] T01 — Dijkstra (O((V+E) log V), sin pesos negativos) ▸ *README_EXTRA pendiente*
- [ ] T02 — Bellman-Ford (pesos negativos, ciclos negativos) ▸ *README_EXTRA pendiente*
- [ ] T03 — Floyd-Warshall (todos los pares, O(V³), DP)
- [ ] T04 — Comparación (tabla con restricciones y complejidades)

**S04 — Árboles generadores y topológico**
- [ ] T01 — MST (árbol generador mínimo, aplicaciones)
- [ ] T02 — Kruskal (Union-Find auxiliar) ▸ *README_EXTRA pendiente*
- [ ] T03 — Prim (cola de prioridad) ▸ *README_EXTRA pendiente*
- [ ] T04 — Ordenamiento topológico (Kahn BFS, DFS post-order, DAGs)

**S05 — Flujo en redes (opcional)**
- [ ] T01 — Redes de flujo (fuente, sumidero, capacidad, flujo máximo)
- [ ] T02 — Ford-Fulkerson (Edmonds-Karp BFS, O(V·E²))
- [ ] T03 — Aplicaciones (matching bipartito, corte mínimo)

### C12 — Administración de almacenamiento `[A]`

**S01 — Gestión de memoria dinámica**
- [ ] T01 — El heap del proceso (brk/sbrk, mmap, free list, metadata)
- [ ] T02 — Estrategias de asignación (first fit, best fit, worst fit, next fit)
- [ ] T03 — Fragmentación (externa vs interna, compactación)
- [ ] T04 — Buddy system (potencias de 2, dividir y fusionar)

**S02 — Arena y pool allocators**
- [ ] T01 — Arena allocator (asignación lineal, liberar todo de golpe)
- [ ] T02 — Pool allocator (bloques fijos, free-list, nodos de listas/árboles)
- [ ] T03 — Allocators en Rust (bumpalo, typed-arena, trait Allocator)

**S03 — Garbage collection**
- [ ] T01 — Conteo de referencias (ciclos como problema, Rc+Weak)
- [ ] T02 — Mark-and-sweep (fase marcado BFS/DFS, fase barrido)
- [ ] T03 — Generacional (young/old, weak generational hypothesis)
- [ ] T04 — Rust como GC en compilación (ownership, Drop determinista)

**S04 — Errores y herramientas**
- [ ] T01 — Memory leaks (patrones en listas, árboles, grafos)
- [ ] T02 — Use-after-free (dangling pointers, borrow checker prevención)
- [ ] T03 — Double free (corrupción del heap, Valgrind/ASan)
- [ ] T04 — Herramienta integrada (Valgrind, ASan, Miri — flujo de trabajo)

---

## README_EXTRA.md — Demostraciones Formales Pendientes

> 21 archivos con pruebas matemáticas rigurosas. Marcar con `[x]` conforme se creen.

### C01 — Fundamentos transversales (2)
- [x] `C01/.../T01_Notacion_O_Omega_Theta/README_EXTRA.md` — Probar 3n²+5n ∈ O(n²), n² ∉ O(n), Θ como intersección
- [x] `C01/.../T03_Analisis_amortizado/README_EXTRA.md` — Método del potencial (Φ = 2n − capacidad), O(1) amortizado

### C03 — Recursión (1)
- [x] `C03/.../T01_Definiciones_recursivas/README_EXTRA.md` — Inducción fuerte sobre factorial, fórmula cerrada Fibonacci

### C06 — Árboles (3)
- [x] `C06/.../T02_Insercion/README_EXTRA.md` — Invariante de bucle BST (∀ nodo: izq < nodo < der)
- [x] `C06/.../T02_Arboles_AVL/README_EXTRA.md` — h ≤ 1.44·log₂(n+2) con árboles de Fibonacci (N(h) ≥ φʰ)
- [x] `C06/.../T03_Arboles_rojinegros/README_EXTRA.md` — h ≤ 2·log₂(n+1) con argumento de black-height

### C07 — Montículos (2)
- [x] `C07/.../T04_Construccion_heapify/README_EXTRA.md` — Build-heap O(n): Σ ⌈n/2^(h+1)⌉·O(h) = O(n·Σ h/2ʰ)
- [x] `C07/.../T01_Heapsort/README_EXTRA.md` — Invariante de bucle del heapsort

### C08 — Ordenamiento (4)
- [x] `C08/.../T02_Limite_inferior/README_EXTRA.md` — Árbol de decisión: n! hojas → altura ≥ log₂(n!) = Ω(n log n)
- [x] `C08/.../T01_Merge_sort/README_EXTRA.md` — T(n) = 2T(n/2) + Θ(n) por Teorema Maestro y árbol de recursión
- [x] `C08/.../T02_Quicksort/README_EXTRA.md` — E[comparaciones] = 2n·Hₙ ≈ 1.39·n·log₂ n
- [x] `C08/.../T03_Quicksort_optimizaciones/README_EXTRA.md` — Quicksort aleatorizado O(n log n) independiente de la entrada

### C09 — Búsqueda (1)
- [x] `C09/.../T02_Busqueda_binaria/README_EXTRA.md` — Invariante (elemento en arr[lo..hi]) y terminación (hi-lo decrece)

### C10 — Tablas hash (4)
- [x] `C10/.../T03_Factor_de_carga/README_EXTRA.md` — E[longitud de cadena] = α por linealidad de la esperanza
- [x] `C10/.../T01_Encadenamiento_separado/README_EXTRA.md` — Búsqueda fallida Θ(1+α), exitosa Θ(1+α/2)
- [x] `C10/.../T02_Sonda_lineal/README_EXTRA.md` — Knuth: exitosa ½(1+1/(1-α)), fallida ½(1+1/(1-α)²)
- [x] `C10/.../T05_Redimensionamiento/README_EXTRA.md` — Rehash amortizado O(1): n inserciones + Σ 2ⁱ = O(n)

### C11 — Grafos (4)
- [x] `C11/.../T01_Dijkstra/README_EXTRA.md` — Correctitud por contradicción: al extraer u, dist[u] es mínima real
- [x] `C11/.../T02_Bellman_Ford/README_EXTRA.md` — Inducción: tras i iteraciones, dist[v] óptima con ≤ i aristas
- [x] `C11/.../T02_Kruskal/README_EXTRA.md` — Propiedad de corte → Kruskal produce un MST
- [x] `C11/.../T03_Prim/README_EXTRA.md` — Propiedad de corte → Prim produce un MST
