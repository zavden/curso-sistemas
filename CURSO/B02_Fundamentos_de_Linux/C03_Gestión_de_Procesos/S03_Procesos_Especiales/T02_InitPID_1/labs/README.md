# Lab — Init / PID 1

## Objetivo

Entender PID 1: inspeccionar el primer proceso del sistema,
observar como adopta huerfanos (reaping), la proteccion especial
de senales del kernel, y por que los contenedores necesitan
tini o --init.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Inspeccionar PID 1

### Objetivo

Examinar que proceso es PID 1 dentro del contenedor y comparar
con el host.

### Paso 1.1: Que es PID 1

```bash
docker compose exec debian-dev bash -c '
echo "=== PID 1 en este contenedor ==="
ps -p 1 -o pid,ppid,user,stat,cmd
echo ""
echo "=== Informacion detallada ==="
cat /proc/1/status | grep -E "^(Name|Pid|PPid|Uid|Gid|Threads):"
echo ""
echo "=== Ejecutable ==="
readlink /proc/1/exe 2>/dev/null || echo "(no disponible)"
echo ""
echo "PID 1 es el primer proceso del espacio de usuario"
echo "Todos los demas procesos descienden de el"
'
```

### Paso 1.2: PID 1 y PID 2

```bash
docker compose exec debian-dev bash -c '
echo "=== PID 1 vs PID 2 ==="
ps -eo pid,ppid,cmd | head -5
echo ""
echo "PID 1 = primer proceso de USUARIO (init/systemd/entrypoint)"
echo "PID 2 = kthreadd (padre de kernel threads)"
echo "        Solo en host, no en contenedores"
echo ""
echo "=== En el contenedor ==="
echo "PID 1 es el entrypoint del contenedor"
echo "No es systemd (los contenedores no arrancan un init system)"
'
```

### Paso 1.3: Arbol de procesos desde PID 1

```bash
docker compose exec debian-dev bash -c '
echo "=== Arbol desde PID 1 ==="
pstree -p 1
echo ""
echo "Todos los procesos del contenedor descienden de PID 1"
echo "Si PID 1 muere, el contenedor se detiene"
'
```

### Paso 1.4: PPID de PID 1

```bash
docker compose exec debian-dev bash -c '
echo "=== PPID de PID 1 ==="
cat /proc/1/status | grep "^PPid:"
echo ""
echo "PPID de PID 1 es 0 (el kernel)"
echo "Ningun otro proceso tiene PPID 0"
echo ""
echo "=== Verificar ==="
ps -eo ppid --no-headers | sort | uniq -c | sort -rn | head -5
echo "(cuantos procesos tienen cada PPID)"
'
```

---

## Parte 2 — Reaping de huerfanos

### Objetivo

Observar como PID 1 adopta procesos huerfanos y los limpia
cuando terminan.

### Paso 2.1: PID 1 adopta huerfanos

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear un huerfano ==="
bash -c '\''sleep 120 & echo "hijo=$! padre=$$"; exit'\'' &
wait $!
sleep 1

CHILD=$(pgrep -nf "sleep 120")
if [ -n "$CHILD" ]; then
    echo ""
    echo "=== Hijo adoptado por PID 1 ==="
    ps -o pid,ppid,stat,cmd -p $CHILD
    echo ""
    echo "PPID = 1 confirma que PID 1 lo adopto"
fi

pkill -f "sleep 120" 2>/dev/null
'
```

### Paso 2.2: PID 1 limpia automaticamente

```bash
docker compose exec debian-dev bash -c '
echo "=== PID 1 hace wait() por sus hijos adoptados ==="
echo "Zombies antes: $(ps -eo stat --no-headers | grep -c "^Z")"
echo ""

echo "Crear huerfano que termina inmediatamente:"
bash -c '\''bash -c "exit 42" & exit'\'' &
wait $!
sleep 1

echo "Zombies despues: $(ps -eo stat --no-headers | grep -c "^Z")"
echo ""
echo "No hay zombie porque PID 1 hizo wait() automaticamente"
echo "Este es el rol clave de PID 1: recoger huerfanos muertos"
'
```

### Paso 2.3: Responsabilidades de PID 1

```bash
docker compose exec debian-dev bash -c '
echo "=== 4 responsabilidades de PID 1 ==="
echo ""
echo "1. INICIALIZACION"
echo "   Arrancar todos los servicios del sistema"
echo "   (en host: systemd levanta sshd, cron, etc.)"
echo "   (en contenedor: ejecutar el entrypoint)"
echo ""
echo "2. REAPING"
echo "   Hacer wait() por huerfanos adoptados"
echo "   Evitar que se acumulen zombies"
echo ""
echo "3. SENALES"
echo "   Reenviar senales a los procesos hijos"
echo "   (SIGTERM de docker stop debe llegar a la app)"
echo ""
echo "4. SHUTDOWN"
echo "   Coordinar el apagado ordenado del sistema"
echo "   (enviar SIGTERM, esperar, SIGKILL si necesario)"
'
```

---

## Parte 3 — PID 1 en contenedores

### Objetivo

Entender los problemas de PID 1 en contenedores y las soluciones.

### Paso 3.1: El problema — proteccion de senales

```bash
docker compose exec debian-dev bash -c '
echo "=== Proteccion especial del kernel para PID 1 ==="
echo ""
echo "El kernel NO entrega senales a PID 1 si PID 1 no tiene"
echo "un handler registrado para esa senal."
echo ""
echo "Esto significa:"
echo "  - SIGTERM a PID 1 puede ser ignorada"
echo "  - SIGINT a PID 1 puede ser ignorada"
echo "  - Solo SIGKILL funciona siempre contra PID 1"
echo ""
echo "=== Consecuencia para docker stop ==="
echo "1. docker stop envia SIGTERM a PID 1 del contenedor"
echo "2. Si PID 1 no tiene handler → SIGTERM ignorada"
echo "3. Docker espera 10s (StopTimeout)"
echo "4. Docker envia SIGKILL → contenedor muere abruptamente"
echo ""
echo "Resultado: docker stop tarda 10s y no hay shutdown limpio"
'
```

### Paso 3.2: Demostrar proteccion de PID 1

```bash
docker compose exec debian-dev bash -c '
echo "=== PID 1 vs proceso normal ==="
echo ""
echo "--- Proceso normal: SIGTERM lo mata ---"
sleep 300 &
PID=$!
kill -TERM $PID
sleep 0.5
ps -o pid,stat -p $PID 2>/dev/null || echo "PID $PID: muerto (esperado)"
wait $PID 2>/dev/null

