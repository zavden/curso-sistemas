# Lab — kill, killall, pkill

## Objetivo

Terminar procesos de forma precisa: kill por PID, killall por
nombre exacto, pkill por patron, y la practica esencial de
verificar con pgrep antes de actuar.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — kill

### Objetivo

Terminar procesos individuales por PID con diferentes senales.

### Paso 1.1: kill basico

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear procesos de prueba ==="
sleep 400 &
PID1=$!
sleep 401 &
PID2=$!
echo "PID1: $PID1, PID2: $PID2"
ps -o pid,stat,cmd -p $PID1,$PID2

echo ""
echo "=== kill PID (SIGTERM por defecto) ==="
kill $PID1
sleep 0.5
ps -o pid,stat,cmd -p $PID1 2>/dev/null || echo "PID1 terminado"

echo ""
echo "=== kill -9 PID (SIGKILL) ==="
kill -9 $PID2
sleep 0.5
ps -o pid,stat,cmd -p $PID2 2>/dev/null || echo "PID2 terminado"
wait 2>/dev/null
'
```

### Paso 1.2: kill con diferentes senales

```bash
docker compose exec debian-dev bash -c '
echo "=== Tres sintaxis para la senal ==="
sleep 400 & P1=$!
sleep 401 & P2=$!
sleep 402 & P3=$!

echo "Por numero:  kill -15 $P1"
kill -15 $P1

echo "Por nombre:  kill -TERM $P2"
kill -TERM $P2

echo "Con SIG:     kill -SIGTERM $P3"
kill -SIGTERM $P3

sleep 0.5
echo ""
echo "Los tres son equivalentes. Todos terminados:"
ps -o pid,stat,cmd -p $P1,$P2,$P3 2>/dev/null || echo "(ninguno existe)"
wait 2>/dev/null
'
```

### Paso 1.3: kill -0 — verificar si existe

```bash
docker compose exec debian-dev bash -c '
echo "=== kill -0 no envia senal, solo verifica ==="
sleep 400 &
PID=$!

echo "Verificar PID $PID:"
kill -0 $PID 2>/dev/null && echo "  existe" || echo "  no existe"

kill $PID
wait $PID 2>/dev/null
sleep 0.5

echo "Verificar PID $PID despues de matarlo:"
kill -0 $PID 2>/dev/null && echo "  existe" || echo "  no existe"
echo ""
echo "kill -0 es util en scripts para verificar si un proceso vive"
'
```

### Paso 1.4: Matar multiples PIDs

```bash
docker compose exec debian-dev bash -c '
echo "=== Matar varios PIDs a la vez ==="
sleep 400 & P1=$!
sleep 401 & P2=$!
sleep 402 & P3=$!
echo "Creados: $P1 $P2 $P3"

kill $P1 $P2 $P3
sleep 0.5
echo "Todos terminados:"
ps -o pid,stat,cmd -p $P1,$P2,$P3 2>/dev/null || echo "(ninguno existe)"
wait 2>/dev/null
'
```

### Paso 1.5: builtin vs externo

```bash
docker compose exec debian-dev bash -c '
echo "=== kill: builtin del shell vs /bin/kill ==="
echo "Tipo del shell: $(type kill)"
echo ""
echo "Comando externo: $(which kill 2>/dev/null || echo "/bin/kill")"
echo ""
echo "Diferencia principal:"
echo "  builtin:  puede enviar senales a procesos del mismo shell"
echo "            aunque el limite de procesos este alcanzado"
echo "  externo:  es un proceso separado que necesita fork()"
echo ""
echo "En la practica ambos funcionan igual"
echo "El builtin es lo que usas normalmente"
'
```

---

## Parte 2 — killall

### Objetivo

Terminar procesos por nombre exacto del ejecutable.

### Paso 2.1: killall basico

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear multiples sleeps ==="
sleep 500 &
sleep 501 &
sleep 502 &
echo "Procesos sleep creados:"
ps -C sleep -o pid,cmd

echo ""
echo "=== killall sleep (mata TODOS los sleep) ==="
killall sleep 2>/dev/null
sleep 0.5
echo "Despues de killall:"
ps -C sleep -o pid,cmd 2>/dev/null || echo "No hay procesos sleep"
wait 2>/dev/null
'
```

