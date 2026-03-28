# Lab — cgroups v2

## Objetivo

Explorar cgroups v2: verificar la version activa, navegar la
jerarquia en /sys/fs/cgroup, leer datos de los controladores
(memory, cpu, pids), y entender como Docker usa cgroups para
limitar recursos de contenedores.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

Nota: dentro de un contenedor, el acceso a /sys/fs/cgroup puede
estar limitado por el cgroup namespace. Los datos visibles
corresponden al cgroup del contenedor.

---

## Parte 1 — Jerarquia de cgroups

### Objetivo

Verificar la version de cgroups y explorar la estructura.

### Paso 1.1: Version de cgroups

```bash
docker compose exec debian-dev bash -c '
echo "=== Verificar version de cgroups ==="
mount | grep cgroup
echo ""
echo "cgroup2 = v2 (jerarquia unificada)"
echo "cgroup (sin 2) con multiples mounts = v1"
echo ""
echo "=== stat del filesystem ==="
stat -fc %T /sys/fs/cgroup/ 2>/dev/null || echo "(no disponible)"
echo "(cgroup2fs = v2, tmpfs = v1)"
'
```

### Paso 1.2: Que hay en /sys/fs/cgroup

```bash
docker compose exec debian-dev bash -c '
echo "=== Contenido de /sys/fs/cgroup/ ==="
ls /sys/fs/cgroup/ 2>/dev/null || echo "(no disponible)"
echo ""
echo "Archivos clave:"
echo "  cgroup.controllers     — controladores disponibles"
echo "  cgroup.procs           — PIDs en este cgroup"
echo "  cgroup.subtree_control — controladores habilitados para hijos"
echo "  memory.*               — datos del controlador de memoria"
echo "  cpu.*                  — datos del controlador de CPU"
echo "  pids.*                 — datos del controlador de PIDs"
'
```

### Paso 1.3: Controladores disponibles

```bash
docker compose exec debian-dev bash -c '
echo "=== Controladores disponibles ==="
cat /sys/fs/cgroup/cgroup.controllers 2>/dev/null \
    || echo "(no disponible en este contenedor)"

echo ""
echo "=== Controladores habilitados para sub-cgroups ==="
cat /sys/fs/cgroup/cgroup.subtree_control 2>/dev/null \
    || echo "(no disponible)"

echo ""
echo "Controladores comunes:"
echo "  cpu     — limitar tiempo de CPU"
echo "  memory  — limitar uso de memoria"
echo "  io      — limitar I/O de disco"
echo "  pids    — limitar numero de procesos"
echo "  cpuset  — asignar CPUs especificas"
'
```

### Paso 1.4: Cgroup del proceso actual

```bash
docker compose exec debian-dev bash -c '
echo "=== En que cgroup estoy? ==="
cat /proc/self/cgroup
echo ""
echo "Formato: jerarquia:controlador:ruta"
echo "En cgroups v2: 0::ruta"
echo ""
echo "Dentro del contenedor se ve como 0::/"
echo "(el cgroup namespace oculta la ruta real del host)"
echo ""
echo "=== En el host seria algo como ==="
echo "0::/system.slice/docker-<container-hash>.scope"
'
```

---

## Parte 2 — Controladores

### Objetivo

Leer datos de los controladores de memoria, CPU, y PIDs.

### Paso 2.1: Controlador memory

```bash
docker compose exec debian-dev bash -c '
echo "=== Uso de memoria del cgroup ==="
echo "--- memory.current (uso actual) ---"
cat /sys/fs/cgroup/memory.current 2>/dev/null \
    && echo "bytes" || echo "(no disponible)"

echo ""
echo "--- memory.max (limite) ---"
cat /sys/fs/cgroup/memory.max 2>/dev/null || echo "(no disponible)"
echo "(max = sin limite, un numero = limite en bytes)"

echo ""
echo "--- memory.stat (detalle) ---"
cat /sys/fs/cgroup/memory.stat 2>/dev/null | head -10 || echo "(no disponible)"
echo "..."
echo ""
echo "Campos clave de memory.stat:"
echo "  anon    — memoria anonima (heap, stack)"
echo "  file    — page cache (archivos mapeados)"
echo "  shmem   — shared memory"
'
```

