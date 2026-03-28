# T02 — kill, killall, pkill

## Errata y notas sobre el material original

1. **max.md ejercicio 4 — `killall -i -y sleep`**: La flag `-y` en
   Linux killall (psmisc) es `--younger-than` y requiere un argumento
   de tiempo (ej: `killall -y 5m sleep`). Sin argumento es un error
   de sintaxis.

2. **max.md ejercicio 6 — "orphan y SIGHUP"**: El título promete
   demostrar SIGHUP matando huérfanos, pero el ejercicio solo muestra
   PGID/SID sin enviar SIGHUP. Reescrito.

3. **max.md ejercicio 7 — pgrep con python3**: Muestra `pgrep -fa
   "python3"` y `pkill -f "python3.*server"` pero no hay procesos
   python3. Mezcla contextos sin sentido.

4. **max.md ejercicio 9 — `pidof bash | head -1`**: `pidof` devuelve
   todos los PIDs en una sola línea, así que `head -1` no filtra nada.
   Debería ser `pidof -s bash` para obtener un solo PID.

5. **Ambos READMEs — killall "nombre exacto"**: Dicen que killall usa
   nombre exacto, pero luego mencionan que "por defecto solo compara
   15 chars". Sin `-e`, killall trunca la comparación a 15 caracteres
   (TASK_COMM_LEN). Solo con `-e` es realmente exacto.

---

## kill — Enviar señal por PID

`kill` es la herramienta base para enviar señales. A pesar de su nombre,
no solo mata procesos — envía cualquier señal:

```bash
# Sintaxis
kill [-señal] PID [PID ...]

# Enviar SIGTERM (default)
kill 1234

# Enviar una señal específica — cuatro formas equivalentes
kill -9 1234          # por número
kill -SIGKILL 1234    # por nombre con SIG
kill -KILL 1234       # por nombre sin SIG
kill -s KILL 1234     # con -s
```

### kill -0: verificar existencia

```bash
# Signal 0 no envía señal real — solo verifica que el proceso existe
# y que tienes permiso para enviarle señales
kill -0 1234 && echo "existe" || echo "no existe o sin permiso"

# Útil en scripts:
if kill -0 "$PID" 2>/dev/null; then
    echo "Proceso $PID sigue corriendo"
fi
```

### Enviar a múltiples procesos

```bash
# Varios PIDs
kill 1234 1235 1236

# A un process group (PID negativo)
kill -TERM -1234      # envía a todo el process group 1234

# Todos los procesos que el usuario puede señalizar (MUY peligroso)
kill -TERM -1         # -1 = todos tus procesos excepto kill mismo
```

> **Cuidado con `kill -TERM -1`**: mata TODOS los procesos del usuario
> actual que puede señalizar. Esto incluye tu shell, tu sesión SSH, etc.
> Solo útil en emergencias o scripts de limpieza controlados.

### Process groups y kill negativo

Cuando se ejecuta un pipeline, el shell crea un **process group**.
Se puede enviar señal a todo el grupo con PID negativo:

```bash
# Ejemplo: matar un pipeline completo
cat /dev/urandom | gzip > /dev/null &
# cat y gzip tienen el mismo PGID

PGID=$(ps -o pgid= -p $! | tr -d ' ')
ps -o pid,pgid,cmd -g $PGID
# 1234 1234 cat /dev/urandom
# 1235 1234 gzip

kill -- -$PGID    # mata cat Y gzip (todo el group)
# El -- evita que -$PGID se interprete como una opción
```

### Listar señales

```bash
kill -l            # lista todas las señales
kill -l TERM       # → 15 (nombre a número)
kill -l 15         # → TERM (número a nombre)
```

### kill es un builtin

```bash
type kill      # kill is a shell builtin
which kill     # /usr/bin/kill

# El builtin puede usar job IDs (%1, %2...)
kill %1        # mata el job 1

# El comando externo solo acepta PIDs
/usr/bin/kill 1234

# En situaciones extremas (fork bomb, sistema sin memoria),
# el builtin funciona cuando el externo no puede ejecutarse
# porque no se puede crear un nuevo proceso
```

---

## killall — Enviar señal por nombre

