# T01 — ps

> **Fuentes**: `README.md`, `README.max.md`, `labs/README.md`
>
> **Correcciones aplicadas**:
> - Ejercicio 6 del max.md: título "pgrep y pgrep" repetía el nombre.
>   Corregido a "pgrep vs pidof".
> - Tabla STAT del max.md: columna "Capturable?" con terminología confusa.
>   Clarificada como "¿Se puede matar con señales?".
> - Patrón "procesos sin terminal" (ambos READMEs):
>   `grep '^\s*[0-9].*?'` no filtra por TTY `?` — el `?` es metacarácter
>   regex. Corregido con `awk '$2 == "?"'`.
> - Añadida demostración de proceso zombie en los labs (presente en teoría
>   pero ausente en los labs).

---

## Tres estilos de sintaxis

`ps` acepta opciones en tres estilos porque fusionó tradiciones Unix
incompatibles:

| Estilo | Prefijo | Ejemplo | Origen |
|---|---|---|---|
| UNIX (POSIX) | con guión `-` | `ps -ef` | System V |
| BSD | sin guión | `ps aux` | BSD |
| GNU | doble guión `--` | `ps --forest` | Linux |

**No son equivalentes.** `ps aux` y `ps -aux` significan cosas diferentes:
- `ps aux` → BSD: todos los procesos, formato extendido
- `ps -aux` → UNIX: intenta buscar usuario `aux` (warning + fallback a BSD)

```bash
# Los dos comandos más usados — memorizarlos
ps aux     # BSD: todos los procesos, formato amplio
ps -ef     # UNIX: todos los procesos, formato completo
```

---

## ps aux — Formato BSD

```bash
ps aux
# USER  PID %CPU %MEM    VSZ   RSS TTY  STAT START   TIME COMMAND
# root    1  0.0  0.1 169524 13456 ?    Ss   Mar15   0:03 /usr/lib/systemd/systemd
# root    2  0.0  0.0      0     0 ?    S    Mar15   0:00 [kthreadd]
# dev  1842  0.2  1.3 437284 42816 ?    Ssl  09:12   0:15 /usr/bin/gnome-shell
```

| Columna | Significado |
|---|---|
| USER | Usuario que ejecuta el proceso |
| PID | Process ID |
| %CPU | Porcentaje de CPU (**promedio** desde que inició, no instantáneo) |
| %MEM | Porcentaje de memoria física (RSS / RAM total) |
| VSZ | Virtual memory Size (KB) — toda la memoria mapeada |
| RSS | Resident Set Size (KB) — memoria física realmente en RAM |
| TTY | Terminal asociada. `?` = no tiene (daemon/servicio) |
| STAT | Estado del proceso (ver sección STAT) |
| START | Hora o fecha de inicio |
| TIME | Tiempo acumulado de CPU |
| COMMAND | Comando con argumentos |

### VSZ vs RSS

```
VSZ = todo lo que el proceso tiene mapeado en memoria virtual
    = código + datos + heap + stack + libs compartidas + mmap + swap
    (incluye páginas reservadas pero no usadas)

RSS = lo que realmente está en RAM física ahora
    = no incluye swap, no incluye páginas no tocadas

Un proceso con VSZ=500MB y RSS=50MB:
- Ha reservado 500MB de espacio virtual
- Solo 50MB están realmente en RAM
```

**RSS sobreestima** el uso real porque cuenta las bibliotecas compartidas
completas en cada proceso. Para medidas más precisas:
`/proc/[pid]/smaps_rollup` → PSS (*Proportional Set Size*) divide las
bibliotecas compartidas entre los procesos que las usan.

---

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

**Diferencia principal con `aux`:** incluye **PPID** (padre) pero no
%CPU/%MEM/VSZ/RSS.

---

## Códigos STAT

La columna STAT codifica el estado del proceso. El primer carácter es el
estado principal, los siguientes son modificadores.

### Estado principal

