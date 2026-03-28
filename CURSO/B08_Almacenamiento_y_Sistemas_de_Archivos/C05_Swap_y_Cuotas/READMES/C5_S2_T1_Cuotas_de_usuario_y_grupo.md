# Cuotas de usuario y grupo

## Índice

1. [Qué son las cuotas de disco](#qué-son-las-cuotas-de-disco)
2. [Tipos de cuota](#tipos-de-cuota)
3. [Qué se limita: bloques e inodos](#qué-se-limita-bloques-e-inodos)
4. [Herramientas principales](#herramientas-principales)
5. [quota — consultar cuota de un usuario](#quota--consultar-cuota-de-un-usuario)
6. [repquota — reporte de todas las cuotas](#repquota--reporte-de-todas-las-cuotas)
7. [edquota — editar cuotas](#edquota--editar-cuotas)
8. [setquota — asignar cuotas desde la línea de comandos](#setquota--asignar-cuotas-desde-la-línea-de-comandos)
9. [Cuotas en XFS vs ext4](#cuotas-en-xfs-vs-ext4)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## Qué son las cuotas de disco

Las cuotas de disco limitan cuánto espacio en disco y cuántos archivos puede usar un usuario o grupo en un filesystem. Sin cuotas, un solo usuario puede llenar un disco completo y afectar a todos.

```
┌─────────────────────────────────────────────────────┐
│                   Sin cuotas                         │
│                                                     │
│   /home (100 GiB)                                   │
│   ┌─────────────────────────────────────────────┐   │
│   │ alice: 85 GiB  │ bob: 14 GiB │ carol: 1G │   │
│   └─────────────────────────────────────────────┘   │
│   → bob y carol no pueden crecer                     │
│                                                     │
│                   Con cuotas                         │
│                                                     │
│   /home (100 GiB) — cuota: 30 GiB por usuario       │
│   ┌─────────────────────────────────────────────┐   │
│   │ alice: 30 GiB │ bob: 14 GiB │ carol: 1G   │   │
│   │  (LÍMITE)     │ (16G libre) │ (29G libre)  │   │
│   └─────────────────────────────────────────────┘   │
│   → alice no puede pasar de 30 GiB                   │
│   → bob y carol tienen espacio garantizado           │
└─────────────────────────────────────────────────────┘
```

Casos de uso:
- **Servidores multiusuario**: limitar espacio por usuario en `/home`
- **Servidores de correo**: limitar buzones
- **Hosting compartido**: limitar espacio por cliente
- **Entornos educativos**: limitar estudiantes

---

## Tipos de cuota

### Por usuario (usrquota)

Limita el uso de disco de cada usuario individual, basándose en el propietario de los archivos.

```bash
# alice tiene cuota de 5 GiB en /home
# Todos los archivos propiedad de alice en /home cuentan
```

### Por grupo (grpquota)

Limita el uso de disco de un grupo. Todos los archivos propiedad del grupo cuentan contra la cuota del grupo.

```bash
# Grupo "developers" tiene cuota de 50 GiB en /projects
# Todos los archivos con grupo "developers" en /projects cuentan
```

### Por proyecto (prjquota) — solo XFS

Limita el uso de disco por un identificador de proyecto, independiente del usuario o grupo propietario. Permite limitar directorios específicos.

```bash
# Proyecto 42 = /var/www/siteA → cuota de 10 GiB
# Cualquier usuario que escriba ahí consume cuota del proyecto
```

### Comparación

```
┌──────────────┬──────────────────────────────────────────┐
│ Tipo         │ Se basa en...                            │
├──────────────┼──────────────────────────────────────────┤
│ usrquota     │ Propietario (UID) del archivo            │
│ grpquota     │ Grupo (GID) del archivo                  │
│ prjquota     │ ID de proyecto asignado al directorio    │
│              │ (solo XFS)                               │
└──────────────┴──────────────────────────────────────────┘
```

---

## Qué se limita: bloques e inodos

Las cuotas controlan dos recursos:

### Bloques (espacio en disco)

La cantidad de espacio en disco que ocupa un usuario. Se mide en **kilobytes** (1K blocks) o bytes, dependiendo de la herramienta.

```bash
# Un archivo de 5 MiB = ~5120 bloques de 1K
```

### Inodos (número de archivos)

El número de archivos y directorios que un usuario puede crear. Cada archivo, directorio, symlink, etc. consume un inodo.

```bash
# Un usuario con cuota de 1000 inodos puede crear máximo 1000 archivos
# (útil para prevenir abuso con millones de archivos pequeños)
```

### Límites por recurso

Cada recurso tiene dos límites:

```
┌────────────────────────────────────────────────────────┐
│                                                        │
│  Soft limit: "aviso" — se puede exceder temporalmente  │
│  Hard limit: "muro" — nunca se puede exceder           │
│                                                        │
│     0 ─────── soft ─────── hard ─────── ∞              │
│     │          │             │                         │
│     │  uso     │  grace      │  prohibido               │
│     │  normal  │  period     │  (write fails)           │
│                                                        │
│  (Los límites se cubren en detalle en T03)              │
│                                                        │
└────────────────────────────────────────────────────────┘
```

---

## Herramientas principales

```
┌─────────────────────────────────────────────────────────┐
│              Herramientas de cuotas                       │
│                                                         │
│  quota      Ver cuota de un usuario                      │
│  repquota   Reporte de todas las cuotas de un filesystem │
│  edquota    Editar cuotas interactivamente (editor)      │
│  setquota   Asignar cuotas desde la línea de comandos    │
│  quotacheck Escanear filesystem y actualizar archivos    │
│             de cuotas (ext4)                             │
│  quotaon    Habilitar enforcement de cuotas (ext4)       │
│  quotaoff   Deshabilitar enforcement                     │
│  xfs_quota  Herramienta de cuotas específica de XFS      │
│                                                         │
│  Paquetes:                                               │
│    RHEL/AlmaLinux: quota                                 │
│    Debian/Ubuntu:  quota                                 │
└─────────────────────────────────────────────────────────┘
```

```bash
# Instalar herramientas de cuotas
dnf install quota    # RHEL/AlmaLinux
apt install quota    # Debian/Ubuntu
```

---

## quota — consultar cuota de un usuario

`quota` muestra el uso actual y los límites de un usuario o grupo.

### Sintaxis

```bash
quota [opciones] [usuario]
```

### Ejemplos

```bash
# Ver cuota del usuario actual
quota

# Ver cuota de un usuario específico (requiere root)
quota alice
# Disk quotas for user alice (uid 1001):
#    Filesystem   blocks   quota   limit   grace   files   quota   limit   grace
#    /dev/vdb1    52000    50000   60000   6days    150     0       0

# Ver cuota de grupo
quota -g developers
# Disk quotas for group developers (gid 1002):
#    Filesystem   blocks   quota   limit   grace   files   quota   limit   grace
#    /dev/vdb1    120000   200000  250000            450    1000    1500
```

### Interpretar la salida

```
Filesystem   blocks   quota   limit   grace   files   quota   limit   grace
/dev/vdb1    52000    50000   60000   6days    150     0       0
             │        │       │       │        │       │       │
             │        │       │       │        │       │       └─ hard limit inodos
             │        │       │       │        │       └───────── soft limit inodos
             │        │       │       │        └──────────────── archivos actuales
             │        │       │       └───────────────────────── grace period
             │        │       └───────────────────────────────── hard limit bloques
             │        └───────────────────────────────────────── soft limit bloques
             └────────────────────────────────────────────────── uso actual (bloques)

* grace aparece cuando el soft limit fue excedido
* 0 en quota/limit = sin límite
```

### Opciones útiles

| Opción | Efecto |
|--------|--------|
| `-u` | Cuota de usuario (default) |
| `-g` | Cuota de grupo |
| `-s` | Mostrar en unidades legibles (KB, MB, GB) |
| `-v` | Verbose — mostrar también filesystems sin cuota |

```bash
# Legible para humanos
quota -s alice
#    Filesystem   blocks   quota   limit   grace   files   quota   limit   grace
#    /dev/vdb1      51M     49M     59M    6days    150     0       0
```

---

## repquota — reporte de todas las cuotas

`repquota` genera un reporte de todas las cuotas de un filesystem — todos los usuarios o grupos en una sola vista.

### Sintaxis

```bash
repquota [opciones] <filesystem>
```

### Ejemplo

```bash
repquota /home
# *** Report for user quotas on device /dev/vdb1
# Block grace time: 7days; Inode grace time: 7days
#                         Block limits                File limits
# User            used    soft    hard  grace    used  soft  hard  grace
# ----------------------------------------------------------------------
# root      --      20       0       0              3     0     0
# alice     +-   52000   50000   60000  6days     150     0     0
# bob       --   14000   50000   60000            45     0     0
# carol     --    1000   50000   60000            12     0     0
```

### Interpretar los indicadores

```
alice     +-   52000   50000   60000  6days

Los dos caracteres después del nombre indican:
  Primer carácter: estado de bloques
  Segundo carácter: estado de inodos

  -  = dentro del límite
  +  = soft limit excedido (en grace period)

Combinaciones:
  --  = todo OK
  +-  = soft limit de bloques excedido
  -+  = soft limit de inodos excedido
  ++  = ambos soft limits excedidos
```

### Opciones útiles

| Opción | Efecto |
|--------|--------|
| `-a` | Todos los filesystems con cuotas |
| `-u` | Cuotas de usuario (default) |
| `-g` | Cuotas de grupo |
| `-s` | Unidades legibles |
| `-v` | Incluir usuarios sin uso |

```bash
# Reporte de todos los filesystems con cuotas
repquota -as

# Reporte de grupos
repquota -gs /home

# Reporte completo (usuarios y grupos)
repquota -augs
```

---

## edquota — editar cuotas

`edquota` abre un editor (definido por `$EDITOR`) para modificar las cuotas de un usuario o grupo interactivamente.

### Sintaxis

```bash
edquota [opciones] <usuario_o_grupo>
```

### Ejemplo

```bash
edquota alice
```

Se abre un editor con:

```
Disk quotas for user alice (uid 1001):
  Filesystem                   blocks       soft       hard     inodes     soft     hard
  /dev/vdb1                     52000      50000      60000        150        0        0
```

Modifica los campos `soft` y `hard` y guarda. Los campos `blocks` e `inodes` son el uso actual — **no modificar**.

```
# Asignar: 100 MiB soft, 120 MiB hard para bloques
# 500 archivos soft, 600 hard para inodos
Disk quotas for user alice (uid 1001):
  Filesystem                   blocks       soft       hard     inodes     soft     hard
  /dev/vdb1                     52000     102400     122880        150      500      600
```

> **Nota**: los bloques están en unidades de 1K. 100 MiB = 102400 bloques de 1K.

### Opciones útiles

| Opción | Efecto |
|--------|--------|
| `-u` | Editar cuota de usuario (default) |
| `-g` | Editar cuota de grupo |
| `-p <user>` | Copiar cuotas de otro usuario (prototype) |
| `-t` | Editar grace period |

### Copiar cuotas entre usuarios

```bash
# Copiar cuotas de alice a bob y carol
edquota -p alice bob carol

# Útil para aplicar la misma cuota a muchos usuarios
for user in bob carol dave eve; do
    edquota -p alice "$user"
done
```

### Editar grace period

```bash
edquota -t
```

Se abre:

```
Grace period before enforcing soft limits for users:
Time units may be: days, hours, minutes, or seconds
  Filesystem             Block grace period     Inode grace period
  /dev/vdb1                     7days                  7days
```

Modifica el tiempo (ej: `14days`, `168hours`).

---

## setquota — asignar cuotas desde la línea de comandos

`setquota` permite asignar cuotas sin editor interactivo. Ideal para scripts.

### Sintaxis

```bash
setquota -u <usuario> <block_soft> <block_hard> <inode_soft> <inode_hard> <filesystem>
```

### Ejemplos

```bash
# Asignar a alice: 100M soft, 120M hard (bloques), 500/600 (inodos)
setquota -u alice 102400 122880 500 600 /home

# Asignar a grupo developers: 500M soft, 600M hard, sin límite de inodos
setquota -g developers 512000 614400 0 0 /home

# 0 = sin límite
setquota -u bob 0 0 0 0 /home    # eliminar cuotas de bob

# Asignar a múltiples usuarios en un loop
for user in alice bob carol; do
    setquota -u "$user" 102400 122880 500 600 /home
done
```

### setquota vs edquota

```
┌───────────────┬────────────────────┬────────────────────┐
│               │ edquota            │ setquota           │
├───────────────┼────────────────────┼────────────────────┤
│ Interfaz      │ Editor interactivo │ Línea de comandos  │
│ Scripts       │ No                 │ Sí                 │
│ Copiar cuotas │ Sí (-p)            │ No (pero scriptable│
│ Grace period  │ Sí (-t)            │ Sí (-t)            │
│ Cuándo usar   │ Ajustes manuales   │ Automatización     │
└───────────────┴────────────────────┴────────────────────┘
```

---

## Cuotas en XFS vs ext4

Las cuotas se implementan de forma diferente según el filesystem.

### ext4: quota files

ext4 usa archivos especiales (`aquota.user`, `aquota.group`) en la raíz del filesystem para almacenar las cuotas. Requiere `quotacheck` para construirlos y `quotaon` para activar enforcement.

```bash
# Flujo ext4:
mount -o usrquota,grpquota /dev/vdb1 /home
quotacheck -cugm /home       # crear archivos de cuota
quotaon /home                 # activar enforcement
edquota alice                 # asignar cuotas
```

### XFS: cuotas en el kernel

XFS maneja las cuotas a nivel de kernel, sin archivos de cuota separados. Se habilitan con opciones de montaje y se gestionan con `xfs_quota` o las herramientas estándar.

```bash
# Flujo XFS:
mount -o usrquota,grpquota /dev/vdb1 /home
# No necesita quotacheck ni quotaon — ya están activas
xfs_quota -x -c 'limit bsoft=100m bhard=120m alice' /home
```

### Comparación

```
┌──────────────────────┬──────────────────┬──────────────────┐
│ Aspecto              │ ext4             │ XFS              │
├──────────────────────┼──────────────────┼──────────────────┤
│ Archivos de cuota    │ aquota.user/group│ No (en kernel)   │
│ quotacheck           │ Necesario        │ No necesario     │
│ quotaon/quotaoff     │ Necesario        │ No necesario     │
│ Herramienta nativa   │ quota tools      │ xfs_quota        │
│ quota/repquota/edquota│ Sí              │ Sí (compatibles) │
│ Project quotas       │ No               │ Sí (prjquota)    │
│ Opciones de montaje  │ usrquota,grpquota│ usrquota,grpquota│
│                      │                  │ prjquota         │
└──────────────────────┴──────────────────┴──────────────────┘
```

### xfs_quota

```bash
# Modo interactivo
xfs_quota -x /home
> report -u          # ver cuotas de usuarios
> report -g          # ver cuotas de grupos
> limit bsoft=100m bhard=120m alice    # asignar cuota
> quit

# Modo comando
xfs_quota -x -c 'report -u' /home
xfs_quota -x -c 'limit bsoft=100m bhard=120m alice' /home
```

---

## Errores comunes

### 1. Olvidar instalar el paquete quota

```bash
# ✗ Las herramientas no están disponibles por defecto
edquota alice
# -bash: edquota: command not found

# ✓ Instalar primero
dnf install quota
```

### 2. No montar con opciones de cuota

```bash
# ✗ Sin opciones de cuota → cuotas no funcionan
mount /dev/vdb1 /home
edquota alice
# edquota: Cannot get quota for user alice: No such process

# ✓ Montar con usrquota y/o grpquota
mount -o usrquota,grpquota /dev/vdb1 /home
```

### 3. Modificar "blocks" o "inodes" en edquota

```bash
# ✗ Cambiar el campo de uso actual
Disk quotas for user alice (uid 1001):
  Filesystem    blocks    soft    hard    inodes    soft    hard
  /dev/vdb1      52000   50000   60000       150       0       0
#                ^^^^^ NO TOCAR           ^^^ NO TOCAR

# Estos campos son el USO ACTUAL — los calcula el sistema
# Solo modificar soft y hard
```

### 4. Confundir unidades en setquota

```bash
# ✗ Pensar que setquota acepta MB
setquota -u alice 100 120 500 600 /home
# Esto asigna 100 KB y 120 KB — no 100 MB

# ✓ Bloques en unidades de 1K
# 100 MiB = 100 × 1024 = 102400
setquota -u alice 102400 122880 500 600 /home
```

### 5. Olvidar quotacheck/quotaon en ext4

```bash
# ✗ Montar con cuotas y asignar directamente (ext4)
mount -o usrquota /dev/vdb1 /home
edquota alice
# Puede dar error o no enforcar las cuotas

# ✓ En ext4: quotacheck + quotaon
mount -o usrquota /dev/vdb1 /home
quotacheck -cum /home
quotaon /home
edquota alice
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│          Cuotas de usuario y grupo — Referencia rápida           │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  CONSULTAR:                                                      │
│    quota alice                  cuota de un usuario              │
│    quota -g developers          cuota de un grupo                │
│    quota -s alice               en unidades legibles             │
│    repquota /home               reporte de todo el filesystem    │
│    repquota -as                 todos los FS, legible            │
│                                                                  │
│  ASIGNAR (interactivo):                                          │
│    edquota alice                editar con editor                │
│    edquota -g developers        editar grupo                    │
│    edquota -p alice bob carol   copiar cuota de alice            │
│    edquota -t                   editar grace period              │
│                                                                  │
│  ASIGNAR (script):                                               │
│    setquota -u alice BSOFT BHARD ISOFT IHARD /home               │
│    (bloques en unidades de 1K: 100 MiB = 102400)                │
│    setquota -u alice 0 0 0 0 /home    eliminar cuota            │
│                                                                  │
│  XFS:                                                            │
│    xfs_quota -x -c 'report -u' /home                             │
│    xfs_quota -x -c 'limit bsoft=100m bhard=120m alice' /home     │
│                                                                  │
│  TIPOS:                                                          │
│    usrquota    por usuario (UID)                                 │
│    grpquota    por grupo (GID)                                   │
│    prjquota    por proyecto (solo XFS)                            │
│                                                                  │
│  RECURSOS LIMITADOS:                                             │
│    blocks = espacio en disco (1K units)                           │
│    inodes = número de archivos                                   │
│    soft   = límite con grace period                              │
│    hard   = límite absoluto                                      │
│                                                                  │
│  INDICADORES repquota:                                           │
│    --  = OK     +-  = bloques excedidos                          │
│    -+  = inodos excedidos    ++  = ambos excedidos               │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Consultar cuotas existentes

En tu VM de lab (o un servidor con cuotas habilitadas):

1. Instala herramientas: `dnf install quota`
2. Verifica si hay cuotas activas: `repquota -a`
3. Verifica cuota del usuario actual: `quota`
4. Si no hay cuotas, prepara un filesystem para el ejercicio 2:
   ```bash
   # Crear filesystem en /dev/vdb1
   mkfs.ext4 /dev/vdb1
   mkdir -p /mnt/quotatest
   mount -o usrquota,grpquota /dev/vdb1 /mnt/quotatest
   ```
5. Para ext4, inicializar cuotas:
   ```bash
   quotacheck -cugm /mnt/quotatest
   quotaon /mnt/quotatest
   ```
6. Verifica: `repquota /mnt/quotatest` — debería mostrar un reporte vacío

> **Pregunta de reflexión**: ¿por qué `quotacheck` necesita la opción `-m` (no remontar como read-only)? ¿Qué riesgo tiene escanear cuotas en un filesystem montado en rw?

### Ejercicio 2: Asignar y verificar cuotas

Continuando con el filesystem del ejercicio anterior:

1. Crea usuarios de prueba: `useradd testuser1 && useradd testuser2`
2. Asigna cuota a testuser1 con edquota:
   ```bash
   edquota testuser1
   # Configurar: soft=50000 hard=60000 (bloques) — ~50/60 MiB
   ```
3. Copia la cuota a testuser2: `edquota -p testuser1 testuser2`
4. Verifica con repquota: `repquota -s /mnt/quotatest`
5. Crea archivos como testuser1:
   ```bash
   su - testuser1 -c "dd if=/dev/zero of=/mnt/quotatest/file1 bs=1M count=30"
   ```
6. Verifica uso: `quota -s testuser1`
7. Intenta exceder el hard limit:
   ```bash
   su - testuser1 -c "dd if=/dev/zero of=/mnt/quotatest/file2 bs=1M count=40"
   ```
8. ¿Qué error ves? Verifica: `repquota -s /mnt/quotatest`

> **Pregunta de reflexión**: cuando `dd` falla por exceder la cuota, ¿el archivo parcial se crea o no? ¿Cuántos bytes se escribieron antes del error?

### Ejercicio 3: setquota en script

1. Crea 3 usuarios más: `useradd user{a,b,c}`
2. Asigna cuotas con un loop:
   ```bash
   for u in usera userb userc; do
       setquota -u "$u" 102400 122880 500 600 /mnt/quotatest
   done
   ```
3. Verifica: `repquota -s /mnt/quotatest`
4. Elimina cuotas de userc: `setquota -u userc 0 0 0 0 /mnt/quotatest`
5. Verifica: `quota userc` — ¿muestra sin límites?
6. Limpieza:
   ```bash
   quotaoff /mnt/quotatest
   umount /mnt/quotatest
   userdel -r testuser1; userdel -r testuser2
   userdel -r usera; userdel -r userb; userdel -r userc
   ```

> **Pregunta de reflexión**: ¿por qué `setquota` con todos los valores en 0 elimina la cuota en vez de establecer un límite de 0 bytes? ¿Qué pasaría si un usuario tiene cuota de 0 bytes hard limit?
