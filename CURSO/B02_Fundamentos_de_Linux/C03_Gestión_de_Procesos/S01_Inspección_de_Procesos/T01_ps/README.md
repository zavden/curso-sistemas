# T01 — ps

## Tres estilos de sintaxis

`ps` acepta opciones en tres estilos diferentes porque fusionó tradiciones Unix
incompatibles. Esto causa confusión constante:

| Estilo | Prefijo | Ejemplo | Origen |
|---|---|---|---|
| UNIX (POSIX) | con guión `-` | `ps -ef` | System V |
| BSD | sin guión | `ps aux` | BSD |
| GNU | doble guión `--` | `ps --forest` | Linux |

**No son equivalentes**. `ps aux` y `ps -aux` significan cosas diferentes:
- `ps aux` → BSD: todos los procesos, formato extendido
- `ps -aux` → UNIX: procesos del usuario `aux` (o warning y fallback a BSD)

```bash
# Los dos comandos más usados — memorizarlos
ps aux     # BSD: todos los procesos, formato amplio
ps -ef     # UNIX: todos los procesos, formato completo
```

## ps aux — Formato BSD

```bash
ps aux
# USER  PID %CPU %MEM    VSZ   RSS TTY  STAT START   TIME COMMAND
# root    1  0.0  0.1 169524 13456 ?    Ss   Mar15   0:03 /usr/lib/systemd/systemd
# root    2  0.0  0.0      0     0 ?    S    Mar15   0:00 [kthreadd]
# dev  1842  0.2  1.3 437284 42816 ?    Ssl  09:12   0:15 /usr/bin/gnome-shell
```

### Columnas de ps aux

| Columna | Significado |
|---|---|
| USER | Usuario que ejecuta el proceso |
| PID | Process ID |
| %CPU | Porcentaje de CPU (desde que inició el proceso, no instantáneo) |
| %MEM | Porcentaje de memoria física (RSS / RAM total) |
| VSZ | Virtual memory Size (KB) — toda la memoria mapeada (incluye compartida, swap, no usada) |
| RSS | Resident Set Size (KB) — memoria física realmente en RAM |
| TTY | Terminal asociada. `?` = no tiene (daemon/servicio) |
| STAT | Estado del proceso (ver abajo) |
| START | Hora o fecha de inicio |
| TIME | Tiempo acumulado de CPU |
| COMMAND | Comando con argumentos |

### VSZ vs RSS

VSZ siempre es mayor o igual que RSS:

```
VSZ = todo lo que el proceso tiene mapeado en memoria virtual
    = código + datos + heap + stack + libs compartidas + mmap + swap

RSS = lo que realmente está en RAM física ahora
    = no incluye swap, no incluye páginas no tocadas

Un proceso con VSZ=500MB y RSS=50MB:
- Ha reservado 500MB de espacio virtual
- Solo 50MB están realmente en RAM
- El resto puede estar en swap, no asignado, o es espacio de libs compartidas
```

RSS sobreestima el uso real porque cuenta las bibliotecas compartidas completas
en cada proceso que las usa. Para uso de memoria más preciso, mirar
`/proc/[pid]/smaps_rollup` (PSS — Proportional Set Size).

## ps -ef — Formato UNIX

```bash
ps -ef
# UID     PID  PPID  C STIME TTY          TIME CMD
# root      1     0  0 Mar15 ?        00:00:03 /usr/lib/systemd/systemd
# root      2     0  0 Mar15 ?        00:00:00 [kthreadd]
# dev    1842  1501  0 09:12 ?        00:00:15 /usr/bin/gnome-shell
```

| Columna | Significado |
|---|---|
| UID | Usuario |
| PID | Process ID |
| PPID | Parent Process ID — quién creó este proceso |
| C | Uso de CPU (entero, menos preciso que %CPU de BSD) |
| STIME | Hora de inicio |
| TTY | Terminal |
| TIME | Tiempo acumulado de CPU |
| CMD | Comando |

La diferencia principal con `aux`: incluye **PPID** (padre) pero no %CPU/%MEM/VSZ/RSS.

## Códigos STAT

La columna STAT indica el estado del proceso. El primer carácter es el estado
principal y los siguientes son modificadores:

### Estado principal

| Código | Nombre | Significado |
|---|---|---|
| R | Running | Ejecutándose o en cola de ejecución |
| S | Sleeping | Esperando un evento (I/O, señal, timer) — interruptible |
| D | Disk sleep | Esperando I/O — **no interruptible** (no responde a señales) |
| Z | Zombie | Terminó pero su padre no recogió su exit status |
| T | Stopped | Detenido por señal (SIGSTOP, SIGTSTP — Ctrl+Z) |
| t | Tracing stop | Detenido por un debugger (ptrace) |
| I | Idle | Thread del kernel inactivo (solo kernel threads) |