`killall` envía una señal a **todos los procesos** que coincidan con
el nombre del ejecutable:

```bash
# Sintaxis
killall [-señal] nombre

# Matar todos los procesos llamados "nginx"
killall nginx       # equivale a: kill $(pidof nginx)

# Con señal específica
killall -9 nginx
killall -KILL nginx

# Solo procesos de un usuario
killall -u dev python3
```

### Opciones útiles

```bash
# -e / --exact: coincidencia exacta del nombre completo
# (sin -e, solo compara los primeros 15 caracteres)
killall -e process-with-very-long-name

# -i / --interactive: pedir confirmación para cada proceso
killall -i python3
# Kill python3(1234) ? (y/N)

# -w / --wait: esperar a que todos los procesos mueran
killall -w nginx

# -r / --regexp: usar expresión regular
killall -r 'worker[0-9]+'

# -v / --verbose: mostrar qué se señalizó
killall -v nginx
# Killed nginx(1234) with signal 15

# -y / --younger-than: solo procesos más recientes que
killall -y 5m sleep     # solo sleeps de menos de 5 minutos

# -o / --older-than: solo procesos más viejos que
killall -o 1h nginx     # solo nginx de más de 1 hora
```

### Peligro: killall en otros Unix

En **Solaris y otros Unix**, `killall` sin argumentos mata **todos los
procesos del sistema**. En Linux (paquete `psmisc`), `killall` requiere
un nombre de proceso. Si trabajas en sistemas no-Linux, verificar qué
versión tienes.

---

## pkill — Enviar señal por patrón

`pkill` envía señales basándose en patrones (regex). Es más flexible
que `killall`:

```bash
# Sintaxis
pkill [-señal] [opciones] patrón

# Coincidencia parcial por defecto (substring regex)
pkill nginx          # mata nginx, nginx-worker, etc.

# Coincidencia exacta
pkill -x nginx       # solo procesos nombrados exactamente "nginx"

# Con señal específica
pkill -9 nginx
pkill -KILL nginx
```

### Buscar en la línea de comando completa

```bash
# Por defecto, pkill busca solo en el nombre del proceso (comm)
# -f busca en toda la línea de comando (cmdline)

pkill python3                    # mata cualquier python3
pkill -f "python3 /srv/app.py"  # solo el que ejecuta app.py
pkill -f "gunicorn.*myproject"   # patrón en argumentos

# Sin -f: "pkill python3" mataría todos los procesos Python
# Con -f: se puede ser específico sobre cuál matar
```

### Filtros de pkill

```bash
# Por usuario
pkill -u dev python3        # solo procesos de dev
pkill -u dev,root nginx     # procesos de dev o root

# Por terminal
pkill -t pts/0              # procesos en pts/0

# Por padre
pkill -P 1234               # solo hijos de PID 1234

# Por grupo
pkill -G developers         # por grupo real

# Más nuevo/viejo
pkill -n nginx              # solo el más nuevo (newest)
pkill -o nginx              # solo el más viejo (oldest)

# Combinando filtros
pkill -u dev -f "python3.*worker"
```

### pgrep — El compañero de pkill

`pgrep` usa la misma sintaxis que `pkill` pero solo **muestra** PIDs
sin enviar señales:

```bash
# Ver qué procesos coinciden ANTES de matarlos
pgrep -la nginx
# 1234 nginx: master process /usr/sbin/nginx
# 1235 nginx: worker process

# -l: nombre + PID
# -a: línea de comando completa + PID

# Contar procesos
pgrep -c nginx     # → 3

# Verificar patrón -f
pgrep -fa "python3.*app"
# 1500 python3 /srv/app/main.py --port 8080

# Si el resultado es correcto, entonces:
pkill -f "python3.*app"
```

**Regla**: siempre ejecutar `pgrep` primero para verificar qué procesos
coinciden. Luego usar `pkill` con el mismo patrón.

---

## pidof — Obtener PID por nombre exacto

```bash
pidof sshd          # devuelve todos los PIDs (en una línea)
# 842 1501

pidof -s sshd       # solo uno (single shot)
# 1501

# Útil en scripts
kill $(pidof -s nginx)
```

---

## Comparación