| Código | Nombre | Significado | ¿Señales lo matan? |
|---|---|---|---|
| R | Running | Ejecutándose o en cola de ejecución | Sí |
| S | Sleeping | Esperando un evento (I/O, señal) — interruptible | Sí |
| D | Disk sleep | Esperando I/O del kernel — **no interruptible** | **No** (ni SIGKILL) |
| Z | Zombie | Terminó pero su padre no recogió el exit status | No (ya muerto) |
| T | Stopped | Detenido por señal (SIGSTOP, Ctrl+Z) | Sí (SIGCONT reanuda) |
| t | Tracing stop | Detenido por un debugger (ptrace) | — |
| I | Idle | Thread del kernel inactivo | — |

### Modificadores

| Código | Significado |
|---|---|
| `s` | Session leader (típicamente la shell del terminal) |
| `l` | Multithreaded (tiene threads/LWPs) |
| `+` | En el foreground process group del terminal |
| `<` | Alta prioridad (nice negativo) |
| `N` | Baja prioridad (nice positivo) |
| `L` | Páginas bloqueadas en memoria (mlock) |

### Interpretar estados comunes

```bash
# Ss   = Sleeping + session leader       → shell bash
# Ssl  = Sleeping + session leader + MT  → servicio systemd
# R+   = Running + foreground            → el propio ps que ejecutaste
# S+   = Sleeping + foreground           → programa interactivo esperando input
# D    = Disk sleep                      → proceso esperando I/O (normal si breve)
# Z    = Zombie                          → padre no hizo wait() — bug en la app
# T    = Stopped                         → Ctrl+Z o kill -STOP
```

### Procesos en estado D

Un proceso en estado `D` **no puede ser matado**, ni siquiera con `SIGKILL`.
Está esperando que una operación de I/O del kernel termine. Es normal verlos
brevemente durante I/O a disco. Si **persisten**, indica:
- NFS colgado (el más común)
- Problema de hardware (disco lento/fallando)
- Bug del kernel

```bash
# Buscar procesos en estado D y ver qué esperan
ps -eo pid,stat,wchan:20,cmd | awk '$2 ~ /D/'
```

### Procesos zombie

Un zombie ya terminó — no consume CPU ni memoria. El problema no es el zombie
sino su **padre**: no está llamando `wait()` para recoger el exit status.
Matar al padre (o esperar a que termine) limpia los zombies.

```bash
# Encontrar zombies y su padre
ps -eo pid,ppid,stat,cmd | awk '$3 ~ /Z/'
```

---

## Selectores — Filtrar procesos

### Por usuario

```bash
ps -u dev              # por UID efectivo (EUID)
ps -U root             # por UID real (RUID — puede diferir con SUID)
```

### Por PID

```bash
ps -p 1                # un proceso específico
ps -p 1,2,3            # varios PIDs
ps -p $$               # tu propia shell
```

### Por nombre de comando

```bash
ps -C sshd             # nombre exacto del ejecutable
ps -C nginx,httpd      # varios nombres

# -C busca el nombre del ejecutable, NO argumentos
# Para buscar en argumentos, usar grep:
ps aux | grep '[n]ginx'
#                ^ corchetes evitan que grep se encuentre a sí mismo
```

El truco `[n]ginx`: el proceso grep tiene como argumento la cadena `[n]ginx`
(con corchetes), que no coincide con la regex `[n]ginx` (que busca `nginx`).
Así grep no se incluye en sus propios resultados.

### Por terminal

```bash
ps -t pts/0            # procesos en un terminal específico
ps -t tty1
```

### Procesos sin terminal (daemons)

```bash
# El TTY de un daemon es "?" — filtrar con awk
ps -eo pid,tty,user,cmd | awk '$2 == "?"' | head -10
```

---

## Formato personalizado con -o

`-o` permite elegir exactamente qué columnas mostrar:

```bash
# Columnas específicas
ps -eo pid,ppid,user,%cpu,%mem,stat,cmd

# Con ancho de columna
ps -eo pid,ppid,user:15,%cpu,%mem,stat,args
#                   ^^^
#                   ancho mínimo de 15 caracteres

# Sin encabezados
ps --no-headers -eo pid,cmd

# Ordenar resultados
ps -eo pid,user,%cpu,%mem,cmd --sort=-%cpu | head -11
#                                    ^ - = descendente
```

