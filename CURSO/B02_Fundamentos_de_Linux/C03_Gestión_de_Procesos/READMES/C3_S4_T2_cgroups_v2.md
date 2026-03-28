# T02 — cgroups v2

## Errata y notas sobre el material base

1. **labs: formato `/proc/PID/cgroup` mal etiquetado** — El lab dice
   "Formato: jerarquía:controlador:ruta". En cgroups v2 el campo del
   medio siempre está vacío (`0::/ruta`); llamarlo "controlador" es
   terminología de v1. El formato correcto en v2 es
   `hierarchy-ID::cgroup-path`.

2. **README ejercicio 3: volver al cgroup original puede fallar** —
   El ejercicio mueve `$$` a un cgroup creado manualmente y luego intenta
   volver con `echo $$ | sudo tee /sys/fs/cgroup/user.slice/cgroup.procs`.
   Esto falla si `user.slice` tiene `subtree_control` activo (regla "no
   internal processes": un cgroup con hijos controlados no puede tener
   procesos directos). Solución: guardar el cgroup original antes de mover.

3. **labs: `--cpu-shares 512` → `cpu.weight = ~50`** — La conversión real
   que hace Docker/systemd usa la fórmula
   `weight = 1 + ((shares - 2) * 9999) / 262142`. Para shares=512 el
   resultado es ~20, no ~50. El mapeo no es directo porque v1 usa rango
   2-262144 y v2 usa 1-10000.

4. **labs: `--blkio-weight` → `io.weight`** — En v2 el I/O scheduler BFQ
   usa `io.bfq.weight` (rango 1-1000), no `io.weight` directamente. El
   mapeo depende del scheduler activo en el kernel.

5. **README: falta `memory.high`** — Solo se explica `memory.max` (hard
   limit → OOM) pero no `memory.high` (soft limit → throttle). En la
   práctica, Docker usa `memory.high` junto con `memory.max` para un
   control más granular.

6. **README: falta la regla "no internal processes"** — Concepto
   fundamental de cgroups v2: un cgroup que tiene `subtree_control` activo
   no puede tener procesos propios (solo en hojas). No se menciona en
   ningún sitio.

---

## Qué son los cgroups

**cgroups** (control groups) son una funcionalidad del kernel que permite
**limitar, contabilizar y aislar** el uso de recursos (CPU, memoria, I/O,
número de procesos) de un grupo de procesos:

```
Namespaces:  "qué puede VER un proceso"    (aislamiento de visibilidad)
cgroups:     "cuánto puede USAR un proceso" (limitación de recursos)

Un contenedor usa ambos:
  - Namespaces → el proceso cree que tiene su propio sistema
  - cgroups    → no puede consumir todos los recursos del host
```

Sin cgroups, un contenedor podría consumir toda la CPU, toda la memoria y
todo el I/O del host, afectando a otros contenedores y al propio sistema.

---

## cgroups v1 vs v2

| Aspecto | v1 (2008) | v2 (kernel 4.5, 2016) |
|---|---|---|
| Jerarquía | Múltiples (una por controlador) | Una sola unificada |
| Mount point | `/sys/fs/cgroup/cpu/`, `/sys/fs/cgroup/memory/`, ... | `/sys/fs/cgroup/` (todo junto) |
| Pertenencia | Un proceso puede estar en cgroups distintos por controlador | Un proceso en UN solo cgroup |
| Internal processes | Permitidos | Prohibidos (ver más abajo) |
| Distribuciones | RHEL 7-8, Debian ≤10 | RHEL 9+, Debian 11+, Ubuntu 21.10+, Fedora 31+ |

```bash
# Verificar qué versión está activa
mount | grep cgroup
# cgroup2 on /sys/fs/cgroup type cgroup2 (...)  → v2
# cgroup on /sys/fs/cgroup/cpu type cgroup (...) → v1

# Alternativa rápida
stat -fc %T /sys/fs/cgroup/
# cgroup2fs → v2
# tmpfs     → v1 (raíz tmpfs con subdirectorios por controlador)
```

Este tema cubre **solo v2** por ser el estándar actual.

---

## Jerarquía de cgroups v2

La jerarquía es un **árbol de directorios** en `/sys/fs/cgroup/`. Cada
directorio es un cgroup. Crear un cgroup = crear un directorio:

```
/sys/fs/cgroup/                 ← cgroup raíz
├── cgroup.controllers          ← controladores disponibles en el sistema
├── cgroup.subtree_control      ← controladores habilitados para hijos
├── cgroup.procs                ← PIDs en este cgroup
├── system.slice/               ← servicios del sistema (systemd)
│   ├── sshd.service/
│   ├── nginx.service/
│   └── docker.service/
├── user.slice/                 ← sesiones de usuario
│   └── user-1000.slice/
│       └── session-1.scope/
└── init.scope/                 ← PID 1
```

### Archivos clave en cada cgroup

| Archivo | Contenido |
|---|---|
| `cgroup.controllers` | Controladores disponibles (heredados del padre) |
| `cgroup.subtree_control` | Controladores habilitados para sub-cgroups |
| `cgroup.procs` | PIDs de procesos en este cgroup (escribir para mover) |
| `cgroup.events` | `populated 0/1` — si tiene procesos (útil para monitoring) |
| `cgroup.type` | `domain` (default) o `threaded` |

### La regla "no internal processes"

En cgroups v2, un cgroup **no puede tener procesos propios Y
`subtree_control` activo al mismo tiempo**. Los procesos solo pueden
estar en cgroups **hoja** (leaf):

```
/sys/fs/cgroup/
├── subtree_control = "cpu memory pids"    ← controladores activos
├── cgroup.procs → ❌ no puedes tener procesos aquí
├── app.slice/
│   ├── subtree_control = "cpu memory pids"
│   ├── cgroup.procs → ❌ tampoco aquí (tiene hijos controlados)
│   ├── web.scope/
│   │   └── cgroup.procs → ✅ hoja, puede tener procesos
│   └── worker.scope/
│       └── cgroup.procs → ✅ hoja, puede tener procesos
```

¿Por qué? En v1, un proceso en un nodo intermedio podía "escapar" de los
límites de sus hijos, complicando la contabilidad. v2 elimina esta
ambigüedad: los recursos se contabilizan solo en las hojas.

Implicación práctica: si intentas `echo $$ > cgroup.procs` en un cgroup
con `subtree_control` activo, el kernel retorna `EBUSY`.

---

## Controladores

Los controladores son módulos del kernel que gestionan recursos
específicos:

```bash
# Controladores disponibles en el sistema
cat /sys/fs/cgroup/cgroup.controllers
# cpuset cpu io memory hugetlb pids rdma misc

# Controladores habilitados para sub-cgroups
cat /sys/fs/cgroup/cgroup.subtree_control
# cpu io memory pids
```

Para habilitar un controlador en los hijos:

```bash
# Habilitar cpu y memory para sub-cgroups
echo "+cpu +memory" | sudo tee /sys/fs/cgroup/cgroup.subtree_control

# Deshabilitar uno
echo "-io" | sudo tee /sys/fs/cgroup/cgroup.subtree_control
```

### Controlador memory

| Archivo | Función | Ejemplo |
|---|---|---|
| `memory.current` | Uso actual (bytes) | `5242880` (5 MB) |
| `memory.min` | Garantía mínima (no reclamable) | Protege memoria crítica |
| `memory.low` | Protección soft (best-effort) | Reclamable bajo presión extrema |
| `memory.high` | **Soft limit** → throttle | Frena al proceso, no lo mata |
| `memory.max` | **Hard limit** → OOM kill | Excederlo invoca al OOM killer |
| `memory.swap.max` | Límite de swap | `0` = sin swap |
| `memory.stat` | Estadísticas detalladas | anon, file, shmem... |
| `memory.events` | Contadores de eventos | oom, oom_kill, high, max |

```
Jerarquía de protección de memoria:

memory.min ≤ memory.low ≤ memory.high ≤ memory.max

  min: "nunca reclamar esta memoria"
  low: "evitar reclamar si es posible"
  high: "si excede, throttle (frenar, NO matar)"
  max: "si excede y no se puede reclamar, OOM kill"
```

La diferencia clave: `memory.high` frena el proceso (lo pone en espera
para que el kernel reclame páginas) mientras que `memory.max` directamente
invoca al OOM killer.