| Herramienta | Busca por | Coincidencia | Mejor para |
|---|---|---|---|
| `kill` | PID | Exacta | Proceso específico conocido |
| `killall` | Nombre | Exacta (15 chars sin -e) | Todas las instancias de un programa |
| `pkill` | Patrón | Regex substring (-x exacto) | Selección flexible por patrón |
| `pgrep` | Patrón | (igual que pkill) | Verificar antes de pkill |
| `pidof` | Nombre | Exacta | Obtener PIDs para scripts |

---

## Patrones prácticos

### Parar un servicio limpiamente

```bash
# Opción 1: systemctl (preferida para servicios gestionados)
sudo systemctl stop nginx

# Opción 2: señal manual
kill $(pidof -s nginx)          # SIGTERM al master
sleep 2
pidof nginx && kill -9 $(pidof nginx)   # SIGKILL si no terminó
```

### Matar un proceso colgado en la terminal

```bash
# Escalada de señales:
# 1. Ctrl+C           → SIGINT
# 2. Ctrl+\           → SIGQUIT (+ core dump)
# 3. Desde otra terminal: kill <PID>     → SIGTERM
# 4. kill -9 <PID>    → SIGKILL
# 5. Si ni -9 funciona → estado D (I/O colgado, reiniciar)
```

### Matar todos los procesos de un usuario

```bash
pgrep -u usuario -la    # ver primero
pkill -u usuario         # luego matar

# loginctl (systemd) — también mata la sesión
sudo loginctl terminate-user usuario
```

### Log rotation con señales

```bash
# Flujo típico de logrotate:
# 1. logrotate renombra: access.log → access.log.1
# 2. Envía señal: kill -USR1 $(pidof nginx)
# 3. nginx cierra el fd viejo y abre access.log nuevo
# 4. logrotate comprime access.log.1

# Sin la señal, el daemon seguiría escribiendo en access.log.1
# (el renombrado no afecta al fd abierto)
```

### timeout — Alternativa para limitar duración

```bash
# timeout mata automáticamente un proceso después de N segundos
timeout 10 long_command        # SIGTERM a los 10s
timeout -s KILL 10 long_cmd    # SIGKILL a los 10s
timeout --preserve-status 5 cmd  # preservar exit code

# Más seguro que "cmd & sleep N; kill $!" porque maneja edge cases
```

---

## Labs

### Parte 1 — kill

#### 1.1 kill básico

```bash
sleep 400 &
PID1=$!
sleep 401 &
PID2=$!
echo "PID1: $PID1, PID2: $PID2"
ps -o pid,stat,cmd -p $PID1,$PID2

echo ""
echo "=== kill PID (SIGTERM) ==="
kill $PID1
sleep 0.5
ps -o pid,stat,cmd -p $PID1 2>/dev/null || echo "PID1 terminado"

echo ""
echo "=== kill -9 PID (SIGKILL) ==="
kill -9 $PID2
sleep 0.5
ps -o pid,stat,cmd -p $PID2 2>/dev/null || echo "PID2 terminado"
wait 2>/dev/null
```

#### 1.2 Tres sintaxis para la señal

```bash
sleep 400 & P1=$!
sleep 401 & P2=$!
sleep 402 & P3=$!

kill -15 $P1        # por número
kill -TERM $P2      # por nombre
kill -SIGTERM $P3   # nombre completo

sleep 0.5
echo "Los tres son equivalentes:"
ps -o pid,stat,cmd -p $P1,$P2,$P3 2>/dev/null || echo "(todos terminados)"
wait 2>/dev/null
```

#### 1.3 kill -0 — verificar existencia

```bash
sleep 400 &
PID=$!

echo "Verificar PID $PID:"
kill -0 $PID 2>/dev/null && echo "  existe" || echo "  no existe"

kill $PID; wait $PID 2>/dev/null
sleep 0.5

echo "Verificar PID $PID después de matarlo:"
kill -0 $PID 2>/dev/null && echo "  existe" || echo "  no existe"
echo ""
echo "kill -0 es útil en scripts para verificar si un proceso vive"
```

#### 1.4 Matar múltiples PIDs

