# T03 — sar

## Qué es sar

sar (System Activity Reporter) recopila y reporta **estadísticas
históricas** del sistema. A diferencia de vmstat e iostat que muestran
datos en vivo, sar almacena datos periódicamente y permite consultar
la actividad de horas, días o semanas anteriores:

```bash
# vmstat / iostat → "¿qué pasa AHORA?"
# sar             → "¿qué pasó AYER a las 03:00?"

# sar es parte del paquete sysstat:
sudo apt install sysstat     # Debian/Ubuntu
sudo dnf install sysstat     # RHEL/Fedora
```

## Configuración de sysstat

### Habilitar la recopilación

```bash
# Debian/Ubuntu — habilitar en /etc/default/sysstat:
cat /etc/default/sysstat
# ENABLED="false"    ← cambiar a "true"

sudo sed -i 's/ENABLED="false"/ENABLED="true"/' /etc/default/sysstat
sudo systemctl restart sysstat

# RHEL/Fedora — habilitado por defecto:
systemctl is-enabled sysstat
# enabled
```

### Cómo recopila los datos

```bash
# sysstat usa un timer o cron para recopilar datos cada 10 minutos:

# Verificar el timer:
systemctl cat sysstat-collect.timer 2>/dev/null
# [Timer]
# OnCalendar=*:00/10    ← cada 10 minutos

# O el cron job:
cat /etc/cron.d/sysstat 2>/dev/null
# */10 * * * * root /usr/lib/sysstat/sa1 1 1
# 53 23 * * * root /usr/lib/sysstat/sa2 -A

# sa1: recopila datos (cada 10 minutos)
# sa2: genera el resumen diario (a las 23:53)
```

### Dónde se almacenan los datos

```bash
# Los datos se guardan en archivos binarios:
ls /var/log/sysstat/    # Debian
ls /var/log/sa/         # RHEL

# Archivos:
# sa01, sa02, ..., sa31  — datos binarios del día 01, 02, ..., 31
# sar01, sar02, ...      — resúmenes de texto (generados por sa2)

# Los archivos del mes anterior se sobreescriben
# (sa01 de marzo reemplaza sa01 de febrero)

# Retención configurable en:
cat /etc/sysstat/sysstat    # Debian
cat /etc/sysstat/sysstat    # RHEL
# HISTORY=28  ← días a mantener
```

## Uso básico

```bash
# CPU del día actual:
sar
# Muestra estadísticas de CPU de hoy, cada 10 minutos

# CPU de ayer:
sar -1
# O:
sar -f /var/log/sysstat/sa$(date -d yesterday +%d)

# CPU de un día específico:
sar -f /var/log/sysstat/sa15    # día 15 del mes actual

# Rango horario:
sar -s 08:00:00 -e 12:00:00
# Solo de 08:00 a 12:00

# En tiempo real (como vmstat):
sar 2 10    # cada 2 segundos, 10 muestras
```

## Tipos de estadísticas

### CPU (-u)

```bash
sar -u
# 12:00:01 AM     CPU     %user     %nice   %system   %iowait    %steal     %idle
# 12:10:01 AM     all      5.20      0.00      2.10      1.50      0.00     91.20
# 12:20:01 AM     all      8.30      0.00      3.40      0.80      0.00     87.50
# ...
# Average:        all      6.50      0.00      2.80      1.10      0.00     89.60

# Por CPU individual:
sar -u -P ALL
# Muestra cada CPU (0, 1, 2, 3, ...)

sar -u -P 0    # solo CPU 0
```

### Memoria (-r)

```bash
sar -r
# 12:00:01 AM kbmemfree kbavail  kbmemused  %memused kbbuffers kbcached kbcommit  %commit kbactive kbinact kbdirty
# 12:10:01 AM   2048000 5120000    2048000     25.00   128000  3072000  4096000    50.00  1536000 2048000    1024
# ...

# Columnas clave:
# kbmemfree  — RAM libre
# kbavail    — RAM disponible (incluyendo cache liberables)
# %memused   — porcentaje de RAM usada
# kbcached   — cache de páginas
# %commit    — memoria comprometida vs total (RAM + swap)
# kbdirty    — páginas modificadas aún no escritas a disco
```

### Swap (-S)

```bash
sar -S
# 12:00:01 AM kbswpfree kbswpused  %swpused kbswpcad  %swpcad
# 12:10:01 AM   2048000         0      0.00        0     0.00
# ...

# kbswpfree — swap libre
# kbswpused — swap usada
# %swpused  — porcentaje de swap usada
# kbswpcad  — swap cacheada (páginas en swap Y en RAM)
```

### Actividad de swap (-W)

```bash
sar -W
# 12:00:01 AM  pswpin/s pswpout/s
# 12:10:01 AM      0.00      0.00
# ...

# pswpin/s  — páginas por segundo leídas del swap (swap in)
# pswpout/s — páginas por segundo escritas al swap (swap out)
# Equivale a si/so de vmstat pero por página (no por KB)
```

### I/O de disco (-b)

```bash
sar -b
# 12:00:01 AM       tps      rtps      wtps      dtps   bread/s   bwrtn/s   bdscd/s
# 12:10:01 AM     25.00     15.00     10.00      0.00    300.00    160.00      0.00
# ...

# tps    — transferencias por segundo (IOPS total)
# rtps   — lecturas por segundo
# wtps   — escrituras por segundo
# bread/s — bloques leídos por segundo
# bwrtn/s — bloques escritos por segundo
```

