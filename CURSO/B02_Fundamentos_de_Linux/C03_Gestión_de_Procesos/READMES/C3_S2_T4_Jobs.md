# T04 — Jobs

## Errata y aclaraciones sobre el material original

### Error: ejercicio 4 — `$!` con setsid en shell interactivo

En el ejercicio 4 (README.max.md):

```bash
setsid sleep 200 &
PID=$!
```

En un shell interactivo con job control, bash coloca cada background job en su
propio process group (el proceso es group leader). Cuando `setsid` detecta que
ya es group leader, **forks**: el padre (cuyo PID captura `$!`) termina
inmediatamente, y el hijo crea la nueva sesión y ejecuta `sleep`. Resultado:
`ps -p $PID` no encuentra nada porque ese PID ya no existe.

**Solución**: Usar `pgrep` para localizar el proceso real:

```bash
setsid sleep 200 </dev/null &>/dev/null &
sleep 0.3
PID=$(pgrep -n -f "sleep 200")
```

### Error: ejercicio 9 — `kill %sleep` con múltiples jobs

```bash
sleep 100 &
sleep 200 &
sleep 300 &
kill %sleep    # ERROR: bash: kill: %sleep: ambiguous job spec
```

El job spec `%string` requiere un match único. Con tres jobs que empiezan con
"sleep", bash lo rechaza como ambiguo. Para matar todos: `kill %1 %2 %3`.

### Error: ejercicio 7 — título no corresponde al contenido

El ejercicio dice "Ctrl+Z + bg + fg" pero usa `kill -SIGSTOP` / `kill -SIGCONT`
y nunca emplea `bg` ni `fg`. El título es engañoso.

### Aclaración: disown NO hace que el proceso "ignore" SIGHUP

Las tablas de comparación en los labs dicen que disown hace SIGHUP → "Ignora".
Esto es impreciso. Los tres mecanismos protegen de forma diferente:

| Mecanismo | Nivel | Cómo protege |
|---|---|---|
| `nohup` | Señal | `signal(SIGHUP, SIG_IGN)` en el proceso — ignora SIGHUP de **cualquier** fuente |
| `disown` | Shell | Quita de la tabla de jobs — el **shell** no envía SIGHUP al cerrar |
| `setsid` | Sesión | Crea nueva sesión sin terminal de control — SIGHUP del terminal no le llega |

Si algo distinto al shell envía SIGHUP a un proceso con `disown` (por ejemplo,
`kill -HUP $PID`), ese proceso **morirá**. Con `nohup`, **sobrevivirá**.

### Aclaración: SIGTSTP vs SIGSTOP

Ctrl+Z envía **SIGTSTP** (Terminal Stop), no SIGSTOP:

| Señal | Nº | Capturable | Uso |
|---|---|---|---|
| SIGTSTP | 20 | **Sí** | Ctrl+Z — el proceso puede limpiar antes de detenerse |
| SIGSTOP | 19 | **No** | `kill -STOP` — detención incondicional |

Programas como `vim` capturan SIGTSTP para restaurar el terminal antes de
detenerse. Si recibiesen SIGSTOP directamente, el terminal quedaría en modo
raw.

### Typo: README.max.md línea 7

"suspederlos" → "suspenderlos"

---

## Job control

El **job control** es una funcionalidad del shell que permite gestionar múltiples
procesos desde un solo terminal: enviar procesos al fondo, traerlos de vuelta,
suspenderlos y reanudarlos.

```
Foreground (primer plano):
  - El proceso recibe input del teclado
  - Solo un job puede estar en foreground a la vez
  - Ctrl+C, Ctrl+Z, Ctrl+\ van al foreground process group

Background (segundo plano):
  - El proceso ejecuta sin recibir input del teclado
  - Múltiples jobs pueden estar en background simultáneamente
  - Si intenta leer del terminal → SIGTTIN (se detiene)
  - Si intenta escribir al terminal → SIGTTOU (solo si stty tostop)
```

> **SIGTTIN y SIGTTOU**: Un proceso background que intenta **leer** del terminal
> recibe SIGTTIN y se detiene automáticamente. Para **escritura**, por defecto SÍ
> puede escribir al terminal (verás output mezclado con tu prompt). Solo si
> activas `stty tostop`, la escritura genera SIGTTOU y lo detiene también.

## Operaciones básicas

### Iniciar en background con &

```bash
# & al final del comando lo envía al background
sleep 300 &
# [1] 1234
#  ^   ^^^^
#  |   PID del proceso
#  job number

make -j4 &
# [2] 1235

# El shell imprime el job number y el PID
# El prompt vuelve inmediatamente — puedes seguir trabajando
```

### Suspender con Ctrl+Z

```bash
# Ctrl+Z envía SIGTSTP al foreground process group
# El proceso se DETIENE (no termina)

vim largefile.txt
# (presionar Ctrl+Z)
# [1]+  Stopped                 vim largefile.txt

# El proceso está congelado en memoria
# Su estado en ps es T (stopped)

# SIGTSTP es capturable: vim restaura el terminal
# antes de detenerse. kill -STOP no da esa oportunidad.
```