### Columnas más útiles

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
| `%cpu` / `pcpu` | % CPU (promedio desde inicio) |
| `%mem` / `pmem` | % Memoria |
| `vsz` | Virtual memory (KB) |
| `rss` | Resident memory (KB) |
| `ni` | Nice value |
| `pri` | Prioridad del scheduler |
| `nlwp` | Número de threads |
| `wchan` | Función del kernel donde duerme |
| `cmd` | Nombre del comando (corto) |
| `args` / `command` | Comando con argumentos completos |
| `etime` | Tiempo transcurrido (formato legible) |
| `etimes` | Tiempo transcurrido en segundos (para scripts) |
| `lstart` | Fecha/hora exacta de inicio |

### Patrones de uso frecuente

```bash
# Top 10 por CPU
ps -eo pid,user,%cpu,%mem,cmd --sort=-%cpu | head -11

# Top 10 por memoria
ps -eo pid,user,%cpu,%mem,rss,cmd --sort=-rss | head -11

# Procesos con más threads
ps -eo pid,user,nlwp,cmd --sort=-nlwp | head -11

# Zombies
ps -eo pid,ppid,stat,cmd | awk '$3 ~ /Z/'

# Cuánto tiempo lleva un proceso
ps -p 1 -o pid,etime,lstart

# Cuántos procesos por usuario
ps -eo user --no-headers | sort | uniq -c | sort -rn
```

---

## Árbol de procesos

### ps con tree view

```bash
# BSD: f = forest (árbol)
ps auxf
# root   1  ...  /usr/lib/systemd/systemd
# root 502  ...   \_ /usr/lib/systemd/systemd-journald
# root 842  ...   \_ /usr/sbin/sshd -D
# root 1501 ...       \_ sshd: dev [priv]
# dev  1506 ...           \_ -bash
# dev  1842 ...               \_ ps auxf

# UNIX: -ejH = todos, jobs format, hierarchy
ps -ejH
```

### pstree

Más legible para jerarquías:

```bash
# Árbol completo
pstree

# Con PIDs
pstree -p

# Árbol de un proceso específico
pstree -p 1501

# Árbol de un usuario
pstree dev

# Mostrar argumentos
pstree -a

# Cadena de tu shell hasta PID 1
pstree -sp $$
```

En Debian, `pstree` está en el paquete `psmisc`. En RHEL suele estar
instalado por defecto.

---

## pgrep y pidof

```bash
# pgrep: buscar por nombre (regex)
pgrep sshd              # PIDs que coinciden
pgrep -x sshd           # nombre exacto (no parcial)
pgrep -l sshd           # PID + nombre
pgrep -a sshd           # PID + línea de comando completa
pgrep -f "nginx.*worker" # busca en toda la línea de comando

# pidof: buscar por nombre exacto del ejecutable
pidof sshd               # solo nombres exactos, sin regex
```

---

## ps en contenedores

Dentro de un contenedor, `ps` solo muestra procesos del PID namespace del
contenedor:

```bash
# Dentro del contenedor
ps aux
# PID 1 = entrypoint del contenedor (no systemd del host)

# En el host: ver el PID real del proceso del contenedor
ps -p $(docker inspect --format '{{.State.Pid}}' mi-container)
```

---

## Diferencias entre distribuciones

| Aspecto | Debian | RHEL |
|---|---|---|
| Paquete | `procps` | `procps-ng` |
| pstree | `psmisc` (instalar) | Incluido por defecto |
| Funcionalidad | Idéntica | Idéntica |

---

## Labs

### Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

### Parte 1 — ps aux vs ps -ef

#### Paso 1.1: ps aux (formato BSD)

```bash
docker compose exec debian-dev bash -c '
echo "=== ps aux (formato BSD) ==="
ps aux | head -5
echo ""
echo "Columnas: USER PID %CPU %MEM VSZ RSS TTY STAT START TIME COMMAND"
echo "Muestra %CPU, %MEM, VSZ, RSS — pero NO muestra PPID"
'
```

`a` = todos los usuarios, `u` = formato usuario, `x` = incluir procesos
sin terminal.

#### Paso 1.2: ps -ef (formato System V)

