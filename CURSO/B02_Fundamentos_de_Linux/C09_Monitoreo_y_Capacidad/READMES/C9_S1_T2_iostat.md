# T02 — iostat

> **Objetivo:** Dominar `iostat` para diagnosticar problemas de rendimiento de almacenamiento. Entender las métricas de I/O, identificar cuellos de botella y correlacionar con otras herramientas.

## Erratas detectadas en el material original

| Archivo | Línea | Error | Corrección |
|---------|-------|-------|------------|
| `README.max.md` | 112 | `rareoq-sz` (typo en nombre de columna) | `rareq-sz` (read average request size) |

---

## Qué es iostat

**iostat** (Input/Output Statistics) muestra estadísticas de **I/O de disco** y uso de **CPU**. Es la herramienta principal para diagnosticar problemas de rendimiento de almacenamiento. Forma parte del paquete **sysstat**.

```bash
# Install:
sudo apt install sysstat     # Debian/Ubuntu
sudo dnf install sysstat     # RHEL/Fedora

# Verify:
iostat -V
```

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           IOSTAT OUTPUT                                     │
│                                                                              │
│  avg-cpu:  %user   %nice %system %iowait  %steal   %idle                  │
│             10.50    0.00    3.20    2.30    0.00   84.00                   │
│                                                                              │
│  Device     tps    kB_read/s    kB_wrtn/s    kB_read    kB_wrtn           │
│  sda       25.00       150.00       80.00     1500000     800000           │
│                                                                              │
│  └──────────────┘  └───────────────┘ └──────────────────┘                  │
│       disco           lectura           escritura                           │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Sintaxis

```bash
# Snapshot (promedio desde el boot):
iostat

# Monitoreo continuo:
iostat 2          # cada 2 segundos (infinito)
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

# IMPORTANTE: como vmstat, la primera línea es el PROMEDIO
# desde el boot. Las siguientes son del intervalo
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
| `rareq-sz` | Tamaño promedio del request de lectura (KB) |

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

### Columnas de MERGE

El kernel combina requests adyacentes (al mismo área del disco) en una sola operación para mayor eficiencia. Los merges reducen IOPS reales enviados al disco.

```bash
# rrqm/s y wrqm/s altos → el kernel está optimizando I/O
# rrqm/s y wrqm/s ~0    → requests ya están dispersos (I/O aleatorio)
```

### Columnas GENERALES

| Columna | Descripción |
|---------|-------------|
| `f/s` | Requests de flush por segundo (sincronización a disco) |
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
│  > 20 ms en SSD    → algo está mal (posible problema de cola)               │
│  > 50 ms en HDD    → disco lento o saturado                                 │
│  > 100 ms           → problema serio                                        │
│                                                                              │
│  DIAGNÓSTICO:                                                               │
│  await alto + %util alto = disco saturado                                   │
│  await alto + %util bajo = problema de hardware o controlador               │
└─────────────────────────────────────────────────────────────────────────────┘
```

```bash
# await = tiempo en cola + tiempo de servicio
# Incluye TODO el tiempo desde que el request se envía hasta que se completa
# Si la cola está larga (aqu-sz alto), await sube aunque el disco sea rápido
```

### %util — Utilización del disco

```bash
# %util = porcentaje de tiempo con al menos una operación en curso

# EN HDD:
# %util ~100% → disco saturado, no puede atender más requests
# El throughput está al máximo

# EN SSD y RAID:
# %util ~100% NO necesariamente significa saturación
# Porque pueden atender múltiples requests en paralelo (queue depth > 1)
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
│  CARGA SECUENCIAL (backup, streaming, copia de archivos grandes)          │
├─────────────────────────────────────────────────────────────────────────────┤
│  r/s o w/s → BAJO                                                          │
│  rkB/s o wkB/s → ALTO                                                     │
│  rareq-sz o wareq-sz → GRANDE (128KB+)                                    │
│                                                                              │
│  El disco lee/escribe grandes bloques secuenciales                         │
│  HDD puede manejar esto razonablemente bien                                │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│  CARGA ALEATORIA (base de datos, web server)                              │
├─────────────────────────────────────────────────────────────────────────────┤
│  r/s o w/s → ALTO                                                          │
│  rkB/s o wkB/s → MODERADO                                                 │
│  rareq-sz o wareq-sz → PEQUEÑO (4-16KB)                                   │
│                                                                              │
│  Muchas operaciones pequeñas en posiciones aleatorias                       │
│  HDD rinde MAL con esto (seek time)                                        │
│  SSD rinde BIEN con ambos                                                   │
└─────────────────────────────────────────────────────────────────────────────┘
```

