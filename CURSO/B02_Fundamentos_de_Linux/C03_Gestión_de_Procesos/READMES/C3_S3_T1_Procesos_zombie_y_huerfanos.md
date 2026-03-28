# T01 — Procesos zombie y huérfanos

## Errata y aclaraciones sobre el material original

### Error: `grep -c Z` sin ancla

En README.md línea 91:

```bash
ps -eo stat | grep -c Z
```

El campo `stat` puede ser multi-carácter (`Ss`, `Sl`, `Z+`). Sin `^`, podría
coincidir con estados que contengan Z en posición no inicial (improbable pero
impreciso). Los labs usan correctamente `grep -c '^Z'`.

### Error: etiqueta "handler" para SIG_IGN

En README.md, la sección de prevención en C dice:

```c
// Opción 2: handler para SIGCHLD
signal(SIGCHLD, SIG_IGN);
```

SIG_IGN no es un "handler" — es **ignorar** la señal. El efecto real es que el
kernel hace auto-reap (no crea zombie). La opción 3 del mismo código sí es un
handler real.

### Aclaración: re-parenting no siempre va a PID 1

Los labs dicen "No importa cuántos niveles había — siempre va a PID 1". Esto
es una simplificación. Desde Linux 3.4, si un ancestro del proceso se ha
declarado **subreaper** (`prctl(PR_SET_CHILD_SUBREAPER, 1)`), el huérfano es
re-parenteado a ese subreaper, no a PID 1:

```
Cadena:  PID 1 → A (subreaper) → B → C → D

Si B muere:
  C y D → re-parenteados a A (no a PID 1)

Sin subreaper:
  C y D → re-parenteados a PID 1
```

En la práctica: systemd, Docker/containerd, y los runtimes de contenedores
usan subreapers. En un shell normal sin contenedores, sí va a PID 1.

### Aclaración: SIGCHLD — disposición por defecto

La disposición por defecto de SIGCHLD es **ignorar** (`SIG_DFL` = ignore para
SIGCHLD). Pero "ignorar SIGCHLD por defecto" y "establecer SIG_IGN
explícitamente" tienen efectos **diferentes** en Linux:

| Disposición | Comportamiento |
|---|---|
| SIG_DFL (defecto) | Señal ignorada, pero el zombie **sí se crea** — el padre debe hacer `wait()` |
| `signal(SIGCHLD, SIG_IGN)` | Señal ignorada y el kernel **auto-reap** — no se crea zombie |

Es una distinción sutil pero importante en programación de sistemas.

---

## Ciclo de vida de un proceso

```
fork()          El padre crea una copia de sí mismo (el hijo)
  │
  ├── Padre     Continúa ejecutando
  │
  └── Hijo      exec() para reemplazar su código por otro programa
                  │
                  └── exit()  El hijo termina
                        │
                        └── El kernel guarda el exit status
                              │
                              └── wait()  El padre recoge el exit status
                                    │
                                    └── El kernel libera la entrada del proceso
```

El paso crítico es el último: el padre **debe** llamar `wait()` (o `waitpid()`)
para recoger el exit status del hijo. Si no lo hace, el hijo queda como zombie.

## Procesos zombie

Un **zombie** es un proceso que ya terminó su ejecución pero cuyo padre aún no
ha recogido su exit status con `wait()`:

```
Estado del zombie:
- Ya NO ejecuta código
- Ya NO consume CPU
- Ya NO tiene memoria asignada (el kernel la liberó)
- PERO sigue ocupando una entrada en la tabla de procesos (~1KB)
- PERO su PID sigue reservado
- Su estado en ps es Z (zombie) o Z+ (zombie en foreground group)
```

### Cómo se ven

```bash
ps aux | grep Z
# USER  PID %CPU %MEM VSZ RSS TTY STAT START TIME COMMAND
# dev  2345  0.0  0.0   0   0 ?   Z    14:30 0:00 [myapp] <defunct>
#                        ^   ^      ^          ^^^^^^^^^^^
#                        sin memoria  zombie    etiqueta

# Más preciso: buscar estado Z con ancla
ps -eo pid,ppid,stat,cmd | awk '$3 ~ /^Z/'
# 2345  2300 Z+  [myapp] <defunct>
#       ^^^^
#       PPID — este es el proceso con el bug
```

### Por qué existen

El exit status existe para que el padre pueda saber **cómo terminó** el hijo:

```c
// El padre necesita saber:
// ¿Terminó normalmente? → WIFEXITED(status) + WEXITSTATUS(status)
// ¿Fue matado por señal? → WIFSIGNALED(status) + WTERMSIG(status)
// ¿Cuál fue su exit code? (0 = éxito, != 0 = error)

// Si el kernel liberara la entrada inmediatamente al morir el hijo,
// el padre no podría obtener esta información.
// Por eso el kernel espera a que el padre haga wait().
```

