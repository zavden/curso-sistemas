# Agregar discos virtuales a una VM

## Índice

1. [Por qué agregar discos extra](#1-por-qué-agregar-discos-extra)
2. [Crear un disco virtual adicional](#2-crear-un-disco-virtual-adicional)
3. [Adjuntar el disco a una VM](#3-adjuntar-el-disco-a-una-vm)
4. [Verificar el disco dentro de la VM](#4-verificar-el-disco-dentro-de-la-vm)
5. [Adjuntar mediante XML](#5-adjuntar-mediante-xml)
6. [Detach: retirar un disco](#6-detach-retirar-un-disco)
7. [Flujo completo para labs del curso](#7-flujo-completo-para-labs-del-curso)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. Por qué agregar discos extra

En los labs de B08 vamos a trabajar con particionado, LVM, RAID y filesystems. Todas
estas prácticas requieren discos adicionales — no queremos experimentar con el disco
del sistema operativo.

La estrategia es:

```
┌─────────────────────────────────────────┐
│            VM de laboratorio            │
│                                         │
│  vda ──► disco del SO (plantilla)       │
│           NO SE TOCA                    │
│                                         │
│  vdb ──► disco extra 1 (desechable)     │
│  vdc ──► disco extra 2 (desechable)     │
│  vdd ──► disco extra N (desechable)     │
│                                         │
│  Después del lab: detach + eliminar     │
│  La VM base queda intacta               │
└─────────────────────────────────────────┘
```

Los discos extra son **desechables**: se crean para un lab específico, se usan y al
terminar se eliminan. La VM base (con su disco `vda`) permanece intacta para el
siguiente lab.

---

## 2. Crear un disco virtual adicional

Un disco extra se crea con `qemu-img create`, igual que cualquier imagen:

```bash
qemu-img create -f qcow2 /var/lib/libvirt/images/extra-disk.qcow2 5G
```

Los parámetros relevantes:

| Parámetro | Significado |
|-----------|-------------|
| `-f qcow2` | Formato — siempre qcow2 para discos de lab |
| Ruta | Dónde se almacena — idealmente dentro del pool default |
| `5G` | Tamaño virtual (thin provisioning: no ocupa 5G reales) |

**Predicción antes de ejecutar**: el disco se crea instantáneamente y ocupa apenas
~200 KB en el host (solo metadatos). Los 5G son el tamaño *máximo* que podrá crecer.

Verifiquemos:

```bash
# Crear el disco
qemu-img create -f qcow2 /var/lib/libvirt/images/lab-disk-01.qcow2 5G

# Verificar tamaño real vs virtual
qemu-img info /var/lib/libvirt/images/lab-disk-01.qcow2
```

Salida esperada:

```
image: /var/lib/libvirt/images/lab-disk-01.qcow2
file format: qcow2
virtual size: 5 GiB (5368709120 bytes)
disk size: 196 KiB                          # ← tamaño real en disco
```

### Tamaños recomendados para labs del curso

| Lab | Tamaño sugerido | Cantidad de discos |
|-----|------------------|--------------------|
| Particionado básico | 2G | 1 |
| LVM | 2G | 2-3 |
| RAID | 2G | 3-4 |
| Filesystems | 1G | 1-2 |
| Práctica libre | 5G | 1 |

---

## 3. Adjuntar el disco a una VM

### 3.1. Con `virsh attach-disk`

La forma más directa de adjuntar un disco a una VM:

```bash
virsh attach-disk lab-debian01 \
  /var/lib/libvirt/images/lab-disk-01.qcow2 \
  vdb \
  --driver qemu \
  --subdriver qcow2 \
  --persistent
```

Desglose de cada argumento:

| Argumento | Significado |
|-----------|-------------|
| `lab-debian01` | Nombre de la VM destino |
| `/var/lib/libvirt/images/lab-disk-01.qcow2` | Ruta absoluta al disco |
| `vdb` | Target device — el nombre que tendrá dentro de la VM |
| `--driver qemu` | Driver de emulación (siempre `qemu` para imágenes) |
| `--subdriver qcow2` | Formato de la imagen |
| `--persistent` | Guardar en la definición XML (sobrevive reboot) |

### Target devices: la secuencia vdX

El disco del SO es `vda`. Los discos extra siguen en orden:

```
vda ──► disco del sistema operativo
vdb ──► primer disco extra
vdc ──► segundo disco extra
vdd ──► tercer disco extra
vde ──► cuarto disco extra
...continúa alfabéticamente...
```

El prefijo `vd` indica **virtio disk** — el bus virtio, que es el más eficiente para
VMs con KVM. Si usaras `sd` en vez de `vd`, emularía un controlador SCSI (más lento).

### 3.2. Adjuntar con la VM encendida vs apagada

```bash
# VM encendida (hot-plug): el disco aparece inmediatamente
virsh attach-disk lab-debian01 \
  /var/lib/libvirt/images/lab-disk-01.qcow2 vdb \
  --driver qemu --subdriver qcow2 --persistent

# VM apagada: solo se modifica el XML, se verá al encender
virsh attach-disk lab-debian01 \
  /var/lib/libvirt/images/lab-disk-01.qcow2 vdb \
  --driver qemu --subdriver qcow2 --config
```

La diferencia entre `--persistent` y `--config`:

| Flag | VM encendida | VM apagada |
|------|--------------|------------|
| `--persistent` | Adjunta ahora + guarda en XML | Igual que `--config` |
| `--config` | Solo guarda en XML (no hot-plug) | Guarda en XML |
| (ninguno) | Adjunta ahora, se pierde al reboot | Error |

**Recomendación para labs**: usa `--persistent` con la VM encendida. Así el disco
aparece inmediatamente y además sobrevive al reboot.

### 3.3. Verificar que se adjuntó

Desde el host:

```bash
# Ver todos los discos de la VM
virsh domblklist lab-debian01
```

Salida esperada:

```
 Target   Source
-----------------------------------------------
 vda      /var/lib/libvirt/images/lab-debian01.qcow2
 vdb      /var/lib/libvirt/images/lab-disk-01.qcow2
```

---

## 4. Verificar el disco dentro de la VM

Conéctate a la VM y confirma que el kernel detectó el nuevo disco:

```bash
# Listar block devices
lsblk
```

Salida esperada:

```
NAME   MAJ:MIN RM  SIZE RO TYPE MOUNTPOINTS
vda      252:0    0   10G  0 disk
├─vda1   252:1    0    9G  0 part /
├─vda2   252:2    0    1K  0 part
└─vda5   252:5    0  975M  0 part [SWAP]
vdb      252:16   0    5G  0 disk              ← nuevo disco, sin particiones
```

El disco `vdb` aparece como un dispositivo de 5G sin particiones ni filesystem.
Está completamente vacío — listo para que en los labs de B08 practiques
particionado, mkfs, LVM, etc.

Información adicional:

```bash
# Ver el disco con fdisk
sudo fdisk -l /dev/vdb
```

```
Disk /dev/vdb: 5 GiB, 5368709120 bytes, 10485760 sectors
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / 512 bytes
Disklabel type: dos                          # ← o "does not contain a valid
                                             #    partition table" si es nuevo
```

### Si el disco no aparece (hot-plug)

En algunos kernels antiguos el disco hot-plugged puede no detectarse automáticamente.
Fuerza un rescan:

```bash
# Rescan del bus virtio (raro que sea necesario, pero por si acaso)
echo "1" | sudo tee /sys/bus/pci/rescan
```

---

## 5. Adjuntar mediante XML

Para mayor control, puedes definir el disco directamente en el XML de la VM.

### 5.1. Con `virsh edit`

```bash
virsh edit lab-debian01
```

Dentro de la sección `<devices>`, agrega un bloque `<disk>`:

```xml
<disk type='file' device='disk'>
  <driver name='qemu' type='qcow2' discard='unmap'/>
  <source file='/var/lib/libvirt/images/lab-disk-01.qcow2'/>
  <target dev='vdb' bus='virtio'/>
</disk>
```

Desglose de los elementos XML:

| Elemento | Significado |
|----------|-------------|
| `type='file'` | El backend es un archivo (vs block device) |
| `device='disk'` | Es un disco (vs cdrom) |
| `driver name='qemu' type='qcow2'` | Driver y formato |
| `discard='unmap'` | Permite que TRIM/discard del guest libere espacio en qcow2 |
| `source file=...` | Ruta absoluta al archivo de imagen |
| `target dev='vdb' bus='virtio'` | Nombre del device y bus |

### 5.2. Ventaja de `discard='unmap'`

Sin `discard='unmap'`, cuando borras archivos dentro de la VM, el espacio no se
libera en el archivo qcow2 del host. Con `discard='unmap'`:

```
Dentro de la VM:          En el host:
┌──────────────┐          ┌──────────────┐
│ rm bigfile   │  ──────► │ qcow2 shrinks│
│ fstrim -a    │          │ space freed  │
└──────────────┘          └──────────────┘
```

Para labs de B08 esto no es crítico (los discos son pequeños y desechables), pero
es buena práctica incluirlo.

### 5.3. Con `virsh attach-device`

Si prefieres no editar el XML completo, puedes usar un archivo XML parcial:

```bash
cat > /tmp/disk-vdb.xml << 'EOF'
<disk type='file' device='disk'>
  <driver name='qemu' type='qcow2' discard='unmap'/>
  <source file='/var/lib/libvirt/images/lab-disk-01.qcow2'/>
  <target dev='vdb' bus='virtio'/>
</disk>
EOF

virsh attach-device lab-debian01 /tmp/disk-vdb.xml --persistent
```

Esto es equivalente a `virsh attach-disk` pero con más opciones de configuración.

---

## 6. Detach: retirar un disco

### 6.1. Retirar el disco de la VM

```bash
# Con la VM encendida (hot-unplug)
virsh detach-disk lab-debian01 vdb --persistent
```

**Predicción**: el disco desaparece de `lsblk` dentro de la VM y se elimina del
XML. El archivo qcow2 sigue existiendo en el host.

Verifica:

```bash
# Ya no aparece en la lista de discos de la VM
virsh domblklist lab-debian01
```

### 6.2. Eliminar el archivo del disco

El detach solo desasocia el disco — el archivo sigue en el host. Para eliminarlo:

```bash
# Opción 1: borrar manualmente
rm /var/lib/libvirt/images/lab-disk-01.qcow2

# Opción 2: si el disco está en un pool, usar vol-delete
virsh vol-delete lab-disk-01.qcow2 --pool default
```

### 6.3. Precauciones al hacer detach

```
⚠ ANTES de hacer detach, dentro de la VM:

1. Desmonta el filesystem:
   sudo umount /dev/vdb1     # (si lo montaste)

2. Si usas LVM, desactiva el VG:
   sudo vgchange -an nombre_vg

3. Si usas RAID, detén el array:
   sudo mdadm --stop /dev/md0

Solo después: virsh detach-disk
```

Si haces detach sin desmontar, el kernel de la VM puede registrar errores de I/O.
En labs no es catastrófico (los discos son desechables), pero es buena práctica
desmontar primero.

---

## 7. Flujo completo para labs del curso

Este es el ciclo que seguirás en cada lab de B08:

```
 ┌─────────────────────────────────────────────────────────────────┐
 │                    CICLO DE UN LAB DE B08                       │
 │                                                                 │
 │  1. PREPARAR         2. PRACTICAR          3. LIMPIAR           │
 │  ┌──────────────┐   ┌──────────────────┐   ┌──────────────┐    │
 │  │ qemu-img     │   │ Dentro de la VM: │   │ umount       │    │
 │  │ create       │──►│ fdisk, mkfs,     │──►│ detach-disk  │    │
 │  │              │   │ mount, lvm,      │   │ rm disco.qcow2│   │
 │  │ attach-disk  │   │ mdadm...         │   │              │    │
 │  └──────────────┘   └──────────────────┘   └──────────────┘    │
 │        │                                          │             │
 │        └──────────────────────────────────────────┘             │
 │                 VM base intacta, repetir                        │
 └─────────────────────────────────────────────────────────────────┘
```

### Ejemplo completo: lab de particionado

```bash
# === PASO 1: PREPARAR (en el host) ===

# Crear un disco de 2G
qemu-img create -f qcow2 /var/lib/libvirt/images/lab-part-01.qcow2 2G

# Adjuntar a la VM (que ya está encendida)
virsh attach-disk lab-debian01 \
  /var/lib/libvirt/images/lab-part-01.qcow2 vdb \
  --driver qemu --subdriver qcow2 --persistent

# Verificar
virsh domblklist lab-debian01
```

```bash
# === PASO 2: PRACTICAR (dentro de la VM) ===

# Verificar que el disco existe
lsblk

# Crear una tabla de particiones y particiones
sudo fdisk /dev/vdb
# ... comandos de fdisk ...

# Crear filesystem
sudo mkfs.ext4 /dev/vdb1

# Montar y usar
sudo mount /dev/vdb1 /mnt
# ... trabajar ...
```

```bash
# === PASO 3: LIMPIAR (después del lab) ===

# Dentro de la VM: desmontar
sudo umount /mnt

# En el host: retirar disco
virsh detach-disk lab-debian01 vdb --persistent

# Eliminar el archivo
rm /var/lib/libvirt/images/lab-part-01.qcow2
```

La VM `lab-debian01` queda exactamente como estaba antes del lab.

### Atajos: recrear sin detach

Si quieres reiniciar un lab rápidamente sin hacer detach/attach, puedes
**sobrescribir** el disco (requiere que la VM esté apagada):

```bash
# Apagar la VM
virsh shutdown lab-debian01

# Recrear el disco (sobrescribe el contenido anterior)
qemu-img create -f qcow2 /var/lib/libvirt/images/lab-part-01.qcow2 2G

# Encender
virsh start lab-debian01
# El disco vdb aparece vacío de nuevo
```

Esto funciona porque `qemu-img create` sobrescribe el archivo existente con una
imagen vacía. No necesitas detach/attach porque el XML ya referencia ese path.

---

## 8. Errores comunes

### Error 1: olvidar `--subdriver qcow2`

```bash
# ✗ Sin --subdriver
virsh attach-disk lab-debian01 disco.qcow2 vdb --driver qemu --persistent

# Error: "unsupported configuration: unsupported driver format value"
# o el disco no se detecta correctamente dentro de la VM

# ✓ Con --subdriver
virsh attach-disk lab-debian01 disco.qcow2 vdb \
  --driver qemu --subdriver qcow2 --persistent
```

### Error 2: usar ruta relativa en attach-disk

```bash
# ✗ Ruta relativa
virsh attach-disk lab-debian01 disco.qcow2 vdb ...

# Puede fallar o crear confusión sobre dónde buscar el archivo

# ✓ Ruta absoluta
virsh attach-disk lab-debian01 \
  /var/lib/libvirt/images/disco.qcow2 vdb ...
```

### Error 3: reusar un target device ocupado

```bash
# Si vdb ya está en uso:
virsh attach-disk lab-debian01 otro-disco.qcow2 vdb ...
# Error: "target vdb already exists"

# ✓ Usar el siguiente disponible
virsh attach-disk lab-debian01 otro-disco.qcow2 vdc ...

# Para ver cuáles están en uso:
virsh domblklist lab-debian01
```

### Error 4: adjuntar un disco ya usado por otra VM

```bash
# Si vm-01 tiene disco.qcow2 adjuntado y lo adjuntas también a vm-02:
# Ambas VMs escriben al mismo archivo → CORRUPCIÓN DE DATOS

# ✓ Un disco qcow2 → una sola VM a la vez
# Verificar quién usa un disco:
virsh domblklist vm-01
virsh domblklist vm-02
```

### Error 5: hacer detach sin desmontar

```bash
# ✗ Detach con el disco montado dentro de la VM
virsh detach-disk lab-debian01 vdb --persistent
# El kernel de la VM genera errores de I/O

# ✓ Primero desmontar dentro de la VM
sudo umount /dev/vdb1
# Luego detach desde el host
virsh detach-disk lab-debian01 vdb --persistent
```

---

## 9. Cheatsheet

```bash
# ── Crear disco ──────────────────────────────────────────────────
qemu-img create -f qcow2 /var/lib/libvirt/images/DISCO.qcow2 SIZE

# ── Adjuntar a VM (hot-plug + persistente) ───────────────────────
virsh attach-disk VM /var/lib/libvirt/images/DISCO.qcow2 vdX \
  --driver qemu --subdriver qcow2 --persistent

# ── Adjuntar a VM apagada (solo config) ──────────────────────────
virsh attach-disk VM /var/lib/libvirt/images/DISCO.qcow2 vdX \
  --driver qemu --subdriver qcow2 --config

# ── Ver discos de una VM ─────────────────────────────────────────
virsh domblklist VM

# ── Verificar dentro de la VM ────────────────────────────────────
lsblk
sudo fdisk -l /dev/vdX

# ── Retirar disco ───────────────────────────────────────────────
virsh detach-disk VM vdX --persistent

# ── Eliminar archivo ────────────────────────────────────────────
rm /var/lib/libvirt/images/DISCO.qcow2
# o: virsh vol-delete DISCO.qcow2 --pool default

# ── Atajo: reiniciar lab (VM apagada) ───────────────────────────
qemu-img create -f qcow2 /var/lib/libvirt/images/DISCO.qcow2 SIZE
# Sobrescribe el disco manteniendo el attach en XML

# ── Adjuntar con XML ────────────────────────────────────────────
virsh attach-device VM /tmp/disk.xml --persistent
```

---

## 10. Ejercicios

### Ejercicio 1: agregar y verificar un disco

1. Crea un disco qcow2 de 3G llamado `practice-disk.qcow2` en el pool default
2. Adjúntalo como `vdb` a tu VM de laboratorio (con la VM encendida, persistente)
3. Verifica con `virsh domblklist` desde el host
4. Entra a la VM y verifica con `lsblk` que aparece `vdb` de 3G
5. Verifica con `qemu-img info` que el disco real ocupa muy poco espacio en el host

**Pregunta de reflexión**: el disco aparece como 3G dentro de la VM pero ocupa
~200 KB en el host. ¿Qué mecanismo hace esto posible y cuándo crecerá el archivo
real?

### Ejercicio 2: ciclo completo de lab

Simula un lab completo:

1. Crea un disco de 2G y adjúntalo como `vdb`
2. Dentro de la VM:
   - Crea una partición con `fdisk /dev/vdb` (una partición que use todo el disco)
   - Formatea con `mkfs.ext4 /dev/vdb1`
   - Monta en `/mnt` y crea un archivo de prueba
   - Verifica con `df -h /mnt`
3. Desmonta y limpia:
   - `umount /mnt`
   - Desde el host: `detach-disk` + `rm` del archivo

**Pregunta de reflexión**: después de crear el filesystem y escribir datos, ¿cuánto
creció el archivo qcow2 en el host? Usa `qemu-img info` antes y después para
comparar.

### Ejercicio 3: múltiples discos simultáneos

1. Crea tres discos de 1G cada uno: `multi-01.qcow2`, `multi-02.qcow2`, `multi-03.qcow2`
2. Adjunta los tres a la VM como `vdb`, `vdc`, `vdd`
3. Verifica dentro de la VM con `lsblk` que aparecen los tres
4. Retira los tres (desmontando si montaste alguno)
5. Elimina los archivos

**Pregunta de reflexión**: en un lab de RAID5, ¿por qué necesitas mínimo 3 discos?
Si uno de los tres falla, ¿qué pasa con los datos? ¿Tendría sentido agregar un
cuarto disco como spare?