```bash
# Resumen:
# Si IOPS alto y throughput bajo → aleatorio (requests pequeños)
# Si IOPS bajo y throughput alto → secuencial (requests grandes)
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
sudo iotop -o     # solo procesos con I/O activo
sudo iotop -oP    # acumulado por proceso
```

### Lectura vs escritura

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

## iostat por partición

```bash
# Ver estadísticas por partición:
iostat -x -p ALL 2
# Muestra sda, sda1, sda2, etc.

# O particiones específicas:
iostat -x -p sda1 sda2 2

# Útil para identificar qué filesystem genera más I/O:
# Si sda1 (/) tiene poco I/O pero sda2 (/var) tiene mucho
# → los logs o la base de datos en /var son la causa
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
│  2. iostat -x → ¿qué disco está saturado?                                 │
│     └─ await, %util, aqu-sz                                                │
│                                                                              │
│  3. iotop → ¿qué proceso genera el I/O?                                   │
│     └─ sudo iotop -oP                                                     │
│                                                                              │
│  4. ionice → ¿puedo priorizar el proceso?                                 │
│     └─ ionice -c 3 -p PID  (clase idle — baja prioridad)                  │
└─────────────────────────────────────────────────────────────────────────────┘
```

```bash
# En terminales separados:
vmstat 1           # observar wa (I/O wait)
iostat -x 2        # observar await, %util por disco
sudo iotop -oP     # observar qué proceso genera I/O
```

---

## Quick reference

```
SINTAXIS:
  iostat              → snapshot (promedio desde boot)
  iostat N            → cada N segundos (infinito)
  iostat N M         → cada N segundos, M muestras
  iostat -d          → solo disco
  iostat -c          → solo CPU
  iostat -x          → estadísticas extendidas (LAS IMPORTANTES)
  iostat -x -p ALL   → por partición
  iostat -h          → human-readable
  iostat -m          → en MB/s
  iostat -t          → con timestamp

MÉTRICAS CLAVE:
  r_await / w_await  → latencia por operación (ms) — LO MÁS IMPORTANTE
  %util              → tiempo ocupado (cuidado en SSD/RAID)
  aqu-sz             → tamaño de cola
  r/s / w/s          → IOPS
  rkB/s / wkB/s      → throughput
  rareq-sz / wareq-sz → tamaño de request (indica tipo de carga)

UMBRALES:
  await < 0.5ms  → SSD NVMe excelente
  await < 2ms    → SSD SATA bueno
  await < 10ms   → HDD normal
  await > 50ms   → problema en HDD
  await > 100ms  → problema serio
  aqu-sz > 4     → posible saturación
  %util alto + await alto → disco saturado

TIPO DE CARGA:
  Alta throughput + bajo IOPS + request grande  → secuencial (backup)
  Bajo throughput + alto IOPS + request pequeño → aleatorio (DB)

FLUJO DE DIAGNÓSTICO:
  vmstat (wa) → iostat -x (qué disco) → iotop (qué proceso) → ionice (priorizar)
```

---

## Ejercicios

### Ejercicio 1 — Verificar iostat y salida básica

```bash
# 1. Verifica que iostat está instalado:
iostat -V

# 2. Ejecuta la salida básica:
iostat

# Predicción: ¿Qué dos secciones verás en la salida?
```

<details><summary>Predicción</summary>

Verás dos secciones:
- `avg-cpu:` — resumen de uso de CPU (`%user`, `%nice`, `%system`, `%iowait`, `%steal`, `%idle`)
- `Device:` — estadísticas por dispositivo de bloque (`tps`, `kB_read/s`, `kB_wrtn/s`, totales)

Como es la primera ejecución sin intervalo, los valores son **promedios desde el boot**.

</details>

### Ejercicio 2 — Salida básica vs extendida

```bash
# 1. Ejecuta la salida básica de disco:
iostat -d 1 3

# 2. Ejecuta la salida extendida:
iostat -x 1 3

# Predicción: ¿Cuántas columnas más tiene -x respecto a -d?
# ¿Cuáles son las columnas que SOLO aparecen en -x?
```

<details><summary>Predicción</summary>

La salida básica (`-d`) tiene 7 columnas: `tps`, `kB_read/s`, `kB_wrtn/s`, `kB_dscd/s`, `kB_read`, `kB_wrtn`, `kB_dscd`.