```bash
docker compose exec debian-dev bash -c '
echo "=== ps -ef (formato System V) ==="
ps -ef | head -5
echo ""
echo "Columnas: UID PID PPID C STIME TTY TIME CMD"
echo "Muestra PPID (padre) — pero NO muestra %CPU, %MEM, VSZ, RSS"
'
```

`-e` = todos los procesos, `-f` = formato completo.

#### Paso 1.3: Diferencia clave — PPID

```bash
docker compose exec debian-dev bash -c '
echo "=== ps aux NO muestra PPID ==="
ps aux | head -1

echo ""
echo "=== ps -ef SÍ muestra PPID ==="
ps -ef | head -1

echo ""
echo "=== Ejemplo: cadena de padres ==="
ps -ef | head -10
echo ""
echo "El PPID permite reconstruir el árbol de procesos"
'
```

#### Paso 1.4: Contar procesos

```bash
docker compose exec debian-dev bash -c '
echo "=== Total de procesos ==="
echo "ps aux: $(ps aux --no-headers | wc -l) procesos"
echo "ps -ef: $(ps -ef --no-headers | wc -l) procesos"
echo ""
echo "(ambos deben mostrar el mismo número)"
echo "(en un contenedor hay pocos comparado con un host)"
'
```

---

### Parte 2 — Códigos STAT

#### Paso 2.1: Distribución de estados

```bash
docker compose exec debian-dev bash -c '
echo "=== Procesos actuales por estado ==="
ps aux --no-headers | awk "{print \$8}" | cut -c1 | sort | uniq -c | sort -rn
echo ""
echo "La mayoría estará en S (sleeping) — es normal"
'
```

#### Paso 2.2: Crear proceso sleeping y stopped

```bash
docker compose exec debian-dev bash -c '
echo "=== Proceso sleeping (S) ==="
sleep 300 &
SLEEP_PID=$!
ps -o pid,stat,cmd -p $SLEEP_PID

echo ""
echo "=== Detener con SIGSTOP → estado T ==="
sleep 301 &
STOP_PID=$!
kill -STOP $STOP_PID
ps -o pid,stat,cmd -p $STOP_PID

echo ""
echo "=== Reanudar con SIGCONT → vuelve a S ==="
kill -CONT $STOP_PID
ps -o pid,stat,cmd -p $STOP_PID

kill $SLEEP_PID $STOP_PID 2>/dev/null
wait 2>/dev/null
'
```

#### Paso 2.3: Crear proceso zombie

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear proceso zombie ==="
# El padre lanza un hijo y NO llama wait()
cat << "ZOMBIE_SCRIPT" > /tmp/make-zombie.sh
#!/bin/bash
# Hijo que termina inmediatamente
(exit 0) &
CHILD_PID=$!
echo "Hijo PID: $CHILD_PID"
# Padre NO hace wait — el hijo queda zombie
sleep 2
echo "Estado del hijo:"
ps -o pid,ppid,stat,cmd -p $CHILD_PID 2>/dev/null || echo "(ya fue limpiado)"
# Ahora limpiar
wait $CHILD_PID 2>/dev/null
ZOMBIE_SCRIPT
chmod +x /tmp/make-zombie.sh
/tmp/make-zombie.sh
echo ""
echo "Los zombies aparecen cuando el padre no llama wait()"
echo "Matar al padre (o que termine) limpia los zombies"
rm -f /tmp/make-zombie.sh
'
```

#### Paso 2.4: ps se muestra a sí mismo en R

```bash
docker compose exec debian-dev bash -c '
echo "=== Estado de ps mientras ejecuta ==="
ps aux | grep "[p]s aux"
echo ""
echo "ps es el único proceso garantizado en estado R en ese instante"
echo "(el truco [p] evita que grep se muestre a sí mismo)"
'
```

---

### Parte 3 — Formato personalizado y selectores

#### Paso 3.1: Formato con -o

```bash
docker compose exec debian-dev bash -c '
echo "=== PID, PPID, estado, comando ==="
ps -eo pid,ppid,stat,cmd | head -10

