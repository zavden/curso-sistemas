# Lab — Procesos zombie y huerfanos

## Objetivo

Crear y observar procesos zombie y huerfanos: entender el ciclo
fork-exit-wait, identificar zombies con ps (estado Z), observar
re-parenting a PID 1, y diagnosticar padres defectuosos.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Procesos zombie

### Objetivo

Crear un zombie, observarlo, y entender por que existe y como
eliminarlo.

### Paso 1.1: Ciclo de vida normal

```bash
docker compose exec debian-dev bash -c '
echo "=== Ciclo de vida normal ==="
echo "1. fork()  — el padre crea un hijo"
echo "2. exec()  — el hijo ejecuta un programa"
echo "3. exit()  — el hijo termina"
echo "4. wait()  — el padre recoge el exit status"
echo "5. kernel libera la entrada del proceso"
echo ""
echo "Si el paso 4 no ocurre → el hijo queda como ZOMBIE"
echo ""
echo "=== Verificar: hay zombies ahora? ==="
ZOMBIES=$(ps -eo stat --no-headers | grep -c "^Z")
echo "Zombies actuales: $ZOMBIES"
'
```

### Paso 1.2: Crear un zombie

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear un zombie ==="
echo "El padre (bash -c) lanza un hijo que muere"
echo "pero el padre no hace wait() — duerme"
echo ""

bash -c '\''
    bash -c "exit 0" &
    CHILD=$!
    echo "Hijo PID: $CHILD"
    echo "Padre PID: $$"
    sleep 30
'\'' &
PARENT=$!
sleep 1

echo ""
echo "=== Buscar zombie ==="
ps -eo pid,ppid,stat,cmd | grep "Z" | grep -v grep
echo ""
echo "=== Todos los hijos del padre $PARENT ==="
ps --ppid $PARENT -o pid,ppid,stat,cmd 2>/dev/null

echo ""
echo "El hijo termino (estado Z) pero el padre no hizo wait()"
echo "El zombie ocupa una entrada en la tabla de procesos"
echo "pero NO consume CPU ni memoria"

kill $PARENT 2>/dev/null
wait 2>/dev/null
'
```

Predice antes de ejecutar: despues de matar al padre,
el zombie desaparecera?

### Paso 1.3: Un zombie NO se puede matar

```bash
docker compose exec debian-dev bash -c '
echo "=== No se puede matar un zombie ==="
echo "Un zombie ya esta muerto — exit() ya ocurrio"
echo "kill -9 no tiene efecto sobre un zombie"
echo "El zombie no es un proceso ejecutando — es una entrada"
echo "en la tabla de procesos esperando wait() del padre"
echo ""
echo "=== Como eliminar un zombie ==="
echo "Opcion 1: Enviar SIGCHLD al padre"
echo "  kill -CHLD <PPID>"
echo "  (solo funciona si el padre tiene handler para SIGCHLD)"
echo ""
echo "Opcion 2: Matar al padre"
echo "  kill <PPID>"
echo "  El zombie queda huerfano → adoptado por PID 1"
echo "  PID 1 hace wait() → zombie desaparece"
echo ""
echo "Opcion 3: Si el padre es critico"
echo "  Es un bug en el padre — debe arreglarse el codigo"
'
```

### Paso 1.4: Verificar que matar al padre limpia el zombie

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear zombie y limpiarlo matando al padre ==="
bash -c '\''
    bash -c "exit 0" &
    sleep 60
'\'' &
PARENT=$!
sleep 1

echo "Antes de matar al padre:"
echo "Zombies: $(ps -eo stat --no-headers | grep -c "^Z")"

kill $PARENT
wait $PARENT 2>/dev/null
sleep 1

echo "Despues de matar al padre:"
echo "Zombies: $(ps -eo stat --no-headers | grep -c "^Z")"
echo "(PID 1 adopto al zombie y hizo wait() — limpio)"
'
```

---

## Parte 2 — Procesos huerfanos

### Objetivo

Observar como el kernel re-parentea procesos cuando el padre muere.

### Paso 2.1: Crear un huerfano

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear un proceso huerfano ==="
bash -c '\''sleep 120 & echo "hijo PID=$!"; exit'\'' &
PARENT=$!
sleep 1
wait $PARENT 2>/dev/null

echo ""
echo "=== El padre ya murio, buscar al hijo ==="
CHILD=$(pgrep -nf "sleep 120")
if [ -n "$CHILD" ]; then
    echo "Hijo PID: $CHILD"
    echo "PPID del hijo:"
    ps -o pid,ppid,stat,cmd -p $CHILD
    echo ""
    echo "PPID deberia ser 1 (re-parenteado a PID 1)"
else
    echo "Hijo no encontrado (ya termino)"
fi

pkill -f "sleep 120" 2>/dev/null
'
```

### Paso 2.2: Los huerfanos son normales

```bash
docker compose exec debian-dev bash -c '
echo "=== Los huerfanos NO son un problema ==="
echo ""
echo "Zombie:   hijo MUERTO, padre VIVO pero no hace wait()"
echo "          → entrada bloqueada en tabla de procesos"
echo "          → PROBLEMA (si se acumulan)"
echo ""
echo "Huerfano: hijo VIVO, padre MUERTO"
echo "          → re-parenteado a PID 1"
echo "          → PID 1 hara wait() cuando el hijo termine"
echo "          → NORMAL (nohup, disown, setsid crean huerfanos)"

