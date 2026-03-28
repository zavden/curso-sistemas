# T03 — sar

> **Objetivo:** Dominar `sar` (System Activity Reporter) para consultar estadísticas históricas del sistema y reconstruir eventos pasado.

## Qué es sar

**sar** recopila y reporta estadísticas **históricas** del sistema. A diferencia de vmstat e iostat que muestran datos en vivo, sar permite consultar la actividad de horas, días o semanas anteriores.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    vmstat / iostat → ¿qué pasa AHORA?                     │
│                    sar → ¿qué pasó AYER a las 03:00?                      │
└─────────────────────────────────────────────────────────────────────────────┘
```

```bash
# Parte del paquete sysstat:
sudo apt install sysstat     # Debian/Ubuntu
sudo dnf install sysstat     # RHEL/Fedora
```

---

## Configuración de sysstat

### Habilitar la recopilación

```bash
# Debian/Ubuntu — editar /etc/default/sysstat:
cat /etc/default/sysstat
# ENABLED="false"    ← cambiar a "true"

sudo sed -i 's/ENABLED="false"/ENABLED="true"/' /etc/default/sysstat
sudo systemctl restart sysstat

# RHEL/Fedora — habilitado por defecto:
systemctl is-enabled sysstat
```

### Cómo se recopilan los datos

```bash
# sysstat usa timer (Debian 10+) o cron para recopilar cada 10 minutos:

# Timer:
systemctl cat sysstat-collect.timer 2>/dev/null
# [Timer]
# OnCalendar=*:00/10    ← cada 10 minutos

# Cron (legacy):
cat /etc/cron.d/sysstat 2>/dev/null
# */10 * * * * root /usr/lib/sysstat/sa1 1 1
# 53 23 * * * root /usr/lib/sysstat/sa2 -A

# sa1  → recopila datos cada 10 minutos
# sa2  → genera el resumen diario (a las 23:53)
```

### Dónde se almacenan los datos

```bash
# Archivos binarios (contienen los datos crudos):
ls /var/log/sysstat/    # Debian
ls /var/log/sa/         # RHEL

# sa01, sa02, ..., sa31  — datos binarios del día 01, 02, ..., 31
# sar01, sar02, ...      — resúmenes de texto (generados por sa2)

# Los archivos del mes anterior se sobreescriben
# Retención configurable:
cat /etc/sysstat/sysstat | grep HISTORY
# HISTORY=28  ← días a mantener
```

---

## Uso básico

```bash
# CPU del día actual:
sar
sar -u

# CPU de ayer:
sar -1

# CPU de un día específico:
sar -f /var/log/sysstat/sa15    # día 15 del mes actual

# Rango horario:
sar -s 08:00:00 -e 12:00:00
# Solo de 08:00 a 12:00 (formato HH:MM:SS)

