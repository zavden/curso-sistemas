# T03 — /dev

## Objetivo

Explorar `/dev`: clasificar dispositivos de bloque y carácter, usar los
dispositivos especiales esenciales, entender terminales y pseudo-terminales,
y conocer cómo udev gestiona `/dev` dinámicamente.

---

## Qué es /dev

`/dev` contiene **archivos de dispositivo** (device nodes) que representan
hardware y dispositivos virtuales. En Unix, todo es un archivo — los
dispositivos no son una excepción. Leer o escribir en estos archivos equivale
a comunicarse con el hardware o con un subsistema del kernel.

```bash
ls /dev/ | head -15
# console  core  cpu  fd  full  null  pts  random
# shm  stderr  stdin  stdout  tty  urandom  zero
```

En sistemas modernos, `/dev` es un filesystem virtual de tipo **devtmpfs**
gestionado por udev:

```bash
mount | grep /dev
# devtmpfs on /dev type devtmpfs (rw,nosuid,size=...,nr_inodes=...)
```

## Dispositivos de bloque vs de carácter

### Dispositivos de bloque (block devices)

Almacenan datos en bloques de tamaño fijo. El kernel permite acceso aleatorio
y usa buffers/cache:

```bash
ls -la /dev/sda
# brw-rw---- 1 root disk 8, 0 ... /dev/sda
# ^
# b = block device
```

Ejemplos: discos duros, SSDs, particiones, loop devices.

### Dispositivos de carácter (character devices)

Transmiten datos como flujo de bytes, sin estructura de bloques. El acceso es
generalmente secuencial:

```bash
ls -la /dev/null
# crw-rw-rw- 1 root root 1, 3 ... /dev/null
# ^
# c = character device
```

Ejemplos: terminales, puertos serie, `/dev/null`, `/dev/random`.

### Major y minor numbers

Cada device node tiene dos números que identifican el driver (major) y el
dispositivo específico (minor):

```bash
ls -la /dev/sda /dev/sda1 /dev/sda2
# brw-rw---- 1 root disk 8, 0 ... /dev/sda
# brw-rw---- 1 root disk 8, 1 ... /dev/sda1
# brw-rw---- 1 root disk 8, 2 ... /dev/sda2
#                         ^  ^
#                     major  minor
```

- **Major 8** = driver `sd` (SCSI/SATA/SAS/USB)
- **Minor 0** = disco completo, 1 = partición 1, 2 = partición 2

```bash
# Registro oficial de major numbers
cat /proc/devices
# Character devices:
#   1 mem         ← /dev/null, /dev/zero, /dev/random
#   5 /dev/tty
#  10 misc
# Block devices:
#   8 sd          ← discos SCSI/SATA
# 252 virtblk     ← discos virtio
# 253 device-mapper ← LVM
```

## Dispositivos especiales esenciales

### /dev/null — Agujero negro

Lee siempre vacío, descarta todo lo que se escribe:

```bash
# Descartar salida de un comando
command_ruidoso > /dev/null 2>&1

# Truncar un archivo sin borrarlo (mantiene inodo y permisos)
> /var/log/debug.log

# Leer de /dev/null da EOF inmediato
wc -c < /dev/null
# 0
```

### /dev/zero — Fuente de ceros

Produce bytes `\0` infinitamente:

```bash
# Crear un archivo de 1 MB lleno de ceros
dd if=/dev/zero of=/tmp/zeros.bin bs=1M count=1

# Verificar
xxd /tmp/zeros.bin | head -3
# 00000000: 0000 0000 0000 0000 0000 0000 0000 0000  ................
```

### /dev/urandom y /dev/random — Generadores de aleatoriedad

```bash
# /dev/urandom — nunca bloquea, suficiente para la mayoría de usos
dd if=/dev/urandom bs=16 count=1 2>/dev/null | xxd

# Generar un password aleatorio
head -c 32 /dev/urandom | base64
```

En kernels modernos (≥5.6), `/dev/random` se comporta igual que `/dev/urandom`
después de la inicialización. En la práctica, usar siempre `/dev/urandom`.

### /dev/full — Disco lleno simulado

