# T03 — chroot y pivot_root

## Errata y notas sobre el material base

1. **README.max.md: "chiamata al sistema"** — Italianismo. Es "llamada
   al sistema" (syscall).

2. **README.max.md: `echo $$` dentro de chroot muestra `1`** — Falso.
   chroot no crea PID namespace. El shell conserva su PID real del host
   (p.ej. 4523). Solo con `unshare --pid` verías PID 1.

3. **README.max.md: escape en shell con `fchdir 3`** — `fchdir` es una
   función de C (`man 2 fchdir`), no un comando de shell. No existe
   `fchdir` como builtin de bash/sh. El escape de chroot solo funciona
   en C (o con un binario compilado). La versión shell presentada no es
   ejecutable tal como está.

4. **README.max.md ejercicio 6: `readlink /proc/1/root` desde dentro
   del contenedor** — Dice que muestra
   `/var/lib/docker/overlay2/...`. Falso: desde dentro del contenedor
   muestra `/` (el mount namespace oculta la ruta real). Solo desde el
   **host** con el PID real del contenedor se vería la ruta overlay2.

5. **README.md: requisito de pivot_root** — Dice "new_root y put_old
   deben ser mount points". Solo `new_root` necesita ser un mount point.
   `put_old` solo debe ser un directorio dentro de `new_root`.

6. **README.max.md: falta `mount --make-rprivate /`** — Antes de
   `pivot_root`, es necesario hacer los mounts privados para evitar que
   eventos de montaje se propaguen al namespace padre. Sin esto,
   `umount -l /oldroot` podría afectar al host.

7. **README.max.md: "lashell"** — Typo: "la shell".

---

## chroot — Cambiar el directorio raíz

`chroot` es una syscall y comando que cambia el directorio raíz (`/`)
de un proceso. Después, el proceso ve un directorio arbitrario como `/`
y no puede resolver rutas por encima de ese punto:

```
Host filesystem              Proceso con chroot(/var/jail)
──────────────              ─────────────────────────────
/                           /var/jail → se convierte en "/"
├── bin                     ├── bin
├── etc                     ├── etc
├── home                    ├── proc  (vacío si no se monta)
├── var/                    └── tmp
│   └── jail/
│       ├── bin
│       ├── etc
│       ├── proc
│       └── tmp
└── usr

El proceso hace chroot("/var/jail") → para él, "/" = /var/jail
cd .. desde / → sigue en /  (no puede subir más allá)
```

```bash
# Sintaxis
sudo chroot /path/to/new-root [command [args...]]

# Si no se especifica command, ejecuta /bin/sh
```

Puntos clave:
- Solo afecta la **resolución de rutas** — el proceso sigue en el mismo
  sistema con el mismo kernel, los mismos PIDs, la misma red
- Todos los hijos heredan el chroot
- Requiere root (o `CAP_SYS_CHROOT`)
- **No es un namespace** — es un atributo por proceso, no una barrera
  del kernel

---

## Preparar un rootfs mínimo

Para hacer chroot se necesita un directorio con al menos un shell y sus
dependencias:

### Método 1: Extraer imagen Docker (rápido, ~5 MB)

```bash
mkdir -p /tmp/chroot-alpine
docker export $(docker create alpine) | sudo tar -xf - -C /tmp/chroot-alpine
```

### Método 2: debootstrap (Debian/Ubuntu completo, ~200 MB)

```bash
sudo apt install debootstrap
sudo debootstrap bookworm /tmp/chroot-debian http://deb.debian.org/debian
```

### Método 3: busybox estático (sin Docker, ~3 MB)

```bash
mkdir -p /tmp/chroot-bb/{bin,proc,sys,dev,tmp,etc}

# Descargar busybox estático (enlazado estáticamente = sin dependencias de libs)
curl -o /tmp/chroot-bb/bin/busybox \
    https://busybox.net/downloads/binaries/1.36.0-x86_64-linux-musl/busybox
chmod +x /tmp/chroot-bb/bin/busybox

# Crear symlinks a los comandos
for cmd in sh ls cat echo ps mount; do
    ln -sf busybox /tmp/chroot-bb/bin/$cmd
done
```

### Copiar un binario dinámico (cuando no hay estático)

Si el binario no es estático, necesita sus librerías:

```bash
# Ver dependencias
ldd /bin/bash
#   linux-vdso.so.1
#   libtinfo.so.6 => /lib/x86_64-linux-gnu/libtinfo.so.6
#   libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6
#   /lib64/ld-linux-x86-64.so.2

# Copiar bash + sus libs al rootfs
cp /bin/bash /tmp/chroot-test/bin/
ldd /bin/bash | grep -o '/[^ ]*' | while read lib; do
    mkdir -p "/tmp/chroot-test$(dirname $lib)"
    cp "$lib" "/tmp/chroot-test$lib"
done
```

---

## Usar chroot

```bash
# Entrar al chroot
sudo chroot /tmp/chroot-alpine /bin/sh

# Dentro del chroot:
pwd           # /
ls /          # bin dev etc home lib media mnt opt proc root ...
cat /etc/os-release   # Alpine Linux

# Intentar acceder al host:
ls /home/usuario      # No such file or directory
cd ..                 # sigue en /

# /proc está vacío (no montado)
ls /proc              # (vacío)
ps aux                # error: /proc not mounted

exit
```

### Montar /proc y /dev dentro del chroot

Para que herramientas como `ps` funcionen:

```bash
# FUERA del chroot — montar pseudofilesystems del kernel
sudo mount --bind /proc /tmp/chroot-alpine/proc
sudo mount --bind /sys  /tmp/chroot-alpine/sys
sudo mount --bind /dev  /tmp/chroot-alpine/dev

sudo chroot /tmp/chroot-alpine /bin/sh

# Ahora ps funciona... pero muestra TODOS los procesos del HOST
ps aux | wc -l        # 150+  ← son los del host
ps aux | head -3
# PID   USER     COMMAND
# 1     root     /sbin/init    ← PID 1 del host (systemd)
# 2     root     [kthreadd]

echo $$               # 4523  ← tu PID real, NO es 1
                       # (chroot no crea PID namespace)

# Limpieza obligatoria al salir
exit
sudo umount /tmp/chroot-alpine/{proc,sys,dev}
```

**Punto clave**: con `/proc` del host montado, el chroot ve **todos
los procesos del host**. chroot no aísla PIDs — para eso se necesitan
PID namespaces (T01).

---

## Limitaciones de chroot — No es aislamiento

chroot solo cambia la resolución de rutas del filesystem. No crea
ninguna barrera de aislamiento:

| Recurso | ¿Aislado? | Implicación |
|---|---|---|
| Filesystem (rutas) | Parcialmente | Viejo rootfs accesible vía file descriptors |
| PIDs | **NO** | Ve y puede señalizar todos los procesos del host |
| Red | **NO** | Comparte interfaces, puertos, tablas de ruteo |
| Usuarios | **NO** | UID 0 dentro = UID 0 fuera (root real) |
| IPC | **NO** | Accede a shared memory del host |
| Hostname | **NO** | Comparte hostname del host |
| Mounts | **NO** | Puede montar filesystems con `CAP_SYS_ADMIN` |

### Escape de chroot (requiere root)

Un proceso root dentro del chroot puede escapar. El método clásico
funciona **solo en C** (no en shell puro):

```c
// Escape de chroot — requiere root
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

int main() {
    // 1. Crear un directorio temporal
    mkdir("escape", 0755);

    // 2. Guardar un fd al directorio actual (dentro del chroot)
    int fd = open(".", O_RDONLY);

    // 3. chroot al directorio temporal → cambia "/" a "escape/"
    chroot("escape");
    //    Ahora "/" es "escape/", pero nuestro cwd sigue siendo
    //    el directorio original (que está FUERA del nuevo chroot)

    // 4. fchdir al fd guardado → volvemos al directorio original
    fchdir(fd);
    //    Ahora estamos fuera del nuevo chroot

    // 5. Subir hasta la raíz real del filesystem
    for (int i = 0; i < 100; i++) chdir("..");

    // 6. chroot a la raíz real → escape completo
    chroot(".");

    // Ahora tenemos acceso al filesystem completo del host
    execl("/bin/sh", "sh", NULL);
}
```

¿Por qué funciona? Porque `chroot` solo cambia la referencia de `/`
para el proceso. Si el proceso ya tiene un file descriptor abierto a
un directorio fuera del chroot, puede usar `fchdir()` para moverse
allí. chroot no revoca los fd existentes.