```bash
# Dentro del contenedor
cat /sys/fs/cgroup/memory.current    # uso actual
cat /sys/fs/cgroup/memory.max        # "max" = sin límite
cat /sys/fs/cgroup/memory.stat       # desglose detallado

# Campos importantes de memory.stat:
#   anon     — memoria anónima (heap, stack, mmap anónimo)
#   file     — page cache (archivos mapeados)
#   shmem    — shared memory (System V, POSIX)
#   pgfault  — page faults totales
#   pgmajfault — page faults mayores (requirieron I/O)
```

### Controlador cpu

| Archivo | Función | Formato |
|---|---|---|
| `cpu.max` | Límite absoluto de CPU | `cuota periodo` (microsegundos) |
| `cpu.weight` | Peso relativo (rango 1-10000) | `100` (default) |
| `cpu.stat` | Estadísticas de uso | usage_usec, nr_throttled... |

```bash
# cpu.max — límite absoluto
cat /sys/fs/cgroup/cpu.max
# max 100000
# ^^^  ^^^^^^
# cuota periodo (microsegundos)

# "max 100000" = sin límite
# "50000 100000"  = 50ms/100ms = 50% de 1 core
# "100000 100000" = 100ms/100ms = 1 core completo
# "200000 100000" = 200ms/100ms = 2 cores máximo

# cpu.weight — peso relativo (solo importa bajo contención)
cat /sys/fs/cgroup/cpu.weight
# 100  (default)
# Un cgroup con peso 200 recibe el doble de CPU que uno con peso 100
# (cuando ambos compiten por CPU)
```

Diferencia importante: `cpu.max` es un **techo absoluto** (incluso si hay
CPU libre, no puede excederlo). `cpu.weight` es **proporcional** (solo
afecta cuando hay contención):

```
cpu.max = 50000 100000:
  Con CPU libre:    usa hasta 50%
  Con contención:   usa hasta 50%  ← siempre limitado

cpu.weight = 200 (vs otro con 100):
  Con CPU libre:    usa 100%       ← no limita
  Con contención:   recibe 2/3     ← proporcional
```

### Controlador io

```bash
# Límite de I/O por dispositivo
cat /sys/fs/cgroup/io.max
# 8:0 rbps=max wbps=max riops=max wiops=max
# ^^^
# major:minor del dispositivo de bloque

# rbps  = read bytes per second
# wbps  = write bytes per second
# riops = read I/O operations per second
# wiops = write I/O operations per second

# Ejemplo: máximo 10 MB/s lectura en sda (8:0)
# echo "8:0 rbps=10485760" > io.max
```

### Controlador pids

```bash
# Límite de procesos/threads en el cgroup
cat /sys/fs/cgroup/pids.max
# max  (sin límite)

cat /sys/fs/cgroup/pids.current
# 5

# pids.max previene fork bombs dentro de un contenedor
# docker run --pids-limit 100 → pids.max = 100
# Si se alcanza: fork() retorna EAGAIN
```

---

## Qué pasa cuando se excede un límite

| Controlador | Acción | Consecuencia |
|---|---|---|
| `memory.high` | **Throttle** (frenar) | Proceso se pausa, kernel reclama páginas |
| `memory.max` | **OOM kill** | Kernel mata un proceso del cgroup con SIGKILL |
| `cpu.max` | **Throttle** (pausar) | Proceso se pausa hasta el siguiente periodo |
| `pids.max` | **EAGAIN** | `fork()` falla, no se crean nuevos procesos |

```bash
# Ver si un cgroup ha tenido eventos de OOM
cat /sys/fs/cgroup/memory.events
# low 0
# high 0          ← veces que se alcanzó memory.high
# max 5           ← veces que se alcanzó memory.max
# oom 3           ← OOM killer invocado
# oom_kill 3      ← procesos matados por OOM

# dmesg muestra: "Memory cgroup out of memory: Killed process..."
# docker events muestra: container oom
```

---

## Ver el cgroup de un proceso

```bash
# Tu shell
cat /proc/$$/cgroup
# 0::/user.slice/user-1000.slice/session-1.scope
# ^  ^
# |  ruta en la jerarquía
# hierarchy-ID (siempre 0 en v2)

# Dentro de un contenedor (con cgroup namespace):
cat /proc/self/cgroup
# 0::/
# (ve su cgroup como raíz — el cgroup namespace oculta la ruta real)

# Desde el host, el mismo proceso se vería como:
# 0::/system.slice/docker-abc123.scope
```

