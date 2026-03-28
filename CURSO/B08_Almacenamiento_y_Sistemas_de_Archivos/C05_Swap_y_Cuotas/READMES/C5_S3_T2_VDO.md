# VDO (Virtual Data Optimizer): Deduplicación y Compresión en Línea

## Índice

1. [Qué es VDO](#1-qué-es-vdo)
2. [Arquitectura interna](#2-arquitectura-interna)
3. [Instalación](#3-instalación)
4. [Crear y gestionar volúmenes VDO](#4-crear-y-gestionar-volúmenes-vdo)
5. [Filesystem sobre VDO](#5-filesystem-sobre-vdo)
6. [Estadísticas y monitorización](#6-estadísticas-y-monitorización)
7. [Casos de uso ideales](#7-casos-de-uso-ideales)
8. [VDO con LVM (lvm-vdo)](#8-vdo-con-lvm-lvm-vdo)
9. [Persistencia con fstab](#9-persistencia-con-fstab)
10. [Destrucción y limpieza](#10-destrucción-y-limpieza)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. Qué es VDO

VDO (Virtual Data Optimizer) es una capa de almacenamiento desarrollada por Red Hat que
aplica **deduplicación** y **compresión** de forma transparente e inline (en tiempo real,
antes de escribir al disco).

```
Sin VDO:                              Con VDO:
┌────────────────────┐                ┌────────────────────┐
│  App escribe 100 GB │                │  App escribe 100 GB │
│  de datos           │                │  de datos           │
└────────┬───────────┘                └────────┬───────────┘
         │                                     │
         ▼                                     ▼
┌────────────────────┐                ┌────────────────────┐
│  Disco: 100 GB      │                │  VDO deduplicación  │
│  ocupados            │                │  + compresión       │
└────────────────────┘                ├────────────────────┤
                                      │  Disco: ~30 GB      │
                                      │  ocupados            │
                                      └────────────────────┘
```

### Deduplicación vs compresión

Son dos técnicas distintas que VDO aplica en secuencia:

```
Datos entrantes
      │
      ▼
┌──────────────────┐
│  DEDUPLICACIÓN    │  ¿Este bloque ya existe?
│                    │  Sí → almacenar solo una referencia
│  Índice UDS        │  No → pasar al siguiente paso
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  COMPRESIÓN       │  Comprimir el bloque con LZ4
│                    │  (solo bloques nuevos/únicos)
│  Packer            │
└────────┬─────────┘
         │
         ▼
    Disco físico
```

**Deduplicación**: si escribes el mismo bloque 10 veces, VDO lo almacena una sola vez y
mantiene 10 referencias. El índice UDS (Universal Deduplication Service) lleva el
registro de hashes de bloques.

**Compresión**: los bloques únicos se comprimen con LZ4 antes de escribirlos al disco.
LZ4 prioriza velocidad sobre ratio de compresión.

### Thin provisioning en VDO

VDO también hace thin provisioning: puedes presentar un tamaño lógico mayor que el
tamaño físico del disco subyacente, confiando en que la deduplicación y compresión
reducirán el consumo real.

---

## 2. Arquitectura interna

```
┌─────────────────────────────────────────────────┐
│              Aplicación / Filesystem             │
├─────────────────────────────────────────────────┤
│                /dev/mapper/vdo0                   │  ← Volumen virtual
├─────────────────────────────────────────────────┤
│                   VDO module                      │
│  ┌──────────┐  ┌───────────┐  ┌──────────────┐  │
│  │   UDS     │  │  Packer    │  │  Block Map    │  │
│  │  (dedup   │  │  (LZ4      │  │  (logical →   │  │
│  │   index)  │  │   compress) │  │   physical)   │  │
│  └──────────┘  └───────────┘  └──────────────┘  │
├─────────────────────────────────────────────────┤
│              Block device físico                  │
│              /dev/vdb (o /dev/md0, LV...)         │
└─────────────────────────────────────────────────┘
```

### Componentes internos

| Componente | Función |
|---|---|
| **UDS index** | Índice de hashes para detectar bloques duplicados. Consume RAM (~1.75 GiB por cada TiB de datos indexados con modo `sparse`) |
| **Packer** | Comprime bloques con LZ4, empaqueta múltiples bloques comprimidos en un solo bloque físico de 4 KiB |
| **Block Map** | Mapeo de direcciones lógicas a físicas (similar al thin provisioning de Device Mapper) |
| **Recovery journal** | Diario de escrituras pendientes para recuperación tras fallos |

### Tamaño lógico vs físico

```
Tamaño físico: capacidad real del disco subyacente (/dev/vdb = 50 GiB)
                │
                ▼
        ┌───────────────┐
        │     VDO        │  dedup + compresión + thin provisioning
        └───────┬───────┘
                │
                ▼
Tamaño lógico: lo que ven las aplicaciones (puede ser 150 GiB)
```

Red Hat recomienda ratios de tamaño lógico según el tipo de carga:

| Tipo de datos | Ratio lógico:físico recomendado |
|---|---|
| Contenedores/VMs (imágenes similares) | 10:1 |
| Backups con versionado | 3:1 |
| Datos mixtos/generales | 1.5:1 |
| Datos ya comprimidos (video, JPEG) | 1:1 (VDO no ayuda) |

---

## 3. Instalación

### Herramienta VDO clásica (RHEL 8, Fedora antigua)

```bash
sudo dnf install vdo kmod-kvdo
```

- `vdo`: herramienta de línea de comandos
- `kmod-kvdo`: módulo del kernel (en kernels más recientes ya está integrado)

### VDO integrado en LVM (RHEL 9+, Fedora reciente)

En RHEL 9 y Fedora reciente, VDO se integra directamente en LVM como un tipo de
volumen lógico. No se necesita el paquete `vdo` independiente:

```bash
# Solo necesitas LVM (ya instalado normalmente)
sudo dnf install lvm2
```

> **Evolución importante**: Red Hat está migrando de la herramienta `vdo` independiente
> hacia la integración con LVM (`lvcreate --type vdo`). En RHEL 9, `vdo` clásico está
> deprecated. Este tema cubre ambos métodos.

---

## 4. Crear y gestionar volúmenes VDO

### Método clásico: comando vdo (RHEL 8)

#### Crear un volumen VDO

```bash
sudo vdo create \
    --name=vdo0 \
    --device=/dev/vdb \
    --vdoLogicalSize=100G
```

Parámetros:
- `--name`: nombre del volumen (crea `/dev/mapper/vdo0`)
- `--device`: block device físico subyacente
- `--vdoLogicalSize`: tamaño lógico presentado a las aplicaciones (puede ser mayor que
  el disco físico)

Si omites `--vdoLogicalSize`, VDO usa un tamaño lógico igual al físico (ratio 1:1, sin
thin provisioning).

#### Opciones de deduplicación y compresión

Ambas están habilitadas por defecto. Puedes controlarlas:

```bash
# Crear sin compresión (solo deduplicación)
sudo vdo create --name=vdo0 --device=/dev/vdb \
    --vdoLogicalSize=100G --compression=disabled

# Crear sin deduplicación (solo compresión)
sudo vdo create --name=vdo0 --device=/dev/vdb \
    --vdoLogicalSize=100G --deduplication=disabled
```

#### Control posterior

```bash
# Deshabilitar compresión en un volumen existente
sudo vdo disableCompression --name=vdo0

# Habilitar compresión
sudo vdo enableCompression --name=vdo0

# Deshabilitar deduplicación
sudo vdo disableDeduplication --name=vdo0

# Habilitar deduplicación
sudo vdo enableDeduplication --name=vdo0
```

#### Listar volúmenes

```bash
sudo vdo list
# vdo0
```

#### Estado de un volumen

```bash
sudo vdo status --name=vdo0
```

Muestra configuración completa: device, tamaño lógico/físico, estado de compresión
y deduplicación, modo de escritura, etc.

### Arrancar y detener

```bash
sudo vdo stop --name=vdo0      # Detener (desmonta el device mapper)
sudo vdo start --name=vdo0     # Iniciar
```

---

## 5. Filesystem sobre VDO

VDO crea un block device virtual (`/dev/mapper/vdo0`). Debes crear un filesystem
encima:

### Formatear

```bash
# XFS (recomendado por Red Hat)
sudo mkfs.xfs -K /dev/mapper/vdo0

# ext4
sudo mkfs.ext4 -E nodiscard /dev/mapper/vdo0
```

> **Importante**: la opción `-K` (XFS) o `-E nodiscard` (ext4) evita que mkfs envíe
> operaciones DISCARD/TRIM a todo el volumen durante el formateo. Sin esta opción, mkfs
> intentaría descartar todo el espacio lógico (que puede ser enorme con thin
> provisioning), lo cual es extremadamente lento e innecesario.

### Montar

```bash
sudo mkdir -p /mnt/vdo-data
sudo mount /dev/mapper/vdo0 /mnt/vdo-data
```

### Habilitar DISCARD periódico

Después del montaje, VDO se beneficia de operaciones DISCARD para reclamar bloques
borrados. Hay dos opciones:

```bash
# Opción 1: montar con discard (TRIM online, pequeño impacto en rendimiento)
sudo mount -o discard /dev/mapper/vdo0 /mnt/vdo-data

# Opción 2: fstrim periódico (recomendado para producción)
sudo fstrim /mnt/vdo-data
# O habilitar el timer de systemd:
sudo systemctl enable --now fstrim.timer
```

Sin DISCARD, VDO no sabe que un bloque fue borrado por el filesystem y no puede
reclamar ese espacio físico.

---

## 6. Estadísticas y monitorización

### vdostats

El comando principal para monitorizar la eficiencia de VDO:

```bash
sudo vdostats --human-readable
```

Salida típica:

```
Device                Size      Used   Available   Use%   Space saving%
/dev/mapper/vdo0     50.0G     21.3G      28.7G    43%             62%
```

Campos clave:
- **Size**: tamaño físico del disco subyacente
- **Used**: espacio físico realmente consumido
- **Space saving%**: porcentaje de ahorro por deduplicación + compresión combinadas

### Estadísticas detalladas

```bash
sudo vdostats --verbose
```

Métricas relevantes:

```
                                   vdo0
  1K-blocks                     : 52428800     ← Tamaño físico en 1K bloques
  1K-blocks used                : 22364160     ← Espacio físico usado
  1K-blocks available           : 30064640     ← Espacio físico libre
  block size                    : 4096         ← Tamaño de bloque
  logical blocks                : 26214400     ← Bloques lógicos (tamaño virtual)
  logical blocks used           : 15728640     ← Bloques lógicos con datos
  physical blocks               : 13107200     ← Bloques físicos totales
  data blocks used              : 5590784      ← Bloques físicos con datos únicos
  dedupe advice valid           : 12582912     ← Bloques deduplicados exitosamente
  dedupe advice stale           : 0
  saving percent                : 62%
```

### Interpretar los ratios

```
Ejemplo con 10 VMs idénticas de 10 GiB cada una:

Sin VDO:
  Espacio lógico: 100 GiB
  Espacio físico: 100 GiB

Con VDO (dedup + compresión):
  Espacio lógico: 100 GiB
  Espacio físico: ~12 GiB   ← Las VMs comparten ~90% de bloques idénticos
  Saving: 88%

Cálculo:
  saving% = 1 - (data_blocks_used / logical_blocks_used) × 100
```

### Monitorización con Prometheus/Grafana

VDO expone métricas que puedes recoger periódicamente:

```bash
# Extraer saving% para un script de monitorización
sudo vdostats --verbose | grep "saving percent"
```

---

## 7. Casos de uso ideales

### Donde VDO brilla

```
╔════════════════════════════════════════════════════════════════╗
║  CASO DE USO                     RATIO DE AHORRO TÍPICO       ║
╠════════════════════════════════════════════════════════════════╣
║                                                                ║
║  Plataformas de contenedores     ████████████░░  80-90%        ║
║  (imágenes Docker con capas                                    ║
║   base similares)                                              ║
║                                                                ║
║  Virtualización                  ████████████░░  70-85%        ║
║  (múltiples VMs con el mismo                                   ║
║   SO base)                                                     ║
║                                                                ║
║  Backups con versionado          ████████░░░░░░  50-70%        ║
║  (cambios incrementales                                        ║
║   entre versiones)                                             ║
║                                                                ║
║  Almacenamiento de logs          ██████░░░░░░░░  40-60%        ║
║  (patrones repetitivos en                                      ║
║   texto)                                                       ║
║                                                                ║
║  Datos mixtos de oficina         ████░░░░░░░░░░  30-50%        ║
║  (documentos, código fuente)                                   ║
║                                                                ║
║  Multimedia (video, imágenes)    █░░░░░░░░░░░░░  0-10%         ║
║  (ya comprimido, pocas                                         ║
║   duplicaciones)                                               ║
║                                                                ║
╚════════════════════════════════════════════════════════════════╝
```

### Donde VDO NO es útil

- **Datos ya comprimidos**: JPEG, MP4, ZIP — la compresión LZ4 no reduce su tamaño
- **Datos completamente únicos**: cifrados (LUKS sobre VDO es contraproducente) o
  aleatorios
- **Cargas con latencia ultra-baja**: VDO añade overhead por la deduplicación/compresión
  (aunque LZ4 es rápido, no es gratis)
- **Bases de datos transaccionales**: el overhead puede afectar la latencia de I/O

### Consideraciones de recursos

VDO consume recursos adicionales:

| Recurso | Consumo |
|---|---|
| **RAM** | ~1.75 GiB por cada TiB de datos indexados (modo sparse). Modo dense: ~4.7 GiB/TiB |
| **CPU** | Cálculo de hashes (MurmurHash3) + compresión LZ4 por cada escritura |
| **Almacenamiento** | El índice UDS y metadata consumen parte del disco físico (~2-3 GiB) |

---

## 8. VDO con LVM (lvm-vdo)

En RHEL 9+ y Fedora reciente, la forma recomendada de usar VDO es a través de LVM.

### Crear un VDO pool con LVM

```bash
# 1. Crear PV y VG normalmente
sudo pvcreate /dev/vdb
sudo vgcreate vdo_vg /dev/vdb

# 2. Crear un LV de tipo VDO pool + LV de datos en un solo comando
sudo lvcreate --type vdo \
    --name vdo_lv \
    --size 50G \
    --virtualsize 150G \
    vdo_vg
```

Parámetros:
- `--type vdo`: indica que es un volumen VDO
- `--size`: tamaño físico del pool VDO
- `--virtualsize`: tamaño lógico presentado (thin provisioning)

Esto crea internamente:
- Un **VDO pool LV** (gestiona dedup/compresión)
- Un **VDO LV** (`/dev/vdo_vg/vdo_lv`) que es el dispositivo usable

### Verificar

```bash
sudo lvs -a -o+vdo_operating_mode,vdo_compression_state,vdo_index_state vdo_vg
```

```
  LV             VG      Attr       LSize    Pool        Origin  Data%  Meta%  Move  Log  Cpy%Sync  Convert  VDOOperatingMode  VDOCompressionState  VDOIndexState
  vdo_lv         vdo_vg  vwi-a-v--- 150.00g  vpool0                                                          normal            online               online
  vpool0         vdo_vg  dwi------- 50.00g                        3.01
```

### Formatear y montar

```bash
sudo mkfs.xfs -K /dev/vdo_vg/vdo_lv
sudo mkdir -p /mnt/vdo-lvm
sudo mount /dev/vdo_vg/vdo_lv /mnt/vdo-lvm
```

### Estadísticas con LVM

```bash
# Uso del pool VDO
sudo lvs -o+vdo_saving_percent vdo_vg
```

### Ventajas de lvm-vdo sobre vdo clásico

| Aspecto | vdo clásico | lvm-vdo |
|---|---|---|
| Gestión | Herramienta `vdo` separada | Integrado en `lvm2` |
| Snapshots | No directamente | Snapshots LVM compatibles |
| Redimensionamiento | Complejo | `lvextend` / `lvresize` estándar |
| Futuro | Deprecated en RHEL 9 | Método recomendado |
| Combinar con RAID | Manual (mdadm debajo) | LVM RAID nativo |

---

## 9. Persistencia con fstab

### VDO clásico

El servicio `vdo` arranca los volúmenes automáticamente al boot (configuración en
`/etc/vdoconf.yml`). Para fstab:

```
# VDO clásico — el servicio vdo crea /dev/mapper/vdo0 al arrancar
/dev/mapper/vdo0  /mnt/vdo-data  xfs  defaults,x-systemd.requires=vdo.service,discard  0  0
```

La opción `x-systemd.requires=vdo.service` asegura que systemd espera a que VDO
arranque el volumen antes de intentar montarlo.

Alternativa con UUID (más robusto):

```bash
sudo blkid /dev/mapper/vdo0
# UUID="xxxxxxxx-xxxx-..."
```

```
UUID=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx  /mnt/vdo-data  xfs  defaults,x-systemd.requires=vdo.service,discard  0  0
```

### lvm-vdo (RHEL 9+)

Con LVM-VDO no necesitas dependencias especiales de systemd — LVM se activa
automáticamente durante el arranque temprano:

```
/dev/vdo_vg/vdo_lv  /mnt/vdo-lvm  xfs  defaults,discard  0  0
```

O con UUID:

```
UUID=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx  /mnt/vdo-lvm  xfs  defaults,discard  0  0
```

---

## 10. Destrucción y limpieza

### VDO clásico

```bash
# 1. Desmontar
sudo umount /mnt/vdo-data

# 2. Detener y eliminar el volumen
sudo vdo stop --name=vdo0
sudo vdo remove --name=vdo0

# 3. Limpiar fstab
sudo vi /etc/fstab   # Eliminar línea de VDO
```

### lvm-vdo

```bash
# 1. Desmontar
sudo umount /mnt/vdo-lvm

# 2. Eliminar LV (elimina también el pool VDO)
sudo lvremove vdo_vg/vdo_lv

# 3. Opcionalmente destruir VG y PV
sudo vgremove vdo_vg
sudo pvremove /dev/vdb

# 4. Limpiar fstab
sudo vi /etc/fstab
```

---

## 11. Errores comunes

### Error 1: Formatear sin -K o -E nodiscard

```bash
sudo mkfs.xfs /dev/mapper/vdo0
# Tarda eternamente o parece colgado
```

mkfs intenta enviar DISCARD a los 100+ GiB del tamaño lógico, lo cual es
extremadamente lento sobre VDO.

**Solución**: siempre usar `-K` para XFS o `-E nodiscard` para ext4:

```bash
sudo mkfs.xfs -K /dev/mapper/vdo0
sudo mkfs.ext4 -E nodiscard /dev/mapper/vdo0
```

### Error 2: Overprovisioning excesivo sin monitorización

```bash
# Pool físico: 50 GiB, tamaño lógico: 500 GiB (ratio 10:1)
# Pero los datos no se deduplican bien...
sudo vdostats --human-readable
# Space saving%: 5%   ← Los datos son mayormente únicos
```

El pool se llena mucho antes de lo esperado. Las escrituras fallan con errores de I/O.

**Solución**: monitorizar `vdostats` regularmente y ajustar el ratio lógico:físico
según los datos reales, no según estimaciones optimistas.

### Error 3: Usar VDO sobre datos cifrados

```
LUKS → VDO: ✗ Inútil
   Los datos cifrados parecen aleatorios → dedup 0%, compresión 0%

VDO → LUKS: ✗ También inútil
   LUKS cifra los bloques antes de que VDO los vea → mismo resultado

Correcto: o cifras o deduplicas, rara vez ambos
```

### Error 4: No reclamar espacio con DISCARD/fstrim

```bash
# Borraste 20 GiB de archivos pero vdostats no muestra cambio
rm -rf /mnt/vdo-data/old-backups/
sudo vdostats --human-readable
# Used: sin cambios   ← VDO no sabe que esos bloques se liberaron
```

**Solución**: ejecutar `fstrim` o montar con `discard`:

```bash
sudo fstrim -v /mnt/vdo-data
# O habilitar el timer:
sudo systemctl enable --now fstrim.timer
```

### Error 5: Ignorar el consumo de RAM del índice UDS

```bash
# Servidor con 4 GiB de RAM total intenta indexar 2 TiB de datos
# UDS necesita ~3.5 GiB solo para el índice → OOM killer
```

**Solución**: verificar la memoria disponible antes de crear el volumen. Usar el modo
`sparse` del índice (1.75 GiB/TiB) en lugar de `dense` (4.7 GiB/TiB):

```bash
# VDO clásico: sparse es el default
sudo vdo create --name=vdo0 --device=/dev/vdb \
    --vdoLogicalSize=100G --indexMem=0.25

# lvm-vdo: configurar durante creación
sudo lvcreate --type vdo --name vdo_lv --size 50G \
    --virtualsize 150G --config 'allocation/vdo_use_sparse_index=1' vdo_vg
```

---

## 12. Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║                         VDO — REFERENCIA RÁPIDA                      ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                      ║
║  VDO CLÁSICO (RHEL 8)                                                ║
║  dnf install vdo kmod-kvdo                                           ║
║  vdo create --name=X --device=D --vdoLogicalSize=SG                  ║
║  vdo list                                Listar volúmenes            ║
║  vdo status --name=X                     Estado detallado            ║
║  vdo start/stop --name=X                 Iniciar/detener             ║
║  vdo remove --name=X                     Eliminar volumen            ║
║                                                                      ║
║  COMPRESIÓN Y DEDUPLICACIÓN                                          ║
║  vdo enableCompression --name=X          Activar compresión          ║
║  vdo disableCompression --name=X         Desactivar compresión       ║
║  vdo enableDeduplication --name=X        Activar deduplicación       ║
║  vdo disableDeduplication --name=X       Desactivar deduplicación    ║
║                                                                      ║
║  LVM-VDO (RHEL 9+, FEDORA)                                          ║
║  lvcreate --type vdo --name X            Crear VDO pool + LV         ║
║    --size SG --virtualsize VG VG_NAME                                ║
║  lvs -o+vdo_saving_percent VG            Estadísticas de ahorro      ║
║  lvremove VG/LV                          Eliminar                    ║
║                                                                      ║
║  ESTADÍSTICAS                                                        ║
║  vdostats --human-readable               Resumen de uso y ahorro     ║
║  vdostats --verbose                      Métricas detalladas         ║
║                                                                      ║
║  FORMATEAR (¡IMPORTANTE!)                                            ║
║  mkfs.xfs -K /dev/mapper/X              XFS sin DISCARD masivo       ║
║  mkfs.ext4 -E nodiscard /dev/mapper/X   ext4 sin DISCARD masivo      ║
║                                                                      ║
║  FSTAB                                                               ║
║  Clásico: UUID=xxx /mnt xfs defaults,x-systemd.requires=            ║
║                         vdo.service,discard 0 0                      ║
║  LVM-VDO: UUID=xxx /mnt xfs defaults,discard 0 0                    ║
║                                                                      ║
║  RECLAMAR ESPACIO                                                    ║
║  fstrim /mnt/vdo-data                   TRIM manual                  ║
║  systemctl enable --now fstrim.timer    TRIM periódico               ║
║                                                                      ║
║  CONSUMO DE RAM (índice UDS)                                         ║
║  Sparse (default): ~1.75 GiB por TiB indexado                        ║
║  Dense:            ~4.7 GiB por TiB indexado                         ║
║                                                                      ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 13. Ejercicios

### Ejercicio 1: VDO básico con deduplicación

En tu VM con un disco virtual libre (`/dev/vdb` de al menos 10 GiB):

1. Instala los paquetes de VDO (usa el método apropiado para tu distro)
2. Crea un volumen VDO:
   ```bash
   sudo vdo create --name=testvdo --device=/dev/vdb --vdoLogicalSize=30G
   ```
3. Formatea con XFS (recuerda la opción correcta para evitar DISCARD lento)
4. Monta en `/mnt/vdo-test`
5. Revisa las estadísticas iniciales con `vdostats --human-readable`
6. Crea un archivo de 500 MiB con datos aleatorios:
   ```bash
   dd if=/dev/urandom of=/mnt/vdo-test/random.bin bs=1M count=500
   ```
7. Revisa `vdostats` — ¿cuánto ahorro muestra?
8. Ahora copia ese mismo archivo 5 veces:
   ```bash
   for i in $(seq 1 5); do cp /mnt/vdo-test/random.bin /mnt/vdo-test/copy_$i.bin; done
   ```
9. Ejecuta `sync` y revisa `vdostats` de nuevo

**Pregunta de predicción**: después de crear 5 copias de un archivo de 500 MiB, ¿cuánto
espacio físico adicional esperas que consuma VDO? ¿Por qué?

**Pregunta de reflexión**: los datos aleatorios (`/dev/urandom`) no son compresibles.
¿Qué componente de VDO (deduplicación o compresión) es el que ahorra espacio en este
caso? ¿Qué pasaría si los archivos tuvieran contenido distinto pero compresible (por
ejemplo, archivos de texto)?

### Ejercicio 2: Monitorización y overprovisioning

Continuando con el volumen del ejercicio anterior:

1. Observa el ratio de ahorro actual con `vdostats --verbose`
2. Anota los valores de `logical blocks used` y `data blocks used`
3. Calcula manualmente el `saving percent`:
   ```
   saving% = (1 - data_blocks_used / logical_blocks_used) × 100
   ```
4. Verifica que coincide con lo que reporta `vdostats`
5. Ahora genera datos completamente únicos (sin posibilidad de dedup):
   ```bash
   dd if=/dev/urandom of=/mnt/vdo-test/unique1.bin bs=1M count=200
   dd if=/dev/urandom of=/mnt/vdo-test/unique2.bin bs=1M count=200
   ```
6. Revisa cómo cambia el `saving percent` con datos que no se deduplican

**Pregunta de reflexión**: si configuras un ratio lógico:físico de 10:1 pero tus datos
tienen solo un 20% de deduplicación, ¿qué problema anticipas? ¿Cómo lo detectarías
antes de que cause errores de I/O?

### Ejercicio 3: Limpieza y DISCARD

1. Verifica el espacio físico usado con `vdostats --human-readable`
2. Borra todos los archivos de `/mnt/vdo-test/`
3. Revisa `vdostats` de nuevo — ¿cambió el espacio físico usado?
4. Ejecuta `fstrim -v /mnt/vdo-test/` y revisa `vdostats` otra vez
5. Compara el espacio físico antes y después de fstrim

**Pregunta de predicción**: antes de ejecutar fstrim, ¿esperas que VDO recupere el
espacio automáticamente? ¿Por qué sí o por qué no?

6. Limpieza final:
   - Desmonta el filesystem
   - Elimina el volumen VDO
   - Elimina la entrada de fstab si la creaste

**Pregunta de reflexión**: ¿Por qué un filesystem no puede comunicar automáticamente
a VDO que un bloque fue liberado sin usar DISCARD/TRIM? Piensa en las capas de
abstracción entre el filesystem y el block device.
