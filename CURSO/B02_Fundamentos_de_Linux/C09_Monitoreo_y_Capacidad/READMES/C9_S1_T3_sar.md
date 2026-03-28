# T03 — sar

> **Objetivo:** Dominar `sar` (System Activity Reporter) para consultar estadísticas históricas del sistema y reconstruir eventos pasados.

## Erratas detectadas en el material original

| Archivo | Línea | Error | Corrección |
|---------|-------|-------|------------|
| `README.max.md` | 331 | `sar -A -s 14:00:00 -e 15:00:00 > /tmp/incident-report.txt` — falta `-1` (el contexto es "ayer", y las 4 líneas anteriores del mismo bloque sí incluyen `-1`) | `sar -A -s 14:00:00 -e 15:00:00 -1 > /tmp/incident-report.txt` |

---

## Qué es sar

**sar** recopila y reporta estadísticas **históricas** del sistema. A diferencia de vmstat e iostat que muestran datos en vivo, sar permite consultar la actividad de horas, días o semanas anteriores.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    vmstat / iostat → ¿qué pasa AHORA?                     │
│                    sar → ¿qué pasó AYER a las 03:00?                      │
└─────────────────────────────────────────────────────────────────────────────┘
```

```bash
# Parte del paquete sysstat (igual que iostat):
sudo apt install sysstat     # Debian/Ubuntu
sudo dnf install sysstat     # RHEL/Fedora
```

---

## Configuración de sysstat

### Habilitar la recopilación

```bash
# Debian/Ubuntu — DESHABILITADO por defecto:
cat /etc/default/sysstat
# ENABLED="false"    ← cambiar a "true"

sudo sed -i 's/ENABLED="false"/ENABLED="true"/' /etc/default/sysstat
sudo systemctl restart sysstat

# RHEL/Fedora — HABILITADO por defecto:
systemctl is-enabled sysstat
# enabled
```

### Cómo se recopilan los datos

```bash
# sysstat recopila datos cada 10 minutos (por defecto):

# Via timer de systemd (Debian 10+, RHEL 7+):
systemctl cat sysstat-collect.timer
# [Timer]
# OnCalendar=*:00/10    ← cada 10 minutos

# Via cron (legacy):
cat /etc/cron.d/sysstat
# */10 * * * * root /usr/lib/sysstat/sa1 1 1
# 53 23 * * * root /usr/lib/sysstat/sa2 -A

# sa1 → recopila datos (cada 10 minutos)
# sa2 → genera el resumen diario de texto (a las 23:53)
```

### Dónde se almacenan los datos

```bash
# Archivos binarios:
ls /var/log/sysstat/    # Debian
ls /var/log/sa/         # RHEL

# sa01, sa02, ..., sa31  — datos binarios del día 01, 02, ..., 31
# sar01, sar02, ...      — resúmenes de texto (generados por sa2)

# Los archivos del mes anterior se sobreescriben
# (sa01 de marzo reemplaza sa01 de febrero)

# Retención configurable:
cat /etc/sysstat/sysstat | grep HISTORY
# HISTORY=28  ← días a mantener (default)
```

---

## Uso básico

```bash
# CPU del día actual:
sar
sar -u         # equivalente

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

| Columna | Descripción | Equivalente en vmstat |
|---------|-------------|----------------------|
| `%user` | Tiempo en código de usuario | `us` |
| `%nice` | Tiempo en procesos con nice positivo | (incluido en `us`) |
| `%system` | Tiempo en código del kernel | `sy` |
| `%iowait` | Tiempo esperando I/O | `wa` |
| `%steal` | Tiempo robado por hipervisor | `st` |
| `%idle` | Tiempo ocioso | `id` |

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
| `kbavail` | RAM disponible (incluyendo cache liberables) — **la que importa** |
| `kbmemused` | RAM usada |
| `%memused` | Porcentaje de RAM usada |
| `kbbuffers` | Buffers del kernel |
| `kbcached` | Page cache |
| `kbcommit` | Memoria comprometida (RAM + swap reservada) |
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

| Columna | Descripción | Equivalente en vmstat |
|---------|-------------|----------------------|
| `pswpin/s` | Páginas/s leídas del swap (swap in) | `si` (pero vmstat usa KB/s) |
| `pswpout/s` | Páginas/s escritas al swap (swap out) | `so` (pero vmstat usa KB/s) |

> **Nota:** sar reporta en **páginas/s**, vmstat en **KB/s**. Una página = 4 KB en la mayoría de sistemas x86_64.

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

