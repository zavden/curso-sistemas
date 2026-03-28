# Filesystem check

## Índice

1. [Por qué verificar filesystems](#por-qué-verificar-filesystems)
2. [fsck — el frontend universal](#fsck--el-frontend-universal)
3. [e2fsck — verificación de ext4](#e2fsck--verificación-de-ext4)
4. [xfs_repair — verificación de XFS](#xfs_repair--verificación-de-xfs)
5. [btrfs check](#btrfs-check)
6. [Verificación automática en arranque](#verificación-automática-en-arranque)
7. [Cuándo ejecutar fsck](#cuándo-ejecutar-fsck)
8. [Flujo de diagnóstico y reparación](#flujo-de-diagnóstico-y-reparación)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## Por qué verificar filesystems

Un filesystem puede corromperse por varias razones:

```
┌─────────────────────────────────────────────────────────┐
│              Causas de corrupción                        │
│                                                         │
│  ● Apagado abrupto (corte de luz, reset forzado)        │
│  ● Fallo de hardware (disco, controladora, RAM)         │
│  ● Bug en el kernel o el driver del filesystem          │
│  ● Escritura interrumpida durante operación crítica     │
│  ● Disco lleno durante operación de metadatos           │
└─────────────────────────────────────────────────────────┘
```

Los filesystems con **journal** (ext4, XFS) mitigan mucho este riesgo: al reiniciar, el journal se replaya automáticamente para completar operaciones pendientes. Pero el journal no protege contra corrupción de hardware ni contra todos los escenarios posibles.

La verificación de filesystem (`fsck`) examina la estructura interna — inodos, bloques, directorios, journal — y repara inconsistencias.

> **Regla fundamental**: nunca ejecutar fsck en un filesystem montado en modo lectura-escritura. Puede causar más corrupción de la que intenta reparar.

---

## fsck — el frontend universal

`fsck` (filesystem check) no es una herramienta única — es un **dispatcher** que llama a la herramienta específica de cada filesystem.

```
            fsck /dev/vdb1
                 │
                 ▼
         ¿Qué tipo de FS?
         ┌───────┼────────┐
         ▼       ▼        ▼
     fsck.ext4  fsck.xfs  fsck.btrfs
     (e2fsck)   (no-op)   (no-op)
```

### Sintaxis

```bash
fsck [opciones] <dispositivo>
```

### Opciones principales

| Opción | Efecto |
|--------|--------|
| `-t <type>` | Forzar tipo de filesystem |
| `-n` | Dry run — no reparar, solo reportar |
| `-y` | Responder "sí" a todas las preguntas |
| `-a` | Reparar automáticamente (sin preguntar) — equivale a `-p` |
| `-f` | Forzar verificación aunque el FS parezca limpio |
| `-C` | Mostrar barra de progreso |
| `-A` | Verificar todos los filesystems de fstab |

### Códigos de retorno

| Código | Significado |
|--------|-------------|
| `0` | Sin errores |
| `1` | Errores corregidos |
| `2` | Errores corregidos, se requiere reinicio |
| `4` | Errores no corregidos |
| `8` | Error operacional (no se pudo ejecutar) |
| `16` | Error de uso (sintaxis incorrecta) |
| `32` | fsck cancelado por el usuario |

```bash
# Verificar sin reparar
fsck -n /dev/vdb1

# Verificar y reparar automáticamente
fsck -y /dev/vdb1

# Forzar verificación completa con progreso
fsck -f -C /dev/vdb1

# Verificar un ext4 específicamente
fsck -t ext4 /dev/vdb1
# Equivale a: fsck.ext4 /dev/vdb1
```

> **Predicción**: si ejecutas `fsck` en un filesystem limpio sin `-f`, responderá inmediatamente "clean" sin verificar. La flag `-f` fuerza la verificación completa aunque el journal indica que el FS se cerró limpiamente.

---

## e2fsck — verificación de ext4

`e2fsck` es la herramienta real detrás de `fsck.ext4` (y `fsck.ext3`, `fsck.ext2`). Es la más completa y configurable de las herramientas de verificación.

### Fases de verificación

```
┌──────────────────────────────────────────────────┐
│              e2fsck — 5 fases                     │
│                                                   │
│  Fase 1: Verificar inodos, bloques, tamaños       │
│          (¿inodos válidos? ¿bloques asignados     │
│           correctamente?)                         │
│              │                                    │
│              ▼                                    │
│  Fase 2: Verificar estructura de directorios      │
│          (¿entradas de directorio apuntan a       │
│           inodos válidos?)                        │
│              │                                    │
│              ▼                                    │
│  Fase 3: Verificar conectividad de directorios    │
│          (¿todos los directorios son alcanzables  │
│           desde root?)                            │
│              │                                    │
│              ▼                                    │
│  Fase 4: Verificar contadores de referencia       │
│          (¿link count correcto para cada inodo?)  │
│              │                                    │
│              ▼                                    │
│  Fase 5: Verificar resumen de grupos              │
│          (¿bitmaps de bloques e inodos correctos?)│
└──────────────────────────────────────────────────┘
```

### Opciones específicas de e2fsck

| Opción | Efecto |
|--------|--------|
| `-n` | Solo lectura — no modificar nada |
| `-y` | Responder "sí" a todo |
| `-p` | Reparar automáticamente lo seguro (usado en arranque) |
| `-f` | Forzar verificación aunque esté marcado como limpio |
| `-b <block>` | Usar superbloque alternativo (si el principal está corrupto) |
| `-B <size>` | Tamaño de bloque |
| `-c` | Escaneo de bloques defectuosos (lento) |
| `-D` | Optimizar directorios |
| `-E <opts>` | Opciones extendidas |

### Ejemplos prácticos

```bash
# Verificación estándar (filesystem desmontado)
umount /mnt/data
e2fsck -f /dev/vdb1

# Salida típica:
# e2fsck 1.46.5 (30-Dec-2021)
# Pass 1: Checking inodes, blocks, and sizes
# Pass 2: Checking directory structure
# Pass 3: Checking directory connectivity
# Pass 4: Checking reference counts
# Pass 5: Checking group summary information
# /dev/vdb1: 11/65536 files (0.0% non-contiguous), 8523/262144 blocks

# Reparar automáticamente
e2fsck -y /dev/vdb1

# Dry run — ver qué haría sin cambiar nada
e2fsck -n /dev/vdb1

# Superbloque principal corrupto — usar backup
# Primero, encontrar superbloques de respaldo:
dumpe2fs /dev/vdb1 | grep -i superblock
#   Primary superblock at 0, ...
#   Backup superblock at 32768, ...
#   Backup superblock at 98304, ...

# Usar superbloque alternativo
e2fsck -b 32768 /dev/vdb1

# Escanear bloques defectuosos + verificar
e2fsck -c -f /dev/vdb1
```

### lost+found

Cuando e2fsck encuentra archivos que pertenecen al filesystem pero no están enlazados en ningún directorio, los coloca en el directorio `lost+found/` en la raíz del filesystem. Los archivos se nombran con su número de inodo.

```bash
# Después de una reparación
ls /mnt/data/lost+found/
# #12345  #12346  #12347

# Intentar identificar archivos recuperados
file /mnt/data/lost+found/#12345
# /mnt/data/lost+found/#12345: ASCII text
```

---

## xfs_repair — verificación de XFS

XFS **no usa `fsck`** de la forma tradicional. El binario `fsck.xfs` existe pero es un **no-op** — no hace nada. Esto es intencional: XFS confía en su journal para recuperarse de cierres abruptos, y la herramienta de reparación se ejecuta manualmente cuando hay corrupción real.

```bash
# fsck.xfs NO hace nada
fsck.xfs /dev/vdb1
# (retorna inmediatamente sin salida)

# La herramienta real es xfs_repair
xfs_repair /dev/vdb1
```

### Flujo de xfs_repair

```
┌────────────────────────────────────────────────┐
│            xfs_repair — flujo                   │
│                                                 │
│  1. Leer superbloque                            │
│  2. Verificar allocation groups                 │
│  3. Verificar inodos y directorios              │
│  4. Verificar free space / free inode maps      │
│  5. Reconstruir metadatos si es necesario       │
│                                                 │
│  IMPORTANTE: el filesystem DEBE estar           │
│  desmontado                                     │
└────────────────────────────────────────────────┘
```

### Opciones principales

| Opción | Efecto |
|--------|--------|
| `-n` | Dry run — solo reportar, no reparar |
| `-L` | Forzar descarte del journal (log) — **destructivo** |
| `-v` | Verbose |
| `-d` | Permitir reparación de un filesystem montado como solo-lectura (solo rootfs en recovery) |

### El problema del dirty log

Cuando un XFS no se desmontó limpiamente, su journal (log) contiene operaciones pendientes. `xfs_repair` necesita un journal limpio para trabajar:

```bash
# Caso típico: intentar reparar tras apagado abrupto
xfs_repair /dev/vdb1
# Phase 1 - find and verify superblock...
# ERROR: The filesystem has valuable metadata changes in a log which needs to
# be replayed.  Mount the filesystem to replay the log, and unmount it before
# re-running xfs_repair.  If you are unable to mount the filesystem, then use
# the -L option to destroy the log and attempt a repair.

# Solución preferida: montar y desmontar para replayar el journal
mount /dev/vdb1 /mnt/temp
umount /mnt/temp
xfs_repair /dev/vdb1
# Ahora funciona

# Solución de último recurso: descartar el journal
# ⚠ PUEDE PERDER DATOS de operaciones pendientes
xfs_repair -L /dev/vdb1
```

> **Predicción**: si intentas `xfs_repair` justo después de un apagado forzado, el error de dirty log es lo esperado. La primera opción siempre debe ser mount+umount para que el kernel replaye el journal. Solo si mount falla (filesystem tan corrupto que no se puede montar) se usa `-L`.

### Ejemplo completo

```bash
# Desmontar
umount /mnt/data

# Dry run primero
xfs_repair -n /dev/vdb1
# Si reporta errores:

# Reparar
xfs_repair /dev/vdb1

# Verificar resultado
xfs_repair -n /dev/vdb1
# No errors found

# Remontar
mount /dev/vdb1 /mnt/data
```

---

## btrfs check

Btrfs tiene su propia herramienta de verificación, pero con una advertencia importante: la funcionalidad de **reparación está marcada como experimental** y puede empeorar la situación.

```bash
# Verificar (solo lectura) — seguro
btrfs check /dev/vdb1

# Reparar — PELIGROSO, usar solo como último recurso
btrfs check --repair /dev/vdb1
# WARNING: the repair mode is not yet fully functional

# El wrapper fsck.btrfs existe pero no hace nada por defecto
fsck.btrfs /dev/vdb1
# (no-op, como fsck.xfs)
```

### Alternativas en Btrfs

En lugar de `btrfs check --repair`, Btrfs ofrece herramientas más seguras:

```bash
# Scrub — verifica checksums en un filesystem MONTADO
btrfs scrub start /mnt/data
btrfs scrub status /mnt/data

# Para corrupción de datos, los snapshots y RAID son la mejor protección
# Restaurar desde snapshot es más seguro que reparar
```

### Comparación de herramientas por filesystem

```
┌────────────┬──────────────┬───────────────┬────────────────────────┐
│ Filesystem │ Herramienta  │ fsck wrapper  │ Notas                  │
├────────────┼──────────────┼───────────────┼────────────────────────┤
│ ext4       │ e2fsck       │ fsck.ext4 ✓   │ Completa y confiable   │
│ XFS        │ xfs_repair   │ fsck.xfs ✗    │ No-op; usar xfs_repair │
│ Btrfs      │ btrfs check  │ fsck.btrfs ✗  │ No-op; reparación      │
│            │              │               │ experimental           │
│ vfat       │ fsck.vfat    │ fsck.vfat ✓   │ (dosfstools)           │
└────────────┴──────────────┴───────────────┴────────────────────────┘
```

---

## Verificación automática en arranque

### Campo pass en fstab (repaso)

El campo 6 de `/etc/fstab` controla si `fsck` se ejecuta durante el arranque:

```
UUID=...   /       ext4   defaults   0   1   ← verificar primero (rootfs)
UUID=...   /home   ext4   defaults   0   2   ← verificar después
UUID=...   /data   xfs    defaults   0   0   ← no verificar (XFS usa xfs_repair)
```

### Cómo funciona en systemd

```
┌──────────────────────────────────────────────────────────┐
│                Arranque con systemd                       │
│                                                          │
│  1. systemd-fsck-root.service                            │
│     → fsck en rootfs (pass=1)                            │
│     → Si falla: emergency shell                          │
│                                                          │
│  2. systemd-fsck@.service (template)                     │
│     → fsck en cada partición con pass=2                  │
│     → En paralelo si son dispositivos distintos          │
│     → Si falla: depende de configuración                 │
│                                                          │
│  3. local-fs.target                                      │
│     → Se alcanza cuando todos los fsck pasan             │
│     → Monta los filesystems                              │
└──────────────────────────────────────────────────────────┘
```

### Forzar fsck en el próximo arranque (ext4)

```bash
# Método 1: tune2fs — configurar conteo de montajes
tune2fs -c 1 /dev/vdb1   # verificar en el próximo montaje
tune2fs -c 30 /dev/vdb1  # verificar cada 30 montajes
tune2fs -c 0 /dev/vdb1   # deshabilitar verificación por conteo

# Método 2: tune2fs — configurar intervalo de tiempo
tune2fs -i 1m /dev/vdb1  # verificar si ha pasado más de 1 mes
tune2fs -i 0 /dev/vdb1   # deshabilitar verificación por tiempo

# Ver configuración actual
tune2fs -l /dev/vdb1 | grep -i -E "mount count|check interval|last checked"
# Mount count:              5
# Maximum mount count:      -1
# Check interval:           0 (<none>)
# Last checked:             Fri Mar 20 10:00:00 2026

# Método 3: crear archivo /forcefsck (legacy, no todos los sistemas lo respetan)
touch /forcefsck
reboot
```

### Deshabilitar verificación automática

Para filesystems que no necesitan fsck en arranque (XFS, Btrfs, o cuando se prefiere verificar manualmente):

```bash
# En fstab: poner pass=0
UUID=...   /data   xfs   defaults   0   0

# Para ext4: deshabilitar conteo e intervalo
tune2fs -c 0 -i 0 /dev/vdb1
```

---

## Cuándo ejecutar fsck

### Ejecutar fsck cuando

| Situación | Acción |
|-----------|--------|
| Apagado abrupto y el journal no pudo replayarse | fsck / xfs_repair |
| Errores de I/O en dmesg (`EXT4-fs error`, `XFS: metadata I/O error`) | fsck urgente |
| Archivos que desaparecen o se corrompen sin explicación | fsck + verificar hardware |
| Sistema arrancó en modo de emergencia | fsck desde recovery |
| Antes de redimensionar un filesystem (shrink) | e2fsck -f obligatorio para ext4 |
| Migración o clonación de disco | fsck del destino |

### NO ejecutar fsck cuando

| Situación | Por qué |
|-----------|---------|
| Filesystem montado en rw | Puede causar más corrupción |
| Sin evidencia de problemas | Innecesario — el journal protege |
| Producción sin ventana de mantenimiento | Requiere desmontar |
| Btrfs con `--repair` sin backup | Experimental y peligroso |

### Cómo detectar problemas

```bash
# Revisar mensajes del kernel sobre errores de filesystem
dmesg | grep -i -E "ext4|xfs|error|corrupt|i/o"

# Revisar journal de systemd
journalctl -k | grep -i -E "ext4|xfs|error|corrupt"

# Estado del filesystem ext4
tune2fs -l /dev/vdb1 | grep -i "state"
# Filesystem state:         clean     ← OK
# Filesystem state:         not clean ← necesita fsck

# Estado del filesystem XFS
xfs_info /dev/vdb1   # (debe estar montado)
```

---

## Flujo de diagnóstico y reparación

### Flujo general

```
    ¿Hay evidencia de corrupción?
    (errores I/O, archivos corruptos, estado "not clean")
              │
         Sí   │   No
              │    └──► No hacer nada
              ▼
    ¿El filesystem está montado?
              │
         Sí   │   No
              │    └──► Ir a reparación
              ▼
    Desmontar el filesystem
    umount /mnt/data
              │
         ¿Falló?
              │
         Sí   │   No
              │    └──► Ir a reparación
              ▼
    Identificar procesos
    lsof /mnt/data
    fuser -vm /mnt/data
    Terminar procesos, reintentar umount
              │
              ▼
    ╔═══════════════════════╗
    ║  REPARACIÓN           ║
    ╠═══════════════════════╣
    ║                       ║
    ║  ext4:                ║
    ║    e2fsck -n (dry)    ║
    ║    e2fsck -y (repair) ║
    ║                       ║
    ║  XFS:                 ║
    ║    mount+umount       ║
    ║    xfs_repair -n      ║
    ║    xfs_repair         ║
    ║                       ║
    ║  Btrfs:               ║
    ║    btrfs check        ║
    ║    (restaurar snap)   ║
    ║                       ║
    ╚═══════════════════════╝
              │
              ▼
    Remontar y verificar
    mount /dev/vdb1 /mnt/data
    ls /mnt/data/lost+found/
```

### Reparación del rootfs (no se puede desmontar)

Si el rootfs está corrupto, no puedes desmontarlo desde el sistema en ejecución. Opciones:

```
Opción 1: Boot en modo de emergencia
─────────────────────────────────────
  En GRUB: editar línea kernel, añadir:
    systemd.unit=emergency.target

  En emergency shell:
    # rootfs montado como ro
    fsck -y /dev/vda2
    reboot

Opción 2: Boot desde ISO/USB live
──────────────────────────────────
  Arrancar desde medio externo
  fsck /dev/vda2        ← no está montado
  reboot

Opción 3: Remontar rootfs como solo-lectura
────────────────────────────────────────────
  mount -o remount,ro /
  fsck -y /dev/vda2     ← ahora es seguro (ro)
  mount -o remount,rw /
  # Solo si no hay procesos escribiendo
```

---

## Errores comunes

### 1. Ejecutar fsck en un filesystem montado

```bash
# ✗ PELIGROSO — puede causar más corrupción
fsck /dev/vdb1
# e2fsck: Cannot continue, aborting.
# (e2fsck detecta el montaje y aborta, pero no todos los FS lo hacen)

# ✓ Siempre desmontar primero
umount /mnt/data
fsck /dev/vdb1
```

### 2. Usar xfs_repair sin replayar el journal

```bash
# ✗ Intentar reparar con dirty log
xfs_repair /dev/vdb1
# ERROR: ...metadata changes in a log which needs to be replayed...

# ✗ Saltar directo a -L sin intentar mount+umount
xfs_repair -L /dev/vdb1    # pierde operaciones pendientes

# ✓ Primero intentar replayar el journal
mount /dev/vdb1 /mnt/temp && umount /mnt/temp
xfs_repair /dev/vdb1
# Solo si mount falla → xfs_repair -L
```

### 3. Confiar en fsck.xfs o fsck.btrfs

```bash
# ✗ Pensar que fsck verificó XFS
fsck /dev/vdb1    # silenciosamente no hizo nada si es XFS

# ✓ Verificar que tipo de FS es y usar la herramienta correcta
blkid /dev/vdb1
# TYPE="xfs"
xfs_repair -n /dev/vdb1
```

### 4. No hacer dry run antes de reparar

```bash
# ✗ Reparar directamente sin saber el alcance del daño
e2fsck -y /dev/vdb1

# ✓ Primero ver qué haría
e2fsck -n /dev/vdb1    # reporta sin cambiar nada
# Si los errores son manejables:
e2fsck -y /dev/vdb1
```

### 5. Olvidar pass=0 para XFS/Btrfs en fstab

```bash
# ✗ pass=2 para XFS — fsck.xfs es no-op, pero systemd esperará
UUID=...   /data   xfs   defaults   0   2

# ✓ pass=0 — no intentar verificación automática
UUID=...   /data   xfs   defaults   0   0
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│                 Filesystem check — Referencia rápida             │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  REGLA #1: SIEMPRE desmontar antes de verificar                  │
│                                                                  │
│  ext4:                                                           │
│    e2fsck -n /dev/X          dry run (solo reportar)             │
│    e2fsck -f /dev/X          forzar verificación completa        │
│    e2fsck -y /dev/X          reparar automáticamente             │
│    e2fsck -b 32768 /dev/X    usar superbloque de respaldo        │
│    dumpe2fs /dev/X | grep superblock   listar superbloques       │
│                                                                  │
│  XFS:                                                            │
│    xfs_repair -n /dev/X      dry run                             │
│    xfs_repair /dev/X         reparar                             │
│    mount+umount primero      replayar journal antes de repair    │
│    xfs_repair -L /dev/X      descartar log (último recurso)     │
│    fsck.xfs → NO-OP          no usar                             │
│                                                                  │
│  Btrfs:                                                          │
│    btrfs check /dev/X        verificar (solo lectura)            │
│    btrfs scrub start /mnt/X  verificar checksums (montado)       │
│    btrfs check --repair      EXPERIMENTAL — evitar               │
│                                                                  │
│  DIAGNÓSTICO:                                                    │
│    dmesg | grep -i ext4      errores del kernel                  │
│    tune2fs -l /dev/X         estado ext4 (clean/not clean)       │
│    blkid /dev/X              identificar tipo de FS              │
│                                                                  │
│  FSTAB pass (campo 6):                                           │
│    1 = rootfs    2 = otros ext4    0 = XFS/Btrfs/swap/NFS       │
│                                                                  │
│  FLUJO:                                                          │
│    detectar problema → desmontar → dry run (-n) → reparar →     │
│    remontar → verificar lost+found                               │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Verificar y reparar ext4

En tu VM de lab con `/dev/vdb1` formateado como ext4:

1. Monta el filesystem en `/mnt/data` y crea algunos archivos de prueba
2. Desmonta: `umount /mnt/data`
3. Verifica el estado: `tune2fs -l /dev/vdb1 | grep -i state`
4. Ejecuta un dry run: `e2fsck -n /dev/vdb1` — ¿qué reporta en cada fase?
5. Fuerza una verificación completa: `e2fsck -f /dev/vdb1`
6. Lista los superbloques de respaldo: `dumpe2fs /dev/vdb1 | grep -i superblock`
7. Remonta y verifica que los archivos siguen intactos

> **Pregunta de reflexión**: ¿Por qué `e2fsck` sin `-f` puede responder "clean" sin verificar las 5 fases? ¿Qué mecanismo usa ext4 para saber si fue desmontado correctamente?

### Ejercicio 2: Simular dirty log en XFS

En tu VM de lab con `/dev/vdc1` formateado como XFS:

1. Monta en `/mnt/xfstest` y escribe datos: `dd if=/dev/urandom of=/mnt/xfstest/data bs=1M count=10`
2. Simula un apagado abrupto — **no desmontes**, reinicia la VM directamente:
   ```bash
   echo b > /proc/sysrq-trigger   # reinicio inmediato sin umount
   ```
3. Tras reiniciar, intenta `xfs_repair /dev/vdc1` — ¿qué error ves?
4. Sigue el flujo correcto: `mount /dev/vdc1 /mnt/temp && umount /mnt/temp`
5. Ahora ejecuta `xfs_repair -n /dev/vdc1` — ¿hay errores?
6. Si los hay, repara con `xfs_repair /dev/vdc1`
7. Monta y verifica los datos

> **Pregunta de reflexión**: ¿Por qué `xfs_repair` exige que replays el journal antes de reparar, en vez de hacerlo automáticamente? ¿Qué riesgo habría si descartara el log sin avisar?

### Ejercicio 3: Comparar comportamiento de fsck por filesystem

1. Crea tres particiones en discos de lab: una ext4, una XFS, una Btrfs (si está disponible)
2. Ejecuta `fsck` genérico en cada una (desmontadas):
   ```bash
   fsck /dev/vdb1    # ext4
   fsck /dev/vdc1    # XFS
   fsck /dev/vdd1    # Btrfs
   ```
3. Observa la salida de cada uno: ¿cuál ejecutó verificación real y cuál no?
4. Ahora ejecuta la herramienta nativa de cada uno:
   ```bash
   e2fsck -f /dev/vdb1
   xfs_repair -n /dev/vdc1
   btrfs check /dev/vdd1
   ```
5. Compara las salidas: ¿qué información da cada herramienta?
6. Verifica `ls -la /sbin/fsck.*` — ¿qué wrappers existen en tu sistema?

> **Pregunta de reflexión**: Si estás escribiendo un script de mantenimiento que debe verificar cualquier filesystem, ¿puedes confiar en `fsck` genérico? ¿Qué estrategia usarías para manejar ext4, XFS y Btrfs correctamente?