### Ver jobs activos

```bash
jobs
# [1]+  Stopped                 vim largefile.txt
# [2]   Running                 make -j4 &
# [3]-  Running                 sleep 300 &

# + = job actual (default para fg/bg sin argumento)
# - = job anterior

# jobs -l: incluir PIDs
jobs -l
# [1]+  1234 Stopped              vim largefile.txt
# [2]   1235 Running              make -j4 &
# [3]-  1236 Running              sleep 300 &

# jobs -p: solo PIDs (uno por job)
jobs -p
# 1234
# 1235
# 1236

# jobs -r: solo running
# jobs -s: solo stopped
```

### bg — Reanudar en background

```bash
# Reanudar el job detenido en background
bg           # reanuda el job marcado con +
bg %1        # reanuda el job 1 específicamente

# El proceso recibe SIGCONT y continúa ejecutando
# pero en background (no recibe input del teclado)

# Flujo típico:
make -j4          # inicia en foreground
# (Ctrl+Z)        # suspender → SIGTSTP
# [1]+  Stopped   make -j4
bg                # reanudar en background → SIGCONT
# [1]+ make -j4 &
```

### fg — Traer al foreground

```bash
# Traer un job al foreground
fg           # trae el job marcado con +
fg %1        # trae el job 1
fg %vim      # trae el job que empieza con "vim"

# Ahora el proceso recibe input del teclado
# y Ctrl+C/Ctrl+Z lo afectan directamente
```

### Especificar jobs (job specs)

```bash
%1        # job número 1
%2        # job número 2
%+        # job actual (último suspendido o puesto en bg)
%%        # igual que %+
%-        # job anterior
%string   # job cuyo comando empieza con "string"
%?string  # job cuyo comando contiene "string"

# Ejemplos
fg %1           # traer job 1
kill %2         # matar job 2
bg %vim         # reanudar el job que empieza con "vim"
fg %?largefile  # traer el job que contiene "largefile"

# Cuidado con ambigüedad:
sleep 100 &
sleep 200 &
kill %sleep     # ERROR: ambiguous job spec (dos matches)
kill %?200      # OK: solo un job contiene "200"
```

## wait — Esperar jobs en background

```bash
# wait espera a que un job termine y devuelve su exit status
sleep 5 &
PID=$!
echo "Esperando..."
wait $PID
echo "Exit status: $?"    # 0

# Esperar por job spec
sleep 5 &
wait %1
echo "Terminó: $?"

# wait sin argumentos: espera TODOS los background jobs
sleep 3 &
sleep 5 &
wait
echo "Todos terminaron"

# Patrón: paralelizar trabajo y esperar
for file in data1.csv data2.csv data3.csv data4.csv; do
    process_file "$file" &
done
wait    # espera los 4
echo "Todos los archivos procesados"
```

> **`$!`** captura el PID del último proceso puesto en background. Si necesitas
> el PID de cada uno, guárdalo inmediatamente: `cmd1 & PID1=$!; cmd2 & PID2=$!`.

## El problema: cerrar el terminal

Cuando se cierra un terminal (o se hace logout), el kernel envía **SIGHUP** al
session leader (el shell). El shell reenvía SIGHUP a todos sus jobs:

```
Terminal abierto:
  shell (session leader, SID=1000)
    ├── job 1 (background, PGID=1001)
    ├── job 2 (background, PGID=1002)
    └── job 3 (foreground, PGID=1003)

Terminal cerrado → SIGHUP:
  kernel → SIGHUP al session leader (shell)
  shell → reenvía SIGHUP a cada job de su tabla
  procesos → mueren (acción por defecto de SIGHUP)
```

La cadena es: **cierre de terminal → kernel → SIGHUP al session leader → shell
reenvía a sus jobs → procesos mueren**.

Hay tres mecanismos para evitar esto, cada uno a un nivel diferente.

## nohup — Ignorar SIGHUP (nivel: señal)

`nohup` ejecuta un comando haciendo que **ignore SIGHUP** (`SIG_IGN`):

```bash
# Sintaxis
nohup comando [argumentos] &

# Ejemplo
nohup python3 /srv/app/server.py &
# nohup: ignoring input and appending output to 'nohup.out'
# [1] 1234

# Lo que hace nohup:
# 1. signal(SIGHUP, SIG_IGN) — el proceso ignora SIGHUP
# 2. Redirige stdout a nohup.out (si stdout es un terminal)
# 3. Redirige stderr a stdout (si stderr es un terminal)
# 4. No cambia stdin
```

### Redireccionar output con nohup

```bash
# nohup escribe a nohup.out por defecto — mejor especificar
nohup python3 server.py > /var/log/server.log 2>&1 &

# Desglose:
# > /var/log/server.log    stdout va al log
# 2>&1                     stderr va donde va stdout (al log)
# &                        background

# Con stdout y stderr separados
nohup python3 server.py > /var/log/server.out 2> /var/log/server.err &
```