En bash, esto se traduce a `$?` y al builtin `wait`:

```bash
# $? = exit status del último comando foreground
# wait $PID → espera y devuelve el exit status del background job
bash -c 'exit 42' &
wait $!
echo $?    # 42
```

### Cuándo son un problema

Un zombie individual es inofensivo. El problema es cuando se acumulan:

```bash
# La tabla de procesos tiene un límite
cat /proc/sys/kernel/pid_max
# 4194304 (default en kernels modernos 64-bit, era 32768 antes)

# Miles de zombies pueden:
# - Agotar PIDs disponibles (especialmente con pid_max bajo)
# - Indicar un bug serio en el padre (no maneja SIGCHLD)
# - Dificultar el monitoreo (ruido en ps/top)
# - En contenedores con límite de PIDs (pids.max en cgroup),
#   causar que no se puedan crear nuevos procesos
```

### Cómo solucionarlos

Un zombie **no se puede matar** con `kill -9` — ya está muerto. La solución
es actuar sobre el **padre**:

```bash
# 1. Identificar al padre del zombie
ps -o ppid= -p <PID_ZOMBIE>

# 2. Opción A: enviar SIGCHLD al padre (pedirle que haga wait)
kill -CHLD <PPID>
# Solo funciona si el padre tiene un handler para SIGCHLD
# (bash sí lo tiene; muchos programas no)

# 3. Opción B: matar al padre
kill <PPID>
# El zombie queda huérfano → adoptado por PID 1 (o subreaper)
# → PID 1 hace wait() → zombie desaparece

# 4. Si el padre es un servicio crítico que no puedes matar:
#    es un bug en el servicio — debe arreglarse el código
```

### Cómo prevenirlos (en código C)

```c
// Opción 1: wait() explícito después de fork()
waitpid(pid, &status, 0);  // bloquea hasta que el hijo termine

// Opción 2: auto-reap con SIG_IGN (Linux-específico)
signal(SIGCHLD, SIG_IGN);  // kernel auto-reap, no crea zombies

// Opción 3: handler que hace wait no-bloqueante
void sigchld_handler(int sig) {
    // WNOHANG: no bloquear, recoger TODOS los hijos terminados
    while (waitpid(-1, NULL, WNOHANG) > 0);
}
signal(SIGCHLD, sigchld_handler);

// Opción 4: doble fork (ver sección dedicada más abajo)
```

### Cómo prevenirlos (en shell scripts)

```bash
# En bash, wait es el equivalente de waitpid():
cmd &
PID=$!
# ... hacer otras cosas ...
wait $PID            # recoge exit status, previene zombie
echo "Exit: $?"

# Esperar TODOS los hijos:
cmd1 &
cmd2 &
cmd3 &
wait                 # espera los 3

# Si no necesitas el exit status, al menos haz wait al final:
cleanup() { wait; }
trap cleanup EXIT
```

## Procesos huérfanos

Un **huérfano** es un proceso cuyo padre terminó antes que él:

```
Antes:
  padre (PID 2300)
    └── hijo (PID 2345) ← ejecutando

padre muere → hijo queda huérfano

Después:
  PID 1 (systemd/init) o subreaper
    └── hijo (PID 2345) ← re-parenteado
```

### Re-parenting

Cuando un padre muere, el kernel **re-parentea** a todos sus hijos al ancestro
subreaper más cercano, o a PID 1 si no hay subreaper:

```bash
# Demostrar re-parenting
bash -c 'sleep 300 & echo "hijo PID=$!"; exit' &
# El subshell muere, sleep queda huérfano

# Verificar
ps -o pid,ppid,cmd -p $(pgrep -f 'sleep 300')
# PID  PPID CMD
# 2345    1 sleep 300
#         ^
#         re-parenteado a PID 1
```

Los huérfanos **no son un problema** — es el mecanismo normal de Linux.
`nohup`, `disown` y `setsid` crean huérfanos intencionalmente. PID 1
automáticamente hace `wait()` por estos huérfanos cuando terminan, así que
no se convierten en zombies.

### Subreaper

Desde Linux 3.4, un proceso puede declararse **subreaper** — adopta huérfanos
de sus descendientes en lugar de PID 1:

```c
// El proceso se declara subreaper
prctl(PR_SET_CHILD_SUBREAPER, 1);
// Ahora, cualquier descendiente huérfano es re-parenteado a ESTE proceso
```