```bash
sleep 400 & P1=$!
sleep 401 & P2=$!
sleep 402 & P3=$!
echo "Creados: $P1 $P2 $P3"

kill $P1 $P2 $P3
sleep 0.5
echo "Todos terminados:"
ps -o pid,stat,cmd -p $P1,$P2,$P3 2>/dev/null || echo "(ninguno existe)"
wait 2>/dev/null
```

#### 1.5 builtin vs externo

```bash
echo "=== kill: builtin del shell vs /usr/bin/kill ==="
echo "Tipo del shell: $(type kill)"
echo "Comando externo: $(which kill 2>/dev/null || echo '/usr/bin/kill')"
echo ""
echo "El builtin puede matar jobs (%1, %2) y funciona"
echo "incluso cuando el sistema no puede crear procesos nuevos"
```

---

### Parte 2 — killall

#### 2.1 killall básico

```bash
sleep 500 &
sleep 501 &
sleep 502 &
echo "Procesos sleep creados:"
ps -C sleep -o pid,cmd

echo ""
echo "=== killall sleep (mata TODOS los sleep) ==="
killall sleep 2>/dev/null
sleep 0.5
echo "Después:"
ps -C sleep -o pid,cmd 2>/dev/null || echo "No hay procesos sleep"
wait 2>/dev/null
```

#### 2.2 killall con señal: pausar y reanudar

```bash
sleep 500 &
sleep 501 &
echo "Creados:"
ps -C sleep -o pid,stat,cmd

echo ""
echo "=== killall -STOP sleep ==="
killall -STOP sleep
ps -C sleep -o pid,stat,cmd
echo "(todos en estado T)"

echo ""
echo "=== killall -CONT sleep ==="
killall -CONT sleep
ps -C sleep -o pid,stat,cmd
echo "(de vuelta a S)"

killall sleep; wait 2>/dev/null
```

#### 2.3 killall -w — esperar

```bash
sleep 500 &
sleep 501 &

echo "Matando con -w (bloquea hasta que todos terminen)..."
killall -w sleep
echo "killall -w retornó: todos terminados"
ps -C sleep -o pid,cmd 2>/dev/null || echo "Confirmado"
wait 2>/dev/null
```

---

### Parte 3 — pkill y pgrep

#### 3.1 pgrep — buscar antes de matar

```bash
sleep 600 &
sleep 601 &
bash -c 'sleep 602' &
sleep 0.5

echo "=== pgrep sleep (solo PIDs) ==="
pgrep sleep
echo ""
echo "=== pgrep -a sleep (con comando completo) ==="
pgrep -a sleep
echo ""
echo "REGLA: siempre pgrep primero para ver qué vas a matar"

killall sleep 2>/dev/null; wait 2>/dev/null
```

#### 3.2 pkill -x vs sin -x

```bash
sleep 800 &
bash -c 'exec -a sleeper sleep 801' &
sleep 0.5

echo "pgrep sleep (substring, sin -x):"
pgrep -a sleep
echo "(encuentra sleep Y sleeper)"
echo ""
echo "pgrep -x sleep (match exacto):"
pgrep -ax sleep
echo "(solo sleep exacto, no sleeper)"

killall sleep 2>/dev/null
pkill sleeper 2>/dev/null
wait 2>/dev/null
```

#### 3.3 pkill -f — buscar en cmdline completa

```bash
sleep 900 &
bash -c 'sleep 901' &
sleep 0.5

echo "pgrep -a sleep (busca en comm):"
pgrep -a sleep
echo ""
echo "pgrep -af 'sleep 900' (busca en cmdline):"
pgrep -af 'sleep 900'
echo "(solo el que tiene argumento 900)"
echo ""
echo "pkill -f 'sleep 900' (matar solo ese):"
pkill -f 'sleep 900'
sleep 0.5
echo "Restantes:"
pgrep -a sleep

killall sleep 2>/dev/null; wait 2>/dev/null
```

`-f` busca en `/proc/PID/cmdline` en vez de `/proc/PID/comm`.

#### 3.4 Filtros

```bash
echo "=== Por usuario (-u) ==="
pgrep -au root | head -5
echo "..."
echo ""
echo "=== Hijos de PID 1 (-P) ==="
pgrep -laP 1 | head -5
echo ""

sleep 100 &
sleep 101 &
sleep 0.5
echo "=== Más reciente (-n) ==="
pgrep -na sleep
echo "=== Más antiguo (-o) ==="
pgrep -oa sleep

killall sleep 2>/dev/null; wait 2>/dev/null
```