# En tiempo real (como vmstat):
sar 2 10    # cada 2 segundos, 10 muestras
```

---

## Tipos de estadísticas

### CPU (-u)

```bash
sar -u
# 12:00:01 AM     CPU     %user     %nice   %system   %iowait    %steal     %idle
# 12:10:01 AM     all      5.20      0.00      2.10      1.50      0.00     91.20
# 12:20:01 AM     all      8.30      0.00      3.40      0.80      0.00     87.50
# ...
# Average:        all      6.50      0.00      2.80      1.10      0.00     89.60
```

| Columna | Descripción |
|---------|-------------|
| `%user` | Tiempo en código de usuario |
| `%nice` | Tiempo en código de usuario con nice positivo |
| `%system` | Tiempo en código del kernel |
| `%iowait` | Tiempo esperando I/O |
| `%steal` | Tiempo robado por hipervisor (VMs) |
| `%idle` | Tiempo ocioso |

```bash
# Por CPU individual:
sar -u -P ALL    # todas las CPUs
sar -u -P 0      # solo CPU 0
```

### Memoria (-r)

```bash
sar -r
# 12:00:01 AM kbmemfree kbavail  kbmemused  %memused kbbuffers kbcached kbcommit  %commit kbactive kbinact kbdirty
# 12:10:01 AM   2048000 5120000    2048000     25.00   128000  3072000  4096000    50.00  1536000 2048000    1024
```

| Columna | Descripción |
|---------|-------------|
| `kbmemfree` | RAM libre |
| `kbavail` | RAM disponible (incluyendo cache liberables) |
| `kbmemused` | RAM usada |
| `%memused` | Porcentaje de RAM usada |
| `kbbuffers` | Buffers del kernel |
| `kbcached` | Page cache |
| `kbcommit` | Memoria comprometida (RAM + swap) |
| `%commit` | Porcentaje de memoria comprometida |
| `kbdirty` | Páginas modificadas aún no escritas a disco |

### Swap (-S)

```bash
sar -S
# 12:00:01 AM kbswpfree kbswpused  %swpused kbswpcad  %swpcad
# 12:10:01 AM   2048000         0      0.00        0     0.00
```

| Columna | Descripción |
|---------|-------------|
| `kbswpfree` | Swap libre |
| `kbswpused` | Swap usada |
| `%swpused` | Porcentaje de swap usada |
| `kbswpcad` | Swap cacheada (páginas en swap Y en RAM) |
| `%swpcad` | Porcentaje de swap cacheada |

### Actividad de swap (-W)

```bash
sar -W
# 12:00:01 AM  pswpin/s pswpout/s
# 12:10:01 AM      0.00      0.00
```

| Columna | Descripción |
|---------|-------------|
| `pswpin/s` | Páginas por segundo leídas del swap |
| `pswpout/s` | Páginas por segundo escritas al swap |

### I/O de disco (-b)

```bash
sar -b
# 12:00:01 AM       tps      rtps      wtps      dtps   bread/s   bwrtn/s   bdscd/s
# 12:10:01 AM     25.00     15.00     10.00      0.00    300.00    160.00      0.00
```

| Columna | Descripción |
|---------|-------------|
| `tps` | IOPS totales |
| `rtps` | Lecturas por segundo |
| `wtps` | Escrituras por segundo |
| `dtps` | Descartes por segundo (SSD TRIM) |
| `bread/s` | Bloques leídos por segundo |
| `bwrtn/s` | Bloques escritos por segundo |

### I/O por dispositivo (-d)

```bash
sar -d
# 12:00:01 AM   DEV       tps     rkB/s     wkB/s     dkB/s  areq-sz  aqu-sz  await  %util
# 12:10:01 AM   sda     25.00    150.00     80.00      0.00     9.20    0.03   1.20   2.50
```

| Columna | Descripción |
|---------|-------------|
| `DEV` | Dispositivo |
| `tps` | IOPS totales del dispositivo |
| `rkB/s` | KB leídos por segundo |
| `wkB/s` | KB escritos por segundo |
| `areq-sz` | Tamaño promedio del request |
| `aqu-sz` | Tamaño promedio de cola |
| `await` | Tiempo de espera promedio (ms) |
| `%util` | Utilización del dispositivo |

### Red — Interfaces (-n DEV)

```bash
sar -n DEV
# 12:00:01 AM   IFACE   rxpck/s   txpck/s   rxkB/s    txkB/s   rxcmp/s  txcmp/s  rxmcst/s  %ifutil
# 12:10:01 AM    eth0    500.00    300.00    150.00     80.00      0.00     0.00      0.00     1.20
```

| Columna | Descripción |
|---------|-------------|
| `rxpck/s` | Paquetes recibidos por segundo |
| `txpck/s` | Paquetes transmitidos por segundo |
| `rxkB/s` | KB recibidos por segundo |
| `txkB/s` | KB transmitidos por segundo |
| `rxcmp/s` | Paquetes comprimidos recibidos |
| `txcmp/s` | Paquetes comprimidos transmitidos |
| `rxmcst/s` | Multicasts recibidos |
| `%ifutil` | Utilización de la interfaz |

### Red — Errores (-n EDEV)

```bash
sar -n EDEV
# rxerr/s  — errores de recepción por segundo
# txerr/s  — errores de transmisión por segundo
# rxdrop/s — paquetes descartados en recepción
# txdrop/s — paquetes descartados en transmisión
# rxmcst/s — multicast recibidos
```

```bash
# Si rxdrop/s > 0:
# El kernel descarta paquetes
# Causas: buffers llenos, CPU saturada, problemas de red
```

### Red — Sockets (-n SOCK)

```bash
sar -n SOCK
# totsck — total de sockets en uso
# tcpsck — sockets TCP
# udpsck — sockets UDP
# rawsck — sockets raw
# ip-frag — fragmentos IP en cola
```

```bash
# Útil para detectar:
# - Connection leaks: totsck creciendo indefinidamente
# - Ataques SYN flood: tcpsck muy alto
```

### Load average (-q)

```bash
sar -q
# 12:00:01 AM   runq-sz  plist-sz   ldavg-1   ldavg-5  ldavg-15   blocked
# 12:10:01 AM         2       345      1.50      1.20      0.90         0
```

| Columna | Descripción |
|---------|-------------|
| `runq-sz` | Tamaño de la run queue (= r en vmstat) |
| `plist-sz` | Número de procesos/threads |
| `ldavg-1/5/15` | Load average de 1, 5, 15 minutos |
| `blocked` | Procesos bloqueados en I/O (= b en vmstat) |

### Context switches (-w)

```bash
sar -w
# 12:00:01 AM    proc/s   cswch/s
# 12:10:01 AM      2.00   5000.00
```

| Columna | Descripción |
|---------|-------------|
| `proc/s` | Procesos creados por segundo (fork) |
| `cswch/s` | Context switches por segundo |

---

## Análisis histórico

### Encontrar picos

```bash
# CPU más alta de ayer:
sar -u -1 | sort -k4 -rn | head -5
# Ordena por %user descendente