echo ""
echo "=== Ordenado por CPU descendente ==="
ps -eo pid,ppid,stat,user,%cpu,cmd --sort=-%cpu | head -10
'
```

#### Paso 3.2: Selectores

```bash
docker compose exec debian-dev bash -c '
echo "=== Por usuario (-u root) ==="
ps -u root -o pid,user,stat,cmd | head -5

echo ""
echo "=== Por nombre de comando (-C sleep) ==="
sleep 200 &
SPID=$!
ps -C sleep -o pid,ppid,cmd
kill $SPID 2>/dev/null

echo ""
echo "=== Por PID (-p 1) ==="
ps -p 1 -o pid,ppid,user,stat,cmd
'
```

#### Paso 3.3: VSZ vs RSS

```bash
docker compose exec debian-dev bash -c '
echo "=== Top procesos por RSS (memoria física) ==="
ps -eo pid,vsz,rss,cmd --sort=-rss | head -10
echo ""
echo "VSZ = memoria virtual (reservada, incluye no usada)"
echo "RSS = memoria física (realmente en RAM)"
echo "VSZ siempre >= RSS"
'
```

#### Paso 3.4: pstree

```bash
docker compose exec debian-dev bash -c '
echo "=== Árbol de procesos ==="
pstree -p | head -20

echo ""
echo "=== Cadena hasta PID 1 ==="
pstree -sp $$
'
```

#### Paso 3.5: Comparar entre distros

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c 'ps -p 1 -o pid,user,cmd; ps --version 2>&1 | head -1'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c 'ps -p 1 -o pid,user,cmd; ps --version 2>&1 | head -1'
```

---

### Limpieza final

No hay recursos que limpiar.

---

## Ejercicios

### Ejercicio 1 — ps aux vs ps -ef: diferencias

```bash
ps aux | head -5
echo "---"
ps -ef | head -5
```

**Pregunta:** ¿Qué columnas tiene `ps aux` que no tiene `ps -ef`? ¿Y al
revés? ¿Cuándo usarías cada uno?

<details><summary>Predicción</summary>

**`ps aux` tiene pero `ps -ef` no:**
- `%CPU` — porcentaje de CPU (promedio desde inicio)
- `%MEM` — porcentaje de memoria
- `VSZ` — memoria virtual
- `RSS` — memoria física
- `STAT` — estado con modificadores (Ss, Ssl, R+, etc.)

**`ps -ef` tiene pero `ps aux` no:**
- `PPID` — PID del proceso padre

**Cuándo usar cada uno:**
- `ps aux` → cuando necesitas saber cuánta **CPU/memoria** consume un proceso
- `ps -ef` → cuando necesitas saber **quién creó** un proceso (PPID)
- `ps -eo ...` → cuando necesitas columnas específicas de ambos mundos

En la práctica, `ps -eo pid,ppid,user,%cpu,%mem,stat,cmd` combina lo mejor
de ambos formatos.

</details>

---

### Ejercicio 2 — Interpretar STAT

Dado este extracto de `ps aux`:

```
USER  PID  %CPU %MEM   VSZ   RSS TTY  STAT  ...  COMMAND
root    1   0.0  0.1 16952  1345  ?   Ss         /sbin/init
root  502   0.0  0.0     0     0  ?   I<         [kworker/0:1H]
dev  1842   2.1  1.3 43728 42816  ?   Ssl        /usr/bin/gnome-shell
dev  2001   0.0  0.0  8216  3456 pts/0 S+        vim file.txt
root 2050   0.0  0.0     0     0  ?   Z          [defunct]
dev  2100   0.0  0.0  5432  1234 pts/1 T         sleep 3600
```

**Pregunta:** Interpreta el STAT de cada proceso.

<details><summary>Predicción</summary>

| PID | STAT | Interpretación |
|---|---|---|
| 1 | `Ss` | **S**leeping + **s**ession leader. Es init/systemd, líder de la sesión. Normal. |
| 502 | `I<` | **I**dle (kernel thread inactivo) + **<** alta prioridad. Thread del kernel esperando trabajo, con nice negativo. Normal. |
| 1842 | `Ssl` | **S**leeping + **s**ession leader + multi-**l**threaded. Servicio con múltiples threads, líder de su sesión. Típico de servicios systemd. |
| 2001 | `S+` | **S**leeping + foreground (**+**). Vim esperando input del usuario, en foreground del terminal pts/0. Normal. |
| 2050 | `Z` | **Z**ombie. Proceso terminado cuyo padre no ha llamado `wait()`. El nombre `[defunct]` lo confirma. No consume recursos pero indica un bug en el padre. |
| 2100 | `T` | S**t**opped. Proceso detenido por señal (probablemente Ctrl+Z o `kill -STOP`). Se puede reanudar con `kill -CONT 2100` o `fg`. |