La salida extendida (`-x`) tiene ~22 columnas. Las exclusivas de `-x` incluyen:
- `r/s`, `w/s`, `d/s`, `f/s` — IOPS desglosados por tipo
- `rkB/s`, `wkB/s`, `dkB/s` — throughput desglosado
- `rrqm/s`, `wrqm/s`, `drqm/s`, `%rrqm`, `%wrqm`, `%drqm` — merges
- `r_await`, `w_await`, `d_await`, `f_await` — latencias
- `rareq-sz`, `wareq-sz`, `dareq-sz` — tamaños de request
- `aqu-sz`, `%util` — cola y utilización

En `-d` tienes `tps` (IOPS totales), en `-x` se desglosa en `r/s` + `w/s` + `d/s`.

</details>

### Ejercicio 3 — Opciones de formato

```bash
# 1. Ejecuta con timestamp:
iostat -t -d 1 2

# 2. Ejecuta en MB/s:
iostat -m -d 1 2

# 3. Ejecuta solo CPU:
iostat -c 1 2

# Predicción: En la salida de -c, ¿qué columna corresponde al
# I/O wait que vimos en vmstat?
```

<details><summary>Predicción</summary>

La columna `%iowait` en la sección `avg-cpu:` de iostat corresponde a la columna `wa` de vmstat. Ambas miden el porcentaje de tiempo de CPU esperando completar operaciones de I/O de disco.

Las columnas de CPU en iostat son: `%user`, `%nice`, `%system`, `%iowait`, `%steal`, `%idle`.

En vmstat son: `us`, `sy`, `id`, `wa`, `st`. Nota que iostat añade `%nice` (tiempo en procesos con prioridad cambiada por `nice`), que en vmstat se incluye dentro de `us`.

</details>

### Ejercicio 4 — Interpretar await y %util

```bash
# Examina estas salidas de iostat -x:
#
# a) Device   r_await  w_await  %util  aqu-sz
#    sda        0.3      0.5     25%    0.02
#
# b) Device   r_await  w_await  %util  aqu-sz
#    sda       85.0     120.0    98%    12.5
#
# c) Device   r_await  w_await  %util  aqu-sz
#    nvme0n1    0.2      0.3    100%    0.8

# Predicción: ¿Cuál tiene problema? ¿Cuál NO tiene problema
# a pesar de %util alto?
```

<details><summary>Predicción</summary>

- **a)** Sin problema. await bajo (~0.3-0.5ms → NVMe o SSD rápido), %util moderado (25%), cola casi vacía (0.02). Disco tranquilo y saludable.

- **b)** **Problema serio**. await altísimo (85-120ms), %util al 98%, cola de 12.5 requests. El disco está completamente saturado. await alto + %util alto = saturación real. Además, `w_await > r_await` indica que el problema es más de escritura.

- **c)** **Sin problema real** a pesar de %util=100%. El await es excelente (0.2-0.3ms, típico de NVMe), y la cola es baja (0.8). El NVMe está ocupado pero atiende requests con paralelismo sin degradar latencia. `%util alto + await bajo = ocupado pero rinde bien`.

</details>

### Ejercicio 5 — Identificar tipo de carga

```bash
# Examina estas salidas:
#
# a) Device  r/s    rkB/s   rareq-sz  w/s    wkB/s   wareq-sz
#    sda     10.0  10240.0   1024.0    0.0     0.0      0.0
#
# b) Device  r/s    rkB/s   rareq-sz  w/s    wkB/s   wareq-sz
#    sda    800.0   3200.0      4.0  200.0    800.0      4.0

# Predicción: ¿Cuál es secuencial y cuál aleatoria?
# ¿Cuál sufriría más en un HDD y por qué?
```

<details><summary>Predicción</summary>

- **a) Secuencial**: IOPS bajo (10 r/s), throughput alto (10 MB/s), request size grande (1024KB = 1MB). Son pocas lecturas grandes y secuenciales — patrón típico de backup o streaming de archivos.

- **b) Aleatoria**: IOPS alto (800 r/s + 200 w/s = 1000 IOPS), throughput moderado (3200+800 = 4000 KB/s ≈ 4 MB/s), request size pequeño (4KB). Son muchas operaciones pequeñas — patrón típico de base de datos o web server.

**La carga (b) sufriría mucho más en un HDD** porque cada operación aleatoria de 4KB requiere un seek físico del cabezal (5-10ms por seek). Con 1000 IOPS aleatorios, un HDD de 7200 RPM (que maneja ~100-150 IOPS aleatorios) se saturaría completamente. La carga (a) secuencial funciona bien en HDD porque el cabezal lee bloques contiguos sin seek.