```bash
# ¿Quién usa subreapers?
# - systemd: gestiona los servicios, recoge zombies de daemons
#   que hacen doble fork
# - Docker/containerd: el runtime del contenedor recoge zombies
#   del PID 1 del contenedor
# - PID 1 del contenedor (si usa tini/dumb-init)

# bash NO es un subreaper
```

## Doble fork (técnica clásica de daemons)

El doble fork evita que un daemon cree zombies o adquiera un terminal de
control:

```
Abuelo (proceso original)
  │
  └── fork() → Hijo intermedio
                  │
                  └── fork() → Nieto (daemon real)
                  │               │
                  exit(0)         └── ejecuta independiente
                  │
        Abuelo hace wait(hijo)
        → no zombie del hijo

        Nieto es huérfano
        → re-parenteado a PID 1
        → cuando nieto termine, PID 1 hace wait()
        → no zombie del nieto
```

El resultado: nadie queda como zombie, y el daemon no tiene terminal de control
(no puede adquirir uno accidentalmente porque no es session leader).

```bash
# En shell, el doble fork es simplemente:
bash -c 'sleep 300 & exit' &
wait $!
# El hijo intermedio (bash -c) sale inmediatamente → wait lo recoge
# El nieto (sleep) queda huérfano → PID 1 lo adopta
```

> Esta técnica está mayormente **obsoleta** gracias a systemd, que gestiona
> el ciclo de vida completo de los servicios. Pero sigue siendo un concepto
> fundamental para entender daemons legacy.

## Zombies en contenedores

El problema de zombies es especialmente relevante en contenedores porque
PID 1 dentro del contenedor puede no estar preparado para hacer `wait()`:

```bash
# Si el entrypoint es tu aplicación directamente:
# CMD ["python3", "app.py"]
# python3 es PID 1 → si algún hijo muere, python3 debe hacer wait()
# Si no lo hace → zombies se acumulan

# Agravado por el límite de PIDs en cgroups:
# Si pids.max = 100 y hay 50 zombies, solo quedan 50 PIDs útiles

# Soluciones:
# 1. docker run --init (usa tini como PID 1)
docker run --init myimage

# 2. Usar tini explícitamente en el Dockerfile
# ENTRYPOINT ["/usr/bin/tini", "--"]
# CMD ["python3", "app.py"]

# 3. signal(SIGCHLD, SIG_IGN) en la aplicación
```

> Recordar de T01_Señales: PID 1 en Linux solo recibe señales para las que
> tiene handler registrado. Esto también aplica a SIGCHLD — si PID 1 no
> maneja SIGCHLD, los zombies se acumulan sin remedio.

## /proc de un zombie

Un zombie tiene la mayoría de su información en `/proc` eliminada o vacía:

```bash
# Lo que SÍ queda:
/proc/PID/status     # State: Z (zombie), PID, PPID
/proc/PID/stat       # Mínimo: PID, comm, Z, PPID...
/proc/PID/comm       # Nombre del comando (max 15 chars)

# Lo que desaparece:
/proc/PID/cmdline    # Vacío
/proc/PID/maps       # Vacío (sin mappings de memoria)
/proc/PID/fd/        # Vacío (sin file descriptors)
/proc/PID/exe        # Error o enlace roto
# VmSize, VmRSS      # No aparecen en status (sin memoria)
```

## Herramientas de diagnóstico

```bash
# Contar zombies
ps -eo stat --no-headers | grep -c '^Z'

# Listar zombies con su padre
ps -eo pid,ppid,stat,cmd | awk '$3 ~ /^Z/'

# Ver cadena zombie → padre → abuelo
pstree -sp <PID_ZOMBIE>

# En top: línea "Tasks" muestra el conteo de zombies
# Tasks: 231 total, 1 running, 229 sleeping, 0 stopped, 1 zombie

# Procedimiento completo:
# 1. Detectar:    ps -eo stat --no-headers | grep -c '^Z'
# 2. Listar:      ps -eo pid,ppid,stat,cmd | awk '$3 ~ /^Z/'
# 3. Investigar:  ps -o pid,cmd -p <PPID>
# 4. Solucionar:  kill -CHLD <PPID>  o  kill <PPID>
# 5. Prevenir:    arreglar el código o usar --init en contenedores
```

## Resumen

| Tipo | Qué pasó | Estado | Padre | Peligro | Solución |
|---|---|---|---|---|---|
| Zombie | Hijo terminó, padre no hizo `wait()` | Z (muerto) | Vivo (con bug) | Acumulación → PIDs agotados | Arreglar padre o matarlo |
| Huérfano | Padre terminó antes que el hijo | S/R (vivo) | Re-parenteado a PID 1 | Ninguno | No necesita solución |