> `fchdir` es una syscall de C, no un builtin de shell. La "versión
> shell" del escape que aparece en algunos materiales (`exec 3<.;
> chroot .; fchdir 3`) **no funciona** porque `fchdir` no existe como
> comando shell.

### Para qué sí sirve chroot

```
Usos legítimos:
  ✓ Reparación del sistema — arrancar desde USB, chroot al disco
    para arreglar GRUB, fstab, paquetes rotos
  ✓ Build environments — compilar en un rootfs limpio
  ✓ Probar otra distribución sin VM
  ✓ arch-chroot (Arch Linux) — envuelve chroot con montaje automático
    de /proc, /sys, /dev y resolv.conf
```

#### Ejemplo clásico: reparar GRUB

```bash
# Desde un live USB
sudo mount /dev/sda2 /mnt             # partición raíz
sudo mount /dev/sda1 /mnt/boot/efi    # partición EFI (si aplica)
sudo mount --bind /proc /mnt/proc
sudo mount --bind /sys  /mnt/sys
sudo mount --bind /dev  /mnt/dev

sudo chroot /mnt
# Ahora estás "dentro" del sistema del disco
update-grub
grub-install /dev/sda
exit

sudo umount /mnt/{proc,sys,dev,boot/efi}
sudo umount /mnt
```

---

## pivot_root — La alternativa segura

`pivot_root` es una syscall (`man 2 pivot_root`) que **intercambia**
la raíz del filesystem a nivel de mount namespace. El viejo rootfs se
mueve a un subdirectorio y puede desmontarse completamente:

```
chroot:
  Solo cambia la referencia de "/" para UN proceso
  El viejo rootfs sigue accesible (vía fds abiertos → escape)
  Es un atributo del proceso, no del namespace

pivot_root:
  Intercambia la raíz del MOUNT NAMESPACE completo
  El viejo rootfs se mueve a put_old (subdirectorio del nuevo root)
  Se puede desmontar → desaparece completamente
  Afecta a todos los procesos del namespace
```

### Requisitos

```bash
# pivot_root new_root put_old

# Requisitos:
# 1. new_root DEBE ser un mount point (mount --bind new_root new_root)
# 2. put_old debe ser un directorio dentro de new_root
# 3. Debe ejecutarse en un mount namespace aislado (unshare --mount)
#    — si no, afectaría a todo el host
# 4. Los mounts deben ser privados (no propagarse al padre)
```

### Ejemplo completo

```bash
# 1. Preparar el rootfs
mkdir -p /tmp/newroot
docker export $(docker create alpine) | sudo tar -xf - -C /tmp/newroot
mkdir -p /tmp/newroot/oldroot   # donde irá el viejo rootfs

# 2. Crear un mount namespace aislado (CRÍTICO)
sudo unshare --mount bash

# 3. Hacer los mounts privados (evitar propagación al host)
mount --make-rprivate /

# 4. Convertir newroot en mount point
mount --bind /tmp/newroot /tmp/newroot

# 5. Ejecutar pivot_root
cd /tmp/newroot
pivot_root . oldroot

# 6. Ahora / es Alpine, /oldroot es el host
ls /
# bin dev etc home lib media mnt oldroot opt proc ...

# 7. Montar /proc
mount -t proc proc /proc

# 8. Desmontar el viejo rootfs — punto de no retorno
umount -l /oldroot
rmdir /oldroot

# 9. Verificar: el host ya NO es accesible
ls /oldroot
# No such file or directory  ← desapareció

exit
# El namespace se destruye cuando el último proceso sale
```

### ¿Por qué `mount --make-rprivate /`?

Por defecto, los mount points pueden ser "shared" (las operaciones de
mount/umount se propagan entre namespaces). Sin `--make-rprivate`, el
`umount -l /oldroot` podría propagarse al namespace padre y desmontar
cosas en el host. `rprivate` (recursivamente privado) corta la
propagación.

### Por qué Docker usa pivot_root (no chroot)

```
chroot:
  ✗ Escape posible con root (fd + fchdir + chdir)
  ✗ Viejo rootfs sigue accesible
  ✗ Solo cambia resolución de rutas

