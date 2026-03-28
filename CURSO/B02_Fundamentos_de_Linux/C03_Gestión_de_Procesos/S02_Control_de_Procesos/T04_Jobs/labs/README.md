# Lab — Jobs

## Objetivo

Gestionar procesos en background y foreground: lanzar con &,
suspender con Ctrl+Z, mover con fg/bg, listar con jobs, y
asegurar que procesos sobrevivan al cierre de terminal con
nohup y disown.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Control de jobs

### Objetivo

Usar el control de jobs del shell para mover procesos entre
foreground y background.

### Paso 1.1: Lanzar en background con &

```bash
docker compose exec debian-dev bash -c '
echo "=== Lanzar en background ==="
sleep 300 &
echo "PID: $!"
echo "Job number: se muestra como [N] PID"

sleep 301 &
echo "Segundo PID: $!"

echo ""
echo "=== Listar jobs ==="
jobs
echo ""
echo "jobs muestra los procesos lanzados en background"
echo "  [1]  = numero de job"
echo "  +    = job actual (default para fg/bg)"
echo "  -    = job anterior"

kill %1 %2 2>/dev/null
wait 2>/dev/null
'
```

### Paso 1.2: jobs con opciones

```bash
docker compose exec debian-dev bash -c '
sleep 300 &
sleep 301 &
sleep 302 &

echo "=== jobs -l (con PID) ==="
jobs -l
echo ""

echo "=== jobs -p (solo PIDs) ==="
jobs -p
echo ""

echo "=== jobs -r (solo running) ==="
jobs -r
echo ""

echo "=== jobs -s (solo stopped) ==="
jobs -s

kill %1 %2 %3 2>/dev/null
wait 2>/dev/null
'
```

### Paso 1.3: Job specs

```bash
docker compose exec debian-dev bash -c '
echo "=== Formas de referirse a un job ==="
sleep 300 &
sleep 301 &
sleep 302 &
sleep 0.5

echo "jobs:"
jobs -l
echo ""
echo "Especificadores:"
echo "  %1        — job numero 1"
echo "  %+  o %%  — job actual (el +)"
echo "  %-        — job anterior (el -)"
echo "  %sleep    — job cuyo comando empieza con sleep"
echo "  %?301     — job cuyo comando contiene 301"
echo ""

echo "=== Ejemplo: kill %2 (matar job 2) ==="
kill %2
sleep 0.5
jobs -l
echo "(job 2 terminado)"

kill %1 %3 2>/dev/null
wait 2>/dev/null
'
```

### Paso 1.4: Simular Ctrl+Z y fg/bg

```bash
docker compose exec debian-dev bash -c '
echo "=== Simular suspension y reanudacion ==="
sleep 300 &
PID=$!
echo "Job en background (PID $PID):"
jobs -l

echo ""
echo "=== kill -TSTP (equivalente a Ctrl+Z) ==="
kill -TSTP $PID
sleep 0.5
echo "Despues de SIGTSTP:"
jobs -l
echo "(estado: Stopped)"

echo ""
echo "=== bg %1 (reanudar en background) ==="
bg %1 2>/dev/null
sleep 0.5
echo "Despues de bg:"
jobs -l
echo "(estado: Running)"

kill $PID
wait $PID 2>/dev/null

echo ""
echo "=== En una terminal interactiva ==="
echo "Ctrl+Z  — suspende el proceso de foreground (SIGTSTP)"
echo "fg %N   — mueve job N a foreground"
echo "bg %N   — reanuda job N en background"
'
```

---

## Parte 2 — Sobrevivir al cierre de terminal

### Objetivo

Entender que pasa con los procesos cuando la terminal se cierra
y como protegerlos.

### Paso 2.1: SIGHUP al cerrar terminal

```bash
docker compose exec debian-dev bash -c '
echo "=== Que pasa al cerrar la terminal ==="
echo ""
echo "1. El terminal se cierra"
echo "2. El kernel envia SIGHUP a todos los procesos del session"
echo "3. Los procesos sin handler para SIGHUP mueren"
echo ""
echo "=== Procesos que sobreviven ==="
echo "- Procesos lanzados con nohup (ignoran SIGHUP)"
echo "- Procesos desasociados con disown"
echo "- Procesos en tmux/screen (otra sesion)"
echo "- Daemons (no tienen terminal de control)"
'
```

### Paso 2.2: nohup

```bash
docker compose exec debian-dev bash -c '
echo "=== nohup — inmune a SIGHUP ==="
nohup sleep 300 &
PID=$!
echo "PID: $PID"
echo ""

echo "=== Verificar ==="
ps -o pid,stat,cmd -p $PID
echo ""
echo "La salida va a nohup.out (o /dev/null si stdout ya redirigido)"
ls -la nohup.out 2>/dev/null || echo "(nohup.out no creado)"

echo ""
echo "=== nohup con redireccion ==="
nohup sleep 301 > /tmp/mi-log.txt 2>&1 &
PID2=$!
echo "Con redireccion explicita, nohup.out no se crea"
ls -la /tmp/mi-log.txt

kill $PID $PID2 2>/dev/null
wait 2>/dev/null
rm -f nohup.out /tmp/mi-log.txt
'
```

### Paso 2.3: disown

```bash
docker compose exec debian-dev bash -c '
echo "=== disown — quitar de la tabla de jobs ==="
sleep 300 &
PID=$!
echo "Job creado:"
jobs -l

echo ""
echo "=== disown %1 ==="
disown %1
echo "Despues de disown:"
jobs -l
echo "(la tabla de jobs esta vacia)"

echo ""
echo "=== Pero el proceso sigue vivo ==="
ps -o pid,stat,cmd -p $PID
echo ""
echo "disown quita el job de la tabla del shell"
echo "El shell no le enviara SIGHUP al cerrar"
echo "Pero stdout/stderr siguen conectados a la terminal"

kill $PID 2>/dev/null
wait 2>/dev/null
'
```

