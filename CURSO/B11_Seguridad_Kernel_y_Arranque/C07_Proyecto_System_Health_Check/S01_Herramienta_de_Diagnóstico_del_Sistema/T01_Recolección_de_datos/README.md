# Recolección de Datos — disk usage, memory, load average, servicios fallidos

## Índice

1. [Objetivo del proyecto](#1-objetivo-del-proyecto)
2. [Arquitectura de la herramienta](#2-arquitectura-de-la-herramienta)
3. [Fuentes de datos del sistema](#3-fuentes-de-datos-del-sistema)
4. [Disk usage: df y /proc/mounts](#4-disk-usage-df-y-procmounts)
5. [Memory: free y /proc/meminfo](#5-memory-free-y-procmeminfo)
6. [Load average: uptime y /proc/loadavg](#6-load-average-uptime-y-procloadavg)
7. [Servicios fallidos: systemctl --failed](#7-servicios-fallidos-systemctl---failed)
8. [Métricas adicionales](#8-métricas-adicionales)
9. [Implementación: script de recolección](#9-implementación-script-de-recolección)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Objetivo del proyecto

En este capítulo construirás una **herramienta de diagnóstico del sistema** que recolecta, formatea y presenta información de salud de un servidor Linux. El proyecto se desarrolla en 4 temas incrementales:

```
T01 — Recolección de datos (este tema)
  └── Obtener métricas crudas del sistema

T02 — Formateo y reporte
  └── Presentar los datos con colores y umbrales

T03 — Mejora 1: exportar como JSON o HTML
  └── Generar reportes en formatos consumibles

T04 — Mejora 2: interfaz TUI con ratatui
  └── Dashboard interactivo en terminal (Rust)
```

### Qué construiremos en T01

Un script Bash que recolecta las métricas fundamentales del sistema y las presenta de forma estructurada. El script será la base sobre la que construiremos el formateo y las mejoras posteriores.

```
┌─────────────────────────────────────────────────┐
│              System Health Check                │
│              hostname — 2026-03-21              │
├─────────────────────────────────────────────────┤
│                                                 │
│  DISK:     /       42G/50G  (84%)               │
│            /home  200G/449G (44%)               │
│                                                 │
│  MEMORY:   Used: 6.2G/15.6G (39%)              │
│            Swap: 0.1G/4.0G  (2%)               │
│                                                 │
│  LOAD:     0.45  0.62  0.38  (CPUs: 8)         │
│                                                 │
│  SERVICES: 0 failed                             │
│                                                 │
│  UPTIME:   14 days, 3 hours                     │
│                                                 │
└─────────────────────────────────────────────────┘
```

---

## 2. Arquitectura de la herramienta

### Filosofía: leer de /proc y /sys, no de comandos

Hay dos formas de obtener información del sistema:

```
Método 1: Parsear la salida de comandos
  df -h | awk '{print $5}' | ...
  ├── Depende del locale (¿"Use%" o "Uso%"?)
  ├── El formato puede cambiar entre versiones
  └── Más lento (fork+exec por cada comando)

Método 2: Leer directamente de /proc y /sys
  cat /proc/meminfo
  ├── Formato estable (interfaz del kernel)
  ├── Independiente del locale
  └── Más rápido (solo lectura de pseudo-archivo)

Nuestra estrategia: usar /proc//sys cuando sea posible,
comandos solo cuando no haya alternativa directa.
```

### Estructura del script

```
health_check.sh
│
├── Funciones de recolección (datos crudos)
│   ├── collect_disk()       → datos de filesystems
│   ├── collect_memory()     → RAM y swap
│   ├── collect_load()       → load average y CPUs
│   ├── collect_services()   → servicios fallidos
│   └── collect_uptime()     → tiempo activo
│
├── Funciones de presentación (T02)
│   ├── format_disk()
│   ├── format_memory()
│   └── ...
│
└── Main
    ├── Recolectar todo
    ├── Formatear
    └── Presentar
```

### Separar recolección de presentación

```
¿Por qué separar?

Recolección (T01):      Presentación (T02+):
  Números crudos          Colores, umbrales
  Sin formato             Formato terminal
  Sin juicios             OK/WARN/CRIT
  Reutilizable            Específico del output

Si mezclas recolección y presentación, no puedes:
  - Exportar como JSON (T03) sin duplicar lógica
  - Cambiar el formato sin tocar la recolección
  - Testear la recolección independientemente
```

---

## 3. Fuentes de datos del sistema

### Mapa de fuentes

```
Métrica              Fuente directa (/proc, /sys)       Comando alternativo
────────────────────────────────────────────────────────────────────────────
Disk usage           /proc/mounts + stat -f              df
Memory               /proc/meminfo                       free
Load average         /proc/loadavg                       uptime
CPU count            /proc/cpuinfo o /sys/.../online     nproc
Uptime               /proc/uptime                        uptime
Servicios fallidos   (no hay pseudo-archivo)              systemctl --failed
Hostname             /proc/sys/kernel/hostname            hostname
Kernel version       /proc/version                       uname -r
```

> **Nota**: para servicios systemd no hay alternativa a `systemctl`. Es la API correcta para consultar el estado del init system.

---

## 4. Disk usage: df y /proc/mounts

### Lectura directa desde el sistema

```bash
# /proc/mounts: lista todos los filesystems montados
# Formato: device mountpoint fstype options dump pass
cat /proc/mounts | head -5
# /dev/sda2 / ext4 rw,relatime 0 0
# /dev/sda1 /boot ext4 rw,relatime 0 0
# tmpfs /tmp tmpfs rw,nosuid,nodev 0 0

# stat -f: información de un filesystem (no requiere root)
stat -f /
# Muestra: tipo, block size, bloques totales/libres/disponibles
```

### Usar df de forma parseable

```bash
# df con formato de salida controlado (independiente del locale)
df -P -B1 --output=source,target,size,used,avail,pcent \
    -x tmpfs -x devtmpfs -x squashfs -x overlay 2>/dev/null

# -P: formato POSIX (una línea por filesystem)
# -B1: tamaños en bytes (fácil de calcular)
# --output: seleccionar columnas exactas
# -x: excluir tipos de filesystem virtuales
```

### Función de recolección de disco

```bash
collect_disk() {
    # Retorna líneas con formato: mountpoint|size_bytes|used_bytes|avail_bytes|percent
    df -P -B1 --output=target,size,used,avail,pcent \
        -x tmpfs -x devtmpfs -x squashfs -x overlay 2>/dev/null | \
        tail -n +2 | \
        while read -r mount size used avail pct; do
            pct=${pct%%%}  # Eliminar el símbolo %
            echo "${mount}|${size}|${used}|${avail}|${pct}"
        done
}

# Uso:
# collect_disk
# /|53687091200|45097156608|5872025600|84
# /home|482082029568|214748364800|243056345088|44
# /boot|1063256064|209715200|799014912|19
```

### Convertir bytes a formato legible

```bash
human_bytes() {
    local bytes=$1
    if [ "$bytes" -ge 1073741824 ]; then
        echo "$(( bytes / 1073741824 ))G"
    elif [ "$bytes" -ge 1048576 ]; then
        echo "$(( bytes / 1048576 ))M"
    elif [ "$bytes" -ge 1024 ]; then
        echo "$(( bytes / 1024 ))K"
    else
        echo "${bytes}B"
    fi
}

# Para más precisión (con decimales), usar awk:
human_bytes_precise() {
    local bytes=$1
    awk -v b="$bytes" 'BEGIN {
        if (b >= 1073741824) printf "%.1fG\n", b/1073741824
        else if (b >= 1048576) printf "%.1fM\n", b/1048576
        else if (b >= 1024) printf "%.1fK\n", b/1024
        else printf "%dB\n", b
    }'
}
```

---

## 5. Memory: free y /proc/meminfo

### /proc/meminfo: la fuente primaria

```bash
cat /proc/meminfo | head -10
# MemTotal:       16384000 kB
# MemFree:         2048000 kB
# MemAvailable:   10240000 kB
# Buffers:          512000 kB
# Cached:          6144000 kB
# SwapCached:        32000 kB
# Active:          8192000 kB
# Inactive:        4096000 kB
# SwapTotal:       4194304 kB
# SwapFree:        4096000 kB
```

### Campos importantes y su significado

```
MemTotal       Memoria física total
MemFree        Memoria no usada (sin contar caches)
MemAvailable   Memoria disponible para nuevos procesos (lo que importa)
Buffers        Cache de metadatos de filesystem
Cached         Cache de páginas de archivo
SwapTotal      Swap total configurada
SwapFree       Swap no usada

                  ┌──────────────────────────────────────┐
                  │            MemTotal                   │
                  ├──────┬────────┬───────┬───────────────┤
                  │Used  │Buffers │Cached │   MemFree     │
                  │(apps)│        │       │               │
                  ├──────┴────────┴───────┴───────────────┤
                  │◄─── "Used" en free ──▶│◄─ Available ─▶│
                  │                       │               │
                  │  Memoria realmente    │  Memoria que  │
                  │  ocupada por procesos │  se puede     │
                  │  + no liberada fácil  │  liberar      │
                  └───────────────────────┴───────────────┘

MemAvailable ≈ MemFree + parte de Buffers + parte de Cached
  (el kernel calcula esto — es la métrica correcta para "cuánta
   memoria hay disponible")
```

> **Pregunta de predicción**: Un servidor muestra MemFree=200MB pero MemAvailable=8GB. ¿El servidor está corto de memoria? ¿Por qué hay tanta diferencia?

No está corto de memoria. Linux usa agresivamente la RAM libre para caches de disco (Buffers + Cached). Esa memoria "usada" se libera inmediatamente cuando un proceso la necesita. MemFree es lo que no se usa ni para caches — mantenerlo bajo es eficiente, no un problema. MemAvailable (8GB) es la métrica correcta para evaluar si hay memoria suficiente.

### Función de recolección de memoria

```bash
collect_memory() {
    # Lee /proc/meminfo directamente (más rápido que free, independiente del locale)
    local mem_total mem_avail mem_free swap_total swap_free

    while IFS=': ' read -r key value _unit; do
        case "$key" in
            MemTotal)     mem_total=$value ;;
            MemAvailable) mem_avail=$value ;;
            MemFree)      mem_free=$value ;;
            SwapTotal)    swap_total=$value ;;
            SwapFree)     swap_free=$value ;;
        esac
    done < /proc/meminfo

    # Valores en kB, convertir a bytes para consistencia
    local mem_used_kb=$(( mem_total - mem_avail ))
    local swap_used_kb=$(( swap_total - swap_free ))

    # Porcentajes
    local mem_pct=0
    local swap_pct=0
    [ "$mem_total" -gt 0 ] && mem_pct=$(( mem_used_kb * 100 / mem_total ))
    [ "$swap_total" -gt 0 ] && swap_pct=$(( swap_used_kb * 100 / swap_total ))

    # Formato: tipo|total_kb|used_kb|free_kb|percent
    echo "ram|${mem_total}|${mem_used_kb}|${mem_avail}|${mem_pct}"
    echo "swap|${swap_total}|${swap_used_kb}|${swap_free}|${swap_pct}"
}

# Uso:
# collect_memory
# ram|16384000|6144000|10240000|37
# swap|4194304|98304|4096000|2
```

---

## 6. Load average: uptime y /proc/loadavg

### /proc/loadavg

```bash
cat /proc/loadavg
# 0.45 0.62 0.38 2/384 12847
#  │    │    │    │     │
#  │    │    │    │     └── último PID asignado
#  │    │    │    └── procesos running/total
#  │    │    └── load average 15 min
#  │    └── load average 5 min
#  └── load average 1 min
```

### Interpretar el load average

```
Load average = número promedio de procesos esperando CPU + ejecutando

Regla general (para un sistema con N CPUs):
  load < N        → sistema holgado
  load ≈ N        → sistema bien utilizado
  load > N        → procesos esperando cola (posible bottleneck)
  load >> N × 2   → sistema sobrecargado

Ejemplo con 4 CPUs:
  load 0.5   → 12.5% de capacidad — muy holgado
  load 4.0   → 100% de capacidad — bien utilizado
  load 8.0   → 200% — procesos en cola, respuesta degradada
  load 20.0  → 500% — sistema severamente sobrecargado

Cuidado: load average incluye procesos en estado D (uninterruptible sleep)
que esperan I/O. Un load alto puede ser CPU-bound o IO-bound.
  → Verificar con: iostat, vmstat, o top (%wa)
```

### Función de recolección de load

```bash
collect_load() {
    local load1 load5 load15 running_procs
    read -r load1 load5 load15 running_procs _ < /proc/loadavg

    local cpus
    cpus=$(nproc)

    # Formato: load1|load5|load15|cpus|running/total
    echo "${load1}|${load5}|${load15}|${cpus}|${running_procs}"
}

# Uso:
# collect_load
# 0.45|0.62|0.38|8|2/384
```

---

## 7. Servicios fallidos: systemctl --failed

### Obtener servicios fallidos

```bash
# Listar servicios en estado failed
systemctl --failed --no-legend --no-pager

# Formato de salida:
# nginx.service loaded failed failed A high performance web server
# backup.timer  loaded failed failed Daily backup timer

# Solo contar
systemctl --failed --no-legend --no-pager | wc -l

# Parseable con output específico
systemctl list-units --state=failed --no-legend --no-pager --plain
```

### Función de recolección de servicios

```bash
collect_services() {
    local failed_list
    failed_list=$(systemctl --failed --no-legend --no-pager --plain 2>/dev/null)

    local count=0
    local services=""

    if [ -n "$failed_list" ]; then
        count=$(echo "$failed_list" | wc -l)
        # Extraer solo los nombres de las units
        services=$(echo "$failed_list" | awk '{print $1}' | tr '\n' ',')
        services=${services%,}  # Eliminar trailing comma
    fi

    # Formato: count|service1,service2,...
    echo "${count}|${services}"
}

# Uso:
# collect_services
# 0|
# o
# 2|nginx.service,backup.timer
```

### Obtener uptime

```bash
collect_uptime() {
    local uptime_seconds
    uptime_seconds=$(awk '{print int($1)}' /proc/uptime)

    local days=$(( uptime_seconds / 86400 ))
    local hours=$(( (uptime_seconds % 86400) / 3600 ))
    local minutes=$(( (uptime_seconds % 3600) / 60 ))

    # Formato: seconds|human_readable
    echo "${uptime_seconds}|${days}d ${hours}h ${minutes}m"
}

# Uso:
# collect_uptime
# 1213380|14d 3h 3m
```

---

## 8. Métricas adicionales

### CPU usage (instantáneo)

```bash
collect_cpu() {
    # /proc/stat acumula ticks desde el boot — necesitamos dos muestras
    local cpu_line1 cpu_line2
    cpu_line1=$(head -1 /proc/stat)
    sleep 1
    cpu_line2=$(head -1 /proc/stat)

    # Campos: cpu user nice system idle iowait irq softirq steal
    local u1 n1 s1 i1 w1 u2 n2 s2 i2 w2
    read -r _ u1 n1 s1 i1 w1 _ _ _ _ <<< "$cpu_line1"
    read -r _ u2 n2 s2 i2 w2 _ _ _ _ <<< "$cpu_line2"

    local total1=$(( u1 + n1 + s1 + i1 + w1 ))
    local total2=$(( u2 + n2 + s2 + i2 + w2 ))
    local idle_diff=$(( i2 - i1 ))
    local total_diff=$(( total2 - total1 ))

    local cpu_pct=0
    [ "$total_diff" -gt 0 ] && cpu_pct=$(( (total_diff - idle_diff) * 100 / total_diff ))

    local iowait_pct=0
    [ "$total_diff" -gt 0 ] && iowait_pct=$(( (w2 - w1) * 100 / total_diff ))

    # Formato: cpu_percent|iowait_percent
    echo "${cpu_pct}|${iowait_pct}"
}
```

### Network basic

```bash
collect_network() {
    # Interfaces activas y sus IPs
    while read -r iface; do
        [ "$iface" = "lo" ] && continue
        local ip
        ip=$(ip -4 addr show "$iface" 2>/dev/null | awk '/inet / {print $2}')
        local state
        state=$(cat "/sys/class/net/$iface/operstate" 2>/dev/null)
        [ -n "$ip" ] && echo "${iface}|${ip}|${state}"
    done < <(ls /sys/class/net/)
}
```

### Top processes by memory

```bash
collect_top_mem() {
    # Top 5 procesos por memoria (sin header)
    ps --no-headers -eo pid,pmem,rss,comm --sort=-rss | head -5 | \
        while read -r pid pmem rss comm; do
            echo "${pid}|${pmem}|${rss}|${comm}"
        done
}
```

---

## 9. Implementación: script de recolección

### Script completo de recolección

```bash
#!/bin/bash
# health_collect.sh — Recolección de datos del sistema
# Parte 1 del proyecto System Health Check
#
# Uso: ./health_collect.sh
# Output: datos estructurados (pipe-separated) a stdout
#
# Cada sección se marca con un header [SECTION]
# para facilitar el parseo en T02

set -uo pipefail

# ── Funciones de utilidad ──────────────────────────

human_kb() {
    # Convertir kB a formato legible
    local kb=$1
    awk -v k="$kb" 'BEGIN {
        if (k >= 1048576) printf "%.1fG", k/1048576
        else if (k >= 1024) printf "%.1fM", k/1024
        else printf "%dK", k
    }'
}

# ── Funciones de recolección ───────────────────────

collect_system_info() {
    echo "[SYSTEM]"
    echo "hostname|$(cat /proc/sys/kernel/hostname)"
    echo "kernel|$(cut -d' ' -f3 /proc/version)"
    echo "date|$(date '+%Y-%m-%d %H:%M:%S')"
    echo "uptime|$(awk '{d=int($1/86400); h=int(($1%86400)/3600); m=int(($1%3600)/60); printf "%dd %dh %dm", d, h, m}' /proc/uptime)"
    echo "uptime_seconds|$(awk '{print int($1)}' /proc/uptime)"
}

collect_disk() {
    echo "[DISK]"
    # source|mountpoint|size_kb|used_kb|avail_kb|percent
    df -P -k --output=source,target,size,used,avail,pcent \
        -x tmpfs -x devtmpfs -x squashfs -x overlay -x efivarfs 2>/dev/null | \
        tail -n +2 | \
        while read -r src mount size used avail pct; do
            pct=${pct%%%}
            echo "${src}|${mount}|${size}|${used}|${avail}|${pct}"
        done
}

collect_memory() {
    echo "[MEMORY]"
    local mem_total=0 mem_avail=0 swap_total=0 swap_free=0

    while IFS=': ' read -r key value _unit; do
        case "$key" in
            MemTotal)     mem_total=$value ;;
            MemAvailable) mem_avail=$value ;;
            SwapTotal)    swap_total=$value ;;
            SwapFree)     swap_free=$value ;;
        esac
    done < /proc/meminfo

    local mem_used=$(( mem_total - mem_avail ))
    local swap_used=$(( swap_total - swap_free ))

    local mem_pct=0 swap_pct=0
    [ "$mem_total" -gt 0 ] && mem_pct=$(( mem_used * 100 / mem_total ))
    [ "$swap_total" -gt 0 ] && swap_pct=$(( swap_used * 100 / swap_total ))

    echo "ram|${mem_total}|${mem_used}|${mem_avail}|${mem_pct}"
    echo "swap|${swap_total}|${swap_used}|${swap_free}|${swap_pct}"
}

collect_load() {
    echo "[LOAD]"
    local load1 load5 load15 procs _
    read -r load1 load5 load15 procs _ < /proc/loadavg
    local cpus
    cpus=$(nproc)
    echo "${load1}|${load5}|${load15}|${cpus}|${procs}"
}

collect_services() {
    echo "[SERVICES]"
    local failed
    failed=$(systemctl --failed --no-legend --no-pager --plain 2>/dev/null || true)

    if [ -z "$failed" ]; then
        echo "0|"
    else
        local count
        count=$(echo "$failed" | wc -l)
        local names
        names=$(echo "$failed" | awk '{print $1}' | tr '\n' ',' | sed 's/,$//')
        echo "${count}|${names}"

        # Detalle de cada servicio fallido
        echo "$failed" | awk '{print "detail|" $1 "|" $3 "|" $4}'
    fi
}

collect_top_processes() {
    echo "[TOP_PROCS]"
    # Top 5 por memoria RSS
    ps --no-headers -eo pid,%mem,rss,comm --sort=-rss 2>/dev/null | head -5 | \
        while read -r pid pmem rss comm; do
            echo "${pid}|${pmem}|${rss}|${comm}"
        done
}

# ── Main ───────────────────────────────────────────

collect_system_info
collect_disk
collect_memory
collect_load
collect_services
collect_top_processes
```

### Ejemplo de salida

```
[SYSTEM]
hostname|web-prod-01
kernel|6.19.8-200.fc43.x86_64
date|2026-03-21 14:30:00
uptime|14d 3h 22m
uptime_seconds|1213920
[DISK]
/dev/sda2|/|52428800|44040192|5767168|84
/dev/sda3|/home|470548480|209715200|237027328|44
/dev/sda1|/boot|1038336|204800|780288|19
[MEMORY]
ram|16384000|6144000|10240000|37
swap|4194304|98304|4096000|2
[TOP_PROCS]
1234|12.3|2015324|postgres
5678|8.1|1327104|java
9012|3.4|557056|nginx
```

### Parsear la salida desde otro script

```bash
# En T02, parsearemos esta salida así:
SECTION=""
while IFS= read -r line; do
    # Detectar headers de sección
    if [[ "$line" =~ ^\[([A-Z_]+)\]$ ]]; then
        SECTION="${BASH_REMATCH[1]}"
        continue
    fi

    case "$SECTION" in
        DISK)
            IFS='|' read -r src mount size used avail pct <<< "$line"
            # Procesar datos de disco...
            ;;
        MEMORY)
            IFS='|' read -r type total used free_avail pct <<< "$line"
            # Procesar datos de memoria...
            ;;
        LOAD)
            IFS='|' read -r l1 l5 l15 cpus procs <<< "$line"
            # Procesar datos de load...
            ;;
        SERVICES)
            IFS='|' read -r count names <<< "$line"
            # Procesar datos de servicios...
            ;;
    esac
done < <(./health_collect.sh)
```

---

## 10. Errores comunes

### Error 1: Usar MemFree en vez de MemAvailable

```bash
# MAL — MemFree ignora los caches reutilizables
mem_free=$(awk '/MemFree/ {print $2}' /proc/meminfo)
# Muestra 200 MB "libres" cuando realmente hay 8 GB disponibles

# BIEN — MemAvailable es la métrica correcta
mem_avail=$(awk '/MemAvailable/ {print $2}' /proc/meminfo)
# Incluye la memoria que el kernel puede liberar de caches
```

### Error 2: Parsear la salida de df dependiente del locale

```bash
# MAL — la columna "Use%" puede ser "Uso%" en español
LANG=es_ES.UTF-8 df -h | awk '{print $5}'
# Puede fallar si el header tiene nombre diferente

# BIEN — usar -P (POSIX) o --output
df -P -k | tail -n +2 | awk '{print $5}'
# -P garantiza formato estándar independiente del locale

# AÚN MEJOR — usar --output para columnas específicas
df --output=pcent | tail -n +2
```

### Error 3: No manejar el caso sin swap

```bash
# MAL — división por cero si no hay swap
swap_total=$(awk '/SwapTotal/ {print $2}' /proc/meminfo)
swap_pct=$(( swap_used * 100 / swap_total ))  # Error si swap_total=0

# BIEN — verificar antes de dividir
if [ "$swap_total" -gt 0 ]; then
    swap_pct=$(( swap_used * 100 / swap_total ))
else
    swap_pct=0
fi
```

### Error 4: Comparar load average como entero

```bash
# MAL — bash no maneja decimales
load=$(awk '{print $1}' /proc/loadavg)  # "2.45"
if [ "$load" -gt 4 ]; then ...  # Error: integer expression expected

# BIEN — usar awk o bc para comparaciones decimales
load=$(awk '{print $1}' /proc/loadavg)
overloaded=$(awk -v load="$load" -v cpus="$(nproc)" \
    'BEGIN { print (load > cpus) ? 1 : 0 }')
if [ "$overloaded" -eq 1 ]; then
    echo "System overloaded"
fi
```

### Error 5: Incluir filesystems virtuales en el reporte de disco

```bash
# MAL — incluye tmpfs, devtmpfs, /proc, etc.
df -h
# Muestra docenas de filesystems virtuales que no son relevantes

# BIEN — excluir tipos virtuales
df -h -x tmpfs -x devtmpfs -x squashfs -x overlay -x efivarfs
# Solo muestra discos reales
```

---

## 11. Cheatsheet

```
DISCO
  df -P -k                               Uso (formato POSIX, kB)
  df --output=target,pcent,avail          Columnas específicas
  df -x tmpfs -x devtmpfs                Excluir virtuales
  df -i                                   Uso de inodes

MEMORIA (/proc/meminfo)
  MemTotal        Total de RAM física
  MemAvailable    RAM disponible (la métrica correcta)
  MemFree         RAM sin usar (NO es la métrica correcta)
  SwapTotal       Swap total
  SwapFree        Swap sin usar

LOAD (/proc/loadavg)
  Campo 1         Load average 1 minuto
  Campo 2         Load average 5 minutos
  Campo 3         Load average 15 minutos
  Campo 4         Procesos running/total
  nproc           Número de CPUs

SERVICIOS
  systemctl --failed --no-legend --no-pager       Listar fallidos
  systemctl is-active SERVICIO                     Verificar uno

UPTIME (/proc/uptime)
  Campo 1         Segundos desde boot
  Campo 2         Segundos idle (acumulado todas las CPUs)

PARSEO EN BASH
  IFS='|' read -r a b c <<< "$line"      Parsear delimitado por |
  awk '/^MemTotal/ {print $2}' /proc/meminfo   Extraer campo
  ${var%%%}                                Eliminar % final
```

---

## 12. Ejercicios

### Ejercicio 1: Recolectar y mostrar datos básicos

**Objetivo**: Escribir funciones individuales de recolección y verificar que funcionan.

```bash
# 1. Crear el script base
cat > /tmp/health_ex1.sh << 'SCRIPT'
#!/bin/bash
set -uo pipefail

# Función: información del sistema
echo "=== System ==="
echo "Hostname: $(cat /proc/sys/kernel/hostname)"
echo "Kernel:   $(cut -d' ' -f3 /proc/version)"
echo "Uptime:   $(awk '{d=int($1/86400); h=int(($1%86400)/3600); printf "%dd %dh", d, h}' /proc/uptime)"
echo ""

# Función: disco (solo filesystems reales)
echo "=== Disk ==="
printf "%-20s %8s %8s %8s %5s\n" "Mount" "Size" "Used" "Avail" "Use%"
df -h --output=target,size,used,avail,pcent \
    -x tmpfs -x devtmpfs -x squashfs -x overlay -x efivarfs 2>/dev/null | \
    tail -n +2 | while read -r mount size used avail pct; do
        printf "%-20s %8s %8s %8s %5s\n" "$mount" "$size" "$used" "$avail" "$pct"
    done
echo ""

# Función: memoria
echo "=== Memory ==="
while IFS=': ' read -r key value _; do
    case "$key" in
        MemTotal)     mt=$value ;;
        MemAvailable) ma=$value ;;
        SwapTotal)    st=$value ;;
        SwapFree)     sf=$value ;;
    esac
done < /proc/meminfo
mu=$(( mt - ma ))
mp=$(( mu * 100 / mt ))
echo "RAM:  $(( mu / 1024 ))M / $(( mt / 1024 ))M (${mp}%)"
if [ "$st" -gt 0 ]; then
    su=$(( st - sf ))
    sp=$(( su * 100 / st ))
    echo "Swap: $(( su / 1024 ))M / $(( st / 1024 ))M (${sp}%)"
else
    echo "Swap: none"
fi
echo ""

# Función: load
echo "=== Load ==="
read -r l1 l5 l15 procs _ < /proc/loadavg
cpus=$(nproc)
echo "Load: $l1 $l5 $l15 (CPUs: $cpus)"
echo "Processes: $procs"
echo ""

# Función: servicios
echo "=== Services ==="
failed=$(systemctl --failed --no-legend --no-pager 2>/dev/null)
if [ -z "$failed" ]; then
    echo "All services OK"
else
    echo "FAILED services:"
    echo "$failed"
fi
SCRIPT
chmod +x /tmp/health_ex1.sh

# 2. Ejecutar
/tmp/health_ex1.sh

# 3. Limpiar
rm /tmp/health_ex1.sh
```

**Pregunta de reflexión**: El script lee `/proc/meminfo` con un bucle `while read`. ¿Por qué no usar simplemente `grep MemTotal /proc/meminfo | awk '{print $2}'` para cada campo? ¿Cuál es más eficiente y por qué?

> El bucle `while read` abre y lee `/proc/meminfo` **una sola vez**, extrayendo todos los campos en una pasada. Usar `grep ... | awk` para cada campo requiere abrir y leer el archivo completo 4 veces (MemTotal, MemAvailable, SwapTotal, SwapFree), además de crear 8 procesos (4 grep + 4 awk). Es 4x más I/O y 8x más forks. En un health check que se ejecuta cada minuto, esta diferencia se acumula, y en un sistema bajo carga, minimizar forks es importante.

---

### Ejercicio 2: Formato pipe-separated con secciones

**Objetivo**: Refactorizar el ejercicio 1 para separar recolección de presentación.

```bash
# 1. Script de recolección (solo datos, sin formato visual)
cat > /tmp/collect.sh << 'SCRIPT'
#!/bin/bash
set -uo pipefail

echo "[SYSTEM]"
echo "hostname|$(cat /proc/sys/kernel/hostname)"
echo "kernel|$(cut -d' ' -f3 /proc/version)"
echo "uptime_sec|$(awk '{print int($1)}' /proc/uptime)"

echo "[DISK]"
df -P -k --output=target,size,used,avail,pcent \
    -x tmpfs -x devtmpfs -x squashfs -x overlay -x efivarfs 2>/dev/null | \
    tail -n +2 | while read -r m s u a p; do
        echo "${m}|${s}|${u}|${a}|${p%%%}"
    done

echo "[MEMORY]"
awk '
/^MemTotal:/     { mt=$2 }
/^MemAvailable:/ { ma=$2 }
/^SwapTotal:/    { st=$2 }
/^SwapFree:/     { sf=$2 }
END {
    mu=mt-ma; mp=(mt>0)?int(mu*100/mt):0
    su=st-sf; sp=(st>0)?int(su*100/st):0
    print "ram|" mt "|" mu "|" ma "|" mp
    print "swap|" st "|" su "|" sf "|" sp
}' /proc/meminfo

echo "[LOAD]"
read -r l1 l5 l15 procs _ < /proc/loadavg
echo "${l1}|${l5}|${l15}|$(nproc)|${procs}"

echo "[SERVICES]"
f=$(systemctl --failed --no-legend --no-pager --plain 2>/dev/null)
if [ -z "$f" ]; then
    echo "0|"
else
    echo "$(echo "$f" | wc -l)|$(echo "$f" | awk '{print $1}' | tr '\n' ',' | sed 's/,$//')"
fi
SCRIPT
chmod +x /tmp/collect.sh

# 2. Script de presentación (lee datos, aplica formato)
cat > /tmp/present.sh << 'SCRIPT'
#!/bin/bash
set -uo pipefail

human_kb() {
    awk -v k="$1" 'BEGIN {
        if (k>=1048576) printf "%.1fG", k/1048576
        else if (k>=1024) printf "%.0fM", k/1024
        else printf "%dK", k
    }'
}

SECTION=""
while IFS= read -r line; do
    if [[ "$line" =~ ^\[([A-Z_]+)\]$ ]]; then
        SECTION="${BASH_REMATCH[1]}"
        case "$SECTION" in
            SYSTEM)   echo "=== System ===" ;;
            DISK)     echo ""; echo "=== Disk ===" ;;
            MEMORY)   echo ""; echo "=== Memory ===" ;;
            LOAD)     echo ""; echo "=== Load ===" ;;
            SERVICES) echo ""; echo "=== Services ===" ;;
        esac
        continue
    fi

    case "$SECTION" in
        SYSTEM)
            IFS='|' read -r key val <<< "$line"
            case "$key" in
                hostname)   echo "Host:   $val" ;;
                kernel)     echo "Kernel: $val" ;;
                uptime_sec)
                    local d=$(( val / 86400 )) h=$(( (val%86400)/3600 ))
                    echo "Uptime: ${d}d ${h}h" ;;
            esac
            ;;
        DISK)
            IFS='|' read -r mount size used avail pct <<< "$line"
            printf "  %-15s %6s / %6s  (%s%%)\n" \
                "$mount" "$(human_kb "$used")" "$(human_kb "$size")" "$pct"
            ;;
        MEMORY)
            IFS='|' read -r type total used free_avail pct <<< "$line"
            label="RAM"
            [ "$type" = "swap" ] && label="Swap"
            printf "  %-5s %6s / %6s  (%s%%)\n" \
                "$label:" "$(human_kb "$used")" "$(human_kb "$total")" "$pct"
            ;;
        LOAD)
            IFS='|' read -r l1 l5 l15 cpus procs <<< "$line"
            echo "  Load: $l1  $l5  $l15  (CPUs: $cpus)"
            echo "  Procs: $procs"
            ;;
        SERVICES)
            IFS='|' read -r count names <<< "$line"
            [[ "$line" == detail* ]] && continue
            if [ "$count" -eq 0 ]; then
                echo "  All services OK"
            else
                echo "  FAILED ($count): $names"
            fi
            ;;
    esac
done < <(/tmp/collect.sh)
SCRIPT
chmod +x /tmp/present.sh

# 3. Ejecutar
echo "--- Raw data ---"
/tmp/collect.sh
echo ""
echo "--- Formatted ---"
/tmp/present.sh

# 4. Limpiar
rm /tmp/collect.sh /tmp/present.sh
```

**Pregunta de reflexión**: ¿Cuál es la ventaja de que `collect.sh` escriba a stdout y `present.sh` lea de stdin? ¿Cómo facilita esto crear la exportación JSON en T03?

> Al separar recolección y presentación con una interfaz de texto (pipe-separated a stdout), puedes conectar **cualquier formateador** a la misma fuente de datos: `./collect.sh | ./present.sh` para terminal, `./collect.sh | ./to_json.sh` para JSON, `./collect.sh | ./to_html.sh` para HTML. La lógica de recolección se escribe una sola vez, y cada formato de salida es un consumidor independiente. Es el principio Unix de "programas que hacen una cosa bien y se conectan con pipes".

---

### Ejercicio 3: Recolección robusta con manejo de errores

**Objetivo**: Hacer la recolección resiliente a sistemas donde alguna fuente no está disponible.

```bash
# 1. Crear script con manejo de errores
cat > /tmp/collect_robust.sh << 'SCRIPT'
#!/bin/bash
set -uo pipefail

# Función segura: si el comando falla, devuelve un valor por defecto
safe_read() {
    local file="$1"
    local default="${2:-N/A}"
    if [ -r "$file" ]; then
        cat "$file"
    else
        echo "$default"
    fi
}

echo "[SYSTEM]"
echo "hostname|$(safe_read /proc/sys/kernel/hostname unknown)"

# ¿Existe /proc/meminfo?
echo "[MEMORY]"
if [ -r /proc/meminfo ]; then
    awk '
    /^MemTotal:/     { mt=$2 }
    /^MemAvailable:/ { ma=$2 }
    /^SwapTotal:/    { st=$2 }
    /^SwapFree:/     { sf=$2 }
    END {
        mu=mt-ma; mp=(mt>0)?int(mu*100/mt):0
        su=st-sf; sp=(st>0)?int(su*100/st):0
        print "ram|" mt "|" mu "|" ma "|" mp
        print "swap|" st "|" su "|" sf "|" sp
    }' /proc/meminfo
else
    echo "ram|0|0|0|0"
    echo "swap|0|0|0|0"
fi

# ¿Está systemctl disponible?
echo "[SERVICES]"
if command -v systemctl &>/dev/null; then
    f=$(systemctl --failed --no-legend --no-pager --plain 2>/dev/null || true)
    if [ -z "$f" ]; then
        echo "0|"
    else
        echo "$(echo "$f" | wc -l)|$(echo "$f" | awk '{print $1}' | tr '\n' ',' | sed 's/,$//')"
    fi
else
    echo "0|systemctl_not_available"
fi

echo "[LOAD]"
if [ -r /proc/loadavg ]; then
    read -r l1 l5 l15 procs _ < /proc/loadavg
    cpus=$(nproc 2>/dev/null || echo 1)
    echo "${l1}|${l5}|${l15}|${cpus}|${procs}"
else
    echo "0|0|0|1|0/0"
fi
SCRIPT
chmod +x /tmp/collect_robust.sh

# 2. Ejecutar en el sistema real
/tmp/collect_robust.sh

# 3. Simular un entorno limitado (sin systemctl, por ejemplo)
echo ""
echo "--- Simulated restricted env ---"
PATH=/usr/bin:/bin /tmp/collect_robust.sh

# 4. Limpiar
rm /tmp/collect_robust.sh
```

**Pregunta de reflexión**: El script verifica `command -v systemctl` antes de usarlo. ¿En qué tipo de sistema Linux no estaría disponible `systemctl`? ¿Cómo podrías obtener información equivalente de servicios en ese caso?

> `systemctl` no está disponible en sistemas que no usan systemd como init: contenedores Docker (típicamente no tienen init), distribuciones con SysVinit (Devuan, Slackware, sistemas legacy RHEL 6), sistemas con OpenRC (Alpine Linux, Gentoo por defecto). En SysVinit se usaría `service --status-all` para ver el estado de los servicios. En OpenRC: `rc-status`. En contenedores, generalmente no hay un init system — los procesos se gestionan directamente o con supervisord, y verificarías con `pgrep` o leyendo el PID file de cada servicio.

---

> **Siguiente tema**: T02 — Formateo y reporte: salida coloreada en terminal, umbrales de alerta, resumen ejecutivo.