# Mayor I/O wait:
sar -u -1 | sort -k6 -rn | head -5
# Ordena por %iowait descendente

# Mayor uso de swap:
sar -W -1 | grep -v Average | sort -k3 -rn | head -5
```

### Correlacionar eventos

```bash
# "El servidor estuvo lento ayer entre las 14:00 y 15:00"

# CPU:
sar -u -s 14:00:00 -e 15:00:00 -1

# Memoria:
sar -r -s 14:00:00 -e 15:00:00 -1

# Disco:
sar -d -s 14:00:00 -e 15:00:00 -1

# Red:
sar -n DEV -s 14:00:00 -e 15:00:00 -1

# Todo junto:
sar -A -s 14:00:00 -e 15:00:00 > /tmp/incident-report.txt
```

### Formatos de salida

```bash
# Texto (default):
sar -u

# JSON (via sadf):
sadf -j /var/log/sysstat/sa17 -- -u | jq '.'

# CSV:
sadf -d /var/log/sysstat/sa17 -- -u

# SVG (gráfico):
sadf -g /var/log/sysstat/sa17 -- -u > /tmp/cpu.svg
# Abrir en navegador para ver gráfico histórico
```

---

## sadf — El companion de sar

```bash
# sadf convierte archivos binarios de sar a múltiples formatos:
sadf -j    # JSON
sadf -d    # CSV
sadf -g    # SVG (gráfico)
sadf -x    # XML

# Ejemplo — extraer CPU del día 15:
sadf -j /var/log/sysstat/sa15 -- -u | jq '.sysstat.hosti.data[] | {time: .timestamp, idle: .cpu[0].idle}'

# Todos los tipos de datos disponibles:
sadf -j /var/log/sysstat/sa15 -- -A | jq '.sysstat.hosti.data[0] | keys'
```

---

## Quick reference

```
DATOS HISTÓRICOS:
  sar                → CPU del día actual
  sar -1            → CPU de ayer
  sar -f sa15       → día 15 del mes

RANGO HORARIO:
  sar -s 08:00 -e 12:00

