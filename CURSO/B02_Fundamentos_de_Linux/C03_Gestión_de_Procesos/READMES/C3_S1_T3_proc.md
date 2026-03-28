# T03 — /proc/[pid]

## Errata y notas sobre el material original

1. **labs paso 1.3 — comm máximo 16 caracteres**: Debería decir 15.
   `TASK_COMM_LEN` en el kernel es 16 bytes incluyendo el null
   terminator, así que son **15 caracteres visibles**. Los README.md
   y README.max.md lo dicen bien (15). El lab dice 16 incorrectamente.

2. **max.md ejercicio 7 — grep Rss/Pss**: `grep Rss` captura tanto
   `Rss:` como `SwapPss:`. Debe ser `grep '^Rss:'` (anclado al
   inicio). Igual para `grep '^Pss:'`.

3. **max.md ejercicio 9 — ls -la ns/ | awk**: La primera línea de
   `ls -la` es "total 0", que hace que awk imprima "0". Necesita
   `tail -n +2` o `ls` sin `-la`.

4. **max.md ejercicio 4 — clasificación de fds**: El script cuenta
   targets exactos (`sort | uniq -c`) en vez de clasificar por tipo
   (socket, pipe, archivo). Esto produce una lista de paths
   individuales poco útil. Mejor clasificar por tipo.

5. **Ambos README — ejemplo `systemctl show`**: No funciona en
   contenedores Docker (no hay systemd). Sustituido por alternativas
   que funcionan en ambos contextos.

---

## Qué es /proc/[pid]

Cada proceso en ejecución tiene un directorio en `/proc/` con su PID
como nombre. Contiene archivos virtuales que exponen el estado interno
del proceso directamente desde el kernel — no hay intermediarios ni
cachés:

```bash
# Ver tu propio proceso (shell actual)
ls /proc/$$/
# attr  cgroup  cmdline  comm  cwd  environ  exe  fd  limits
# maps  mem  mountinfo  net  ns  oom_adj  root  stat  status ...

# /proc/self siempre apunta al proceso que lee
ls -l /proc/self
# lrwxrwxrwx 1 dev dev ... /proc/self -> 1842
```

`ps`, `top` y `htop` leen estos archivos internamente. Aprender a
leerlos directamente permite obtener información que esas herramientas
no muestran.

### /proc/self vs /proc/$$

Ambos apuntan al proceso actual pero de forma diferente:

```bash
# /proc/self resuelve dinámicamente en CADA syscall
# En un pipe, cada comando es un proceso diferente:
readlink /proc/self    # PID del readlink (subproceso)
echo $$                # PID del shell (siempre fijo)

# Demostración:
readlink /proc/self    # → PID del readlink (efímero)
echo /proc/$$/comm     # → siempre apunta al shell

# Para inspeccionar el shell actual: usar /proc/$$
# Para inspeccionar "quien esté ejecutando esto": usar /proc/self
```

---

## Archivos fundamentales

### cmdline — Línea de comando

```bash
cat /proc/$$/cmdline | tr '\0' ' ' ; echo
# -bash
# o: /usr/bin/python3 /srv/app/main.py --port 8080

# Los argumentos están separados por NULL (\0), no por espacios
# Por eso se usa tr '\0' ' ' para hacerlo legible

# En ps, COMMAND muestra esto mismo formateado
```

Un proceso puede modificar su propia `cmdline` (algunos daemons lo
hacen para mostrar estado). No confiar en ella como dato de seguridad.

### comm — Nombre corto del comando

```bash
cat /proc/$$/comm
# bash

# Máximo 15 caracteres (TASK_COMM_LEN=16 incluyendo null)
# Es lo que muestra la columna COMM de ps
# A diferencia de cmdline, no incluye argumentos ni ruta
```

### status — Estado legible del proceso

```bash
cat /proc/$$/status
# Name:   bash
# Umask:  0022
# State:  S (sleeping)
# Tgid:   1506              ← Thread Group ID (= PID del proceso principal)
# Pid:    1506              ← PID (en threads, cada thread tiene su propio PID)
# PPid:   1505              ← Parent PID
# TracerPid: 0              ← PID del debugger (0 = no hay)
# Uid:    1000 1000 1000 1000   ← real, effective, saved, filesystem
# Gid:    1000 1000 1000 1000
# Groups: 27 100 999 1000       ← grupos suplementarios
# VmPeak: 12345 kB          ← máximo de memoria virtual alcanzado
# VmSize: 11200 kB          ← memoria virtual actual (≈ VSZ de ps)
# VmRSS:  6400 kB           ← memoria residente en RAM (≈ RSS de ps)
# VmSwap: 0 kB              ← memoria en swap
# Threads: 1                ← número de threads
# voluntary_ctxt_switches:  42    ← context switches voluntarios (sleep, I/O)
# nonvoluntary_ctxt_switches: 5  ← context switches forzados (preemption)
```

Es el archivo más útil para inspección rápida. Muestra los 4 UIDs y
GIDs (real, effective, saved, filesystem), la memoria en detalle, y
los context switches que ayudan a diagnosticar rendimiento:

- **voluntary alto, nonvoluntary bajo** → proceso I/O-bound (cede CPU
  esperando disco/red)
- **nonvoluntary alto** → proceso CPU-bound (el scheduler lo desaloja
  porque agota su timeslice)

### stat — Estado crudo (para parsing)