```bash
echo "test" > /dev/full
# bash: echo: write error: No space left on device
```

Útil para probar que programas manejan correctamente el error ENOSPC.

## Terminales y pseudo-terminales

### /dev/tty

La terminal controladora del proceso actual:

```bash
# Forzar output a la terminal, incluso cuando stdout está redirigido
echo "mensaje al usuario" > /dev/tty
```

### /dev/pts/ — Pseudo-terminales

Las terminales gráficas, SSH y tmux/screen usan pseudo-terminales:

```bash
ls /dev/pts/
# 0  1  2  ptmx

# Ver qué terminal estás usando
tty
# /dev/pts/0
```

Cada sesión SSH, pestaña de terminal o panel de tmux obtiene un `/dev/pts/N`.

### /dev/ttyS* — Puertos serie

```bash
ls /dev/ttyS*
# /dev/ttyS0  /dev/ttyS1  /dev/ttyS2  /dev/ttyS3
```

En máquinas virtuales, la consola serie suele ser `ttyS0` — útil para acceso
de emergencia cuando SSH no funciona.

## Dispositivos de almacenamiento

### Nomenclatura

| Patrón | Tipo | Ejemplos |
|---|---|---|
| `sd[a-z]` | SCSI/SATA/SAS/USB | sda, sdb, sdc |
| `sd[a-z][1-9]` | Particiones SCSI | sda1, sda2 |
| `nvme[0-9]n[1-9]` | NVMe | nvme0n1 |
| `nvme[0-9]n[1-9]p[1-9]` | Particiones NVMe | nvme0n1p1 |
| `vd[a-z]` | virtio (VMs) | vda, vdb |
| `loop[0-9]` | Loop devices | loop0, loop1 |
| `dm-[0-9]` | Device mapper (LVM) | dm-0, dm-1 |
| `sr[0-9]` | CD/DVD | sr0 |

### Symlinks estables en /dev/disk/

Los nombres como `sda` pueden cambiar entre reinicios. Por eso existen
symlinks estables:

```bash
ls /dev/disk/
# by-id  by-label  by-partuuid  by-path  by-uuid

# Por UUID (el más usado en /etc/fstab)
ls -la /dev/disk/by-uuid/
# abc12345-... -> ../../sda1

# Por label
ls -la /dev/disk/by-label/
# ROOT -> ../../sda2
```

## /dev/shm — Memoria compartida

Filesystem tmpfs para POSIX shared memory:

```bash
df -h /dev/shm
# tmpfs  7.8G  0  7.8G  0%  /dev/shm
```

Los programas crean archivos aquí para compartir memoria entre procesos
(PostgreSQL, Chrome, etc.). Tamaño por defecto: 50% de la RAM.

## udev — Gestión dinámica de dispositivos

**udev** gestiona `/dev` dinámicamente. Cuando se conecta o desconecta
hardware:

1. Recibe un evento del kernel (uevent)
2. Evalúa sus reglas (`/usr/lib/udev/rules.d/` y `/etc/udev/rules.d/`)
3. Crea/elimina el device node en `/dev`
4. Crea symlinks (como `/dev/disk/by-uuid/`)
5. Ejecuta acciones configuradas

### Formato de reglas

```bash
# Ejemplo: /etc/udev/rules.d/99-usb-serial.rules
SUBSYSTEM=="tty", ATTRS{idVendor}=="2341", ATTRS{idProduct}=="0043", \
    GROUP="dialout", MODE="0660", SYMLINK+="arduino"
```

- `==` es condición (match)
- `=` es asignación
- `+=` es agregar (SYMLINK, RUN)

Las reglas en `/etc/udev/rules.d/` tienen precedencia sobre las del sistema.

### udevadm

```bash
# Información de un dispositivo
udevadm info /dev/sda

# Monitorear eventos en tiempo real
udevadm monitor

# Recargar reglas sin reiniciar
sudo udevadm control --reload-rules && sudo udevadm trigger
```

## /dev en contenedores

Docker proporciona un `/dev` **mínimo** por seguridad:

```bash
docker run --rm debian ls /dev/
# console  core  fd  full  mqueue  null  ptmx  pts  random
# shm  stderr  stdin  stdout  tty  urandom  zero
```

No incluye dispositivos de bloque del host. Para acceso explícito:

```bash
# Un dispositivo específico
docker run --rm --device /dev/sda debian fdisk -l /dev/sda

# Todos los dispositivos (privilegiado — riesgo de seguridad)
docker run --rm --privileged debian ls /dev/
```

---

## Ejercicios

Todos los comandos se ejecutan dentro de los contenedores del curso.

### Ejercicio 1 — Clasificar dispositivos por tipo

Examina el `/dev` mínimo del contenedor e identifica dispositivos de bloque
y carácter.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Contenido de /dev ==="
ls /dev/

echo ""
echo "=== Dispositivos de carácter (c) ==="
ls -la /dev/ | grep "^c"

echo ""
echo "=== Dispositivos de bloque (b) ==="
ls -la /dev/ | grep "^b" || echo "(ninguno visible en el contenedor)"

echo ""
echo "=== Conteo ==="
echo "Carácter: $(ls -la /dev/ | grep -c "^c")"
echo "Bloque:   $(ls -la /dev/ | grep -c "^b")"
echo "Total:    $(ls /dev/ | wc -l)"
'
```

<details>
<summary>Predicción</summary>

El contenedor tiene un `/dev` mínimo con ~15-20 entradas. La mayoría son
dispositivos de **carácter**:
- `null` (1,3) — agujero negro
- `zero` (1,5) — fuente de ceros
- `full` (1,7) — disco lleno simulado
- `random` (1,8) y `urandom` (1,9) — aleatoriedad
- `tty` (5,0) — terminal controladora
- `console` (5,1) — consola del sistema
- `ptmx` (5,2) — multiplexor de pseudo-terminales

Dispositivos de **bloque**: probablemente 0. Docker no expone discos del host
por seguridad. Los contenedores acceden al filesystem via overlay2, no
directamente a dispositivos de bloque.

Además hay directorios especiales como `pts/` (pseudo-terminales), `shm/`
(memoria compartida), `fd/` (symlinks a `/proc/self/fd/`), y symlinks como
`stdin` → `/proc/self/fd/0`.

</details>

---

### Ejercicio 2 — Inspeccionar major/minor numbers

Examina cómo los números major y minor identifican drivers y dispositivos.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Major/minor de dispositivos especiales ==="
ls -la /dev/null /dev/zero /dev/full /dev/random /dev/urandom /dev/tty

echo ""
echo "=== Registro de drivers (major numbers) ==="
echo "--- Dispositivos de carácter ---"
cat /proc/devices | sed -n "/Character/,/Block/p" | head -10
echo ""
echo "--- Dispositivos de bloque ---"
cat /proc/devices | sed -n "/Block/,\$p" | head -10
'
```

<details>
<summary>Predicción</summary>

Los dispositivos especiales comparten el **major 1** (driver `mem`):
- `null`: 1,3
- `zero`: 1,5
- `full`: 1,7
- `random`: 1,8
- `urandom`: 1,9

Todos son gestionados por el mismo driver del kernel (`mem`). El minor number
distingue qué dispositivo específico es.

`tty` tiene major 5 (driver `tty`).

`/proc/devices` muestra el mapeo completo de major numbers a drivers. Los
character devices tienen muchos más registros que los block devices. En un
contenedor, los block devices visibles pueden ser limitados.

El minor number es interpretado por cada driver de forma diferente. Para el
driver `sd` (major 8): minor 0 = disco completo, 1-15 = particiones del primer
disco, 16 = segundo disco completo, etc.

</details>

---

### Ejercicio 3 — Usar /dev/null, /dev/zero y /dev/full

Practica con los tres dispositivos especiales más usados.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== /dev/null: descartar salida ==="
echo "esto se pierde" > /dev/null
echo "Exit code: $? (0 = éxito, la escritura siempre funciona)"
echo "Bytes leídos: $(wc -c < /dev/null) (siempre 0)"