#### 3.5 pidof

```bash
sleep 200 &
sleep 201 &
sleep 0.5

echo "pidof sleep: $(pidof sleep)"
echo "(todos los PIDs en una línea)"
echo ""
echo "pidof -s sleep: $(pidof -s sleep)"
echo "(solo uno — single shot)"

killall sleep 2>/dev/null; wait 2>/dev/null
```

---

## Ejercicios

### Ejercicio 1 — kill: tres sintaxis y SIGTERM default

```bash
sleep 300 &
PID=$!
echo "PID: $PID"

echo ""
echo "=== Verificar que existe ==="
kill -0 $PID && echo "Existe"

echo ""
echo "=== Matar con SIGTERM (default) ==="
kill $PID
sleep 0.5
kill -0 $PID 2>/dev/null && echo "Vive" || echo "Muerto"
wait $PID 2>/dev/null
```

Ahora repetir con `kill -15`, `kill -TERM`, y `kill -SIGTERM`.
¿El resultado cambia?

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

Las cuatro formas son equivalentes — todas envían SIGTERM (señal 15).
El proceso muere en todos los casos.

`kill -0` antes de matar confirma que existe (exit code 0).
`kill -0` después de matar falla (exit code 1) porque el PID ya no
existe.

**Punto clave**: `kill` sin señal = SIGTERM. Nunca es SIGKILL. Esto
es fundamental: `kill PID` es la forma educada, `kill -9 PID` es la
forzada.

</details>

---

### Ejercicio 2 — kill -0 en scripts: monitorear proceso

```bash
sleep 5 &
PID=$!
echo "Monitoreando PID $PID..."

while kill -0 $PID 2>/dev/null; do
    echo "  $(date +%H:%M:%S) — PID $PID sigue vivo"
    sleep 1
done
echo "  $(date +%H:%M:%S) — PID $PID terminó"
wait $PID 2>/dev/null
echo "Exit code: $?"
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

El bucle imprime "sigue vivo" cada segundo durante ~5 segundos.
Cuando `sleep 5` termina naturalmente, `kill -0` falla (retorna 1)
y el bucle termina.

Exit code: 0 (sleep 5 terminó exitosamente).

**¿Por qué `kill -0` en vez de `ps -p $PID`?**
- `kill -0` es un syscall directo (rápido, atómico)
- `ps -p` ejecuta un proceso externo (más lento)
- `kill -0` también verifica que tienes permiso para señalizar el
  proceso (relevante si el proceso cambió de UID)

**Patrón en scripts de producción:**
```bash
# Esperar a que un servicio arranque
for i in $(seq 1 30); do
    kill -0 $(cat /var/run/app.pid) 2>/dev/null && break
    sleep 1
done
```

</details>

---

### Ejercicio 3 — killall: matar por nombre

```bash
sleep 200 &
sleep 201 &
sleep 202 &
echo "=== Antes ==="
ps -C sleep -o pid,cmd

echo ""
echo "=== killall -v sleep ==="
killall -v sleep 2>&1
sleep 0.5

echo ""
echo "=== Después ==="
ps -C sleep -o pid,cmd 2>/dev/null || echo "Ningún sleep"
wait 2>/dev/null
```

Responder:
1. ¿Cuántos procesos mató killall?
2. ¿Usó SIGTERM o SIGKILL?
3. ¿Qué pasaría con `killall -e sleep`?

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

`killall -v sleep` imprime algo como:
```
Killed sleep(1234) with signal 15
Killed sleep(1235) with signal 15
Killed sleep(1236) with signal 15
```

1. Mató los 3 procesos sleep.
2. Usó señal 15 (SIGTERM) — es el default.
3. Con `-e`: resultado idéntico porque "sleep" tiene 5 caracteres (< 15).
   `-e` solo importa cuando el nombre tiene más de 15 caracteres (sin
   `-e`, killall trunca la comparación a 15 chars de TASK_COMM_LEN).

**killall busca por `/proc/PID/comm`** (nombre del ejecutable), no por
los argumentos. `killall sleep` mata `sleep 200`, `sleep 201`, y
`sleep 202` porque todos tienen comm="sleep".

</details>

---

### Ejercicio 4 — killall vs pkill: diferencia de matching

```bash
bash -c 'exec -a sleepd sleep 200' &
bash -c 'exec -a sleep-worker sleep 201' &
sleep 202 &
sleep 0.5

