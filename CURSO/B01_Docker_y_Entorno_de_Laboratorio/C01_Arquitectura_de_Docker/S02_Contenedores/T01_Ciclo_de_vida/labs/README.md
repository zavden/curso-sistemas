# Lab — Ciclo de vida

## Objetivo

Laboratorio practico para verificar de forma empirica los estados de un
contenedor Docker (created, running, paused, exited), las transiciones entre
ellos, el rol de PID 1, los exit codes, y la persistencia de la capa de
escritura.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada: `docker pull debian:bookworm`
- Estar en el directorio `labs/` de este topico

## Archivos del laboratorio

| Archivo | Proposito |
|---|---|
| `README.md` | Este documento: guia paso a paso |

Este laboratorio no requiere archivos de soporte. Todos los ejercicios usan
la imagen `debian:bookworm` directamente.

---

## Parte 1 — Estados y transiciones

### Objetivo

Recorrer todos los estados de un contenedor (created, running, paused, exited)
observando cada transicion con `docker inspect`.

### Paso 1.1: Crear un contenedor sin iniciarlo

```bash
docker create --name lifecycle debian:bookworm sleep 300
```

Antes de inspeccionar, predice:

- ¿En que estado esta el contenedor?
- ¿Hay algun proceso ejecutandose dentro?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.2: Verificar el estado `created`

```bash
docker inspect lifecycle --format '{{.State.Status}}'
```

Salida esperada:

```
created
```

El contenedor existe pero nunca se ha iniciado. Docker reservo la capa de
escritura y la configuracion de red, pero no hay ningun proceso corriendo.

```bash
# Confirmar que no aparece en docker ps (sin -a)
docker ps --filter name=lifecycle
```

Salida esperada:

```
CONTAINER ID   IMAGE   COMMAND   CREATED   STATUS   PORTS   NAMES
```

Solo `docker ps -a` muestra contenedores que no estan en estado `running`.

```bash
docker ps -a --filter name=lifecycle --format 'table {{.Names}}\t{{.Status}}'
```

Salida esperada:

```
NAMES       STATUS
lifecycle   Created
```

### Paso 1.3: Transicion created -> running

```bash
docker start lifecycle
docker inspect lifecycle --format '{{.State.Status}}'
```

Salida esperada:

```
running
```

```bash
# Ahora SI aparece en docker ps
docker ps --filter name=lifecycle --format 'table {{.Names}}\t{{.Status}}'
```

Salida esperada:

```
NAMES       STATUS
lifecycle   Up X seconds
```

### Paso 1.4: Transicion running -> paused

```bash
docker pause lifecycle
docker inspect lifecycle --format '{{.State.Status}}'
```

Salida esperada:

```
paused
```

```bash
docker ps --filter name=lifecycle --format 'table {{.Names}}\t{{.Status}}'
```

Salida esperada:

```
NAMES       STATUS
lifecycle   Up X seconds (Paused)
```

El proceso esta congelado: no ejecuta instrucciones ni consume CPU, pero
mantiene toda la memoria asignada. Docker usa el cgroups freezer para esto.

### Paso 1.5: Transicion paused -> running

```bash
docker unpause lifecycle
docker inspect lifecycle --format '{{.State.Status}}'
```

Salida esperada:

```
running
```

El proceso se reanuda exactamente donde estaba.

### Paso 1.6: Transicion running -> exited

```bash
docker stop lifecycle
docker inspect lifecycle --format '{{.State.Status}}'
```

Salida esperada:

```
exited
```

```bash
# Ya no aparece en docker ps (sin -a)
docker ps --filter name=lifecycle
# (vacio)

# Pero SI aparece en docker ps -a
docker ps -a --filter name=lifecycle --format 'table {{.Names}}\t{{.Status}}'
```

Salida esperada:

```
NAMES       STATUS
lifecycle   Exited (137) X seconds ago
```

El contenedor sigue existiendo. Su capa de escritura, logs y configuracion
se conservan. Solo se detuvo el proceso.

### Paso 1.7: Transicion exited -> running (reiniciar)

```bash
docker start lifecycle
docker inspect lifecycle --format '{{.State.Status}}'
```

Salida esperada:

```
running
```

`docker start` reinicia el proceso principal. La capa de escritura se preserva
entre stop y start.

### Paso 1.8: Resumen visual de transiciones

Ejecuta todo el recorrido observando el campo `Status`:

```bash
docker stop lifecycle
echo "=== Estado actual ==="
docker inspect lifecycle --format '{{.State.Status}}'

echo ""
echo "=== Recorrido completo ==="
echo "created -> $(docker inspect lifecycle --format '{{.State.Status}}')"

docker start lifecycle
echo "running -> $(docker inspect lifecycle --format '{{.State.Status}}')"

docker pause lifecycle
echo "paused  -> $(docker inspect lifecycle --format '{{.State.Status}}')"

docker unpause lifecycle
echo "running -> $(docker inspect lifecycle --format '{{.State.Status}}')"

docker stop lifecycle
echo "exited  -> $(docker inspect lifecycle --format '{{.State.Status}}')"
```

