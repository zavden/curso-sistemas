# T02 — iostat

> **Objetivo:** Dominar `iostat` para diagnosticar problemas de rendimiento de almacenamiento. Entender las métricas de I/O, identificar cuellos de botella y correlacionar con otras herramientas.

## Qué es iostat

**iostat** (Input/Output Statistics) muestra estadísticas de **I/O de disco** y uso de **CPU**. Es la herramienta principal para diagnosticar problemas de rendimiento de almacenamiento. Forma parte del paquete **sysstat**.

```bash
# Instalar:
sudo apt install sysstat     # Debian/Ubuntu
sudo dnf install sysstat     # RHEL/Fedora

# Verificar:
iostat -V
```

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           IOSTAT OUTPUT                                     │
│                                                                              │
│  avg-cpu:  %user   %nice %system %iowait  %steal   %idle                  │
│             10.50    0.00    3.20    2.30    0.00   84.00               │
│                                                                              │
│  Device     tps    kB_read/s    kB_wrtn/s    kB_read    kB_wrtn           │
│  sda       25.00       150.00       80.00     1500000     800000           │
│                                                                              │
│  └──────────────┘  └───────────────┘ └──────────────────┘                  │
│       disco           lectura           escritura                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Sintaxis

```bash
# Snapshot (promedio desde el boot):
iostat

# Monitoreo continuo:
iostat 2          # cada 2 segundos
iostat 2 10       # cada 2 segundos, 10 muestras

# Solo disco (sin CPU):
iostat -d 2

# Solo CPU (sin disco):
iostat -c 2

# Estadísticas extendidas (-x) — LAS MÁS ÚTILES:
iostat -x 2

# Dispositivos específicos:
iostat -x sda sdb 2

# Unidades legibles:
iostat -h 2       # human-readable (KB, MB, GB)

# En MB/s:
iostat -m 2

# Con timestamp:
iostat -t 2

# Por partición:
iostat -x -p ALL 2
```

---

## Salida básica (-d)

```bash
iostat -d 2
# Device     tps    kB_read/s    kB_wrtn/s    kB_dscd/s    kB_read    kB_wrtn    kB_dscd
# sda       25.00       150.00       80.00         0.00    1500000     800000          0
# sdb        5.00        20.00       10.00         0.00     200000     100000          0
```

| Columna | Descripción |
|---------|-------------|
| `tps` | Transfers Per Second — operaciones de I/O por segundo (IOPS) |
| `kB_read/s` | KB leídos por segundo (throughput de lectura) |
| `kB_wrtn/s` | KB escritos por segundo (throughput de escritura) |
| `kB_dscd/s` | KB descartados por segundo (TRIM en SSD) |
| `kB_read` | KB leídos totales (desde el boot o intervalo) |
| `kB_wrtn` | KB escritos totales |
| `kB_dscd` | KB descartados totales |

---

## Estadísticas extendidas (-x)

Las estadísticas extendidas son las que realmente necesitas para diagnosticar:

```bash
iostat -x 2
# Device  r/s  rkB/s  rrqm/s  %rrqm  r_await rareq-sz  w/s   wkB/s  wrqm/s  %wrqm  w_await wareq-sz  d/s  dkB/s  drqm/s  %drqm  d_await dareq-sz  f/s  f_await  aqu-sz  %util
# sda    15.00 150.00   2.00  11.76    1.50    10.00 10.00   80.00   5.00  33.33    2.00      8.00  0.00   0.00    0.00   0.00     0.00      0.00  0.00     0.00    0.03   2.50
```

### Columnas de LECTURA

| Columna | Descripción |
|---------|-------------|
| `r/s` | Lecturas por segundo (IOPS de lectura) |
| `rkB/s` | KB leídos por segundo (throughput de lectura) |
| `rrqm/s` | Requests de lectura mergeados por segundo |
| `%rrqm` | Porcentaje de reads mergeados |
| `r_await` | Tiempo de espera promedio por lectura (ms) — **LA MÁS IMPORTANTE** |
| `rareoq-sz` | Tamaño promedio del request de lectura (KB) |

### Columnas de ESCRITURA

| Columna | Descripción |
|---------|-------------|
| `w/s` | Escrituras por segundo (IOPS de escritura) |
| `wkB/s` | KB escritos por segundo (throughput de escritura) |
| `wrqm/s` | Requests de escritura mergeados por segundo |
| `%wrqm` | Porcentaje de writes mergeados |
| `w_await` | Tiempo de espera promedio por escritura (ms) — **LA MÁS IMPORTANTE** |
| `wareq-sz` | Tamaño promedio del request de escritura (KB) |

### Columnas de DESCARTE (SSD TRIM)

| Columna | Descripción |
|---------|-------------|
| `d/s` | Operaciones de descarte por segundo |
| `dkB/s` | KB descartados por segundo |
| `drqm/s` | Requests de descarte mergeados |
| `%drqm` | Porcentaje de discards mergeados |
| `d_await` | Tiempo de espera de descarte (ms) |
| `dareq-sz` | Tamaño promedio de descarte |