echo "=== Procesos creados ==="
ps -o pid,comm,cmd -p $(pgrep -d, 'sleep')

echo ""
echo "=== killall sleep (nombre exacto en comm) ==="
killall sleep 2>/dev/null
sleep 0.5
echo "Restantes:"
ps -o pid,comm -p $(pgrep -d, 'sleep') 2>/dev/null || echo "Ninguno con 'sleep' en comm"

echo ""
echo "=== pkill sleep (substring match) ==="
pkill sleep 2>/dev/null
sleep 0.5
echo "Restantes:"
pgrep -a sleep || echo "Ninguno"
wait 2>/dev/null
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

Procesos creados:
- PID A: comm=`sleepd`, cmd=`sleep 200`
- PID B: comm=`sleep-worker`, cmd=`sleep 201`
  (nota: si "sleep-worker" > 15 chars, se trunca)
- PID C: comm=`sleep`, cmd=`sleep 202`

**killall sleep**: Solo mata el PID C (comm exacto = "sleep").
`sleepd` y `sleep-worker` no coinciden con "sleep" exacto.

**pkill sleep**: Mata TODOS — `sleepd`, `sleep-worker`, y `sleep`
porque pkill hace **substring match** por defecto. "sleep" es
substring de "sleepd" y "sleep-worker".

**Moraleja**:
- `killall` = nombre exacto → más seguro pero menos flexible
- `pkill` = substring regex → más flexible pero riesgo de matar
  de más si no se es específico
- `pkill -x sleep` = equivalente a killall (match exacto)

</details>

---

### Ejercicio 5 — pgrep antes de pkill: la regla de oro

```bash
bash -c 'exec -a worker-web sleep 200' &
bash -c 'exec -a worker-api sleep 201' &
bash -c 'exec -a worker-batch sleep 202' &
bash -c 'exec -a logger-worker sleep 203' &
sleep 0.5

echo "=== ¿Qué mataría 'pkill worker'? ==="
pgrep -a worker
echo "(¡también mataría logger-worker!)"

echo ""
echo "=== Patrón más específico ==="
pgrep -a '^worker'
echo "(solo los que empiezan con worker)"

echo ""
echo "=== Matar solo worker-api ==="
pgrep -ax worker-api
pkill -x worker-api
sleep 0.5

echo ""
echo "=== Verificar ==="
pgrep -a worker

# Limpiar
pkill worker 2>/dev/null; wait 2>/dev/null
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

`pgrep -a worker` muestra los 4 procesos (worker-web, worker-api,
worker-batch, logger-worker) porque "worker" es substring de todos.

`pgrep -a '^worker'` muestra solo worker-web, worker-api, worker-batch
(los que empiezan con "worker"). pkill soporta regex completo.

`pkill -x worker-api` mata solo el proceso con comm exacto
"worker-api". Los demás sobreviven.

**Flujo profesional:**
```
1. pgrep -a PATRÓN        # ¿qué coincide?
2. Ajustar patrón si es necesario
3. pkill [-x|-f] PATRÓN   # matar
4. pgrep -a PATRÓN        # verificar
```

</details>

---

### Ejercicio 6 — pkill -f: buscar en cmdline completa

```bash
bash -c 'python3 -c "import time; time.sleep(300)"' &
bash -c 'python3 -c "import time; time.sleep(301)"' &
sleep 0.5

echo "=== pgrep python3 (busca en comm) ==="
pgrep -a python3

echo ""
echo "=== pgrep -f 'sleep(300)' (busca en cmdline) ==="
pgrep -af 'sleep\(300\)'

echo ""
echo "=== Matar solo el primero ==="
pkill -f 'sleep\(300\)'
sleep 0.5
echo "Restantes:"
pgrep -a python3

