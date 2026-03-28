# Snapshots

## Índice

1. [Qué es un snapshot](#qué-es-un-snapshot)
2. [Snapshots internos vs externos](#snapshots-internos-vs-externos)
3. [Crear snapshot: snapshot-create-as](#crear-snapshot-snapshot-create-as)
4. [Listar snapshots: snapshot-list](#listar-snapshots-snapshot-list)
5. [Inspeccionar: snapshot-info y snapshot-dumpxml](#inspeccionar-snapshot-info-y-snapshot-dumpxml)
6. [Revertir: snapshot-revert](#revertir-snapshot-revert)
7. [Eliminar: snapshot-delete](#eliminar-snapshot-delete)
8. [Árboles de snapshots](#árboles-de-snapshots)
9. [Patrón de uso para labs del curso](#patrón-de-uso-para-labs-del-curso)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## Qué es un snapshot

Un snapshot captura el **estado completo** de una VM en un momento dado:
disco, memoria (opcional) y configuración XML. Es como una foto instantánea
que puedes usar para volver atrás.

```
┌──────────────────────────────────────────────────────────────────┐
│                                                                  │
│  Línea temporal de la VM:                                        │
│                                                                  │
│  ──────┬──────────────┬────────────────────┬─────────────►       │
│        │              │                    │             tiempo  │
│     instalar       snapshot            romper algo               │
│     SO + config    "pre-lab"           (fdisk, mkraid)           │
│                       │                    │                     │
│                       │                    │                     │
│                       └────────────────────┘                     │
│                         snapshot-revert                          │
│                         (volver al estado limpio)                │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

Para nuestro curso esto es crítico: en labs de particionado, RAID o LUKS,
las operaciones son destructivas. Sin snapshots, tendrías que reinstalar
la VM cada vez que quieras repetir un ejercicio.

### Qué contiene un snapshot

| Componente | Con VM corriendo | Con VM apagada |
|------------|-----------------|----------------|
| Estado del disco | ✅ Siempre | ✅ Siempre |
| Estado de la RAM | ✅ (hibernación) | — (no aplica) |
| Configuración XML | ✅ Siempre | ✅ Siempre |

Un snapshot con VM corriendo incluye la RAM, lo que permite volver
exactamente al instante capturado (procesos en ejecución, conexiones
abiertas, etc.). Un snapshot con VM apagada solo captura el disco.

---

## Snapshots internos vs externos

libvirt soporta dos tipos de snapshots fundamentalmente diferentes:

```
┌───────────────────────────────────────────────────────────────────┐
│                                                                   │
│  INTERNO (qcow2 internal snapshot)                                │
│  ─────────────────────────────────                                │
│  Todo dentro del MISMO archivo qcow2:                             │
│                                                                   │
│  disco.qcow2                                                     │
│  ├── datos actuales                                               │
│  ├── snapshot "pre-lab" (estado del disco en ese momento)         │
│  └── snapshot "pre-raid" (otro estado)                            │
│                                                                   │
│  • Un solo archivo — simple de gestionar                          │
│  • Crear/revertir/borrar con virsh                                │
│  • Rendimiento ligeramente menor (el archivo crece)               │
│  • Requiere formato qcow2                                        │
│                                                                   │
│  ─────────────────────────────────────────────────────────────── │
│                                                                   │
│  EXTERNO (qcow2 overlay)                                          │
│  ──────────────────────                                           │
│  Archivos separados (backing chain):                              │
│                                                                   │
│  disco-snap2.qcow2 (activo — escrituras van aquí)                │
│       │                                                           │
│       ▼ backing                                                   │
│  disco-snap1.qcow2 (read-only)                                   │
│       │                                                           │
│       ▼ backing                                                   │
│  disco-base.qcow2 (read-only)                                    │
│                                                                   │
│  • Múltiples archivos — más complejo                              │
│  • Mejor rendimiento (el archivo activo es pequeño)               │
│  • Más flexible (backups, merge selectivo)                        │
│  • Soporta raw como formato base                                  │
│                                                                   │
└───────────────────────────────────────────────────────────────────┘
```

### Comparación

| Aspecto | Interno | Externo |
|---------|---------|---------|
| Archivos involucrados | Uno (todo en el qcow2) | Múltiples (cadena de overlays) |
| Crear snapshot | `snapshot-create-as` | `snapshot-create-as --diskspec ...` |
| Revertir | `snapshot-revert` (simple) | Más complejo (manual o blockcommit) |
| Borrar | `snapshot-delete` (simple) | Requiere merge manual |
| Formato de disco | Solo qcow2 | qcow2, raw (la base puede ser raw) |
| Rendimiento I/O | Ligeramente menor | Mejor |
| Complejidad | Baja | Alta |
| Uso recomendado | Labs, desarrollo, pruebas | Producción, backups |

**Para el curso usaremos snapshots internos**: son más simples de gestionar,
`virsh snapshot-revert` funciona directamente, y la penalización de
rendimiento es irrelevante para labs.

---

## Crear snapshot: snapshot-create-as

### Snapshot con VM apagada (solo disco)

```bash
virsh snapshot-create-as debian-lab \
    --name "pre-lab-raid" \
    --description "Estado limpio antes del lab de RAID"
```

```
Domain snapshot pre-lab-raid created
```

### Snapshot con VM corriendo (disco + RAM)

```bash
# La VM debe estar running
virsh snapshot-create-as debian-lab \
    --name "pre-lab-lvm" \
    --description "Estado limpio antes del lab de LVM"
```

```
Domain snapshot pre-lab-lvm created
```

Cuando la VM está corriendo, el snapshot captura también el estado de la
RAM. Esto tarda más (debe volcar la memoria) pero permite restaurar al
estado exacto, con todos los procesos en ejecución.

### Flags de snapshot-create-as

| Flag | Significado |
|------|-------------|
| `--name "nombre"` | Nombre del snapshot (debe ser único para esta VM) |
| `--description "texto"` | Descripción legible (opcional pero recomendada) |
| `--atomic` | Si falla, no dejar cambios parciales |
| `--disk-only` | Solo disco, no capturar RAM aunque la VM esté corriendo |
| `--live` | No pausar la VM durante la captura (menor consistencia) |
| `--quiesce` | Pedirle al guest agent que haga sync antes (más consistente) |

### Snapshot disk-only (VM corriendo, sin capturar RAM)

```bash
virsh snapshot-create-as debian-lab \
    --name "pre-particiones" \
    --description "Antes de reparticionar discos" \
    --disk-only
```

Esto es más rápido que un snapshot completo porque no vuelca la RAM. Pero
al revertir, la VM volverá al estado del disco sin los procesos que estaban
corriendo — es como si se hubiera cortado la luz.

> **Predicción**: un snapshot completo (disco + RAM) de una VM con 2 GB de
> RAM tardará unos segundos mientras vuelca la memoria. Un snapshot
> disk-only es casi instantáneo.

---

## Listar snapshots: snapshot-list

```bash
virsh snapshot-list debian-lab
```

```
 Name              Creation Time               State
--------------------------------------------------------------
 pre-lab-raid      2026-03-20 10:15:30 +0100   shutoff
 pre-lab-lvm       2026-03-20 11:30:45 +0100   running
 pre-particiones   2026-03-20 14:22:10 +0100   disk-snapshot
```

La columna `State` indica en qué estado estaba la VM cuando se tomó el
snapshot:

| State | Significado |
|-------|-------------|
| `shutoff` | VM apagada al tomar el snapshot |
| `running` | VM corriendo (snapshot incluye RAM) |
| `disk-snapshot` | Solo disco, sin RAM (flag `--disk-only`) |

### Listar con detalle

```bash
# Con árbol de parentesco
virsh snapshot-list debian-lab --tree
```

```
pre-lab-raid
  |
  +- pre-lab-lvm
      |
      +- pre-particiones
```

```bash
# Solo los nombres (útil para scripts)
virsh snapshot-list debian-lab --name
```

```
pre-lab-raid
pre-lab-lvm
pre-particiones
```

### Snapshot actual (current)

```bash
virsh snapshot-current debian-lab --name
# pre-particiones
```

El snapshot "current" es el punto desde el cual la VM está divergiendo
actualmente.

---

## Inspeccionar: snapshot-info y snapshot-dumpxml

### Información resumida

```bash
virsh snapshot-info debian-lab --snapshotname "pre-lab-raid"
```

```
Name:           pre-lab-raid
Domain:         debian-lab
Current:        no
State:          shutoff
Location:       internal
Parent:         -
Children:       1
Descendants:    2
Metadata:       yes
```

| Campo | Significado |
|-------|-------------|
| `Current: no` | No es el snapshot activo |
| `State: shutoff` | La VM estaba apagada al tomarlo |
| `Location: internal` | Snapshot interno (dentro del qcow2) |
| `Parent: -` | No tiene padre (es el primero) |
| `Children: 1` | Un snapshot hijo directo |
| `Descendants: 2` | Total de snapshots descendientes |

### XML completo

```bash
virsh snapshot-dumpxml debian-lab --snapshotname "pre-lab-raid"
```

```xml
<domainsnapshot>
  <name>pre-lab-raid</name>
  <description>Estado limpio antes del lab de RAID</description>
  <state>shutoff</state>
  <creationTime>1710925530</creationTime>
  <memory snapshot='no'/>
  <disks>
    <disk name='vda' snapshot='internal'/>
  </disks>
  <domain type='kvm'>
    <!-- XML completo de la VM en ese momento -->
    ...
  </domain>
</domainsnapshot>
```

El XML del snapshot incluye la configuración completa de la VM en el
momento de la captura. Si revertir, la VM vuelve a esa configuración.

---

## Revertir: snapshot-revert

```bash
virsh snapshot-revert debian-lab --snapshotname "pre-lab-raid"
```

```
Domain snapshot pre-lab-raid reverted
```

Lo que sucede:

```
ANTES de revertir:
  VM en estado actual (discos modificados, particiones hechas, etc.)

DURANTE snapshot-revert:
  1. Si la VM está corriendo → se apaga automáticamente
  2. El disco vuelve al estado del snapshot
  3. Si el snapshot incluía RAM → se restaura la RAM
  4. Si el snapshot era shutoff → la VM queda apagada

DESPUÉS de revertir:
  VM en el estado exacto del snapshot
  Todo lo hecho después del snapshot se PIERDE
```

### Revertir a un snapshot con RAM (running)

```bash
virsh snapshot-revert debian-lab --snapshotname "pre-lab-lvm"
```

La VM vuelve al estado running con los mismos procesos que tenía cuando
se tomó el snapshot. Es como retroceder en el tiempo.

### Revertir a un snapshot sin RAM (shutoff)

```bash
virsh snapshot-revert debian-lab --snapshotname "pre-lab-raid"
```

La VM queda apagada. El disco vuelve al estado del snapshot. Necesitas
hacer `virsh start` para arrancarla.

### Revertir con VM corriendo

Si la VM está corriendo y reviertes a un snapshot de tipo `shutoff`, virsh
necesita autorización explícita:

```bash
virsh snapshot-revert debian-lab --snapshotname "pre-lab-raid" --running
```

El flag `--running` le dice: "revierte al estado del disco de ese snapshot
y luego arranca la VM". Sin `--running`, virsh dará un error porque el
snapshot era `shutoff` pero la VM está `running`.

Alternativamente, `--paused` deja la VM en estado pausado después de
revertir:

```bash
virsh snapshot-revert debian-lab --snapshotname "pre-lab-raid" --paused
```

### Lo que se pierde al revertir

> **Atención**: **todo** lo hecho después del snapshot se pierde
> permanentemente. No hay "undo" del revert. Si necesitas conservar
> el estado actual antes de revertir, crea otro snapshot primero.

```bash
# Patrón seguro: guardar el estado actual antes de revertir
virsh snapshot-create-as debian-lab \
    --name "antes-de-revertir" \
    --description "Por si acaso"

# Ahora revertir tranquilamente
virsh snapshot-revert debian-lab --snapshotname "pre-lab-raid"

# Si necesitas volver al estado que tenías:
virsh snapshot-revert debian-lab --snapshotname "antes-de-revertir"
```

---

## Eliminar: snapshot-delete

```bash
virsh snapshot-delete debian-lab --snapshotname "pre-lab-raid"
```

```
Domain snapshot pre-lab-raid deleted
```

`snapshot-delete` elimina un snapshot pero **no** afecta el estado actual
de la VM. Los datos del snapshot se fusionan en la imagen para mantener
la consistencia.

### Eliminar solo metadata

```bash
virsh snapshot-delete debian-lab \
    --snapshotname "pre-lab-raid" --metadata
```

Borra la definición del snapshot (metadata) pero no los datos del disco.
Útil si la metadata está corrupta pero los datos aún existen.

### Eliminar incluyendo hijos

Si un snapshot tiene hijos, no se puede borrar directamente sin especificar
qué hacer con ellos:

```bash
# Eliminar y reparentar hijos (los hijos apuntan al padre del eliminado)
virsh snapshot-delete debian-lab \
    --snapshotname "pre-lab-raid" --children-only

# Eliminar el snapshot Y todos sus descendientes
virsh snapshot-delete debian-lab \
    --snapshotname "pre-lab-raid" --children
```

### Limpieza total de snapshots

```bash
# Eliminar TODOS los snapshots de una VM
for snap in $(virsh snapshot-list debian-lab --name); do
    virsh snapshot-delete debian-lab --snapshotname "$snap"
done
```

> **Nota**: los snapshots internos ocupan espacio dentro del archivo qcow2.
> Eliminarlos libera ese espacio (aunque el archivo qcow2 puede no
> reducirse inmediatamente; usa `qemu-img convert` para compactarlo).

---

## Árboles de snapshots

Los snapshots forman un árbol. Cada snapshot tiene un padre (excepto el
primero) y puede tener hijos:

### Árbol lineal (lo más común en labs)

```
instalar SO
     │
     ▼
  "limpio"    ← snapshot
     │
     ▼
  hacer lab 1
     │
     ▼
  "post-lab1" ← snapshot
     │
     ▼
  hacer lab 2
     │
     ▼
  "post-lab2" ← snapshot
     │
     ▼
  estado actual
```

### Árbol ramificado (revert + continuar)

```
  "limpio"
     │
     ├──────────────────────────┐
     ▼                          ▼
  "intento-1"               revert a "limpio"
  (lab salió mal)               │
                                ▼
                            "intento-2"
                            (lab salió bien)
                                │
                                ▼
                            estado actual
```

Esto ocurre cuando reviertes a un snapshot y continúas trabajando: se crea
una rama nueva en el árbol.

### Visualizar el árbol

```bash
virsh snapshot-list debian-lab --tree
```

```
limpio
  |
  +- intento-1
  |
  +- intento-2
      |
      +- post-lab
```

### Ver el padre de un snapshot

```bash
virsh snapshot-info debian-lab --snapshotname "intento-2" | grep Parent
# Parent: limpio
```

---

## Patrón de uso para labs del curso

### Flujo recomendado

```bash
# ── PREPARACIÓN (una vez por lab) ─────────────────────────

# 1. Crear VM desde plantilla (overlay)
sudo qemu-img create -f qcow2 \
    -b /var/lib/libvirt/images/tpl-debian12.qcow2 -F qcow2 \
    /var/lib/libvirt/images/lab-fs.qcow2
# Añadir discos extra si el lab lo requiere
sudo qemu-img create -f qcow2 /var/lib/libvirt/images/lab-fs-disk1.qcow2 5G

# 2. Crear y arrancar la VM
sudo virt-install --name lab-fs --ram 1024 --vcpus 1 --import \
    --disk path=/var/lib/libvirt/images/lab-fs.qcow2 \
    --disk path=/var/lib/libvirt/images/lab-fs-disk1.qcow2 \
    --os-variant debian12 --network network=default \
    --graphics none --noautoconsole

# 3. Tomar snapshot "limpio" (estado base del lab)
virsh snapshot-create-as lab-fs \
    --name "limpio" \
    --description "Estado inicial del lab de filesystems"


# ── HACER EL EJERCICIO ───────────────────────────────────

# 4. Conectar y hacer el ejercicio
virsh console lab-fs
# mkfs.ext4 /dev/vdb, mount, escribir datos, etc.
# Ctrl+]


# ── SI NECESITAS REPETIR ─────────────────────────────────

# 5. Revertir al estado limpio
virsh snapshot-revert lab-fs --snapshotname "limpio" --running

# 6. Reconectar y repetir
virsh console lab-fs
# El disco vdb está limpio de nuevo, como si nada hubiera pasado


# ── AL TERMINAR ──────────────────────────────────────────

# 7. Destruir la VM
virsh destroy lab-fs
virsh undefine lab-fs --remove-all-storage
```

### Ejemplo concreto: lab de particionado

```bash
# Crear VM con disco extra
sudo ./create-lab-vm.sh lab-part 1 10G

# Snapshot antes de particionar
virsh snapshot-create-as lab-part --name "disco-limpio"

# ─── Intento 1: particionado con fdisk ───
virsh console lab-part
# sudo fdisk /dev/vdb → crear particiones
# Ctrl+]

# No salió bien, revertir
virsh snapshot-revert lab-part --snapshotname "disco-limpio" --running

# ─── Intento 2: particionado con gdisk ───
virsh console lab-part
# sudo gdisk /dev/vdb → crear particiones GPT
# Funcionó bien, guardar este estado
# Ctrl+]

virsh snapshot-create-as lab-part --name "particionado-gpt" \
    --description "Disco particionado con GPT: 3 particiones"

# ─── Continuar con mkfs ───
virsh console lab-part
# sudo mkfs.ext4 /dev/vdb1
# sudo mkfs.xfs /dev/vdb2
# ...

# Si mkfs sale mal, puedo revertir a "particionado-gpt"
# sin perder el particionado
```

### Snapshots escalonados

Para labs complejos con múltiples etapas, crear snapshots en cada paso
importante:

```bash
virsh snapshot-create-as lab-lvm --name "01-discos-limpios"
# ... crear PVs ...
virsh snapshot-create-as lab-lvm --name "02-pvs-creados"
# ... crear VG ...
virsh snapshot-create-as lab-lvm --name "03-vg-creado"
# ... crear LVs ...
virsh snapshot-create-as lab-lvm --name "04-lvs-creados"
# ... formatear y montar ...
virsh snapshot-create-as lab-lvm --name "05-montado"
```

Si algo sale mal en el paso de LVs, puedes revertir solo a `03-vg-creado`
sin perder los pasos anteriores.

```bash
virsh snapshot-list lab-lvm --tree
```

```
01-discos-limpios
  |
  +- 02-pvs-creados
      |
      +- 03-vg-creado
          |
          +- 04-lvs-creados
              |
              +- 05-montado
```

---

## Errores comunes

### 1. Snapshot en disco raw (no qcow2)

```bash
# ❌ El disco de la VM es raw — no soporta snapshots internos
virsh snapshot-create-as lab-raw --name "test"
# error: unsupported configuration: internal snapshot for disk vda
# unsupported for storage type raw

# ✅ Convertir a qcow2 primero (VM apagada)
qemu-img convert -f raw -O qcow2 disk.raw disk.qcow2
# O usar qcow2 desde el principio (lo normal con virt-install)
```

Los snapshots internos son una funcionalidad de qcow2. Si tu disco es raw,
necesitas usar snapshots externos o convertir a qcow2.

### 2. Revertir sin --running cuando la VM está corriendo

```bash
# ❌ Snapshot era shutoff, VM está running
virsh snapshot-revert debian-lab --snapshotname "pre-lab"
# error: revert requires force: Target state 'shutoff' doesn't match
# current state 'running'

# ✅ Especificar qué hacer después del revert
virsh snapshot-revert debian-lab --snapshotname "pre-lab" --running
# o
virsh snapshot-revert debian-lab --snapshotname "pre-lab" --paused
```

### 3. Asumir que snapshot-delete pierde datos actuales

```bash
# Creencia errónea: "si borro el snapshot, pierdo lo que guardé"
# Realidad: snapshot-delete elimina el PUNTO DE RESTAURACIÓN,
#           no afecta el estado actual de la VM

virsh snapshot-delete debian-lab --snapshotname "pre-lab"
# El estado actual de la VM NO cambia
# Solo pierdes la capacidad de volver a "pre-lab"
```

### 4. Demasiados snapshots acumulados

```bash
# ❌ Docenas de snapshots olvidados — el qcow2 crece indefinidamente
virsh snapshot-list debian-lab | wc -l
# 47

# Los snapshots internos ocupan espacio dentro del qcow2
# Cada snapshot almacena los clusters que han cambiado

# ✅ Limpiar snapshots que ya no necesitas
virsh snapshot-delete debian-lab --snapshotname "viejo-1"
virsh snapshot-delete debian-lab --snapshotname "viejo-2"

# Para recuperar espacio después de borrar muchos snapshots:
virsh shutdown debian-lab
qemu-img convert -f qcow2 -O qcow2 disk.qcow2 disk-compactado.qcow2
mv disk-compactado.qcow2 disk.qcow2
```

### 5. Snapshot de VM con múltiples discos (solo algunos en qcow2)

```bash
# ❌ VM con vda (qcow2) y vdb (raw) — falla
virsh snapshot-create-as lab-mixed --name "test"
# error: unsupported configuration: internal snapshot for disk vdb
# unsupported for storage type raw

# ✅ Opción A: convertir todos los discos a qcow2
# ✅ Opción B: excluir discos raw del snapshot
virsh snapshot-create-as lab-mixed --name "test" \
    --diskspec vda,snapshot=internal \
    --diskspec vdb,snapshot=no
```

---

## Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║  SNAPSHOTS — REFERENCIA RÁPIDA                                     ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CREAR:                                                            ║
║    virsh snapshot-create-as VM --name "NOMBRE" \                   ║
║      --description "descripción"                                   ║
║                                                                    ║
║    Flags opcionales:                                               ║
║      --disk-only     solo disco (sin RAM, más rápido)              ║
║      --atomic        todo o nada                                   ║
║      --live          no pausar VM durante captura                  ║
║                                                                    ║
║  LISTAR:                                                           ║
║    virsh snapshot-list VM              tabla con fechas/estado      ║
║    virsh snapshot-list VM --tree       árbol visual                 ║
║    virsh snapshot-list VM --name       solo nombres                 ║
║    virsh snapshot-current VM --name    snapshot activo              ║
║                                                                    ║
║  INSPECCIONAR:                                                     ║
║    virsh snapshot-info VM --snapshotname "NOMBRE"                  ║
║    virsh snapshot-dumpxml VM --snapshotname "NOMBRE"               ║
║                                                                    ║
║  REVERTIR:                                                         ║
║    virsh snapshot-revert VM --snapshotname "NOMBRE"                ║
║    virsh snapshot-revert VM --snapshotname "NOMBRE" --running      ║
║    virsh snapshot-revert VM --snapshotname "NOMBRE" --paused       ║
║                                                                    ║
║  ELIMINAR:                                                         ║
║    virsh snapshot-delete VM --snapshotname "NOMBRE"                ║
║    virsh snapshot-delete VM --snapshotname "NOMBRE" --children     ║
║                                                                    ║
║  PATRÓN PARA LABS:                                                 ║
║    1. Crear VM desde plantilla                                     ║
║    2. snapshot-create-as → "limpio"                                ║
║    3. Hacer ejercicio                                              ║
║    4. Si sale mal → snapshot-revert "limpio" --running             ║
║    5. Repetir desde 3                                              ║
║                                                                    ║
║  REGLAS:                                                           ║
║    • Requiere discos qcow2 (no raw)                                ║
║    • snapshot-delete NO afecta el estado actual                    ║
║    • Revertir PIERDE todo lo posterior al snapshot                 ║
║    • Limpiar snapshots viejos para evitar que el qcow2 crezca     ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Ciclo básico de snapshots

**Objetivo**: crear, listar, revertir y eliminar snapshots.

1. Crea una VM de lab (o usa una existente):
   ```bash
   sudo ./create-lab-vm.sh lab-snap 1 5G
   ```

2. Espera a que arranque y conéctate:
   ```bash
   sleep 20
   virsh console lab-snap
   ```

3. Crea un archivo de referencia dentro de la VM:
   ```bash
   echo "estado original" | sudo tee /root/marker.txt
   ls /dev/vdb   # verificar que el disco extra existe
   ```
   Sal con `Ctrl+]`.

4. Toma un snapshot:
   ```bash
   virsh snapshot-create-as lab-snap \
       --name "original" \
       --description "Estado limpio con marker.txt"
   ```

5. Lista los snapshots:
   ```bash
   virsh snapshot-list lab-snap
   virsh snapshot-info lab-snap --snapshotname "original"
   ```

6. Modifica la VM (simular un lab destructivo):
   ```bash
   virsh console lab-snap
   echo "datos modificados" | sudo tee /root/marker.txt
   sudo dd if=/dev/zero of=/dev/vdb bs=1M count=100
   cat /root/marker.txt   # "datos modificados"
   ```
   Sal con `Ctrl+]`.

7. Revierte al snapshot:
   ```bash
   virsh snapshot-revert lab-snap --snapshotname "original" --running
   ```

8. Verifica que volvió al estado original:
   ```bash
   virsh console lab-snap
   cat /root/marker.txt
   # ¿Dice "estado original" o "datos modificados"?
   ```

9. Elimina el snapshot:
   ```bash
   virsh snapshot-delete lab-snap --snapshotname "original"
   virsh snapshot-list lab-snap
   # (vacío)
   ```

10. Limpia:
    ```bash
    sudo ./destroy-lab-vm.sh lab-snap
    ```

**Pregunta de reflexión**: cuando revertiste al snapshot, ¿la VM se reinició
o continuó donde estaba? ¿Qué tipo de snapshot era (running o shutoff)?
¿Cómo afecta esto al comportamiento del revert?

---

### Ejercicio 2: Snapshots escalonados para un lab multi-etapa

**Objetivo**: simular un lab de LVM con puntos de restauración por etapa.

1. Crea una VM con 3 discos extra:
   ```bash
   sudo ./create-lab-vm.sh lab-lvm-snap 3 5G
   sleep 20
   ```

2. Snapshot inicial:
   ```bash
   virsh snapshot-create-as lab-lvm-snap --name "00-inicio"
   ```

3. Etapa 1 — crear PVs:
   ```bash
   virsh console lab-lvm-snap
   sudo pvcreate /dev/vdb /dev/vdc /dev/vdd 2>/dev/null || \
       echo "pvcreate: OK o paquete lvm2 no instalado (simular)"
   echo "PVs creados" | sudo tee /root/etapa.txt
   ```
   Sal con `Ctrl+]`.
   ```bash
   virsh snapshot-create-as lab-lvm-snap --name "01-pvs"
   ```

4. Etapa 2 — crear VG:
   ```bash
   virsh console lab-lvm-snap
   echo "VG creado" | sudo tee /root/etapa.txt
   ```
   Sal con `Ctrl+]`.
   ```bash
   virsh snapshot-create-as lab-lvm-snap --name "02-vg"
   ```

5. Etapa 3 — simular error:
   ```bash
   virsh console lab-lvm-snap
   echo "LVs MAL configurados" | sudo tee /root/etapa.txt
   sudo dd if=/dev/zero of=/dev/vdb bs=1M count=10
   ```
   Sal con `Ctrl+]`.

6. Revertir solo a la etapa 2 (no perder PVs ni VG):
   ```bash
   virsh snapshot-revert lab-lvm-snap --snapshotname "02-vg" --running
   virsh console lab-lvm-snap
   cat /root/etapa.txt
   # ¿Dice "VG creado"? ¡Los PVs y VG están intactos!
   ```

7. Ver el árbol de snapshots:
   ```bash
   virsh snapshot-list lab-lvm-snap --tree
   ```
   ¿Se creó una rama al revertir y continuar?

8. Limpia:
   ```bash
   sudo ./destroy-lab-vm.sh lab-lvm-snap
   ```

**Pregunta de reflexión**: si no tuvieras snapshots y quisieras repetir
solo la etapa 3, ¿qué opciones tendrías? ¿Cuánto tiempo ahorran los
snapshots escalonados en un lab de 5 etapas?

---

### Ejercicio 3: Impacto de snapshots en el tamaño del disco

**Objetivo**: observar cómo los snapshots internos afectan al archivo qcow2.

1. Crea una VM de lab:
   ```bash
   sudo ./create-lab-vm.sh lab-size 0
   sleep 20
   ```

2. Mide el tamaño inicial del disco:
   ```bash
   sudo qemu-img info /var/lib/libvirt/images/lab-size-os.qcow2 | grep "disk size"
   # Anota: _______ MB
   ```

3. Toma un snapshot:
   ```bash
   virsh snapshot-create-as lab-size --name "snap1"
   sudo qemu-img info /var/lib/libvirt/images/lab-size-os.qcow2 | grep "disk size"
   # Anota: _______ MB (¿creció?)
   ```

4. Escribe datos dentro de la VM:
   ```bash
   virsh console lab-size
   sudo dd if=/dev/urandom of=/root/datos.bin bs=1M count=50
   sync
   ```
   Sal con `Ctrl+]`.

5. Mide de nuevo:
   ```bash
   sudo qemu-img info /var/lib/libvirt/images/lab-size-os.qcow2 | grep "disk size"
   # Anota: _______ MB (creció ~50 MB)
   ```

6. Toma otro snapshot y escribe más datos:
   ```bash
   virsh snapshot-create-as lab-size --name "snap2"
   virsh console lab-size
   sudo dd if=/dev/urandom of=/root/datos2.bin bs=1M count=50
   sync
   ```
   Sal con `Ctrl+]`.

7. Mide:
   ```bash
   sudo qemu-img info /var/lib/libvirt/images/lab-size-os.qcow2 | grep "disk size"
   # Anota: _______ MB
   ```

8. Elimina todos los snapshots:
   ```bash
   virsh snapshot-delete lab-size --snapshotname "snap1"
   virsh snapshot-delete lab-size --snapshotname "snap2"
   sudo qemu-img info /var/lib/libvirt/images/lab-size-os.qcow2 | grep "disk size"
   # ¿Se redujo el archivo? (probablemente no mucho)
   ```

9. Compacta el archivo:
   ```bash
   virsh shutdown lab-size
   sleep 10
   sudo qemu-img convert -f qcow2 -O qcow2 \
       /var/lib/libvirt/images/lab-size-os.qcow2 \
       /var/lib/libvirt/images/lab-size-os-compact.qcow2
   ls -lh /var/lib/libvirt/images/lab-size-os*.qcow2
   # Comparar tamaños
   ```

10. Limpia:
    ```bash
    sudo ./destroy-lab-vm.sh lab-size
    sudo rm -f /var/lib/libvirt/images/lab-size-os-compact.qcow2
    ```

**Pregunta de reflexión**: los snapshots internos nunca "devuelven" espacio
al archivo qcow2 al ser eliminados (el archivo no se encoge). ¿Por qué?
¿Qué operación es necesaria para recuperar ese espacio? ¿Es esto un problema
práctico para labs con discos pequeños?
