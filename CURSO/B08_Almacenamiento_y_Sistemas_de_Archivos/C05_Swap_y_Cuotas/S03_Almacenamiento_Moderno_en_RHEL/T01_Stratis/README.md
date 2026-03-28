# Stratis: Almacenamiento Pool-Based Simplificado

## Índice

1. [Qué problema resuelve Stratis](#1-qué-problema-resuelve-stratis)
2. [Arquitectura de Stratis](#2-arquitectura-de-stratis)
3. [Instalación y activación](#3-instalación-y-activación)
4. [Pools](#4-pools)
5. [Filesystems](#5-filesystems)
6. [Snapshots](#6-snapshots)
7. [Thin provisioning y monitorización](#7-thin-provisioning-y-monitorización)
8. [Persistencia con fstab](#8-persistencia-con-fstab)
9. [Destrucción y limpieza](#9-destrucción-y-limpieza)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué problema resuelve Stratis

Para gestionar almacenamiento avanzado en Linux tradicionalmente necesitas combinar
múltiples capas manualmente:

```
Capa tradicional:
┌─────────────┐
│  Filesystem  │   mkfs.xfs / mkfs.ext4
├─────────────┤
│     LVM      │   pvcreate → vgcreate → lvcreate
├─────────────┤
│  Disco/RAID  │   /dev/sda, /dev/md0
└─────────────┘

Cada capa se configura con herramientas distintas,
cada redimensionamiento requiere coordinar varias capas.
```

Stratis fue creado por Red Hat para **simplificar** esta complejidad. En lugar de
configurar cada capa por separado, Stratis las gestiona como una unidad:

```
Capa Stratis:
┌─────────────────────────────────┐
│          stratis CLI             │  ← Una sola herramienta
├─────────────────────────────────┤
│  Stratis daemon (stratisd)       │
├────────┬────────┬───────────────┤
│  XFS   │  thin  │  Device Mapper │  ← Stratis coordina todo
│  (auto)│  prov. │  (auto)        │    internamente
├────────┴────────┴───────────────┤
│         Block devices            │
└─────────────────────────────────┘
```

### Características clave

- **Pool-based**: los discos se agrupan en un pool, similar a un VG de LVM
- **Thin provisioning automático**: los filesystems no reservan espacio por adelantado
- **XFS obligatorio**: Stratis siempre crea XFS internamente (no hay elección)
- **Snapshots integrados**: sin necesidad de calcular tamaños de snapshot
- **Sin RAID propio** (aún): Stratis no implementa redundancia internamente; si
  necesitas RAID, coloca los discos en un array mdadm primero y dale el array a Stratis
- **Disponibilidad**: Fedora, RHEL 8+, CentOS Stream — no disponible en Debian/Ubuntu
  por defecto

### Stratis NO es un filesystem

Un error conceptual frecuente: Stratis **no** es un filesystem. Es una **capa de
gestión** que internamente usa Device Mapper para thin provisioning y crea XFS sobre
los volúmenes thin. Lo que montas sigue siendo XFS.

---

## 2. Arquitectura de Stratis

```
                    stratis CLI
                        │
                        ▼
                   ┌─────────┐
                   │ stratisd │  ← Daemon (D-Bus API)
                   └────┬────┘
                        │
           ┌────────────┼────────────┐
           ▼            ▼            ▼
      ┌────────┐  ┌────────┐  ┌────────┐
      │ Pool A │  │ Pool B │  │  ...   │
      └───┬────┘  └───┬────┘  └────────┘
          │           │
    ┌─────┼─────┐     │
    ▼     ▼     ▼     ▼
  /dev  /dev  /dev  /dev        ← Block devices (discos, particiones, RAID)
  sdb   sdc   sdd   sde

Dentro de cada pool:
┌─────────────────────────────────────────────────┐
│                    Pool                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐      │
│  │ data tier │  │ data tier │  │cache tier│      │
│  │  /dev/sdb │  │  /dev/sdc │  │ /dev/nvme│      │
│  └──────┬───┘  └──────┬───┘  └────┬─────┘      │
│         └──────┬───────┘          │ (opcional)   │
│                ▼                  ▼              │
│  ┌──────────────────────────────────────┐       │
│  │     Device Mapper (thin pool)         │       │
│  ├──────────┬──────────┬────────────────┤       │
│  │  thin LV  │  thin LV  │   thin LV     │       │
│  │   (fs1)   │   (fs2)   │   (snap1)     │       │
│  ├──────────┤──────────┤────────────────┤       │
│  │   XFS    │   XFS    │     XFS        │       │
│  └──────────┘──────────┘────────────────┘       │
└─────────────────────────────────────────────────┘
```

### Componentes internos

| Componente | Rol |
|---|---|
| **stratisd** | Daemon que gestiona pools/filesystems via D-Bus |
| **stratis** | CLI que habla con stratisd |
| **Data tier** | Discos que aportan capacidad al pool |
| **Cache tier** | SSD/NVMe opcional para caché (similar a bcache/dm-cache) |
| **Thin pool** | Device Mapper thin provisioning — Stratis lo crea automáticamente |
| **Filesystem** | Volumen thin + XFS — Stratis lo formatea automáticamente |

### Ruta de los dispositivos

Stratis crea dispositivos en una ruta predecible:

```
/dev/stratis/<pool_name>/<filesystem_name>
```

Por ejemplo:

```
/dev/stratis/mypool/projects
/dev/stratis/mypool/backups
```

Internamente estos son symlinks a dispositivos Device Mapper:

```bash
ls -l /dev/stratis/mypool/projects
# lrwxrwxrwx. 1 root root 10 ... /dev/stratis/mypool/projects -> /dev/dm-4
```

---

## 3. Instalación y activación

### Instalar paquetes

```bash
# Fedora / RHEL 8+ / CentOS Stream
sudo dnf install stratisd stratis-cli
```

- `stratisd`: el daemon
- `stratis-cli`: la herramienta de línea de comandos

### Activar el servicio

```bash
sudo systemctl enable --now stratisd
```

Verificar que está corriendo:

```bash
systemctl status stratisd
# ● stratisd.service - Stratis daemon
#      Active: active (running)
```

### Verificar versión

```bash
stratis --version
# 3.x.x

stratis daemon version
# Muestra la versión del daemon
```

> **Nota**: `stratisd` debe estar corriendo para que cualquier comando `stratis`
> funcione. Si el daemon no está activo, todos los comandos fallarán con un error de
> conexión D-Bus.

---

## 4. Pools

Un pool agrupa uno o más block devices en una única reserva de almacenamiento.

### Crear un pool

```bash
# Pool con un solo disco
sudo stratis pool create mypool /dev/vdb

# Pool con múltiples discos (capacidad combinada)
sudo stratis pool create datapool /dev/vdb /dev/vdc /dev/vdd
```

**Requisitos de los discos**:
- No deben tener particiones ni filesystems existentes
- No deben estar montados
- Deben ser discos completos o particiones sin uso
- Si tienen datos previos, limpiar con `wipefs -a /dev/vdX`

### Listar pools

```bash
stratis pool list
```

Salida típica:

```
Name        Total / Used / Free              Properties   UUID   Alerts
mypool   10.00 GiB / 0.53 GiB / 9.47 GiB   ~Ca,~Cr      abc...
```

Campos importantes:
- **Total**: capacidad total del pool (suma de todos los discos)
- **Used**: espacio consumido (metadata + datos de filesystems)
- **Free**: espacio disponible para nuevos datos
- **Properties**: `~Ca` = sin cache, `Ca` = cache activo; `~Cr` = no cifrado, `Cr` = cifrado

### Añadir discos a un pool existente

```bash
sudo stratis pool add-data mypool /dev/vde
```

El pool crece inmediatamente. Los filesystems existentes pueden usar el espacio
adicional sin ninguna operación extra (gracias al thin provisioning).

No existe operación para **quitar** un disco de un pool sin destruir el pool. Esto es
una limitación importante de Stratis frente a LVM (que permite `pvmove` + `vgreduce`).

### Cache tier

Puedes añadir un dispositivo rápido (SSD/NVMe) como caché:

```bash
sudo stratis pool init-cache mypool /dev/nvme0n1

# Añadir más dispositivos al cache tier
sudo stratis pool add-cache mypool /dev/nvme0n2
```

El cache tier acelera lecturas frecuentes. No aumenta la capacidad del pool.

### Renombrar un pool

```bash
sudo stratis pool rename mypool newpoolname
```

> **Cuidado**: renombrar un pool cambia las rutas `/dev/stratis/<pool>/...`, por lo que
> debes actualizar fstab y cualquier referencia.

---

## 5. Filesystems

### Crear un filesystem

```bash
sudo stratis filesystem create mypool projects
```

Esto internamente:
1. Crea un thin volume en el thin pool de Device Mapper
2. Formatea ese volumen con XFS
3. Crea el symlink `/dev/stratis/mypool/projects`

El filesystem aparece con un tamaño virtual de **1 TiB** por defecto, pero no consume
ese espacio realmente (thin provisioning). Solo ocupa espacio a medida que escribes
datos.

### Listar filesystems

```bash
stratis filesystem list mypool
```

Salida:

```
Pool     Filesystem   Total / Used / Free / Limit          Created             Device                        UUID
mypool   projects     1 TiB / 0.55 GiB / 1023.45 GiB / ~  Mar 20 2026 10:00  /dev/stratis/mypool/projects  abc-123...
```

- **Total**: tamaño virtual (1 TiB por defecto)
- **Used**: datos reales escritos
- **Limit**: `~` significa sin límite explícito (usa todo el pool disponible)

### Montar un filesystem

```bash
sudo mkdir -p /mnt/projects
sudo mount /dev/stratis/mypool/projects /mnt/projects
```

Verificar:

```bash
df -h /mnt/projects
# Filesystem                          Size  Used  Avail  Use%  Mounted on
# /dev/dm-4                           1.0T  7.2G  1017G    1%  /mnt/projects
```

El tamaño mostrado es el **virtual** (1 TiB). El espacio real disponible depende del
pool.

### Establecer un límite de tamaño

Si quieres evitar que un filesystem consuma todo el pool:

```bash
# Establecer límite de 50 GiB
sudo stratis filesystem set-size-limit mypool projects 50GiB

# Verificar
stratis filesystem list mypool
# ... Limit = 50 GiB

# Quitar el límite
sudo stratis filesystem unset-size-limit mypool projects
```

### Renombrar un filesystem

```bash
sudo stratis filesystem rename mypool projects dev-projects
```

Al igual que con pools, actualiza fstab y referencias tras renombrar.

---

## 6. Snapshots

Los snapshots en Stratis son **thin snapshots**: no copian datos, solo registran las
diferencias a partir del momento de creación.

### Crear un snapshot

```bash
sudo stratis filesystem snapshot mypool projects projects-snap-20260320
```

El snapshot es un filesystem completo e independiente que se puede montar:

```bash
sudo mkdir -p /mnt/snap-projects
sudo mount /dev/stratis/mypool/projects-snap-20260320 /mnt/snap-projects
```

### Comportamiento del snapshot

```
Momento de creación:
┌──────────────────┐     ┌──────────────────┐
│    projects       │     │  projects-snap    │
│  datos: A B C D   │     │  datos: A B C D   │  ← Comparten bloques
└──────────────────┘     └──────────────────┘
        (ambos apuntan a los mismos bloques thin)

Después de modificar projects:
┌──────────────────┐     ┌──────────────────┐
│    projects       │     │  projects-snap    │
│  datos: A B' C D  │     │  datos: A B C D   │
└──────────────────┘     └──────────────────┘
         │                        │
         ▼                        ▼
    B' (nuevo)              B (original preservado)
```

### Listar snapshots

Los snapshots aparecen como filesystems normales:

```bash
stratis filesystem list mypool
```

```
Pool     Filesystem              Total / Used / Free / Limit          Created             Device
mypool   projects                1 TiB / 2.5 GiB / ...               Mar 20 2026 10:00  /dev/stratis/mypool/projects
mypool   projects-snap-20260320  1 TiB / 0.55 GiB / ...              Mar 20 2026 11:30  /dev/stratis/mypool/projects-snap-20260320
```

### Revertir a un snapshot

Stratis **no tiene operación de merge/revert** como LVM. Para revertir:

```bash
# 1. Desmontar ambos
sudo umount /mnt/projects
sudo umount /mnt/snap-projects

# 2. Destruir el filesystem original
sudo stratis filesystem destroy mypool projects

# 3. Renombrar el snapshot al nombre original
sudo stratis filesystem rename mypool projects-snap-20260320 projects

# 4. Montar de nuevo
sudo mount /dev/stratis/mypool/projects /mnt/projects
```

Este flujo es más manual que `lvconvert --merge`, pero conceptualmente limpio.

### Eliminar un snapshot

```bash
sudo stratis filesystem destroy mypool projects-snap-20260320
```

Los snapshots no se invalidan ni crecen sin control como los snapshots LVM clásicos
(porque son thin, no CoW sobre espacio reservado).

---

## 7. Thin provisioning y monitorización

### El concepto de overprovisioning

Cada filesystem de Stratis tiene un tamaño virtual de 1 TiB, pero el pool real puede
ser mucho menor. Si creas 5 filesystems sobre un pool de 20 GiB:

```
5 filesystems × 1 TiB virtual = 5 TiB virtuales
Pool real = 20 GiB

Esto funciona porque cada filesystem solo consume
espacio real a medida que escribes datos.
```

El peligro: si la suma de datos reales escritos supera la capacidad del pool, los
filesystems se quedarán sin espacio aunque `df` muestre espacio virtual disponible.

### Monitorizar el uso real

El indicador correcto es el **pool**, no el filesystem individual:

```bash
# Uso real del pool
stratis pool list
```

```
Name     Total / Used / Free
mypool   20.00 GiB / 15.30 GiB / 4.70 GiB
```

Si **Free** se acerca a cero, debes:
1. Añadir más discos: `stratis pool add-data mypool /dev/vdf`
2. Eliminar filesystems/snapshots innecesarios
3. Borrar datos dentro de los filesystems montados

### Alerta de espacio

Stratis genera alertas cuando el pool está cerca de llenarse. Revísalas con:

```bash
stratis pool list
# La columna "Alerts" mostrará advertencias
```

### Qué pasa cuando el pool se llena

Si el pool alcanza el 100% de uso:
- Las escrituras en **todos** los filesystems del pool fallan con errores de I/O
- Los filesystems XFS pueden quedar en estado inconsistente
- Solución: añadir disco inmediatamente y ejecutar `xfs_repair` si es necesario

> **Regla práctica**: monitoriza el pool, no los filesystems individuales. Configura
> alertas antes de que el pool supere el 85% de uso.

---

## 8. Persistencia con fstab

Los symlinks `/dev/stratis/...` **no existen** durante el arranque temprano porque
`stratisd` aún no ha iniciado. Por eso, fstab para Stratis debe usar **UUID** y
especificar `x-systemd.requires=stratisd.service`:

### Obtener el UUID

```bash
# Método 1: desde stratis
stratis filesystem list mypool
# Copiar el UUID de la columna correspondiente

# Método 2: desde blkid
sudo blkid /dev/stratis/mypool/projects
# /dev/stratis/mypool/projects: UUID="xxxxxxxx-xxxx-..." TYPE="xfs"
```

### Entrada en fstab

```
UUID=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx  /mnt/projects  xfs  defaults,x-systemd.requires=stratisd.service  0  0
```

Desglose de las opciones:
- `defaults`: opciones estándar de montaje
- `x-systemd.requires=stratisd.service`: systemd esperará a que stratisd esté activo
  antes de intentar montar este filesystem
- `0 0`: sin dump ni fsck (XFS usa `xfs_repair` manualmente, no fsck al arranque)

### Validar

```bash
sudo systemctl daemon-reload
sudo mount -a
findmnt /mnt/projects
```

> **Error frecuente**: usar `/dev/stratis/mypool/fs` en fstab en lugar de UUID. Esto
> falla porque el symlink no existe hasta que stratisd crea el pool, y fstab se procesa
> antes.

---

## 9. Destrucción y limpieza

El orden de destrucción es inverso al de creación: primero filesystems, luego pool.

### Destruir un filesystem

```bash
# Desmontar primero
sudo umount /mnt/projects

# Destruir
sudo stratis filesystem destroy mypool projects
```

### Destruir un pool

```bash
# Todos los filesystems del pool deben estar destruidos primero
sudo stratis pool destroy mypool
```

Si el pool tiene filesystems activos:

```
Error: Pool has filesystems: projects, backups
```

Debes destruir cada filesystem individualmente antes de destruir el pool.

### Limpieza completa

```bash
# 1. Desmontar todos los filesystems del pool
sudo umount /mnt/projects
sudo umount /mnt/backups

# 2. Destruir filesystems
sudo stratis filesystem destroy mypool projects
sudo stratis filesystem destroy mypool backups

# 3. Destruir pool
sudo stratis pool destroy mypool

# 4. Limpiar entradas de fstab
sudo vi /etc/fstab   # Eliminar líneas de Stratis

# 5. Los discos quedan libres para reutilizar
# (opcionalmente wipefs -a /dev/vdX)
```

---

## 10. Errores comunes

### Error 1: Intentar crear un pool con un disco que tiene datos

```bash
sudo stratis pool create testpool /dev/vdb
# Error: Block device /dev/vdb has existing signatures
```

**Solución**: limpiar firmas previas:

```bash
sudo wipefs -a /dev/vdb
sudo stratis pool create testpool /dev/vdb
```

### Error 2: Usar la ruta /dev/stratis/... en fstab

```
# INCORRECTO — falla en el arranque
/dev/stratis/mypool/projects  /mnt/projects  xfs  defaults  0  0

# CORRECTO — usa UUID + dependencia de systemd
UUID=xxx-xxx  /mnt/projects  xfs  defaults,x-systemd.requires=stratisd.service  0  0
```

El symlink `/dev/stratis/...` no existe hasta que stratisd arranca y ensambla los pools.

### Error 3: Confiar en df para el espacio disponible

```bash
df -h /mnt/projects
# Filesystem   Size  Used  Avail  Use%
# /dev/dm-4    1.0T  5.0G  1019G    1%   ← ¡Esto es virtual!
```

El espacio real es el del **pool**:

```bash
stratis pool list
# mypool   20.00 GiB / 18.50 GiB / 1.50 GiB   ← ¡Solo 1.5 GiB reales libres!
```

### Error 4: Intentar quitar un disco de un pool

```bash
# NO EXISTE este comando
sudo stratis pool remove-data mypool /dev/vdb
# Error
```

Una vez que un disco se añade a un pool, **no se puede quitar** sin destruir todo el
pool. Planifica la asignación de discos cuidadosamente.

### Error 5: Olvidar que stratisd debe estar corriendo

```bash
stratis pool list
# Error: Failed to communicate with stratisd
```

**Solución**:

```bash
sudo systemctl start stratisd
sudo systemctl enable stratisd   # Para que arranque automáticamente
```

---

## 11. Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║                        STRATIS — REFERENCIA RÁPIDA                  ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                      ║
║  INSTALACIÓN Y SERVICIO                                              ║
║  dnf install stratisd stratis-cli                                    ║
║  systemctl enable --now stratisd                                     ║
║                                                                      ║
║  POOLS                                                               ║
║  stratis pool create <pool> <dev> [dev...]    Crear pool             ║
║  stratis pool list                            Listar pools           ║
║  stratis pool add-data <pool> <dev>           Añadir disco           ║
║  stratis pool init-cache <pool> <dev>         Añadir cache SSD       ║
║  stratis pool rename <old> <new>              Renombrar pool         ║
║  stratis pool destroy <pool>                  Destruir pool          ║
║                                                                      ║
║  FILESYSTEMS                                                         ║
║  stratis fs create <pool> <fs>                Crear filesystem       ║
║  stratis fs list [pool]                       Listar filesystems     ║
║  stratis fs rename <pool> <old> <new>         Renombrar              ║
║  stratis fs set-size-limit <pool> <fs> <sz>   Limitar tamaño         ║
║  stratis fs destroy <pool> <fs>               Destruir filesystem    ║
║                                                                      ║
║  SNAPSHOTS                                                           ║
║  stratis fs snapshot <pool> <fs> <snap>       Crear snapshot         ║
║  (los snapshots son filesystems — mismas operaciones)                ║
║                                                                      ║
║  MONTAJE                                                             ║
║  mount /dev/stratis/<pool>/<fs> /mnt/punto                           ║
║                                                                      ║
║  FSTAB                                                               ║
║  UUID=xxx  /mnt/punto  xfs  defaults,x-systemd.requires=            ║
║                              stratisd.service  0  0                  ║
║                                                                      ║
║  MONITORIZACIÓN                                                      ║
║  stratis pool list           ← Espacio REAL (no df)                  ║
║  stratis blockdev list       ← Discos de cada pool                   ║
║                                                                      ║
║  ORDEN DE DESTRUCCIÓN                                                ║
║  umount → fs destroy (todos) → pool destroy                          ║
║                                                                      ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 12. Ejercicios

### Ejercicio 1: Pool y filesystems básicos

En tu VM con al menos dos discos virtuales libres (`/dev/vdb` y `/dev/vdc`):

1. Instala los paquetes de Stratis y activa el daemon
2. Crea un pool llamado `labpool` con `/dev/vdb`
3. Verifica la capacidad del pool con `stratis pool list`
4. Crea dos filesystems: `webdata` y `logs`
5. Monta ambos en `/mnt/webdata` y `/mnt/logs`
6. Escribe 100 MiB de datos en cada uno con `dd if=/dev/urandom of=/mnt/webdata/test.bin bs=1M count=100`
7. Compara la salida de `df -h /mnt/webdata` con `stratis pool list`

**Pregunta de predicción**: ¿Qué tamaño mostrará `df` para `/mnt/webdata`? ¿Coincidirá
con el tamaño real del pool?

8. Ahora añade `/dev/vdc` al pool: `stratis pool add-data labpool /dev/vdc`
9. Verifica que la capacidad total del pool aumentó

**Pregunta de reflexión**: ¿Por qué no necesitaste hacer `xfs_growfs` ni ninguna
operación de redimensionamiento en los filesystems después de añadir un disco al pool?

### Ejercicio 2: Snapshot y recuperación

Continuando con el pool del ejercicio anterior:

1. Crea un archivo importante: `echo "production data v1" > /mnt/webdata/config.txt`
2. Crea un snapshot: `stratis filesystem snapshot labpool webdata webdata-snap`
3. Lista los filesystems y observa que el snapshot aparece como uno más
4. Modifica el archivo original: `echo "production data v2 (broken)" > /mnt/webdata/config.txt`
5. Monta el snapshot en `/mnt/snap-webdata` y verifica que contiene la versión original
6. Ahora simula una reversión:
   - Desmonta ambos filesystems
   - Destruye `webdata`
   - Renombra el snapshot a `webdata`
   - Monta `webdata` de nuevo
7. Verifica que `config.txt` contiene "production data v1"

**Pregunta de reflexión**: ¿En qué se diferencia este flujo de reversión del
`lvconvert --merge` de LVM? ¿Cuál consideras más seguro y por qué?

### Ejercicio 3: fstab y persistencia

1. Obtén el UUID del filesystem `webdata` con `blkid`
2. Añade una entrada en `/etc/fstab` con las opciones correctas para Stratis
3. Desmonta `/mnt/webdata` y prueba con `mount -a`
4. Ahora intenta deliberadamente una entrada incorrecta: cambia el UUID por la ruta
   `/dev/stratis/labpool/webdata` y quita la opción `x-systemd.requires`
5. Ejecuta `mount -a` — ¿funciona ahora? (Sí, porque stratisd ya está corriendo)
6. Piensa: ¿por qué esto funcionaría ahora pero fallaría en un reboot real?
7. Corrige la entrada de fstab con el UUID correcto y la dependencia de systemd
8. Limpieza: destruye todos los filesystems y el pool

**Pregunta de reflexión**: ¿Por qué Stratis requiere `x-systemd.requires=stratisd.service`
en fstab mientras que LVM no necesita una dependencia equivalente? Pista: piensa en
cuándo se ensamblan los VGs de LVM versus cuándo stratisd crea los symlinks de los pools.