echo ""
echo "=== /dev/zero: crear archivo de ceros ==="
dd if=/dev/zero of=/tmp/zeros.bin bs=1K count=4 2>&1
xxd /tmp/zeros.bin | head -2
echo "Tamaño: $(wc -c < /tmp/zeros.bin) bytes"
rm /tmp/zeros.bin

echo ""
echo "=== /dev/full: simular disco lleno ==="
echo "test" > /dev/full 2>&1
echo "Exit code: $? (1 = error ENOSPC)"
'
```

<details>
<summary>Predicción</summary>

**`/dev/null`**: la escritura siempre tiene éxito (exit code 0) pero los datos
se descartan. La lectura devuelve EOF inmediatamente (0 bytes). Es el
"agujero negro" de Unix.

**`/dev/zero`**: `dd` crea un archivo de exactamente 4096 bytes (4 bloques de
1 KB). `xxd` muestra todo ceros (`0000 0000 ...`). `/dev/zero` produce bytes
`\0` infinitamente — `dd` controla cuántos leer con `bs` y `count`.

**`/dev/full`**: la escritura falla con "No space left on device" (exit code 1).
Simula el error ENOSPC que ocurre cuando un disco se llena. Útil para testing:
```bash
# Probar que un programa maneja ENOSPC correctamente
./mi_programa > /dev/full
```

Los tres tienen major 1 (driver `mem`). Son dispositivos de carácter que
existen en todos los sistemas Unix.

</details>

---

### Ejercicio 4 — Generar datos aleatorios con /dev/urandom

Usa `/dev/urandom` para generar passwords, tokens y datos aleatorios.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== 16 bytes aleatorios en hexadecimal ==="
dd if=/dev/urandom bs=16 count=1 2>/dev/null | xxd

echo ""
echo "=== Password aleatorio (base64) ==="
head -c 24 /dev/urandom | base64

echo ""
echo "=== Token hexadecimal ==="
head -c 16 /dev/urandom | xxd -p

echo ""
echo "=== Número aleatorio (0-99) ==="
echo $(($(od -An -tu4 -N4 /dev/urandom | tr -d " ") % 100))

echo ""
echo "=== /dev/random vs /dev/urandom ==="
echo "Kernel: $(uname -r)"
echo "En kernels >= 5.6, ambos son equivalentes después de la inicialización"
'
```

<details>
<summary>Predicción</summary>

Cada ejecución produce valores diferentes (aleatorios).

- `xxd` muestra 16 bytes en formato hexadecimal + ASCII
- `base64` codifica 24 bytes aleatorios en una cadena de ~32 caracteres
  (ratio 4/3) apta como password
- `xxd -p` produce hexadecimal puro (32 caracteres para 16 bytes), útil como
  token de API
- `od -An -tu4 -N4` lee 4 bytes y los interpreta como un entero sin signo
  de 32 bits, luego `% 100` lo reduce al rango 0-99

`/dev/urandom` nunca bloquea. En kernels ≥5.6 (la mayoría de sistemas actuales),
`/dev/random` se comporta igual que `/dev/urandom` después de que el generador
de números aleatorios se inicializa al arranque. La distinción histórica
(random bloqueaba esperando "entropía suficiente") ya no aplica.

</details>

---

### Ejercicio 5 — Explorar pseudo-terminales

Examina cómo funcionan las pseudo-terminales en el contenedor.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Terminal actual ==="
tty

echo ""
echo "=== Pseudo-terminales abiertas ==="
ls /dev/pts/

echo ""
echo "=== Multiplexor de pseudo-terminales ==="
ls -la /dev/ptmx

echo ""
echo "=== stdin, stdout, stderr son symlinks ==="
ls -la /dev/stdin /dev/stdout /dev/stderr