### Paso 1.9: Limpiar

```bash
docker rm lifecycle
```

---

## Parte 2 — PID 1 y exit codes

### Objetivo

Demostrar que el ciclo de vida del contenedor esta vinculado a PID 1: cuando
PID 1 termina, el contenedor pasa a `exited`, y el exit code del contenedor
es el exit code de PID 1.

### Paso 2.1: Terminacion exitosa (exit code 0)

```bash
docker run --name exit-ok debian:bookworm true
```

Antes de inspeccionar, predice:

- ¿Cual sera el exit code del contenedor?
- ¿En que estado estara?

Intenta predecir antes de continuar al siguiente paso.

```bash
docker inspect exit-ok --format 'Status: {{.State.Status}} | ExitCode: {{.State.ExitCode}}'
```

Salida esperada:

```
Status: exited | ExitCode: 0
```

El comando `true` retorna 0 (exito). PID 1 termino con 0, asi que el
contenedor refleja ese exit code.

### Paso 2.2: Terminacion con error (exit code 1)

```bash
docker run --name exit-err debian:bookworm bash -c "exit 1"
docker inspect exit-err --format 'Status: {{.State.Status}} | ExitCode: {{.State.ExitCode}}'
```

Salida esperada:

```
Status: exited | ExitCode: 1
```

### Paso 2.3: Terminacion por SIGKILL (exit code 137)

```bash
docker run -d --name exit-kill debian:bookworm sleep 300
docker kill exit-kill
docker inspect exit-kill --format 'Status: {{.State.Status}} | ExitCode: {{.State.ExitCode}}'
```

Salida esperada:

```
Status: exited | ExitCode: 137
```

137 = 128 + 9 (SIGKILL). La formula para senales es: exit code = 128 + numero
de senal.

### Paso 2.4: Terminacion por SIGTERM con --init (exit code 143)

```bash
docker run -d --init --name exit-term debian:bookworm sleep 300
docker stop exit-term
docker inspect exit-term --format 'Status: {{.State.Status}} | ExitCode: {{.State.ExitCode}}'
```

Salida esperada:

```
Status: exited | ExitCode: 143
```

143 = 128 + 15 (SIGTERM). Con `--init`, tini propaga SIGTERM a PID 2 (sleep),
que termina limpiamente con exit code 143.

### Paso 2.5: Comparar todos los exit codes

```bash
docker ps -a --filter "name=exit-" --format 'table {{.Names}}\t{{.Status}}'
```

Salida esperada:

```
NAMES       STATUS
exit-term   Exited (143) X seconds ago
exit-kill   Exited (137) X seconds ago
exit-err    Exited (1) X seconds ago
exit-ok     Exited (0) X seconds ago
```

Observa como cada exit code refleja exactamente como termino PID 1.

### Paso 2.6: Comando no encontrado (exit code 127)

```bash
docker run --rm debian:bookworm comando_inexistente
echo "Exit code: $?"
```

Salida esperada:

```
docker: Error response from daemon: ... executable file not found ...
Exit code: 127
```

### Paso 2.7: Limpiar

```bash
docker rm exit-ok exit-err exit-kill exit-term
```

---

## Parte 3 — Filtrado por estado

### Objetivo

Usar `docker ps` con filtros de estado para localizar contenedores en
diferentes estados de su ciclo de vida.

### Paso 3.1: Preparar contenedores en distintos estados

```bash
# Contenedor en estado running
docker run -d --name filter-run debian:bookworm sleep 300

# Contenedor en estado exited
docker run --name filter-stop debian:bookworm echo "done"

# Contenedor en estado paused
docker run -d --name filter-pause debian:bookworm sleep 300
docker pause filter-pause
```

### Paso 3.2: Ver todos los contenedores

```bash
docker ps -a --filter "name=filter-" --format 'table {{.Names}}\t{{.Status}}'
```

Salida esperada:

```
NAMES          STATUS
filter-pause   Up X seconds (Paused)
filter-stop    Exited (0) X seconds ago
filter-run     Up X seconds
```

### Paso 3.3: Filtrar por estado running

Antes de ejecutar, predice: ¿cuantos contenedores mostrara este filtro?

Intenta predecir antes de continuar al siguiente paso.

```bash
docker ps --filter status=running --filter "name=filter-" --format 'table {{.Names}}\t{{.Status}}'
```

Salida esperada:

```
NAMES        STATUS
filter-run   Up X seconds
```

Solo uno. El contenedor pausado **no** tiene status `running` — tiene status
`paused`.

### Paso 3.4: Filtrar por estado paused

```bash
docker ps --filter status=paused --filter "name=filter-" --format 'table {{.Names}}\t{{.Status}}'
```

Salida esperada:

```
NAMES          STATUS
filter-pause   Up X seconds (Paused)
```

### Paso 3.5: Filtrar por estado exited

