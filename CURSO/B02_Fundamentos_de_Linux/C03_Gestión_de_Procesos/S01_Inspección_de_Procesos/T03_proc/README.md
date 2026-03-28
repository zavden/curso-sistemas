# T03 — /proc/[pid]

## Qué es /proc/[pid]

Cada proceso en ejecución tiene un directorio en `/proc/` con su PID como
nombre. Este directorio contiene archivos virtuales que exponen el estado
interno del proceso directamente desde el kernel — no hay intermediarios ni
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

`ps`, `top` y `htop` leen estos archivos internamente. Aprender a leerlos
directamente permite obtener información que esas herramientas no muestran.

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

Un proceso puede modificar su propia `cmdline` (algunos daemons lo hacen para
mostrar estado). No confiar en ella como dato de seguridad.

### comm — Nombre corto del comando

```bash
cat /proc/$$/comm
# bash

# Máximo 15 caracteres. Es lo que muestra la columna COMM de ps
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
# VmSize: 11200 kB          ← memoria virtual actual
# VmRSS:  6400 kB           ← memoria residente (en RAM)
# VmSwap: 0 kB              ← memoria en swap
# Threads: 1                ← número de threads
# voluntary_ctxt_switches:  42    ← context switches voluntarios (sleep, I/O)
# nonvoluntary_ctxt_switches: 5  ← context switches forzados (preemption)
```

Es el archivo más útil para inspección rápida. Muestra los 4 UIDs y GIDs
(real, effective, saved, filesystem), la memoria en detalle, y los context
switches que ayudan a diagnosticar rendimiento.

### stat — Estado crudo (para parsing)

```bash
cat /proc/$$/stat
# 1506 (bash) S 1505 1506 1506 34816 1842 4194304 ...

# Es una línea con campos separados por espacios
# El formato está documentado en man proc(5)
# Los campos más relevantes:
# campo 1: PID
# campo 2: (comm)
# campo 3: estado (R/S/D/Z/T)
# campo 4: PPID
# campo 14: utime (tiempo en modo usuario, en jiffies)
# campo 15: stime (tiempo en modo kernel)
# campo 20: num_threads
# campo 22: starttime (cuándo inició, en jiffies desde boot)
```

`stat` es difícil de leer para humanos pero eficiente para herramientas como
`ps` y `top`. Usar `status` para inspección manual.

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
# Muestra el entorno EXACTO con el que se inició el proceso
# Si el proceso modificó su entorno después de iniciar,
# los cambios NO se reflejan aquí
```

Útil para debugging: verificar qué variables de entorno recibió un servicio
que no se comporta como se espera.

### exe — Enlace al ejecutable

```bash
ls -l /proc/$$/exe
# lrwxrwxrwx 1 dev dev ... /proc/1506/exe -> /usr/bin/bash

# Apunta al binario real en disco
# Si el binario fue eliminado o reemplazado:
# /proc/1506/exe -> /usr/bin/bash (deleted)
```

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
cat /proc/$$/limits | grep "open files"
# Max open files            1024                 1048576              files
#                           ^soft limit          ^hard limit
```

### Tipos de fds

```bash
ls -la /proc/$(pgrep -x sshd | head -1)/fd/ 2>/dev/null
# 0 -> /dev/null                 ← archivo especial
# 3 -> socket:[12345]            ← socket de red
# 4 -> /var/log/auth.log         ← archivo regular
# 5 -> pipe:[67890]              ← pipe/FIFO
# 6 -> anon_inode:[eventpoll]    ← epoll fd
```

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
- `[vdso]` — virtual dynamic shared object (syscalls rápidas)
- Archivos `.so` — bibliotecas compartidas cargadas

`maps` es esencial para debugging de memoria y análisis de seguridad (verificar
qué bibliotecas carga un proceso, si ASLR está activo, etc.).

### smaps_rollup — Resumen de memoria

```bash
cat /proc/$$/smaps_rollup
# Rss:            6400 kB      ← memoria residente total
# Pss:            4200 kB      ← proportional set size (compartida dividida)
# Shared_Clean:   3200 kB
# Shared_Dirty:    800 kB
# Private_Clean:  1600 kB
# Private_Dirty:   800 kB
# Swap:              0 kB
# SwapPss:           0 kB
```

**PSS** (Proportional Set Size) es la métrica más precisa de uso de memoria:
divide la memoria compartida proporcionalmente entre todos los procesos que la
usan. Si 3 procesos comparten 300KB de libc, cada uno cuenta 100KB en PSS.

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