`killall` usa el nombre **exacto** del ejecutable. `killall sleep`
mata todos los procesos llamados `sleep`.

### Paso 2.2: killall con senal especifica

```bash
docker compose exec debian-dev bash -c '
echo "=== killall con senal ==="
sleep 500 &
sleep 501 &
echo "Creados:"
ps -C sleep -o pid,stat,cmd

echo ""
echo "=== killall -STOP sleep (pausar todos) ==="
killall -STOP sleep
ps -C sleep -o pid,stat,cmd
echo "(todos en estado T)"

echo ""
echo "=== killall -CONT sleep (resumir todos) ==="
killall -CONT sleep
ps -C sleep -o pid,stat,cmd
echo "(todos de vuelta a S)"

killall sleep
wait 2>/dev/null
'
```

### Paso 2.3: killall -w — esperar

```bash
docker compose exec debian-dev bash -c '
echo "=== killall -w (esperar a que mueran) ==="
sleep 500 &
sleep 501 &

echo "Matando con -w (bloquea hasta que todos terminen)..."
killall -w sleep
echo "Todos terminados (killall -w retorno)"
ps -C sleep -o pid,cmd 2>/dev/null || echo "Confirmado: no hay sleep"
wait 2>/dev/null
'
```

### Paso 2.4: killall es seguro por nombre exacto

```bash
docker compose exec debian-dev bash -c '
echo "=== killall usa nombre EXACTO ==="
sleep 500 &
SPID=$!
bash -c "echo sleeping; exec sleep 501" &
BPID=$!
sleep 0.5

echo "Antes:"
ps -o pid,cmd -p $SPID,$BPID

echo ""
echo "killall sleep mata ambos (el exec reemplazo bash por sleep)"
killall sleep 2>/dev/null
sleep 0.5
echo "Despues:"
ps -o pid,cmd -p $SPID,$BPID 2>/dev/null || echo "(todos terminados)"
wait 2>/dev/null

echo ""
echo "Nota: killall busca por /proc/PID/comm (nombre del ejecutable)"
echo "No por los argumentos del comando"
'
```

---

## Parte 3 — pkill y pgrep

### Objetivo

Buscar y terminar procesos por patron con pkill/pgrep, usando
filtros avanzados.

### Paso 3.1: pgrep — buscar antes de matar

```bash
docker compose exec debian-dev bash -c '
echo "=== Siempre verificar con pgrep antes de pkill ==="
sleep 600 &
sleep 601 &
bash -c "sleep 602" &
sleep 0.5

echo "=== pgrep sleep ==="
pgrep sleep
echo ""

echo "=== pgrep -a sleep (con comando completo) ==="
pgrep -a sleep
echo ""

echo "=== pgrep -la sleep (con PID y argumentos) ==="
pgrep -la sleep
echo ""
echo "REGLA: siempre pgrep primero para ver que vas a matar"

killall sleep 2>/dev/null
wait 2>/dev/null
'
```

### Paso 3.2: pkill — matar por patron

```bash
docker compose exec debian-dev bash -c '
echo "=== pkill vs killall ==="
sleep 700 &
sleep 701 &
sleep 0.5

echo "Antes:"
pgrep -a sleep

echo ""
echo "=== pkill sleep ==="
pkill sleep
sleep 0.5
echo "Despues:"
pgrep -a sleep || echo "No hay procesos sleep"
wait 2>/dev/null

echo ""
echo "Diferencia con killall:"
echo "  killall: nombre EXACTO del ejecutable"
echo "  pkill:   patron (substring match por defecto)"
'
```

