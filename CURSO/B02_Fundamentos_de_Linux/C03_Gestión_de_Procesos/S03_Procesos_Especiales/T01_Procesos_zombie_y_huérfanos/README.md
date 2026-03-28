# T01 — Procesos zombie y huérfanos

## Ciclo de vida de un proceso

Para entender zombies y huérfanos hay que entender cómo Linux gestiona el
ciclo de vida de los procesos:

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
- PERO sigue ocupando una entrada en la tabla de procesos
- PERO su PID sigue reservado
- Su estado en ps es Z (zombie) o Z+ (zombie en foreground group)
```

### Cómo se ven

```bash
ps aux | grep Z
# USER  PID %CPU %MEM VSZ RSS TTY STAT START TIME COMMAND
# dev  2345  0.0  0.0   0   0 ?   Z    14:30 0:00 [myapp] <defunct>
#                        ^   ^      ^          ^^^^^^^^^^^
#                        sin memoria  zombie    etiqueta de zombie

# Con ps -eo
ps -eo pid,ppid,stat,cmd | awk '$3 ~ /Z/'
# 2345  2300 Z+  [myapp] <defunct>
#       ^^^^
#       PID del padre — este es el proceso que tiene el bug
```

### Por qué existen

El exit status existe para que el padre pueda saber **cómo terminó** el hijo:

```bash
# El padre necesita saber:
# - ¿Terminó normalmente? → WIFEXITED() + WEXITSTATUS()
# - ¿Fue matado por una señal? → WIFSIGNALED() + WTERMSIG()
# - ¿Cuál fue su exit code? (0 = éxito, != 0 = error)

# Si el kernel liberara la entrada inmediatamente al morir el hijo,
# el padre no podría obtener esta información
# Por eso el kernel espera a que el padre haga wait()
```

### Cuándo son un problema

Un zombie individual es inofensivo — solo ocupa ~1KB en la tabla de procesos.
El problema es cuando se acumulan:

```bash
# La tabla de procesos tiene un límite
cat /proc/sys/kernel/pid_max
# 4194304 (default en kernels modernos, era 32768 antes)

# Miles de zombies pueden:
# - Agotar PIDs disponibles (en sistemas con pid_max bajo)
# - Indicar un bug serio en el padre (no maneja SIGCHLD)
# - Dificultar el monitoreo (ruido en ps/top)
# - En contenedores con límite de PIDs (pids.max), causar que
#   no se puedan crear nuevos procesos

# Ver cuántos zombies hay
ps -eo stat | grep -c Z
```

### Cómo solucionarlos

Un zombie **no se puede matar** con `kill -9` — ya está muerto. La solución
es actuar sobre el **padre**:

```bash
# 1. Identificar el padre del zombie
ps -o ppid= -p <PID_ZOMBIE>
# 2300

# 2. Opción A: enviar SIGCHLD al padre (pedirle que haga wait)
kill -CHLD 2300
# Funciona solo si el padre tiene un handler para SIGCHLD

# 3. Opción B: matar al padre
kill 2300
# El zombie queda huérfano → adoptado por PID 1 → PID 1 hace wait()
# → zombie desaparece

# 4. Si el padre es un servicio crítico que no puedes matar:
#    es un bug en el servicio — debe arreglarse el código
```

### Cómo se crean (ejemplo en C)

```c
// Padre que crea zombies: fork sin wait
#include <unistd.h>
#include <stdlib.h>

int main() {
    pid_t pid = fork();
    if (pid == 0) {
        // Hijo: termina inmediatamente
        exit(0);
    }
    // Padre: no hace wait(), duerme para siempre
    // El hijo queda como zombie
    while (1) sleep(1);
    return 0;
}
```

### Cómo prevenirlos (en código)

```c
// Opción 1: wait() explícito
waitpid(pid, &status, 0);

// Opción 2: handler para SIGCHLD
signal(SIGCHLD, SIG_IGN);  // el kernel auto-reap sin crear zombies

// Opción 3: handler que hace wait
void sigchld_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
    // WNOHANG: no bloquear, recoger todos los hijos terminados
}
signal(SIGCHLD, sigchld_handler);

// Opción 4: doble fork (el hijo intermedio muere,
// el nieto es adoptado por init que siempre hace wait)
```

## Procesos huérfanos

Un **huérfano** es un proceso cuyo padre terminó antes que él:

```
Antes:
  padre (PID 2300)
    └── hijo (PID 2345) ← ejecutando