```bash
cat /proc/$$/stat
# 1506 (bash) S 1505 1506 1506 34816 1842 4194304 ...

# Es una línea con campos separados por espacios
# El formato está documentado en man proc(5)
# Los campos más relevantes:
# campo 1: PID
# campo 2: (comm) — entre paréntesis
# campo 3: estado (R/S/D/Z/T/I)
# campo 4: PPID
# campo 14: utime (tiempo en modo usuario, en clock ticks)
# campo 15: stime (tiempo en modo kernel)
# campo 20: num_threads
# campo 22: starttime (cuándo inició, en clock ticks desde boot)
```

`stat` es difícil de leer para humanos pero eficiente para herramientas
como `ps` y `top`. Usar `status` para inspección manual.

### environ — Variables de entorno

```bash
cat /proc/$$/environ | tr '\0' '\n'
# HOME=/home/dev
# SHELL=/bin/bash
# USER=dev
# PATH=/usr/local/bin:/usr/bin:/bin
# LANG=en_US.UTF-8
# ...

# Las variables están separadas por NULL (\0)
# Muestra el entorno del bloque de memoria original del proceso
# Si el proceso añadió variables con setenv() en memoria nueva,
# esos cambios pueden NO reflejarse aquí
```

Útil para debugging: verificar qué variables de entorno recibió un
servicio que no se comporta como se espera.

### exe — Enlace al ejecutable

```bash
ls -l /proc/$$/exe
# lrwxrwxrwx 1 dev dev ... /proc/1506/exe -> /usr/bin/bash

# Apunta al binario real en disco
# Si el binario fue eliminado o reemplazado:
# /proc/1506/exe -> /usr/bin/bash (deleted)
```

> `exe` apuntando a `(deleted)` indica que el ejecutable en disco fue
> actualizado pero el proceso sigue corriendo con la versión vieja.
> Común después de un `apt upgrade` — reiniciar el servicio para usar
> la versión nueva.

### cwd — Directorio de trabajo actual

```bash
ls -l /proc/$$/cwd
# lrwxrwxrwx 1 dev dev ... /proc/1506/cwd -> /home/dev/project

# Es el directorio actual del proceso (lo que pwd devolvería dentro de él)
```

### root — Raíz del filesystem del proceso

```bash
ls -l /proc/$$/root
# lrwxrwxrwx 1 dev dev ... /proc/1506/root -> /

# En un chroot o contenedor, apunta a la raíz del namespace
# del proceso, no necesariamente a / del host
```

---

## fd — File descriptors abiertos

```bash
ls -la /proc/$$/fd/
# lrwx------ 1 dev dev ... 0 -> /dev/pts/0    ← stdin
# lrwx------ 1 dev dev ... 1 -> /dev/pts/0    ← stdout
# lrwx------ 1 dev dev ... 2 -> /dev/pts/0    ← stderr
# lr-x------ 1 dev dev ... 3 -> /usr/share/bash-completion/bash_completion
# lrwx------ 1 dev dev ... 255 -> /dev/pts/0

# Cada número es un file descriptor
# Los enlaces muestran a qué archivo, socket, o pipe apuntan
```

### Contar file descriptors

```bash
# Cuántos fds tiene abiertos un proceso
ls /proc/$$/fd | wc -l

# Para un servicio como nginx o una base de datos, muchos fds es normal
# Si un proceso acumula fds sin cerrarlos → file descriptor leak

# Ver los límites de fds del proceso
grep "open files" /proc/$$/limits
# Max open files            1024                 1048576              files
#                           ^soft limit          ^hard limit
```

### Tipos de fds

Los file descriptors apuntan a diferentes tipos de recursos:

```bash
# Tipos comunes:
# /dev/pts/N            ← terminal (pseudo-terminal)
# /dev/null             ← archivo especial
# socket:[12345]        ← socket de red
# pipe:[67890]          ← pipe/FIFO entre procesos
# anon_inode:[eventpoll] ← epoll fd (I/O multiplexing)
# /var/log/syslog       ← archivo regular
```

---

## maps — Mapa de memoria

```bash
head -20 /proc/$$/maps
# 55a3c8400000-55a3c8420000 r--p 00000000 08:02 1234567  /usr/bin/bash
# 55a3c8420000-55a3c84f0000 r-xp 00020000 08:02 1234567  /usr/bin/bash
# 55a3c84f0000-55a3c8530000 r--p 000f0000 08:02 1234567  /usr/bin/bash
# 55a3c8530000-55a3c8534000 rw-p 00130000 08:02 1234567  /usr/bin/bash
# 55a3c9200000-55a3c9380000 rw-p 00000000 00:00 0        [heap]
# 7f1234000000-7f1234200000 r--p 00000000 08:02 2345678  /usr/lib/x86_64-linux-gnu/libc.so.6
# ...
# 7ffd12300000-7ffd12321000 rw-p 00000000 00:00 0        [stack]
# 7ffd123fe000-7ffd12400000 r--p 00000000 00:00 0        [vvar]
# 7ffd12400000-7ffd12402000 r-xp 00000000 00:00 0        [vdso]
```

Cada línea es una región de memoria:
```
dirección_inicio-dirección_fin  permisos  offset  dispositivo  inodo  ruta
```

| Permiso | Significado |
|---|---|
| r | Lectura |
| w | Escritura |
| x | Ejecución |
| p | Privado (copy-on-write) |
| s | Compartido |

Regiones especiales:
- `[heap]` — memoria dinámica (malloc/new)
- `[stack]` — pila del thread principal
- `[vdso]` — virtual dynamic shared object (syscalls rápidas sin
  cambio a kernel mode)
- `[vvar]` — variables del kernel accesibles sin syscall (hora, etc.)
- Archivos `.so` — bibliotecas compartidas cargadas