echo ""
echo "=== /dev/fd/ (file descriptors del proceso) ==="
ls -la /dev/fd/
'
```

<details>
<summary>Predicción</summary>

Con `-T` (sin TTY), `tty` reporta `not a tty` porque no se asignó una
pseudo-terminal. Sin el flag `-T`, mostraría algo como `/dev/pts/0`.

`/dev/pts/` muestra las pseudo-terminales activas en el contenedor. Con `-T`,
puede estar vacío o contener solo `ptmx`.

`/dev/ptmx` es el "master" del subsistema de pseudo-terminales. Cuando un
programa necesita una nueva terminal (como `sshd`, `screen`, o
`docker compose exec`), abre `/dev/ptmx` y el kernel asigna un nuevo
`/dev/pts/N`.

`stdin`, `stdout`, `stderr` son symlinks a `/proc/self/fd/0`, `/proc/self/fd/1`,
`/proc/self/fd/2`. Es la conexión entre `/dev` y `/proc`.

`/dev/fd/` es un symlink a `/proc/self/fd/` — permite acceder a los file
descriptors del proceso actual como si fueran archivos en `/dev/`.

</details>

---

### Ejercicio 6 — Redirecciones prácticas con /dev/null

Domina los patrones de redirección más comunes con `/dev/null`.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Patrón 1: descartar stderr, mantener stdout ==="
ls /dev/null /dev/no_existe 2>/dev/null
echo ""

echo "=== Patrón 2: descartar stdout, mantener stderr ==="
ls /dev/null /dev/no_existe >/dev/null
echo ""

echo "=== Patrón 3: descartar todo ==="
ls /dev/null /dev/no_existe >/dev/null 2>&1
echo "Exit code: $?"
echo ""

echo "=== Patrón 4: redirigir stderr a stdout ==="
ls /dev/null /dev/no_existe 2>&1 | grep -c "."
echo "  líneas totales (stdout + stderr combinados)"
echo ""

echo "=== Patrón 5: truncar un archivo ==="
echo "contenido temporal" > /tmp/test_truncate.txt
echo "Antes: $(wc -c < /tmp/test_truncate.txt) bytes"
> /tmp/test_truncate.txt
echo "Después: $(wc -c < /tmp/test_truncate.txt) bytes"
rm /tmp/test_truncate.txt
'
```

<details>
<summary>Predicción</summary>

**Patrón 1** (`2>/dev/null`): muestra solo stdout (`/dev/null`), oculta el
error de `/dev/no_existe`.

**Patrón 2** (`>/dev/null`): oculta stdout, muestra solo stderr
(`ls: cannot access '/dev/no_existe': No such file or directory`).

**Patrón 3** (`>/dev/null 2>&1`): no muestra nada. El `2>&1` redirige stderr
al mismo lugar que stdout (que ya fue redirigido a `/dev/null`). Exit code = 2
(error parcial de `ls`).

**Patrón 4** (`2>&1 |`): combina stdout y stderr en un solo flujo que pasa por
el pipe. `grep -c "."` cuenta el total de líneas (1 de stdout + 1 de stderr =
2).

**Patrón 5** (`> archivo`): la redirección vacía trunca el archivo a 0 bytes
sin borrarlo. El inodo se mantiene (importante si otros procesos tienen el
archivo abierto, como logs).

El orden importa: `>/dev/null 2>&1` funciona, `2>&1 >/dev/null` no (stderr
se redirige al stdout original antes de que stdout sea redirigido).

</details>

---

### Ejercicio 7 — /dev/shm y memoria compartida

Examina el filesystem de memoria compartida.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== /dev/shm ==="
df -h /dev/shm
mount | grep shm

echo ""
echo "=== Contenido ==="
ls -la /dev/shm/ 2>/dev/null || echo "(vacío)"

echo ""
echo "=== Crear archivo en memoria compartida ==="
echo "datos compartidos" > /dev/shm/test_shm.txt
ls -la /dev/shm/test_shm.txt
echo "Contenido: $(cat /dev/shm/test_shm.txt)"

echo ""
echo "=== Esto está en RAM (tmpfs), no en disco ==="
df -h /dev/shm