Notas adicionales:
- PID 502 tiene VSZ=0 y RSS=0 porque es un kernel thread (no tiene espacio
  de usuario propio)
- PID 2050 (zombie) también consume 0 recursos — solo ocupa una entrada en
  la tabla de procesos

</details>

---

### Ejercicio 3 — Formato personalizado y ordenación

```bash
# Top 5 procesos por memoria
ps -eo pid,user,%mem,rss,cmd --sort=-rss | head -6

# Top 5 procesos por CPU
ps -eo pid,user,%cpu,cmd --sort=-%cpu | head -6

# Procesos con más threads
ps -eo pid,user,nlwp,cmd --sort=-nlwp | head -6
```

**Pregunta:** ¿`--sort=-rss` ordena ascendente o descendente? ¿Qué proceso
tiene más threads?

<details><summary>Predicción</summary>

`--sort=-rss` ordena **descendente** (el `-` indica orden inverso). Sin `-`
sería ascendente.

El encabezado se cuenta como primera línea, por eso `head -6` muestra el
header + 5 procesos.

Los procesos con más threads (`nlwp` = Number of Light Weight Processes)
típicamente son:
- Servidores web (nginx, apache)
- Bases de datos (mysql, postgres)
- Entornos de escritorio (gnome-shell, Xorg)
- Java/JVM (cualquier aplicación Java usa muchos threads)

En un contenedor, los números serán mucho menores que en un host.

`nlwp=1` significa que el proceso es single-threaded (no usa threads
adicionales más allá del thread principal).

</details>

---

### Ejercicio 4 — Selectores: encontrar procesos

```bash
# Por usuario
ps -u root -o pid,user,stat,cmd | head -10

# Por nombre exacto
ps -C bash -o pid,ppid,tty,cmd

# Por PID — tu propia shell
ps -p $$ -o pid,ppid,user,stat,tty,cmd

# Buscar en argumentos (grep)
ps aux | grep '[s]sh'
```

**Pregunta:** ¿Por qué `ps -C ssh` y `ps aux | grep ssh` pueden dar
resultados diferentes? ¿Qué hace el truco `[s]sh`?

<details><summary>Predicción</summary>

**`ps -C ssh`** busca procesos cuyo nombre de **ejecutable** sea exactamente
`ssh`. No encuentra `sshd` (es otro nombre) ni procesos cuyos argumentos
contengan "ssh".

**`ps aux | grep ssh`** busca la cadena "ssh" en **toda la línea** de salida
de `ps`, incluyendo argumentos. Encuentra:
- `sshd` (el daemon)
- `ssh user@host` (clientes SSH)
- `ssh-agent`
- Y **grep ssh** (el propio grep, si no se usa el truco)

**El truco `[s]sh`:**
- La regex `[s]sh` busca una `s` seguida de `sh` — coincide con `ssh`
- Pero el proceso `grep [s]sh` tiene la cadena literal `[s]sh` en sus
  argumentos, que NO coincide con la regex `[s]sh`
- Así grep se excluye a sí mismo de los resultados

Alternativa más limpia: `pgrep -a ssh` (no tiene el problema del self-match).

</details>

---

### Ejercicio 5 — Árbol de procesos

```bash
pstree -sp $$
echo "---"
ps auxf | head -25
```

**Pregunta:** ¿Cuántos niveles hay entre tu shell y PID 1? ¿Qué procesos
intermedios hay?

<details><summary>Predicción</summary>

En un contenedor Docker, la cadena típica es corta:

```
init(1)───bash(PID)
```

O si estás conectado via `docker compose exec`:
```
init(1)───bash(X)───bash($$)
```