`maps` es esencial para debugging de memoria y análisis de seguridad
(verificar qué bibliotecas carga un proceso, si ASLR está activo, etc.).

### smaps_rollup — Resumen de memoria

```bash
cat /proc/$$/smaps_rollup
# Rss:            6400 kB      ← memoria residente total
# Pss:            4200 kB      ← proportional set size
# Shared_Clean:   3200 kB      ← compartida, no modificada
# Shared_Dirty:    800 kB      ← compartida, modificada
# Private_Clean:  1600 kB      ← privada, no modificada
# Private_Dirty:   800 kB      ← privada, modificada
# Swap:              0 kB
# SwapPss:           0 kB
```

**PSS** (Proportional Set Size) es la métrica más precisa de uso de
memoria: divide la memoria compartida proporcionalmente entre todos
los procesos que la usan. Si 3 procesos comparten 300KB de libc, cada
uno cuenta 100KB en PSS.

| Métrica | Qué mide | Cuándo usar |
|---|---|---|
| VIRT/VSZ | Todo lo mapeado (reservado, no necesariamente en RAM) | Casi nunca — muy inflado |
| RSS | Memoria en RAM (cuenta compartida al 100%) | Aproximación rápida |
| PSS | RSS con compartida dividida | Uso real por proceso |
| Private_Dirty | Solo memoria privada modificada | Lo que se perdería si el proceso muere |

---

## limits — Límites del proceso

```bash
cat /proc/$$/limits
# Limit                     Soft Limit  Hard Limit  Units
# Max cpu time              unlimited   unlimited   seconds
# Max file size             unlimited   unlimited   bytes
# Max data size             unlimited   unlimited   bytes
# Max stack size            8388608     unlimited   bytes
# Max core file size        0           unlimited   bytes
# Max resident set          unlimited   unlimited   bytes
# Max processes             63229       63229       processes
# Max open files            1024        1048576     files
# Max locked memory         8388608     8388608     bytes
# Max address space         unlimited   unlimited   bytes
# Max file locks            unlimited   unlimited   locks
# Max pending signals       63229       63229       signals
# Max msgqueue size         819200      819200      bytes
# Max nice priority         0           0
# Max realtime priority     0           0
# Max realtime timeout      unlimited   unlimited   us
```

- **Soft limit**: el límite efectivo actual (el proceso puede subirlo
  hasta el hard con `ulimit`)
- **Hard limit**: el máximo que un proceso sin privilegios puede
  establecer como soft
- Solo root puede subir hard limits

El límite más comúnmente ajustado es **Max open files** (nofile), que
por defecto es 1024 — insuficiente para servidores web y bases de datos.

```bash
# Equivalente con ulimit
ulimit -Sn   # soft limit de open files
ulimit -Hn   # hard limit de open files
```

---

## oom_score — OOM killer

Cuando el sistema se queda sin memoria, el kernel invoca el OOM
(Out of Memory) killer para matar procesos y liberar RAM.

```bash
# Score actual (0-1000, mayor = más probable de ser matado)
cat /proc/$$/oom_score

# Ajuste manual (-1000 a 1000)
cat /proc/$$/oom_score_adj
# -1000 = nunca matar (protegido)
#     0 = default
#  1000 = matar primero
```

Procesos con más memoria y sin ajuste especial tienen mayor oom_score.
Servicios críticos (como bases de datos) se protegen con
`oom_score_adj = -1000`.

---

## io — Estadísticas de I/O

```bash
cat /proc/$$/io
# rchar:     1234567     ← bytes leídos (incluye cache)
# wchar:      567890     ← bytes escritos (incluye cache)
# syscr:        1234     ← syscalls de lectura (read, pread)
# syscw:         567     ← syscalls de escritura (write, pwrite)
# read_bytes:  45678     ← bytes leídos del disco (bypass cache)
# write_bytes: 12345     ← bytes escritos al disco
# cancelled_write_bytes: 0

# rchar/wchar incluyen datos servidos desde cache
# read_bytes/write_bytes son los que realmente tocaron disco
```

> La diferencia entre `rchar` y `read_bytes` muestra cuánto se
> sirvió desde cache vs cuánto requirió acceso real a disco.

---

## cgroup — Grupo de control

```bash
cat /proc/$$/cgroup
# 0::/user.slice/user-1000.slice/session-1.scope

# En cgroups v2 (una sola línea con 0::)
# Muestra a qué cgroup pertenece el proceso
# Útil para verificar límites de recursos en contenedores
```

---

## ns — Namespaces

```bash
ls -la /proc/$$/ns/
# lrwxrwxrwx 1 dev dev ... cgroup -> 'cgroup:[4026531835]'
# lrwxrwxrwx 1 dev dev ... ipc -> 'ipc:[4026531839]'
# lrwxrwxrwx 1 dev dev ... mnt -> 'mnt:[4026531841]'
# lrwxrwxrwx 1 dev dev ... net -> 'net:[4026531840]'
# lrwxrwxrwx 1 dev dev ... pid -> 'pid:[4026531836]'
# lrwxrwxrwx 1 dev dev ... user -> 'user:[4026531837]'
# lrwxrwxrwx 1 dev dev ... uts -> 'uts:[4026531838]'

# El número entre corchetes es el ID del namespace (inode)
# Procesos en el mismo namespace comparten el mismo ID
# Procesos en contenedores tienen IDs diferentes al host
```

Comparar los namespace IDs de un proceso en un contenedor con los del
host permite verificar el aislamiento.

---

## mountinfo — Puntos de montaje visibles