### Limitaciones de nohup

```bash
# nohup solo protege contra SIGHUP
# SIGTERM, SIGKILL, etc. siguen matando el proceso

# nohup no desacopla completamente del terminal:
# - El proceso sigue en la misma sesión (mismo SID)
# - Si el shell muere, el proceso queda huérfano
#   (re-parenteado a PID 1 o subreaper)
# - Funciona, pero no es tan limpio como setsid o un daemon
```

## disown — Desasociar del shell (nivel: tabla de jobs)

`disown` elimina un job de la tabla de jobs del shell. El shell ya no le
enviará SIGHUP cuando se cierre:

```bash
# Iniciar en background
python3 server.py &
# [1] 1234

# Desasociar del shell
disown %1
# o
disown        # desasocia el job actual (+)

# Ahora no aparece en jobs
jobs
# (vacío)

# Pero el proceso sigue corriendo
ps -p 1234
```

### disown -h — Proteger sin quitar de la tabla

```bash
# -h: no enviar SIGHUP al cerrar, pero mantener en la tabla de jobs
disown -h %1

# El job sigue apareciendo en jobs
jobs
# [1]+  Running    python3 server.py &

# Pero no recibirá SIGHUP al cerrar el terminal
# Y puedes seguir usando fg %1 / bg %1
```

### nohup vs disown — diferencia real

| Aspecto | nohup | disown |
|---|---|---|
| Cuándo se usa | Al **iniciar** el proceso | **Después** de iniciar |
| Mecanismo | `SIG_IGN` en el proceso | Quita de tabla del shell |
| SIGHUP manual (`kill -HUP`) | **Sobrevive** (ignora) | **Muere** (disposición por defecto) |
| Output | Redirige a `nohup.out` | No redirige — output se pierde |
| Disponibilidad | Comando externo (`/usr/bin/nohup`) | Builtin del shell (bash/zsh) |

```bash
# Patrón "olvidé nohup":
python3 server.py        # ups, lo inicié sin nohup
# Ctrl+Z                  # suspender
bg                       # reanudar en background
disown -h %1             # proteger contra SIGHUP del shell

# NOTA: solo protege contra SIGHUP del shell al cerrar.
# Si algo más envía SIGHUP, el proceso muere.
# Mejor haber usado nohup desde el inicio.
```

## setsid — Crear nueva sesión (nivel: sesión)

`setsid` ejecuta un programa en una **nueva sesión** completamente desacoplada
del terminal:

```bash
setsid python3 server.py &

# Lo que hace:
# 1. Crea una nueva sesión (el proceso es session leader)
# 2. No tiene terminal de control
# 3. SIGHUP del terminal original no le llega
# 4. Es la base de cómo funcionan los daemons
```

### Tres mecanismos, un resultado

| Mecanismo | Nivel | Qué modifica | SIGHUP externo |
|---|---|---|---|
| `nohup` | Señal | Disposición → SIG_IGN | Sobrevive |
| `disown` | Shell | Tabla de jobs → no envía | Muere |
| `setsid` | Sesión | Nueva sesión → sin terminal | No le llega del terminal |

`setsid` es el más "limpio" porque el proceso no pertenece a la sesión del
terminal. Los daemons reales usan esta técnica (o la llamada al sistema
`daemon()`).

## screen y tmux — Terminales persistentes

Para procesos interactivos de larga duración (no daemons), la solución
correcta es un multiplexor de terminal:

```bash
# tmux: crear una sesión
tmux new -s trabajo

# (ejecutar comandos dentro de tmux)
make -j4

# Desconectar de tmux: Ctrl+B, luego D
# El terminal tmux sigue vivo en background

# Reconectar (incluso desde otra sesión SSH)
tmux attach -t trabajo
# o
tmux a -t trabajo

# Listar sesiones activas
tmux ls
```

```bash
# screen: equivalente más antiguo
screen -S trabajo

# Desconectar: Ctrl+A, luego D
# Reconectar:
screen -r trabajo
```

tmux/screen funcionan porque el proceso corre dentro del **servidor
tmux/screen** (un proceso separado con su propia sesión), no dentro del
terminal SSH. Cerrar SSH no afecta al servidor tmux.

## Comparación de métodos

| Método | Sobrevive logout | Interactivo | Output | Re-attach | Complejidad |
|---|---|---|---|---|---|
| `cmd &` | No | No | Terminal | No | Mínima |
| `nohup cmd &` | Sí | No | `nohup.out` | No | Baja |
| `disown` | Sí* | No | Se pierde | No | Baja |
| `setsid cmd` | Sí | No | Se pierde | No | Baja |
| `tmux`/`screen` | Sí | Sí | Visible | Sí | Media |
| systemd service | Sí | No | journald | N/A | Media-Alta |

\* disown protege solo contra SIGHUP del shell. Si otro proceso envía SIGHUP,
no sobrevive.