### Columnas GENERALES

| Columna | Descripción |
|---------|-------------|
| `f/s` | Requests de flush por segundo (sincronización) |
| `f_await` | Tiempo de espera de flush (ms) |
| `aqu-sz` | **Average Queue Size** — requests en cola en promedio |
| `%util` | Porcentaje de tiempo que el dispositivo estuvo ocupado |

---

## Interpretar las métricas clave

### await — Tiempo de respuesta (LA MÉTRICA MÁS IMPORTANTE)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  VALORES DE REFERENCIA PARA await (lectura o escritura)                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  SSD NVMe:     < 0.5 ms    → excelente                                     │
│  SSD SATA:     < 2 ms      → bueno                                         │
│  HDD 7200 RPM: 5-10 ms    → normal                                         │
│  HDD 5400 RPM: 10-20 ms   → normal                                         │
│                                                                              │
│  ALERTAS:                                                                    │
│  > 20 ms en SSD    → algo está mal (posible problema de cola)            │
│  > 50 ms en HDD    → disco lento o saturado                               │
│  > 100 ms           → problema serio                                      │
│                                                                              │
│  DIAGNÓSTICO:                                                               │
│  await alto + %util alto = disco saturado                                 │
│  await alto + %util bajo = problema de hardware o controlador             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### %util — Utilización del disco

```bash
# %util = porcentaje de tiempo con al menos una operación en curso
# 100% = saturado (en HDD tradicionales)

# EN HDD:
# %util ~100% → disco saturado, no puede atender más requests

# EN SSD y RAID:
# %util ~100% NO necesariamente significa saturación
# Porque pueden atender múltiples requests en paralelo
# Un NVMe puede tener %util=100% pero await bajo → maneja bien la carga

# REGLA:
#   %util alto + await bajo  → ocupado pero rinde bien
#   %util alto + await alto  → problema real
```

### aqu-sz — Tamaño de cola

```bash
# Average Queue Size — requests esperando en promedio:

# < 1    → pocas operaciones pendientes (bien)
# 1-4    → carga moderada
# > 4    → posible saturación (depende del dispositivo)
# > 10   → el disco no da abasto

# En RAID o SSD NVMe, aqu-sz alto es más tolerable
# porque pueden procesar múltiples operaciones en paralelo
```

---

## Identificar tipo de carga

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  CARGA SECUENCIAL (backup, streaming)                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│  r/s o w/s → BAJO                                                          │
│  rkB/s o wkB/s → ALTO                                                     │
│  rareq-sz o wareq-sz → GRANDE (128KB+)                                   │
│                                                                              │
│  El disco lee/escribe grandes bloques secuenciales                         │
│  HDD puede manejar esto razonablemente bien                                 │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│  CARGA ALEATORIA (base de datos, web server)                              │
├─────────────────────────────────────────────────────────────────────────────┤
│  r/s o w/s → ALTO                                                          │
│  rkB/s o wkB/s → MODERADO                                                 │
│  rareq-sz o wareq-sz → PEQUEÑO (4-16KB)                                  │
│                                                                              │
│  Muchas operaciones pequeñas en posiciones aleatorias                       │
│  HDD rinde MAL con esto (seek time)                                        │
│  SSD rinde BIEN con ambos                                                   │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Patrones de diagnóstico

### Disco saturado

```bash
iostat -x 1 5
# Señales de saturación:
#   %util > 90%
#   await > 50ms (HDD) o > 10ms (SSD)
#   aqu-sz > 4

# Causas posibles:
#   - Aplicación haciendo I/O excesivo
#   - Swap (verificar con vmstat si/so)
#   - Disco lento o fallando
#   - RAID en modo degradado (reconstruyendo)

# Siguiente paso — usar iotop:
sudo iotop -o    # solo procesos con I/O activo
```

### Lectura vs Escritura

```bash
iostat -x 1 5
# Comparar: r_await vs w_await
# Comparar: r/s vs w/s, rkB/s vs wkB/s

# LECTURA ALTA indica:
#   - Base de datos leyendo mucho (cache fría, queries sin índice)
#   - Aplicación leyendo archivos grandes

# ESCRITURA ALTA indica:
#   - Logging excesivo
#   - Base de datos escribiendo (commits, checkpoints)
#   - Backup en progreso
#   - Swap out (falta de RAM)
```

### Comparar dispositivos

```bash
# Ver todos los dispositivos:
iostat -x 1
# Comparar %util y await entre sda, sdb, nvme0n1, etc.

# Si un dispositivo tiene await mayor que otros:
#   - Puede ser un disco más lento (HDD vs SSD)
#   - Puede estar fallando
#   - Puede tener más carga

# En RAID:
# Si un disco tiene más actividad que otros
# → hot disk o RAID desbalanceado
```

---