```bash
head -5 /proc/$$/mountinfo
# 24 1 8:2 / / rw,relatime - ext4 /dev/sda2 rw
# 25 24 0:5 / /dev rw,nosuid - devtmpfs devtmpfs rw
# 26 24 0:23 / /proc rw,nosuid,nodev,noexec,relatime - proc proc rw
# 27 24 0:24 / /sys rw,nosuid,nodev,noexec,relatime - sysfs sysfs rw
```

En contenedores, cada proceso ve sus propios puntos de montaje (mount
namespace separado). Útil para diagnosticar problemas de montaje.

---

## Permisos de /proc/[pid]

No todos los procesos pueden leer la información de otros:

```bash
# Desde kernel 4.10+ con hidepid:
# mount -o remount,hidepid=2 /proc
# hidepid=0 → todos ven todos los procesos (default)
# hidepid=1 → ves PIDs de otros pero no su detalle
# hidepid=2 → solo ves tus propios procesos

# En la práctica, sin hidepid:
# - Puedes ver cmdline, status, maps de otros usuarios
# - Pero NO puedes leer environ, fd/, o mem de otros (requiere mismo UID o root)
```

---

## Tabla: archivos clave de /proc/[pid]

| Archivo | Contenido | Permiso otros |
|---|---|---|
| cmdline | Línea de comando completa | Sí |
| comm | Nombre corto del ejecutable (15 chars) | Sí |
| status | Estado legible (UIDs, memoria, threads) | Sí |
| stat | Estado crudo para parsing (campos numéricos) | Sí |
| environ | Variables de entorno | No (mismo UID o root) |
| exe | Symlink al binario | Sí |
| cwd | Symlink al directorio de trabajo | Sí |
| root | Symlink a la raíz del namespace | Sí |
| fd/ | File descriptors abiertos | No (mismo UID o root) |
| maps | Mapa de memoria (regiones, permisos, .so) | Sí |
| smaps_rollup | Resumen de memoria (RSS, PSS) | Sí |
| limits | Límites soft/hard del proceso | Sí |
| io | Estadísticas de lectura/escritura | No (mismo UID o root) |
| oom_score | Probabilidad de ser matado por OOM | Sí |
| oom_score_adj | Ajuste manual de OOM score | Sí (escribir: root) |
| cgroup | Cgroup del proceso | Sí |
| ns/ | IDs de namespaces | Sí |
| mountinfo | Puntos de montaje visibles | Sí |

---

## Patrones de uso práctico

### Debugging de un servicio

```bash
# Encontrar el PID del servicio (sin systemctl en contenedores)
PID=$(pgrep -x nginx | head -1)
# O: PID=$(pidof -s sshd)

# ¿Con qué comando se inició?
cat /proc/$PID/cmdline | tr '\0' ' '; echo

# ¿Qué directorio de trabajo tiene?
readlink /proc/$PID/cwd

# ¿Qué variables de entorno recibió?
cat /proc/$PID/environ | tr '\0' '\n' | sort

# ¿Cuántos file descriptors tiene abiertos?
ls /proc/$PID/fd | wc -l

# ¿Cuánta memoria usa realmente?
grep -E '^(VmRSS|VmSwap|Threads):' /proc/$PID/status

# ¿Está limitado en recursos?
grep "open files" /proc/$PID/limits

# ¿A qué cgroup pertenece?
cat /proc/$PID/cgroup
```

### Encontrar file descriptor leaks

```bash
# Monitorear el crecimiento de fds de un proceso
PID=1234
while true; do
    echo "$(date +%H:%M:%S) fds: $(ls /proc/$PID/fd 2>/dev/null | wc -l)"
    sleep 10
done

# Si el número crece continuamente, el proceso tiene un fd leak
```

### Ver qué archivos tiene abiertos un proceso

```bash
# Los fds son symlinks al recurso real
ls -la /proc/$PID/fd/ | grep -v '/dev/\|socket:\|pipe:'
# Muestra solo archivos regulares abiertos

# Alternativa más completa: lsof
lsof -p $PID
```

### Clasificar fds por tipo

```bash
# Contar fds por tipo
for fd in /proc/$PID/fd/*; do
    readlink "$fd" 2>/dev/null
done | sed 's|.*socket:.*|socket|; s|.*pipe:.*|pipe|;
    s|/dev/.*|device|; s|anon_inode:.*|anon_inode|;
    s|^/.*|file|' | sort | uniq -c | sort -rn
```

---

## Labs

### Parte 1 — Archivos básicos de /proc/[pid]

#### 1.1 Qué hay en /proc/self

```bash
echo "=== /proc/self es un enlace al proceso actual ==="
echo "PID del shell: $$"
ls /proc/self/ | head -20
echo "..."
echo "Total de entradas: $(ls /proc/self/ | wc -l)"
```

`/proc/self` es un enlace mágico que siempre apunta al proceso que lo
lee. Cada entrada es un archivo virtual generado por el kernel al
momento de la lectura.

#### 1.2 status — información completa

```bash
echo "=== Campos clave de status ==="
echo ""
echo "--- Identidad ---"
grep -E '^(Name|Pid|PPid|Tgid|Uid|Gid):' /proc/$$/status
echo ""
echo "--- Memoria ---"
grep -E '^(VmPeak|VmSize|VmRSS|VmData|VmStk|VmLib):' /proc/$$/status
echo ""
echo "--- Threads y contexto ---"
grep -E '^(Threads|voluntary_ctxt|nonvoluntary_ctxt):' /proc/$$/status
```

Context switches voluntarios altos → proceso I/O-bound (cede CPU).
Involuntarios altos → proceso CPU-bound (el scheduler lo desaloja).