### Paso 3.3: pkill -x — match exacto

```bash
docker compose exec debian-dev bash -c '
echo "=== pkill sin -x (substring match) ==="
sleep 800 &
bash -c "exec -a sleeper sleep 801" &
sleep 0.5

echo "pgrep sleep (sin -x):"
pgrep -a sleep
echo "(encuentra ambos — sleep y sleeper)"

echo ""
echo "pgrep -x sleep (match exacto):"
pgrep -ax sleep
echo "(solo encuentra sleep exacto, no sleeper)"

killall sleep 2>/dev/null
kill $(pgrep sleeper) 2>/dev/null
wait 2>/dev/null
'
```

### Paso 3.4: pkill -f — buscar en linea de comando completa

```bash
docker compose exec debian-dev bash -c '
echo "=== pkill -f busca en toda la linea de comando ==="
sleep 900 &
bash -c "sleep 901" &
sleep 0.5

echo "pgrep sleep (solo nombre):"
pgrep -a sleep

echo ""
echo "pgrep -f \"sleep 900\" (linea completa):"
pgrep -af "sleep 900"
echo "(solo el que tiene argumento 900)"

echo ""
echo "pkill -f \"sleep 900\" (matar solo ese):"
pkill -f "sleep 900"
sleep 0.5
echo "Restantes:"
pgrep -a sleep

killall sleep 2>/dev/null
wait 2>/dev/null
'
```

`-f` busca en `/proc/PID/cmdline` completo en vez de solo
`/proc/PID/comm`.

### Paso 3.5: Filtros de pkill/pgrep

```bash
docker compose exec debian-dev bash -c '
echo "=== Filtrar por usuario (-u) ==="
pgrep -au root | head -5
echo "..."
echo ""

echo "=== Filtrar por PPID (-P) ==="
echo "Hijos de PID 1:"
pgrep -laP 1 | head -5
echo ""

echo "=== Filtrar: el mas reciente (-n) ==="
sleep 100 &
sleep 101 &
sleep 0.5
echo "El sleep mas reciente:"
pgrep -na sleep
echo ""

echo "=== Filtrar: el mas antiguo (-o) ==="
echo "El sleep mas antiguo:"
pgrep -oa sleep

killall sleep 2>/dev/null
wait 2>/dev/null
'
```

### Paso 3.6: pidof — alternativa simple

```bash
docker compose exec debian-dev bash -c '
echo "=== pidof — buscar PID por nombre exacto ==="
sleep 200 &
sleep 201 &
sleep 0.5

echo "pidof sleep:"
pidof sleep
echo "(devuelve todos los PIDs en una linea)"
echo ""
echo "pidof es mas simple que pgrep pero menos flexible"
echo "No soporta patrones ni filtros"

killall sleep 2>/dev/null
wait 2>/dev/null
'
```

---

## Limpieza final

```bash
docker compose exec debian-dev bash -c '
killall sleep 2>/dev/null
echo "Limpieza completada"
'
```

---

## Conceptos reforzados

1. `kill` envia senales por **PID**. Sin senal especificada,
   envia SIGTERM (15). `kill -0` verifica existencia sin enviar
   senal.

2. `killall` mata por **nombre exacto** del ejecutable. Mata
   todos los procesos con ese nombre. `killall -w` espera.

3. `pkill` mata por **patron** (substring por defecto, `-x` para
   exacto, `-f` para buscar en toda la linea de comando).

4. **Siempre usar `pgrep` antes de `pkill`** para verificar que
   vas a matar los procesos correctos. `pgrep -a` muestra el
   comando completo.

5. Los filtros `-u` (usuario), `-P` (padre), `-n` (mas reciente),
   `-o` (mas antiguo) permiten precision al seleccionar procesos.

6. `pidof` es la alternativa mas simple para obtener PIDs por
   nombre exacto, pero sin la flexibilidad de pgrep/pkill.