Para servicios en producción, la respuesta correcta es casi siempre un **unit
file de systemd**, no nohup ni screen.

## Diferencias entre shells

| Funcionalidad | bash | zsh |
|---|---|---|
| Job control | Sí | Sí |
| disown | Sí | Sí |
| `setopt NO_HUP` | — | Sí (no envía SIGHUP al cerrar) |
| `setopt NO_CHECK_JOBS` | — | Sí (no avisa de jobs al salir) |
| Aviso al salir con jobs | "There are stopped jobs" | Configurable |
| PGID en pipeline | PID del primer proceso | PID del último proceso |

```bash
# bash: si intentas salir con jobs pendientes
exit
# There are running jobs.
# (hay que hacer exit otra vez para forzar)

# zsh: se puede desactivar el aviso
setopt NO_CHECK_JOBS
setopt NO_HUP          # no enviar SIGHUP a los jobs
```

## Tabla de teclas y comandos de job control

| Tecla / Comando | Señal | Acción |
|---|---|---|
| `Ctrl+Z` | SIGTSTP | Suspender proceso foreground (capturable) |
| `Ctrl+C` | SIGINT | Interrumpir proceso foreground |
| `Ctrl+\` | SIGQUIT | Quit con core dump |
| `bg [%spec]` | SIGCONT | Reanudar job detenido en background |
| `fg [%spec]` | — | Traer job a foreground |
| `jobs -l` | — | Ver jobs con PIDs |
| `wait [spec]` | — | Esperar a que job termine |
| `disown %n` | — | Desasociar job n del shell |
| `nohup cmd &` | — | Ejecutar inmune a SIGHUP |
| `setsid cmd` | — | Nueva sesión sin terminal de control |

---

## Labs

### Parte 1 — Control de jobs

#### Paso 1.1: Lanzar en background con &

```bash
sleep 300 &
echo "PID: $!"

sleep 301 &
echo "Segundo PID: $!"

echo "=== Listar jobs ==="
jobs
# [N]  = número de job
# +    = job actual (default para fg/bg)
# -    = job anterior

kill %1 %2 2>/dev/null
wait 2>/dev/null
```

#### Paso 1.2: jobs con opciones

```bash
sleep 300 &
sleep 301 &
sleep 302 &

echo "=== jobs -l (con PID) ==="
jobs -l

echo "=== jobs -p (solo PIDs) ==="
jobs -p

echo "=== jobs -r (solo running) ==="
jobs -r

echo "=== jobs -s (solo stopped) ==="
jobs -s

kill %1 %2 %3 2>/dev/null
wait 2>/dev/null
```

#### Paso 1.3: Job specs

```bash
sleep 300 &
sleep 301 &
sleep 302 &
sleep 0.5

echo "Jobs:"
jobs -l

echo "Especificadores:"
echo "  %1        — job número 1"
echo "  %+  o %%  — job actual (el +)"
echo "  %-        — job anterior (el -)"
echo "  %?301     — job cuyo comando contiene '301'"

echo "=== kill %?301 ==="
kill %?301
sleep 0.5
jobs -l

kill %1 %3 2>/dev/null
wait 2>/dev/null
```

> **Nota**: `%sleep` daría "ambiguous job spec" porque los tres jobs empiezan
> con "sleep". Usar `%?301` (substring) para elegir uno específico.

#### Paso 1.4: Simular Ctrl+Z y fg/bg

```bash
sleep 300 &
PID=$!
echo "Job en background (PID $PID):"
jobs -l

echo "=== kill -TSTP (equivalente a Ctrl+Z) ==="
kill -TSTP $PID
sleep 0.5
echo "Después de SIGTSTP:"
jobs -l

echo "=== bg %1 (reanudar en background) ==="
bg %1 2>/dev/null
sleep 0.5
echo "Después de bg:"
jobs -l

kill $PID
wait $PID 2>/dev/null

echo ""
echo "En terminal interactivo:"
echo "  Ctrl+Z  — suspende foreground (SIGTSTP, capturable)"
echo "  fg %N   — mueve job N a foreground"
echo "  bg %N   — reanuda job N en background"
echo "  kill -STOP — detención incondicional (no capturable)"
```

### Parte 2 — Sobrevivir al cierre de terminal

#### Paso 2.1: SIGHUP al cerrar terminal

```bash
echo "=== Qué pasa al cerrar la terminal ==="
echo "1. El terminal se cierra"
echo "2. El kernel envía SIGHUP al session leader (shell)"
echo "3. El shell reenvía SIGHUP a cada job de su tabla"
echo "4. Los procesos sin handler para SIGHUP mueren"
echo ""
echo "Mecanismos de protección:"
echo "  nohup:  cambia disposición de señal → SIG_IGN"
echo "  disown: quita de la tabla → shell no envía SIGHUP"
echo "  setsid: nueva sesión → sin terminal de control"
```

#### Paso 2.2: nohup

```bash
nohup sleep 300 &
PID=$!
echo "PID: $PID"