#### 1.3 cmdline, comm y environ

```bash
echo "=== cmdline ==="
cat /proc/$$/cmdline | tr '\0' ' '
echo ""
echo "(argumentos separados por null bytes)"

echo ""
echo "=== comm ==="
cat /proc/$$/comm
echo "(solo nombre, máximo 15 caracteres, sin ruta ni argumentos)"

echo ""
echo "=== environ (primeras 5 variables) ==="
cat /proc/$$/environ | tr '\0' '\n' | head -5
```

#### 1.4 stat — datos crudos

```bash
echo "=== stat (una línea con todos los datos) ==="
cat /proc/$$/stat
echo ""
echo "Campos: 1=PID, 2=(comm), 3=estado, 4=PPID,"
echo "14=utime, 15=stime, 20=num_threads, 22=starttime"
echo "Para humanos, usar status. stat es para herramientas."
```

---

### Parte 2 — File descriptors y memoria

#### 2.1 File descriptors (fd/)

```bash
echo "=== File descriptors del shell ==="
ls -la /proc/$$/fd/
echo ""
echo "0=stdin, 1=stdout, 2=stderr"
echo "Los demás son archivos, sockets, pipes abiertos"
```

#### 2.2 Abrir fds y observar

```bash
echo "=== Antes de abrir fds ==="
echo "fds: $(ls /proc/$$/fd | wc -l)"

exec 3>/tmp/test-fd.txt
exec 4</etc/passwd

echo ""
echo "=== Después de abrir 2 fds ==="
ls -la /proc/$$/fd/
echo ""
echo "fd 3 → /tmp/test-fd.txt (escritura)"
echo "fd 4 → /etc/passwd (lectura)"

exec 3>&-
exec 4<&-
rm -f /tmp/test-fd.txt
```

#### 2.3 Límites

```bash
echo "=== Límite de archivos abiertos ==="
grep "open files" /proc/$$/limits
echo ""
echo "=== Equivalente con ulimit ==="
echo "Soft: $(ulimit -Sn)"
echo "Hard: $(ulimit -Hn)"
```

#### 2.4 Mapa de memoria

```bash
echo "=== Regiones clave en maps ==="
echo "--- Código ejecutable (r-xp) ---"
grep 'r-xp' /proc/$$/maps | head -3
echo ""
echo "--- Heap ---"
grep '\[heap\]' /proc/$$/maps
echo ""
echo "--- Stack ---"
grep '\[stack\]' /proc/$$/maps
echo ""
echo "--- Bibliotecas compartidas ---"
grep '\.so' /proc/$$/maps | awk '{print $6}' | sort -u
```

#### 2.5 Resumen de memoria (smaps_rollup)

```bash
echo "=== smaps_rollup ==="
cat /proc/$$/smaps_rollup 2>/dev/null || echo "(no disponible)"
echo ""
echo "PSS es mejor que RSS para medir uso real:"
echo "divide memoria compartida proporcionalmente entre procesos"
```

---

### Parte 3 — Enlaces y metadatos

#### 3.1 exe, cwd, root

```bash
echo "=== exe (ejecutable) ==="
readlink /proc/$$/exe

echo ""
echo "=== cwd (directorio de trabajo) ==="
readlink /proc/$$/cwd

echo ""
echo "=== root (raíz del filesystem) ==="
readlink /proc/$$/root
echo "(en contenedores, puede no ser / del host)"
```

#### 3.2 Inspeccionar PID 1

```bash
echo "=== PID 1 ==="
echo "Ejecutable: $(readlink /proc/1/exe 2>/dev/null || echo 'sin permiso')"
echo "CWD:        $(readlink /proc/1/cwd 2>/dev/null || echo 'sin permiso')"
echo "Comando:    $(cat /proc/1/cmdline 2>/dev/null | tr '\0' ' ')"
echo ""
grep -E '^(Name|Pid|PPid|Uid|Threads):' /proc/1/status 2>/dev/null
```

#### 3.3 Namespaces

```bash
echo "=== Namespaces del shell ==="
ls -la /proc/$$/ns/ | tail -n +2 | awk '{print $NF}'
echo ""
echo "=== Namespaces de PID 1 ==="
ls -la /proc/1/ns/ 2>/dev/null | tail -n +2 | awk '{print $NF}'
echo ""
echo "Si los inode IDs son iguales, comparten ese namespace"
```

#### 3.4 cgroup

```bash
echo "=== cgroup del shell ==="
cat /proc/$$/cgroup
echo ""
echo "=== cgroup de PID 1 ==="
cat /proc/1/cgroup 2>/dev/null
```

---

## Ejercicios

### Ejercicio 1 — Inspeccionar tu shell

```bash
echo "PID: $$"
cat /proc/$$/status | head -20
echo ""
grep '^Uid:' /proc/$$/status
echo "      real  effective  saved  filesystem"
echo ""
echo "fds abiertos: $(ls /proc/$$/fd | wc -l)"
echo ""
grep "open files" /proc/$$/limits
```

Responder:
1. ¿Los 4 UIDs son iguales? ¿Por qué?
2. ¿Cuántos fds tiene abiertos tu shell?
3. ¿Cuál es el soft limit de open files?

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

1. Los 4 UIDs (real, effective, saved, filesystem) deberían ser
   iguales para un shell normal. Solo difieren cuando hay SUID
   (effective != real) o cuando un programa guardó un UID previo
   (saved). Filesystem UID normalmente coincide con effective.

2. Un shell bash típico tiene ~4-5 fds: 0 (stdin), 1 (stdout),
   2 (stderr), y alguno más para bash-completion o el script history.