El **cgroup namespace** (visto en T01) hace que cada contenedor vea su
propio cgroup como `/`, ocultando la jerarquía del host.

---

## systemd y cgroups

systemd crea automáticamente un cgroup por cada **service**, **scope** y
**slice**:

```
Terminología systemd:
  slice   → agrupación jerárquica (system.slice, user.slice, machine.slice)
  service → unidad gestionada por systemd (sshd.service, nginx.service)
  scope   → grupo de procesos externos (session-1.scope, docker containers)
```

```bash
# Ver la jerarquía de cgroups de systemd
systemd-cgls
# Control group /:
# -.slice
# ├─init.scope
# │ └─1 /usr/lib/systemd/systemd
# ├─system.slice
# │ ├─sshd.service
# │ │ └─842 /usr/sbin/sshd
# │ └─docker.service
# │   └─900 /usr/bin/dockerd
# └─user.slice
#   └─user-1000.slice
#     └─session-1.scope
#       ├─1500 sshd: dev
#       └─1506 -bash
```

### Limitar recursos con systemd

systemd traduce sus propiedades a archivos de cgroup:

| Propiedad systemd | Archivo cgroup v2 | Ejemplo |
|---|---|---|
| `MemoryMax=512M` | `memory.max = 536870912` | Hard limit |
| `MemoryHigh=256M` | `memory.high = 268435456` | Soft limit (throttle) |
| `CPUQuota=50%` | `cpu.max = 50000 100000` | 50% de un core |
| `CPUWeight=200` | `cpu.weight = 200` | Peso relativo |
| `TasksMax=100` | `pids.max = 100` | Límite de procesos |
| `IOReadBandwidthMax=/dev/sda 10M` | `io.max = 8:0 rbps=10485760` | Límite I/O |

```bash
# Establecer límites en caliente
sudo systemctl set-property nginx.service MemoryMax=512M
sudo systemctl set-property nginx.service CPUQuota=50%

# Ver los límites actuales
systemctl show nginx.service -p MemoryMax,CPUQuota,TasksMax

# En el unit file directamente:
# [Service]
# MemoryMax=512M
# MemoryHigh=256M
# CPUQuota=50%
# TasksMax=100
```

---

## Crear un cgroup manualmente

```bash
# 1. Guardar cgroup original (para poder volver)
ORIGINAL_CGROUP=$(cat /proc/$$/cgroup | cut -d: -f3)
echo "Cgroup actual: $ORIGINAL_CGROUP"

# 2. Crear un cgroup
sudo mkdir /sys/fs/cgroup/test-group

# 3. Verificar controladores disponibles (heredados del padre)
cat /sys/fs/cgroup/test-group/cgroup.controllers

# 4. Establecer límites
echo 104857600 | sudo tee /sys/fs/cgroup/test-group/memory.max   # 100 MB
echo 20 | sudo tee /sys/fs/cgroup/test-group/pids.max            # 20 procesos

# 5. Mover tu shell al cgroup
echo $$ | sudo tee /sys/fs/cgroup/test-group/cgroup.procs

# 6. Verificar
cat /proc/$$/cgroup             # 0::/test-group
cat /sys/fs/cgroup/test-group/memory.current
cat /sys/fs/cgroup/test-group/pids.current

# 7. Volver al cgroup original y limpiar
echo $$ | sudo tee /sys/fs/cgroup${ORIGINAL_CGROUP}/cgroup.procs
sudo rmdir /sys/fs/cgroup/test-group
```

> **Nota**: `rmdir` solo funciona si el cgroup no tiene procesos ni
> sub-cgroups. Hay que mover todos los procesos fuera antes de eliminar.

---

## cgroups y Docker

Docker traduce cada flag de `docker run` a escribir un valor en el archivo
cgroup correspondiente:

| `docker run` flag | Archivo cgroup v2 | Valor |
|---|---|---|
| `--memory 256m` | `memory.max` | `268435456` |
| `--memory-reservation 128m` | `memory.low` | `134217728` |
| `--cpus 0.5` | `cpu.max` | `50000 100000` |
| `--pids-limit 100` | `pids.max` | `100` |