### Paso 2.2: Controlador cpu

```bash
docker compose exec debian-dev bash -c '
echo "=== CPU del cgroup ==="
echo "--- cpu.max (limite) ---"
cat /sys/fs/cgroup/cpu.max 2>/dev/null || echo "(no disponible)"
echo ""
echo "Formato: cuota periodo"
echo "  max 100000  = sin limite de CPU"
echo "  50000 100000 = 50ms de cada 100ms = 50% de 1 core"
echo "  200000 100000 = 200ms de cada 100ms = 2 cores max"

echo ""
echo "--- cpu.weight (peso relativo) ---"
cat /sys/fs/cgroup/cpu.weight 2>/dev/null || echo "(no disponible)"
echo "(100 = default, rango 1-10000)"

echo ""
echo "--- cpu.stat (estadisticas) ---"
cat /sys/fs/cgroup/cpu.stat 2>/dev/null || echo "(no disponible)"
echo ""
echo "  usage_usec   — tiempo total de CPU usado"
echo "  user_usec    — tiempo en user space"
echo "  system_usec  — tiempo en kernel"
echo "  nr_throttled — veces que fue throttled por cpu.max"
'
```

### Paso 2.3: Controlador pids

```bash
docker compose exec debian-dev bash -c '
echo "=== PIDs del cgroup ==="
echo "--- pids.current (procesos actuales) ---"
cat /sys/fs/cgroup/pids.current 2>/dev/null || echo "(no disponible)"

echo ""
echo "--- pids.max (limite) ---"
cat /sys/fs/cgroup/pids.max 2>/dev/null || echo "(no disponible)"
echo "(max = sin limite)"

echo ""
echo "pids.max previene fork bombs dentro del contenedor"
echo "docker run --pids-limit 100 establece pids.max = 100"
echo ""
echo "Si se alcanza pids.max:"
echo "  fork() retorna EAGAIN (error)"
echo "  No se puede crear ningun proceso nuevo"
'
```

### Paso 2.4: Eventos de memoria

```bash
docker compose exec debian-dev bash -c '
echo "=== memory.events (contadores de eventos) ==="
cat /sys/fs/cgroup/memory.events 2>/dev/null || echo "(no disponible)"
echo ""
echo "Campos:"
echo "  low       — memoria por debajo de memory.low"
echo "  high      — memoria por encima de memory.high"
echo "  max       — intento de exceder memory.max"
echo "  oom       — OOM killer invocado"
echo "  oom_kill  — procesos matados por OOM"
echo ""
echo "oom_kill > 0 indica que el contenedor"
echo "ha sido matado por falta de memoria"
'
```

---

## Parte 3 — cgroups y Docker

### Objetivo

Conectar los cgroups con los flags de docker run.

### Paso 3.1: Mapeo de flags Docker a cgroups

```bash
docker compose exec debian-dev bash -c '
echo "=== Docker flags → archivos de cgroup ==="
printf "%-25s %-30s\n" "docker run flag" "Archivo cgroup"
printf "%-25s %-30s\n" "-----------------------" "----------------------------"
printf "%-25s %-30s\n" "--memory 256m" "memory.max = 268435456"
printf "%-25s %-30s\n" "--memory-reservation 128m" "memory.low = 134217728"
printf "%-25s %-30s\n" "--cpus 0.5" "cpu.max = 50000 100000"
printf "%-25s %-30s\n" "--cpu-shares 512" "cpu.weight = ~50"
printf "%-25s %-30s\n" "--pids-limit 100" "pids.max = 100"
printf "%-25s %-30s\n" "--blkio-weight 300" "io.weight = 300"
echo ""
echo "Cada flag de docker run se traduce a escribir"
echo "un valor en el archivo correspondiente del cgroup"
'
```

### Paso 3.2: Ver limites de este contenedor

