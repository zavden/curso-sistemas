# /proc/mdstat y mdadm --detail

## Índice

1. [Dos fuentes de información](#dos-fuentes-de-información)
2. [/proc/mdstat — vista rápida](#procmdstat--vista-rápida)
3. [Leer /proc/mdstat campo a campo](#leer-procmdstat-campo-a-campo)
4. [Estados del array en mdstat](#estados-del-array-en-mdstat)
5. [mdadm --detail — vista completa](#mdadm---detail--vista-completa)
6. [Leer --detail campo a campo](#leer---detail-campo-a-campo)
7. [mdadm --examine — superbloque del disco](#mdadm---examine--superbloque-del-disco)
8. [Monitorización continua](#monitorización-continua)
9. [mdadm --monitor — alertas automáticas](#mdadm---monitor--alertas-automáticas)
10. [Diagnóstico de problemas](#diagnóstico-de-problemas)
11. [Errores comunes](#errores-comunes)
12. [Cheatsheet](#cheatsheet)
13. [Ejercicios](#ejercicios)

---

## Dos fuentes de información

RAID por software ofrece dos herramientas principales de inspección:

```
┌──────────────────────────────────────────────────────┐
│                                                      │
│  /proc/mdstat              mdadm --detail /dev/md0   │
│  ┌──────────────────┐      ┌──────────────────────┐  │
│  │ Todos los arrays │      │ Un array específico  │  │
│  │ Estado resumido  │      │ Detalle completo     │  │
│  │ Progreso rebuild │      │ UUIDs, roles, estado │  │
│  │ Sin root (lectu- │      │ Requiere root        │  │
│  │ ra libre)        │      │                      │  │
│  └──────────────────┘      └──────────────────────┘  │
│                                                      │
│  Rápido: cat /proc/mdstat                            │
│  Completo: mdadm --detail /dev/md0                   │
│  Disco: mdadm --examine /dev/vdb                     │
│                                                      │
└──────────────────────────────────────────────────────┘
```

En la práctica, `cat /proc/mdstat` es el primer reflejo para verificar el estado de RAID, y `mdadm --detail` se usa cuando necesitas información específica.

---

## /proc/mdstat — vista rápida

`/proc/mdstat` es un archivo virtual del kernel que muestra el estado de todos los arrays md en una sola vista.

### Array sano (RAID 1)

```
Personalities : [raid1]
md0 : active raid1 vdc[1] vdb[0]
      1047552 blocks super 1.2 [2/2] [UU]

unused devices: <none>
```

### Array sano (RAID 5)

```
Personalities : [raid1] [raid6] [raid5] [raid4]
md0 : active raid5 vdd[3] vdc[1] vdb[0]
      2093056 blocks super 1.2 level 5, 512k chunk, algorithm 2 [3/3] [UUU]

unused devices: <none>
```

### Array durante rebuild

```
Personalities : [raid1]
md0 : active raid1 vdd[2] vdb[0]
      1047552 blocks super 1.2 [2/1] [U_]
      [======>.............]  recovery = 34.5% (361728/1047552) finish=0.2min speed=57142K/sec

unused devices: <none>
```

### Array degradado

```
Personalities : [raid1]
md0 : active raid1 vdc[1](F) vdb[0]
      1047552 blocks super 1.2 [2/1] [U_]

unused devices: <none>
```

---

## Leer /proc/mdstat campo a campo

Desglose de una línea completa:

```
md0 : active raid5 vde[4](S) vdd[3] vdc[1] vdb[0]
│       │      │    │            │       │      │
│       │      │    │            │       │      └─ vdb, slot 0
│       │      │    │            │       └──────── vdc, slot 1
│       │      │    │            └──────────────── vdd, slot 3
│       │      │    └───────────────────────────── vde, slot 4, (S)pare
│       │      └────────────────────────────────── nivel RAID
│       └───────────────────────────────────────── estado del array
└───────────────────────────────────────────────── nombre del dispositivo
```

```
      2093056 blocks super 1.2 level 5, 512k chunk, algorithm 2 [3/3] [UUU]
      │              │          │        │            │          │      │
      │              │          │        │            │          │      └─ estado discos
      │              │          │        │            │          └──────── [total/activos]
      │              │          │        │            └─────────────────── algoritmo paridad
      │              │          │        └──────────────────────────────── tamaño de chunk
      │              │          └───────────────────────────────────────── nivel
      │              └──────────────────────────────────────────────────── versión metadatos
      └─────────────────────────────────────────────────────────────────── bloques (×1024=bytes)
```

### Indicadores de disco

| Indicador | Significado |
|-----------|-------------|
| `vdb[0]` | Disco en slot 0, activo |
| `vdc[1]` | Disco en slot 1, activo |
| `vde[4](S)` | Disco en slot 4, **spare** |
| `vdc[1](F)` | Disco en slot 1, **faulty** (fallido) |
| `vdd[2](W)` | Disco en slot 2, **write-mostly** (RAID 1) |

### Estado del array [total/activos]

| Indicador | Significado |
|-----------|-------------|
| `[2/2] [UU]` | 2 de 2 discos activos — sano |
| `[2/1] [U_]` | 1 de 2 activos — degradado |
| `[3/3] [UUU]` | 3 de 3 activos — sano |
| `[3/2] [UU_]` | 2 de 3 activos — degradado |
| `[4/3] [UUU_]` | 3 de 4 activos — degradado (RAID 6 tolera esto) |

`U` = Up, `_` = ausente/fallido.

---

## Estados del array en mdstat

### Resync (sincronización inicial)

Ocurre al crear un array nuevo. Los datos se distribuyen y la paridad se calcula.

```
md0 : active raid5 vdd[3] vdc[1] vdb[0]
      2093056 blocks super 1.2 level 5, 512k chunk, algorithm 2 [3/3] [UUU]
      [========>............]  resync = 42.3% (886272/2093056) finish=0.3min speed=73856K/sec
```

### Recovery (rebuild)

Ocurre cuando un disco nuevo reemplaza a uno fallido. Los datos se reconstruyen en el nuevo disco.

```
md0 : active raid1 vdd[2] vdb[0]
      1047552 blocks super 1.2 [2/1] [U_]
      [======>.............]  recovery = 34.5% (361728/1047552) finish=0.2min speed=57142K/sec
```

### Reshape (cambio de geometría)

Ocurre al añadir discos con `--grow` o cambiar el nivel RAID.

```
md0 : active raid5 vde[3] vdd[2] vdc[1] vdb[0]
      3139584 blocks super 1.2 level 5, 512k chunk, algorithm 2 [4/4] [UUUU]
      [=>.................]  reshape = 10.2% ...
```

### Check / Repair

Ocurre durante una verificación de consistencia programada.

```
md0 : active raid5 vdd[3] vdc[1] vdb[0]
      2093056 blocks super 1.2 level 5, 512k chunk, algorithm 2 [3/3] [UUU]
      [============>........]  check = 63.5% ...
```

### Línea de progreso

```
[======>.............]  recovery = 34.5% (361728/1047552) finish=0.2min speed=57142K/sec
 │                      │               │                 │             │
 │                      │               │                 │             └─ velocidad actual
 │                      │               │                 └─────────────── tiempo estimado
 │                      │               └───────────────────────────────── bloques procesados
 │                      └───────────────────────────────────────────────── tipo de operación
 └──────────────────────────────────────────────────────────────────────── barra visual
```

---

## mdadm --detail — vista completa

`mdadm --detail` muestra toda la información de un array específico.

```bash
mdadm --detail /dev/md0
```

### Salida completa anotada

```
/dev/md0:
           Version : 1.2                    ← versión de metadatos
     Creation Time : Thu Mar 20 10:00:00 2026  ← cuándo se creó
        Raid Level : raid5                  ← nivel RAID
        Array Size : 2093056 (2044.00 MiB 2143.29 MB)  ← tamaño útil
     Used Dev Size : 1046528 (1022.00 MiB 1071.64 MB)  ← tamaño por disco
      Raid Devices : 3                      ← discos esperados
     Total Devices : 4                      ← discos presentes (incl. spare)
       Persistence : Superblock is persistent

     Intent Bitmap : Internal              ← bitmap para rebuild rápido

       Update Time : Thu Mar 20 10:05:00 2026  ← última modificación
             State : clean                  ← estado general
    Active Devices : 3                      ← discos activos
   Working Devices : 4                      ← discos funcionales (activos + spare)
    Failed Devices : 0                      ← discos fallidos
     Spare Devices : 1                      ← hot spares

            Layout : left-symmetric         ← algoritmo de paridad
        Chunk Size : 512K                   ← tamaño del stripe

Consistency Policy : bitmap                 ← política de consistencia

              Name : lab-vm1:0              ← nombre del array
              UUID : 12345678:abcdef01:23456789:abcdef01  ← identificador único
            Events : 42                     ← contador de cambios

    Number   Major   Minor   RaidDevice State
       0     252      16        0      active sync   /dev/vdb
       1     252      32        1      active sync   /dev/vdc
       3     252      48        2      active sync   /dev/vdd
       4     252      64        -      spare          /dev/vde
```

---

## Leer --detail campo a campo

### Campos del encabezado

| Campo | Significado |
|-------|-------------|
| `Version` | Formato de metadatos (1.0, 1.2) |
| `Raid Level` | Nivel RAID (raid0, raid1, raid5, raid6, raid10) |
| `Array Size` | Tamaño usable del array |
| `Used Dev Size` | Tamaño usado de cada disco miembro |
| `Raid Devices` | Número de discos que componen el array |
| `Total Devices` | Discos totales incluyendo spares |

### Campo State (estado)

| Valor | Significado |
|-------|-------------|
| `clean` | Array sano, sin operaciones pendientes |
| `active` | Array activo, posiblemente con I/O |
| `degraded` | Falta al menos un disco |
| `recovering` | Rebuild en progreso |
| `resyncing` | Sincronización en progreso |
| `clean, degraded` | Sano pero con disco faltante |
| `clean, degraded, recovering` | Rebuild en curso |

### Tabla de discos

```
    Number   Major   Minor   RaidDevice State
       0     252      16        0      active sync   /dev/vdb
       1     252      32        1      active sync   /dev/vdc
       3     252      48        2      active sync   /dev/vdd
       4     252      64        -      spare          /dev/vde
```

| Columna | Significado |
|---------|-------------|
| `Number` | Número interno del disco |
| `Major/Minor` | Números de dispositivo del kernel |
| `RaidDevice` | Slot en el array (- = no asignado) |
| `State` | Estado del disco |

### Estados de disco

| Estado | Significado |
|--------|-------------|
| `active sync` | Disco activo, sincronizado |
| `spare` | Hot spare, esperando |
| `spare rebuilding` | Reconstruyendo datos en este disco |
| `faulty` | Disco marcado como fallido |
| `removed` | Disco retirado |

---

## mdadm --examine — superbloque del disco

Mientras `--detail` examina el array, `--examine` examina el **superbloque de un disco individual**.

```bash
mdadm --examine /dev/vdb
```

```
/dev/vdb:
          Magic : a92b4efc                  ← magic number de md
        Version : 1.2
    Feature Map : 0x1                       ← features activas
     Array UUID : 12345678:abcdef01:23456789:abcdef01
           Name : lab-vm1:0
  Creation Time : Thu Mar 20 10:00:00 2026
     Raid Level : raid5
   Raid Devices : 3
  Used Dev Size : 1046528 (1022.00 MiB)
    Data Offset : 2048 sectors
   Super Offset : 8 sectors
          State : clean
    Device UUID : aabbccdd:11223344:55667788:99aabbcc
    Update Time : Thu Mar 20 10:05:00 2026
       Checksum : abcdef01 - correct
         Events : 42
         Layout : left-symmetric
     Chunk Size : 512K
   Device Role : Active device 0            ← este disco es el slot 0
```

### Cuándo usar --examine

- **Disco suelto** que no sabes a qué array pertenece
- **Array no ensamblado** — los discos tienen superbloques pero `/dev/mdN` no existe
- **Recuperación** — verificar que los superbloques coinciden entre discos
- **Antes de reutilizar** un disco — confirmar si tiene superbloque md

```bash
# Verificar si un disco tiene superbloque
mdadm --examine /dev/vdb 2>&1
# Si no tiene: "No md superblock detected"
# Si tiene: muestra los metadatos

# Examinar todos los discos que podrían ser parte de un array
mdadm --examine --scan
```

### --detail vs --examine

```
┌────────────────────┬────────────────────────────────┐
│ --detail /dev/md0  │ --examine /dev/vdb             │
├────────────────────┼────────────────────────────────┤
│ Examina el ARRAY   │ Examina el DISCO               │
│ Array debe estar   │ No necesita array ensamblado    │
│ activo             │                                │
│ Muestra todos los  │ Muestra info de UN disco       │
│ discos y su estado │ y a qué array pertenece        │
│ Estado en vivo     │ Último estado conocido          │
│ (actual)           │ (grabado en superbloque)       │
└────────────────────┴────────────────────────────────┘
```

---

## Monitorización continua

### watch + /proc/mdstat

La forma más directa de monitorizar un rebuild o resync:

```bash
# Actualizar cada 1 segundo
watch -n1 cat /proc/mdstat
```

### Verificación de consistencia (scrub)

Linux puede verificar la consistencia de un array comparando datos y paridad:

```bash
# Iniciar verificación
echo check > /sys/block/md0/md/sync_action

# Ver progreso
cat /proc/mdstat

# Ver resultado (mismatches)
cat /sys/block/md0/md/mismatch_cnt
# 0 = todo consistente
# >0 = bloques inconsistentes encontrados

# Reparar inconsistencias
echo repair > /sys/block/md0/md/sync_action
```

### Verificación programada

Muchas distribuciones incluyen un cron o timer para verificar arrays periódicamente:

```bash
# RHEL/AlmaLinux: timer de systemd
systemctl status mdcheck_start.timer

# Ver si hay verificación programada
systemctl list-timers | grep md
```

### Velocidad de rebuild

El rebuild compite con el I/O normal. Se puede ajustar la velocidad mínima y máxima:

```bash
# Ver límites actuales
cat /proc/sys/dev/raid/speed_limit_min   # default: 1000 KB/s
cat /proc/sys/dev/raid/speed_limit_max   # default: 200000 KB/s

# Aumentar velocidad mínima de rebuild (acelerar rebuild)
echo 50000 > /proc/sys/dev/raid/speed_limit_min

# Reducir velocidad máxima (priorizar I/O de aplicaciones)
echo 100000 > /proc/sys/dev/raid/speed_limit_max
```

---

## mdadm --monitor — alertas automáticas

`mdadm --monitor` vigila los arrays y envía alertas cuando detecta eventos como fallos de disco o degradación.

### Modo daemon

```bash
# Iniciar monitor como daemon
mdadm --monitor --scan --daemonise --mail=root@localhost --pid-file=/var/run/mdadm/monitor.pid

# O usar el servicio de systemd
systemctl enable --now mdmonitor
systemctl status mdmonitor
```

### Configurar alertas por email

```bash
# En /etc/mdadm.conf
MAILADDR root@localhost
# O una dirección real
MAILADDR admin@example.com

# mdmonitor envía email cuando:
# - Un disco falla
# - Un array se degrada
# - Un rebuild comienza o termina
# - Un spare se activa
```

### Eventos que genera el monitor

| Evento | Significado |
|--------|-------------|
| `Fail` | Un disco fue marcado como fallido |
| `FailSpare` | Un spare falló durante rebuild |
| `DegradedArray` | El array está degradado |
| `SpareActive` | Un spare se activó para rebuild |
| `RebuildStarted` | Rebuild iniciado |
| `RebuildFinished` | Rebuild completado |
| `DeviceDisappeared` | El array desapareció |
| `MoveSpare` | Un spare fue movido entre arrays |

### Probar el monitor

```bash
# Verificar que el servicio está activo
systemctl status mdmonitor

# Simular un fallo para probar alertas
mdadm /dev/md0 --fail /dev/vdc
# El monitor debería enviar email y/o escribir al syslog

# Verificar logs
journalctl -u mdmonitor --since "5 minutes ago"
```

---

## Diagnóstico de problemas

### Flujo de diagnóstico

```
  ¿Problema con RAID?
          │
          ▼
  cat /proc/mdstat
          │
          ├── [UU] / [UUU] = sano
          │   └── Si hay problemas de rendimiento → ver sync_action, speed_limit
          │
          ├── [U_] / [UU_] = degradado
          │   └── mdadm --detail /dev/md0 → identificar disco fallido
          │       └── --fail → --remove → --add → watch rebuild
          │
          ├── rebuild en progreso
          │   └── Esperar. Monitorizar con watch
          │       └── Si es muy lento → ajustar speed_limit_min
          │
          └── array no aparece
              └── mdadm --examine /dev/vdb → ¿tiene superbloque?
                  ├── Sí → mdadm --assemble --scan
                  └── No → array fue destruido o discos son incorrectos
```

### Escenario: array no se ensambla al arrancar

```bash
# 1. Verificar si los discos tienen superbloques
mdadm --examine /dev/vdb
mdadm --examine /dev/vdc

# 2. Verificar que los UUIDs coinciden
mdadm --examine /dev/vdb | grep "Array UUID"
mdadm --examine /dev/vdc | grep "Array UUID"
# Deben ser iguales

# 3. Verificar mdadm.conf
cat /etc/mdadm.conf
# ¿El UUID coincide con el de los superbloques?

# 4. Ensamblar manualmente
mdadm --assemble /dev/md0 /dev/vdb /dev/vdc

# 5. Si funciona: actualizar mdadm.conf y regenerar initramfs
mdadm --detail --scan > /etc/mdadm.conf
dracut --force    # RHEL
```

### Escenario: mismatch_cnt > 0 después de check

```bash
# Ver el conteo de mismatches
cat /sys/block/md0/md/mismatch_cnt
# 128    ← bloques con datos inconsistentes

# Para RAID 1: puede ser inofensivo (áreas no inicializadas)
# Para RAID 5/6: puede indicar corrupción real

# Reparar (sobreescribe con datos recalculados)
echo repair > /sys/block/md0/md/sync_action
# Esperar a que termine
cat /proc/mdstat

# Verificar de nuevo
echo check > /sys/block/md0/md/sync_action
# Esperar
cat /sys/block/md0/md/mismatch_cnt
# 0    ← resuelto
```

### Escenario: ver qué discos pertenecen a qué arrays

```bash
# Escanear todos los superbloques
mdadm --examine --scan
# ARRAY /dev/md/0  metadata=1.2 UUID=12345678:... name=lab-vm1:0

# O examinar disco por disco
for d in /dev/vd{b,c,d,e}; do
    echo "=== $d ==="
    mdadm --examine "$d" 2>&1 | grep -E "Array UUID|Raid Level|Device Role"
done
```

---

## Errores comunes

### 1. Confundir --detail con --examine

```bash
# ✗ --detail en un disco (no es un array)
mdadm --detail /dev/vdb
# mdadm: /dev/vdb does not appear to be an md device

# ✗ --examine en un array (no es un disco)
mdadm --examine /dev/md0
# (puede dar resultados confusos)

# ✓ --detail para ARRAYS, --examine para DISCOS
mdadm --detail /dev/md0      # información del array
mdadm --examine /dev/vdb     # superbloque del disco
```

### 2. Ignorar mismatch_cnt después de un check

```bash
# ✗ Ejecutar check y no mirar el resultado
echo check > /sys/block/md0/md/sync_action
# ... y olvidarse

# ✓ Siempre verificar mismatch_cnt
echo check > /sys/block/md0/md/sync_action
# Esperar a que termine (watch cat /proc/mdstat)
cat /sys/block/md0/md/mismatch_cnt
# 0 = OK, >0 = investigar
```

### 3. No monitorizar durante el rebuild

```bash
# ✗ Añadir disco y asumir que todo va bien
mdadm /dev/md0 --add /dev/vdd
# (salir y olvidarse)

# ✓ Verificar que el rebuild progresa y completa
mdadm /dev/md0 --add /dev/vdd
watch cat /proc/mdstat
# Esperar hasta [UU] o [UUU]
mdadm --detail /dev/md0    # confirmar State: clean
```

### 4. Alarma por rebuild lento sin ajustar speed_limit

```bash
# ✗ Esperar horas con speed_limit_min = 1000
cat /proc/sys/dev/raid/speed_limit_min
# 1000    ← muy lento

# ✓ Aumentar si el sistema no está bajo carga
echo 50000 > /proc/sys/dev/raid/speed_limit_min
# El rebuild se acelera significativamente
```

### 5. No tener mdmonitor activo

```bash
# ✗ Disco falla silenciosamente, nadie se entera
# Días después, segundo disco falla → pérdida de datos

# ✓ Activar monitorización
systemctl enable --now mdmonitor
# Configurar MAILADDR en /etc/mdadm.conf
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│          /proc/mdstat y mdadm --detail — Referencia rápida       │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  INSPECCIÓN RÁPIDA:                                              │
│    cat /proc/mdstat                    todos los arrays          │
│    watch -n1 cat /proc/mdstat          monitorizar en vivo       │
│                                                                  │
│  INSPECCIÓN DETALLADA:                                           │
│    mdadm --detail /dev/md0             detalle del ARRAY         │
│    mdadm --examine /dev/vdb            superbloque del DISCO     │
│    mdadm --examine --scan              escanear todos            │
│                                                                  │
│  LEER /proc/mdstat:                                              │
│    [UU]  = sano          [U_] = degradado                        │
│    (F)   = faulty        (S)  = spare                            │
│    resync/recovery/reshape = operación en curso                  │
│                                                                  │
│  LEER --detail:                                                  │
│    State: clean           = sano                                 │
│    State: clean, degraded = falta disco                          │
│    Active/Working/Failed/Spare Devices = conteo                  │
│                                                                  │
│  VERIFICACIÓN:                                                   │
│    echo check  > /sys/block/md0/md/sync_action                   │
│    echo repair > /sys/block/md0/md/sync_action                   │
│    cat /sys/block/md0/md/mismatch_cnt   0 = OK                   │
│                                                                  │
│  VELOCIDAD REBUILD:                                              │
│    cat /proc/sys/dev/raid/speed_limit_min                        │
│    echo 50000 > /proc/sys/dev/raid/speed_limit_min               │
│                                                                  │
│  MONITORIZACIÓN:                                                 │
│    systemctl enable --now mdmonitor                              │
│    MAILADDR admin@example.com   (en /etc/mdadm.conf)             │
│    journalctl -u mdmonitor                                       │
│                                                                  │
│  DIAGNÓSTICO:                                                    │
│    No aparece → mdadm --examine discos → --assemble              │
│    Degradado  → --detail → identificar fallo → reemplazar        │
│    Lento      → ajustar speed_limit_min                          │
│    mismatch   → repair → re-check                                │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Interpretar /proc/mdstat

En tu VM de lab, crea un RAID 5 con 3 discos y observa los estados:

1. Crea el array:
   ```bash
   mdadm --create /dev/md0 --level=5 --raid-devices=3 /dev/vdb /dev/vdc /dev/vdd
   ```
2. Inmediatamente, ejecuta `cat /proc/mdstat` — ¿ves `resync`?
3. Identifica en la salida: nivel, discos, slots, porcentaje, velocidad, tiempo estimado
4. Espera a que termine la sincronización (`[UUU]`)
5. Ejecuta `mdadm --detail /dev/md0` — anota: State, Active Devices, Array UUID
6. Compara la información de mdstat con la de --detail: ¿cuál tiene más datos?
7. Ejecuta `mdadm --examine /dev/vdb` — ¿qué información adicional tiene el superbloque?

> **Pregunta de reflexión**: ¿por qué /proc/mdstat muestra el progreso de sincronización pero mdadm --detail no? ¿Cuál usarías para un script de monitorización y por qué?

### Ejercicio 2: Diagnosticar y reparar un fallo

Continuando con el RAID 5 del ejercicio anterior:

1. Crea filesystem, monta, escribe datos
2. Ejecuta `cat /proc/mdstat` y anota el estado
3. Simula fallo: `mdadm /dev/md0 --fail /dev/vdc`
4. Lee `/proc/mdstat` — identifica: qué disco tiene `(F)`, qué muestra `[UU_]`
5. Lee `mdadm --detail /dev/md0` — ¿qué dice State? ¿Cuántos Failed Devices?
6. Verifica que los datos siguen accesibles: `cat /mnt/raid/testfile`
7. Retira y añade:
   ```bash
   mdadm /dev/md0 --remove /dev/vdc
   mdadm --zero-superblock /dev/vdc
   mdadm /dev/md0 --add /dev/vdc
   ```
8. Ejecuta `watch -n1 cat /proc/mdstat` — observa el rebuild completo
9. Cuando termine: `mdadm --detail /dev/md0` — ¿State volvió a `clean`?

> **Pregunta de reflexión**: durante el rebuild (paso 8), ¿qué pasaría si otro disco fallara en un RAID 5? ¿Y si fuera RAID 6?

### Ejercicio 3: Verificación de consistencia y monitorización

Continuando con el array:

1. Inicia una verificación:
   ```bash
   echo check > /sys/block/md0/md/sync_action
   ```
2. Monitoriza el progreso: `watch cat /proc/mdstat`
3. Cuando termine, lee el resultado: `cat /sys/block/md0/md/mismatch_cnt`
4. ¿El valor es 0? Si no, ejecuta repair y verifica de nuevo
5. Verifica si mdmonitor está activo: `systemctl status mdmonitor`
6. Simula un fallo y revisa los logs:
   ```bash
   mdadm /dev/md0 --fail /dev/vdd
   journalctl -u mdmonitor --since "1 minute ago"
   ```
7. ¿Apareció un evento en los logs?
8. Repara el array y limpia todo al terminar

> **Pregunta de reflexión**: si configuramos `MAILADDR` en mdadm.conf pero no tenemos un servidor de correo configurado en el sistema, ¿las alertas se pierden? ¿Dónde más podrías buscar los eventos de RAID?