```bash
docker ps -a --filter status=exited --filter "name=filter-" --format 'table {{.Names}}\t{{.Status}}'
```

Salida esperada:

```
NAMES         STATUS
filter-stop   Exited (0) X seconds ago
```

Nota que se necesita `-a` para ver contenedores en estado `exited`. Sin `-a`,
`docker ps` solo muestra `running` y `paused`.

### Paso 3.6: Filtrar por exit code

```bash
# Todos los que terminaron con exit code 0
docker ps -a --filter exited=0 --filter "name=filter-" --format 'table {{.Names}}\t{{.Status}}'
```

### Paso 3.7: Usar `-q` para scripting

```bash
# Obtener solo los IDs de contenedores running
docker ps -q --filter status=running --filter "name=filter-"

# Contar contenedores en cada estado
echo "Running: $(docker ps -q --filter status=running --filter 'name=filter-' | wc -l)"
echo "Paused:  $(docker ps -q --filter status=paused --filter 'name=filter-' | wc -l)"
echo "Exited:  $(docker ps -aq --filter status=exited --filter 'name=filter-' | wc -l)"
```

Salida esperada:

```
Running: 1
Paused:  1
Exited:  1
```

### Paso 3.8: Limpiar

```bash
docker unpause filter-pause
docker rm -f filter-run filter-stop filter-pause
```

---

## Parte 4 — Persistencia de la capa de escritura

### Objetivo

Demostrar que un contenedor detenido conserva su capa de escritura intacta,
y que `docker rm` la destruye permanentemente.

### Paso 4.1: Crear datos en un contenedor

```bash
docker run -d --name persist-test debian:bookworm sleep 300
docker exec persist-test bash -c "echo 'datos importantes' > /tmp/evidence.txt"
docker exec persist-test cat /tmp/evidence.txt
```

Salida esperada:

```
datos importantes
```

### Paso 4.2: Stop preserva la capa de escritura

Antes de ejecutar, predice: despues de `docker stop` y `docker start`,
¿seguira existiendo `/tmp/evidence.txt`?

Intenta predecir antes de continuar al siguiente paso.

```bash
docker stop persist-test
docker start persist-test
docker exec persist-test cat /tmp/evidence.txt
```

Salida esperada:

```
datos importantes
```

El archivo sobrevive al ciclo stop/start. La capa de escritura se preserva.

### Paso 4.3: Verificar que el contenedor detenido sigue existiendo

```bash
docker stop persist-test
docker ps -a --filter name=persist-test --format 'table {{.Names}}\t{{.Status}}'
```

Salida esperada:

```
NAMES          STATUS
persist-test   Exited (137) X seconds ago
```

Un contenedor en estado `exited` ocupa espacio en disco (su capa de escritura),
pero no consume CPU ni memoria.

### Paso 4.4: `docker rm` destruye la capa de escritura

```bash
docker rm persist-test
```

Despues de `rm`, la capa de escritura se elimino permanentemente. El archivo
`/tmp/evidence.txt` ya no existe en ninguna parte. No hay forma de recuperarlo.

```bash
# Confirmar que el contenedor ya no existe
docker ps -a --filter name=persist-test
```

Salida esperada:

```
CONTAINER ID   IMAGE   COMMAND   CREATED   STATUS   PORTS   NAMES
```

### Paso 4.5: Contraste con --rm

```bash
# Con --rm, el contenedor se elimina automaticamente al terminar
docker run --rm --name auto-clean debian:bookworm echo "efimero"
docker ps -a --filter name=auto-clean
```

Salida esperada:

```
CONTAINER ID   IMAGE   COMMAND   CREATED   STATUS   PORTS   NAMES
```

El contenedor nunca llego a existir en estado `exited` — se elimino
automaticamente.

---

## Limpieza final

```bash
# Eliminar contenedores huerfanos (si quedo alguno por un paso saltado)
docker rm -f lifecycle exit-ok exit-err exit-kill exit-term \
    filter-run filter-stop filter-pause persist-test 2>/dev/null
```

---

## Conceptos reforzados

1. Un contenedor tiene estados bien definidos: `created`, `running`, `paused`,
   `exited`. Cada transicion se realiza con un comando especifico (`create`,
   `start`, `pause`, `unpause`, `stop`).
2. El ciclo de vida del contenedor esta atado a **PID 1**: cuando PID 1
   termina, el contenedor pasa a `exited`. El exit code del contenedor es el
   exit code de PID 1.
3. La formula para exit codes por senal es **128 + numero de senal**: SIGKILL
   (9) produce 137, SIGTERM (15) produce 143.
4. `docker ps` sin `-a` solo muestra contenedores `running` (y `paused`).
   Para ver contenedores `exited` o `created` se necesita `-a`.
5. Un contenedor detenido (`exited`) **conserva** su capa de escritura.
   `docker rm` la **destruye** permanentemente. Para datos persistentes se
   necesitan volumenes.
6. `--rm` elimina el contenedor automaticamente cuando PID 1 termina, evitando
   la acumulacion de contenedores en estado `exited`.