TIPOS DE ESTADÍSTICAS:
  -u    CPU
  -r    Memoria
  -S    Swap
  -W    Actividad de swap
  -b    I/O disco
  -d    I/O por dispositivo
  -n DEV   Interfaces de red
  -n EDEV  Errores de red
  -n SOCK  Sockets
  -q    Load average
  -w    Context switches
  -A    Todo

FORMATOS:
  sadf -j  → JSON
  sadf -d  → CSV
  sadf -g  → SVG (gráfico)

EN VIVO:
  sar 2 5  → cada 2s, 5 muestras
```

---

## Ejercicios

### Ejercicio 1 — Verificar sysstat

```bash
# 1. ¿Está sysstat activo?
systemctl is-active sysstat 2>/dev/null

# 2. ¿Hay datos recopilados?
ls -la /var/log/sysstat/ 2>/dev/null || ls -la /var/log/sa/ 2>/dev/null

# 3. Ver CPU de hoy:
sar -u 2>/dev/null | head -10 || echo "Sin datos"

# 4. Ver versión:
sar -V
```

### Ejercicio 2 — Estadísticas en vivo

```bash
# Capturar 10 muestras de CPU, memoria y disco:
sar -u -r -b 2 10
```

### Ejercicio 3 — Análisis histórico

```bash
# 1. ¿Cuándo fue el pico de CPU ayer?
sar -u -1 2>/dev/null | sort -k4 -rn | head -5

# 2. ¿Cuándo hubo más I/O wait?
sar -u -1 2>/dev/null | sort -k6 -rn | head -5

# 3. ¿Se usó swap?
sar -S -1 2>/dev/null | grep -v "kbswpfree" | sort -k3 -rn | head -5
```

### Ejercicio 4 — Correlacionar un incidente

```bash
# Escenario: "El servidor estuvo lento ayer a las 14:30"

# 1. CPU:
echo "=== CPU 14:00-15:00 ==="
sar -u -s 14:00:00 -e 15:00:00 -1 2>/dev/null

# 2. Memoria:
echo "=== Memoria ==="
sar -r -s 14:00:00 -e 15:00:00 -1 2>/dev/null

# 3. I/O:
echo "=== Disco ==="
sar -d -s 14:00:00 -e 15:00:00 -1 2>/dev/null

# 4. Load:
echo "=== Load ==="
sar -q -s 14:00:00 -e 15:00:00 -1 2>/dev/null
```

### Ejercicio 5 — Generar gráficos

```bash
# 1. Generar SVG de CPU del día 15:
sadf -g /var/log/sysstat/sa15 -- -u > /tmp/cpu-day15.svg

# 2. Generar SVG de memoria:
sadf -g /var/log/sysstat/sa15 -- -r > /tmp/mem-day15.svg

# 3. Transferir a tu máquina local y abrir en navegador:
# scp usuario@servidor:/tmp/*.svg .

# 4. O generar un reporte completo en JSON:
sadf -j /var/log/sysstat/sa15 -- -A > /tmp/full-report-day15.json
```

### Ejercicio 6 — Detectar anomalías

```bash
# Script para detectar anomalías en sar:

#!/bin/bash
DATE="${1:-$(date -d yesterday +%d)}"
SAFILE="/var/log/sysstat/sa${DATE}"

echo "=== Análisis de anomalías - Día $DATE ==="

# CPU alta:
echo "--- Top 5 picos de CPU ---"
sar -u -f "$SAFILE" 2>/dev/null | sort -k4 -rn | head -6

# I/O wait alto:
echo "--- Top 5 picos de I/O wait ---"
sar -u -f "$SAFILE" 2>/dev/null | sort -k6 -rn | head -6

# Swap usado:
echo "--- Swap utilizado ---"
sar -S -f "$SAFILE" 2>/dev/null | grep -v "kbswpfree" | sort -k3 -rn | head -5

# Sockets (posible ataque):
echo "--- Sockets TCP ---"
sar -n SOCK -f "$SAFILE" 2>/dev/null | sort -k3 -rn | head -5
```
