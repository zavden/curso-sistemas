# Configuración de cuotas

## Índice

1. [Flujo de configuración](#flujo-de-configuración)
2. [Paso 1: Montar con opciones de cuota](#paso-1-montar-con-opciones-de-cuota)
3. [Paso 2: quotacheck — crear archivos de cuota (ext4)](#paso-2-quotacheck--crear-archivos-de-cuota-ext4)
4. [Paso 3: quotaon — activar enforcement (ext4)](#paso-3-quotaon--activar-enforcement-ext4)
5. [Paso 4: Asignar cuotas](#paso-4-asignar-cuotas)
6. [Configuración en XFS](#configuración-en-xfs)
7. [Persistencia en fstab](#persistencia-en-fstab)
8. [quotaoff — desactivar cuotas](#quotaoff--desactivar-cuotas)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## Flujo de configuración

El proceso varía según el filesystem. ext4 requiere más pasos que XFS.

```
┌──────────────────────────────────────────────────────────┐
│                                                          │
│  ext4:                              XFS:                 │
│                                                          │
│  1. mount -o usrquota,grpquota      1. mount -o usrquota │
│  2. quotacheck -cugm /mount         (cuotas ya activas)  │
│  3. quotaon /mount                  2. Asignar cuotas     │
│  4. Asignar cuotas                                       │
│                                                          │
│  4 pasos                            2 pasos               │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

---

## Paso 1: Montar con opciones de cuota

Las cuotas se habilitan a través de opciones de montaje. Sin estas opciones, las herramientas de cuota no funcionan.

### Opciones de montaje

| Opción | Efecto |
|--------|--------|
| `usrquota` | Habilitar cuotas por usuario |
| `grpquota` | Habilitar cuotas por grupo |
| `prjquota` | Habilitar cuotas por proyecto (solo XFS) |
| `quota` | Alias de `usrquota` en algunos sistemas |
| `noquota` | Desactivar cuotas explícitamente |

### Montar manualmente

```bash
# ext4: usuario y grupo
mount -o usrquota,grpquota /dev/vdb1 /mnt/data

# XFS: usuario y grupo
mount -o usrquota,grpquota /dev/vdb1 /mnt/data

# XFS: usuario, grupo y proyecto
mount -o usrquota,grpquota,prjquota /dev/vdb1 /mnt/data
```

### Remontar un filesystem ya montado

```bash
# Si /home ya está montado sin cuotas:
mount -o remount,usrquota,grpquota /home

# Verificar que las opciones están activas
mount | grep /home
# /dev/vdb1 on /home type ext4 (rw,usrquota,grpquota)

# O con findmnt
findmnt -o TARGET,OPTIONS /home
```

> **XFS importante**: en XFS, las opciones de cuota deben estar presentes en el **primer montaje** después de crear el filesystem (o después de un reinicio). No se pueden habilitar con remount en todos los casos. Si remount no funciona, desmonta y vuelve a montar.

---

## Paso 2: quotacheck — crear archivos de cuota (ext4)

`quotacheck` escanea el filesystem y genera los archivos de cuota que contienen el uso actual de cada usuario y grupo. **Solo necesario para ext4** — XFS no usa archivos de cuota.

### Sintaxis

```bash
quotacheck [opciones] <filesystem>
```

### Opciones principales

| Opción | Efecto |
|--------|--------|
| `-c` | Crear archivos de cuota nuevos (ignorar existentes) |
| `-u` | Escanear cuotas de usuario |
| `-g` | Escanear cuotas de grupo |
| `-m` | No remontar como read-only (necesario si el FS está en uso) |
| `-f` | Forzar escaneo aunque las cuotas parezcan activas |
| `-v` | Verbose |

### Uso típico

```bash
# Crear archivos de cuota para usuarios y grupos
quotacheck -cugm /mnt/data

# Verbose para ver progreso
quotacheck -cugmv /mnt/data
# quotacheck: Scanning /dev/vdb1 [/mnt/data] done
# quotacheck: Cannot stat old user quota file /mnt/data/aquota.user: ...
# quotacheck: Old group file not found. Usage will not be subtracted.
# quotacheck: Checked 150 directories and 1234 files

# Verificar que se crearon los archivos
ls -la /mnt/data/aquota.*
# -rw------- 1 root root 8192 ... /mnt/data/aquota.group
# -rw------- 1 root root 8192 ... /mnt/data/aquota.user
```

### Archivos de cuota

```
/mnt/data/aquota.user    ← cuotas de usuario (formato binario)
/mnt/data/aquota.group   ← cuotas de grupo (formato binario)
```

Estos archivos son **binarios** — no editarlos directamente. Usar `edquota`, `setquota` o `repquota` para interactuar con ellos.

### ¿Cuándo ejecutar quotacheck?

| Situación | Acción |
|-----------|--------|
| Primera vez habilitando cuotas | `quotacheck -cugm` |
| Después de restaurar backup | `quotacheck -ugm` (recalcular) |
| Sospechas de inconsistencia | `quotacheck -ugmf` |
| Rutina periódica | No necesario — el kernel mantiene los archivos actualizados |

> **Predicción**: `quotacheck` con `-c` crea los archivos desde cero. Sin `-c`, actualiza archivos existentes. Si los archivos no existen y no usas `-c`, quotacheck falla.

---

## Paso 3: quotaon — activar enforcement (ext4)

`quotaon` activa el enforcement de cuotas en un filesystem. Sin esto, los archivos de cuota existen pero los límites no se aplican. **Solo necesario para ext4.**

### Sintaxis

```bash
quotaon [opciones] <filesystem>
```

### Uso

```bash
# Activar cuotas de usuario y grupo
quotaon /mnt/data
# O explícitamente:
quotaon -ug /mnt/data

# Activar en todos los filesystems con cuotas en fstab
quotaon -a

# Verbose
quotaon -v /mnt/data
# /dev/vdb1 [/mnt/data]: user quotas turned on
# /dev/vdb1 [/mnt/data]: group quotas turned on
```

### Verificar que las cuotas están activas

```bash
# Verificar con repquota
repquota /mnt/data
# Si funciona → cuotas activas
# Si error → cuotas no activas

# Verificar con quota
quota -v
```

---

## Paso 4: Asignar cuotas

Con las cuotas activas, asigna límites usando `edquota` o `setquota` (cubiertos en T01).

### Flujo completo ext4 — de cero a cuotas activas

```bash
# 1. Crear filesystem
mkfs.ext4 /dev/vdb1

# 2. Montar con opciones de cuota
mkdir -p /mnt/data
mount -o usrquota,grpquota /dev/vdb1 /mnt/data

# 3. Crear archivos de cuota
quotacheck -cugm /mnt/data

# 4. Activar enforcement
quotaon /mnt/data

# 5. Asignar cuotas
setquota -u alice 102400 122880 500 600 /mnt/data
setquota -u bob   102400 122880 500 600 /mnt/data

# 6. Verificar
repquota -s /mnt/data

# 7. Persistencia (fstab)
echo 'UUID=xxx  /mnt/data  ext4  defaults,usrquota,grpquota  0  2' >> /etc/fstab
```

---

## Configuración en XFS

XFS simplifica el proceso porque las cuotas se gestionan a nivel del kernel, sin archivos de cuota separados.

### Flujo completo XFS

```bash
# 1. Crear filesystem
mkfs.xfs /dev/vdb1

# 2. Montar con opciones de cuota
mkdir -p /mnt/data
mount -o usrquota,grpquota /dev/vdb1 /mnt/data

# 3. Asignar cuotas (no necesita quotacheck ni quotaon)
xfs_quota -x -c 'limit bsoft=100m bhard=120m alice' /mnt/data
xfs_quota -x -c 'limit bsoft=100m bhard=120m bob' /mnt/data

# 4. Verificar
xfs_quota -x -c 'report -u -h' /mnt/data

# 5. Persistencia
echo 'UUID=xxx  /mnt/data  xfs  defaults,usrquota,grpquota  0  0' >> /etc/fstab
```

### xfs_quota — comandos principales

```bash
# Reporte de usuarios
xfs_quota -x -c 'report -u -h' /mnt/data
# User quota on /mnt/data (/dev/vdb1)
#                         Blocks
# User ID      Used   Soft   Hard  Warn/Grace
# ---------- ---------------------------------
# alice          30M   100M   120M  00 [------]
# bob            10M   100M   120M  00 [------]

# Asignar cuota de bloques
xfs_quota -x -c 'limit bsoft=100m bhard=120m alice' /mnt/data

# Asignar cuota de inodos
xfs_quota -x -c 'limit isoft=500 ihard=600 alice' /mnt/data

# Cuota de grupo
xfs_quota -x -c 'limit -g bsoft=500m bhard=600m developers' /mnt/data

# Ver estado de enforcement
xfs_quota -x -c 'state' /mnt/data
# User quota state on /mnt/data (/dev/vdb1)
#   Accounting: ON
#   Enforcement: ON

# Desactivar enforcement (mantener accounting)
xfs_quota -x -c 'disable -u' /mnt/data

# Reactivar enforcement
xfs_quota -x -c 'enable -u' /mnt/data
```

### Cuotas por proyecto en XFS

Project quotas permiten limitar directorios específicos, independiente del usuario:

```bash
# 1. Montar con prjquota
mount -o usrquota,prjquota /dev/vdb1 /mnt/data

# 2. Definir proyecto en /etc/projects
echo '42:/mnt/data/siteA' >> /etc/projects

# 3. Nombrar el proyecto en /etc/projid
echo 'siteA:42' >> /etc/projid

# 4. Inicializar directorio del proyecto
xfs_quota -x -c 'project -s siteA' /mnt/data

# 5. Asignar cuota al proyecto
xfs_quota -x -c 'limit -p bsoft=5g bhard=6g siteA' /mnt/data

# 6. Verificar
xfs_quota -x -c 'report -p -h' /mnt/data
```

---

## Persistencia en fstab

Para que las cuotas sobrevivan al reinicio, las opciones de montaje deben estar en fstab.

### ext4

```bash
# fstab con opciones de cuota
UUID=xxx  /mnt/data  ext4  defaults,usrquota,grpquota  0  2
```

Además, en ext4 necesitas que `quotaon` se ejecute al arrancar. Opciones:

```bash
# Opción 1: systemd-quotacheck.service (automático en la mayoría de distros)
# Se ejecuta quotacheck y quotaon automáticamente si fstab tiene usrquota/grpquota
systemctl status systemd-quotacheck.service
systemctl status quotaon.service

# Opción 2: rc.local o script de arranque (legacy)
# No recomendado en sistemas con systemd
```

> En la mayoría de distribuciones modernas con systemd, si fstab tiene `usrquota`/`grpquota`, el sistema ejecuta `quotacheck` y `quotaon` automáticamente al arrancar.

### XFS

```bash
# fstab con opciones de cuota
UUID=xxx  /mnt/data  xfs  defaults,usrquota,grpquota  0  0
```

XFS no necesita servicios adicionales — las cuotas se activan al montar con las opciones correctas. Los límites se almacenan en los metadatos del filesystem, no en archivos separados.

### Verificar después de reiniciar

```bash
# Reiniciar
reboot

# Verificar opciones de montaje
findmnt -o TARGET,FSTYPE,OPTIONS /mnt/data
# TARGET     FSTYPE  OPTIONS
# /mnt/data  ext4    rw,usrquota,grpquota

# Verificar enforcement
repquota -s /mnt/data    # ext4
xfs_quota -x -c 'state' /mnt/data    # XFS

# Verificar que los límites persisten
quota -s alice
```

---

## quotaoff — desactivar cuotas

### ext4

```bash
# Desactivar enforcement (los archivos de cuota se mantienen)
quotaoff /mnt/data

# Desactivar en todos los filesystems
quotaoff -a

# Verbose
quotaoff -v /mnt/data
# /dev/vdb1 [/mnt/data]: user quotas turned off
# /dev/vdb1 [/mnt/data]: group quotas turned off
```

Después de `quotaoff`:
- Los usuarios pueden escribir sin límite
- Los archivos `aquota.user`/`aquota.group` siguen existiendo
- `quotaon` puede reactivar sin repetir `quotacheck`

### XFS

```bash
# Desactivar enforcement (mantener accounting)
xfs_quota -x -c 'disable -u' /mnt/data
xfs_quota -x -c 'disable -g' /mnt/data

# Desactivar todo (accounting y enforcement)
# Requiere remount o reinicio con noquota
umount /mnt/data
mount -o noquota /dev/vdb1 /mnt/data
```

### Eliminar cuotas completamente (ext4)

```bash
# 1. Desactivar
quotaoff /mnt/data

# 2. Eliminar archivos de cuota
rm /mnt/data/aquota.user /mnt/data/aquota.group

# 3. Remontar sin opciones de cuota
mount -o remount /mnt/data
# Y actualizar fstab para quitar usrquota,grpquota
```

---

## Errores comunes

### 1. quotacheck sin -m en filesystem montado

```bash
# ✗ quotacheck intenta remontar como ro — puede fallar en rootfs
quotacheck -cug /home
# quotacheck: Cannot remount filesystem mounted on /home read-only

# ✓ Usar -m para no remontar
quotacheck -cugm /home
```

### 2. Cuotas en XFS con quotacheck/quotaon

```bash
# ✗ Intentar flujo de ext4 en XFS
mount -o usrquota /dev/vdb1 /mnt/data    # XFS
quotacheck -cugm /mnt/data
# quotacheck: Cannot find filesystem to check (XFS no lo necesita)

# ✓ XFS no necesita quotacheck ni quotaon
# Usar xfs_quota directamente
xfs_quota -x -c 'limit bsoft=100m bhard=120m alice' /mnt/data
```

### 3. Olvidar opciones de cuota en fstab

```bash
# ✗ Funciona ahora pero no sobrevive al reinicio
mount -o usrquota,grpquota /dev/vdb1 /mnt/data
quotacheck -cugm /mnt/data
quotaon /mnt/data
# Después de reboot: cuotas desactivadas

# ✓ Actualizar fstab
UUID=xxx  /mnt/data  ext4  defaults,usrquota,grpquota  0  2
```

### 4. Remontar XFS con cuotas cuando no se montó inicialmente con ellas

```bash
# ✗ Remount en XFS puede no aplicar cuotas
mount /dev/vdb1 /mnt/data           # sin usrquota
mount -o remount,usrquota /mnt/data # puede no funcionar en XFS

# ✓ Desmontar y volver a montar
umount /mnt/data
mount -o usrquota /dev/vdb1 /mnt/data
```

### 5. No verificar después de reiniciar

```bash
# ✗ Configurar cuotas y asumir que persisten
# Después de reiniciar: sorpresa, cuotas no activas

# ✓ Siempre verificar tras reiniciar
reboot
repquota -s /mnt/data    # ¿muestra límites?
quota alice              # ¿muestra cuotas?
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│             Configuración de cuotas — Referencia rápida          │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  EXT4 — FLUJO COMPLETO:                                         │
│    mount -o usrquota,grpquota /dev/X /mnt/data                   │
│    quotacheck -cugm /mnt/data                                    │
│    quotaon /mnt/data                                             │
│    setquota -u alice 102400 122880 500 600 /mnt/data             │
│    → fstab: defaults,usrquota,grpquota                           │
│                                                                  │
│  XFS — FLUJO COMPLETO:                                           │
│    mount -o usrquota,grpquota /dev/X /mnt/data                   │
│    xfs_quota -x -c 'limit bsoft=100m bhard=120m alice' /mnt/data │
│    → fstab: defaults,usrquota,grpquota                           │
│                                                                  │
│  HERRAMIENTAS EXT4:                                              │
│    quotacheck -cugm /mount     crear archivos de cuota           │
│    quotaon /mount              activar enforcement               │
│    quotaoff /mount             desactivar enforcement            │
│                                                                  │
│  HERRAMIENTAS XFS:                                               │
│    xfs_quota -x -c 'state' /mount          ver estado            │
│    xfs_quota -x -c 'report -u -h' /mount   reporte              │
│    xfs_quota -x -c 'enable -u' /mount      activar              │
│    xfs_quota -x -c 'disable -u' /mount     desactivar           │
│                                                                  │
│  ARCHIVOS (ext4):                                                │
│    aquota.user   — cuotas de usuario (binario)                   │
│    aquota.group  — cuotas de grupo (binario)                     │
│    Ubicados en la raíz del filesystem con cuotas                 │
│                                                                  │
│  OPCIONES DE MONTAJE:                                            │
│    usrquota      por usuario                                     │
│    grpquota      por grupo                                       │
│    prjquota      por proyecto (solo XFS)                         │
│    noquota       desactivar                                      │
│                                                                  │
│  PERSISTENCIA:                                                   │
│    fstab: agregar usrquota,grpquota a las opciones               │
│    ext4: systemd ejecuta quotacheck+quotaon al arrancar          │
│    XFS: se activan con las opciones de montaje                   │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Configurar cuotas en ext4 desde cero

En tu VM de lab:

1. Crea un filesystem ext4 en `/dev/vdb1`
2. Monta con opciones de cuota:
   ```bash
   mkdir -p /mnt/quotalab
   mount -o usrquota,grpquota /dev/vdb1 /mnt/quotalab
   ```
3. Ejecuta `quotacheck -cugmv /mnt/quotalab`
4. Verifica que se crearon `aquota.user` y `aquota.group`: `ls -la /mnt/quotalab/aquota.*`
5. Activa: `quotaon -v /mnt/quotalab`
6. Asigna cuota a un usuario: `setquota -u root 51200 61440 200 250 /mnt/quotalab`
7. Verifica: `repquota -s /mnt/quotalab`
8. Desactiva: `quotaoff -v /mnt/quotalab`
9. Reactiva: `quotaon -v /mnt/quotalab` — ¿los límites siguen?
10. Limpia: `umount /mnt/quotalab`

> **Pregunta de reflexión**: ¿por qué `quotaoff` seguido de `quotaon` mantiene los límites sin necesidad de `quotacheck`? ¿Dónde están almacenados los límites entre esos dos comandos?

### Ejercicio 2: Configurar cuotas en XFS

1. Crea un filesystem XFS en `/dev/vdc1`
2. Monta con cuotas:
   ```bash
   mkdir -p /mnt/xfsquota
   mount -o usrquota,grpquota /dev/vdc1 /mnt/xfsquota
   ```
3. Verifica el estado: `xfs_quota -x -c 'state' /mnt/xfsquota`
4. Asigna cuota: `xfs_quota -x -c 'limit bsoft=50m bhard=60m root' /mnt/xfsquota`
5. Verifica: `xfs_quota -x -c 'report -u -h' /mnt/xfsquota`
6. Compara: ejecuta también `repquota -s /mnt/xfsquota` — ¿funciona en XFS?
7. Compara el flujo con ext4: ¿cuántos pasos te ahorraste?
8. Limpia: `umount /mnt/xfsquota`

> **Pregunta de reflexión**: XFS no tiene archivos `aquota.*` en el filesystem. ¿Dónde almacena XFS la información de cuotas? ¿Qué ventaja tiene ese enfoque?

### Ejercicio 3: Persistencia tras reinicio

1. Configura cuotas en ext4 (como ejercicio 1) con cuotas asignadas a 2 usuarios
2. Añade a fstab:
   ```bash
   # Obtener UUID
   blkid /dev/vdb1
   echo 'UUID=xxx  /mnt/quotalab  ext4  defaults,usrquota,grpquota  0  2' >> /etc/fstab
   ```
3. Reinicia la VM
4. Verifica:
   - ¿El filesystem se montó? `findmnt /mnt/quotalab`
   - ¿Las opciones de cuota están? `findmnt -o OPTIONS /mnt/quotalab`
   - ¿Las cuotas están activas? `repquota -s /mnt/quotalab`
   - ¿Los límites persisten? `quota -s root`
5. Limpia: edita fstab, desmonta, limpia

> **Pregunta de reflexión**: si el servicio `systemd-quotacheck` no existe en tu sistema, ¿cómo se asegura systemd de ejecutar `quotaon` al arrancar cuando fstab tiene `usrquota`?
