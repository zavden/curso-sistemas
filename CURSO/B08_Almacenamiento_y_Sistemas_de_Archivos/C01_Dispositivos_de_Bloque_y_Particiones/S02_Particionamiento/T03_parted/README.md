# parted

## Índice

1. [Qué es parted y cuándo usarlo](#1-qué-es-parted-y-cuándo-usarlo)
2. [Modo interactivo vs modo comando](#2-modo-interactivo-vs-modo-comando)
3. [Crear tablas de particiones](#3-crear-tablas-de-particiones)
4. [Crear particiones](#4-crear-particiones)
5. [Redimensionar particiones](#5-redimensionar-particiones)
6. [Eliminar particiones](#6-eliminar-particiones)
7. [Verificar alineación](#7-verificar-alineación)
8. [Diferencias clave con fdisk/gdisk](#8-diferencias-clave-con-fdiskgdisk)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Qué es parted y cuándo usarlo

`parted` (GNU Parted) es una herramienta de particionamiento que soporta tanto
MBR como GPT. Su diferencia fundamental con fdisk/gdisk:

```
┌────────────────────────────────────────────────────────────┐
│  fdisk/gdisk:                                              │
│    Los cambios se acumulan en memoria                      │
│    Solo se escriben al disco con 'w'                       │
│    Puedes salir con 'q' sin efecto                         │
│                                                            │
│  parted:                                                   │
│    ⚠ Los cambios se aplican INMEDIATAMENTE                 │
│    No hay 'w' para confirmar                               │
│    No hay undo                                             │
│                                                            │
│  Por eso parted pide confirmación en cada operación.       │
└────────────────────────────────────────────────────────────┘
```

### Cuándo usar parted

| Situación | Herramienta |
|-----------|-------------|
| Particionamiento manual simple | fdisk o gdisk |
| Necesitas redimensionar particiones | **parted** |
| Scripts de automatización | **parted** (modo comando) |
| Verificar alineación | **parted** |
| Examen RHCSA/LPIC | Conocer **parted** y fdisk |

---

## 2. Modo interactivo vs modo comando

### 2.1. Modo interactivo

```bash
sudo parted /dev/vdb
```

```
GNU Parted 3.5
Using /dev/vdb
Welcome to GNU Parted! Type 'help' to view a list of commands.
(parted)
```

En este modo escribes comandos uno a uno dentro del prompt `(parted)`.

### 2.2. Modo comando (one-liner)

```bash
# Cada operación en un solo comando
sudo parted /dev/vdb mklabel gpt
sudo parted /dev/vdb mkpart primary ext4 1MiB 1GiB
sudo parted /dev/vdb mkpart primary linux-swap 1GiB 1.5GiB
```

El modo comando es ideal para scripts — cada línea es una operación completa.

### 2.3. Flag `-s` (script mode)

Por defecto, parted pide confirmación para operaciones destructivas. Con `-s`
se suprime:

```bash
# Sin -s: pregunta "Are you sure?"
sudo parted /dev/vdb mklabel gpt
# Warning: The existing disk label... Destroy? Yes/No?

# Con -s: no pregunta (peligroso, útil en scripts)
sudo parted -s /dev/vdb mklabel gpt
```

---

## 3. Crear tablas de particiones

### 3.1. Modo interactivo

```bash
sudo parted /dev/vdb
```

```
(parted) mklabel msdos           ← tabla MBR
# o
(parted) mklabel gpt             ← tabla GPT
```

### 3.2. Modo comando

```bash
sudo parted -s /dev/vdb mklabel gpt
sudo parted -s /dev/vdc mklabel msdos
```

### 3.3. Tipos de label

| Label | Significado |
|-------|-------------|
| `msdos` | MBR (sinónimo de `dos` en fdisk) |
| `gpt` | GPT |
| `loop` | Sin tabla de particiones (todo el disco como una partición) |

---

## 4. Crear particiones

### 4.1. Sintaxis de mkpart

```
mkpart PART-TYPE [FS-TYPE] START END
```

| Parámetro | Significado | Ejemplo |
|-----------|-------------|---------|
| PART-TYPE | Tipo de partición | `primary`, `logical`, `extended` (solo MBR) |
| FS-TYPE | Hint del filesystem (opcional) | `ext4`, `xfs`, `linux-swap` |
| START | Dónde empieza | `1MiB`, `0%`, `1GiB` |
| END | Dónde termina | `1GiB`, `50%`, `100%`, `-1s` |

**Importante**: `FS-TYPE` es solo una etiqueta — **no crea el filesystem**. Solo
marca el tipo de partición en la tabla. Aún necesitas `mkfs` después.

### 4.2. Unidades de tamaño

parted acepta varias unidades:

| Unidad | Ejemplo | Significado |
|--------|---------|-------------|
| `MiB` | `1MiB` | Mebibytes (1024² bytes) — alineado |
| `GiB` | `1GiB` | Gibibytes (1024³ bytes) — alineado |
| `MB` | `1MB` | Megabytes (10⁶ bytes) — puede desalinear |
| `GB` | `1GB` | Gigabytes (10⁹ bytes) — puede desalinear |
| `%` | `50%` | Porcentaje del disco |
| `s` | `2048s` | Sectores |

**Recomendación**: usa siempre `MiB`/`GiB` (binarios) para garantizar
alineación. Las unidades decimales (`MB`/`GB`) pueden crear particiones
desalineadas.

### 4.3. Ejemplo GPT completo

```bash
sudo parted /dev/vdb
```

```
(parted) mklabel gpt

(parted) mkpart primary ext4 1MiB 1GiB
# Partición 1: 1MiB a 1GiB (≈1023 MiB de ext4)

(parted) mkpart primary linux-swap 1GiB 1.5GiB
# Partición 2: 1GiB a 1.5GiB (512 MiB de swap)

(parted) mkpart primary ext4 1.5GiB 100%
# Partición 3: 1.5GiB hasta el final del disco

(parted) print
Model: Virtio Block Device (virtblk)
Disk /dev/vdb: 2147MB
Sector size (logical/physical): 512B/512B
Partition Table: gpt

Number  Start   End     Size    File system  Name     Flags
 1      1049kB  1074MB  1073MB               primary
 2      1074MB  1611MB  537MB                primary
 3      1611MB  2147MB  536MB                primary

(parted) quit
```

### 4.4. Ejemplo MBR con extendida

```bash
sudo parted /dev/vdc
```

```
(parted) mklabel msdos

(parted) mkpart primary ext4 1MiB 500MiB
(parted) mkpart primary linux-swap 500MiB 750MiB
(parted) mkpart extended 750MiB 100%
(parted) mkpart logical ext4 750MiB 1250MiB
(parted) mkpart logical ext4 1250MiB 100%

(parted) print
Number  Start   End     Size    Type      File system  Flags
 1      1049kB  524MB   523MB   primary
 2      524MB   786MB   262MB   primary
 3      786MB   2147MB  1361MB  extended               lba
 5      787MB   1311MB  524MB   logical
 6      1312MB  2147MB  835MB   logical

(parted) quit
```

Nota: la partición 4 no existe (3 es la extendida, las lógicas empiezan en 5).

### 4.5. Modo comando (one-liners para scripts)

```bash
# Crear tabla y particiones en una secuencia
sudo parted -s /dev/vdb mklabel gpt
sudo parted -s /dev/vdb mkpart primary ext4 1MiB 1GiB
sudo parted -s /dev/vdb mkpart primary linux-swap 1GiB 1.5GiB
sudo parted -s /dev/vdb mkpart primary ext4 1.5GiB 100%
```

Esto es lo que usarías en scripts de provisioning.

### 4.6. Nombrar particiones (GPT)

```bash
(parted) name 1 "data"
(parted) name 2 "swap"
(parted) name 3 "extra"
```

O en modo comando:

```bash
sudo parted -s /dev/vdb name 1 "data"
```

---

## 5. Redimensionar particiones

Esta es la funcionalidad que distingue a parted — fdisk y gdisk no pueden
redimensionar particiones.

### 5.1. Concepto

```
Antes:
┌──────────────┬──────────────┬────────────────────────────┐
│  Partición 1 │  Partición 2 │       Espacio libre        │
│    500M      │    500M      │         1G                 │
└──────────────┴──────────────┴────────────────────────────┘

Después de resizepart 2:
┌──────────────┬──────────────────────────┬────────────────┐
│  Partición 1 │       Partición 2        │  Espacio libre │
│    500M      │         1G               │     500M       │
└──────────────┴──────────────────────────┴────────────────┘
```

### 5.2. Ampliar una partición (crecer)

```bash
sudo parted /dev/vdb
```

```
(parted) print
Number  Start   End     Size    File system  Name   Flags
 1      1049kB  1074MB  1073MB  ext4         data
 2      1074MB  1611MB  537MB                swap
 3      1611MB  2147MB  536MB                extra

(parted) resizepart 3 100%
# Partición 3 ahora usa todo el espacio disponible hasta el final
```

O en modo comando:

```bash
sudo parted -s /dev/vdb resizepart 3 100%
```

### 5.3. Reducir una partición (encoger)

```bash
(parted) resizepart 3 1800MiB
# Reduce la partición 3 a que termine en 1800MiB
```

**Precaución al encoger**:

```
⚠ ANTES de encoger una partición con filesystem:

1. Encoger el filesystem PRIMERO (resize2fs, xfs no puede)
2. Luego encoger la partición con resizepart

Si encoges la partición sin encoger el filesystem,
pierdes los datos del final.

Para AMPLIAR es al revés:
1. Ampliar la partición con resizepart
2. Luego ampliar el filesystem (resize2fs, xfs_growfs)
```

### 5.4. Limitaciones de resizepart

| Operación | ¿Funciona? | Nota |
|-----------|------------|------|
| Ampliar al final | Sí | Solo si hay espacio libre después |
| Reducir al final | Sí | Cuidado con el filesystem |
| Mover una partición | No | parted no mueve particiones |
| Ampliar al inicio | No | Cambiar el inicio requiere recrear |

---

## 6. Eliminar particiones

### 6.1. Modo interactivo

```bash
(parted) rm 3                    ← eliminar partición 3
```

**Recuerda**: en parted esto se aplica inmediatamente. No hay undo.

### 6.2. Modo comando

```bash
sudo parted -s /dev/vdb rm 3
```

### 6.3. Eliminar todas las particiones

No hay un "delete all" directo. La forma más limpia es recrear la tabla:

```bash
sudo parted -s /dev/vdb mklabel gpt    # borra todo, crea tabla nueva
```

---

## 7. Verificar alineación

### 7.1. align-check

```bash
sudo parted /dev/vdb align-check optimal 1
# 1 aligned

sudo parted /dev/vdb align-check optimal 2
# 2 aligned
```

### 7.2. Verificar todas las particiones

```bash
for i in 1 2 3; do
  echo -n "Partition $i: "
  sudo parted /dev/vdb align-check optimal $i
done
```

```
Partition 1: 1 aligned
Partition 2: 2 aligned
Partition 3: 3 aligned
```

### 7.3. Cómo garantizar alineación

Siempre empieza las particiones en múltiplos de `1MiB`:

```bash
# ✓ Alineado
sudo parted -s /dev/vdb mkpart primary ext4 1MiB 500MiB
sudo parted -s /dev/vdb mkpart primary ext4 500MiB 1GiB

# ✗ Posiblemente desalineado
sudo parted -s /dev/vdb mkpart primary ext4 1MB 500MB
# 1MB = 1,000,000 bytes, no es múltiplo de 1,048,576 (1MiB)
```

---

## 8. Diferencias clave con fdisk/gdisk

```
┌────────────────────────────────────────────────────────────────┐
│            parted vs fdisk/gdisk                               │
│                                                                │
│  Aspecto          parted              fdisk/gdisk              │
│  ─────────────    ─────────────       ─────────────            │
│  Aplicación       Inmediata ⚠         Solo con 'w'            │
│  Redimensionar    Sí (resizepart)     No                       │
│  Modo comando     Sí (one-liners)     No (solo interactivo)    │
│  Scripting        Excelente (-s)      Posible (con heredoc)    │
│  MBR              Sí                  fdisk=sí, gdisk=no       │
│  GPT              Sí                  fdisk=sí, gdisk=sí       │
│  Tamaño units     MiB, GiB, %        Sectores, +SIZE          │
│  Tipo partición   FS-TYPE hint        Hex code                 │
│  Conversión       No                  gdisk: MBR→GPT           │
│  Nombre (GPT)     name command        gdisk: c command         │
│  Alineación       align-check         Solo con parted          │
│  Riesgo           Alto (inmediato)    Bajo (confirmación 'w')  │
└────────────────────────────────────────────────────────────────┘
```

### Cuál usar para los labs

```
Aprendizaje interactivo    →  fdisk (MBR) o gdisk (GPT)
  - Puedes experimentar sin riesgo (q para salir)
  - Mejor para entender los conceptos

Scripts de provisioning    →  parted
  - Modo comando, una línea por operación
  - Ideal para automatizar

Redimensionar             →  parted (la única opción)

Exámenes RHCSA/LPIC       →  Conocer ambos
```

---

## 9. Errores comunes

### Error 1: asumir que parted espera confirmación

```bash
# ✗ Pensar que puedes deshacer como en fdisk
(parted) rm 1
# La partición 1 YA FUE ELIMINADA. No hay vuelta atrás.

# ✓ Verificar antes de actuar
(parted) print                   ← ver estado actual
(parted) rm 1                   ← solo si estás seguro
```

### Error 2: usar MB/GB en vez de MiB/GiB

```bash
# ✗ Unidades decimales — pueden causar desalineación
sudo parted -s /dev/vdb mkpart primary ext4 1MB 500MB

# ✓ Unidades binarias — siempre alineadas
sudo parted -s /dev/vdb mkpart primary ext4 1MiB 500MiB
```

La diferencia parece sutil (1MB = 1,000,000 vs 1MiB = 1,048,576), pero el
inicio de la partición puede quedar en un sector no alineado a 4K.

### Error 3: creer que mkpart crea el filesystem

```bash
# ✗ "Ya hice mkpart con ext4, puedo montar"
sudo parted -s /dev/vdb mkpart primary ext4 1MiB 1GiB
sudo mount /dev/vdb1 /mnt
# Error: wrong fs type — NO hay filesystem

# ✓ mkpart solo etiqueta el tipo. Después:
sudo mkfs.ext4 /dev/vdb1
sudo mount /dev/vdb1 /mnt
```

### Error 4: redimensionar la partición sin tocar el filesystem

```bash
# ✗ Ampliar partición sin ampliar el filesystem
sudo parted -s /dev/vdb resizepart 1 2GiB
# La partición creció, pero el filesystem ext4 sigue con el tamaño viejo
df -h /mnt    # muestra el tamaño original

# ✓ Después de ampliar la partición, ampliar el filesystem
sudo resize2fs /dev/vdb1      # para ext4
# o
sudo xfs_growfs /mnt          # para xfs (usa mount point)
```

### Error 5: olvidar el 1MiB inicial

```bash
# ✗ Empezar en 0
sudo parted -s /dev/vdb mkpart primary ext4 0% 50%
# parted puede crear una partición desde el sector 34 (GPT)
# desalineada

# ✓ Empezar en 1MiB explícitamente
sudo parted -s /dev/vdb mkpart primary ext4 1MiB 50%
# Garantiza alineación
```

Cuando usas `0%`, parted intenta empezar lo más al inicio posible, lo cual
puede no estar alineado. `1MiB` siempre es seguro.

---

## 10. Cheatsheet

```bash
# ── Modo interactivo ─────────────────────────────────────────────
sudo parted /dev/vdX
# Dentro:
#   print              ver tabla actual
#   mklabel gpt        crear tabla GPT
#   mklabel msdos      crear tabla MBR
#   mkpart TYPE FS START END    crear partición
#   rm N               eliminar partición N
#   resizepart N END   redimensionar partición N
#   name N "nombre"    nombrar partición (GPT)
#   align-check optimal N    verificar alineación
#   quit               salir

# ── Modo comando (one-liners) ───────────────────────────────────
sudo parted -s /dev/vdX mklabel gpt
sudo parted -s /dev/vdX mkpart primary ext4 1MiB 1GiB
sudo parted -s /dev/vdX mkpart primary linux-swap 1GiB 1.5GiB
sudo parted -s /dev/vdX mkpart primary ext4 1.5GiB 100%
sudo parted -s /dev/vdX rm 3
sudo parted -s /dev/vdX resizepart 2 2GiB
sudo parted -s /dev/vdX name 1 "data"

# ── Imprimir tabla ───────────────────────────────────────────────
sudo parted /dev/vdX print
sudo parted -s /dev/vdX print         # sin prompt interactivo
sudo parted -s /dev/vdX unit s print  # en sectores
sudo parted -s /dev/vdX unit MiB print  # en MiB

# ── Unidades (SIEMPRE usar MiB/GiB) ─────────────────────────────
# 1MiB = 1,048,576 bytes (alineado ✓)
# 1MB  = 1,000,000 bytes (puede desalinear ✗)
# 100% = todo el disco
# -1s  = último sector

# ── Verificar alineación ────────────────────────────────────────
sudo parted /dev/vdX align-check optimal 1

# ── Redimensionar: orden de operaciones ──────────────────────────
# AMPLIAR:
#   1. parted resizepart (la partición)
#   2. resize2fs o xfs_growfs (el filesystem)
#
# ENCOGER:
#   1. resize2fs (encoger el filesystem PRIMERO)
#   2. parted resizepart (la partición)
#   ⚠ xfs NO puede encogerse

# ── Script de ejemplo ───────────────────────────────────────────
# sudo parted -s /dev/vdb mklabel gpt
# sudo parted -s /dev/vdb mkpart primary ext4 1MiB 1GiB
# sudo parted -s /dev/vdb mkpart primary linux-swap 1GiB 1.5GiB
# sudo parted -s /dev/vdb mkpart primary ext4 1.5GiB 100%
# sudo mkfs.ext4 /dev/vdb1
# sudo mkswap /dev/vdb2
# sudo mkfs.ext4 /dev/vdb3
```

---

## 11. Ejercicios

### Ejercicio 1: particionar con parted interactivo

1. Entra a `sudo parted /dev/vdb`
2. Crea una tabla GPT con `mklabel gpt`
3. Crea 3 particiones:
   - 1: ext4, 1MiB a 800MiB
   - 2: linux-swap, 800MiB a 1GiB
   - 3: ext4, 1GiB a 100%
4. Verifica con `print`
5. Nombra las particiones: "root", "swap", "home"
6. Verifica alineación de las 3 particiones con `align-check optimal`
7. Sal con `quit`
8. Verifica desde fuera con `lsblk -o NAME,SIZE,PARTLABEL /dev/vdb`
9. Limpia con `sudo parted -s /dev/vdb mklabel gpt` (recrea tabla vacía)

**Pregunta de reflexión**: durante este ejercicio, ¿en qué momento se escribieron
los cambios al disco? ¿Hubo algún punto donde podrías haber cancelado sin
efecto, como con fdisk (`q`)?

### Ejercicio 2: parted en modo script

1. Escribe un script que particione `/dev/vdc` usando parted en modo comando:
   ```bash
   #!/bin/bash
   DISK="/dev/vdc"
   sudo parted -s $DISK mklabel gpt
   sudo parted -s $DISK mkpart primary ext4 1MiB 500MiB
   sudo parted -s $DISK mkpart primary ext4 500MiB 100%
   sudo parted -s $DISK name 1 "data"
   sudo parted -s $DISK name 2 "backup"
   ```
2. Ejecútalo y verifica con `sudo parted -s /dev/vdc print`
3. Compara la experiencia con particionar interactivamente en fdisk

**Pregunta de reflexión**: en un script de provisioning que prepara 10 discos,
¿preferirías parted o fdisk? ¿Por qué el modo comando de parted es más adecuado
para automatización que el modo interactivo de fdisk?

### Ejercicio 3: redimensionar una partición

1. Crea una tabla GPT en `/dev/vdd` con 2 particiones:
   - Partición 1: 500MiB (ext4)
   - Partición 2: 500MiB (ext4)
2. Crea filesystem en la partición 2: `sudo mkfs.ext4 /dev/vdd2`
3. Verifica el tamaño con `sudo parted -s /dev/vdd print`
4. Amplía la partición 2 hasta el final del disco:
   `sudo parted -s /dev/vdd resizepart 2 100%`
5. Verifica que la partición creció con `print`
6. Amplía el filesystem: `sudo resize2fs /dev/vdd2`
7. Monta y verifica el tamaño con `df -h`
8. Limpia: desmonta y recrea la tabla

**Pregunta de reflexión**: al ampliar la partición, ¿por qué fue necesario
ejecutar también `resize2fs`? ¿Qué hubiera pasado si montas sin ejecutar
`resize2fs` — verías el tamaño nuevo o el viejo?