rm /dev/shm/test_shm.txt
'
```

<details>
<summary>Predicción</summary>

`/dev/shm` es un tmpfs (filesystem en RAM). `df -h` muestra su tamaño
(normalmente 64 MB en contenedores Docker por defecto, o el 50% de la RAM en
el host).

El archivo se crea instantáneamente en RAM. Es más rápido que escribir en
disco pero se pierde al reiniciar.

`/dev/shm` es la implementación de POSIX shared memory. Los programas
usan `shm_open()` en C para crear segmentos de memoria compartida entre
procesos, que aparecen como archivos aquí. PostgreSQL, Chrome, y otros
programas que necesitan compartir grandes cantidades de datos entre procesos
lo usan.

En Docker, el tamaño de `/dev/shm` se puede configurar con `--shm-size` o
`shm_size:` en `compose.yml`. Si una aplicación necesita más shared memory
de la disponible, fallará con errores ENOMEM.

</details>

---

### Ejercicio 8 — Nomenclatura de dispositivos de almacenamiento

Examina la nomenclatura de discos y el concepto de symlinks estables.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Dispositivos de bloque visibles ==="
ls -la /dev/ | grep "^b" || echo "No hay dispositivos de bloque en el contenedor"

echo ""
echo "=== Nomenclatura (referencia) ==="
echo "sd[a-z]        SCSI/SATA/SAS/USB"
echo "sd[a-z][1-9]   Particiones SCSI"
echo "nvme[0-9]n[1]  NVMe"
echo "nvme0n1p[1-9]  Particiones NVMe"
echo "vd[a-z]        virtio (VMs)"
echo "loop[0-9]      Loop devices"
echo "dm-[0-9]       Device mapper (LVM)"

echo ""
echo "=== lsblk (si disponible) ==="
lsblk 2>/dev/null || echo "lsblk no disponible en el contenedor"

echo ""
echo "=== /dev/disk/ symlinks (si accesible) ==="
ls /dev/disk/ 2>/dev/null || echo "/dev/disk/ no existe en el contenedor"
'
```

<details>
<summary>Predicción</summary>

No hay dispositivos de bloque visibles en el contenedor (Docker no los expone
por seguridad). `lsblk` probablemente no está instalado o no tiene
dispositivos que listar.

`/dev/disk/` tampoco existe porque es creado por udev basándose en los
dispositivos de bloque reales, que no están disponibles en el contenedor.

En el **host**, `/dev/disk/` contiene subdirectorios con symlinks estables:
- `by-uuid/` — por UUID del filesystem (usado en `/etc/fstab`)
- `by-label/` — por label del filesystem
- `by-id/` — por identificador del hardware
- `by-path/` — por ruta del bus

Los symlinks estables resuelven el problema de que `sda` pueda convertirse en
`sdb` si se añade un disco. Los UUID son únicos e inmutables, por eso
`/etc/fstab` usa `UUID=...` en vez de `/dev/sda1`.

</details>

---

### Ejercicio 9 — Comparar /dev entre contenedor y host

Observa las diferencias entre el `/dev` mínimo del contenedor y el completo
del host.

```bash
echo "=== /dev en contenedor Debian ==="
docker compose exec -T debian-dev bash -c '
echo "Entradas: $(ls /dev/ | wc -l)"
echo "Carácter: $(ls -la /dev/ | grep -c "^c")"
echo "Bloque:   $(ls -la /dev/ | grep -c "^b")"
echo ""
echo "Dispositivos:"
ls /dev/ | sort
'

echo ""
echo "=== /dev en contenedor AlmaLinux ==="
docker compose exec -T alma-dev bash -c '
echo "Entradas: $(ls /dev/ | wc -l)"
echo "Carácter: $(ls -la /dev/ | grep -c "^c")"
echo "Bloque:   $(ls -la /dev/ | grep -c "^b")"
'

echo ""
echo "=== /dev en el host (si disponible) ==="
echo "Entradas: $(ls /dev/ 2>/dev/null | wc -l)"
echo "Carácter: $(ls -la /dev/ 2>/dev/null | grep -c "^c")"
echo "Bloque:   $(ls -la /dev/ 2>/dev/null | grep -c "^b")"
```

<details>
<summary>Predicción</summary>

**Contenedores**: ~15-20 entradas, casi todos dispositivos de carácter,
probablemente 0 de bloque. Ambos contenedores muestran prácticamente lo mismo
porque Docker proporciona el mismo `/dev` mínimo a todos.