---

## Labs

### Parte 1 — Procesos zombie

#### Paso 1.1: Ciclo de vida normal

```bash
echo "=== Ciclo de vida normal ==="
echo "1. fork()  — el padre crea un hijo"
echo "2. exec()  — el hijo ejecuta un programa"
echo "3. exit()  — el hijo termina"
echo "4. wait()  — el padre recoge el exit status"
echo "5. kernel libera la entrada del proceso"
echo ""
echo "Si el paso 4 no ocurre → el hijo queda como ZOMBIE"
echo ""
echo "Zombies actuales:"
ps -eo stat --no-headers | grep -c '^Z'
```

#### Paso 1.2: Crear un zombie

```bash
bash -c '
    bash -c "exit 0" &
    CHILD=$!
    echo "Hijo PID: $CHILD, Padre PID: $$"
    sleep 30
' &
PARENT=$!
sleep 1

echo "=== Buscar zombie ==="
ps --ppid $PARENT -o pid,ppid,stat,cmd --no-headers

echo ""
echo "El hijo terminó (estado Z) pero el padre no hizo wait()"
echo "El zombie ocupa una entrada en la tabla de procesos"
echo "pero NO consume CPU ni memoria"

kill $PARENT 2>/dev/null
wait $PARENT 2>/dev/null
```

> Después de matar al padre, el zombie desaparece: queda huérfano, PID 1
> lo adopta y hace `wait()`.

#### Paso 1.3: Un zombie NO se puede matar

```bash
bash -c '
    bash -c "exit 0" &
    CHILD=$!
    echo "ZOMBIE=$CHILD"
    sleep 60
' &
PARENT=$!
sleep 1

ZOMBIE=$(ps --ppid $PARENT -o pid= --no-headers | head -1 | tr -d ' ')
echo "Zombie PID: $ZOMBIE"
echo ""

echo "=== Intentar kill -9 ==="
kill -9 $ZOMBIE 2>&1
sleep 0.5
echo "¿Sigue ahí?"
ps -o pid,stat,cmd -p $ZOMBIE 2>/dev/null && echo "Sí — no se puede matar lo que ya está muerto"

kill $PARENT 2>/dev/null
wait $PARENT 2>/dev/null
```

#### Paso 1.4: Verificar que matar al padre limpia el zombie

```bash
bash -c '
    for i in 1 2 3; do
        bash -c "exit 0" &
    done
    sleep 60
' &
PARENT=$!
sleep 1

echo "Antes de matar al padre:"
echo "Zombies: $(ps -eo stat --no-headers | grep -c '^Z')"

kill $PARENT
wait $PARENT 2>/dev/null
sleep 0.5

echo "Después de matar al padre:"
echo "Zombies: $(ps -eo stat --no-headers | grep -c '^Z')"
echo "(PID 1 adoptó a los zombies y hizo wait())"
```

### Parte 2 — Procesos huérfanos

#### Paso 2.1: Crear un huérfano

```bash
bash -c 'sleep 120 & echo "hijo PID=$!"; exit' &
sleep 1
wait $! 2>/dev/null

CHILD=$(pgrep -nf "sleep 120")
if [ -n "$CHILD" ]; then
    echo "Hijo PID: $CHILD"
    ps -o pid,ppid,stat,cmd -p $CHILD
    echo ""
    echo "PPID = 1 → re-parenteado a PID 1"
fi

pkill -f "sleep 120" 2>/dev/null
```

#### Paso 2.2: Los huérfanos son normales

```bash
echo "=== Zombie vs Huérfano ==="
echo ""
echo "Zombie:   hijo MUERTO, padre VIVO pero no hace wait()"
echo "          → entrada bloqueada en tabla de procesos"
echo "          → PROBLEMA (si se acumulan)"
echo ""
echo "Huérfano: hijo VIVO, padre MUERTO"
echo "          → re-parenteado a PID 1 (o subreaper)"
echo "          → PID 1 hará wait() cuando termine"
echo "          → NORMAL (nohup, disown, setsid crean huérfanos)"
```

### Parte 3 — Diagnóstico

#### Paso 3.1: Contar zombies

```bash
echo "=== Métodos para contar zombies ==="
echo ""
echo "1. ps:"
echo "   Zombies: $(ps -eo stat --no-headers | grep -c '^Z')"
echo ""
echo "2. top (línea Tasks):"
top -bn1 | head -2
echo ""
echo "3. /proc/loadavg (último campo = running/total):"
cat /proc/loadavg
```

#### Paso 3.2: Crear zombies y diagnosticar