> `--cpu-shares` (v1) se convierte a `cpu.weight` (v2) con una fórmula
> no lineal: `weight = 1 + ((shares - 2) × 9999) / 262142`. No es una
> correspondencia directa.

```bash
# Ver los límites de este contenedor (desde dentro)
echo "Memoria:"
MEM_MAX=$(cat /sys/fs/cgroup/memory.max 2>/dev/null)
MEM_CUR=$(cat /sys/fs/cgroup/memory.current 2>/dev/null)
if [ "$MEM_MAX" != "max" ] && [ -n "$MEM_MAX" ]; then
    echo "  Límite: $((MEM_MAX / 1024 / 1024)) MB"
else
    echo "  Límite: sin límite"
fi
echo "  Uso actual: $((MEM_CUR / 1024 / 1024)) MB"

echo "CPU:"
echo "  cpu.max: $(cat /sys/fs/cgroup/cpu.max 2>/dev/null)"

echo "PIDs:"
echo "  Límite: $(cat /sys/fs/cgroup/pids.max 2>/dev/null)"
echo "  Actual: $(cat /sys/fs/cgroup/pids.current 2>/dev/null)"

# Desde el host — inspeccionar el cgroup del contenedor
CPID=$(docker inspect --format '{{.State.Pid}}' mi-container)
CGROUP=$(cat /proc/$CPID/cgroup | cut -d: -f3)
cat /sys/fs/cgroup${CGROUP}/memory.max
cat /sys/fs/cgroup${CGROUP}/cpu.max
cat /sys/fs/cgroup${CGROUP}/pids.max
```

### Namespaces vs cgroups — resumen

| Aspecto | Namespaces | Cgroups |
|---|---|---|
| Propósito | Qué **VE** el proceso | Cuánto puede **USAR** |
| Mecanismo | Aislar visibilidad | Limitar recursos |
| Ejemplo | Solo ve sus PIDs | Máx 256 MB de RAM |
| API | `/proc/[pid]/ns/` | `/sys/fs/cgroup/` |
| Sin ellos | Ve todo el sistema | Puede consumir todo |
| Syscalls | `clone`, `unshare`, `setns` | mkdir + write en cgroupfs |

Un contenedor necesita **ambos**: namespaces para aislamiento, cgroups
para límites.

---

## Ejercicios

### Ejercicio 1 — Versión de cgroups y jerarquía

Determina qué versión de cgroups usa tu sistema y explora la raíz.

```bash
# ¿v1 o v2?
mount | grep cgroup
stat -fc %T /sys/fs/cgroup/

# Contenido de la raíz
ls /sys/fs/cgroup/

# ¿En qué cgroup estás?
cat /proc/$$/cgroup

# ¿Qué controladores existen?
cat /sys/fs/cgroup/cgroup.controllers
```

<details><summary>Predicción</summary>

- `mount | grep cgroup` mostrará `cgroup2 on /sys/fs/cgroup type cgroup2`
  (si el kernel es moderno). Si ves múltiples líneas con `cgroup` y
  rutas como `/sys/fs/cgroup/cpu`, es v1.
- `stat -fc %T` devolverá `cgroup2fs` en v2 o `tmpfs` en v1.
- `ls` mostrará archivos como `cgroup.controllers`, `cgroup.procs`,
  `cpu.stat`, `memory.current`, más subdirectorios como `system.slice/`,
  `user.slice/`, `init.scope/`.
- `/proc/$$/cgroup` mostrará `0::/ruta` — un solo campo con hierarchy 0,
  sin nombre de controlador (campo vacío), y la ruta. Dentro de un
  contenedor: `0::/`.
- `cgroup.controllers` listará los disponibles: probablemente
  `cpuset cpu io memory hugetlb pids rdma misc`.

</details>

### Ejercicio 2 — Leer el controlador memory

Inspecciona el uso de memoria del cgroup actual.