ps -o pid,stat,cmd -p $PID
echo "Output va a nohup.out:"
ls -la nohup.out 2>/dev/null || echo "(no creado)"

nohup sleep 301 > /tmp/mi-log.txt 2>&1 &
PID2=$!
echo "Con redirección explícita, nohup.out no se crea"

kill $PID $PID2 2>/dev/null
wait 2>/dev/null
rm -f nohup.out /tmp/mi-log.txt
```

#### Paso 2.3: disown y disown -h

```bash
# disown: quita de la tabla
sleep 300 &
PID=$!
echo "Antes de disown:"
jobs -l

disown %1
echo "Después de disown:"
jobs -l
echo "(tabla vacía, pero proceso vivo)"
ps -o pid,stat,cmd -p $PID

kill $PID 2>/dev/null
wait $PID 2>/dev/null

# disown -h: protege pero mantiene en tabla
sleep 301 &
PID=$!
echo ""
echo "Antes de disown -h:"
jobs -l
disown -h %1
echo "Después de disown -h:"
jobs -l
echo "(sigue en tabla, pero no recibirá SIGHUP del shell)"

kill $PID 2>/dev/null
wait $PID 2>/dev/null
```

#### Paso 2.4: Diferencia real — SIGHUP manual

```bash
# nohup: sobrevive a SIGHUP de cualquier fuente
nohup sleep 300 > /dev/null 2>&1 &
PID_NOHUP=$!

# disown: solo protege contra SIGHUP del shell
sleep 301 &
PID_DISOWN=$!
disown %1

# Enviar SIGHUP manual a ambos
kill -HUP $PID_NOHUP
kill -HUP $PID_DISOWN
sleep 0.5

echo "nohup (SIGHUP manual):"
ps -o pid,cmd -p $PID_NOHUP 2>/dev/null && echo "  VIVO" || echo "  MUERTO"
echo "disown (SIGHUP manual):"
ps -o pid,cmd -p $PID_DISOWN 2>/dev/null && echo "  VIVO" || echo "  MUERTO"

# nohup sobrevive porque SIG_IGN. disown muere porque disposición por defecto.

kill $PID_NOHUP 2>/dev/null
wait 2>/dev/null
rm -f nohup.out
```

### Parte 3 — Comparación de mecanismos

#### Paso 3.1: Tabla resumen

```bash
echo "=== Tres niveles de protección ==="
printf "%-12s %-25s %-20s\n" "Método" "Mecanismo" "SIGHUP externo"
printf "%-12s %-25s %-20s\n" "----------" "-----------------------" "------------------"
printf "%-12s %-25s %-20s\n" "nohup"  "SIG_IGN en el proceso"   "Sobrevive"
printf "%-12s %-25s %-20s\n" "disown" "Quita de tabla del shell" "Muere"
printf "%-12s %-25s %-20s\n" "setsid" "Nueva sesión sin TTY"    "No le llega"

echo ""
echo "Recomendación:"
echo "  One-off:                nohup cmd > log 2>&1 &"
echo "  Sesión interactiva:    tmux"
echo "  Servicio permanente:   systemd"
```

---

## Ejercicios

### Ejercicio 1 — Job control básico

```bash
# Lanzar tres procesos en background
sleep 100 &
sleep 200 &
sleep 300 &

# a) ¿Cuántos jobs hay?
jobs

# b) ¿Cuál tiene el + y cuál el -?
jobs -l

# c) Suspender el job 2
kill -TSTP %2
sleep 0.5
jobs

# d) Reanudar job 2 en background
bg %2
sleep 0.5
jobs

# e) Matar todos
kill %1 %2 %3
wait 2>/dev/null
```

<details><summary>Predicción</summary>

a) 3 jobs, todos Running.

b) `+` en job 3 (el más reciente), `-` en job 2 (el anterior). El marcador
`+` indica el job "actual" — el que `fg` y `bg` sin argumento afectan.

c) Job 2 aparece como `Stopped`, los demás siguen `Running`. SIGTSTP lo
detiene pero no lo mata.

d) Job 2 vuelve a `Running`. `bg` envía SIGCONT al job detenido.

e) Los tres reciben SIGTERM y terminan.

</details>

### Ejercicio 2 — Job specs y ambigüedad

```bash
sleep 100 &
sleep 200 &
dd if=/dev/zero of=/dev/null bs=1k 2>/dev/null &

sleep 0.5
echo "=== Todos los jobs ==="
jobs -l

# a) Intentar matar por prefijo ambiguo
echo "=== kill %sleep ==="
kill %sleep 2>&1

# b) Matar por substring único
echo "=== kill %?200 ==="
kill %?200 2>&1
sleep 0.5
jobs

# c) Matar por prefijo único
echo "=== kill %dd ==="
kill %dd 2>&1
sleep 0.5
jobs

