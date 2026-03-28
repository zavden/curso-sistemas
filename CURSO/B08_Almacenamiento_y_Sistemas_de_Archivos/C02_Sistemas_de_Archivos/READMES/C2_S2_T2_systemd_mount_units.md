# systemd mount units

## Índice

1. [De fstab a systemd](#de-fstab-a-systemd)
2. [Unidades .mount](#unidades-mount)
3. [Anatomía de un .mount](#anatomía-de-un-mount)
4. [Crear una unidad .mount](#crear-una-unidad-mount)
5. [Unidades .automount](#unidades-automount)
6. [Crear una unidad .automount](#crear-una-unidad-automount)
7. [Gestión con systemctl](#gestión-con-systemctl)
8. [Dependencias y ordenamiento](#dependencias-y-ordenamiento)
9. [fstab vs .mount — cuándo usar cada uno](#fstab-vs-mount--cuándo-usar-cada-uno)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## De fstab a systemd

En sistemas con systemd, `/etc/fstab` no se lee directamente para montar filesystems. Lo que ocurre es:

```
┌──────────────┐     ┌──────────────────────┐     ┌─────────────┐
│  /etc/fstab  │────►│ systemd-fstab-       │────►│  .mount      │
│              │     │ generator            │     │  units       │
│  (6 campos)  │     │ (traducción          │     │  (dinámicas) │
│              │     │  automática)         │     │              │
└──────────────┘     └──────────────────────┘     └──────────────┘
                                                        │
                                                        ▼
                                                  systemd monta
                                                  el filesystem
```

`systemd-fstab-generator` se ejecuta en el arranque temprano y convierte cada línea de fstab en una unidad `.mount` temporal. Estas unidades generadas se almacenan en `/run/systemd/generator/` y desaparecen tras reiniciar.

Pero también puedes crear **tus propias** unidades `.mount` directamente, sin pasar por fstab. Esto te da acceso a toda la maquinaria de systemd: dependencias, condiciones, reintentos, logs con journald, y montaje bajo demanda con `.automount`.

```bash
# Ver las unidades .mount activas (incluye las generadas desde fstab)
systemctl list-units --type=mount

# Ver las generadas automáticamente desde fstab
ls /run/systemd/generator/*.mount 2>/dev/null
```

---

## Unidades .mount

Una unidad `.mount` es un archivo que describe un punto de montaje. El nombre del archivo **debe corresponder exactamente** al punto de montaje, con las barras (`/`) reemplazadas por guiones (`-`) y sin el guión inicial.

### Regla de nomenclatura

```
Punto de montaje          Nombre del archivo .mount
─────────────────         ────────────────────────────
/                         -.mount
/home                     home.mount
/mnt/data                 mnt-data.mount
/var/lib/docker           var-lib-docker.mount
/mnt/my-disk              mnt-my\x2ddisk.mount  (guión escapado)
```

> **Importante**: el nombre del archivo no es arbitrario. Si el nombre no coincide con el punto de montaje, systemd ignorará la unidad o fallará al activarla. Usa `systemd-escape` para generar nombres correctos:

```bash
# Generar el nombre correcto para un path
systemd-escape -p --suffix=mount /mnt/data
# mnt-data.mount

# Paths con caracteres especiales
systemd-escape -p --suffix=mount /mnt/my-disk
# mnt-my\x2ddisk.mount
```

---

## Anatomía de un .mount

Un archivo `.mount` tiene tres secciones: `[Unit]`, `[Mount]` y `[Install]`.

```ini
# /etc/systemd/system/mnt-data.mount

[Unit]
Description=Mount data disk on /mnt/data
After=blockdev@dev-vdb1.target

[Mount]
What=/dev/disk/by-uuid/f47ac10b-58cc-4372-a567-0e02b2c3d479
Where=/mnt/data
Type=ext4
Options=defaults,noatime

[Install]
WantedBy=multi-user.target
```

### Campos de [Mount]

| Campo | Obligatorio | Equivalente en fstab | Descripción |
|-------|-------------|---------------------|-------------|
| `What=` | Sí | Campo 1 (device) | Dispositivo, UUID path, LABEL path, o recurso de red |
| `Where=` | Sí | Campo 2 (mountpoint) | Punto de montaje — debe coincidir con el nombre del archivo |
| `Type=` | No | Campo 3 (type) | Tipo de filesystem (ext4, xfs, etc.) |
| `Options=` | No | Campo 4 (options) | Opciones de montaje, separadas por comas |
| `SloppyOptions=` | No | — | `yes` para ignorar opciones no reconocidas sin fallar |
| `LazyUnmount=` | No | — | `yes` para lazy umount al desactivar |
| `ForceUnmount=` | No | — | `yes` para forzar umount (NFS) |
| `DirectoryMode=` | No | — | Permisos del directorio si se crea automáticamente |
| `TimeoutSec=` | No | — | Timeout para la operación de montaje |

### Campos de [Unit] relevantes

| Campo | Uso |
|-------|-----|
| `Description=` | Descripción legible |
| `After=` | Montar después de esta unidad |
| `Requires=` | Dependencia dura — si falla, esta unidad también falla |
| `Wants=` | Dependencia suave — si falla, esta unidad continúa |
| `ConditionPathExists=` | Solo montar si existe este path |

### Campos de [Install]

| Campo | Uso |
|-------|-----|
| `WantedBy=multi-user.target` | Montar en arranque normal (equivalente a `auto` en fstab) |
| `WantedBy=local-fs.target` | Montar más temprano, con los filesystems locales |

---

## Crear una unidad .mount

### Paso a paso

```bash
# 1. Determinar el nombre correcto
systemd-escape -p --suffix=mount /mnt/data
# mnt-data.mount

# 2. Crear el punto de montaje
mkdir -p /mnt/data

# 3. Obtener el path estable del dispositivo
ls -l /dev/disk/by-uuid/ | grep vdb1
# ... -> ../../vdb1

# 4. Crear la unidad
cat > /etc/systemd/system/mnt-data.mount << 'EOF'
[Unit]
Description=Mount data partition
After=blockdev@dev-vdb1.target

[Mount]
What=/dev/disk/by-uuid/f47ac10b-58cc-4372-a567-0e02b2c3d479
Where=/mnt/data
Type=ext4
Options=defaults,noatime

[Install]
WantedBy=multi-user.target
EOF

# 5. Recargar systemd
systemctl daemon-reload

# 6. Activar ahora
systemctl start mnt-data.mount

# 7. Habilitar para arranque
systemctl enable mnt-data.mount

# 8. Verificar
systemctl status mnt-data.mount
findmnt /mnt/data
```

> **Predicción**: `systemctl start` montará el filesystem inmediatamente. `systemctl enable` crea un symlink en `multi-user.target.wants/` para que se active en cada arranque. El estado mostrará `active (mounted)`.

### Usando What= con UUID

En `What=` no se usa la sintaxis `UUID=xxxx` de fstab. Se usa el **path completo** del symlink:

```ini
# ✗ Esto NO funciona en .mount
What=UUID=f47ac10b-58cc-4372-a567-0e02b2c3d479

# ✓ Usar el path del symlink
What=/dev/disk/by-uuid/f47ac10b-58cc-4372-a567-0e02b2c3d479

# ✓ O el path del dispositivo (menos estable)
What=/dev/vdb1
```

---

## Unidades .automount

Una unidad `.automount` monta el filesystem **bajo demanda**: el punto de montaje existe, pero el filesystem solo se monta cuando algo accede al directorio. Tras un periodo de inactividad, se puede desmontar automáticamente.

```
┌─────────────────────────────────────────────────────────┐
│                   Flujo .automount                       │
│                                                         │
│   ls /mnt/data ──► autofs intercepta ──► systemd monta  │
│                    el acceso              el .mount       │
│                                          asociado        │
│                                                         │
│   Idle timeout ──► systemd desmonta automáticamente      │
│   (configurable)                                        │
└─────────────────────────────────────────────────────────┘
```

Casos de uso:

- **Montajes de red** (NFS, CIFS): no bloquear el arranque si el servidor no responde
- **Medios removibles**: montar USB o ISO solo cuando se accede
- **Reducir recursos**: no mantener montados filesystems que se usan raramente

El `.automount` siempre necesita un `.mount` asociado con el **mismo nombre base**.

```
mnt-data.mount      ← define CÓMO montar
mnt-data.automount  ← define CUÁNDO montar (bajo demanda)
```

---

## Crear una unidad .automount

### Paso a paso

Partiendo de que ya existe `mnt-data.mount`:

```bash
# 1. Crear la unidad .automount
cat > /etc/systemd/system/mnt-data.automount << 'EOF'
[Unit]
Description=Automount data partition on access

[Automount]
Where=/mnt/data
TimeoutIdleSec=300

[Install]
WantedBy=multi-user.target
EOF

# 2. Recargar y activar
systemctl daemon-reload

# 3. Desactivar el .mount directo (para que no se monte en boot)
systemctl disable mnt-data.mount
systemctl stop mnt-data.mount

# 4. Activar el .automount
systemctl enable mnt-data.automount
systemctl start mnt-data.automount

# 5. Verificar — el automount está "esperando"
systemctl status mnt-data.automount
# ● mnt-data.automount - Automount data partition on access
#    Loaded: loaded
#    Active: active (waiting)

# 6. Acceder al directorio → se monta automáticamente
ls /mnt/data

# 7. Verificar — ahora está montado
systemctl status mnt-data.mount
# Active: active (mounted)
findmnt /mnt/data
```

> **Predicción**: después de `start mnt-data.automount`, el directorio `/mnt/data` existe pero el filesystem NO está montado. Al ejecutar `ls /mnt/data`, systemd intercepta el acceso, monta el filesystem, y devuelve el contenido. Tras 300 segundos (5 minutos) de inactividad, se desmonta automáticamente.

### Campos de [Automount]

| Campo | Descripción |
|-------|-------------|
| `Where=` | Punto de montaje — debe coincidir con el `.mount` asociado |
| `TimeoutIdleSec=` | Segundos de inactividad antes de desmontar (0 = nunca) |
| `DirectoryMode=` | Permisos del directorio autofs |

### Ejemplo: NFS con automount

```ini
# /etc/systemd/system/mnt-nfs.mount
[Unit]
Description=NFS share from server
After=network-online.target
Requires=network-online.target

[Mount]
What=server:/export/data
Where=/mnt/nfs
Type=nfs
Options=vers=4.2,soft,timeo=150
TimeoutSec=30

[Install]
WantedBy=multi-user.target
```

```ini
# /etc/systemd/system/mnt-nfs.automount
[Unit]
Description=Automount NFS on access

[Automount]
Where=/mnt/nfs
TimeoutIdleSec=600

[Install]
WantedBy=multi-user.target
```

Con este par, el recurso NFS solo se monta cuando se accede, y se desmonta tras 10 minutos de inactividad. El arranque nunca se bloquea esperando al servidor NFS.

---

## Gestión con systemctl

### Operaciones básicas

```bash
# Montar
systemctl start mnt-data.mount

# Desmontar
systemctl stop mnt-data.mount

# Habilitar en arranque
systemctl enable mnt-data.mount

# Deshabilitar
systemctl disable mnt-data.mount

# Estado detallado
systemctl status mnt-data.mount

# Ver logs de montaje
journalctl -u mnt-data.mount

# Listar todos los montajes
systemctl list-units --type=mount --state=active

# Listar todos los automount
systemctl list-units --type=automount
```

### Recargar tras cambios

```bash
# Después de editar un .mount o .automount
systemctl daemon-reload

# Reaplicar cambios en unidad activa
systemctl daemon-reload
systemctl restart mnt-data.mount
```

### Ver el contenido de una unidad (incluidas las generadas desde fstab)

```bash
# Ver una unidad generada desde fstab
systemctl cat home.mount
# Muestra el contenido aunque esté en /run/systemd/generator/

# Ver la configuración efectiva (incluye overrides)
systemctl show mnt-data.mount
```

---

## Dependencias y ordenamiento

Una de las ventajas principales de `.mount` sobre fstab es el control fino de dependencias.

### Targets relevantes

```
┌─────────────────────────────────────────────────┐
│              Orden de arranque                   │
│                                                  │
│  local-fs-pre.target                             │
│       │                                          │
│       ▼                                          │
│  local-fs.target  ← filesystems locales listos   │
│       │                                          │
│       ▼                                          │
│  network.target   ← red básica configurada       │
│       │                                          │
│       ▼                                          │
│  network-online.target ← red completamente up    │
│       │                                          │
│       ▼                                          │
│  remote-fs.target ← NFS, CIFS montados           │
│       │                                          │
│       ▼                                          │
│  multi-user.target ← sistema operativo           │
└─────────────────────────────────────────────────┘
```

### Ejemplo: montar después de que LVM esté listo

```ini
[Unit]
Description=Mount LVM volume for database
After=lvm2-activation.service
Requires=lvm2-activation.service

[Mount]
What=/dev/mapper/vg0-dbdata
Where=/var/lib/pgsql
Type=xfs
Options=defaults,noatime

[Install]
WantedBy=multi-user.target
```

### Ejemplo: servicio que depende de un montaje

```ini
# /etc/systemd/system/myapp.service
[Unit]
Description=My Application
After=mnt-data.mount
Requires=mnt-data.mount

[Service]
ExecStart=/usr/bin/myapp
```

Con `Requires=` + `After=`, systemd garantiza que `/mnt/data` esté montado antes de iniciar `myapp`. Si el montaje falla, el servicio no se inicia. Con fstab puro, esto no es posible de forma declarativa.

---

## fstab vs .mount — cuándo usar cada uno

```
┌────────────────────────┬─────────────────┬──────────────────────┐
│ Criterio               │ fstab           │ .mount / .automount  │
├────────────────────────┼─────────────────┼──────────────────────┤
│ Simplicidad            │ ✓ Una línea     │ ✗ Archivo completo   │
│ Universalidad          │ ✓ Funciona sin  │ ✗ Solo con systemd   │
│                        │   systemd       │                      │
│ Montaje bajo demanda   │ ✗ No soportado  │ ✓ .automount         │
│ Dependencias finas     │ ✗ Solo _netdev  │ ✓ After=, Requires=  │
│ Logs con journald      │ ~ Indirecto     │ ✓ journalctl -u      │
│ Control individual     │ ✗ mount -a todo │ ✓ start/stop por     │
│                        │                 │   unidad             │
│ Idle timeout           │ ✗ No            │ ✓ TimeoutIdleSec=    │
│ Condiciones            │ ✗ No            │ ✓ ConditionPath...   │
│ Integración servicios  │ ✗ No            │ ✓ Requires=, After=  │
│ Documentación estándar │ ✓ Conocido por  │ ~ Menos conocido     │
│                        │   todos         │                      │
│ Examen RHCSA/LFCS      │ ✓ Esperado      │ ✓ También esperado   │
└────────────────────────┴─────────────────┴──────────────────────┘
```

### Recomendación práctica

| Escenario | Usar |
|-----------|------|
| Discos locales simples (/, /home, /var, swap) | **fstab** |
| Montajes de red (NFS, CIFS) | **.automount** — evita bloquear el boot |
| Filesystem que un servicio necesita antes de iniciar | **.mount** con dependencia |
| Montaje bajo demanda con idle timeout | **.automount** |
| Labs y entornos temporales | **fstab** — más rápido de configurar |
| Producción con requisitos de ordenamiento | **.mount** — control total |

> **No mezclar**: si un punto de montaje está en fstab Y tiene un `.mount` manual, systemd usará el `.mount` manual y emitirá un warning. Elige uno u otro para cada montaje.

---

## Errores comunes

### 1. Nombre del archivo no coincide con Where=

```bash
# ✗ Archivo se llama data.mount pero Where=/mnt/data
# systemd espera mnt-data.mount para Where=/mnt/data

# ✓ Usar systemd-escape para el nombre correcto
systemd-escape -p --suffix=mount /mnt/data
# mnt-data.mount
```

Si el nombre no coincide, `systemctl start` lanzará un error o montará en un lugar inesperado.

### 2. Olvidar daemon-reload

```bash
# ✗ Editar un .mount y esperar que los cambios apliquen
vim /etc/systemd/system/mnt-data.mount
systemctl restart mnt-data.mount  # usa la versión anterior

# ✓ Siempre recargar después de editar
vim /etc/systemd/system/mnt-data.mount
systemctl daemon-reload
systemctl restart mnt-data.mount
```

### 3. Usar UUID= en What= (sintaxis de fstab)

```ini
# ✗ Sintaxis de fstab — no funciona en .mount
[Mount]
What=UUID=f47ac10b-58cc-4372-a567-0e02b2c3d479

# ✓ Path completo del symlink
[Mount]
What=/dev/disk/by-uuid/f47ac10b-58cc-4372-a567-0e02b2c3d479
```

### 4. Activar .mount y .automount simultáneamente

```bash
# ✗ Ambos habilitados — conflicto
systemctl enable mnt-data.mount
systemctl enable mnt-data.automount
# El .automount necesita controlar cuándo se activa el .mount

# ✓ Si usas automount, deshabilita el .mount directo
systemctl disable mnt-data.mount
systemctl enable mnt-data.automount
# El .automount activará el .mount cuando se acceda al directorio
```

### 5. Montaje de red sin dependencia de red

```ini
# ✗ Sin dependencia — intenta montar antes de que haya red
[Unit]
Description=NFS mount

[Mount]
What=server:/share
Where=/mnt/nfs
Type=nfs

# ✓ Con dependencias de red explícitas
[Unit]
Description=NFS mount
After=network-online.target
Requires=network-online.target

[Mount]
What=server:/share
Where=/mnt/nfs
Type=nfs
Options=vers=4.2
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│                systemd mount units — Referencia rápida           │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  NOMENCLATURA:                                                   │
│    /mnt/data  →  mnt-data.mount / mnt-data.automount             │
│    systemd-escape -p --suffix=mount /mnt/my-path                 │
│                                                                  │
│  ESTRUCTURA .mount:                                              │
│    [Unit]                                                        │
│    Description=...                                               │
│    After=...         Requires=...                                │
│                                                                  │
│    [Mount]                                                       │
│    What=/dev/disk/by-uuid/...   ← NO usar UUID=xxx              │
│    Where=/mnt/data                                               │
│    Type=ext4                                                     │
│    Options=defaults,noatime                                      │
│                                                                  │
│    [Install]                                                     │
│    WantedBy=multi-user.target                                    │
│                                                                  │
│  ESTRUCTURA .automount:                                          │
│    [Automount]                                                   │
│    Where=/mnt/data                                               │
│    TimeoutIdleSec=300          ← desmonta tras 5 min inactivo    │
│                                                                  │
│  COMANDOS:                                                       │
│    systemctl daemon-reload              recargar tras editar     │
│    systemctl start  mnt-data.mount      montar ahora             │
│    systemctl stop   mnt-data.mount      desmontar                │
│    systemctl enable mnt-data.mount      montar en cada boot      │
│    systemctl status mnt-data.mount      ver estado               │
│    journalctl -u mnt-data.mount         ver logs                 │
│    systemctl list-units --type=mount    listar montajes          │
│    systemctl cat home.mount             ver contenido de unit    │
│                                                                  │
│  CUÁNDO USAR:                                                    │
│    fstab       → discos locales simples, labs                    │
│    .mount      → dependencias finas, control de servicios        │
│    .automount  → montaje bajo demanda, NFS, idle timeout         │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Crear un .mount para un disco extra

En tu VM de lab con `/dev/vdb1` formateado:

1. Obtén el UUID: `blkid /dev/vdb1`
2. Genera el nombre correcto: `systemd-escape -p --suffix=mount /mnt/data`
3. Crea `/etc/systemd/system/mnt-data.mount` con `What=` usando el path `/dev/disk/by-uuid/...`
4. `systemctl daemon-reload`
5. `systemctl start mnt-data.mount`
6. Verifica con `systemctl status mnt-data.mount` y `findmnt /mnt/data`
7. Habilita con `systemctl enable mnt-data.mount`
8. Reinicia y confirma que persiste
9. Compara con `systemctl cat home.mount` — ¿esa unidad fue generada desde fstab?

> **Pregunta de reflexión**: si tienes el mismo montaje definido en fstab Y en un archivo .mount, ¿cuál tiene prioridad? ¿Qué ocurre con el otro?

### Ejercicio 2: Montaje bajo demanda con .automount

Partiendo del `.mount` del ejercicio anterior:

1. Crea `/etc/systemd/system/mnt-data.automount` con `TimeoutIdleSec=120`
2. `systemctl daemon-reload`
3. `systemctl stop mnt-data.mount` y `systemctl disable mnt-data.mount`
4. `systemctl enable --now mnt-data.automount`
5. Verifica: `systemctl status mnt-data.automount` — debería estar `active (waiting)`
6. Verifica: `systemctl status mnt-data.mount` — debería estar `inactive`
7. Ejecuta `ls /mnt/data` — ¿qué cambia en el status de ambas unidades?
8. Espera 2 minutos sin acceder a `/mnt/data`, luego verifica si se desmontó
9. Revisa los logs: `journalctl -u mnt-data.automount -u mnt-data.mount --since "10 minutes ago"`

> **Pregunta de reflexión**: ¿Qué ventaja tiene `.automount` para montajes NFS en un laptop que no siempre está conectado a la red de la oficina?

### Ejercicio 3: Unidad .mount con dependencia de servicio

1. Crea un `.mount` para `/mnt/appdata` usando `/dev/vdb1`
2. Crea un servicio simple `/etc/systemd/system/myapp.service`:
   ```ini
   [Unit]
   Description=Test application
   After=mnt-appdata.mount
   Requires=mnt-appdata.mount

   [Service]
   ExecStart=/bin/bash -c 'echo "App started, data at /mnt/appdata" && sleep infinity'

   [Install]
   WantedBy=multi-user.target
   ```
3. `systemctl daemon-reload`
4. `systemctl start myapp.service` — ¿se monta `/mnt/appdata` automáticamente?
5. `systemctl stop mnt-appdata.mount` — ¿qué pasa con `myapp.service`?
6. Comprueba con `systemctl status myapp.service`

> **Pregunta de reflexión**: ¿Cuál es la diferencia entre usar `Requires=` y `Wants=` en este escenario? ¿Qué pasaría con el servicio si el montaje falla con cada una?