```bash
# Uso actual
cat /sys/fs/cgroup/memory.current

# Convertir a MB
echo "$(( $(cat /sys/fs/cgroup/memory.current) / 1024 / 1024 )) MB"

# ¿Hay límite?
cat /sys/fs/cgroup/memory.max

# ¿Hay soft limit?
cat /sys/fs/cgroup/memory.high 2>/dev/null

# Desglose
cat /sys/fs/cgroup/memory.stat | head -5

# ¿Ha habido OOM kills?
cat /sys/fs/cgroup/memory.events
```

<details><summary>Predicción</summary>

- `memory.current` muestra bytes (p.ej. `8388608` = 8 MB).
- `memory.max` probablemente dice `max` (sin límite) salvo que Docker
  fue lanzado con `--memory`.
- `memory.high` probablemente dice `max` (sin soft limit).
- `memory.stat` muestra campos como `anon`, `file`, `shmem`, cada uno
  con un valor en bytes. `anon` es la memoria de heap/stack,
  `file` es page cache.
- `memory.events` debería tener `oom 0` y `oom_kill 0` (sin OOM hasta
  ahora). Si alguno es >0, el contenedor ha sufrido OOM kills.

</details>

### Ejercicio 3 — Leer los controladores cpu y pids

```bash
# CPU: límite absoluto
cat /sys/fs/cgroup/cpu.max

# CPU: peso relativo
cat /sys/fs/cgroup/cpu.weight

# CPU: estadísticas
cat /sys/fs/cgroup/cpu.stat

# PIDs: cuántos hay y cuántos se permiten
cat /sys/fs/cgroup/pids.current
cat /sys/fs/cgroup/pids.max
```

<details><summary>Predicción</summary>

- `cpu.max`: probablemente `max 100000` (sin límite de CPU). Si Docker
  fue lanzado con `--cpus 0.5`, sería `50000 100000`.
- `cpu.weight`: `100` (default).
- `cpu.stat`: mostrará `usage_usec` (tiempo total de CPU en
  microsegundos), `user_usec`, `system_usec`, y `nr_throttled`
  (debería ser 0 si no hay límite).
- `pids.current`: un número pequeño (cuántos procesos hay ahora en el
  cgroup).
- `pids.max`: `max` salvo que Docker usó `--pids-limit`.

</details>

### Ejercicio 4 — Formato de `/proc/PID/cgroup`

Compara el formato desde distintos contextos.

```bash
# Dentro del contenedor
cat /proc/self/cgroup
cat /proc/1/cgroup

# ¿Son iguales? ¿Por qué?

# Comparar con lo que se ve en el host (si tienes acceso):
# CPID=$(docker inspect --format '{{.State.Pid}}' debian-dev)
# cat /proc/$CPID/cgroup
```

<details><summary>Predicción</summary>

- Tanto `self` como PID 1 muestran `0::/` dentro del contenedor.
  Esto es porque el **cgroup namespace** (visto en T01) hace que el
  contenedor vea su propio cgroup como la raíz `/`.
- Desde el host, el mismo proceso mostraría algo como
  `0::/system.slice/docker-abc123def456.scope` — la ruta real en la
  jerarquía del host.
- El formato `0::/ruta` tiene tres campos separados por `:`:
  - `0` = hierarchy ID (siempre 0 en v2)
  - `` (vacío) = lista de controladores (vacía en v2; en v1 decía
    `cpu,cpuacct` etc.)
  - `/ruta` = path del cgroup

</details>

### Ejercicio 5 — subtree_control y la regla "no internal processes"

Explora cómo se habilitan controladores para sub-cgroups.

```bash
# ¿Qué controladores están habilitados para hijos?
cat /sys/fs/cgroup/cgroup.subtree_control

# ¿Y dentro de tu cgroup?
CGROUP_PATH="/sys/fs/cgroup$(cat /proc/$$/cgroup | cut -d: -f3)"
cat "$CGROUP_PATH/cgroup.subtree_control" 2>/dev/null

# ¿Hay sub-cgroups debajo de tu cgroup?
ls -d "$CGROUP_PATH"/*/ 2>/dev/null

# Si subtree_control tiene controladores Y hay procesos directos,
# ¿qué implica eso?
cat "$CGROUP_PATH/cgroup.procs" | head -5
```

<details><summary>Predicción</summary>

- La raíz `/sys/fs/cgroup/cgroup.subtree_control` tendrá algo como
  `cpu io memory pids` — los controladores delegados a hijos.