| Columna | Descripción | Equivalente en iostat |
|---------|-------------|----------------------|
| `tps` | IOPS totales | `tps` |
| `rkB/s` | KB leídos por segundo | `rkB/s` |
| `wkB/s` | KB escritos por segundo | `wkB/s` |
| `areq-sz` | Tamaño promedio del request | `rareq-sz` / `wareq-sz` |
| `aqu-sz` | Tamaño promedio de cola | `aqu-sz` |
| `await` | Tiempo de espera promedio (ms) | `r_await` / `w_await` |
| `%util` | Utilización del dispositivo | `%util` |

> **Nota:** `sar -d` es como `iostat -x` pero con capacidad de consulta histórica. iostat separa lectura/escritura en await (`r_await`, `w_await`), sar combina en un solo `await`.

### Red — Interfaces (-n DEV)

```bash
sar -n DEV
# 12:00:01 AM   IFACE   rxpck/s   txpck/s   rxkB/s    txkB/s   rxcmp/s  txcmp/s  rxmcst/s  %ifutil
# 12:10:01 AM    eth0    500.00    300.00    150.00     80.00      0.00     0.00      0.00     1.20
# 12:10:01 AM      lo     10.00     10.00      1.00      1.00      0.00     0.00      0.00     0.00
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

# Si rxdrop/s > 0:
# El kernel descarta paquetes
# Causas: buffers de red llenos, CPU saturada, problemas de red
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
# - Connection leaks: totsck creciendo indefinidamente
# - Ataques SYN flood: tcpsck muy alto
```

### Load average (-q)

```bash
sar -q
# 12:00:01 AM   runq-sz  plist-sz   ldavg-1   ldavg-5  ldavg-15   blocked
# 12:10:01 AM         2       345      1.50      1.20      0.90         0
```

| Columna | Descripción | Equivalente en vmstat |
|---------|-------------|----------------------|
| `runq-sz` | Tamaño de la run queue | `r` |
| `plist-sz` | Número de procesos/threads | — |
| `ldavg-1/5/15` | Load average de 1, 5, 15 minutos | — |
| `blocked` | Procesos bloqueados en I/O | `b` |

### Context switches (-w)

```bash
sar -w
# 12:00:01 AM    proc/s   cswch/s
# 12:10:01 AM      2.00   5000.00
```

| Columna | Descripción | Equivalente en vmstat |
|---------|-------------|----------------------|
| `proc/s` | Procesos creados por segundo (fork) | — |
| `cswch/s` | Context switches por segundo | `cs` |

---

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

### Correlacionar un incidente

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
sar -A -s 14:00:00 -e 15:00:00 -1 > /tmp/incident-report.txt
```

### sadf — El companion de sar

```bash
# sadf convierte archivos binarios de sar a múltiples formatos:

# JSON:
sadf -j /var/log/sysstat/sa17 -- -u | jq '.'

# CSV (separado por punto y coma):
sadf -d /var/log/sysstat/sa17 -- -u

# SVG (gráfico — abrir en navegador):
sadf -g /var/log/sysstat/sa17 -- -u > /tmp/cpu.svg

# XML:
sadf -x /var/log/sysstat/sa17 -- -u
```

---

## Debian vs RHEL

| Aspecto | Debian/Ubuntu | RHEL/Fedora |
|---------|---------------|-------------|
| Habilitado por defecto | No (`ENABLED="false"`) | Sí |
| Archivo de config | `/etc/default/sysstat` | — (systemd unit) |
| Directorio de datos | `/var/log/sysstat/` | `/var/log/sa/` |
| Recopilación | Timer o cron | Timer (systemd) |
| Config de retención | `/etc/sysstat/sysstat` | `/etc/sysstat/sysstat` |

---

## Quick reference

```
DATOS HISTÓRICOS:
  sar                → CPU del día actual
  sar -1             → CPU de ayer
  sar -f sa15        → CPU del día 15 del mes

RANGO HORARIO:
  sar -s 08:00:00 -e 12:00:00

TIPOS DE ESTADÍSTICAS:
  -u       CPU
  -r       Memoria
  -S       Swap (uso)
  -W       Swap (actividad — pswpin/pswpout)
  -b       I/O disco (global)
  -d       I/O disco (por dispositivo — como iostat -x)
  -n DEV   Interfaces de red
  -n EDEV  Errores de red
  -n SOCK  Sockets
  -q       Load average y run queue
  -w       Context switches
  -A       TODO