echo ""
echo "--- PID 1: SIGTERM no lo mata ---"
kill -TERM 1 2>&1
ps -o pid,stat -p 1
echo "PID 1 sigue vivo (protegido por el kernel)"
'
```

### Paso 3.3: La solucion — tini / --init

```bash
docker compose exec debian-dev bash -c '
echo "=== tini: PID 1 minimo para contenedores ==="
echo ""
echo "tini hace exactamente dos cosas:"
echo "  1. Reenviar senales al proceso hijo"
echo "  2. Hacer wait() para recoger zombies"
echo ""
echo "=== Sin tini (problematico) ==="
echo "  CMD [\"python3\", \"app.py\"]"
echo "  python3 es PID 1"
echo "  → No reenvio de senales"
echo "  → No reaping de zombies"
echo ""
echo "=== Con tini (correcto) ==="
echo "  ENTRYPOINT [\"/usr/bin/tini\", \"--\"]"
echo "  CMD [\"python3\", \"app.py\"]"
echo "  tini es PID 1, python3 es PID 2"
echo "  → tini reenvio SIGTERM a python3"
echo "  → tini recoge zombies"
echo ""
echo "=== docker run --init ==="
echo "  Equivalente a usar tini sin modificar el Dockerfile"
echo "  docker run --init myimage"
'
```

### Paso 3.4: Shell form vs exec form

```bash
docker compose exec debian-dev bash -c '
echo "=== Shell form vs exec form en Dockerfile ==="
echo ""
echo "--- Shell form (MAL) ---"
echo "  CMD python3 app.py"
echo "  Docker lo ejecuta como: /bin/sh -c \"python3 app.py\""
echo "  PID 1 = /bin/sh"
echo "  PID 2 = python3"
echo "  sh no reenvio senales → python3 nunca recibe SIGTERM"
echo ""
echo "--- Exec form (CORRECTO) ---"
echo "  CMD [\"python3\", \"app.py\"]"
echo "  python3 es PID 1 directamente"
echo "  Recibe SIGTERM si tiene handler"
echo ""
echo "--- Exec form + tini (MEJOR) ---"
echo "  ENTRYPOINT [\"/usr/bin/tini\", \"--\"]"
echo "  CMD [\"python3\", \"app.py\"]"
echo "  tini es PID 1 → reenvio senales + reaping"
'
```

### Paso 3.5: Comparar con el host

```bash
docker compose exec debian-dev bash -c '
echo "=== En el host vs en un contenedor ==="
printf "%-20s %-25s %-25s\n" "Aspecto" "Host" "Contenedor"
printf "%-20s %-25s %-25s\n" "----------" "---------------------" "---------------------"
printf "%-20s %-25s %-25s\n" "PID 1" "systemd" "entrypoint de la app"
printf "%-20s %-25s %-25s\n" "Reaping" "Si (systemd lo hace)" "Depende de la app"
printf "%-20s %-25s %-25s\n" "Senales" "Si (handlers en systemd)" "Depende de la app"
printf "%-20s %-25s %-25s\n" "Shutdown" "systemctl stop (limpio)" "docker stop (10s+kill)"
printf "%-20s %-25s %-25s\n" "Solucion" "Innecesaria" "tini / --init"
'
```

---

## Limpieza final

```bash
docker compose exec debian-dev bash -c '
pkill -f "sleep 120" 2>/dev/null
echo "Limpieza completada"
'
```

---

## Conceptos reforzados

1. PID 1 es el primer proceso de usuario. Su PPID es 0 (kernel).
   Si PID 1 muere, el sistema (o contenedor) se detiene.

2. La responsabilidad clave de PID 1 es **reaping**: hacer wait()
   por huerfanos adoptados para evitar zombies acumulados.

3. El kernel **protege PID 1** de senales: no entrega senales
   para las que PID 1 no tiene handler. Solo SIGKILL funciona.

4. En contenedores, el entrypoint es PID 1 pero normalmente no
   esta preparado para ese rol: no reenvia senales ni recoge
   zombies.

5. **tini** (o `docker run --init`) resuelve ambos problemas:
   reenvia senales y hace wait(). Es el PID 1 minimo para
   contenedores.

6. Usar **exec form** (`CMD ["app"]`) en Dockerfiles, no shell
   form (`CMD app`), para que la app reciba senales directamente.