- Tu cgroup (probablemente una hoja como `session-1.scope` o `/`)
  probablemente tiene `subtree_control` vacío (sin hijos controlados),
  lo que permite tener procesos directos.
- Si `subtree_control` tiene controladores Y hay hijos, entonces
  `cgroup.procs` debería estar vacío (los procesos solo en hojas).
  Intentar escribir un PID daría error `EBUSY`.
- Dentro de un contenedor, estás en el cgroup raíz del namespace
  (`0::/`) y probablemente no hay sub-cgroups visibles.

</details>

### Ejercicio 6 — cpu.max vs cpu.weight

Entiende la diferencia entre límite absoluto y peso relativo.

```bash
# Leer ambos valores
cat /sys/fs/cgroup/cpu.max
cat /sys/fs/cgroup/cpu.weight

# Si cpu.max = "max 100000":
#   ¿Qué porcentaje de CPU puede usar este cgroup?
#   ¿Qué pasa si la CPU está libre?
#   ¿Qué pasa si hay contención?

# Si cpu.max = "50000 100000":
#   ¿Cuánta CPU puede usar como máximo?
#   ¿Importa si la CPU está libre?
```

<details><summary>Predicción</summary>

- Con `cpu.max = max 100000`: no hay límite absoluto. El proceso puede
  usar todo el CPU disponible. El `cpu.weight` solo importa cuando hay
  **contención** (varios cgroups compitiendo por CPU).
- Con `cpu.max = 50000 100000`: el cgroup puede usar máximo 50% de un
  core (50ms de cada periodo de 100ms). **Incluso si la CPU está
  100% libre**, no puede exceder el 50%. Es un techo duro.
- `cpu.weight = 100` (default) solo determina la proporción cuando
  hay competencia. Si un cgroup tiene peso 200 y otro 100, bajo
  contención el primero recibe 2/3 y el segundo 1/3.
- Docker `--cpus 0.5` establece `cpu.max`, no `cpu.weight`. Es decir,
  limita de forma absoluta.

</details>

### Ejercicio 7 — memory.events: detectar OOM

Investiga si el cgroup ha sufrido algún evento de memoria.

```bash
# Leer eventos de memoria
cat /sys/fs/cgroup/memory.events

# Interpretar cada campo
# ¿Cuál indica que el OOM killer actuó?
# ¿Cuál indica que se alcanzó el soft limit?

# Leer memoria actual vs límite
echo "Uso: $(cat /sys/fs/cgroup/memory.current) bytes"
echo "Max: $(cat /sys/fs/cgroup/memory.max)"

# ¿Cuánto porcentaje del límite se usa?
if [ "$(cat /sys/fs/cgroup/memory.max)" != "max" ]; then
    USAGE=$(cat /sys/fs/cgroup/memory.current)
    LIMIT=$(cat /sys/fs/cgroup/memory.max)
    echo "Uso: $(( USAGE * 100 / LIMIT ))%"
fi
```

<details><summary>Predicción</summary>

- `memory.events` muestra contadores acumulativos:
  - `low` — veces que se cayó por debajo de `memory.low`
  - `high` — veces que se excedió `memory.high` (throttle)
  - `max` — veces que se intentó exceder `memory.max`
  - `oom` — veces que se invocó el OOM killer
  - `oom_kill` — procesos efectivamente matados por OOM
- Si `oom_kill > 0`, el contenedor ha sufrido OOM kills.
- Si `high > 0`, el contenedor ha sido throttled por memoria.
- Si `memory.max` es `max`, el cálculo de porcentaje no se ejecuta
  (el `if` lo filtra).

</details>

### Ejercicio 8 — Mapeo Docker → cgroup

Verifica cómo Docker traduce sus flags a archivos de cgroup.

```bash
# Leer los límites del contenedor actual
echo "=== memory.max ==="
cat /sys/fs/cgroup/memory.max

echo "=== cpu.max ==="
cat /sys/fs/cgroup/cpu.max

echo "=== pids.max ==="
cat /sys/fs/cgroup/pids.max

echo "=== io.max ==="
cat /sys/fs/cgroup/io.max 2>/dev/null || echo "(no disponible)"

# Pregunta: si al levantar el contenedor usamos
# docker run --memory 256m --cpus 0.5 --pids-limit 100
# ¿qué valores esperarías en cada archivo?
```