MODIFICADORES:
  -s HH:MM:SS   desde hora
  -e HH:MM:SS   hasta hora
  -1             datos de ayer
  -f archivo     leer archivo específico
  -P ALL         por CPU individual
  -P 0           solo CPU 0

FORMATOS (sadf):
  sadf -j  → JSON
  sadf -d  → CSV
  sadf -g  → SVG (gráfico)
  sadf -x  → XML

EN VIVO:
  sar 2 5  → cada 2s, 5 muestras (como vmstat/iostat)

RECOPILACIÓN:
  sa1 → recopila datos (cada 10 min)
  sa2 → genera resumen diario (23:53)
  Datos en: /var/log/sysstat/ (Debian) o /var/log/sa/ (RHEL)
```

---

## Ejercicios

### Ejercicio 1 — Verificar sysstat

```bash
# 1. ¿Está sysstat instalado y activo?
sar -V
systemctl is-active sysstat

# 2. ¿Está habilitada la recopilación?
# (Debian):
cat /etc/default/sysstat

# Predicción: En una instalación fresca de Debian, ¿qué valor
# tendrá ENABLED? ¿Recopilará datos automáticamente?
```

<details><summary>Predicción</summary>

En Debian, `ENABLED="false"` por defecto. **No** recopilará datos automáticamente hasta que cambies a `ENABLED="true"` y reinicies el servicio.

En RHEL/Fedora, sysstat está habilitado por defecto — la recopilación comienza inmediatamente después de instalar el paquete.

</details>

### Ejercicio 2 — Recopilación y almacenamiento

```bash
# 1. ¿Cómo se invocan sa1 y sa2?
systemctl cat sysstat-collect.timer 2>/dev/null
cat /etc/cron.d/sysstat 2>/dev/null

# 2. ¿Hay datos recopilados?
ls -la /var/log/sysstat/ 2>/dev/null || ls -la /var/log/sa/ 2>/dev/null

# 3. ¿Cuántos días de retención hay configurados?
grep HISTORY /etc/sysstat/sysstat

# Predicción: Si hoy es 25 de marzo y HISTORY=28,
# ¿el archivo sa25 contiene datos de marzo o de febrero?
```

<details><summary>Predicción</summary>

El archivo `sa25` contiene datos de **marzo** (hoy). Los archivos se nombran solo con el día del mes (`sa01`-`sa31`), sin indicar el mes. Cada mes, `sa25` de febrero se sobreescribe con datos de marzo.

Con `HISTORY=28`, los datos se mantienen 28 días. Si el sistema lleva corriendo más de un mes, los archivos del mes anterior ya han sido sobreescritos. Si `HISTORY` fuera mayor (ej. 90), sysstat usa subdirectorios por año/mes para evitar la sobreescritura.

</details>

### Ejercicio 3 — Estadísticas de CPU en vivo

```bash
# 1. Captura 5 muestras de CPU:
sar -u 1 5

# 2. Captura por CPU individual:
sar -u -P ALL 1 3

# Predicción: ¿Qué columna de sar -u corresponde a "wa" de vmstat?
# ¿Qué columna tiene sar que vmstat NO tiene?
```

<details><summary>Predicción</summary>

- `%iowait` en sar corresponde a `wa` en vmstat. Miden lo mismo: porcentaje de CPU esperando I/O.
- `%nice` es la columna que sar tiene y vmstat **no**. En vmstat, el tiempo de procesos con `nice` se incluye dentro de `us` (user). sar lo separa en `%nice` para mayor detalle.

Correspondencias completas:

| sar | vmstat |
|-----|--------|
| `%user` | `us` (incluye nice) |
| `%nice` | (incluido en `us`) |
| `%system` | `sy` |
| `%iowait` | `wa` |
| `%steal` | `st` |
| `%idle` | `id` |

</details>

### Ejercicio 4 — Memoria y swap

```bash
# 1. Captura memoria:
sar -r 1 3

# 2. Captura uso de swap:
sar -S 1 3

# 3. Captura actividad de swap:
sar -W 1 3

# Predicción: ¿Cuál es la diferencia entre -S y -W?
# Si %swpused es 20% pero pswpin/s y pswpout/s son 0,
# ¿hay problema de swap actualmente?
```

<details><summary>Predicción</summary>

- `-S` muestra el **estado** del swap: cuánto hay usado, cuánto libre. Es una foto estática.
- `-W` muestra la **actividad** del swap: cuántas páginas se mueven entre RAM y swap por segundo. Es el flujo.

Si `%swpused=20%` pero `pswpin/s=0` y `pswpout/s=0`: **no hay problema actual**. El swap se usó en el pasado (algún momento de presión de memoria), pero ahora no hay movimiento activo. Las páginas siguen en swap pero el sistema no está activamente swappeando.

Es similar a la distinción en vmstat: `swpd > 0` (swap del pasado) vs `si/so > 0` (swap activo = problema).

</details>

### Ejercicio 5 — I/O de disco

```bash
# 1. I/O global:
sar -b 1 3