</details>

### Ejercicio 6 — Generar I/O y observar columnas

```bash
# Terminal 1 — genera carga de escritura secuencial:
dd if=/dev/zero of=/tmp/testfile bs=1M count=500 2>/dev/null &

# Terminal 2 — observa:
iostat -x 1 10

# Predicción: ¿Qué columnas cambiarán significativamente?
# ¿Cómo será el wareq-sz?

# Al terminar, limpia:
rm -f /tmp/testfile
```

<details><summary>Predicción</summary>

Columnas que cambiarán significativamente:
- `w/s` — sube (escrituras por segundo)
- `wkB/s` — sube mucho (throughput de escritura alto, dd escribe a velocidad máxima)
- `wareq-sz` — será grande (~512KB-1024KB, porque dd usa `bs=1M`)
- `%util` — sube (disco ocupado con escritura)
- `w_await` — puede subir si el disco se satura
- `aqu-sz` — puede subir levemente
- `wrqm/s` — puede subir (el kernel puede mergear requests contiguos)

Columnas que NO cambiarán mucho:
- `r/s`, `rkB/s`, `r_await` — dd escribe, no lee (lee de `/dev/zero` que no es disco)
- `r_await` — sin cambio porque no hay lectura de disco

El patrón será típicamente **secuencial**: IOPS moderado pero throughput alto y request size grande.

</details>

### Ejercicio 7 — Por partición

```bash
# 1. Ver estadísticas de TODAS las particiones:
iostat -x -p ALL 1 3

# 2. Identifica:
#    - ¿Qué partición tiene más I/O (mayor %util)?
#    - ¿Cuántos dispositivos y particiones aparecen?

# Predicción: ¿Por qué la suma de I/O de las particiones
# debería coincidir aproximadamente con el dispositivo padre?
```

<details><summary>Predicción</summary>

Si tienes `sda` con `sda1`, `sda2`, `sda3`, la actividad de I/O del dispositivo `sda` es la suma de todas sus particiones. Los requests van al dispositivo físico, pero iostat puede desglosarlos por partición.

Esto es útil para diagnosticar: si `sda` tiene %util alto, mirar las particiones revela si el I/O viene de `/` (`sda1`), `/var` (`sda2`), `/home` (`sda3`), etc. Si `/var` genera la mayor parte del I/O, los logs o la base de datos son la causa probable.

Nota: la suma puede no ser exacta porque el scheduler del kernel agrupa y reordena requests, y los merges afectan las estadísticas.

</details>

### Ejercicio 8 — Flujo de diagnóstico completo

```bash
# Escenario: el servidor reporta lentitud.

# Paso 1 — ¿Hay I/O wait?
vmstat 1 3

# Paso 2 — ¿Qué disco está saturado?
iostat -x 1 3

# Paso 3 (si aplica) — ¿Qué proceso genera I/O?
# sudo iotop -oP

# Predicción: Si vmstat muestra wa=0% y id=5%, us=90%,
# ¿el problema es de disco? ¿Dónde buscarías?
```

<details><summary>Predicción</summary>

**No**, el problema NO es de disco. `wa=0%` indica que la CPU no está esperando I/O. El problema es de **CPU**: `us=90%` (user space) y `id=5%` (poco idle) indican que una aplicación está consumiendo casi toda la CPU.

No tendría sentido seguir con `iostat -x` en este caso. El siguiente paso sería:
1. `top` o `htop` — para identificar qué proceso consume CPU
2. `vmstat` → `r` (run queue) — si `r > nproc`, la CPU está saturada
3. Investigar el proceso: ¿es normal esa carga? ¿Hay un bug (loop infinito)? ¿Se necesita más CPU?

El flujo de diagnóstico completo requiere primero **descartar** dónde NO está el problema. Si `wa` es bajo, el disco no es el cuello de botella.

</details>

### Ejercicio 9 — Interpretar escenarios completos

```bash
# Para cada escenario, indica: ¿problema o normal? ¿De qué tipo? ¿Qué harías?
#
# a) r/s=500, rkB/s=2000, r_await=0.5, %util=30%, aqu-sz=0.1
#
# b) w/s=50, wkB/s=25000, w_await=80, %util=98%, aqu-sz=8.5
#
# c) r/s=5, w/s=5, r_await=1, w_await=1, %util=1%, aqu-sz=0.01
#
# d) r/s=1000, rkB/s=4000, r_await=15, %util=95%, aqu-sz=6.2
#
# e) r/s=200, rkB/s=51200, r_await=0.4, %util=99%, aqu-sz=2.5
```