- **Soft limit**: el límite efectivo actual (el proceso puede subirlo hasta el hard)
- **Hard limit**: el máximo que un proceso sin privilegios puede establecer como soft
- Solo root puede subir hard limits

El límite más comúnmente ajustado es **Max open files** (nofile), que por
defecto es 1024 — insuficiente para servidores web y bases de datos.

## cgroup — Grupo de control

```bash
cat /proc/$$/cgroup
# 0::/user.slice/user-1000.slice/session-1.scope

# En cgroups v2 (una sola línea con 0::)
# Muestra a qué cgroup pertenece el proceso
# Útil para verificar límites de recursos en contenedores
```

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

# El número entre corchetes es el ID del namespace
# Procesos en el mismo namespace comparten el mismo ID
# Procesos en contenedores tienen IDs diferentes al host
```

Comparar los namespace IDs de un proceso en un contenedor con los del host
permite verificar el aislamiento.

## mountinfo — Puntos de montaje visibles

```bash
head -5 /proc/$$/mountinfo
# 24 1 8:2 / / rw,relatime - ext4 /dev/sda2 rw
# 25 24 0:5 / /dev rw,nosuid - devtmpfs devtmpfs rw
# 26 24 0:23 / /proc rw,nosuid,nodev,noexec,relatime - proc proc rw
# 27 24 0:24 / /sys rw,nosuid,nodev,noexec,relatime - sysfs sysfs rw
```

En contenedores, cada proceso ve sus propios puntos de montaje (mount namespace
separado). Útil para diagnosticar problemas de montaje dentro de contenedores.

## Patrones de uso práctico

### Debugging de un servicio

```bash
PID=$(systemctl show -p MainPID sshd | cut -d= -f2)

# ¿Con qué comando se inició?
cat /proc/$PID/cmdline | tr '\0' ' '; echo

# ¿Qué directorio de trabajo tiene?
ls -l /proc/$PID/cwd

# ¿Qué variables de entorno recibió?
cat /proc/$PID/environ | tr '\0' '\n' | sort

# ¿Cuántos file descriptors tiene abiertos?
ls /proc/$PID/fd | wc -l

# ¿Cuánta memoria usa realmente?
grep -E 'VmRSS|VmSwap' /proc/$PID/status

# ¿Está limitado en recursos?
cat /proc/$PID/limits

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

### Comparar procesos en host vs contenedor

```bash
# En el host: ver namespaces de un proceso del contenedor
CPID=$(docker inspect --format '{{.State.Pid}}' mi-container)
ls -la /proc/$CPID/ns/

# Comparar con un proceso del host
ls -la /proc/$$/ns/

# Si los IDs del namespace son diferentes, están aislados
```

## Permisos de /proc/[pid]

No todos los procesos pueden leer la información de otros:

```bash
# Desde kernel 4.10+ con hidepid:
# mount -o remount,hidepid=2 /proc
# hidepid=0 → todos ven todos los procesos (default)
# hidepid=1 → solo ves tus propios procesos en detalle
# hidepid=2 → solo ves tus propios procesos
# hidepid=invisible → igual que 2 pero con excepciones por grupo

# En la práctica, sin hidepid:
# - Puedes ver cmdline, status, maps de otros usuarios
# - Pero NO puedes leer environ, fd/, o mem de otros (requiere permisos)
```

---

## Ejercicios

### Ejercicio 1 — Inspeccionar tu shell

```bash
# PID de tu shell
echo $$

# Leer status completo
cat /proc/$$/status

# ¿Cuáles son tus 4 UIDs? (real, effective, saved, filesystem)
grep Uid /proc/$$/status

# ¿Cuántos file descriptors tienes abiertos?
ls /proc/$$/fd | wc -l

# ¿Cuál es tu límite de archivos abiertos?
grep "open files" /proc/$$/limits
```

### Ejercicio 2 — Comparar ps con /proc

```bash
# Obtener el PID de bash
PID=$$

# ¿Coinciden?
echo "cmdline: $(cat /proc/$PID/cmdline | tr '\0' ' ')"
ps -p $PID -o cmd=

echo "estado: $(grep State /proc/$PID/status)"
ps -p $PID -o stat=

echo "memoria: $(grep VmRSS /proc/$PID/status)"
ps -p $PID -o rss=
```

### Ejercicio 3 — Mapa de memoria

```bash
# ¿Qué bibliotecas carga tu shell?
grep '\.so' /proc/$$/maps | awk '{print $6}' | sort -u

# ¿Tiene heap asignado?
grep '\[heap\]' /proc/$$/maps

# ¿Dónde está su stack?
grep '\[stack\]' /proc/$$/maps
```