```bash
bash -c '
    for i in 1 2 3; do
        bash -c "exit 0" &
    done
    echo "Padre defectuoso PID: $$"
    sleep 30
' &
BAD_PARENT=$!
sleep 1

echo "=== Zombies y su padre ==="
ps -eo pid,ppid,stat,cmd | awk '$3 ~ /^Z/'

echo ""
echo "=== Padre defectuoso ==="
ps -o pid,cmd -p $BAD_PARENT

echo ""
echo "=== Cadena con pstree ==="
pstree -sp $BAD_PARENT 2>/dev/null || pstree -p $BAD_PARENT 2>/dev/null

kill $BAD_PARENT
wait $BAD_PARENT 2>/dev/null
sleep 0.5
echo ""
echo "Zombies después de matar padre: $(ps -eo stat --no-headers | grep -c '^Z')"
```

---

## Ejercicios

### Ejercicio 1 — Crear y observar un zombie

```bash
# Crear un zombie: hijo que muere, padre que no hace wait()
bash -c '
    bash -c "exit 0" &
    CHILD=$!
    echo "Hijo PID: $CHILD, Padre PID: $$"
    sleep 60
' &
PARENT=$!
sleep 1

# a) ¿Hay un zombie?
ps --ppid $PARENT -o pid,ppid,stat,cmd --no-headers

# b) ¿Cuánta memoria consume?
ps --ppid $PARENT -o pid,vsz,rss,stat --no-headers

# Limpieza
kill $PARENT 2>/dev/null
wait $PARENT 2>/dev/null
```

<details><summary>Predicción</summary>

a) Sí, hay un proceso con estado `Z` (o `Z+`). El hijo hizo `exit(0)` pero
el padre está en `sleep 60` sin hacer `wait()`. El comando aparece como
`[bash] <defunct>`.

b) VSZ y RSS son **0**. El zombie no tiene memoria asignada — el kernel la
liberó al morir. Solo ocupa una entrada en la tabla de procesos (~1KB en
estructuras internas del kernel, no visible como RSS).

</details>

### Ejercicio 2 — kill -9 no mata un zombie

```bash
bash -c '
    bash -c "exit 0" &
    CHILD=$!
    echo "ZOMBIE_PID=$CHILD"
    sleep 60
' &
PARENT=$!
sleep 1

ZOMBIE=$(ps --ppid $PARENT -o pid= --no-headers | head -1 | tr -d ' ')
echo "Zombie: $ZOMBIE"

# a) Intentar matarlo con kill -9
kill -9 $ZOMBIE 2>&1

# b) ¿Sigue ahí?
sleep 0.5
ps -o pid,stat -p $ZOMBIE 2>/dev/null

# c) ¿Y con kill -0?
kill -0 $ZOMBIE 2>&1

# Limpieza
kill $PARENT 2>/dev/null
wait $PARENT 2>/dev/null
```

<details><summary>Predicción</summary>

a) `kill -9` no da error (el proceso existe en la tabla), pero no tiene efecto.
No puedes matar lo que ya está muerto — `exit()` ya ocurrió.

b) El zombie **sigue ahí** con estado Z. SIGKILL solo puede actuar sobre
procesos que ejecutan código. Un zombie no ejecuta nada.

c) `kill -0` retorna éxito (exit 0). Esto significa que el PID existe y
tenemos permiso para enviarle señales. Pero que exista no significa que esté
vivo — un zombie "existe" en la tabla de procesos pero está muerto.

La única forma de eliminarlo es actuar sobre el **padre** (SIGCHLD o matarlo).

</details>

### Ejercicio 3 — Limpiar zombies matando al padre

```bash
# Crear 5 zombies con un padre defectuoso
bash -c '
    for i in 1 2 3 4 5; do
        bash -c "exit $i" &
    done
    echo "Padre PID: $$"
    sleep 60
' &
PARENT=$!
sleep 1

# a) Contar zombies
echo "Zombies antes:"
ps -eo stat --no-headers | grep -c '^Z'
ps --ppid $PARENT -o pid,stat --no-headers

# b) Matar al padre
kill $PARENT
wait $PARENT 2>/dev/null
sleep 0.5

# c) ¿Cuántos zombies quedan?
echo "Zombies después:"
ps -eo stat --no-headers | grep -c '^Z'
```

<details><summary>Predicción</summary>

a) 5 zombies, todos con PPID = $PARENT.

b-c) Después de matar al padre: **0 zombies**. La cadena es:
1. El padre muere
2. Los 5 zombies quedan huérfanos
3. PID 1 (o subreaper) los adopta (PPID cambia a 1)
4. PID 1 hace `wait()` por cada uno
5. El kernel libera sus entradas de la tabla de procesos