# Limpiar
pkill python3 2>/dev/null; wait 2>/dev/null
```

Si no hay python3, usar sleeps:

```bash
sleep 300 &
bash -c 'sleep 300' &
sleep 0.5

echo "pgrep -a sleep:     $(pgrep -c sleep) procesos"
echo "pgrep -af 'bash.*sleep': $(pgrep -cf 'bash.*sleep') procesos"
echo ""
echo "pkill -f 'bash.*sleep' mata solo el que corre dentro de bash"
pkill -f 'bash.*sleep'
sleep 0.5
echo "Restantes: $(pgrep -c sleep)"

killall sleep 2>/dev/null; wait 2>/dev/null
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

Sin `-f`, `pgrep python3` busca en `/proc/PID/comm` (nombre del
ejecutable). Ambos procesos son "python3", así que coinciden ambos.

Con `-f`, `pgrep -af 'sleep\(300\)'` busca en `/proc/PID/cmdline`
completo. Solo el que tiene `sleep(300)` en su línea de comando
coincide.

**Cuidado con pkill -f**: Si el patrón es muy genérico, puede coincidir
con su propia línea de comando. pkill se excluye a sí mismo
automáticamente, pero si usas `kill $(pgrep -f patrón)`, pgrep
podría incluir su propio PID en la lista.

</details>

---

### Ejercicio 7 — pkill con filtros: -u, -P, -n, -o

```bash
sleep 100 &
PID_FIRST=$!
sleep 1
sleep 101 &
sleep 102 &
sleep 0.5

echo "=== Todos los sleep ==="
pgrep -a sleep
echo ""

echo "=== Solo de mi usuario (-u) ==="
pgrep -au $(whoami) sleep
echo ""

echo "=== Solo hijos de mi shell (-P) ==="
pgrep -aP $$ sleep
echo ""

echo "=== El más nuevo (-n) ==="
pgrep -na sleep
echo ""

echo "=== El más viejo (-o) ==="
pgrep -oa sleep
echo ""

echo "=== Matar solo el más viejo ==="
pkill -o sleep
sleep 0.5
echo "¿PID $PID_FIRST existe? $(kill -0 $PID_FIRST 2>/dev/null && echo SÍ || echo NO)"

killall sleep 2>/dev/null; wait 2>/dev/null
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

- `-u $(whoami)`: Solo los sleep del usuario actual (todos, en este
  caso).
- `-P $$`: Solo los sleep que son hijos directos del shell actual.
- `-n`: El sleep con PID más alto (el creado más recientemente,
  sleep 102).
- `-o`: El sleep con PID más bajo (el creado primero, sleep 100 =
  `$PID_FIRST`).

Después de `pkill -o sleep`: `$PID_FIRST` ya no existe (era el más
viejo). Los otros dos siguen vivos.

**Combinaciones útiles:**
- `pkill -u dev -o python3`: el python3 más viejo del usuario dev
- `pkill -P 1234 -f worker`: workers hijos del PID 1234

</details>

---

### Ejercicio 8 — Process groups: matar un pipeline

```bash
cat /dev/zero | gzip > /dev/null &
PIPELINE_PID=$!
sleep 0.5

PGID=$(ps -o pgid= -p $PIPELINE_PID | tr -d ' ')

echo "=== Procesos del pipeline ==="
ps -o pid,pgid,stat,cmd -g $PGID
echo ""
echo "PGID: $PGID"
echo "Todos comparten el mismo PGID"

echo ""
echo "=== Matar todo el group ==="
kill -- -$PGID
sleep 0.5

echo ""
echo "=== ¿Quedan? ==="
ps -o pid,pgid,cmd -g $PGID 2>/dev/null || echo "Grupo completo terminado"
wait 2>/dev/null
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

El pipeline `cat /dev/zero | gzip > /dev/null` crea 2 procesos:
- `cat /dev/zero` (PID A, PGID = A)
- `gzip` (PID B, PGID = A)

Ambos comparten el mismo PGID (el PID del primer proceso del pipeline
suele ser el PGID).

`kill -- -$PGID` envía SIGTERM a todos los procesos del grupo. Ambos
mueren. `--` es necesario para que `-$PGID` no se interprete como una
opción (flag) de kill.