<details><summary>Predicción</summary>

**a) Normal** — SSD NVMe con carga aleatoria de lectura moderada.
- 500 IOPS, 2000KB/s, request=4KB → aleatorio
- await=0.5ms (excelente para NVMe), %util=30%, cola=0.1
- No hay problema. El disco maneja la carga cómodamente.

**b) Problema — disco saturado en escritura.**
- 50 w/s, 25000KB/s (25 MB/s), request=500KB → secuencial
- await=80ms (muy alto), %util=98%, cola=8.5
- El disco no puede con la tasa de escritura. Posible HDD o disco fallando.
- Acción: `iotop` para ver qué proceso escribe tanto, verificar si hay backup o logging excesivo.

**c) Normal — disco prácticamente idle.**
- IOPS bajísimo, throughput mínimo, await=1ms, %util=1%
- Nada que hacer, el disco está descansando.

**d) Problema — HDD saturado con I/O aleatorio.**
- 1000 r/s, 4000KB/s, request=4KB → aleatorio intenso
- await=15ms (alto para SSD, normal-alto para HDD), %util=95%, cola=6.2
- Probablemente un HDD que no puede con 1000 IOPS aleatorios.
- Acción: migrar a SSD, o identificar el proceso con `iotop` y optimizar (añadir índices a la DB, más cache).

**e) Normal — NVMe a máxima capacidad sin degradar.**
- 200 r/s, 51200KB/s (50 MB/s), request=256KB → secuencial/mixto
- await=0.4ms (excelente), %util=99% pero cola=2.5 y await bajo
- El NVMe está ocupado pero rinde perfectamente. `%util alto + await bajo = sin problema`.

</details>

### Ejercicio 10 — Correlación iostat con vmstat

```bash
# 1. Ejecuta ambos en paralelo (terminales separados):
vmstat 1 10
iostat -x 1 10

# 2. Mientras corren, genera algo de I/O:
dd if=/dev/zero of=/tmp/testio bs=1M count=200 2>/dev/null; rm -f /tmp/testio

# Predicción: ¿Qué columna de vmstat se correlaciona con qué
# columnas de iostat? Nombra al menos 3 correlaciones.
```

<details><summary>Predicción</summary>

Correlaciones entre vmstat e iostat:

| vmstat | iostat | Qué miden |
|--------|--------|-----------|
| `wa` (CPU) | `%iowait` (avg-cpu) y `%util` (device) | Tiempo de CPU esperando I/O. `wa` sube cuando algún disco tiene `%util` alto |
| `bi` (blocks in) | `rkB/s` (read throughput) | KB leídos desde disco por segundo. Misma métrica, misma fuente (`/proc/diskstats`) |
| `bo` (blocks out) | `wkB/s` (write throughput) | KB escritos a disco por segundo |
| `b` (blocked procs) | `aqu-sz` y `await` | `b` en vmstat indica procesos bloqueados esperando I/O. Si `aqu-sz` y `await` son altos en iostat, `b` será > 0 en vmstat |
| `so` (swap out) | `wkB/s` en el dispositivo de swap | Si vmstat muestra `so > 0`, iostat mostrará escritura en el dispositivo de swap |

La principal correlación: cuando `wa` sube en vmstat, verás `%util` alto y/o `await` alto en iostat. vmstat te dice que **hay** un problema de I/O; iostat te dice **en qué disco** y **de qué tipo**.

</details>

---

## Tabla comparativa de métricas

| Métrica | Qué mide | Alerta si |
|---------|----------|-----------|
| `tps` | IOPS totales | Depende del disco |
| `r/s` | Lecturas por segundo | Alto + await alto |
| `w/s` | Escrituras por segundo | Alto + await alto |
| `rkB/s` | Throughput lectura | Depende del disco |
| `wkB/s` | Throughput escritura | Depende del disco |
| `r_await` | Latencia lectura (ms) | >20ms SSD, >50ms HDD |
| `w_await` | Latencia escritura (ms) | >20ms SSD, >50ms HDD |
| `rareq-sz` | Tamaño request lectura | Indica tipo de carga |
| `wareq-sz` | Tamaño request escritura | Indica tipo de carga |
| `aqu-sz` | Cola promedio | > 4 |
| `%util` | Tiempo ocupado | > 90% + await alto |