3. El soft limit de open files es típicamente 1024. Para un shell
   interactivo es suficiente, pero servidores necesitan más
   (ajustable con `ulimit -Sn VALOR` hasta el hard limit).

</details>

---

### Ejercicio 2 — ps lee /proc: verificarlo

```bash
PID=$$

echo "=== Comparar cmdline ==="
echo "/proc:  $(cat /proc/$PID/cmdline | tr '\0' ' ')"
echo "ps:     $(ps -p $PID -o cmd=)"

echo ""
echo "=== Comparar estado ==="
echo "/proc:  $(grep '^State:' /proc/$PID/status)"
echo "ps:     $(ps -p $PID -o stat=)"

echo ""
echo "=== Comparar memoria (RSS en kB) ==="
echo "/proc:  $(grep '^VmRSS:' /proc/$PID/status)"
echo "ps:     RSS=$(ps -p $PID -o rss=) kB"

echo ""
echo "=== Comparar PPID ==="
echo "/proc:  $(grep '^PPid:' /proc/$PID/status)"
echo "ps:     PPID=$(ps -p $PID -o ppid=)"
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

Los valores deben coincidir porque `ps` lee de `/proc`:

- **cmdline**: Idéntico. `ps -o cmd` formatea, pero el contenido es
  el mismo de `/proc/PID/cmdline`.

- **Estado**: `/proc` muestra `S (sleeping)`, `ps` muestra `Ss`
  (S=sleeping, s=session leader). Los modificadores de ps (`s`, `l`,
  `+`, etc.) vienen de otros campos de `/proc/PID/stat`.

- **RSS**: Pueden diferir ligeramente si se leen en momentos diferentes
  (el kernel actualiza estos valores en tiempo real). La unidad es kB.

- **PPID**: Idéntico. Viene del campo 4 de `/proc/PID/stat` o de
  `PPid:` en `/proc/PID/status`.

**Punto clave**: `ps` no tiene información propia — todo lo saca de
`/proc`. Es solo un formateador.

</details>

---

### Ejercicio 3 — File descriptors: abrir, contar, clasificar

```bash
echo "=== fds iniciales ==="
ls /proc/$$/fd | wc -l

# Abrir algunos fds
exec 3>/tmp/test_fd_a.txt
exec 4>/tmp/test_fd_b.txt
exec 5</etc/hostname

