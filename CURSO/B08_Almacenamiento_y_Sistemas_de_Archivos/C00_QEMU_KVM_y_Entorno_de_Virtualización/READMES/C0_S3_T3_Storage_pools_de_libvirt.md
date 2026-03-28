# Storage pools de libvirt

## Índice

1. [Qué es un storage pool](#qué-es-un-storage-pool)
2. [Pools vs gestión manual de imágenes](#pools-vs-gestión-manual-de-imágenes)
3. [El pool default](#el-pool-default)
4. [Ciclo de vida de un pool](#ciclo-de-vida-de-un-pool)
5. [Crear un pool en directorio custom](#crear-un-pool-en-directorio-custom)
6. [Gestionar volúmenes dentro de pools](#gestionar-volúmenes-dentro-de-pools)
7. [Tipos de pools](#tipos-de-pools)
8. [XML de definición de pools](#xml-de-definición-de-pools)
9. [Permisos y SELinux](#permisos-y-selinux)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## Qué es un storage pool

Un storage pool es la abstracción que libvirt usa para **agrupar y gestionar**
los recursos de almacenamiento donde viven las imágenes de disco de las VMs.

Sin libvirt, gestionarías archivos qcow2 directamente con comandos de
filesystem (`cp`, `mv`, `rm`, `qemu-img`). Esto funciona, pero no escala:
no hay inventario centralizado, los permisos se manejan a mano, y herramientas
como virt-manager no saben dónde buscar imágenes.

Un pool resuelve esto:

```
┌──────────────────────────────────────────────────────────────┐
│                       libvirt                                │
│                                                              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐          │
│  │ Pool:       │  │ Pool:       │  │ Pool:       │          │
│  │ "default"   │  │ "labs"      │  │ "isos"      │          │
│  │             │  │             │  │             │          │
│  │ /var/lib/   │  │ /home/user/ │  │ /srv/iso/   │          │
│  │ libvirt/    │  │ lab-images/ │  │             │          │
│  │ images/     │  │             │  │             │          │
│  │             │  │             │  │             │          │
│  │ Volúmenes:  │  │ Volúmenes:  │  │ Volúmenes:  │          │
│  │ • vm1.qcow2 │  │ • base.qcow2│  │ • debian.iso│          │
│  │ • vm2.qcow2 │  │ • lab_a.qcow│  │ • alma.iso  │          │
│  └─────────────┘  └─────────────┘  └─────────────┘          │
│                                                              │
│  virsh pool-list  →  muestra los tres pools                  │
│  virsh vol-list labs → muestra base.qcow2, lab_a.qcow2      │
│  virt-manager     →  ve todos los pools y volúmenes en GUI   │
└──────────────────────────────────────────────────────────────┘
```

Dentro de un pool, cada imagen de disco es un **volumen**. La relación es
sencilla: un pool contiene volúmenes, un volumen es un disco virtual.

---

## Pools vs gestión manual de imágenes

| Aspecto | Gestión manual (`qemu-img` + `cp`) | Storage pools de libvirt |
|---------|-----------------------------------|--------------------------|
| Inventario | `ls /directorio/` | `virsh vol-list pool` |
| Crear volumen | `qemu-img create -f qcow2 ...` | `virsh vol-create-as pool ...` |
| Permisos | `chown qemu:qemu` manual | libvirt aplica permisos correctos |
| SELinux labels | `restorecon` manual | libvirt etiqueta automáticamente |
| Visibilidad en virt-manager | No aparece sin pool | Aparece en la GUI |
| Limpieza al destruir VM | Manual (`rm`) | `virsh vol-delete` integrado |
| Múltiples backends | Solo directorios | Directorios, LVM, NFS, iSCSI, ZFS... |

> **Predicción**: si creas una imagen qcow2 con `qemu-img` en un directorio
> que no es un pool de libvirt, la imagen funcionará perfectamente con QEMU
> directo, pero no aparecerá en `virsh vol-list` ni en virt-manager hasta
> que la importes o registres el directorio como pool.

---

## El pool default

Cuando instalas libvirt, se crea automáticamente un pool llamado `default`
que apunta a `/var/lib/libvirt/images/`:

```bash
virsh pool-list --all
```

```
 Name      State    Autostart
-------------------------------
 default   active   yes
```

```bash
virsh pool-info default
```

```
Name:           default
UUID:           a1b2c3d4-e5f6-7890-abcd-ef1234567890
State:          running
Persistent:     yes
Autostart:      yes
Capacity:       100.00 GiB
Allocation:     12.34 GiB
Available:      87.66 GiB
```

Los campos clave:

| Campo | Significado |
|-------|-------------|
| `State: running` | El pool está activo y disponible |
| `Persistent: yes` | Sobrevive reinicios (definido en XML, no transitorio) |
| `Autostart: yes` | Se activa automáticamente al arrancar libvirtd |
| `Capacity` | Espacio total del filesystem subyacente |
| `Allocation` | Espacio usado por los volúmenes del pool |
| `Available` | Espacio libre |

Para ver dónde apunta:

```bash
virsh pool-dumpxml default | grep path
```

```xml
<path>/var/lib/libvirt/images</path>
```

Para listar los volúmenes dentro del pool default:

```bash
virsh vol-list default
```

```
 Name              Path
-----------------------------------------------------------
 vm1.qcow2         /var/lib/libvirt/images/vm1.qcow2
 vm2.qcow2         /var/lib/libvirt/images/vm2.qcow2
```

---

## Ciclo de vida de un pool

Un pool pasa por estados bien definidos. Para entender los comandos `virsh`
necesitas conocer este ciclo:

```
                    pool-define-as
                    (o pool-define)
                         │
                         ▼
               ┌─────────────────┐
               │   DEFINED       │     Pool registrado en libvirt
               │   (inactive)    │     pero no disponible todavía
               └────────┬────────┘
                        │
                   pool-build
                   (solo pools nuevos,
                    crea directorio)
                        │
                        ▼
               ┌─────────────────┐
               │   BUILT         │     Directorio/backend creado
               │   (inactive)    │     pero pool no activado
               └────────┬────────┘
                        │
                   pool-start
                        │
                        ▼
               ┌─────────────────┐
               │   ACTIVE        │     Pool disponible para
               │   (running)     │     crear/usar volúmenes
               └────────┬────────┘
                        │
              ┌─────────┴──────────┐
              │                    │
        pool-autostart        pool-stop
        (activar al boot)         │
                                  ▼
                         ┌─────────────────┐
                         │   INACTIVE       │
                         │   (stopped)      │
                         └────────┬─────────┘
                                  │
                            pool-undefine
                            (+ pool-delete
                             para borrar datos)
                                  │
                                  ▼
                         ┌─────────────────┐
                         │   REMOVED        │
                         │   (no existe)    │
                         └──────────────────┘
```

Cada comando:

| Comando | Qué hace |
|---------|----------|
| `pool-define-as` | Registra el pool en libvirt (guarda XML en `/etc/libvirt/storage/`) |
| `pool-build` | Crea el recurso subyacente (mkdir, mkfs, etc.) |
| `pool-start` | Activa el pool (lo hace disponible) |
| `pool-autostart` | Marca el pool para activarse automáticamente con libvirtd |
| `pool-stop` | Desactiva el pool (los volúmenes dejan de ser accesibles via libvirt) |
| `pool-undefine` | Elimina la definición del pool de libvirt |
| `pool-delete` | Borra el recurso subyacente (¡el directorio y su contenido!) |

> **Advertencia**: `pool-delete` **destruye datos**. Borra el directorio
> completo con todo su contenido. Usar con extremo cuidado. No es lo mismo
> que `pool-undefine`, que solo elimina la definición sin tocar los archivos.

### Atajo: pool-create-as

Para pools temporales (no persistentes), `pool-create-as` combina define +
build + start en un solo paso, pero el pool desaparece al reiniciar libvirtd:

```bash
# Pool transitorio (no sobrevive reinicios)
virsh pool-create-as temp_pool dir --target /tmp/vms

# Pool persistente (procedimiento completo)
virsh pool-define-as my_pool dir --target /srv/vms
virsh pool-build my_pool
virsh pool-start my_pool
virsh pool-autostart my_pool
```

---

## Crear un pool en directorio custom

El caso más común: quieres almacenar imágenes en un directorio distinto del
default, por ejemplo en un disco dedicado montado en `/srv/lab-images/`.

### Paso a paso

```bash
# 1. Definir el pool
#    Sintaxis: virsh pool-define-as NOMBRE TIPO --target RUTA
virsh pool-define-as labs dir --target /srv/lab-images
```

```
Pool labs defined
```

`dir` es el tipo de pool (directorio del filesystem). `--target` indica la
ruta del directorio.

```bash
# 2. Construir el pool (crea el directorio si no existe)
virsh pool-build labs
```

```
Pool labs built
```

Si el directorio ya existe, `pool-build` simplemente ajusta permisos. Si no
existe, lo crea con los permisos correctos.

```bash
# 3. Arrancar el pool
virsh pool-start labs
```

```
Pool labs started
```

```bash
# 4. Configurar arranque automático
virsh pool-autostart labs
```

```
Pool labs marked as autostarted
```

### Verificar

```bash
virsh pool-list --all
```

```
 Name      State    Autostart
-------------------------------
 default   active   yes
 labs      active   yes
```

```bash
virsh pool-info labs
```

```
Name:           labs
UUID:           b2c3d4e5-f6a7-8901-bcde-f23456789012
State:          running
Persistent:     yes
Autostart:      yes
Capacity:       200.00 GiB
Allocation:     0.00 B
Available:      200.00 GiB
```

```bash
# Ver el XML completo de la definición
virsh pool-dumpxml labs
```

```xml
<pool type='dir'>
  <name>labs</name>
  <uuid>b2c3d4e5-f6a7-8901-bcde-f23456789012</uuid>
  <capacity unit='bytes'>214748364800</capacity>
  <allocation unit='bytes'>0</allocation>
  <available unit='bytes'>214748364800</available>
  <source>
  </source>
  <target>
    <path>/srv/lab-images</path>
    <permissions>
      <mode>0711</mode>
      <owner>0</owner>
      <group>0</group>
    </permissions>
  </target>
</pool>
```

### Dónde guarda libvirt la definición

```bash
ls /etc/libvirt/storage/
```

```
default.xml  labs.xml
```

```bash
ls /etc/libvirt/storage/autostart/
```

```
default.xml -> ../default.xml
labs.xml -> ../labs.xml
```

El autostart funciona mediante symlinks, similar a systemd:

```
/etc/libvirt/storage/
├── default.xml          ← definición del pool
├── labs.xml             ← definición del pool
└── autostart/
    ├── default.xml → ../default.xml   ← symlink = autostart
    └── labs.xml → ../labs.xml         ← symlink = autostart
```

---

## Gestionar volúmenes dentro de pools

Los volúmenes son las imágenes de disco individuales dentro de un pool.

### Crear un volumen

```bash
# Sintaxis: virsh vol-create-as POOL NOMBRE TAMAÑO --format FORMATO
virsh vol-create-as labs base_debian.qcow2 20G --format qcow2
```

```
Vol base_debian.qcow2 created
```

Esto crea `/srv/lab-images/base_debian.qcow2` con:
- Formato qcow2
- Tamaño virtual de 20 GiB
- Permisos correctos (`qemu:qemu` o `root:root` según configuración)
- Etiqueta SELinux correcta (si aplica)

### Listar volúmenes

```bash
virsh vol-list labs
```

```
 Name                Path
-----------------------------------------------------------
 base_debian.qcow2   /srv/lab-images/base_debian.qcow2
```

### Información de un volumen

```bash
virsh vol-info base_debian.qcow2 --pool labs
```

```
Name:           base_debian.qcow2
Type:           file
Capacity:       20.00 GiB
Allocation:     196.00 KiB
```

Equivale a `qemu-img info`, pero integrado con libvirt. `Capacity` es el
tamaño virtual, `Allocation` es el tamaño real en disco.

### Ruta completa de un volumen

```bash
virsh vol-path base_debian.qcow2 --pool labs
```

```
/srv/lab-images/base_debian.qcow2
```

Útil en scripts donde necesitas la ruta absoluta para pasársela a
`virt-install` o `qemu-system`.

### Clonar un volumen

```bash
virsh vol-clone base_debian.qcow2 vm1_debian.qcow2 --pool labs
```

```
Vol vm1_debian.qcow2 cloned from base_debian.qcow2
```

Esto hace una **copia completa** (no un overlay). Para overlays, sigue
usando `qemu-img create -b`.

### Redimensionar un volumen

```bash
virsh vol-resize base_debian.qcow2 30G --pool labs
```

```
Size of volume 'base_debian.qcow2' successfully changed to 30G
```

### Eliminar un volumen

```bash
virsh vol-delete base_debian.qcow2 --pool labs
```

```
Vol base_debian.qcow2 deleted
```

> **Cuidado**: `vol-delete` borra el archivo del disco. No pide confirmación.

### Subir una imagen existente como volumen

Si ya tienes una imagen creada con `qemu-img` y quieres que aparezca en un
pool:

```bash
# Opción 1: refrescar el pool (si el archivo ya está en el directorio)
cp mi_imagen.qcow2 /srv/lab-images/
virsh pool-refresh labs
virsh vol-list labs   # ahora aparece mi_imagen.qcow2

# Opción 2: subir un archivo desde cualquier ruta
virsh vol-upload base_debian.qcow2 /tmp/descarga.qcow2 --pool labs
```

`pool-refresh` le dice a libvirt que re-escanee el directorio y registre
archivos nuevos o eliminados que no pasaron por `virsh vol-*`.

---

## Tipos de pools

El tipo `dir` (directorio) es el más simple y el que usaremos en el curso,
pero libvirt soporta muchos backends:

```
┌──────────────────────────────────────────────────────────────────┐
│                    TIPOS DE STORAGE POOL                        │
├─────────────┬────────────────────────────────────────────────────┤
│ Tipo        │ Descripción                                       │
├─────────────┼────────────────────────────────────────────────────┤
│ dir         │ Directorio del filesystem local                   │
│             │ /var/lib/libvirt/images/, /srv/vms/               │
├─────────────┼────────────────────────────────────────────────────┤
│ fs          │ Filesystem formateado en un bloque dedicado        │
│             │ Monta un /dev/sdX con ext4/xfs                    │
├─────────────┼────────────────────────────────────────────────────┤
│ netfs       │ Filesystem de red (NFS, GlusterFS)                │
│             │ nfs-server:/export/vms                            │
├─────────────┼────────────────────────────────────────────────────┤
│ logical     │ Grupo de volúmenes LVM                            │
│             │ Cada volumen es un LV                             │
├─────────────┼────────────────────────────────────────────────────┤
│ disk        │ Disco completo con tabla de particiones            │
│             │ Cada volumen es una partición                     │
├─────────────┼────────────────────────────────────────────────────┤
│ iscsi       │ Target iSCSI remoto                               │
│             │ Cada LUN es un volumen                            │
├─────────────┼────────────────────────────────────────────────────┤
│ zfs         │ Pool ZFS                                          │
│             │ Cada volumen es un zvol                           │
├─────────────┼────────────────────────────────────────────────────┤
│ rbd         │ Ceph RADOS Block Device                           │
│             │ Almacenamiento distribuido                        │
└─────────────┴────────────────────────────────────────────────────┘
```

Para nuestro curso, `dir` es suficiente. Los tipos `logical` (LVM) y `fs`
los exploraremos cuando estudiemos esos filesystems en bloques posteriores.

### Ejemplo rápido: pool LVM

```bash
# Crear un VG primero (fuera de libvirt)
pvcreate /dev/sdb
vgcreate vg_vms /dev/sdb

# Registrar el VG como pool de libvirt
virsh pool-define-as lvm_pool logical \
    --source-name vg_vms --target /dev/vg_vms
virsh pool-start lvm_pool
virsh pool-autostart lvm_pool

# Crear un volumen (será un LV)
virsh vol-create-as lvm_pool vm_root 20G
# Equivale a: lvcreate -L 20G -n vm_root vg_vms
```

La ventaja de LVM: los volúmenes son dispositivos de bloque reales (no
archivos), eliminando la capa de filesystem intermedia. Rendimiento
ligeramente superior para cargas I/O intensivas.

---

## XML de definición de pools

Aunque `pool-define-as` crea el XML por ti, a veces necesitas más control.
Puedes definir un pool desde un archivo XML:

### Pool tipo dir

```xml
<pool type='dir'>
  <name>labs</name>
  <target>
    <path>/srv/lab-images</path>
    <permissions>
      <mode>0711</mode>
      <owner>0</owner>
      <group>0</group>
    </permissions>
  </target>
</pool>
```

```bash
virsh pool-define pool_labs.xml
virsh pool-build labs
virsh pool-start labs
virsh pool-autostart labs
```

### Pool tipo netfs (NFS)

```xml
<pool type='netfs'>
  <name>nfs_vms</name>
  <source>
    <host name='192.168.122.1'/>
    <dir path='/export/vms'/>
    <format type='nfs'/>
  </source>
  <target>
    <path>/mnt/nfs_vms</path>
  </target>
</pool>
```

### Volumen desde XML

También puedes definir volúmenes con XML cuando necesitas parámetros
avanzados:

```xml
<volume type='file'>
  <name>base_debian.qcow2</name>
  <capacity unit='GiB'>20</capacity>
  <allocation unit='KiB'>196</allocation>
  <target>
    <format type='qcow2'/>
    <permissions>
      <mode>0600</mode>
      <owner>107</owner>
      <group>107</group>
    </permissions>
    <features>
      <lazy_refcounts/>
    </features>
  </target>
</volume>
```

```bash
virsh vol-create labs vol_base.xml
```

---

## Permisos y SELinux

Uno de los beneficios principales de los pools es que libvirt gestiona
permisos automáticamente. Pero conviene entender qué hace.

### Permisos de archivo

Cuando libvirt crea un volumen, aplica la propiedad según la configuración
de `/etc/libvirt/qemu.conf`:

```
# En qemu.conf:
user = "qemu"       # ← el proceso QEMU corre como este usuario
group = "qemu"      # ← y este grupo

# El volumen se crea como:
# -rw------- 1 qemu qemu 20G base_debian.qcow2
```

Si usas `qemu:///session` (conexión no-root), el volumen se crea con tu
usuario normal.

### SELinux (Fedora/RHEL)

En distribuciones con SELinux enforcing, libvirt etiqueta los volúmenes con
el contexto `svirt_image_t`:

```bash
ls -Z /var/lib/libvirt/images/
```

```
system_u:object_r:svirt_image_t:s0:c100,c200  vm1.qcow2
```

Si creas una imagen manualmente fuera de un pool y luego intentas usarla
con libvirt, SELinux puede bloquear el acceso:

```bash
# Síntoma: la VM no arranca, error de permisos
# Diagnóstico:
ausearch -m avc -ts recent | grep qemu

# Solución: restaurar el contexto SELinux correcto
restorecon -v /srv/lab-images/mi_imagen.qcow2

# O mejor: usar virsh pool-refresh para que libvirt re-etiquete
virsh pool-refresh labs
```

### Permisos del directorio del pool

El directorio del pool necesita permisos que permitan al proceso QEMU
acceder:

```bash
# pool-build aplica esto automáticamente:
ls -ld /srv/lab-images/
# drwx--x--x 2 root root 4096 ... /srv/lab-images/
#      ^ ^ ^
#      │ │ └── otros: execute (para que qemu pueda traverse)
#      │ └──── grupo: execute
#      └────── owner: rwx
```

El modo `0711` permite que cualquier usuario (incluido `qemu`) pueda entrar
al directorio, pero solo `root` puede listar su contenido. Cada archivo
dentro tiene sus propios permisos (`0600` para `qemu:qemu`).

> **Predicción**: si creas un directorio con `mkdir` y permisos `700`
> (solo root), y luego lo registras como pool, el proceso QEMU (que corre
> como usuario `qemu`) no podrá acceder a los archivos dentro. `pool-build`
> previene esto aplicando `711`.

---

## Errores comunes

### 1. Crear volumen sin arrancar el pool

```bash
# ❌ Pool definido pero no arrancado
virsh vol-create-as labs disco.qcow2 20G --format qcow2
# error: failed to get pool 'labs'
# error: Requested operation is not valid: storage pool 'labs' is not active

# ✅ Arrancar primero
virsh pool-start labs
virsh vol-create-as labs disco.qcow2 20G --format qcow2
```

### 2. Olvidar pool-refresh tras copiar archivos manualmente

```bash
# Copias un archivo al directorio del pool
cp /tmp/nueva_imagen.qcow2 /srv/lab-images/

# ❌ virsh no lo ve
virsh vol-list labs
# (la imagen no aparece)

# ✅ Refrescar el pool
virsh pool-refresh labs
virsh vol-list labs
# nueva_imagen.qcow2 ahora aparece
```

libvirt no monitoriza el directorio con inotify. Solo escanea al arrancar
el pool o con `pool-refresh` explícito.

### 3. Confundir pool-delete con pool-undefine

```bash
# pool-undefine: elimina la DEFINICIÓN del pool de libvirt
#                (los archivos quedan intactos)
virsh pool-undefine labs
# El directorio /srv/lab-images/ sigue existiendo con sus archivos

# pool-delete: BORRA el recurso subyacente (¡EL DIRECTORIO Y SU CONTENIDO!)
virsh pool-delete labs
# /srv/lab-images/ ha sido ELIMINADO
```

El orden seguro para eliminar completamente un pool:

```bash
virsh pool-stop labs       # desactivar
virsh pool-undefine labs   # eliminar definición
# Los archivos siguen en /srv/lab-images/ — puedes borrarlos manualmente si quieres
```

### 4. Pool en directorio sin permisos adecuados

```bash
# ❌ Directorio con permisos restrictivos
mkdir /root/secret_vms
chmod 700 /root/secret_vms
virsh pool-define-as bad_pool dir --target /root/secret_vms
virsh pool-build bad_pool    # puede fallar o dejar permisos incorrectos
virsh pool-start bad_pool
virsh vol-create-as bad_pool test.qcow2 10G --format qcow2
# La VM no arrancará: QEMU (usuario qemu) no puede acceder a /root/

# ✅ Usar directorios accesibles
# /srv/lab-images, /var/lib/libvirt/images/, etc.
# O configurar qemu.conf para correr como root (no recomendado)
```

El problema no es solo el directorio del pool, sino **toda la ruta**. Si
algún directorio padre tiene permisos `700`, QEMU no puede llegar al archivo.

### 5. Pool con el mismo nombre que uno existente

```bash
# ❌ Nombre duplicado
virsh pool-define-as default dir --target /otro/directorio
# error: pool 'default' already exists

# ✅ Usar nombre único
virsh pool-define-as otro_pool dir --target /otro/directorio
```

---

## Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║  STORAGE POOLS DE LIBVIRT — REFERENCIA RÁPIDA                      ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CICLO DE VIDA DEL POOL:                                           ║
║    virsh pool-define-as NAME dir --target /path  # definir         ║
║    virsh pool-build NAME                         # crear dir       ║
║    virsh pool-start NAME                         # activar         ║
║    virsh pool-autostart NAME                     # auto al boot    ║
║    virsh pool-stop NAME                          # desactivar      ║
║    virsh pool-undefine NAME                      # borrar definic. ║
║    virsh pool-delete NAME                        # ¡BORRAR DATOS!  ║
║                                                                    ║
║  CONSULTAR POOLS:                                                  ║
║    virsh pool-list --all                         # listar todos    ║
║    virsh pool-info NAME                          # detalles        ║
║    virsh pool-dumpxml NAME                       # XML completo    ║
║                                                                    ║
║  GESTIONAR VOLÚMENES:                                              ║
║    virsh vol-create-as POOL VOL SIZE --format FMT  # crear         ║
║    virsh vol-list POOL                             # listar        ║
║    virsh vol-info VOL --pool POOL                  # detalles      ║
║    virsh vol-path VOL --pool POOL                  # ruta absoluta ║
║    virsh vol-clone VOL NUEVO --pool POOL           # clonar        ║
║    virsh vol-resize VOL SIZE --pool POOL           # redimensionar ║
║    virsh vol-delete VOL --pool POOL                # eliminar      ║
║                                                                    ║
║  SINCRONIZAR CON CAMBIOS MANUALES:                                 ║
║    virsh pool-refresh POOL                       # re-escanear dir ║
║                                                                    ║
║  REGLAS DE ORO:                                                    ║
║    • pool-start antes de operar con volúmenes                      ║
║    • pool-refresh después de copiar archivos manualmente           ║
║    • pool-undefine ≠ pool-delete (uno borra definición, otro datos)║
║    • Permisos 0711 en directorio, 0600 en archivos                 ║
║    • Toda la ruta al directorio debe ser traversable por QEMU      ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Crear y gestionar un pool custom

**Objetivo**: practicar el ciclo completo de un storage pool.

1. Crea un directorio para el pool:
   ```bash
   sudo mkdir -p /srv/curso-labs
   ```

2. Define el pool con virsh:
   ```bash
   sudo virsh pool-define-as curso_labs dir --target /srv/curso-labs
   ```

3. Verifica que aparece como inactivo:
   ```bash
   sudo virsh pool-list --all
   ```
   Deberías ver `curso_labs` con `State: inactive`.

4. Construye, arranca y configura autostart:
   ```bash
   sudo virsh pool-build curso_labs
   sudo virsh pool-start curso_labs
   sudo virsh pool-autostart curso_labs
   ```

5. Verifica el estado final:
   ```bash
   sudo virsh pool-info curso_labs
   sudo virsh pool-list --all
   ```

6. Examina el XML generado:
   ```bash
   sudo virsh pool-dumpxml curso_labs
   ```
   Identifica: `<path>`, `<mode>`, `<owner>`, `<group>`.

7. Verifica que los archivos de definición existen:
   ```bash
   ls -la /etc/libvirt/storage/curso_labs.xml
   ls -la /etc/libvirt/storage/autostart/curso_labs.xml
   ```

8. Comprueba los permisos del directorio:
   ```bash
   ls -ld /srv/curso-labs
   stat -c '%a %U:%G' /srv/curso-labs
   ```

**Pregunta de reflexión**: si reiniciases libvirtd ahora mismo, ¿el pool
`curso_labs` estaría disponible automáticamente? ¿Qué mecanismo lo garantiza?

---

### Ejercicio 2: Volúmenes dentro del pool

**Objetivo**: crear, inspeccionar y eliminar volúmenes con virsh.

1. Crea tres volúmenes en el pool `curso_labs`:
   ```bash
   sudo virsh vol-create-as curso_labs base_debian.qcow2 20G --format qcow2
   sudo virsh vol-create-as curso_labs datos_extra.qcow2 5G --format qcow2
   sudo virsh vol-create-as curso_labs test_raw.img 2G --format raw
   ```

2. Lista los volúmenes:
   ```bash
   sudo virsh vol-list curso_labs
   ```

3. Consulta la información de cada uno. Para cada volumen, anota:
   - `Capacity` (tamaño virtual)
   - `Allocation` (tamaño real en disco)
   ```bash
   sudo virsh vol-info base_debian.qcow2 --pool curso_labs
   sudo virsh vol-info test_raw.img --pool curso_labs
   ```
   ¿Por qué `test_raw.img` muestra `Allocation` cercano a 0 y no 2 GiB?

4. Obtén la ruta absoluta de un volumen:
   ```bash
   sudo virsh vol-path base_debian.qcow2 --pool curso_labs
   ```

5. Verifica que el archivo existe en el filesystem:
   ```bash
   ls -lh /srv/curso-labs/
   ```

6. Clona un volumen:
   ```bash
   sudo virsh vol-clone base_debian.qcow2 base_debian_copia.qcow2 --pool curso_labs
   sudo virsh vol-list curso_labs
   ```

7. Simula un cambio manual: crea un archivo con `qemu-img` directamente en
   el directorio del pool:
   ```bash
   sudo qemu-img create -f qcow2 /srv/curso-labs/manual.qcow2 10G
   sudo virsh vol-list curso_labs   # ¿aparece manual.qcow2?
   sudo virsh pool-refresh curso_labs
   sudo virsh vol-list curso_labs   # ¿y ahora?
   ```

8. Limpia eliminando volúmenes:
   ```bash
   sudo virsh vol-delete manual.qcow2 --pool curso_labs
   ls /srv/curso-labs/manual.qcow2   # ¿sigue existiendo el archivo?
   ```

**Pregunta de reflexión**: `virsh vol-clone` hace una copia completa del
volumen. Si lo que quieres es un overlay ligero (como vimos en T02), ¿puedes
crearlo con `virsh vol-create-as` o necesitas usar `qemu-img create -b`
directamente?

---

### Ejercicio 3: Pool completo para patrón de labs

**Objetivo**: combinar pools y overlays en un flujo de trabajo de laboratorio.

1. Asegúrate de que el pool `curso_labs` sigue activo:
   ```bash
   sudo virsh pool-info curso_labs
   ```

2. Crea un volumen base:
   ```bash
   sudo virsh vol-create-as curso_labs base_lab.qcow2 10G --format qcow2
   ```

3. Crea overlays con `qemu-img` (libvirt no tiene comando nativo para overlays):
   ```bash
   cd /srv/curso-labs
   sudo qemu-img create -f qcow2 -b base_lab.qcow2 -F qcow2 lab_fs.qcow2
   sudo qemu-img create -f qcow2 -b base_lab.qcow2 -F qcow2 lab_lvm.qcow2
   sudo qemu-img create -f qcow2 -b base_lab.qcow2 -F qcow2 lab_raid.qcow2
   ```

4. Haz que libvirt reconozca los overlays:
   ```bash
   sudo virsh pool-refresh curso_labs
   sudo virsh vol-list curso_labs
   ```

5. Verifica que los overlays muestran el backing file correcto:
   ```bash
   sudo qemu-img info /srv/curso-labs/lab_fs.qcow2
   ```

6. Compara los tamaños en disco:
   ```bash
   sudo virsh vol-info base_lab.qcow2 --pool curso_labs
   sudo virsh vol-info lab_fs.qcow2 --pool curso_labs
   ```

7. Simula "destruir y recrear" un lab:
   ```bash
   sudo virsh vol-delete lab_fs.qcow2 --pool curso_labs
   sudo qemu-img create -f qcow2 -b base_lab.qcow2 -F qcow2 \
       /srv/curso-labs/lab_fs.qcow2
   sudo virsh pool-refresh curso_labs
   sudo virsh vol-list curso_labs
   ```

8. Cuando termines, limpia todo:
   ```bash
   sudo virsh pool-stop curso_labs
   sudo virsh pool-undefine curso_labs
   # Los archivos siguen en /srv/curso-labs/ — verificar:
   ls /srv/curso-labs/
   # Limpiar manualmente si quieres:
   sudo rm -rf /srv/curso-labs
   ```

**Pregunta de reflexión**: en el paso 3 usamos `qemu-img` directamente porque
`virsh vol-create-as` no soporta crear overlays con backing file. ¿Consideras
esto una limitación de libvirt o una decisión de diseño deliberada? ¿Qué
implicaría para libvirt rastrear las dependencias entre backing files?