**¿Por qué es útil?** Si mataras solo `cat`, `gzip` recibiría SIGPIPE
al intentar leer del pipe roto — y moriría también. Pero con `kill --
-PGID` es más limpio y predecible: todos reciben SIGTERM al mismo
tiempo.

</details>

---

### Ejercicio 9 — Patrón completo: service stop manual

```bash
# Simular un servicio con master y workers
bash -c '
    echo "master PID=$$"
    for i in 1 2 3; do
        sleep 300 &
        echo "  worker $i PID=$!"
    done
    wait
' &
MASTER=$!
sleep 1

echo "=== Estado del servicio ==="
echo "Master: $MASTER"
echo "Workers (hijos de $MASTER):"
pgrep -laP $MASTER
echo ""

echo "=== Paso 1: SIGTERM al master ==="
kill $MASTER
sleep 2

echo "=== Paso 2: ¿Quedan workers? ==="
REMAINING=$(pgrep -cP $MASTER 2>/dev/null || echo 0)
echo "Workers restantes: $REMAINING"

if [ "$REMAINING" -gt 0 ] 2>/dev/null; then
    echo "=== Paso 3: SIGKILL a los rezagados ==="
    pkill -9 -P $MASTER
fi

sleep 0.5
echo "=== Verificación final ==="
pgrep -laP $MASTER 2>/dev/null || echo "Todos terminados"
wait 2>/dev/null
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

El master bash crea 3 workers (sleep 300). `wait` hace que el master
espere a que todos terminen.

**SIGTERM al master**: El master (bash) recibe SIGTERM y muere (acción
por defecto). Los workers se vuelven huérfanos y son adoptados por
PID 1. Los workers **siguen vivos** (SIGTERM al padre no mata a los
hijos automáticamente).

**Workers restantes > 0**: Los sleep siguen corriendo. Necesitamos
matarlos explícitamente. `pkill -9 -P $MASTER` puede no funcionar
si ya fueron re-parenteados a PID 1.

**Lección**: Matar al padre no mata a los hijos. Para un shutdown
limpio de un servicio con workers:
1. Enviar SIGTERM al master (que debería tener un handler que
   reenvíe SIGTERM a sus hijos)
2. O usar `kill -- -$PGID` para matar todo el process group
3. O usar `pkill -P $MASTER` antes de matar al master

</details>

---

### Ejercicio 10 — kill builtin: jobs y fork bomb recovery

```bash
echo "=== kill builtin maneja jobs ==="
sleep 300 &
sleep 301 &
echo "Jobs:"
jobs -l

echo ""
echo "=== Matar job %1 con builtin ==="
kill %1 2>/dev/null
sleep 0.5
echo "Después:"
jobs -l

echo ""
echo "=== Matar job %2 ==="
kill %2 2>/dev/null
sleep 0.5
jobs -l

echo ""
echo "=== ¿Por qué importa que kill sea builtin? ==="
echo "En una fork bomb o con memoria agotada:"
echo "  /usr/bin/kill necesita fork() → falla"
echo "  kill (builtin) no necesita nuevo proceso → funciona"
echo ""
echo "Si el sistema está colgado por fork bomb:"
echo "  1. kill -9 -1    (mata todos tus procesos)"
echo "  2. El builtin no necesita crear proceso → puede ejecutarse"
echo "  3. Esto mata la fork bomb"
wait 2>/dev/null
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

`jobs -l` muestra los dos sleeps con sus job IDs ([1] y [2]).

`kill %1` mata el job 1. Solo un job queda en `jobs -l`.

`kill %2` mata el job 2. No quedan jobs.

**¿Por qué importa?** El builtin `kill` se ejecuta dentro del proceso
del shell (no necesita fork). El comando externo `/usr/bin/kill` es
un programa separado que necesita fork() + exec().

En una **fork bomb** (`:(){ :|:& };:`), el sistema agota su límite
de procesos. `fork()` falla para todos. Pero el builtin `kill` no
necesita fork — se ejecuta directamente en tu bash existente.

Solución para fork bomb desde un shell:
```bash
kill -9 -1     # builtin: mata todos tus procesos (incluida la bomba)
```

Si no tienes un shell abierto, necesitas acceso físico o OOM killer.

</details>