```bash
docker compose exec debian-dev bash -c '
echo "=== Limites de este contenedor ==="
echo ""
echo "Memoria:"
MEM_MAX=$(cat /sys/fs/cgroup/memory.max 2>/dev/null)
MEM_CUR=$(cat /sys/fs/cgroup/memory.current 2>/dev/null)
if [ -n "$MEM_MAX" ] && [ "$MEM_MAX" != "max" ]; then
    echo "  Limite: $((MEM_MAX / 1024 / 1024)) MB"
else
    echo "  Limite: sin limite"
fi
if [ -n "$MEM_CUR" ]; then
    echo "  Uso actual: $((MEM_CUR / 1024 / 1024)) MB"
fi

echo ""
echo "CPU:"
CPU_MAX=$(cat /sys/fs/cgroup/cpu.max 2>/dev/null)
echo "  cpu.max: ${CPU_MAX:-no disponible}"

echo ""
echo "PIDs:"
PIDS_MAX=$(cat /sys/fs/cgroup/pids.max 2>/dev/null)
PIDS_CUR=$(cat /sys/fs/cgroup/pids.current 2>/dev/null)
echo "  Limite: ${PIDS_MAX:-no disponible}"
echo "  Actual: ${PIDS_CUR:-no disponible}"
'
```

### Paso 3.3: Que pasa cuando se excede un limite

```bash
docker compose exec debian-dev bash -c '
echo "=== Comportamiento al exceder limites ==="
echo ""
echo "memory.max excedido:"
echo "  1. El kernel intenta reclamar memoria del cgroup"
echo "  2. Si no puede → OOM killer mata un proceso del cgroup"
echo "  3. dmesg muestra: Memory cgroup out of memory"
echo "  4. docker events muestra: container oom"
echo ""
echo "cpu.max excedido:"
echo "  No mata — throttle (pausa el proceso hasta el siguiente periodo)"
echo "  cpu.stat nr_throttled cuenta las veces"
echo ""
echo "pids.max excedido:"
echo "  fork() retorna error EAGAIN"
echo "  No se pueden crear procesos nuevos"
echo "  Previene fork bombs"
'
```

### Paso 3.4: Namespaces vs cgroups

```bash
docker compose exec debian-dev bash -c '
echo "=== Resumen: namespaces vs cgroups ==="
printf "%-15s %-30s %-30s\n" "Aspecto" "Namespaces" "Cgroups"
printf "%-15s %-30s %-30s\n" "-------------" "----------------------------" "----------------------------"
printf "%-15s %-30s %-30s\n" "Proposito" "Que VE el proceso" "Cuanto puede USAR"
printf "%-15s %-30s %-30s\n" "Mecanismo" "Aislar visibilidad" "Limitar recursos"
printf "%-15s %-30s %-30s\n" "Ejemplo" "Solo ve sus PIDs" "Max 256MB de RAM"
printf "%-15s %-30s %-30s\n" "API" "/proc/[pid]/ns/" "/sys/fs/cgroup/"
printf "%-15s %-30s %-30s\n" "Sin ellos" "Ve todo el sistema" "Puede consumir todo"
echo ""
echo "Un contenedor necesita AMBOS:"
echo "  Namespaces para aislamiento"
echo "  Cgroups para limites de recursos"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. **cgroups v2** usa una jerarquia unificada en `/sys/fs/cgroup/`.
   Verificar version con `mount | grep cgroup` o `stat -fc %T`.

2. Los controladores principales son **memory** (memory.max,
   memory.current), **cpu** (cpu.max, cpu.weight), **pids**
   (pids.max), e **io** (io.max).

3. `memory.max` es un limite estricto: excederlo invoca el **OOM
   killer**. `cpu.max` hace throttle (pausa). `pids.max` bloquea
   fork().

4. Cada flag de `docker run` (`--memory`, `--cpus`, `--pids-limit`)
   se traduce a escribir un valor en el archivo cgroup
   correspondiente.

5. El **cgroup namespace** hace que el contenedor vea su cgroup
   como `/`, ocultando la ruta real del host.

6. **Namespaces** controlan que ve el proceso (aislamiento).
   **Cgroups** controlan cuanto puede usar (limites). Los
   contenedores necesitan ambos.
