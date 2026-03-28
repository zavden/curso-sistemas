# Imágenes backing (overlays)

## Índice

1. [El concepto de backing](#el-concepto-de-backing)
2. [Crear un overlay](#crear-un-overlay)
3. [Anatomía interna de un overlay](#anatomía-interna-de-un-overlay)
4. [Patrón de uso en labs](#patrón-de-uso-en-labs)
5. [Cadenas de backing](#cadenas-de-backing)
6. [Commit: fusionar overlay en base](#commit-fusionar-overlay-en-base)
7. [Rebase: cambiar la imagen base](#rebase-cambiar-la-imagen-base)
8. [Operaciones avanzadas con cadenas](#operaciones-avanzadas-con-cadenas)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## El concepto de backing

En el tema anterior vimos que qcow2 soporta copy-on-write internamente a nivel
de cluster. Las imágenes backing llevan ese concepto un paso más allá: separan
la **lectura** de la **escritura** entre dos archivos distintos.

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│  OVERLAY (read-write)                                       │
│  overlay.qcow2  — solo almacena las diferencias             │
│       │                                                     │
│       │ backing_file = base.qcow2                           │
│       ▼                                                     │
│  BASE (read-only)                                           │
│  base.qcow2 — imagen original, intacta                     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

El mecanismo funciona así:

1. **Lectura**: QEMU busca el sector en el overlay. Si el cluster está
   asignado allí, devuelve esos datos. Si no, desciende al backing file y lee
   de la base.

2. **Escritura**: QEMU **siempre** escribe en el overlay. Nunca toca la base.
   La primera escritura a un cluster que todavía apunta a la base dispara una
   **copia del cluster** al overlay (COW), y luego escribe el dato modificado.

3. **Resultado**: la base permanece intacta. Toda divergencia vive en el
   overlay.

```
Flujo de lectura:

  Guest lee sector 1000
       │
       ▼
  ¿Sector 1000 asignado en overlay?
       │
    Sí │              No
       │               │
       ▼               ▼
  Leer de            Leer de
  overlay           base (backing)


Flujo de escritura:

  Guest escribe sector 1000
       │
       ▼
  ¿Cluster ya existe en overlay?
       │
    Sí │              No
       │               │
       ▼               ▼
  Escribir         Asignar cluster
  directamente     nuevo en overlay
                       │
                       ▼
                   Escribir dato
                   en overlay
```

> **Predicción**: si creas un overlay de una base de 8 GB y solo modificas
> 100 MB de datos, el overlay ocupará aproximadamente 100 MB en disco (más
> metadata), no 8 GB. La base sigue ocupando sus 8 GB originales, pero se
> comparte entre todos los overlays que la usen.

---

## Crear un overlay

La sintaxis fundamental:

```bash
qemu-img create -f qcow2 -b base.qcow2 -F qcow2 overlay.qcow2
```

Los flags:

| Flag | Significado |
|------|-------------|
| `-f qcow2` | Formato del **overlay** (siempre qcow2 para backing) |
| `-b base.qcow2` | Ruta al **backing file** |
| `-F qcow2` | Formato del **backing file** (obligatorio desde QEMU 6.1) |

> **Nota importante sobre `-F`**: en versiones anteriores de QEMU, omitir `-F`
> hacía que QEMU "adivinara" el formato del backing file. Esto se deshabilitó
> por seguridad: un atacante podría renombrar un archivo raw como `.qcow2` y
> explotar la detección automática. Siempre especifica `-F`.

### Ejemplo completo paso a paso

```bash
# 1. Crear una imagen base (o usar una existente)
qemu-img create -f qcow2 base.qcow2 20G

# 2. Instalar un sistema operativo en la base
#    (normalmente con virt-install, lo veremos en S04)
#    ... instalación completa ...

# 3. Verificar la base
qemu-img info base.qcow2

# 4. Crear overlay que apunte a la base
qemu-img create -f qcow2 -b base.qcow2 -F qcow2 overlay.qcow2
```

Salida esperada del paso 4:

```
Formatting 'overlay.qcow2', fmt=qcow2 cluster_size=65536
backing_file=base.qcow2 backing_fmt=qcow2
size=21474836480 lazy_refcounts=off refcount_bits=16
```

Observa que **no especificamos tamaño** en el overlay: hereda el tamaño
virtual de la base automáticamente. Si quisieras un overlay con un disco
más grande (para luego hacer `growpart` dentro del guest), puedes indicar
un tamaño explícito mayor:

```bash
qemu-img create -f qcow2 -b base.qcow2 -F qcow2 overlay.qcow2 40G
```

### Verificar el overlay

```bash
qemu-img info overlay.qcow2
```

```
image: overlay.qcow2
file format: qcow2
virtual size: 20 GiB (21474836480 bytes)
disk size: 196 KiB              <-- ¡casi vacío!
cluster_size: 65536
backing file: base.qcow2        <-- referencia a la base
backing file format: qcow2
Format specific information:
    compat: 1.1
    compression type: zlib
    lazy refcounts: false
    refcount bits: 16
    corrupt: false
    extended l2: false
```

El overlay recién creado ocupa ~196 KiB. Solo contiene la header qcow2 y la
tabla L1, sin datos de usuario.

### Rutas absolutas vs relativas

El backing file se almacena como una ruta en la header del overlay:

```bash
# Ruta relativa (por defecto) — portátil si mueves ambos archivos juntos
qemu-img create -f qcow2 -b base.qcow2 -F qcow2 overlay.qcow2

# Ruta absoluta — funciona desde cualquier directorio de trabajo
qemu-img create -f qcow2 -b /var/lib/libvirt/images/base.qcow2 \
    -F qcow2 overlay.qcow2
```

Para ver qué ruta almacenó:

```bash
qemu-img info overlay.qcow2 | grep "backing file:"
# backing file: base.qcow2                          (relativa)
# backing file: /var/lib/libvirt/images/base.qcow2  (absoluta)
```

> **Predicción**: si mueves el overlay a otro directorio sin mover la base, y
> la ruta era relativa, QEMU no encontrará el backing file y la VM no
> arrancará. Obtendrás un error tipo `Could not open backing file`.

---

## Anatomía interna de un overlay

Internamente el overlay qcow2 tiene la misma estructura de clusters y tablas
L1/L2 que vimos en T01, pero con campos adicionales en la header:

```
┌─────────────────────────────────────────────────────┐
│  QCOW2 HEADER (overlay.qcow2)                      │
│                                                     │
│  magic: 0x514649fb ("QFI\xfb")                      │
│  version: 3                                         │
│  backing_file_offset: 0x90    ◄── offset en header  │
│  backing_file_size: 10        ◄── longitud string   │
│  ...                                                │
│  [offset 0x90]: "base.qcow2" ◄── la ruta guardada  │
│                                                     │
├─────────────────────────────────────────────────────┤
│  L1 TABLE                                           │
│  [0] → 0  (no asignado, leer de backing)            │
│  [1] → 0  (no asignado, leer de backing)            │
│  [2] → offset_cluster_N (asignado, dato local)      │
│  [3] → 0  (no asignado, leer de backing)            │
│  ...                                                │
├─────────────────────────────────────────────────────┤
│  CLUSTERS DE DATOS                                  │
│  Solo los clusters que el overlay ha escrito         │
│  (todo lo demás se lee de la base)                  │
└─────────────────────────────────────────────────────┘
```

La regla es sencilla: una entrada L1/L2 con valor **0** significa "no hay dato
aquí, buscar en el backing". Cualquier otro valor apunta a un cluster local
en el overlay.

---

## Patrón de uso en labs

Este es el uso más poderoso de las imágenes backing para nuestro curso: una
**base instalada una vez** y **múltiples overlays desechables** para distintos
ejercicios.

### Escenario: lab de filesystems

```
                        base_debian.qcow2
                    (Debian 12 instalado, 4.2 GB)
                    ┌───────────┴───────────┐
                    │                       │
                    ▼                       ▼
           lab_ext4.qcow2           lab_xfs.qcow2
           (ejercicio ext4)         (ejercicio xfs)
           overlay: ~50 MB          overlay: ~50 MB

Total en disco: 4.2 GB + 50 MB + 50 MB = 4.3 GB
Sin overlays:   4.2 GB × 3 copias     = 12.6 GB  ← ¡desperdicio!
```

### Flujo de trabajo

```bash
# 1. Preparar la base (una sola vez, puede tardar)
qemu-img create -f qcow2 base_debian.qcow2 20G
# ... instalar Debian completo en esta imagen ...

# 2. Marcar la base como read-only (protección adicional)
chmod 444 base_debian.qcow2

# 3. Para cada ejercicio, crear un overlay instantáneo
qemu-img create -f qcow2 -b base_debian.qcow2 -F qcow2 lab_ext4.qcow2
qemu-img create -f qcow2 -b base_debian.qcow2 -F qcow2 lab_xfs.qcow2
qemu-img create -f qcow2 -b base_debian.qcow2 -F qcow2 lab_btrfs.qcow2

# 4. Arrancar la VM usando el overlay (no la base)
qemu-system-x86_64 -enable-kvm -m 2048 \
    -drive file=lab_ext4.qcow2,format=qcow2 \
    -nographic

# 5. Hacer el ejercicio dentro de la VM...
#    (formatear discos, montar, romper cosas)

# 6. Al terminar, destruir y recrear en segundos
rm lab_ext4.qcow2
qemu-img create -f qcow2 -b base_debian.qcow2 -F qcow2 lab_ext4.qcow2
# ¡VM fresca instantánea!
```

### Ventajas del patrón

| Aspecto | Sin overlays | Con overlays |
|---------|-------------|--------------|
| Espacio por VM adicional | ~4 GB (copia completa) | ~200 KB (overlay vacío) |
| Tiempo crear VM nueva | Minutos (copiar imagen) | Instantáneo (~0.1s) |
| Restaurar estado limpio | Copiar de nuevo la base | `rm overlay && create` |
| Actualizar base (apt upgrade) | Actualizar cada copia | Actualizar solo la base |

> **Advertencia**: si modificas la base **después** de crear overlays, los
> overlays se **corrompen**. La base debe ser inmutable una vez que existen
> overlays. Por eso usamos `chmod 444`.

---

## Cadenas de backing

Los overlays pueden apuntar a otros overlays, formando una **cadena**:

```
snapshot_dia3.qcow2    ← overlay activo (lectura/escritura)
       │
       │ backing
       ▼
snapshot_dia2.qcow2    ← overlay intermedio (read-only para dia3)
       │
       │ backing
       ▼
snapshot_dia1.qcow2    ← overlay intermedio (read-only para dia2)
       │
       │ backing
       ▼
base.qcow2             ← imagen base original (read-only para todos)
```

### Cómo funciona la lectura en una cadena

```
Guest lee sector 5000
       │
       ▼
¿En snapshot_dia3?  ──No──► ¿En snapshot_dia2?  ──No──► ¿En snapshot_dia1?  ──No──► Leer de base
       │                           │                           │
      Sí                          Sí                          Sí
       │                           │                           │
       ▼                           ▼                           ▼
  Devolver dato              Devolver dato              Devolver dato
```

QEMU recorre la cadena de arriba hacia abajo hasta encontrar el cluster. Si
llega a la base sin encontrarlo, lee de la base.

### Crear una cadena

```bash
# Base original
qemu-img create -f qcow2 base.qcow2 20G
# ... instalar SO ...

# Dia 1: crear primer overlay
qemu-img create -f qcow2 -b base.qcow2 -F qcow2 dia1.qcow2
# ... trabajar con la VM usando dia1.qcow2 ...

# Dia 2: crear overlay sobre dia1
qemu-img create -f qcow2 -b dia1.qcow2 -F qcow2 dia2.qcow2
# ... trabajar con la VM usando dia2.qcow2 ...

# Dia 3: crear overlay sobre dia2
qemu-img create -f qcow2 -b dia2.qcow2 -F qcow2 dia3.qcow2
# ... trabajar con la VM usando dia3.qcow2 ...
```

### Ver la cadena completa

```bash
qemu-img info --backing-chain dia3.qcow2
```

```
image: dia3.qcow2
file format: qcow2
virtual size: 20 GiB (21474836480 bytes)
disk size: 2.1 MiB
cluster_size: 65536
backing file: dia2.qcow2
backing file format: qcow2

image: dia2.qcow2
file format: qcow2
virtual size: 20 GiB (21474836480 bytes)
disk size: 148 MiB
cluster_size: 65536
backing file: dia1.qcow2
backing file format: qcow2

image: dia1.qcow2
file format: qcow2
virtual size: 20 GiB (21474836480 bytes)
disk size: 512 MiB
cluster_size: 65536
backing file: base.qcow2
backing file format: qcow2

image: base.qcow2
file format: qcow2
virtual size: 20 GiB (21474836480 bytes)
disk size: 4.2 GiB
cluster_size: 65536
```

### Evitar cadenas largas

Cada nivel adicional en la cadena añade latencia de lectura. En el peor caso
(sector solo en la base), QEMU tiene que consultar N archivos:

```
Cadena de 2 niveles:  overlay → base           (máx. 2 lecturas)
Cadena de 5 niveles:  o4 → o3 → o2 → o1 → base (máx. 5 lecturas)
Cadena de 20 niveles: desastre de rendimiento
```

**Regla práctica**: mantén las cadenas en **3 niveles o menos**. Si necesitas
más, haz `commit` o `rebase` para aplanar la cadena (secciones siguientes).

---

## Commit: fusionar overlay en base

`qemu-img commit` escribe los cambios del overlay **de vuelta** en su backing
file, fusionándolos:

```
ANTES del commit:

  overlay.qcow2  (cambios: +500 MB)
       │
       ▼
  base.qcow2    (estado original)


DESPUÉS del commit:

  overlay.qcow2  (vacío, todos los cambios ya están en base)
       │
       ▼
  base.qcow2    (estado original + cambios del overlay)
```

### Sintaxis

```bash
# VM apagada (obligatorio)
qemu-img commit overlay.qcow2
```

Salida:

```
Image committed.
```

> **Importante**: la VM **debe estar apagada**. Hacer commit de una imagen en
> uso corrompe datos. QEMU rechazará la operación si detecta que el archivo
> está bloqueado.

### Verificar el resultado

```bash
# Antes del commit
qemu-img info base.qcow2     # disk size: 4.2 GiB
qemu-img info overlay.qcow2  # disk size: 500 MiB

# Después del commit
qemu-img info base.qcow2     # disk size: 4.6 GiB (absorbió los cambios)
qemu-img info overlay.qcow2  # disk size: 196 KiB  (volvió a estar vacío)
```

### Commit en cadenas: flag `-b`

En una cadena de múltiples niveles, `commit` por defecto fusiona al **padre
inmediato**. Puedes especificar hasta qué nivel fusionar:

```
dia3.qcow2  →  dia2.qcow2  →  dia1.qcow2  →  base.qcow2
```

```bash
# Commit normal: dia3 → dia2
qemu-img commit dia3.qcow2

# Commit hasta la base: dia3 → base
qemu-img commit -b base.qcow2 dia3.qcow2
```

Con `-b`, QEMU fusiona los cambios de dia3 directamente en `base.qcow2`,
saltando los niveles intermedios. Los datos de dia2 y dia1 que dia3 no
sobreescribió **no** se tocan.

### Caso de uso típico del commit

Preparaste una base limpia, creaste un overlay, hiciste una actualización
de paquetes (`apt upgrade`) en el overlay, y quieres que esos cambios se
conviertan en la nueva base para futuros overlays:

```bash
# 1. Actualizar en overlay
qemu-img create -f qcow2 -b base_v1.qcow2 -F qcow2 temp_update.qcow2
# ... arrancar VM con temp_update.qcow2, hacer apt upgrade, apagar ...

# 2. Commit para integrar en la base
qemu-img commit temp_update.qcow2

# 3. Limpiar
rm temp_update.qcow2

# 4. Renombrar si quieres versionado
mv base_v1.qcow2 base_v2.qcow2

# 5. Crear nuevos overlays desde la base actualizada
qemu-img create -f qcow2 -b base_v2.qcow2 -F qcow2 lab_nuevo.qcow2
```

---

## Rebase: cambiar la imagen base

`qemu-img rebase` cambia el backing file de un overlay. Hay dos modos:

### Rebase unsafe (`-u`)

Cambia **solo** el puntero en la header, sin tocar datos. Es instantáneo
pero peligroso si las bases no son idénticas:

```bash
qemu-img rebase -u -b nueva_base.qcow2 -F qcow2 overlay.qcow2
```

```
ANTES:  overlay → vieja_base.qcow2
DESPUÉS: overlay → nueva_base.qcow2  (solo cambió el puntero)
```

Cuándo es seguro usar `-u`:

- Renombraste o moviste la base (mismo contenido, nueva ruta)
- Copiaste la base a otra ubicación
- Convertiste la base de un formato a otro sin alterar datos

```bash
# Ejemplo: mover la base de directorio
mv /tmp/base.qcow2 /var/lib/libvirt/images/base.qcow2

# El overlay todavía apunta a /tmp/base.qcow2 — corregir
qemu-img rebase -u -b /var/lib/libvirt/images/base.qcow2 \
    -F qcow2 overlay.qcow2
```

### Rebase safe (sin `-u`)

Compara la base vieja con la nueva y copia al overlay todos los clusters
que difieren. Esto garantiza que el overlay producirá el mismo resultado
final independientemente de la nueva base:

```bash
qemu-img rebase -b nueva_base.qcow2 -F qcow2 overlay.qcow2
```

```
ANTES:
  overlay (cambios del usuario)
       │
       ▼
  vieja_base (Debian 12.4)

DURANTE el rebase safe:
  Para cada cluster:
    vieja = leer cluster de vieja_base
    nueva = leer cluster de nueva_base
    if vieja != nueva:
        copiar cluster vieja al overlay
        (para que overlay + nueva_base = overlay + vieja_base)

DESPUÉS:
  overlay (cambios del usuario + clusters divergentes)
       │
       ▼
  nueva_base (Debian 12.5)
```

El rebase safe es **lento** (debe comparar toda la imagen) pero **seguro**:
el contenido visible del guest no cambia.

### Rebase para aplanar cadenas

El rebase es la herramienta para **eliminar niveles intermedios** de una
cadena:

```
ANTES:    dia3 → dia2 → dia1 → base

# Queremos eliminar dia2 y dia1
qemu-img rebase -b base.qcow2 -F qcow2 dia3.qcow2

DESPUÉS:  dia3 → base   (dia3 ahora contiene todo lo de dia2+dia1+dia3)
```

> **Predicción**: después del rebase safe, `dia3.qcow2` será significativamente
> más grande porque absorbió todos los clusters que antes vivían en dia2 y dia1.
> Pero la cadena es más corta y el rendimiento de lectura mejora.

---

## Operaciones avanzadas con cadenas

### Mapa de clusters: ver qué viene de dónde

```bash
qemu-img map --output=json overlay.qcow2 | head -20
```

```json
[
  { "start": 0, "length": 65536, "depth": 1, "zero": false,
    "data": true, "offset": 327680 },
  { "start": 65536, "length": 131072, "depth": 0, "zero": true,
    "data": false },
  { "start": 196608, "length": 65536, "depth": 0, "zero": false,
    "data": true, "offset": 393216 }
]
```

| Campo | Significado |
|-------|-------------|
| `depth: 0` | Dato en el overlay mismo |
| `depth: 1` | Dato en el backing file (primer nivel) |
| `depth: 2` | Dato en el segundo nivel de la cadena |
| `zero: true` | Cluster marcado como cero (no ocupa espacio) |

### Aplanar completamente: convertir overlay a imagen independiente

Si quieres eliminar toda dependencia del backing file y obtener una imagen
autónoma:

```bash
# Opción 1: convert (crea archivo nuevo)
qemu-img convert -f qcow2 -O qcow2 overlay.qcow2 standalone.qcow2

# Opción 2: rebase a "ninguna base" (modifica en sitio)
qemu-img rebase -b "" overlay.qcow2
```

Con `convert`, la imagen resultante es completamente independiente: no tiene
backing file. Contiene todos los datos (base + cambios del overlay) en un
solo archivo.

```bash
qemu-img info standalone.qcow2 | grep backing
# (sin output — no tiene backing file)
```

### Comprobar integridad de la cadena

```bash
# Verificar que todos los backing files existen y son accesibles
qemu-img check overlay.qcow2
```

Si un backing file falta o está corrupto:

```
qemu-img: Could not open backing file: Could not open 'base.qcow2':
No such file or directory
```

### Comparar overlay con su base

Para ver cuánto ha divergido un overlay de su base:

```bash
# Tamaño real del overlay = datos que difieren de la base
qemu-img info overlay.qcow2 | grep "disk size"
# disk size: 347 MiB

# Porcentaje de divergencia
qemu-img map overlay.qcow2 | grep -c "depth.*0"
# (número de clusters propios del overlay)
```

---

## Errores comunes

### 1. Modificar la base después de crear overlays

```bash
# ❌ MAL: modificar la base con overlays existentes
qemu-img create -f qcow2 -b base.qcow2 -F qcow2 overlay.qcow2
# ... crear la VM, usarla ...
qemu-img resize base.qcow2 +10G    # ¡CORRUPCIÓN!
```

El overlay almacena un hash de la base en su header. Si la base cambia, los
clusters que el overlay lee de la base serán datos diferentes de los que
esperaba. **Solución**: nunca tocar la base directamente. Usa commit o
trabaja siempre a través de overlays.

### 2. Olvidar `-F` al crear el overlay

```bash
# ❌ Versiones recientes de QEMU rechazan esto
qemu-img create -f qcow2 -b base.qcow2 overlay.qcow2
# qemu-img: overlay.qcow2: Backing format not specified and target
# does not have a default backing format

# ✅ Siempre especificar -F
qemu-img create -f qcow2 -b base.qcow2 -F qcow2 overlay.qcow2
```

### 3. Arrancar la VM con la base en vez del overlay

```bash
# ❌ Arrancar con la base directamente — modifica la base
qemu-system-x86_64 -drive file=base.qcow2,format=qcow2

# ✅ Arrancar con el overlay
qemu-system-x86_64 -drive file=overlay.qcow2,format=qcow2
```

Si arrancas con la base, todas las escrituras van a la base. Esto invalida
todos los overlays existentes. Protege la base con `chmod 444`.

### 4. Hacer commit con la VM encendida

```bash
# ❌ VM corriendo, intentar commit
qemu-img commit overlay.qcow2
# qemu-img: Failed to get shared "write" lock
# Is another process using the image [overlay.qcow2]?

# ✅ Primero apagar la VM, luego commit
virsh shutdown mi_vm
# esperar a que se apague...
qemu-img commit overlay.qcow2
```

### 5. Cadenas de backing demasiado largas

```bash
# ❌ Cadena de 8 niveles — rendimiento degradado
o8 → o7 → o6 → o5 → o4 → o3 → o2 → o1 → base

# ✅ Aplanar periódicamente
qemu-img rebase -b base.qcow2 -F qcow2 o8.qcow2
# Ahora: o8 → base (2 niveles)
```

Una lectura del peor caso en una cadena de 8 niveles requiere abrir 8
archivos y buscar el sector en cada uno secuencialmente.

---

## Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║  IMÁGENES BACKING — REFERENCIA RÁPIDA                              ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  Crear overlay:                                                    ║
║    qemu-img create -f qcow2 -b BASE -F qcow2 OVERLAY              ║
║                                                                    ║
║  Ver info con backing:                                             ║
║    qemu-img info OVERLAY                                           ║
║                                                                    ║
║  Ver cadena completa:                                              ║
║    qemu-img info --backing-chain OVERLAY                           ║
║                                                                    ║
║  Mapa de clusters (qué viene de dónde):                            ║
║    qemu-img map --output=json OVERLAY                              ║
║                                                                    ║
║  Commit (fusionar overlay → base):                                 ║
║    qemu-img commit OVERLAY           # VM apagada                  ║
║    qemu-img commit -b BASE OVERLAY   # commit hasta BASE específ.  ║
║                                                                    ║
║  Rebase unsafe (cambiar ruta, misma base):                         ║
║    qemu-img rebase -u -b NUEVA_RUTA -F qcow2 OVERLAY              ║
║                                                                    ║
║  Rebase safe (cambiar base real):                                  ║
║    qemu-img rebase -b NUEVA_BASE -F qcow2 OVERLAY                 ║
║                                                                    ║
║  Aplanar (eliminar backing, imagen independiente):                 ║
║    qemu-img convert -f qcow2 -O qcow2 OVERLAY STANDALONE          ║
║    qemu-img rebase -b "" OVERLAY     # alternativa in-place        ║
║                                                                    ║
║  Proteger base contra escritura:                                   ║
║    chmod 444 BASE                                                  ║
║                                                                    ║
║  REGLAS DE ORO:                                                    ║
║    • La VM arranca con el overlay, NUNCA con la base               ║
║    • La base es inmutable una vez que existen overlays              ║
║    • Cadenas ≤ 3 niveles — aplanar si crecen más                   ║
║    • commit/rebase solo con VM apagada                             ║
║    • Siempre usar -F para especificar formato del backing          ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Patrón de lab con múltiples overlays

**Objetivo**: experimentar el flujo de trabajo overlay para labs.

1. Crea una imagen base de 10 GB:
   ```bash
   qemu-img create -f qcow2 base_lab.qcow2 10G
   ```

2. Crea tres overlays a partir de la base:
   ```bash
   qemu-img create -f qcow2 -b base_lab.qcow2 -F qcow2 lab_a.qcow2
   qemu-img create -f qcow2 -b base_lab.qcow2 -F qcow2 lab_b.qcow2
   qemu-img create -f qcow2 -b base_lab.qcow2 -F qcow2 lab_c.qcow2
   ```

3. Verifica cada overlay con `qemu-img info`. Confirma:
   - Los tres tienen el mismo `virtual size` (10 GiB).
   - Los tres tienen `backing file: base_lab.qcow2`.
   - Los tres pesan menos de 300 KiB en disco.

4. Escribe datos diferentes en cada overlay usando `qemu-io`:
   ```bash
   # Escribir 1 MB de unos en lab_a
   qemu-io -c "write -P 0x41 0 1M" lab_a.qcow2

   # Escribir 5 MB de doses en lab_b
   qemu-io -c "write -P 0x42 0 5M" lab_b.qcow2

   # No escribir nada en lab_c
   ```

5. Compara los tamaños en disco de los tres overlays (`ls -lh`). ¿Por qué
   `lab_c.qcow2` sigue pesando ~196 KiB?

6. Destruye `lab_a.qcow2` y recréalo. Verifica que está limpio.

**Pregunta de reflexión**: si tienes 10 VMs de laboratorio idénticas excepto
por la configuración de red, ¿cuánto espacio extra consume cada VM adicional
usando overlays? ¿Y sin overlays?

---

### Ejercicio 2: Cadena de backing y commit

**Objetivo**: entender cadenas de backing y la operación commit.

1. Crea una cadena de 3 niveles:
   ```bash
   qemu-img create -f qcow2 base.qcow2 5G
   qemu-io -c "write -P 0xBB 0 2M" base.qcow2

   qemu-img create -f qcow2 -b base.qcow2 -F qcow2 snap1.qcow2
   qemu-io -c "write -P 0x11 0 1M" snap1.qcow2

   qemu-img create -f qcow2 -b snap1.qcow2 -F qcow2 snap2.qcow2
   qemu-io -c "write -P 0x22 1M 1M" snap2.qcow2
   ```

2. Visualiza la cadena completa:
   ```bash
   qemu-img info --backing-chain snap2.qcow2
   ```

3. Lee diferentes offsets desde snap2 y predice de dónde vienen los datos:
   ```bash
   # ¿De dónde viene este dato? (offset 0, escrito en snap1 con 0x11)
   qemu-io -c "read -P 0x11 0 1M" snap2.qcow2

   # ¿Y este? (offset 1M, escrito en snap2 con 0x22)
   qemu-io -c "read -P 0x22 1M 1M" snap2.qcow2

   # ¿Y este? (offset 2M, nunca sobreescrito, viene de base con 0xBB... ¿o es cero?)
   qemu-io -c "read -P 0x00 2M 1M" snap2.qcow2
   ```

4. Haz commit de snap2 en snap1:
   ```bash
   qemu-img commit snap2.qcow2
   ```

5. Verifica que snap1 ahora contiene los datos de snap2:
   ```bash
   qemu-io -c "read -P 0x22 1M 1M" snap1.qcow2
   ```

**Pregunta de reflexión**: después del commit, snap2 está esencialmente vacío
pero sigue apuntando a snap1 como backing. ¿Podrías eliminar snap2 y seguir
trabajando con snap1 directamente? ¿Qué implicaría para una VM que usaba
snap2 como disco?

---

### Ejercicio 3: Rebase para aplanar y mover imágenes

**Objetivo**: practicar rebase safe y unsafe.

1. Crea una cadena base → overlay1 → overlay2:
   ```bash
   qemu-img create -f qcow2 base.qcow2 5G
   qemu-io -c "write -P 0xAA 0 4M" base.qcow2

   qemu-img create -f qcow2 -b base.qcow2 -F qcow2 overlay1.qcow2
   qemu-io -c "write -P 0xBB 2M 2M" overlay1.qcow2

   qemu-img create -f qcow2 -b overlay1.qcow2 -F qcow2 overlay2.qcow2
   qemu-io -c "write -P 0xCC 3M 1M" overlay2.qcow2
   ```

2. Predice el contenido de overlay2 en cada rango de 1 MB:
   - `0-1M`: ¿?
   - `1M-2M`: ¿?
   - `2M-3M`: ¿?
   - `3M-4M`: ¿?

   Verifica con `qemu-io -c "read -P 0xVALOR OFFSET SIZE" overlay2.qcow2`.

3. Aplana la cadena con rebase safe:
   ```bash
   qemu-img rebase -b base.qcow2 -F qcow2 overlay2.qcow2
   ```

4. Verifica:
   - `qemu-img info overlay2.qcow2` muestra `backing file: base.qcow2`
     (ya no menciona overlay1).
   - El tamaño en disco de overlay2 creció (absorbió datos de overlay1).
   - Los datos leídos son idénticos al paso 2.

5. Ahora simula mover la base de directorio con rebase unsafe:
   ```bash
   mkdir -p /tmp/lab_images
   cp base.qcow2 /tmp/lab_images/base.qcow2
   qemu-img rebase -u -b /tmp/lab_images/base.qcow2 -F qcow2 overlay2.qcow2
   qemu-img info overlay2.qcow2 | grep "backing file"
   ```

6. Verifica que los datos siguen intactos leyendo los mismos rangos del paso 2.

**Pregunta de reflexión**: ¿qué pasaría si usaras rebase unsafe (`-u`) para
cambiar a una base con contenido **diferente** (no solo reubicada)? ¿QEMU
daría error o silenciosamente leería datos incorrectos?