### Modificadores

| Código | Significado |
|---|---|
| `s` | Session leader (típicamente la shell del terminal) |
| `l` | Multithreaded (usa clone para threads) |
| `+` | En el foreground process group del terminal |
| `<` | Alta prioridad (nice negativo) |
| `N` | Baja prioridad (nice positivo) |
| `L` | Páginas bloqueadas en memoria (mlock) |

```bash
# Interpretar estados comunes:
# Ss   = Sleeping + session leader (típico: shell bash)
# Ssl  = Sleeping + session leader + multithreaded (típico: servicio systemd)
# R+   = Running + foreground (típico: el propio ps que ejecutaste)
# S+   = Sleeping + foreground
# D    = Disk sleep (proceso esperando I/O, no se puede matar)
# Z    = Zombie (padre no hizo wait(), problema en la aplicación)
# T    = Stopped (Ctrl+Z o kill -STOP)
```

### Procesos en estado D

Un proceso en estado `D` (uninterruptible sleep) **no puede ser matado**, ni
siquiera con `SIGKILL`. Está esperando que una operación de I/O del kernel
termine. Es normal verlos brevemente durante I/O a disco. Si persisten, indica
un problema de hardware, NFS colgado, o bug del kernel:

```bash
# Buscar procesos en estado D
ps aux | awk '$8 ~ /D/ {print}'
```

## Selectores — Filtrar procesos

### Por usuario

```bash
# Procesos de un usuario específico
ps -u dev
ps -U root        # por UID real
ps -u root        # por UID efectivo (EUID — puede diferir si hay SUID)
```

### Por PID

```bash
# Un proceso específico
ps -p 1
ps -p 1,2,3       # varios PIDs

# El propio shell
ps -p $$
```

### Por nombre de comando

```bash
# Buscar por nombre de programa
ps -C sshd
ps -C nginx,httpd   # varios nombres

# Esto busca el nombre exacto del ejecutable, NO argumentos
# Para buscar en argumentos, usar grep:
ps aux | grep '[n]ginx'
#                  ^ Los corchetes evitan que grep se encuentre a sí mismo
```

### Por terminal

```bash
# Procesos en un terminal específico
ps -t pts/0
ps -t tty1
```

### Por grupo

```bash
# Procesos de un grupo
ps -G developers     # por GID real
ps -g developers     # por GID efectivo
```

## Formato personalizado con -o

`-o` permite elegir exactamente qué columnas mostrar:

```bash
# Columnas específicas
ps -eo pid,ppid,user,%cpu,%mem,stat,cmd

# Con encabezados personalizados
ps -eo pid,ppid,user:15,%cpu,%mem,stat,args
#                   ^^^
#                   ancho mínimo de columna

# Sin encabezados
ps --no-headers -eo pid,cmd

# Columnas útiles para debugging
ps -eo pid,ppid,pgid,sid,tty,stat,user,cmd
```

### Columnas disponibles más útiles

| Columna | Significado |
|---|---|
| `pid` | Process ID |
| `ppid` | Parent PID |
| `pgid` | Process Group ID |
| `sid` | Session ID |
| `tty` | Terminal |
| `stat` | Estado con modificadores |
| `user` / `euser` | Usuario efectivo |
| `ruser` | Usuario real |
| `uid` / `euid` | UID efectivo |
| `ruid` | UID real |
| `%cpu` / `pcpu` | % CPU |
| `%mem` / `pmem` | % Memoria |
| `vsz` | Virtual memory (KB) |
| `rss` | Resident memory (KB) |
| `ni` | Nice value |
| `pri` | Prioridad |
| `nlwp` | Número de threads |
| `wchan` | Función del kernel donde duerme el proceso |
| `cmd` | Nombre del comando (corto) |
| `args` / `command` | Comando con argumentos completos |
| `etime` | Tiempo transcurrido desde inicio |
| `etimes` | Tiempo transcurrido en segundos (útil para ordenar/scripting) |
| `lstart` | Fecha/hora exacta de inicio |

```bash
# Los 10 procesos que más CPU usan
ps -eo pid,user,%cpu,%mem,cmd --sort=-%cpu | head -11

# Los 10 que más memoria usan
ps -eo pid,user,%cpu,%mem,rss,cmd --sort=-rss | head -11

# Procesos con más threads
ps -eo pid,user,nlwp,cmd --sort=-nlwp | head -11

# Procesos zombie
ps -eo pid,ppid,stat,cmd | grep '^.*Z'

# Cuánto tiempo lleva ejecutando un proceso
ps -p 1 -o pid,etime,lstart
```

