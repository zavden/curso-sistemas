# T02 — iostat

## Qué es iostat

iostat muestra estadísticas de **I/O de disco** y **CPU**. Es la
herramienta principal para diagnosticar problemas de rendimiento de
almacenamiento. Forma parte del paquete **sysstat**:

```bash
# Instalar:
sudo apt install sysstat     # Debian/Ubuntu
sudo dnf install sysstat     # RHEL/Fedora

# Verificar:
iostat
# Linux 6.x (server)   03/17/2026  _x86_64_  (4 CPU)
#
# avg-cpu:  %user   %nice %system %iowait  %steal   %idle
#            10.50    0.00    3.20    2.30    0.00   84.00
#
# Device     tps    kB_read/s    kB_wrtn/s    kB_dscd/s    kB_read    kB_wrtn    kB_dscd
# sda       25.00       150.00       80.00         0.00    1500000     800000          0
```

## Sintaxis

```bash
# Snapshot (promedios desde el boot):
iostat

# Monitoreo continuo:
iostat 2          # cada 2 segundos
iostat 2 10       # cada 2 segundos, 10 muestras

# Solo estadísticas de disco (sin CPU):
iostat -d 2

# Solo estadísticas de CPU (sin disco):
iostat -c 2

# Estadísticas extendidas (las más útiles):
iostat -x 2

# Dispositivos específicos:
iostat -x sda sdb 2

# En unidades legibles:
iostat -h 2       # human-readable (KB, MB, GB)

# En MB/s:
iostat -m 2

# Con timestamp:
iostat -t 2

# IMPORTANTE: como vmstat, la primera línea es el promedio
# desde el boot. Las siguientes son del intervalo
```

## Salida básica

```bash
iostat -d 2
# Device     tps    kB_read/s    kB_wrtn/s    kB_dscd/s    kB_read    kB_wrtn    kB_dscd
# sda       25.00       150.00       80.00         0.00    1500000     800000          0
# sdb        5.00        20.00       10.00         0.00     200000     100000          0

# tps        = Transfers Per Second (operaciones de I/O por segundo — IOPS)
# kB_read/s  = KB leídos por segundo
# kB_wrtn/s  = KB escritos por segundo
# kB_dscd/s  = KB descartados por segundo (TRIM en SSD)
# kB_read    = KB leídos totales (desde el boot o el intervalo)
# kB_wrtn    = KB escritos totales
# kB_dscd    = KB descartados totales
```

## Estadísticas extendidas (-x)

Las estadísticas extendidas son las que realmente necesitas para
diagnosticar problemas:

```bash
iostat -x 2
# Device  r/s   rkB/s  rrqm/s  %rrqm  r_await  rareq-sz  w/s   wkB/s  wrqm/s  %wrqm  w_await  wareq-sz  d/s  dkB/s  drqm/s  %drqm  d_await  dareq-sz  f/s  f_await  aqu-sz  %util
# sda    15.00  150.00   2.00  11.76    1.50     10.00   10.00   80.00    5.00  33.33    2.00      8.00  0.00   0.00    0.00   0.00     0.00      0.00  0.00     0.00    0.03   2.50
```

### Columnas clave

```bash
# LECTURA:
# r/s        = Lecturas por segundo (IOPS de lectura)
# rkB/s      = KB leídos por segundo (throughput de lectura)
# r_await    = Tiempo promedio de espera por lectura (ms)
#              Incluye tiempo en cola + tiempo de servicio
# rareq-sz   = Tamaño promedio de request de lectura (KB)

# ESCRITURA:
# w/s        = Escrituras por segundo (IOPS de escritura)
# wkB/s      = KB escritos por segundo (throughput de escritura)
# w_await    = Tiempo promedio de espera por escritura (ms)
# wareq-sz   = Tamaño promedio de request de escritura (KB)

# MERGE:
# rrqm/s     = Requests de lectura mergeados por segundo
# wrqm/s     = Requests de escritura mergeados por segundo
# El kernel combina requests adyacentes para eficiencia

# GENERALES:
# aqu-sz     = Average Queue Size (tamaño promedio de la cola)
#              Cuántos requests están esperando en promedio
# %util      = Porcentaje de tiempo que el dispositivo estuvo ocupado
#              100% = saturado (pero no siempre significa problema en SSD/RAID)
```

## Interpretar las métricas clave

### await — Tiempo de respuesta

```bash
# r_await y w_await son las métricas MÁS IMPORTANTES
# Miden cuánto tarda cada operación de I/O en completarse

# Valores de referencia:
# SSD NVMe:     < 0.5 ms    (excelente)
# SSD SATA:     < 2 ms      (bueno)
# HDD 7200 RPM: 5-10 ms    (normal)
# HDD 5400 RPM: 10-20 ms   (normal)

# Alertas:
# > 20 ms en SSD   → algo está mal
# > 50 ms en HDD   → disco lento o saturado
# > 100 ms          → problema serio

# await alto + %util alto = disco saturado
# await alto + %util bajo = problema de hardware o controlador
```