En un sistema real con SSH:
```
systemd(1)───sshd(A)───sshd(B)───sshd(C)───bash($$)
```

`pstree -sp $$` muestra:
- `-s` = muestra los padres (ancestors)
- `-p` = muestra los PIDs

`ps auxf` muestra el árbol completo con indentación `\_` para relaciones
padre-hijo. Es equivalente a `pstree` pero con todas las columnas de `aux`.

La diferencia: `pstree` es visual y compacto, `ps auxf` muestra datos
de cada proceso (CPU, memoria, etc.).

</details>

---

### Ejercicio 6 — pgrep vs pidof

```bash
# pgrep -l: PID + nombre
pgrep -l bash

# pgrep -a: PID + línea completa
pgrep -a bash

# pgrep -x: nombre exacto (no parcial)
pgrep -x bash
pgrep -x bas     # ¿encuentra algo?

# pidof: solo ejecutables exactos
pidof bash
```

**Pregunta:** ¿`pgrep bash` y `pgrep -x bash` dan los mismos resultados?
¿Y `pgrep -x bas`? ¿Cuál es la diferencia entre `pgrep` y `pidof`?

<details><summary>Predicción</summary>

**`pgrep bash`** encuentra procesos cuyo nombre **contiene** "bash":
- bash
- bash (otros)
- Potencialmente: xbash, foobash, etc. (si existieran)

**`pgrep -x bash`** encuentra solo procesos cuyo nombre es **exactamente**
"bash". No coincide con nombres parciales.

**`pgrep -x bas`** no encuentra nada — ningún proceso se llama exactamente
"bas".

**Diferencias pgrep vs pidof:**
- `pgrep` usa regex por defecto, `-x` para exacto
- `pidof` siempre busca nombre exacto del ejecutable
- `pgrep -f` busca en toda la línea de comando
- `pidof` no soporta regex ni búsqueda en argumentos
- `pgrep` devuelve uno por línea, `pidof` devuelve todos en una línea

En scripts, `pgrep` es más flexible. `pidof` es más simple para casos
básicos.

</details>

---

### Ejercicio 7 — Threads de un proceso

```bash
# Crear proceso multi-threaded (bash en sí es single-threaded)
# Usar un proceso existente con threads
ps -eo pid,nlwp,cmd --sort=-nlwp | head -6

# Ver threads individuales de un proceso con -L
PID=$(ps -eo pid,nlwp --sort=-nlwp --no-headers | head -1 | awk '{print $1}')
ps -o pid,tid,stat,cmd -L -p $PID | head -10
```

**Pregunta:** ¿Qué muestra `-L`? ¿Cuál es la diferencia entre PID y TID?

<details><summary>Predicción</summary>

`-L` muestra los **threads** (LWPs — Light Weight Processes) individuales
de un proceso. Sin `-L`, cada proceso aparece una sola vez aunque tenga
muchos threads.

```
PID    TID   STAT CMD
1842   1842  Ssl  /usr/bin/gnome-shell
1842   1850  Sl   /usr/bin/gnome-shell
1842   1851  Sl   /usr/bin/gnome-shell
```

- **PID** (*Process ID*): identificador del proceso completo. Todos los
  threads de un proceso comparten el mismo PID.
- **TID** (*Thread ID*): identificador individual de cada thread. El thread
  principal tiene TID = PID.

`nlwp` (*Number of LWPs*) muestra cuántos threads tiene el proceso.
Un proceso con `nlwp=1` es single-threaded.

Todos los threads de un proceso comparten: espacio de memoria, file
descriptors, y PID. Cada thread tiene su propio: TID, stack, y registros
del CPU.

</details>

---

### Ejercicio 8 — Procesos más antiguos

```bash
ps -eo pid,etime,lstart,cmd --sort=-etimes | head -11
```

**Pregunta:** ¿Qué proceso lleva más tiempo ejecutando? ¿`etime` y `etimes`
muestran lo mismo?

<details><summary>Predicción</summary>

El proceso más antiguo será PID 1 (init/systemd), seguido de kernel threads
y servicios del sistema.