### Paso 2.4: disown -h vs disown

```bash
docker compose exec debian-dev bash -c '
echo "=== disown -h — mantener en tabla pero proteger ==="
sleep 300 &
PID=$!

echo "Antes de disown -h:"
jobs -l

disown -h %1

echo ""
echo "Despues de disown -h:"
jobs -l
echo "(el job sigue en la tabla)"
echo ""
echo "Diferencia:"
echo "  disown    — quita de la tabla Y protege de SIGHUP"
echo "  disown -h — solo protege de SIGHUP, sigue en tabla"
echo "             (puedes seguir usando fg/bg)"

kill $PID 2>/dev/null
wait 2>/dev/null
'
```

---

## Parte 3 — Comparacion de metodos

### Objetivo

Comparar las diferentes formas de mantener procesos vivos.

### Paso 3.1: nohup vs disown

```bash
docker compose exec debian-dev bash -c '
echo "=== nohup vs disown ==="
printf "%-15s %-25s %-25s\n" "Aspecto" "nohup" "disown"
printf "%-15s %-25s %-25s\n" "-----------" "---------------------" "---------------------"
printf "%-15s %-25s %-25s\n" "Cuando" "Al lanzar" "Despues de lanzar"
printf "%-15s %-25s %-25s\n" "SIGHUP" "Ignora (handler)" "No recibe (no en tabla)"
printf "%-15s %-25s %-25s\n" "stdout/stderr" "Redirige a nohup.out" "Sigue en terminal"
printf "%-15s %-25s %-25s\n" "Tabla de jobs" "Si" "No (se quita)"
printf "%-15s %-25s %-25s\n" "Control (fg/bg)" "Si" "No"

echo ""
echo "=== Cuando usar cada uno ==="
echo "nohup: sabes de antemano que quieres desacoplar"
echo "disown: ya lanzaste el proceso y te diste cuenta"
echo "        que necesitas cerrar la terminal"
'
```

### Paso 3.2: tmux/screen

```bash
docker compose exec debian-dev bash -c '
echo "=== tmux/screen — la solucion robusta ==="
echo ""
echo "tmux y screen crean una sesion independiente de tu terminal"
echo "Puedes:"
echo "  1. Crear una sesion:     tmux new -s trabajo"
echo "  2. Ejecutar comandos dentro"
echo "  3. Desconectarte:        Ctrl+b d (tmux) o Ctrl+a d (screen)"
echo "  4. Cerrar tu terminal"
echo "  5. Reconectarte:         tmux attach -t trabajo"
echo ""
echo "Ventajas sobre nohup/disown:"
echo "  - Puedes volver a ver la salida"
echo "  - Puedes interactuar con el proceso"
echo "  - Multiples paneles y ventanas"
echo "  - No pierdes el historial de la sesion"

echo ""
echo "=== Disponibilidad ==="
which tmux 2>/dev/null && echo "tmux: disponible" || echo "tmux: no disponible"
which screen 2>/dev/null && echo "screen: disponible" || echo "screen: no disponible"
'
```

### Paso 3.3: Tabla de comparacion completa

```bash
docker compose exec debian-dev bash -c '
echo "=== Comparacion completa ==="
printf "%-12s %-10s %-10s %-10s %-12s\n" "Metodo" "SIGHUP" "Ver salida" "Reattach" "Complejidad"
printf "%-12s %-10s %-10s %-10s %-12s\n" "----------" "--------" "--------" "--------" "----------"
printf "%-12s %-10s %-10s %-10s %-12s\n" "cmd &"      "Muere"   "Si"       "No"      "Minima"
printf "%-12s %-10s %-10s %-10s %-12s\n" "nohup"      "Ignora"  "nohup.out" "No"     "Baja"
printf "%-12s %-10s %-10s %-10s %-12s\n" "disown"     "Ignora"  "No"       "No"      "Baja"
printf "%-12s %-10s %-10s %-10s %-12s\n" "tmux"       "Ignora"  "Si"       "Si"      "Media"
printf "%-12s %-10s %-10s %-10s %-12s\n" "systemd"    "Ignora"  "journald" "N/A"     "Media"

echo ""
echo "Para tareas one-off: nohup"
echo "Para sesiones interactivas persistentes: tmux"
echo "Para servicios permanentes: systemd"
'
```

---

## Limpieza final

```bash
docker compose exec debian-dev bash -c '
killall sleep 2>/dev/null
rm -f nohup.out
echo "Limpieza completada"
'
```

---

## Conceptos reforzados

1. `&` lanza en background, `Ctrl+Z` (SIGTSTP) suspende, `fg`
   trae a foreground, `bg` reanuda en background. `jobs -l`
   lista con PIDs.

2. Los **job specs** (`%1`, `%+`, `%-`, `%string`, `%?string`)
   permiten referirse a jobs por numero, posicion, o patron.

3. Al cerrar una terminal, el kernel envia **SIGHUP** a los
   procesos del session. Sin proteccion, los procesos mueren.

4. **nohup** se usa al lanzar (redirige stdout a nohup.out y
   ignora SIGHUP). **disown** se usa despues de lanzar (quita
   de la tabla de jobs).

5. **tmux/screen** son la solucion robusta: permiten desconectar
   y reconectar sesiones, ver la salida, e interactuar.

6. Para servicios permanentes, **systemd** es la herramienta
   correcta, no nohup ni tmux.