# 2. I/O por dispositivo:
sar -d 1 3

# Predicción: ¿Qué información tiene sar -d que sar -b NO tiene?
# ¿Qué información tiene iostat -x que sar -d NO tiene?
```

<details><summary>Predicción</summary>

**sar -d tiene y sar -b NO:**
- Desglose por dispositivo (sda, sdb, nvme0n1...)
- `await` (latencia por operación en ms)
- `%util` (utilización del dispositivo)
- `aqu-sz` (tamaño de cola)
- `areq-sz` (tamaño promedio de request)

`sar -b` solo muestra totales globales: IOPS (`tps`, `rtps`, `wtps`) y bloques (`bread/s`, `bwrtn/s`).

**iostat -x tiene y sar -d NO:**
- `r_await` y `w_await` separados (sar combina en un solo `await`)
- `rareq-sz` y `wareq-sz` separados (sar combina en `areq-sz`)
- `rrqm/s`, `wrqm/s`, `%rrqm`, `%wrqm` (estadísticas de merge)
- `f/s`, `f_await` (flush)
- Columnas de descarte (d/s, dkB/s, d_await, dareq-sz)

La ventaja de sar -d sobre iostat -x: **datos históricos**.

</details>

### Ejercicio 6 — Red

```bash
# 1. Interfaces de red:
sar -n DEV 1 3

# 2. Errores de red:
sar -n EDEV 1 3

# 3. Sockets:
sar -n SOCK 1 3

# Predicción: Si ves rxdrop/s > 0 sostenido, ¿qué significa?
# ¿Qué indicaría tcpsck creciendo indefinidamente?
```

<details><summary>Predicción</summary>

**rxdrop/s > 0 sostenido** significa que el kernel está **descartando paquetes entrantes**. El sistema no puede procesarlos a la velocidad que llegan. Causas:
- Buffers de red llenos (ring buffer de la NIC saturado)
- CPU saturada (no hay ciclos para procesar interrupciones de red)
- Ataques de red (flood)
- Interfaz con velocidad insuficiente

Acción: verificar `sar -u` (¿CPU saturada?), verificar `ethtool -S eth0` (estadísticas de la NIC), aumentar buffers (`ethtool -G`).

**tcpsck creciendo indefinidamente** indica un **connection leak**: la aplicación abre conexiones TCP pero no las cierra. Eventualmente se agotan los file descriptors o los puertos efímeros. Es un bug de la aplicación. Verificar con `ss -s` (resumen de sockets) y `ss -tnp` (ver qué proceso tiene las conexiones).

</details>

### Ejercicio 7 — Load average y context switches

```bash
# 1. Load average y run queue:
sar -q 1 5

# 2. Context switches:
sar -w 1 5

# Predicción: Si runq-sz=12, nproc=4, y ldavg-1=10.5,
# ¿el sistema está sobrecargado? ¿Qué relación hay entre
# runq-sz y ldavg?
```

<details><summary>Predicción</summary>

**Sí, el sistema está sobrecargado.** `runq-sz=12` con 4 CPUs significa que hay 3x más procesos esperando CPU que CPUs disponibles. La regla es: `runq-sz > nproc` sostenido = CPU saturada.

`ldavg-1=10.5` confirma: el load average de 1 minuto es 10.5, que con 4 CPUs significa ~2.6x de sobrecarga.

**Relación runq-sz vs ldavg:**
- `runq-sz` es el tamaño **instantáneo** de la run queue en el momento de la muestra. Es como `r` de vmstat.
- `ldavg` es el **promedio exponencial** de la run queue + procesos en I/O wait (bloqueados) calculado por el kernel. Load average incluye procesos en estado D (uninterruptible sleep / I/O wait), runq-sz no.

Por eso `ldavg` puede ser mayor que `runq-sz`: incluye procesos bloqueados en I/O. Y al ser un promedio suavizado, no tiene los picos/valles que sí muestra `runq-sz`.

</details>

### Ejercicio 8 — Análisis histórico y rangos

```bash
# Supón que tienes datos de ayer. Para consultar:

# 1. CPU de ayer completo:
sar -u -1

# 2. CPU de ayer entre 14:00 y 15:00:
sar -u -s 14:00:00 -e 15:00:00 -1

# 3. Top 5 picos de CPU de ayer:
sar -u -1 | sort -k4 -rn | head -5

# 4. Reporte completo de ayer:
sar -A -1 > /tmp/report.txt

# Predicción: ¿Qué diferencia hay entre sar -1 y sar -f /var/log/sysstat/sa$(date -d yesterday +%d)?
```

<details><summary>Predicción</summary>

**Son equivalentes.** `-1` es un atajo que internamente busca el archivo `saDD` del día de ayer. Es decir:

- `sar -1` → busca automáticamente `/var/log/sysstat/sa24` (si hoy es 25)
- `sar -f /var/log/sysstat/sa24` → abre directamente ese archivo

Puedes usar `-2` para anteayer, `-3` para hace 3 días, etc. (hasta que se sobreescriban los archivos). `-f` es más flexible porque permite especificar cualquier archivo, incluso de otro servidor o una copia de respaldo.

</details>

### Ejercicio 9 — Correlacionar un incidente

```bash
# Escenario: "El servidor estuvo lento ayer a las 14:30"
# Investiga CPU, memoria, disco y red en la ventana 14:00-15:00.

# Predicción: ¿Qué 4 comandos de sar ejecutarías para
# cubrir los subsistemas principales? Si encuentras
# %iowait=45% en ese rango, ¿qué mirarías después?
```

<details><summary>Predicción</summary>

Los 4 comandos principales:
```bash
sar -u -s 14:00:00 -e 15:00:00 -1    # CPU
sar -r -s 14:00:00 -e 15:00:00 -1    # Memoria
sar -d -s 14:00:00 -e 15:00:00 -1    # Disco por dispositivo
sar -n DEV -s 14:00:00 -e 15:00:00 -1  # Red
```

O todo junto: `sar -A -s 14:00:00 -e 15:00:00 -1 > /tmp/incident.txt`

**Si %iowait=45%**, el problema es de I/O. Después mirarías:
1. `sar -d -s 14:00:00 -e 15:00:00 -1` → ¿qué disco tenía `await` alto y `%util` alto?
2. `sar -W -s 14:00:00 -e 15:00:00 -1` → ¿había actividad de swap? (¿la causa era falta de RAM?)
3. `sar -r -s 14:00:00 -e 15:00:00 -1` → ¿kbavail estaba bajando? ¿kbdirty alto?

El flujo completo: `sar -u` (detectar iowait) → `sar -d` (identificar disco) → `sar -W` / `sar -r` (¿es swap o I/O de app?). Si el incidente fuera ahora, seguirías con `iostat -x` (detalle del disco) → `iotop` (qué proceso).

</details>

### Ejercicio 10 — sadf y formatos de exportación

```bash
# 1. Listar formatos disponibles de sadf:
sadf -h 2>&1 | head -20

# 2. Ejemplo de exportación JSON:
# sadf -j /var/log/sysstat/sa$(date +%d) -- -u | jq '.'

# 3. Ejemplo de gráfico SVG:
# sadf -g /var/log/sysstat/sa$(date +%d) -- -u > /tmp/cpu-today.svg

# Predicción: ¿Por qué se usa "--" entre las opciones de sadf
# y las opciones de sar (como -u)? ¿Qué pasaría sin el "--"?
```

<details><summary>Predicción</summary>

El `--` es el separador estándar de opciones en comandos Unix. Indica **fin de opciones del comando actual**. En sadf, todo antes de `--` son opciones de sadf (formato, archivo); todo después de `--` son opciones que se pasan a sar (qué estadísticas extraer).

```bash
sadf -j /var/log/sysstat/sa25 -- -u
#     ^^^^^^^^^^^^^^^^^^^^^^^^    ^^
#     opciones de sadf            opciones de sar
#     (formato JSON, archivo)     (estadísticas de CPU)
```

Sin el `--`, sadf intentaría interpretar `-u` como una opción propia de sadf, no como una opción de sar. Resultaría en un error como "invalid option -- 'u'" o un comportamiento inesperado.

Otros ejemplos:
```bash
sadf -g /var/log/sysstat/sa25 -- -r      # gráfico SVG de memoria
sadf -d /var/log/sysstat/sa25 -- -n DEV  # CSV de red
sadf -j /var/log/sysstat/sa25 -- -A      # JSON de todo
```

</details>