echo ""
echo "=== fds después de abrir 3 nuevos ==="
ls -la /proc/$$/fd/
echo ""
echo "=== Clasificar por tipo ==="
for fd in /proc/$$/fd/*; do
    target=$(readlink "$fd" 2>/dev/null)
    case "$target" in
        socket:*)      echo "socket" ;;
        pipe:*)        echo "pipe" ;;
        /dev/*)        echo "device" ;;
        anon_inode:*)  echo "anon_inode" ;;
        /*)            echo "file" ;;
        *)             echo "other" ;;
    esac
done | sort | uniq -c | sort -rn

# Limpiar
exec 3>&- 4>&- 5<&-
rm -f /tmp/test_fd_a.txt /tmp/test_fd_b.txt
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

Antes de abrir: ~4-5 fds (stdin/stdout/stderr + bash internals).

Después de abrir 3: los fds 3, 4, 5 aparecen como nuevos symlinks.
- fd 3 → /tmp/test_fd_a.txt (file)
- fd 4 → /tmp/test_fd_b.txt (file)
- fd 5 → /etc/hostname (file)

La clasificación mostrará:
- `device`: 2-3 (los pts de stdin/stdout/stderr)
- `file`: 3+ (los que acabamos de abrir + bash-completion si existe)
- `pipe`: 0 (a menos que haya un pipe abierto)

**Nota**: Los números de fd se asignan secuencialmente: el kernel
siempre usa el número más bajo disponible. Por eso 3, 4, 5 (0, 1, 2
ya están ocupados por stdin/stdout/stderr).

</details>

---

### Ejercicio 4 — Mapa de memoria y bibliotecas

```bash
echo "=== Bibliotecas compartidas cargadas ==="
grep '\.so' /proc/$$/maps | awk '{print $6}' | sort -u
echo ""

echo "=== Regiones ejecutables (código) ==="
grep ' r-xp ' /proc/$$/maps | head -5
echo ""

echo "=== Heap y stack ==="
grep -E '\[(heap|stack)\]' /proc/$$/maps
echo ""

echo "=== Contar regiones por tipo de permiso ==="
awk '{print $2}' /proc/$$/maps | sort | uniq -c | sort -rn
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

**Bibliotecas**: Las .so cargadas por bash. Como mínimo:
- `libc.so.6` — biblioteca C estándar
- `ld-linux-x86-64.so.2` — linker dinámico
- `libtinfo.so` o `libncurses.so` — para la terminal
- `libdl.so`, `libpthread.so` (a veces integrados en libc moderna)

**Regiones ejecutables (r-xp)**: Código del binario y de cada .so.
Solo estas regiones pueden ejecutar instrucciones. Si una región es
`rwxp` (lectura+escritura+ejecución), es un riesgo de seguridad
(permite inyección de código).

**Heap**: Una sola región `rw-p` etiquetada `[heap]`. Crece con
`malloc()`.

**Stack**: Una sola región `rw-p` etiquetada `[stack]`. Crece hacia
direcciones bajas.

**Conteo de permisos**: `r--p` (datos de solo lectura) y `rw-p`
(datos modificables) serán los más comunes, seguidos de `r-xp` (código).

</details>

---

### Ejercicio 5 — PSS vs RSS

```bash
echo "=== RSS y PSS del shell ==="
grep '^Rss:' /proc/$$/smaps_rollup 2>/dev/null
grep '^Pss:' /proc/$$/smaps_rollup 2>/dev/null
echo ""

echo "=== Desglose de memoria compartida vs privada ==="
grep -E '^(Shared_Clean|Shared_Dirty|Private_Clean|Private_Dirty):' \
    /proc/$$/smaps_rollup 2>/dev/null
echo ""

echo "=== RSS desde status (para comparar) ==="
grep '^VmRSS:' /proc/$$/status
```

Responder:
1. ¿PSS es menor que RSS? ¿Por qué?
2. ¿Cuánta memoria es Shared vs Private?
3. ¿Cuánta es Dirty (modificada)?

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

**PSS < RSS**: Sí, porque RSS cuenta la memoria compartida (como libc)
al 100% para cada proceso. PSS la divide proporcionalmente. Si 10
procesos comparten 1000 kB de libc, cada uno cuenta 1000 kB en RSS
pero solo 100 kB en PSS.

**Shared vs Private**: La mayor parte de la memoria compartida es
`Shared_Clean` (código de .so cargadas, solo lectura). Private_Dirty
es la memoria que el proceso ha asignado y modificado (sus variables,
heap, stack).

**Dirty**: `Private_Dirty` es la memoria más "propia" del proceso —
si el proceso muere, esta memoria se pierde. `Shared_Dirty` es rara
(memoria compartida que el proceso modificó via copy-on-write).

**La métrica que importa varía según la pregunta:**
- "¿Cuánta RAM usa este proceso?" → PSS
- "¿Cuánta RAM se libera si lo mato?" → Private_Dirty + Private_Clean
- "¿Cuánta RAM usan todos estos procesos combinados?" → sumar PSS

</details>

---

### Ejercicio 6 — environ: qué vio el proceso al nacer

```bash
# Crear un proceso con un entorno específico
MY_SECRET=hunter2 CUSTOM_VAR=hello bash -c '
    echo "=== environ del nuevo bash ==="
    cat /proc/self/environ | tr "\0" "\n" | grep -E "^(MY_SECRET|CUSTOM_VAR|PATH)="
    echo ""

    echo "=== Ahora modificamos una variable ==="
    export CUSTOM_VAR=modified
    export NEW_VAR=added
    echo "Shell dice: CUSTOM_VAR=$CUSTOM_VAR, NEW_VAR=$NEW_VAR"
    echo ""

    echo "=== ¿environ refleja los cambios? ==="
    cat /proc/self/environ | tr "\0" "\n" | grep -E "^(CUSTOM_VAR|NEW_VAR)="
    echo "(NEW_VAR probablemente no aparece — fue añadida en memoria nueva)"
'
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

Al inicio, `environ` muestra `MY_SECRET=hunter2`, `CUSTOM_VAR=hello`,
y `PATH=...` — todas las variables con las que se invocó el proceso.

Después de `export CUSTOM_VAR=modified`:
- Si la nueva cadena cabe en el espacio original, `/proc/self/environ`
  **podría** mostrar el valor modificado (misma dirección de memoria).
- Si no cabe, `setenv()` asigna memoria nueva fuera del bloque
  original y `/proc/self/environ` muestra el valor viejo.

`NEW_VAR=added` **no aparecerá** en environ porque fue añadida con
`export` (que internamente llama `setenv`), que asigna memoria fuera
del bloque original que el kernel rastrea.

**Implicación de seguridad**: `/proc/PID/environ` puede exponer
secretos (contraseñas, tokens) pasados como variables de entorno.
Requiere mismo UID o root para leerlo (no es accesible a otros
usuarios), pero es una razón para preferir archivos o secret stores
sobre variables de entorno para datos sensibles.

</details>

---

### Ejercicio 7 — Context switches: CPU-bound vs I/O-bound

```bash
echo "=== Shell inactivo ==="
grep -E 'ctxt_switches' /proc/$$/status
echo ""

echo "=== Proceso CPU-bound ==="
bash -c '
    for i in $(seq 1 1000000); do :; done
    grep -E "ctxt_switches" /proc/self/status
'
echo ""

echo "=== Proceso I/O-bound ==="
bash -c '
    for i in $(seq 1 100); do cat /etc/hostname > /dev/null; done
    grep -E "ctxt_switches" /proc/self/status
'
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

**Shell inactivo**: Muchos voluntary (cada vez que espera input del
teclado, el shell cede CPU voluntariamente). Pocos nonvoluntary.

**CPU-bound** (loop vacío): nonvoluntary_ctxt_switches alto — el
proceso nunca cede CPU voluntariamente, así que el scheduler lo
desaloja cuando agota su timeslice (~4ms en CFS). Voluntary bajo.

**I/O-bound** (leer archivos): voluntary_ctxt_switches alto — cada
`cat` hace syscalls de lectura que pueden bloquear brevemente
(especialmente si los datos no están en cache). Nonvoluntary bajo
(el proceso no consume mucho CPU entre I/Os).

**Regla de diagnóstico:**

| Situación | voluntary | nonvoluntary | Diagnóstico |
|---|---|---|---|
| Shell esperando input | Alto | Bajo | Normal (idle) |
| Bucle infinito | Bajo | Alto | CPU-bound |
| Lectura de archivos | Alto | Bajo | I/O-bound |
| Servidor web | Ambos moderados | Ambos moderados | Mixto |

</details>

---

### Ejercicio 8 — oom_score: quién mata el OOM killer

```bash
echo "=== OOM score de procesos ==="
for pid in /proc/[0-9]*/; do
    pid=${pid#/proc/}
    pid=${pid%/}
    score=$(cat /proc/$pid/oom_score 2>/dev/null) || continue
    adj=$(cat /proc/$pid/oom_score_adj 2>/dev/null) || continue
    comm=$(cat /proc/$pid/comm 2>/dev/null) || continue
    echo "$score $adj $comm"
done | sort -rn | head -10
echo ""
echo "(columnas: oom_score  oom_score_adj  comando)"
echo "Mayor oom_score = más probable de ser matado"
echo ""

echo "=== Tu shell ==="
echo "oom_score:     $(cat /proc/$$/oom_score)"
echo "oom_score_adj: $(cat /proc/$$/oom_score_adj)"
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

Los procesos con más memoria residente tendrán oom_score más alto.
En un contenedor simple, bash y los pocos procesos corriendo tendrán
scores bajos (< 100).

oom_score_adj de 0 es el default. Procesos del sistema críticos
(como PID 1) pueden tener -1000 (nunca matar). En Kubernetes, el
kubelet ajusta oom_score_adj según la QoS class del pod:
- Guaranteed: -997
- BestEffort: 1000
- Burstable: intermedio

**Cómo calcula el kernel oom_score:**
- Base = (RSS del proceso / RAM total) × 1000
- Ajustado por oom_score_adj
- El proceso con mayor score final es el elegido para matar
- oom_score_adj = -1000 lo protege absolutamente

</details>

---

### Ejercicio 9 — I/O por proceso

```bash
echo "=== I/O de tu shell ==="
cat /proc/$$/io 2>/dev/null || echo "(requiere permisos)"
echo ""

echo "=== Generar I/O y comparar ==="
# Capturar antes
BEFORE=$(grep '^write_bytes:' /proc/$$/io 2>/dev/null | awk '{print $2}')

# Escribir datos
dd if=/dev/zero of=/tmp/test_io_proc bs=1K count=100 2>/dev/null

# Capturar después
AFTER=$(grep '^write_bytes:' /proc/$$/io 2>/dev/null | awk '{print $2}')

echo "write_bytes antes: $BEFORE"
echo "write_bytes después: $AFTER"
echo "Diferencia: $(( AFTER - BEFORE )) bytes escritos al disco"

rm -f /tmp/test_io_proc
echo ""
echo "Nota: rchar/wchar incluyen datos desde/hacia cache."
echo "      read_bytes/write_bytes son solo acceso real a disco."
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

**Antes del dd**: `write_bytes` será bajo (el shell no ha escrito
mucho a disco). `rchar` será más alto (ha leído ~/.bashrc, scripts,
etc., mayoritariamente desde cache).

**Después del dd**: `write_bytes` sube ~100 KB (100 × 1K).
`wchar` sube también. La diferencia muestra cuánto fue al disco.

**rchar vs read_bytes**: Si `rchar` >> `read_bytes`, la mayoría de
las lecturas se sirvieron desde cache del kernel (bueno — el disco
no fue tocado).

**Uso práctico**: Si un proceso tiene `write_bytes` creciendo
rápidamente, está escribiendo mucho a disco (puede saturar I/O).
Combinado con `wa` alto en `top`, confirma un cuello de botella
en disco.

</details>

---

### Ejercicio 10 — Namespaces y aislamiento

```bash
echo "=== Namespaces de tu shell ==="
ls -la /proc/$$/ns/ | tail -n +2 | awk '{print $NF}'
echo ""

echo "=== Namespaces de PID 1 ==="
ls -la /proc/1/ns/ 2>/dev/null | tail -n +2 | awk '{print $NF}'
echo ""

echo "=== ¿Comparten los mismos namespaces? ==="
SELF_PID_NS=$(readlink /proc/$$/ns/pid)
PID1_PID_NS=$(readlink /proc/1/ns/pid 2>/dev/null)
echo "Shell pid_ns: $SELF_PID_NS"
echo "PID 1 pid_ns: $PID1_PID_NS"

if [ "$SELF_PID_NS" = "$PID1_PID_NS" ]; then
    echo "→ Mismo PID namespace (estás en el mismo contexto que PID 1)"
else
    echo "→ PID namespace diferente (estás aislado de PID 1)"
fi

echo ""
echo "=== ¿Estamos en un contenedor? ==="
if [ -f /proc/1/cgroup ]; then
    echo "cgroup de PID 1: $(cat /proc/1/cgroup 2>/dev/null)"
fi
if [ -f /.dockerenv ]; then
    echo "→ Detectado: archivo /.dockerenv presente (es un contenedor Docker)"
else
    echo "→ No se detectó /.dockerenv"
fi
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

**Dentro de un contenedor Docker**: Tu shell y PID 1 comparten los
mismos namespace IDs (están en el mismo contenedor). Pero estos IDs
son **diferentes** a los del host.

Los namespaces aislados en un contenedor típico:
- **pid**: PID 1 dentro del contenedor ≠ PID 1 del host
- **mnt**: Sistema de archivos propio (overlay)
- **net**: Red propia (veth)
- **uts**: Hostname propio
- **ipc**: Memoria compartida y semáforos aislados

Pueden estar compartidos:
- **user**: A menudo compartido (contenedores sin user namespaces)
- **cgroup**: Puede estar compartido o no

**Detectar contenedor**: `/.dockerenv` existe solo en contenedores
Docker. Otra forma: verificar si el cgroup de PID 1 contiene "docker"
o "containerd" en su ruta.

**En el host**: Los namespace IDs del shell y de PID 1 (systemd)
coinciden porque ambos están en los namespaces raíz del kernel.

</details>