echo ""
echo "=== Muchos programas crean huerfanos a proposito ==="
echo "- nohup lanza procesos que sobreviven al terminal"
echo "- Daemons hacen doble fork (el hijo intermedio muere)"
echo "- Contenedores: PID 1 adopta subprocesos huerfanos"
'
```

### Paso 2.3: Verificar re-parenting con multiples niveles

```bash
docker compose exec debian-dev bash -c '
echo "=== Cadena padre-hijo-nieto ==="
bash -c '\''
    bash -c "sleep 180" &
    CHILD=$!
    echo "nieto PID=$CHILD, padre=$$"
    exit
'\'' &
PARENT=$!
sleep 1
wait $PARENT 2>/dev/null

GRANDCHILD=$(pgrep -nf "sleep 180")
if [ -n "$GRANDCHILD" ]; then
    echo ""
    echo "El nieto (sleep 180):"
    ps -o pid,ppid,stat,cmd -p $GRANDCHILD
    echo ""
    echo "Fue re-parenteado a PID 1 (PPID=1)"
    echo "No importa cuantos niveles habia — siempre va a PID 1"
fi

pkill -f "sleep 180" 2>/dev/null
'
```

---

## Parte 3 — Diagnostico

### Objetivo

Diagnosticar zombies en un sistema en produccion.

### Paso 3.1: Contar zombies

```bash
docker compose exec debian-dev bash -c '
echo "=== Contar zombies ==="
ZOMBIES=$(ps -eo stat --no-headers | grep -c "^Z")
echo "Zombies actuales: $ZOMBIES"
echo ""

echo "=== Metodos alternativos ==="
echo "1. ps -eo stat | grep -c ^Z"
echo "2. top -bn1 | grep zombie (linea de tareas)"
echo "3. cat /proc/loadavg (ultimo campo: running/total)"
echo ""
echo "=== En top ==="
top -bn1 | head -2
echo "(la linea Tasks muestra zombies)"
'
```

### Paso 3.2: Identificar el padre de un zombie

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear zombies para diagnosticar ==="
bash -c '\''
    for i in 1 2 3; do
        bash -c "exit 0" &
    done
    sleep 30
'\'' &
BAD_PARENT=$!
sleep 1

echo "=== Buscar zombies y su padre ==="
ps -eo pid,ppid,stat,cmd | awk '\''$3 ~ /^Z/ {print}'\''
echo ""
echo "=== Padre de los zombies ==="
ps -o pid,ppid,stat,cmd -p $BAD_PARENT

echo ""
echo "El PPID de los zombies apunta al proceso con el bug"
echo "En produccion: investigar por que ese proceso no hace wait()"

kill $BAD_PARENT
wait $BAD_PARENT 2>/dev/null
sleep 1
echo ""
echo "Zombies despues de matar al padre: $(ps -eo stat --no-headers | grep -c "^Z")"
'
```

### Paso 3.3: Resumen de diagnostico

```bash
docker compose exec debian-dev bash -c '
echo "=== Procedimiento de diagnostico ==="
echo ""
echo "1. Detectar:"
echo "   ps -eo stat | grep -c ^Z"
echo ""
echo "2. Listar zombies con su padre:"
echo "   ps -eo pid,ppid,stat,cmd | awk \"\\\$3 ~ /^Z/\""
echo ""
echo "3. Investigar al padre:"
echo "   ps -o pid,ppid,cmd -p <PPID>"
echo ""
echo "4. Solucionar:"
echo "   a) kill -CHLD <PPID>  (pedir wait)"
echo "   b) kill <PPID>        (si a) no funciona)"
echo "   c) Arreglar el codigo (solucion real)"
echo ""
echo "5. Prevencion (en codigo):"
echo "   - signal(SIGCHLD, SIG_IGN)  → auto-reap"
echo "   - waitpid(-1, NULL, WNOHANG) en handler"
echo "   - En contenedores: docker run --init"
'
```

---

## Limpieza final

```bash
docker compose exec debian-dev bash -c '
pkill -f "sleep 120" 2>/dev/null
pkill -f "sleep 180" 2>/dev/null
echo "Limpieza completada"
'
```

---

## Conceptos reforzados

1. Un **zombie** es un proceso que termino pero cuyo padre no
   hizo wait(). Estado Z en ps. No consume CPU ni memoria, pero
   ocupa una entrada en la tabla de procesos.

2. Un zombie **no se puede matar** con kill -9 — ya esta muerto.
   La solucion es actuar sobre el padre (SIGCHLD o matarlo).

3. Un **huerfano** es un proceso cuyo padre murio. Es re-parenteado
   a PID 1, que hara wait() automaticamente. Los huerfanos son
   normales y no son un problema.

4. Para diagnosticar zombies: `ps -eo pid,ppid,stat,cmd | awk
   '$3~/^Z/'` identifica tanto el zombie como su padre.

5. En **contenedores**, PID 1 debe manejar SIGCHLD para recoger
   zombies. `docker run --init` (tini) resuelve esto.

6. La prevencion en codigo es `signal(SIGCHLD, SIG_IGN)` o un
   handler que llame `waitpid(-1, NULL, WNOHANG)`.