## Combinar con otras herramientas

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  DIAGNÓSTICO COMPLETO DE I/O                                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  1. vmstat → ¿hay I/O wait en CPU?                                        │
│     └─ Si wa > 20%, hay problema de I/O                                   │
│                                                                              │
│  2. iostat -x → ¿qué disco está saturado?                                  │
│     └─ await, %util, aqu-sz                                                │
│                                                                              │
│  3. iotop → ¿qué proceso genera el I/O?                                   │
│     └─ sudo iotop -oP                                                     │
│                                                                              │
│  4. ionice → ¿puedo priorizar el proceso?                                 │
│     └─ ionice -c 3 -p PID  (baja prioridad)                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

```bash
# iostat muestra QUÉ DISCO está ocupado
# iotop muestra QUÉ PROCESO genera el I/O

# Impacto en CPU (wa = I/O wait):
vmstat 1

# Completo:
vmstat 1
# En otro terminal:
iostat -x 2
# En otro:
sudo iotop -oP
```

---

## Quick reference

```
SINTAXIS:
  iostat              → snapshot
  iostat N            → cada N segundos (infinito)
  iostat N M         → cada N segundos, M muestras
  iostat -d          → solo disco
  iostat -c          → solo CPU
  iostat -x          → estadísticas extendidas (LAS IMPORTANTES)
  iostat -x -p ALL   → por partición
  iostat -h          → human-readable
  iostat -m          → en MB/s

MÉTRICAS CLAVE:
  await < 0.5ms  → SSD NVMe excelente
  await < 2ms    → SSD SATA bueno
  await < 10ms   → HDD normal
  aqu-sz > 4    → posible saturación
  %util + await altos → disco saturado

TIPO DE CARGA:
  Alta throughput + bajo IOPS  → secuencial (backup)
  Bajo throughput + alto IOPS  → aleatorio (DB)
```

---

## Ejercicios

### Ejercicio 1 — Lectura básica

```bash
# 1. Ver estadísticas de todos los dispositivos:
iostat -x 1 3

# 2. ¿Cuántos dispositivos de bloque tienes?
iostat -d | grep -c "^[a-z]"

# 3. ¿Cuál tiene mayor %util?
iostat -x 1 1 | grep -v "^$" | grep -v "Linux" | grep -v "Device"

# 4. ¿Cuál es el await promedio?
iostat -x 1 1 | awk 'NR>6 {print $1, $10}'
```

### Ejercicio 2 — Generar I/O y observar

```bash
# Terminal 1: generar lectura secuencial:
dd if=/dev/zero of=/tmp/testfile bs=1M count=1000 2>/dev/null &

# Terminal 2: observar durante la escritura:
iostat -x 1 10

# ¿Qué columnas cambian? ¿wkB/s? ¿w/s? ¿%util? ¿w_await?

# Limpiar:
rm /tmp/testfile
```

### Ejercicio 3 — Interpretar escenarios

```bash
# ¿Qué indica cada salida?

# a) r/s=500, rkB/s=2000, r_await=0.5, %util=30%
#    → SSD NVMe con carga moderada (bajo await, moderada util)

# b) w/s=50, wkB/s=25000, w_await=80, %util=98%
#    → Disco saturado en escritura (alto await, alta util)

# c) r/s=5, w/s=5, r_await=1, w_await=1, %util=1%
#    → Disco prácticamente idle (todo bajo)

# d) r/s=1000, rkB/s=4000, r_await=15, %util=95%
#    → HDD con carga aleatoria intensa (IOPS alto, await alto, util alta)
```

### Ejercicio 4 — Diagnóstico de problema

```bash
# Escenario: el servidor está lento

# 1. Ver si hay I/O wait en CPU:
vmstat 1 3
# Si wa > 10%, hay problema de I/O

# 2. Ver cuál disco está saturado:
iostat -x 1 5
# Identificar: %util > 80%? await > 20ms?

# 3. Ver qué proceso causa el I/O:
sudo iotop -oP
# Identificar el proceso con mayor I/O

# 4. ¿Lectura o escritura?
# Comparar r/s vs w/s en iostat
```

### Ejercicio 5 — Comparar tipos de carga

```bash
# 1. Generar carga SECUENCIAL:
dd if=/dev/zero of=/tmp/seq bs=1M count=500 2>/dev/null &

# 2. Observar:
iostat -x 1 5
# Nota: alto wkB/s, bajo w/s, wareq-sz grande (1MB)

# 3. Limpiar:
rm /tmp/seq

# 4. Generar carga ALEATORIA:
for i in $(seq 1 1000); do
    dd if=/dev/urandom of=/tmp/rand-$i bs=4K count=1 2>/dev/null
done &

# 5. Observar:
iostat -x 1 5
# Nota: alto w/s, bajo wkB/s, wareq-sz pequeño (4KB)

# 6. Limpiar:
rm /tmp/rand-*
```

### Ejercicio 6 — Por partición

```bash
# Ver estadísticas por partición:
iostat -x -p ALL 1 3

# Identificar:
# - ¿Qué partición tiene más I/O?
# - ¿/var o /home o / son los más activos?
# - ¿Hay alguna partición con await mucho mayor?
```