pivot_root:
  ✓ Viejo rootfs se mueve a put_old
  ✓ Después de umount, NO hay forma de acceder al host
  ✓ Cambio real a nivel de mount namespace
  ✓ Usado por Docker, Podman, containerd, runc — todos los runtimes OCI
```

### Flujo de un runtime de contenedor

```
Lo que hace Docker/runc al crear un contenedor:

1. clone(CLONE_NEWNS | CLONE_NEWPID | ...)
   → nuevos namespaces (mount, PID, UTS, net, ...)

2. mount --make-rprivate /
   → aislar propagación de mounts

3. mount(overlay2_layers) en /rootfs
   → combinar capas de la imagen como un solo filesystem

4. pivot_root(/rootfs, /rootfs/oldroot)
   → la imagen se convierte en /

5. umount -l /oldroot
   → filesystem del host ya NO accesible

6. mount -t proc proc /proc
   mount -t sysfs sysfs /sys
   mount -t devtmpfs devtmpfs /dev
   → filesystems virtuales propios

7. exec(entrypoint)
   → el proceso del usuario se convierte en PID 1
```

---

## Comparación directa

| Aspecto | chroot | pivot_root |
|---|---|---|
| Tipo | Atributo del proceso | Operación sobre mount namespace |
| Mecanismo | Cambia referencia de `/` | Intercambia rootfs del namespace |
| Escape con root | Posible (fd + fchdir) | No (si se desmonta oldroot) |
| Viejo rootfs | Accesible vía fds | Se mueve a put_old → se desmonta |
| Requiere namespace | No | Sí (mount namespace) |
| Complejidad | Baja (un comando) | Mayor (namespace + mounts privados) |
| Uso moderno | Reparación, builds | Contenedores (Docker, Podman, runc) |
| Seguridad | Baja | Alta (combinado con namespaces) |

```
Regla general:
  - ¿Reparar un sistema roto? → chroot
  - ¿Aislar un proceso? → pivot_root + namespaces (o Docker)
  - ¿"Contenedor casero"? → unshare + pivot_root (siguiente tema T04)
```

---

## Verificar desde dentro de un contenedor

Dentro de un contenedor Docker, podemos observar los efectos de
pivot_root sin ejecutarlo:

```bash
# ¿Qué ve PID 1 como raíz?
readlink /proc/1/root
# /  ← desde dentro siempre es /, gracias al mount namespace

# Desde el HOST, el mismo PID mostraría la ruta real:
# readlink /proc/<host-PID>/root
# /var/lib/docker/overlay2/abc123.../merged

# ¿Podemos ver el filesystem del host?
ls /host 2>/dev/null || echo "No existe /host"
cd .. && pwd    # / — no hay forma de subir

# Puntos de montaje dentro del contenedor
mount | head -5
# overlay on / type overlay (...)  ← el rootfs es overlay2
# proc on /proc type proc (...)
# devpts on /dev/pts type devpts (...)
```

---

## Ejercicios

### Ejercicio 1 — Explorar el rootfs del contenedor

¿Cómo llegó este filesystem a ser tu `/`?

```bash
# ¿Qué tipo de filesystem es /?
mount | grep ' / '

# ¿Qué distribución ves?
cat /etc/os-release | head -2

# ¿Es la misma que el host? (pista: el host es Fedora)
# ¿Cómo es posible ver otra distribución?

# ¿Puedes ver algo del host?
ls /host 2>/dev/null
ls -la /proc/1/root
```

<details><summary>Predicción</summary>

- `mount | grep ' / '` mostrará algo como
  `overlay on / type overlay (rw,...)` — el rootfs es un overlay
  filesystem (capas de la imagen Docker combinadas).
- `/etc/os-release` muestra Debian (el contenedor del curso), no Fedora
  (el host). Esto es posible porque `pivot_root` cambió la raíz al
  rootfs de la imagen.
- No existe `/host` — el filesystem del host fue desmontado con
  `umount -l` durante el arranque del contenedor.
- `readlink /proc/1/root` muestra `/` — desde dentro del mount
  namespace, la raíz siempre es `/`.

</details>

### Ejercicio 2 — chroot dentro del contenedor

Crea un mini-rootfs y haz chroot (si las capabilities lo permiten).

```bash
# Crear rootfs mínimo
mkdir -p /tmp/jail/{bin,proc,tmp}