Este es el mecanismo de seguridad de Linux: PID 1 SIEMPRE hace `wait()` por
los procesos que adopta, garantizando que los zombies huérfanos se limpian.

</details>

### Ejercicio 4 — SIGCHLD al padre

```bash
# Crear un zombie
bash -c '
    bash -c "exit 0" &
    CHILD=$!
    echo "Zombie PID: $CHILD, Padre PID: $$"
    sleep 60
' &
PARENT=$!
sleep 1

# a) Verificar que existe el zombie
echo "Antes de SIGCHLD:"
ps --ppid $PARENT -o pid,stat --no-headers

# b) Enviar SIGCHLD al padre
kill -CHLD $PARENT
sleep 0.5

# c) ¿Desapareció el zombie?
echo "Después de SIGCHLD:"
ps --ppid $PARENT -o pid,stat --no-headers 2>/dev/null | grep Z || echo "  (sin zombies)"

# Limpieza
kill $PARENT 2>/dev/null
wait $PARENT 2>/dev/null
```

<details><summary>Predicción</summary>

a) Hay un zombie con estado Z.

b-c) El zombie **desaparece**. Bash tiene un handler interno para SIGCHLD que
llama `waitpid()` internamente. Al recibir SIGCHLD, bash recoge el exit status
del hijo muerto y el zombie se limpia.

**Pero cuidado**: esto funciona porque bash tiene un handler para SIGCHLD.
Muchos programas (especialmente en C) no lo tienen. En ese caso, SIGCHLD tiene
disposición por defecto (ignore), y `kill -CHLD` no tiene efecto — no
resuelve el zombie.

Regla práctica: probar `kill -CHLD` primero. Si no funciona, matar al padre.

</details>

### Ejercicio 5 — Observar re-parenting a PID 1

```bash
# a) Crear un huérfano intencionalmente
bash -c 'sleep 120 & echo "hijo=$!"; exit' &
sleep 1
wait $! 2>/dev/null

# b) ¿Quién es el padre del huérfano ahora?
CHILD=$(pgrep -nf "sleep 120")
echo "Hijo PID: $CHILD"
echo "PPID: $(ps -o ppid= -p $CHILD | tr -d ' ')"

# c) ¿Es un zombie?
echo "Estado: $(ps -o stat= -p $CHILD)"

# d) Ver con pstree
pstree -sp $CHILD 2>/dev/null

# Limpieza
pkill -f "sleep 120"
```

<details><summary>Predicción</summary>

b) PPID = 1 (o el PID del subreaper, si hay uno). El padre original (`bash -c`)
hizo `exit`, así que el kernel re-parenteó al hijo (`sleep 120`) al ancestro
subreaper más cercano, o a PID 1.

c) Estado: `S` (sleeping). El huérfano **no** es un zombie — está vivo y
ejecutando. Huérfano ≠ zombie:
- Zombie: hijo **muerto**, padre vivo que no hizo `wait()`
- Huérfano: hijo **vivo**, padre muerto → re-parenteado

d) `pstree` muestra la cadena desde PID 1 hasta el sleep, confirmando la
adopción.

Los huérfanos son el mecanismo normal de `nohup`, `disown` y `setsid`.

</details>

### Ejercicio 6 — Múltiples zombies: diagnóstico con pstree

```bash
# Crear un "servidor defectuoso" que genera zombies
bash -c '
    for i in $(seq 1 5); do
        bash -c "exit $i" &
    done
    echo "PADRE_DEFECTUOSO=$$"
    sleep 60
' &
BAD=$!
sleep 1

# a) ¿Cuántos zombies hay en total en el sistema?
echo "Total zombies: $(ps -eo stat --no-headers | grep -c '^Z')"

# b) Listar zombies con su padre
echo ""
echo "Zombies y su padre:"
ps -eo pid,ppid,stat,cmd | awk '$3 ~ /^Z/'

# c) Identificar al culpable
echo ""
echo "Proceso padre defectuoso:"
ps -o pid,ppid,stat,cmd -p $BAD

# d) Ver la cadena completa
echo ""
pstree -sp $BAD 2>/dev/null

# Limpieza
kill $BAD
wait $BAD 2>/dev/null
```

<details><summary>Predicción</summary>

a) Al menos 5 zombies (podrían haber más si el sistema ya tenía alguno).

b) Los 5 zombies tienen el mismo PPID — apuntando al padre defectuoso. En
producción, este patrón (múltiples zombies con el mismo PPID) identifica
inmediatamente al proceso con el bug.