<details><summary>Predicción</summary>

- Si no se pasaron flags de límite, todos dirán `max` (sin límite).
- Con `--memory 256m`: `memory.max = 268435456` (256 × 1024 × 1024).
- Con `--cpus 0.5`: `cpu.max = 50000 100000` (50ms de cada 100ms).
- Con `--pids-limit 100`: `pids.max = 100`.
- `io.max` probablemente no está disponible o muestra `max` salvo
  que se usó `--device-read-bps` o similar.
- El contenedor del curso probablemente no tiene ninguno de estos
  límites establecidos, así que todo debería ser `max`.

</details>

### Ejercicio 9 — Comparar cgroups de dos procesos

Compara el cgroup de distintos procesos dentro del contenedor.

```bash
# Tu shell
cat /proc/$$/cgroup

# PID 1 del contenedor
cat /proc/1/cgroup

# Lanzar un proceso hijo y verificar su cgroup
sleep 60 &
BGPID=$!
cat /proc/$BGPID/cgroup
kill $BGPID

# ¿Están todos en el mismo cgroup? ¿Por qué?

# Ahora comparar los procesos listados en cgroup.procs
cat /sys/fs/cgroup/cgroup.procs | head -10
```

<details><summary>Predicción</summary>

- Todos los procesos muestran `0::/` — están en el mismo cgroup.
  Dentro de un contenedor, todos los procesos comparten el cgroup
  del contenedor (que Docker crea para ellos).
- El hijo `sleep` hereda el cgroup de su padre (como hereda el
  namespace). En cgroups v2, un proceso `fork()`-eado queda en el
  mismo cgroup que su padre.
- `cgroup.procs` lista todos los PIDs del cgroup. Debería incluir
  PID 1 (init del contenedor), `$$` (tu shell), y cualquier proceso
  hijo que esté corriendo.
- Todos comparten los mismos límites de recursos — un solo cgroup
  para todo el contenedor.

</details>

### Ejercicio 10 — Namespaces + cgroups = contenedor

Ejercicio integrador: ¿qué aísla cada tecnología en tu contenedor?

```bash
echo "=== Lo que VE el proceso (Namespaces) ==="
echo "PID namespace:"
echo "  PID 1 = $(cat /proc/1/comm) (no systemd del host)"
echo "  Total procesos: $(ls /proc | grep -c '^[0-9]')"

echo "UTS namespace:"
echo "  Hostname: $(hostname)"

echo "Cgroup namespace:"
echo "  Mi cgroup: $(cat /proc/self/cgroup)"
echo "  (ve / en vez de la ruta real del host)"

echo ""
echo "=== Lo que PUEDE USAR el proceso (Cgroups) ==="
echo "Memoria: $(cat /sys/fs/cgroup/memory.max)"
echo "CPU:     $(cat /sys/fs/cgroup/cpu.max 2>/dev/null)"
echo "PIDs:    $(cat /sys/fs/cgroup/pids.max 2>/dev/null)"
echo "I/O:     $(cat /sys/fs/cgroup/io.max 2>/dev/null || echo 'N/A')"

echo ""
echo "=== Conclusión ==="
echo "Namespaces: lo que VE → PIDs propios, hostname propio, cgroup /"
echo "Cgroups:    lo que USA → límites de memoria, CPU, procesos"
echo "Juntos = un contenedor"
```

<details><summary>Predicción</summary>

- **Namespaces** (visibilidad):
  - PID 1 será `bash` o `sleep` (el entrypoint del contenedor), no
    `systemd` — aislamiento PID.
  - El hostname será el del contenedor (ID corto o nombre configurado).
  - El cgroup se ve como `0::/` — aislamiento cgroup.
- **Cgroups** (recursos):
  - Si no se configuraron límites, todo dice `max`.
  - Si Docker usó flags, aparecen los valores numéricos.
- Este ejercicio muestra la **complementariedad**: namespaces ocultan
  el host, cgroups limitan el consumo. Un proceso con solo namespaces
  no ve el host pero puede consumir toda su RAM. Un proceso con solo
  cgroups está limitado pero puede ver los procesos del host. Se
  necesitan ambos para un contenedor funcional.

</details>