```
PID     ELAPSED                    LSTART                    CMD
  1  10-03:45:12  Sat Mar 15 05:30:00 2026  /usr/lib/systemd/systemd
  2  10-03:45:12  Sat Mar 15 05:30:00 2026  [kthreadd]
```

- **`etime`** muestra tiempo transcurrido en formato legible:
  `dd-hh:mm:ss` (días-horas:minutos:segundos)
- **`etimes`** muestra lo mismo pero en **segundos** (útil para scripts y
  ordenación)
- **`lstart`** muestra la fecha/hora exacta de inicio

`--sort=-etimes` ordena por tiempo transcurrido en segundos (descendente),
mostrando primero los más antiguos.

En un contenedor, el "más antiguo" será el PID 1 del contenedor, que existe
desde que el contenedor se inició.

</details>

---

### Ejercicio 9 — Encontrar daemons (procesos sin terminal)

```bash
echo "=== Procesos SIN terminal (daemons) ==="
ps -eo pid,tty,user,cmd | awk '$2 == "?"' | head -10

echo ""
echo "=== Procesos CON terminal ==="
ps -eo pid,tty,user,cmd | awk '$2 != "?" && NR > 1' | head -10

echo ""
echo "=== Conteo ==="
echo "Sin terminal: $(ps -eo tty --no-headers | grep -c '^?')"
echo "Con terminal: $(ps -eo tty --no-headers | grep -cv '^?')"
```

**Pregunta:** ¿Hay más procesos con terminal o sin terminal? ¿Por qué?

<details><summary>Predicción</summary>

Hay **muchos más procesos sin terminal** (`?`).

En un sistema típico:
```
Sin terminal: ~200+
Con terminal: ~5-10
```

En un contenedor Docker:
```
Sin terminal: ~3-5
Con terminal: ~2-3
```

Los procesos **sin terminal** son daemons y servicios del sistema: systemd,
sshd, cron, networkmanager, etc. Se iniciaron sin asociarse a ningún
terminal porque ejecutan en background de forma permanente.

Los procesos **con terminal** son shells interactivas y los comandos que
ejecutas desde ellas (tu bash, el propio ps, vim, etc.). Están asociados
a un pseudo-terminal (pts/0, pts/1) o a una consola (tty1).

Un daemon bien diseñado se desasocia del terminal durante su inicio
(fork + setsid) para no depender de ninguna sesión de usuario.

</details>

---

### Ejercicio 10 — ps en contenedor vs host

```bash
echo "=== Debian container ==="
docker compose exec debian-dev bash -c '
echo "PID 1: $(ps -p 1 -o cmd --no-headers)"
echo "Total procesos: $(ps -e --no-headers | wc -l)"
ps -eo pid,ppid,cmd | head -5
'

echo ""
echo "=== AlmaLinux container ==="
docker compose exec alma-dev bash -c '
echo "PID 1: $(ps -p 1 -o cmd --no-headers)"
echo "Total procesos: $(ps -e --no-headers | wc -l)"
ps -eo pid,ppid,cmd | head -5
'
```

**Pregunta:** ¿El PID 1 del contenedor es systemd? ¿Cuántos procesos hay
comparado con un host normal?

<details><summary>Predicción</summary>

**PID 1 del contenedor:** No es systemd. Probablemente es `bash` o el
entrypoint definido en el Dockerfile. En contenedores Docker, PID 1 es
el proceso especificado por `CMD`/`ENTRYPOINT`.

```
Debian container:
  PID 1: /bin/bash (o similar)
  Total: ~3-5 procesos

AlmaLinux container:
  PID 1: /bin/bash (o similar)
  Total: ~3-5 procesos
```

**En un host normal**, PID 1 sería `/usr/lib/systemd/systemd` y habría
cientos de procesos (150-300+).

Esto es porque el contenedor tiene su propio **PID namespace** — un
espacio aislado donde la numeración de PIDs empieza desde 1. Los procesos
del host son invisibles desde dentro del contenedor.

El PID 1 del contenedor tiene una responsabilidad especial: si termina,
el contenedor entero se detiene. Además, debería manejar señales y
recoger procesos zombies (equivalente a lo que hace init/systemd en un
host).

</details>

---

## Limpieza de ejercicios

No hay recursos que limpiar.