padre muere → hijo queda huérfano

Después:
  PID 1 (systemd/init)
    └── hijo (PID 2345) ← re-parenteado a PID 1
```

### Re-parenting

Cuando un padre muere, el kernel **re-parentea** a todos sus hijos a PID 1
(init/systemd). PID 1 automáticamente hace `wait()` por estos huérfanos cuando
terminan, así que no se convierten en zombies:

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

Los huérfanos no son un problema — es el mecanismo normal de Linux. `nohup`,
`disown` y `setsid` crean huérfanos intencionalmente.

### Subreaper

Desde Linux 3.4, un proceso puede declararse **subreaper** — adopta huérfanos
en lugar de PID 1:

```bash
# prctl(PR_SET_CHILD_SUBREAPER, 1)
# Los descendientes huérfanos de este proceso son re-parenteados
# a él en lugar de a PID 1

# ¿Quién usa esto?
# - systemd: cada servicio tiene un subreaper (systemd se encarga
#   de recoger zombies de servicios que hacen doble fork)
# - Docker/containerd: el runtime del contenedor actúa como subreaper
# - bash NO es un subreaper
```

## Zombies en contenedores

El problema de zombies es especialmente relevante en contenedores porque
PID 1 dentro del contenedor puede no estar preparado para hacer `wait()`:

```bash
# Si el entrypoint del contenedor es tu aplicación directamente:
# CMD ["python3", "app.py"]
# python3 es PID 1 → si algún hijo muere, python3 debe hacer wait()
# Si python3 no lo hace, zombies se acumulan

# Agravado por el límite de PIDs en cgroups:
# Si pids.max = 100 y hay 50 zombies, solo quedan 50 PIDs útiles

# Soluciones:
# 1. docker run --init (usa tini como PID 1)
docker run --init myimage

# 2. Usar tini explícitamente en el Dockerfile
# ENTRYPOINT ["/usr/bin/tini", "--"]
# CMD ["python3", "app.py"]

# 3. Programar la aplicación para manejar SIGCHLD correctamente
```

## Herramientas de diagnóstico

```bash
# Contar zombies
ps -eo stat | grep -c '^Z'

# Listar zombies con su padre
ps -eo pid,ppid,stat,cmd | awk '$3 ~ /^Z/ {print}'

# Ver la cadena zombie → padre → abuelo
ZPID=2345
echo "Zombie: $ZPID"
echo "Padre: $(ps -o ppid= -p $ZPID)"
pstree -sp $ZPID

# En top: línea "Tasks" muestra el conteo de zombies
# Tasks: 231 total, 1 running, 229 sleeping, 0 stopped, 1 zombie
```

## Resumen

| Tipo | Qué pasó | Estado | Padre | Solución |
|---|---|---|---|---|
| Zombie | Hijo terminó, padre no hizo wait() | Z (muerto) | Vivo (con bug) | Arreglar el padre o matarlo |
| Huérfano | Padre terminó antes que el hijo | S/R (vivo) | Re-parenteado a PID 1 | No necesita solución |

---

## Ejercicios

### Ejercicio 1 — Crear y observar un zombie

```bash
# Crear un zombie temporal
bash -c '
    bash -c "exit 0" &   # hijo que muere inmediatamente
    CHILD=$!
    echo "Hijo PID: $CHILD"
    sleep 30              # padre no hace wait() por 30 segundos
' &
PARENT=$!

sleep 1
# ¿Hay un zombie?
ps -o pid,ppid,stat,cmd --ppid $PARENT

# Esperar a que el padre termine (auto-limpia al morir)
wait $PARENT
```

### Ejercicio 2 — Observar re-parenting

```bash
# Crear un huérfano
bash -c 'sleep 120 & echo "hijo=$!"; exit' &
sleep 1

# ¿Quién es el padre del sleep ahora?
ps -o pid,ppid,cmd -p $(pgrep -nf 'sleep 120')
# PPID debería ser 1

# Limpiar
pkill -f 'sleep 120'
```

### Ejercicio 3 — Contar zombies en el sistema

```bash
# ¿Cuántos zombies hay?
ZOMBIES=$(ps -eo stat | grep -c '^Z')
echo "Zombies: $ZOMBIES"

# Si hay alguno, ¿quién es el padre?
if [ "$ZOMBIES" -gt 0 ]; then
    echo "Zombies y sus padres:"
    ps -eo pid,ppid,stat,cmd | awk '$3 ~ /^Z/'
fi
```