c) El padre tiene estado `S` (sleeping en el `sleep 60`). Está vivo pero
no recoge a sus hijos.

d) `pstree -sp` muestra el árbol desde init hasta el padre, con los zombies
como hojas. El flag `-s` muestra ancestros, `-p` muestra PIDs.

Procedimiento de diagnóstico: contar → listar con PPID → investigar padre →
decidir si `kill -CHLD` o matar padre.

</details>

### Ejercicio 7 — /proc de un zombie

```bash
# Crear un zombie
bash -c '
    bash -c "exit 42" &
    CHILD=$!
    echo "ZOMBIE=$CHILD"
    sleep 60
' &
PARENT=$!
sleep 1

ZOMBIE=$(ps --ppid $PARENT -o pid= --no-headers | head -1 | tr -d ' ')
echo "Zombie PID: $ZOMBIE"
echo ""

# a) Estado en /proc
echo "=== State y PIDs ==="
grep -E '^(Name|State|Pid|PPid)' /proc/$ZOMBIE/status 2>/dev/null

# b) ¿Tiene memoria?
echo ""
echo "=== Memoria ==="
grep -E '^(VmSize|VmRSS)' /proc/$ZOMBIE/status 2>/dev/null || echo "  (sin campos Vm — no tiene memoria)"

# c) ¿Tiene cmdline?
echo ""
echo "=== cmdline ==="
CMDLINE=$(cat /proc/$ZOMBIE/cmdline 2>/dev/null)
[ -z "$CMDLINE" ] && echo "  (vacío)" || echo "  $CMDLINE"

# d) ¿Tiene comm?
echo ""
echo "=== comm ==="
cat /proc/$ZOMBIE/comm 2>/dev/null || echo "  (inaccesible)"

# e) ¿Tiene maps?
echo ""
echo "=== maps ==="
MAPS=$(wc -l < /proc/$ZOMBIE/maps 2>/dev/null)
echo "  Líneas en maps: ${MAPS:-0}"

# Limpieza
kill $PARENT 2>/dev/null
wait $PARENT 2>/dev/null
```

<details><summary>Predicción</summary>

a) `State: Z (zombie)`, Pid y PPid presentes. El nombre (`Name:`) sigue visible.

b) Los campos `VmSize` y `VmRSS` **no aparecen**. El kernel liberó toda la
memoria del proceso. Esto confirma que un zombie no consume RAM.

c) `cmdline` está **vacío**. La línea de comando completa se almacenaba en la
memoria del proceso, que ya fue liberada.

d) `comm` **sí existe** — muestra "bash". El nombre del comando (max 15 chars)
se almacena en la estructura `task_struct` del kernel, no en la memoria del
proceso. Por eso `ps` puede mostrarlo como `[bash] <defunct>`.

e) `maps` tiene **0 líneas**. Sin mappings de memoria.

Un zombie es literalmente una entrada en la tabla de procesos del kernel con
el mínimo de metadatos: PID, PPID, exit status, nombre. Todo lo demás fue
liberado.

</details>

### Ejercicio 8 — wait previene zombies en scripts

```bash
# a) SIN wait: padre duerme sin recoger hijos → zombie
echo "=== Sin wait ==="
bash -c '
    bash -c "exit 0" &
    CHILD=$!
    sleep 5
' &
PARENT_NO_WAIT=$!
sleep 1
echo "Zombie?"
ps --ppid $PARENT_NO_WAIT -o pid,stat --no-headers 2>/dev/null
kill $PARENT_NO_WAIT 2>/dev/null
wait $PARENT_NO_WAIT 2>/dev/null

echo ""

# b) CON wait: padre recoge al hijo → sin zombie
echo "=== Con wait ==="
bash -c '
    bash -c "exit 0" &
    CHILD=$!
    wait $CHILD
    echo "Exit status del hijo: $?"
    sleep 5
' &
PARENT_WAIT=$!
sleep 1
echo "Zombie?"
ps --ppid $PARENT_WAIT -o pid,stat --no-headers 2>/dev/null | grep Z || echo "  (sin zombies)"
kill $PARENT_WAIT 2>/dev/null
wait $PARENT_WAIT 2>/dev/null
```

<details><summary>Predicción</summary>

a) **Sí hay zombie** (estado Z). El padre lanzó un hijo con `&` y se fue a
dormir con `sleep 5` sin hacer `wait`. El hijo terminó pero su exit status
nunca fue recogido.

b) **No hay zombie**. `wait $CHILD` recogió el exit status (0) del hijo. El
kernel liberó la entrada del proceso. Solo queda el `sleep 5` como hijo del
padre (con estado S, no Z).