## Árbol de procesos

### ps con tree view

```bash
# BSD: f = forest (árbol)
ps auxf
# root   1  ...  /usr/lib/systemd/systemd
# root 502  ...   \_ /usr/lib/systemd/systemd-journald
# root 520  ...   \_ /usr/lib/systemd/systemd-udevd
# root 842  ...   \_ /usr/sbin/sshd -D
# root 1501 ...       \_ sshd: dev [priv]
# dev  1505 ...           \_ sshd: dev@pts/0
# dev  1506 ...               \_ -bash
# dev  1842 ...                   \_ ps auxf

# UNIX: -ejH = todos, jobs format, hierarchy
ps -ejH
```

### pstree

`pstree` es más legible para visualizar la jerarquía:

```bash
# Árbol completo
pstree
# systemd─┬─ModemManager───2*[{ModemManager}]
#          ├─NetworkManager───2*[{NetworkManager}]
#          ├─sshd───sshd───sshd───bash───pstree
#          └─systemd-journal

# Con PIDs
pstree -p
# systemd(1)─┬─ModemManager(502)─┬─{ModemManager}(510)
#             │                    └─{ModemManager}(511)

# Árbol de un proceso específico
pstree -p 1501

# Árbol de un usuario
pstree dev

# Mostrar argumentos del comando
pstree -a

# Resaltar un PID
pstree -H 1501
```

En Debian, `pstree` está en el paquete `psmisc`. En RHEL suele estar instalado
por defecto.

## Patrones prácticos

```bash
# ¿Quién es el padre de un proceso?
ps -o ppid= -p 1842
# 1501

# Cadena completa de padres hasta init
ps -o pid,ppid,cmd -p $(ps -o ppid= -p $(ps -o ppid= -p $$))
# Más fácil con pstree:
pstree -sp $$

# ¿Cuántos procesos hay en total?
ps -e --no-headers | wc -l

# ¿Cuántos procesos por usuario?
ps -eo user --no-headers | sort | uniq -c | sort -rn

# Procesos sin terminal (daemons/servicios)
ps -eo pid,tty,cmd | grep '^\s*[0-9].*?'

# Encontrar el PID de un proceso por nombre
pgrep -x sshd         # exacto
pgrep -f "nginx.*worker"  # busca en toda la línea de comando
pidof sshd             # alternativa, solo ejecutables exactos
```

## Diferencias entre distribuciones

Ambas distribuciones usan el mismo `ps` del paquete `procps` (o `procps-ng`).
Las diferencias son mínimas:

| Aspecto | Debian | RHEL |
|---|---|---|
| Paquete | `procps` | `procps-ng` |
| pstree | `psmisc` (puede necesitar instalarse) | Incluido por defecto |
| Versión | Generalmente idéntica funcionalidad | Idéntica |

## ps en contenedores

Dentro de un contenedor, `ps` solo muestra los procesos visibles en el
PID namespace del contenedor. El PID 1 dentro del contenedor no es el PID 1
del host:

```bash
# En el host
ps -p $(docker inspect --format '{{.State.Pid}}' mi-container)
# Muestra el PID real en el host

# Dentro del contenedor
ps aux
# Solo muestra procesos del contenedor, PID 1 = entrypoint
```

---

## Ejercicios

### Ejercicio 1 — Formatos y columnas

```bash
# Comparar los dos formatos principales
ps aux | head -5
ps -ef | head -5

# ¿Qué columnas tiene uno que no tiene el otro?

# Formato personalizado: PID, padre, usuario, estado, comando
ps -eo pid,ppid,user,stat,cmd | head -10
```

### Ejercicio 2 — Selectores y filtros

```bash
# Procesos del usuario actual
ps -u $(whoami)

# Buscar un proceso por nombre
ps -C bash

# Los 5 procesos con más memoria
ps -eo pid,user,%mem,rss,cmd --sort=-rss | head -6

# Buscar procesos zombie
ps -eo pid,ppid,stat,cmd | awk '$3 ~ /Z/'
```

### Ejercicio 3 — Árbol de procesos

```bash
# Ver la jerarquía completa
ps auxf | head -30

# Árbol de tu sesión actual
pstree -sp $$

# ¿Cuántos niveles hay entre tu shell y PID 1?
pstree -sp $$ | tr '─' '\n' | grep -c '('
```