# ¿Hay busybox disponible?
which busybox 2>/dev/null && echo "busybox disponible" || echo "no hay busybox"

# Copiar bash al rootfs (con sus libs)
cp /bin/bash /tmp/jail/bin/
mkdir -p /tmp/jail/lib /tmp/jail/lib64
ldd /bin/bash | grep -o '/[^ ]*' | while read lib; do
    mkdir -p "/tmp/jail$(dirname $lib)"
    cp "$lib" "/tmp/jail$lib" 2>/dev/null
done

# Intentar chroot
chroot /tmp/jail /bin/bash -c 'echo "Dentro del jail: pwd=$(pwd), ls /=$(ls /)"'

# Si falla: ¿qué capability se necesita?

# Limpiar
rm -rf /tmp/jail
```

<details><summary>Predicción</summary>

- `which busybox` probablemente falla (Debian no lo incluye por defecto).
- La copia de bash con sus libs debería funcionar.
- `chroot` podría funcionar si el contenedor tiene `CAP_SYS_CHROOT`
  (Docker lo incluye por defecto en las capabilities). Si funciona,
  verás `pwd=/` y `ls /=bin lib lib64 proc tmp`.
- Si falla con "Operation not permitted", es porque el contenedor fue
  lanzado con `--cap-drop=SYS_CHROOT` o en modo rootless.
- Dentro del chroot, `/proc` está vacío (no lo montamos), así que
  `ps` no funcionaría.

</details>

### Ejercicio 3 — chroot no aísla PIDs

Demuestra que chroot no crea aislamiento de procesos.

```bash
# Preparar rootfs con bash
mkdir -p /tmp/jail/{bin,proc,lib,lib64}
cp /bin/bash /tmp/jail/bin/
ldd /bin/bash | grep -o '/[^ ]*' | while read lib; do
    mkdir -p "/tmp/jail$(dirname $lib)"
    cp "$lib" "/tmp/jail$lib" 2>/dev/null
done

# Copiar ps también
cp /bin/ps /tmp/jail/bin/ 2>/dev/null
ldd /bin/ps 2>/dev/null | grep -o '/[^ ]*' | while read lib; do
    mkdir -p "/tmp/jail$(dirname $lib)"
    cp "$lib" "/tmp/jail$lib" 2>/dev/null
done

# Montar /proc del host dentro del jail
mount --bind /proc /tmp/jail/proc

# chroot y contar procesos
chroot /tmp/jail /bin/bash -c 'echo "Procesos visibles: $(ls /proc | grep -c "^[0-9]")"'

# Comparar con lo que ves fuera
echo "Procesos fuera: $(ls /proc | grep -c '^[0-9]')"

# ¿Son iguales? ¿Por qué?

# Limpiar
umount /tmp/jail/proc
rm -rf /tmp/jail
```

<details><summary>Predicción</summary>

- El número de procesos visibles será **igual** dentro y fuera del
  chroot. chroot no crea PID namespace — solo cambia dónde está `/`.
  Al montar `/proc` del host (bind mount), el jail ve exactamente los
  mismos procesos.
- Si `/bin/ps` no está disponible, `ls /proc | grep -c '^[0-9]'`
  funciona igual — cada directorio numérico en `/proc` es un PID.
- Esto contrasta con `unshare --pid` (T01) donde solo verías los
  procesos del nuevo namespace.

</details>

### Ejercicio 4 — Entender el escape de chroot

Analiza por qué el escape funciona (ejercicio conceptual, no
ejecutable).

```bash
# Lee el código de escape y responde:

echo "Código de escape (C):"
echo "  1. mkdir(\"escape\")"
echo "  2. fd = open(\".\", O_RDONLY)"
echo "  3. chroot(\"escape\")"
echo "  4. fchdir(fd)"
echo "  5. for(...) chdir(\"..\")"
echo "  6. chroot(\".\")"
echo ""
echo "Preguntas:"
echo "  a) ¿Por qué el paso 2 guarda un fd ANTES del chroot?"
echo "  b) ¿Por qué funciona el paso 3 (chroot a un subdirectorio)?"
echo "  c) ¿Por qué fchdir(fd) nos saca del nuevo chroot?"
echo "  d) ¿Por qué esto requiere root?"
echo "  e) ¿Por qué pivot_root es inmune a este ataque?"
```

<details><summary>Predicción</summary>

- **a)** El fd se guarda antes del segundo chroot porque apunta a un
  directorio que quedará **fuera** del nuevo chroot. Los file
  descriptors sobreviven a `chroot` — el kernel no los revoca.
- **b)** Funciona porque root puede llamar a `chroot()` las veces que
  quiera. Al hacer `chroot("escape")`, la raíz cambia a `escape/`,
  pero el cwd del proceso sigue en el directorio original (que ahora
  está fuera del nuevo chroot).
- **c)** `fchdir(fd)` cambia el directorio de trabajo al fd guardado.
  Como ese fd apunta fuera del nuevo chroot, el proceso queda con su
  cwd fuera de la "jaula". Desde ahí, `chdir("..")` repetido llega a
  la raíz real.
- **d)** Requiere root porque `chroot()` (la syscall) necesita
  `CAP_SYS_CHROOT`.
- **e)** `pivot_root` es inmune porque **desmonta** el viejo rootfs.
  No hay fd que valga si el filesystem fue desmontado — el inodo ya
  no apunta a nada accesible. El escape de chroot depende de que el
  viejo rootfs siga montado.

</details>

### Ejercicio 5 — Reparación con chroot (simulación)

Simula el flujo de reparación de un sistema con chroot.

```bash
# Simular un "disco dañado" con un rootfs Alpine
mkdir -p /tmp/disco-roto
docker export $(docker create alpine) | tar -xf - -C /tmp/disco-roto 2>/dev/null

# Crear un "problema" — fstab corrupto
echo "ESTO_ESTA_MAL" > /tmp/disco-roto/etc/fstab

# Flujo de reparación:
echo "=== Simulación: reparar sistema con chroot ==="

# 1. "Montar el disco" (ya lo tenemos en /tmp/disco-roto)
echo "1. mount /dev/sda2 /mnt  → simulado con /tmp/disco-roto"

# 2. Montar pseudofilesystems
mount --bind /proc /tmp/disco-roto/proc 2>/dev/null

# 3. chroot
chroot /tmp/disco-roto /bin/sh -c '
    echo "2. Dentro del sistema dañado"
    echo "3. fstab actual (corrupto):"
    cat /etc/fstab
    echo ""
    echo "4. Reparando fstab..."
    echo "# /etc/fstab - reparado" > /etc/fstab
    echo "/dev/sda2 / ext4 defaults 0 1" >> /etc/fstab
    echo "5. fstab reparado:"
    cat /etc/fstab
' 2>/dev/null || echo "(chroot no disponible — mostrando concepto)"

# 4. Verificar reparación desde fuera
echo "6. Verificación desde fuera:"
cat /tmp/disco-roto/etc/fstab

# Limpiar
umount /tmp/disco-roto/proc 2>/dev/null
rm -rf /tmp/disco-roto
```

<details><summary>Predicción</summary>

- El chroot nos permite entrar "dentro" del rootfs como si fuera el
  sistema arrancado. Podemos leer y modificar sus archivos.
- Dentro, `cat /etc/fstab` muestra `ESTO_ESTA_MAL` (el fstab corrupto).
- Después de escribir el fstab correcto, la verificación desde fuera
  (`cat /tmp/disco-roto/etc/fstab`) confirma que el cambio se guardó.
- Este es el uso más legítimo de chroot: no busca seguridad, solo
  acceso al rootfs de otro sistema para repararlo.
- En la vida real, además de fstab repararías GRUB (`grub-install`,
  `update-grub`) o reinstalarías paquetes (`apt install --reinstall`).

</details>

### Ejercicio 6 — chroot vs unshare+chroot

Compara el aislamiento con y sin PID namespace.

```bash
# Preparar rootfs
mkdir -p /tmp/compare/{bin,proc,lib,lib64}
cp /bin/bash /tmp/compare/bin/
ldd /bin/bash | grep -o '/[^ ]*' | while read lib; do
    mkdir -p "/tmp/compare$(dirname $lib)"
    cp "$lib" "/tmp/compare$lib" 2>/dev/null
done

echo "=== TEST A: Solo chroot ==="
mount --bind /proc /tmp/compare/proc
chroot /tmp/compare /bin/bash -c '
    echo "PID: $$"
    echo "Procesos: $(ls /proc | grep -c "^[0-9]")"