### %util — Utilización

```bash
# %util indica qué porcentaje del tiempo el dispositivo tuvo
# al menos una operación de I/O en curso

# En discos HDD tradicionales:
# %util ~100% → disco saturado, no puede atender más requests
# El throughput está al máximo

# En SSD y RAID:
# %util ~100% NO necesariamente significa saturación
# Porque pueden atender múltiples requests en paralelo
# Un SSD NVMe puede tener %util=100% pero aún tener capacidad
# Mirar await: si await es bajo con %util alto → el dispositivo
# maneja la carga bien

# Regla: %util alto + await alto = problema real
#        %util alto + await bajo = el dispositivo está ocupado pero rinde bien
```

### aqu-sz — Tamaño de cola

```bash
# aqu-sz indica cuántos requests están en cola en promedio

# aqu-sz < 1    → pocas operaciones pendientes (bien)
# aqu-sz 1-4    → carga moderada
# aqu-sz > 4    → posible saturación (depende del dispositivo)
# aqu-sz > 10   → el disco no da abasto

# En RAID o SSD NVMe, aqu-sz alto es más tolerable
# porque pueden procesar múltiples operaciones en paralelo
```

## Identificar tipo de carga

```bash
# Carga secuencial (backup, streaming):
# r/s o w/s bajo, pero rkB/s o wkB/s alto
# rareq-sz o wareq-sz grande (128KB+)
# El disco está leyendo/escribiendo grandes bloques secuenciales

# Carga aleatoria (base de datos, web server):
# r/s o w/s alto, pero rkB/s o wkB/s moderado
# rareq-sz o wareq-sz pequeño (4-16KB)
# El disco está haciendo muchas operaciones pequeñas

# HDD rinde mal con I/O aleatorio (seek time)
# SSD rinde bien con ambos tipos
```

## Patrones de diagnóstico

### Disco saturado

```bash
iostat -x 1 5
# Si ves:
# %util > 90%
# await > 50ms (HDD) o > 10ms (SSD)
# aqu-sz > 4

# Causas:
# - Aplicación haciendo I/O excesivo
# - Swap (verificar con vmstat si/so)
# - Disco lento o fallando
# - RAID en degradado (reconstruyendo)

# Siguiente paso:
# iotop (ver qué proceso hace más I/O)
sudo iotop -o    # solo procesos con I/O activo
```

### Lectura vs escritura

```bash
# ¿El problema es de lectura o escritura?
iostat -x 1 5
# Comparar r_await vs w_await
# Comparar r/s vs w/s, rkB/s vs wkB/s

# Lectura alta:
# - Base de datos leyendo mucho (cache fría, queries sin índice)
# - Aplicación leyendo archivos grandes

# Escritura alta:
# - Logging excesivo
# - Base de datos escribiendo (commits, checkpoints)
# - Backup en progreso
# - Swap out (falta de RAM)
```

### Comparar dispositivos

```bash
# Ver todos los dispositivos:
iostat -x 1
# Comparar %util y await entre sda, sdb, nvme0n1, etc.

# Si un dispositivo tiene await mucho mayor que otros:
# - Puede ser un disco más lento (HDD vs SSD)
# - Puede estar fallando
# - Puede tener más carga

# En RAID:
# Si un disco tiene más actividad que los otros,
# puede indicar un hot disk o un RAID desbalanceado
```

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

## Combinar con otras herramientas

```bash
# iostat muestra QUÉ disco está ocupado
# iotop muestra QUÉ PROCESO genera el I/O:
sudo iotop -oP     # procesos con I/O, acumulado por proceso

# vmstat muestra el impacto en CPU (wa = I/O wait):
vmstat 1

# Para diagnóstico completo:
# 1. vmstat → ¿hay I/O wait?
# 2. iostat -x → ¿qué disco está saturado?
# 3. iotop → ¿qué proceso genera el I/O?
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
```

### Ejercicio 2 — Generar I/O y observar

```bash
# En un terminal: generar lectura
# dd if=/dev/sda of=/dev/null bs=1M count=100 2>/dev/null &

# En otro terminal: observar
# iostat -x 1 10
# ¿Qué columnas cambian? ¿r/s? ¿rkB/s? ¿%util?
```

### Ejercicio 3 — Interpretar escenarios

```bash
# ¿Qué indica cada salida?
# a) r/s=500, rkB/s=2000, r_await=0.5, %util=30%
#    (SSD con carga moderada)

# b) w/s=50, wkB/s=25000, w_await=80, %util=98%
#    (disco saturado en escritura)

# c) r/s=5, w/s=5, r_await=1, w_await=1, %util=1%
#    (disco prácticamente idle)
```