# Limpieza
kill %1 2>/dev/null
wait 2>/dev/null
```

<details><summary>Predicción</summary>

a) Error: `bash: kill: %sleep: ambiguous job spec`. Hay dos jobs que empiezan
con "sleep" (job 1 y job 2), así que `%sleep` es ambiguo.

b) OK — solo un job contiene "200" (el segundo sleep). Se mata.

c) OK — solo un job empieza con "dd". Se mata.

Después de b): quedan sleep 100 y dd.
Después de c): queda solo sleep 100.

La regla: `%string` busca por **prefijo** del nombre del comando. `%?string`
busca por **substring** en toda la línea de comando. Ambos requieren match
único.

</details>

### Ejercicio 3 — wait y exit status

```bash
# a) Proceso que falla
bash -c 'sleep 1; exit 42' &
PID_FAIL=$!

# b) Proceso que tiene éxito
bash -c 'sleep 2; exit 0' &
PID_OK=$!

# c) Esperar al primero y capturar exit status
wait $PID_FAIL
echo "Exit status (falla): $?"

# d) Esperar al segundo
wait $PID_OK
echo "Exit status (éxito): $?"

# e) wait sin argumentos
bash -c 'sleep 1' &
bash -c 'sleep 2' &
echo "Esperando todos..."
wait
echo "Todos terminaron. Exit status: $?"
```

<details><summary>Predicción</summary>

c) `Exit status (falla): 42`. `wait $PID` bloquea hasta que el proceso termine
y devuelve su exit code en `$?`.

d) `Exit status (éxito): 0`.

e) `wait` sin argumentos espera a TODOS los background jobs. `$?` es el exit
status del último job que terminó (0).

Nota: `$!` captura el PID del último proceso puesto en background. Si
necesitas múltiples PIDs, hay que guardar `$!` después de cada `&`.

</details>

### Ejercicio 4 — Stopped vs Running: efecto real

```bash
# Proceso que escribe continuamente
bash -c 'i=0; while true; do echo $((i++)) >> /tmp/counter.txt; sleep 0.5; done' &
PID=$!

# a) Verificar que escribe
sleep 2
wc -l /tmp/counter.txt

# b) Detenerlo con SIGTSTP
kill -TSTP $PID
sleep 0.5
echo "Estado: $(ps -o stat= -p $PID)"
jobs

# c) ¿Sigue escribiendo?
BEFORE=$(wc -l < /tmp/counter.txt)
sleep 2
AFTER=$(wc -l < /tmp/counter.txt)
echo "Líneas antes: $BEFORE, después: $AFTER"

# d) Reanudar
bg %1 2>/dev/null || kill -CONT $PID
sleep 2
RESUMED=$(wc -l < /tmp/counter.txt)
echo "Después de reanudar: $RESUMED líneas"

# Limpieza
kill $PID 2>/dev/null
wait $PID 2>/dev/null
rm -f /tmp/counter.txt
```

<details><summary>Predicción</summary>

a) ~4 líneas (2 segundos / 0.5s por línea).

b) Estado: `T` (stopped). jobs muestra `Stopped`.

c) BEFORE y AFTER son **iguales**. El proceso está completamente congelado en
memoria — no ejecuta instrucciones, no escribe al archivo.

d) RESUMED > AFTER. Al recibir SIGCONT (vía `bg` o `kill -CONT`), el proceso
reanuda exactamente donde se detuvo y sigue escribiendo.

Un proceso en estado T consume memoria pero **cero** CPU. Es diferente de un
proceso en sleep (S), que sí puede despertar por timers o I/O.

</details>

### Ejercicio 5 — nohup: verificar protección contra SIGHUP

```bash
# a) Proceso SIN nohup
sleep 300 &
PID_NORMAL=$!

# b) Proceso CON nohup
nohup sleep 301 > /dev/null 2>&1 &
PID_NOHUP=$!

# c) Enviar SIGHUP a ambos
kill -HUP $PID_NORMAL
kill -HUP $PID_NOHUP
sleep 0.5

# d) ¿Cuál sobrevive?
echo "Normal:"
ps -o pid,stat,cmd -p $PID_NORMAL 2>/dev/null || echo "  Muerto"
echo "Nohup:"
ps -o pid,stat,cmd -p $PID_NOHUP 2>/dev/null || echo "  Muerto"

# e) Verificar disposición de señal en /proc
echo ""
echo "SigIgn del proceso nohup:"
grep SigIgn /proc/$PID_NOHUP/status 2>/dev/null

# Limpieza
kill $PID_NOHUP 2>/dev/null
wait 2>/dev/null
```

<details><summary>Predicción</summary>

d) Normal: **Muerto**. Nohup: **Vivo**. SIGHUP es la acción por defecto
"terminar". nohup establece SIG_IGN, así que el proceso ignora SIGHUP.

e) SigIgn muestra una máscara hexadecimal. SIGHUP = señal 1, corresponde al
bit 0 (valor `0x1`). Verás algo como `SigIgn: 0000000000000001`, indicando que
SIGHUP está ignorada.

El campo SigIgn refleja la **disposición de señal del proceso**. Esto es lo que
`nohup` modifica antes de ejecutar el comando. `disown` no toca esta
disposición — solo cambia la tabla interna del shell.

</details>

### Ejercicio 6 — disown vs disown -h

```bash
# a) disown completo
sleep 100 &
PID1=$!
echo "Antes de disown:"
jobs -l
disown %1
echo "Después de disown:"
jobs -l