' 2>/dev/null || echo "(chroot falló)"
umount /tmp/compare/proc 2>/dev/null

echo ""
echo "=== TEST B: unshare --pid + chroot ==="
unshare --pid --fork --mount-proc=/tmp/compare/proc \
    chroot /tmp/compare /bin/bash -c '
    echo "PID: $$"
    echo "Procesos: $(ls /proc | grep -c "^[0-9]")"
' 2>/dev/null || echo "(unshare+chroot falló — necesita privilegios)"

# Limpiar
rm -rf /tmp/compare

# ¿Cuál se parece más a un contenedor?
```

<details><summary>Predicción</summary>

- **Test A** (solo chroot):
  - `$$` muestra el PID real del proceso (un número alto como 4523).
  - Procesos: el mismo número que el host (50+). Ve todo.
- **Test B** (unshare --pid + chroot):
  - `$$` muestra `1` — es PID 1 en su propio PID namespace.
  - Procesos: `2` (bash + ls). Solo ve los suyos.
- Test B se parece mucho más a un contenedor. La combinación de
  `unshare --pid` (aislamiento de PIDs) + `chroot` (filesystem propio)
  ya se aproxima al comportamiento de Docker.
- Si ninguno funciona (faltan capabilities), la diferencia conceptual
  sigue siendo clara: chroot solo cambia rutas, unshare crea
  aislamiento real.

</details>

### Ejercicio 7 — ¿Qué filesystem montó Docker?

Investiga cómo Docker construyó el rootfs del contenedor.

```bash
# ¿Qué tipo de filesystem es la raíz?
mount | grep ' / '

# ¿Qué capas tiene el overlay?
mount | grep ' / ' | tr ',' '\n' | grep -E 'lower|upper|work'

# ¿Cuántos puntos de montaje hay?
mount | wc -l

# ¿Hay bind mounts?
mount | grep 'bind'

# ¿De dónde viene /etc/resolv.conf?
mount | grep resolv
cat /etc/resolv.conf
```

<details><summary>Predicción</summary>

- La raíz es `overlay` — Docker usa overlay2 para combinar las capas
  de la imagen (lower = capas de solo lectura, upper = capa de
  escritura, work = directorio de trabajo interno).
- `lowerdir` tendrá varias rutas separadas por `:` (una por capa de
  la imagen). `upperdir` es donde se escriben los cambios del
  contenedor. `workdir` es interno de overlayfs.
- Habrá varios puntos de montaje: overlay para `/`, proc para `/proc`,
  devpts para `/dev/pts`, tmpfs para varios, etc.
- `/etc/resolv.conf` probablemente es un bind mount desde el host —
  Docker inyecta la configuración DNS del host.

</details>

### Ejercicio 8 — pivot_root: ¿qué pasó con el viejo rootfs?

Verifica que el filesystem del host no es accesible.

```bash
# Intentar acceder al host de cualquier forma

# ¿Hay /oldroot?
ls /oldroot 2>/dev/null || echo "/oldroot no existe"

# ¿/proc/1/root apunta fuera?
readlink /proc/1/root

# ¿Algún mount point revela el host?
mount | grep -v 'overlay\|proc\|sysfs\|devpts\|tmpfs\|cgroup\|mqueue'

# ¿Podemos ver el PID real en el host?
cat /proc/1/status | grep -E '^(Pid|NSpid)'