**Host**: cientos de entradas (>100). Incluye:
- Decenas de dispositivos de carácter (tty0-tty63, ttyS0-S3, etc.)
- Varios de bloque (sda, sda1-N, loop0-N, dm-0-N)
- Directorios creados por udev (disk/, cpu/, dri/, snd/)

Docker limita `/dev` por seguridad: un contenedor comprometido no puede
acceder a discos del host, puertos serie, ni otros dispositivos de hardware.
Solo expone los dispositivos esenciales que cualquier programa Unix necesita
(null, zero, random, pts).

El flag `--privileged` deshabilita esta restricción y da acceso completo a
`/dev` del host. Solo usar en entornos de desarrollo controlados.

</details>

---

### Ejercicio 10 — Crear un device node manualmente (concepto)

Entiende cómo se crean device nodes con `mknod` (requiere root).

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Crear un device node manual ==="
echo "Sintaxis: mknod <path> <tipo> <major> <minor>"
echo "  tipo: c (carácter), b (bloque)"
echo ""

echo "=== Intentar crear un duplicado de /dev/null ==="
mknod /tmp/my_null c 1 3 2>/dev/null
if [ -e /tmp/my_null ]; then
    echo "Creado: /tmp/my_null"
    ls -la /tmp/my_null
    echo ""
    echo "Funciona como /dev/null:"
    echo "test" > /tmp/my_null
    echo "Exit code: $?"
    wc -c < /tmp/my_null
    rm /tmp/my_null
else
    echo "No se pudo crear (requiere CAP_MKNOD, deshabilitado en Docker por defecto)"
fi

echo ""
echo "=== Verificar: /dev/null original ==="
ls -la /dev/null
echo "Major 1, minor 3 = driver mem, dispositivo null"

echo ""
echo "=== En resumen ==="
echo "Los device nodes son archivos especiales con major:minor"
echo "El kernel usa esos números para enrutar las operaciones de I/O"
echo "al driver correcto. udev los crea automáticamente."
'
```

<details>
<summary>Predicción</summary>

`mknod` probablemente falla en el contenedor. Docker deshabilita la capability
`CAP_MKNOD` por defecto (desde Docker 1.13), impidiendo la creación de device
nodes. Esto es una protección de seguridad: un contenedor no debería poder
crear dispositivos arbitrarios.

Si tuviera éxito (en un contenedor privilegiado), el device node `/tmp/my_null`
se comportaría exactamente como `/dev/null`: misma funcionalidad porque los
números major 1, minor 3 apuntan al mismo driver del kernel (`mem`) y al
mismo dispositivo (`null`).

El concepto clave: un device node es solo un archivo especial con dos números.
El kernel usa esos números para determinar qué driver maneja las operaciones
de I/O. El nombre del archivo es irrelevante — lo que importa son los major y
minor numbers. Por eso `/dev/null` y un hipotético `/tmp/my_null` con los
mismos números serían idénticos.

En sistemas reales, udev crea los device nodes automáticamente. `mknod`
manual es raro fuera de entornos de recuperación o sistemas embebidos.

</details>

---

## Resumen de conceptos

| Concepto | Detalle clave |
|---|---|
| `/dev` | Device nodes que representan hardware y dispositivos virtuales |
| Bloque (b) | Acceso aleatorio por bloques: discos, particiones |
| Carácter (c) | Flujo secuencial de bytes: terminales, null, random |
| Major/minor | Major = driver, minor = dispositivo específico |
| `/dev/null` | Descarta escrituras, lectura da EOF |
| `/dev/zero` | Produce `\0` infinitamente |
| `/dev/urandom` | Datos pseudo-aleatorios, nunca bloquea |
| `/dev/full` | Simula ENOSPC para testing |
| `/dev/pts/N` | Pseudo-terminales (SSH, tmux, exec) |
| `/dev/shm` | tmpfs para POSIX shared memory |
| `/dev/disk/by-uuid/` | Symlinks estables (inmunes a renombrado) |
| udev | Gestión dinámica: crea/elimina device nodes según eventos |
| Contenedores | `/dev` mínimo por seguridad; `--device` o `--privileged` para más |