# b) disown -h
sleep 200 &
PID2=$!
echo ""
echo "Antes de disown -h:"
jobs -l
disown -h %1
echo "Después de disown -h:"
jobs -l

# c) ¿Cuál muere con SIGHUP manual?
kill -HUP $PID1
kill -HUP $PID2
sleep 0.5
echo ""
echo "disown (PID $PID1):"
ps -o pid,cmd -p $PID1 2>/dev/null && echo "  Vivo" || echo "  Muerto"
echo "disown -h (PID $PID2):"
ps -o pid,cmd -p $PID2 2>/dev/null && echo "  Vivo" || echo "  Muerto"

# Limpieza
kill $PID1 $PID2 2>/dev/null
wait 2>/dev/null
```

<details><summary>Predicción</summary>

a) Después de `disown`: la tabla de jobs está **vacía**. El job fue eliminado.

b) Después de `disown -h`: el job **sigue en la tabla** (Running). La diferencia
es que con `-h` puedes seguir usando `fg`/`bg` con ese job.

c) **AMBOS mueren**. Tanto `disown` como `disown -h` solo protegen contra
SIGHUP **enviado por el shell al cerrar**. Ninguno cambia la disposición de
señal del proceso. Un `kill -HUP` manual envía SIGHUP directamente al proceso,
que tiene la disposición por defecto (terminar).

Esta es la diferencia clave con `nohup`: nohup establece `SIG_IGN` en el
proceso, haciéndolo inmune a SIGHUP de **cualquier** fuente.

</details>

### Ejercicio 7 — setsid: verificar nueva sesión

```bash
# a) Proceso normal: mismo SID que el shell
sleep 100 &
PID_NORMAL=$!
SID_SHELL=$(ps -o sid= -p $$ | tr -d ' ')
SID_NORMAL=$(ps -o sid= -p $PID_NORMAL | tr -d ' ')
echo "Shell:  PID=$$, SID=$SID_SHELL"
echo "Normal: PID=$PID_NORMAL, SID=$SID_NORMAL"

# b) Proceso con setsid: SID diferente
setsid sleep 101 </dev/null &>/dev/null &
sleep 0.3
PID_SETSID=$(pgrep -n -f "sleep 101")
if [ -n "$PID_SETSID" ]; then
    SID_SETSID=$(ps -o sid= -p $PID_SETSID | tr -d ' ')
    echo "Setsid: PID=$PID_SETSID, SID=$SID_SETSID"
else
    echo "No se encontró el proceso setsid"
fi

# c) ¿Tiene terminal de control?
echo ""
echo "TTY del shell:    $(ps -o tty= -p $$)"
echo "TTY del normal:   $(ps -o tty= -p $PID_NORMAL)"
echo "TTY del setsid:   $(ps -o tty= -p $PID_SETSID 2>/dev/null)"

# Limpieza
kill $PID_NORMAL $PID_SETSID 2>/dev/null
wait 2>/dev/null
```

<details><summary>Predicción</summary>

a) SID_NORMAL = SID_SHELL. Los procesos hijos heredan la sesión del padre.

b) SID_SETSID ≠ SID_SHELL. `setsid` crea una nueva sesión. Además,
SID_SETSID = PID_SETSID — el proceso es su propio **session leader** (SID =
PID del líder).

c) Shell y normal: `pts/0` (o similar). Setsid: `?` (sin terminal de control).

Al no tener terminal de control, el proceso con `setsid` no recibirá SIGHUP
cuando se cierre el terminal original. Es el mecanismo más limpio de los tres.

Nota: usamos `pgrep` en vez de `$!` porque en un shell interactivo, `setsid`
forks cuando el proceso ya es group leader, y `$!` captura el PID del padre
que termina inmediatamente.

</details>

### Ejercicio 8 — SIGTTIN: background y lectura del terminal

```bash
# a) Proceso background que intenta leer del terminal
bash -c 'sleep 0.5; read line; echo "Leí: $line"' &
PID=$!
sleep 1

# b) ¿Qué pasó?
echo "Estado:"
jobs -l
echo "ps stat: $(ps -o stat= -p $PID 2>/dev/null)"

# c) ¿Y un proceso que escribe?
echo "test" | cat &
sleep 0.5
echo ""
echo "cat con pipe (no lee del terminal):"
jobs