# ¿Hay file descriptors apuntando fuera?
ls -la /proc/1/fd/ | head -10
```

<details><summary>Predicción</summary>

- `/oldroot` no existe — Docker lo desmontó con `umount -l` después
  de `pivot_root`.
- `/proc/1/root` → `/` (siempre, desde dentro del namespace).
- No debería haber mount points del host visibles.
- `NSpid` mostrará dos líneas: el PID en el namespace del contenedor
  (1) y el PID real en el host (si el kernel lo expone). Esto muestra
  que estamos en un PID namespace distinto.
- Los fd de PID 1 apuntan a `/dev/null`, pipes, o sockets — ninguno
  al filesystem del host.
- Todo esto confirma que pivot_root + umount eliminó completamente
  el acceso al host.

</details>

### Ejercicio 9 — Mapa mental: chroot → namespaces → contenedor

Ejercicio integrador: ubica cada tecnología en la evolución hacia
contenedores.

```bash
echo "=== Evolución: chroot → contenedor ==="
echo ""
echo "1. chroot (1979) — solo filesystem"
echo "   Cambia /. No aísla PIDs, red, usuarios."
echo "   Escape trivial con root."
echo ""
echo "2. chroot + namespaces (2002-2020)"
echo "   chroot + unshare --pid → aísla PIDs"
echo "   chroot + unshare --net → aísla red"
echo "   Mejor, pero el viejo rootfs sigue accesible."
echo ""
echo "3. pivot_root + namespaces (Docker, 2013+)"
echo "   Reemplaza chroot por pivot_root"
echo "   Desmonta el viejo rootfs → no hay escape"
echo "   + cgroups → limita recursos"
echo "   = Contenedor moderno"
echo ""
echo "=== Verificar en este contenedor ==="
echo "Namespaces activos:"
ls -la /proc/1/ns/ | awk '{print $NF}' | grep -v '^$'
echo ""
echo "Filesystem raíz:"
mount | grep ' / '
echo ""
echo "cgroup:"
cat /proc/self/cgroup
```

<details><summary>Predicción</summary>

- La evolución muestra tres etapas: chroot solo (inseguro), chroot +
  namespaces (mejor pero con huecos), pivot_root + namespaces + cgroups
  (contenedor moderno).
- Los namespaces activos incluirán: `mnt`, `pid`, `net`, `uts`, `ipc`,
  `cgroup` — todos los que vimos en T01.
- El filesystem raíz es `overlay` (pivot_root a las capas de la imagen).
- El cgroup es `0::/` (cgroup namespace oculta la ruta real, como
  vimos en T02).
- Este contenedor usa **las tres tecnologías juntas**: namespaces
  (aislamiento), pivot_root (filesystem propio), cgroups (límites).

</details>

### Ejercicio 10 — ¿Qué falta para un contenedor completo?

Piensa en qué más necesita un contenedor además de chroot/pivot_root.

```bash
echo "=== Ingredientes de un contenedor ==="
echo ""
echo "Filesystem:"
echo "  [x] pivot_root — raíz propia (overlay2)"
mount | grep ' / ' | head -1

echo ""
echo "Aislamiento (namespaces):"
for ns in mnt pid net uts ipc cgroup user; do
    if [ -e /proc/1/ns/$ns ]; then
        echo "  [x] $ns namespace"
    else
        echo "  [ ] $ns namespace (no presente)"
    fi
done

echo ""
echo "Límites (cgroups):"
echo "  memory.max: $(cat /sys/fs/cgroup/memory.max 2>/dev/null || echo 'N/A')"
echo "  cpu.max:    $(cat /sys/fs/cgroup/cpu.max 2>/dev/null || echo 'N/A')"
echo "  pids.max:   $(cat /sys/fs/cgroup/pids.max 2>/dev/null || echo 'N/A')"

echo ""
echo "Seguridad adicional:"
echo "  Capabilities: $(cat /proc/1/status | grep CapEff)"
echo "  Seccomp:      $(cat /proc/1/status | grep Seccomp)"
echo "  AppArmor:     $(cat /proc/1/attr/current 2>/dev/null || echo 'N/A')"
```

<details><summary>Predicción</summary>

- **Filesystem**: overlay on / — confirmado con pivot_root.
- **Namespaces**: la mayoría presentes (mnt, pid, net, uts, ipc, cgroup).
  `user` namespace puede o no estar presente (Docker no siempre lo usa
  por defecto).
- **Cgroups**: probablemente todos `max` (sin límites explícitos en el
  contenedor del curso).
- **Seguridad adicional** (lo que aún no hemos cubierto):
  - `CapEff` (capabilities efectivas) — un subconjunto de las 40+
    capabilities de Linux. Docker elimina las peligrosas por defecto.
  - `Seccomp` — filtro de syscalls (modo 2 = filtro activo). Docker
    bloquea ~50 syscalls peligrosas.
  - `AppArmor`/SELinux — MAC (Mandatory Access Control) adicional.
- Un contenedor completo no es solo pivot_root. Es la combinación de:
  pivot_root + 6-8 namespaces + cgroups + capabilities reducidas +
  seccomp + AppArmor/SELinux.

</details>