Regla para shell scripts: si haces `cmd &`, siempre haz `wait` en algún
momento (al menos al final del script). Esto es el equivalente en shell del
`waitpid()` de C.

</details>

### Ejercicio 9 — Doble fork: técnica de daemons

```bash
# a) Fork simple: el hijo queda vinculado al shell
sleep 500 &
PID_SIMPLE=$!
echo "Simple: PID=$PID_SIMPLE, PPID=$(ps -o ppid= -p $PID_SIMPLE | tr -d ' ')"

# b) Doble fork: el nieto queda adoptado por PID 1
bash -c 'sleep 501 & exit' &
wait $!
sleep 0.5
PID_DOUBLE=$(pgrep -nf "sleep 501")
echo "Doble:  PID=$PID_DOUBLE, PPID=$(ps -o ppid= -p $PID_DOUBLE | tr -d ' ')"

# c) ¿Quién es el session leader de cada uno?
echo ""
echo "SID del shell:    $(ps -o sid= -p $$ | tr -d ' ')"
echo "SID del simple:   $(ps -o sid= -p $PID_SIMPLE | tr -d ' ')"
echo "SID del doble:    $(ps -o sid= -p $PID_DOUBLE | tr -d ' ')"

# d) ¿Zombies?
echo ""
echo "Zombies en el sistema: $(ps -eo stat --no-headers | grep -c '^Z')"

# Limpieza
kill $PID_SIMPLE $PID_DOUBLE 2>/dev/null
wait 2>/dev/null
```

<details><summary>Predicción</summary>

a) PPID del simple = PID del shell ($$). Sigue vinculado.

b) PPID del doble = 1 (PID 1). El hijo intermedio (`bash -c`) hizo `exit`,
nuestro shell hizo `wait $!` (recogió al hijo intermedio, sin zombie), y el
nieto (`sleep 501`) quedó huérfano, adoptado por PID 1.

c) Ambos comparten el mismo **SID** que el shell. El doble fork por sí solo
no crea nueva sesión — para eso se necesita `setsid`. El doble fork solo
desvincula del padre, no de la sesión.

d) 0 zombies. El hijo intermedio fue recogido con `wait $!`. El nieto está
vivo (no es zombie). Nadie queda sin recoger.

En un daemon real, el doble fork se combina con `setsid()` entre los dos
forks para crear una nueva sesión sin terminal de control.

</details>

### Ejercicio 10 — pid_max y el límite de zombies

```bash
# a) ¿Cuál es el límite de PIDs?
echo "pid_max: $(cat /proc/sys/kernel/pid_max)"

# b) ¿Cuántos PIDs están en uso?
echo "PIDs en uso: $(ls -d /proc/[0-9]* 2>/dev/null | wc -l)"

# c) Crear zombies y contar
bash -c '
    for i in $(seq 1 10); do
        bash -c "exit 0" &
    done
    echo "PADRE=$$"
    sleep 30
' &
PARENT=$!
sleep 1

echo ""
echo "Zombies creados: $(ps --ppid $PARENT -o stat= --no-headers | grep -c Z)"
echo "PIDs en uso ahora: $(ls -d /proc/[0-9]* 2>/dev/null | wc -l)"

# d) ¿Cuánto "pesan" los zombies?
echo ""
echo "Memoria total de zombies (RSS):"
ps --ppid $PARENT -o rss= --no-headers | awk '{sum+=$1} END{print sum " KB"}'

# e) En contenedores con pids.max limitado:
echo ""
echo "En un contenedor con pids.max=100:"
echo "  10 zombies = 10% de los PIDs desperdiciados"
echo "  Los procesos reales no podrán crear nuevos hijos"

# Limpieza
kill $PARENT 2>/dev/null
wait $PARENT 2>/dev/null
```

<details><summary>Predicción</summary>

a) pid_max típico: 4194304 en kernels modernos 64-bit (antes era 32768).

b) Varía según el sistema — decenas a cientos de procesos.

c) 10 zombies creados. Los PIDs en uso aumentaron en ~10.

d) 0 KB. Los zombies tienen RSS = 0 — no consumen memoria de usuario. Su
costo real es la entrada en la tabla de procesos del kernel (~1KB por
proceso en `task_struct`, no visible como RSS).

e) En contenedores, el límite `pids.max` del cgroup restringe el total de
procesos. Los zombies cuentan contra este límite porque ocupan un PID. Con
un límite bajo (50-100), unos pocos zombies pueden impedir la creación de
procesos nuevos → el servicio deja de funcionar.

Por eso `docker run --init` es importante: tini como PID 1 hace `wait()` por
todos los hijos, previniendo la acumulación de zombies.

</details>