### I/O por dispositivo (-d)

```bash
sar -d
# 12:00:01 AM   DEV       tps     rkB/s     wkB/s     dkB/s  areq-sz  aqu-sz  await  %util
# 12:10:01 AM   sda     25.00    150.00     80.00      0.00     9.20    0.03   1.20   2.50
# ...

# Similar a iostat -x pero con datos históricos
# Mismas columnas: tps, rkB/s, wkB/s, await, %util
```

### Red — Interfaces (-n DEV)

```bash
sar -n DEV
# 12:00:01 AM   IFACE   rxpck/s   txpck/s   rxkB/s    txkB/s   rxcmp/s  txcmp/s  rxmcst/s  %ifutil
# 12:10:01 AM    eth0    500.00    300.00    150.00     80.00      0.00     0.00      0.00     1.20
# 12:10:01 AM      lo     10.00     10.00      1.00      1.00      0.00     0.00      0.00     0.00
# ...

# rxpck/s  — paquetes recibidos por segundo
# txpck/s  — paquetes transmitidos por segundo
# rxkB/s   — KB recibidos por segundo
# txkB/s   — KB transmitidos por segundo
# %ifutil  — utilización de la interfaz
```

### Red — Errores (-n EDEV)

```bash
sar -n EDEV
# rxerr/s  — errores de recepción
# txerr/s  — errores de transmisión
# rxdrop/s — paquetes descartados (recepción)
# txdrop/s — paquetes descartados (transmisión)

# Si rxdrop/s > 0 → el kernel descarta paquetes
# Posibles causas: buffers de red llenos, CPU saturada
```

### Red — Sockets (-n SOCK)

```bash
sar -n SOCK
# totsck — total de sockets en uso
# tcpsck — sockets TCP
# udpsck — sockets UDP
# rawsck — sockets raw
# ip-frag — fragmentos IP en cola

# Útil para detectar:
# - Connection leaks (totsck creciendo indefinidamente)
# - Ataques SYN flood (tcpsck muy alto)
```

### Load average (-q)

```bash
sar -q
# 12:00:01 AM   runq-sz  plist-sz   ldavg-1   ldavg-5  ldavg-15   blocked
# 12:10:01 AM         2       345      1.50      1.20      0.90         0
# ...

# runq-sz   — tamaño de la run queue (= r de vmstat)
# plist-sz  — número total de procesos/threads
# ldavg-1/5/15 — load average de 1, 5, 15 minutos
# blocked   — procesos bloqueados en I/O (= b de vmstat)
```

### Context switches (-w)

```bash
sar -w
# 12:00:01 AM    proc/s   cswch/s
# 12:10:01 AM      2.00   5000.00
# ...

# proc/s  — procesos creados por segundo (fork)
# cswch/s — context switches por segundo
```

## Análisis histórico

### Encontrar picos

```bash
# ¿Cuándo tuvo la CPU más carga ayer?
sar -u -1 | sort -k4 -rn | head -5
# Ordena por %user descendente

# ¿Cuándo hubo más I/O wait?
sar -u -1 | sort -k6 -rn | head -5
# Ordena por %iowait descendente

# ¿Cuándo se usó más swap?
sar -W -1 | grep -v Average | sort -k3 -rn | head -5
```

### Correlacionar eventos

```bash
# "El servidor estuvo lento ayer entre las 14:00 y 15:00"

# CPU en ese rango:
sar -u -s 14:00:00 -e 15:00:00 -1

# Memoria:
sar -r -s 14:00:00 -e 15:00:00 -1

# Disco:
sar -d -s 14:00:00 -e 15:00:00 -1

# Red:
sar -n DEV -s 14:00:00 -e 15:00:00 -1

# Todo junto:
sar -A -s 14:00:00 -e 15:00:00 -1 > /tmp/incident-report.txt
```

### Formato de salida

```bash
# Texto (default):
sar -u

# JSON:
sadf -j /var/log/sysstat/sa17 -- -u | jq '.'
# sadf convierte los datos binarios de sar a otros formatos

# CSV:
sadf -d /var/log/sysstat/sa17 -- -u
# Separado por punto y coma

# SVG (gráfico):
sadf -g /var/log/sysstat/sa17 -- -u > /tmp/cpu.svg
# Genera un gráfico SVG visualizable en un navegador
```

---

## Ejercicios

### Ejercicio 1 — Verificar sysstat

```bash
# 1. ¿Está sysstat habilitado?
systemctl is-active sysstat 2>/dev/null || echo "sysstat no activo"

# 2. ¿Hay datos recopilados?
ls /var/log/sysstat/ 2>/dev/null || ls /var/log/sa/ 2>/dev/null || echo "Sin datos"

# 3. Si hay datos, ver la CPU de hoy:
sar -u 2>/dev/null | head -10 || echo "Sin datos de sar"
```

### Ejercicio 2 — Estadísticas en vivo

```bash
# Capturar 5 muestras de CPU, memoria y disco:
sar -u -r -b 2 5
```

### Ejercicio 3 — Análisis histórico

```bash
# Si hay datos de ayer:
sar -u -1 2>/dev/null | tail -5 || echo "Sin datos de ayer"
# ¿Cuál fue el promedio de CPU?
# ¿Hubo algún pico de I/O wait?
```