# Limpieza
kill $PID 2>/dev/null
wait 2>/dev/null
```

<details><summary>Predicción</summary>

a-b) El proceso se detuvo. Estado: `T` (Stopped). jobs muestra `Stopped (tty
input)`.

Cuando un proceso en background intenta **leer del terminal**, el kernel envía
**SIGTTIN** a todo el process group. La acción por defecto de SIGTTIN es
detener el proceso (como SIGTSTP). Para que continúe, habría que traerlo a
foreground con `fg`.

c) `cat` con pipe lee de la pipe, no del terminal. No recibe SIGTTIN.
Termina normalmente con estado `Done`.

Por defecto, un proceso background SÍ puede **escribir** al terminal (el
output se mezcla con tu prompt). Solo si se activa `stty tostop`, escribir
genera **SIGTTOU** y detiene el proceso.

</details>

### Ejercicio 9 — Process group en pipeline

```bash
# a) Pipeline en background
sleep 501 | sleep 502 | sleep 503 &
sleep 0.5

# b) Ver el job
jobs -l

# c) Verificar PGID compartido
echo "Procesos del pipeline:"
for PID in $(pgrep -f "sleep 50[123]"); do
    PGID=$(ps -o pgid= -p $PID | tr -d ' ')
    CMD=$(ps -o args= -p $PID)
    echo "  PID=$PID  PGID=$PGID  CMD=$CMD"
done

# d) Matar todo el group con un solo comando
PGID=$(ps -o pgid= -p $(pgrep -n -f "sleep 501") | tr -d ' ')
echo ""
echo "Matando process group $PGID"
kill -- -$PGID
sleep 0.5
jobs
```

<details><summary>Predicción</summary>

b) `jobs -l` muestra un solo job con múltiples PIDs. Un pipeline es un solo
job para el shell.

c) Los tres sleep tienen el **mismo PGID**. En bash, el PGID es el PID del
**primer** proceso del pipeline (`sleep 501`). En zsh sería el último.

d) `kill -- -$PGID` (nota el `-` antes del PGID) envía SIGTERM a **todos** los
procesos del process group. Los tres mueren con un solo comando.

El `--` es necesario para que `kill` no interprete `-$PGID` como un flag.
Sin `--`, kill interpretaría `-1234` como la señal 1234 (que no existe).

Este mecanismo es cómo el shell implementa `kill %1` internamente: envía la
señal al process group del job.

</details>

### Ejercicio 10 — Patrón "olvidé nohup" y mecanismos

```bash
# Simular: lanzaste un proceso largo sin nohup
bash -c 'i=0; while true; do echo "$(date +%H:%M:%S) tick $((i++))" >> /tmp/daemon-test.log; sleep 1; done' &
PID_DISOWN=$!
echo "PID del proceso: $PID_DISOWN"

# a) Verificar que escribe
sleep 2
tail -2 /tmp/daemon-test.log

# b) Protegerlo con disown -h (el patrón "olvidé nohup")
disown -h %1
echo "Protegido con disown -h"
jobs -l

# c) ¿Sobrevive a SIGHUP manual? (simular cierre de terminal)
kill -HUP $PID_DISOWN
sleep 0.5
echo "¿Vive tras SIGHUP manual?"
ps -o pid,cmd -p $PID_DISOWN 2>/dev/null && echo "Vivo" || echo "Muerto"

# d) Comparar: nohup SÍ sobrevive
nohup bash -c 'while true; do sleep 1; done' > /dev/null 2>&1 &
PID_NOHUP=$!
kill -HUP $PID_NOHUP
sleep 0.5
echo ""
echo "¿nohup sobrevive SIGHUP manual?"
ps -o pid,cmd -p $PID_NOHUP 2>/dev/null && echo "Vivo — SIG_IGN protege" || echo "Muerto"

# e) Verificar SigIgn
echo ""
echo "SigIgn del proceso nohup:"
grep SigIgn /proc/$PID_NOHUP/status 2>/dev/null

# Limpieza
kill $PID_DISOWN $PID_NOHUP 2>/dev/null
wait 2>/dev/null
rm -f /tmp/daemon-test.log
```

<details><summary>Predicción</summary>

a) El log muestra ticks con timestamp, confirmando que el proceso escribe.

b) `disown -h` marca el job para no recibir SIGHUP del shell al cerrar, pero
lo mantiene en la tabla de jobs (sigue visible con `jobs -l`).

c) **Muerto**. `disown -h` solo impide que el **shell** envíe SIGHUP al cerrar.
Un `kill -HUP` manual envía SIGHUP directamente al proceso, que tiene la
disposición por defecto (terminar). El patrón "olvidé nohup" protege contra
el cierre de terminal, pero no contra SIGHUP de otras fuentes.

d) **Vivo**. `nohup` establece `SIG_IGN` para SIGHUP en el proceso, así que
ignora SIGHUP de **cualquier** fuente.

e) `SigIgn: 0000000000000001` — el bit 0 indica SIGHUP (señal 1) ignorada.

**Resumen de mecanismos**:
- `nohup`: protección a nivel de **proceso** (SIG_IGN)
- `disown`: protección a nivel de **shell** (tabla de jobs)
- `setsid`: protección a nivel de **sesión** (sin terminal de control)

Para máxima seguridad en producción: usar systemd services.

</details>
