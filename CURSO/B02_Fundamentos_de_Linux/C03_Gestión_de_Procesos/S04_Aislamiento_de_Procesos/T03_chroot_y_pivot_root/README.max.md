# T03 — chroot y pivot_root

## Tabla de Contenidos

1. [chroot — Cambiar el directorio raíz](#chroot--cambiar-el-directorio-raíz)
2. [Preparación de un rootfs mínimo](#preparación-de-un-rootfs-mínimo)
3. [Uso de chroot](#uso-de-chroot)
4. [Montar /proc y /dev dentro del chroot](#montar-proc-y-dev-dentro-del-chroot)
5. [Limitaciones de chroot](#limitaciones-de-chroot)
6. [pivot_root — La alternativa segura](#pivot_root--la-alternativa-segura)
7. [Comparación directa](#comparación-directa)
8. [Ejercicios](#ejercicios)

---

## chroot — Cambiar el directorio raíz

`chroot` es un chiamada al sistema que cambia el directorio raíz (`/`) de un proceso y todos sus descendientes. Después de ejecutar `chroot`, el proceso percibe un directorio arbitrario como si fuera `/`, y no puede resolver rutas que estén "por encima" de ese punto.

```
Concepto clave:

  Host filesystem          Proceso con chroot(/var/chroot)
  ─────────────           ──────────────────────────────
  /                       /var/chroot
  ├── bin                 ├── bin
  ├── etc                 ├── etc
  ├── home                 ├── home      ← ve SU /home, no el del host
  ├── var                  ├── var
  │   └── chroot           ├── tmp       ← "raíz" para este proceso
  │       ├── bin          ├── proc      (montado después)
  │       ├── etc          └── ...
  │       └── ...
  └── usr

  El proceso hace chroot("/var/chroot") → para él, "/" = /var/chroot
```

### Sintaxis

```bash
sudo chroot /path/to/new-root [command [arg...]]

# Si no se especifica command, se ejecuta el shell por defecto del sistema (/bin/sh)
```

**Puntos clave:**
- Solo afecta la resolución de rutas — el proceso sigue ejecutándose en el mismo sistema
- Todos los procesos hijos heredan el chroot
- Requiere privilegios de root fuera del chroot
- Es una llamada de usuario, no un namespace de kernel (a diferencia de mount namespaces)

---

## Preparación de un rootfs mínimo

Para usar chroot necesitas un directorio que contenga al menos un ejecutable (shell) y todas sus dependencias. Hay tres métodos prácticos:

### Método 1: debootstrap (Debian/Ubuntu)

Crea un rootfs Debian mínimo completo:

```bash
sudo apt install debootstrap

# Crear un rootfs Debian Bookworm
sudo debootstrap bookworm /tmp/chroot-debian http://deb.debian.org/debian

# Verificar estructura
ls /tmp/chroot-debian/
# bin  boot  dev  etc  home  lib  lib64  media  mnt  opt  proc  root
# run  sbin  srv  sys  tmp  usr  var
```

### Método 2: Imagen Docker de Alpine (más rápido)

Extraer una imagen de Alpine (muy ligera, ~5MB):

```bash
mkdir -p /tmp/chroot-alpine
docker export $(docker create alpine) | sudo tar -xf - -C /tmp/chroot-alpine

# Verificar
ls /tmp/chroot-alpine/
# bin  dev  etc  home  lib  media  mnt  opt  proc  root  run  sbin
# srv  sys  tmp  usr  var
```

### Método 3: busybox estático (sin Docker, sin debootstrap)

Útil cuando no hay Docker ni debootstrap disponibles:

```bash
mkdir -p /tmp/chroot-minimal/{bin,lib,lib64,proc,sys,dev,tmp,etc}

# Descargar busybox estático
curl -o /tmp/chroot-minimal/busybox https://busybox.net/downloads/binaries/1.36.0-x86_64-linux-musl/busybox
chmod +x /tmp/chroot-minimal/busybox

# Crear symlinks a los comandos básicos
cd /tmp/chroot-minimal
for cmd in sh ls cat echo grep find mount umount ps; do
    ln -sf busybox $cmd
done

# /proc y /sys son necesarios para que muchas herramientas funcionen
mkdir -p proc sys dev tmp
```

### Comparación de métodos

| Método | Tamaño | Dependencias | Velocidad | Isolation real |
|--------|--------|---------------|-----------|----------------|
| debootstrap | ~200MB+ | Completo | Lento | No |
| Docker Alpine | ~5-10MB | Mínimo | Rápido | No |
| busybox estático | ~3MB | Ninguna | Rápido | No |

**Nota:** Los tres métodos crean un filesystem funcional, pero ninguno proporciona aislamiento real. chroot solo cambia la raíz del filesystem.

---

## Uso de chroot

### Entrar a un chroot básico

```bash
# Entrar al chroot de Alpine
sudo chroot /tmp/chroot-alpine /bin/sh

# Lo que ves dentro:
/ # pwd
/
/ # ls /
bin  dev  etc  home  lib  media  mnt  opt  proc  root
run  sbin srv  sys  tmp  usr  var

/ # cat /etc/os-release
NAME="Alpine Linux"
VERSION_ID=v3.19
...
```

### Verificar que el exterior es inaccesible

```bash
# Dentro del chroot intentamos acceder a rutas del host:

/ # ls /home/dev
ls: /home/dev: No such file or directory   ← /home del host no existe aquí

/ # ls /tmp
tmp                                        ← /tmp de Alpine (no el del host)

/ # cat /etc/hostname
localhost                                   ← hostname del contenedor, no del host
```

### Intentando "escapar" teóricamente

```bash
# Dentro del chroot, intenta subir directorios:
/ # cd ..
/ # pwd
/                                 ← cd .. se detiene en la raíz del chroot

# Pero si tienes root dentro del chroot, el escape ES posible (ver sección siguiente)
```

---

## Montar /proc y /dev dentro del chroot

Por defecto, `/proc` y `/sys` dentro del chroot están vacíos porque son directorios vacíos del rootfs. Para que herramientas como `ps` funcionen, hay que montar los pseudofilesystems del kernel:

```bash
# FUERA del chroot, montar los pseudofilesystems
sudo mount --bind /proc /tmp/chroot-alpine/proc
sudo mount --bind /sys  /tmp/chroot-alpine/sys
sudo mount --bind /dev  /tmp/chroot-alpine/dev

# Ahora entrar al chroot
sudo chroot /tmp/chroot-alpine /bin/sh

# Dentro, ahora ps funciona:
/ # ps aux
USER   PID  COMMAND
root     1  /bin/sh                     ← shell dentro del chroot
root    15  ps aux                      ← comando que acabamos de ejecutar

# ⚠️ ATENCIÓN: Esto muestra los procesos del HOST, no procesos aislados
/ # echo $$
1                                ← Dentro del chroot, somos PID 1
/ # cat /proc/1/cmdline | tr '\0' ' '
/bin/sh                          ← Pero ps muestra TODOS los procesos del sistema
```

**Advertiencias críticas:**

```bash
# ⚠️ Dentro del chroot con /proc montado, puedes ver TODOS los procesos del host:
/ # ps aux | head -20
USER   PID  COMMAND
root     1  /sbin/init
root   234  [kworker/0:1]
...
# ↑ Son los procesos del HOST, no hay aislamiento de PID

# ⚠️ Puedes enviar señales a procesos del host:
/ # kill -0 1    # verificar si puedes ver PID 1 del host
# (sin error = puedes señalizarlo)
```

### Limpieza al salir

```bash
# Salir del chroot
exit

# Desmontar los pseudofilesystems (importante: antes de borrar el rootfs)
sudo umount /tmp/chroot-alpine/proc
sudo umount /tmp/chroot-alpine/sys
sudo umount /tmp/chroot-alpine/dev

# Limpiar rootfs
sudo rm -rf /tmp/chroot-alpine
```

---

## Limitaciones de chroot

### Por qué chroot NO es aislamiento

chroot solo cambia la referencia de "/" para resolución de rutas. No crea ningún tipo de namespace ni aislamiento:

| Recurso | ¿Aislado por chroot? | Implicación |
|---------|---------------------|-------------|
| Filesystem | Parcialmente (rutas) | Viejo rootfs accesible via fd abiertos |
| PIDs | **NO** | Puede ver/señalar procesos del host |
| Red | **NO** | Comparte interfaces, puertos, rutas |
| Usuarios | **NO** | UID 0 dentro = UID 0 fuera |
| IPC | **NO** | Acceso a shared memory del host |
| Hostname | **NO** | Comparte el hostname del host |
| Mounts | **NO** | Puede montar filesystems si tiene CAP_SYS_ADMIN |

### Escape de chroot con root

Un proceso root dentro de un chroot puede escapar usando el siguiente método clásico:

```c
// El exploit funciona así:
// 1. Crear un directorio temporal
mkdir("escape", 0755);

// 2. Abrir un file descriptor al directorio actual (dentro del chroot)
int fd = open(".", O_RDONLY);

// 3. Hacer chroot al directorio temporal (cambia la raíz)
chroot("escape");

// 4. Usar fchdir para volver al fd abierto (que apunta fuera del nuevo chroot)
fchdir(fd);

// 5. Hacer cd .. repetidamente hasta llegar a la raíz real del filesystem
for (int i = 0; i < 100; i++) chdir("..");

// 6. Ahora hacer chroot a la raíz real (estamos fuera del chroot original)
chroot(".");

// 7. Ahora tenemos acceso al filesystem completo del host
```

```bash
# Versión shell del exploit (requiere estar root dentro del chroot):
mkdir escape
cd escape
exec 3<.
chroot .
fchdir 3
for i in $(seq 1 100); do cd ..; done
chroot .
# Ahora estás fuera del chroot original
```

### Lo que SÍ hace chroot

```
Casos de uso legítimos de chroot:
  ✓ Reparación de sistemas: arrancar desde live USB, chroot al disco
    para arreglar GRUB, fstab, o paquetes rotos
  ✓ Build environments: compilar software en un rootfs limpio y aislado
    (para no contaminar el sistema host)
  ✓ Pruebas de distribuciones: ejecutar otra distro sin VM
  ✓ Aislamiento SUPERFICIAL de servicios legacy (sin garantía de seguridad)
  ✓ Chroots de emergencia (por ejemplo, pacstrap en Arch Linux)
```

### Ejemplo: Reparar GRUB con chroot

```bash
# Arrancar desde live USB
# Montar la partición raíz del sistema dañado
sudo mount /dev/sda2 /mnt

# Montar los pseudofilesystems necesarios
sudo mount --bind /proc /mnt/proc
sudo mount --bind /sys  /mnt/sys
sudo mount --bind /dev  /mnt/dev

# Hacer chroot
sudo chroot /mnt

# Ahora estamos "dentro" del sistema del disco
# Podemos reinstalar GRUB, reparar paquetes, editar fstab, etc.
update-grub
grub-install /dev/sda
exit

# Desmontar
sudo umount /mnt/proc /mnt/sys /mnt/dev
sudo umount /mnt
```

---

## pivot_root — La alternativa segura

`pivot_root` es una llamada al sistema (no solo un comando) que **intercambia** la raíz del filesystem a nivel de mount namespace. A diferencia de chroot, el viejo rootfs se mueve a un subdirectorio y puede ser completamente desmontado.

```
Diferencia fundamental entre chroot y pivot_root:

chroot:
  Host:  /old/root      →  Nuevo proceso ve /old/root como /
  El viejo "/" sigue accesible por cualquier fd abierto

pivot_root:
  Host:  /old/root
  New:   /new/root
  ─────────────────────────────────────────────
  Después de pivot_root(/new/root, /old/root):
    → /new/root se convierte en / (nueva raíz)
    → /old/root se mueve a /old/root (dentro del nuevo root)
    → El viejo rootfs YA NO es accesible desde /
    → Puede desmontarse completamente (umount -l /old/root)
```

### Requisitos para usar pivot_root

```bash
# Requisitos estrictos:
# 1. new_root y put_old deben ser mount points
# 2. new_root no puede ser el rootfs actual
# 3. put_old debe estar DENTRO de new_root
# 4. DEBE ejecutarse dentro de un mount namespace aislado (unshare --mount)
#    de lo contrario, afectaría a todo el sistema
```

### Ejemplo completo con pivot_root

```bash
# 1. Preparar el nuevo rootfs
mkdir -p /tmp/newroot
docker export $(docker create alpine) | sudo tar -xf - -C /tmp/newroot

# 2. Crear el directorio para el viejo rootfs (DENTRO del nuevo)
mkdir -p /tmp/newroot/oldroot

# 3. Crear un mount namespace aislado (CRÍTICO)
sudo unshare --mount bash

# 4. Dentro del namespace aislado, hacer que newroot sea un mount point
cd /tmp/newroot
mount --bind /tmp/newroot /tmp/newroot

# 5. Ejecutar pivot_root
pivot_root . oldroot

# 6. Ahora lashell está en el nuevo rootfs
ls /
# bin  dev  etc  home  lib  media  mnt  oldroot  opt  proc  root  run  sbin srv  sys  tmp  usr  var
# oldroot aparece aquí = el viejo rootfs se movió aquí

# 7. Montar /proc para que funcionen las herramientas
mount -t proc proc /proc

# 8. Desmontar el viejo rootfs (ya no lo necesitamos)
umount -l /oldroot
rmdir /oldroot

# 9. Verificar que el host ya no es accesible
ls /oldroot
# ls: /oldroot: No such file or directory  ← El viejo rootfs desapareció

# 10. Salir y verificar limpieza
exit

# El namespace se destruye automáticamente cuando el último proceso sale
```

### Por qué Docker usa pivot_root

```
Comparación de seguridad chroot vs pivot_root:

chroot:
  ✗ Escape posible con root (exploit anterior)
  ✗ Viejo rootfs sigue accesible (via fds, o escape)
  ✗ No hay garantía de que el proceso no vea el host
  ✗ Solo cambia resolución de rutas

pivot_root:
  ✓ Viejo rootfs se mueve a un punto de montaje
  ✓ Puede desmontarse completamente (umount -l)
  ✓ Una vez desmontado, NO HAY forma de volver al host
  ✓ Funciona correctamente con mount namespaces
  ✓ Es lo que usan Docker, Podman, containerd, runc — todos los runtimes OCI
```

### Flujo exacto que usa Docker internamente

```
Runtime de contenedor (como lo hace Docker/runc):

1. clone(CLONE_NEWNS)        → Crear nuevo mount namespace
2. mount(rootfs_image)        → Montar la imagen como rootfs
3. pivot_root(rootfs, put_old) → Cambiar la raíz
4. umount(put_old)            → Desmontar y eliminar el viejo rootfs
5. mount(/proc)               → Montar proc
6. mount(/sys)                → Montar sys
7. mount(/dev)                → Montar dev
8. exec(entrypoint)           → Ejecutar el proceso

En resumen:
  chroot  = cambio de raíz a nivel de ruta
  pivot_root = cambio de raíz a nivel de mount namespace
```

---

## Comparación Directa

| Aspecto | chroot | pivot_root |
|---------|--------|------------|
| Mecanismo | Cambia referencia de "/" | Intercambia rootfs del mount namespace |
| Escape con root | Posible (exploit anterior) | No (si se desmonta el viejo rootfs) |
| Viejo rootfs | Accesible via fds abiertos | Se mueve a put_old, puede desmontarse |
| Requiere mount namespace | No | Sí (para utilidad real) |
| Complejidad | Baja (un comando) | Mayor (necesita namespaces + montajes) |
| Uso moderno | Reparación, builds, chroots | Contenedores (Docker, Podman, runc) |
| Seguridad | Baja (no es aislamiento real) | Alta (combinado con namespaces) |
| Reversible | No (solo sale el proceso) | Sí (viejo rootfs accesible hasta desmontar) |

---

## Ejercicios

### Ejercicio 1 — chroot básico

```bash
# Crear un rootfs de Alpine
mkdir -p /tmp/chroot-test
docker export $(docker create alpine) | sudo tar -xf - -C /tmp/chroot-test

# Entrar al chroot
sudo chroot /tmp/chroot-test /bin/sh

# Dentro, responder:
# 1. ¿Qué distribución ves?
cat /etc/os-release

# 2. ¿Cuál es tu directorio actual?
pwd

# 3. Intenta ver el exterior:
ls /home
# ¿Qué resultado esperas?

# 4. Montar /proc para ver procesos (FUERA del chroot):
exit
sudo mount --bind /proc /tmp/chroot-test/proc
sudo chroot /tmp/chroot-test /bin/sh

# 5. ¿Cuántos procesos ves?
ps aux | wc -l

# ¿Son del host o del chroot?

exit
sudo umount /tmp/chroot-test/proc
sudo rm -rf /tmp/chroot-test
```

### Ejercicio 2 — Demostrar que chroot NO aísla PIDs

```bash
# Preparar
mkdir -p /tmp/chroot-ns-test
docker export $(docker create alpine) | sudo tar -xf - -C /tmp/chroot-ns-test

# 1. Sin mount binds: /proc está vacío
sudo chroot /tmp/chroot-ns-test /bin/sh -c 'ps aux'
# USER   PID  COMMAND
# Error: ps: you need to mount /proc

# 2. Con /proc del host montado: ves TODOS los procesos
sudo mount --bind /proc /tmp/chroot-ns-test/proc
sudo chroot /tmp/chroot-ns-test /bin/sh

# ¿Cuántos procesos ves?
ps aux | wc -l

# ¿Puedes ver el proceso PID 1 del host?
ps aux | grep 'init\|systemd' | head -5

# Ahora intenta señalizar un proceso del host
# kill -0 1    # Verifica si puedes "ver" PID 1
# kill -TERM 1 # NO HACER ESTO, solo verifica acceso

exit
sudo umount /tmp/chroot-ns-test/proc
sudo rm -rf /tmp/chroot-ns-test
```

### Ejercicio 3 — Comparar chroot vs unshare + chroot

```bash
# Objetivo: ver la diferencia entre solo chroot y chroot + PID namespace

mkdir -p /tmp/compare-test/rootfs
docker export $(docker create alpine) | sudo tar -xf - -C /tmp/compare-test/rootfs

# TEST A: Solo chroot (ve procesos del host)
sudo mount --bind /proc /tmp/compare-test/rootfs/proc
echo "=== Solo chroot ==="
sudo chroot /tmp/compare-test/rootfs sh -c 'echo "PID actual: $$"; ps aux | wc -l'
sudo umount /tmp/compare-test/rootfs/proc

# TEST B: unshare con PID namespace + chroot
echo "=== unshare --pid + chroot ==="
sudo unshare --pid --fork --mount-proc=/tmp/compare-test/rootfs/proc \
    chroot /tmp/compare-test/rootfs sh -c 'echo "PID actual: $$"; ps aux | wc -l'

# Preguntas:
# 1. En el test A, ¿cuántos procesos ves?
# 2. En el test B, ¿cuántos procesos ves?
# 3. ¿Cuál se parece más a un contenedor real?

sudo rm -rf /tmp/compare-test
```

### Ejercicio 4 — Escape de chroot (demostración conceptual)

```bash
# ADVERTENCIA: Solo para entender cómo funciona, NO ejecutes en sistemas de producción
# Requiere estar root DENTRO del chroot

# Este ejercicio es conceptual. Para probarlo:
mkdir -p /tmp/escape-test/rootfs
docker export $(docker create alpine) | sudo tar -xf - -C /tmp/escape-test/rootfs
sudo chroot /tmp/escape-test/rootfs /bin/sh

# Dentro del chroot, ejecuta como root:
# (NO LO HAGAS en un sistema que te importe)

# El flujo conceptual:
# mkdir escape; cd escape; exec 3<.; chroot .; fchdir 3
# for i in $(seq 1 100); do cd ..; done; chroot .

# Después de esto, estarías en el filesystem del host
# Verifica: ls /          ← ves los directorios del host

# SALIDA INMEDIATAMENTE después de verificar
exit

# Limpieza
sudo rm -rf /tmp/escape-test
```

### Ejercicio 5 — Reparación de sistema con chroot

```bash
# Este ejercicio simula la reparación de GRUB
# Supongamos que /dev/sda2 es la partición raíz

# mkdir -p /mnt
# sudo mount /dev/sda2 /mnt
# sudo mount --bind /proc /mnt/proc
# sudo mount --bind /sys  /mnt/sys
# sudo mount --bind /dev  /mnt/dev
# sudo chroot /mnt

# Dentro del chroot podrías hacer:
# update-grub
# grub-install /dev/sda
# apt-get install --reinstall grub-pc

# Salir y desmontar
# exit
# sudo umount /mnt/proc /mnt/sys /mnt/dev
# sudo umount /mnt
```

### Ejercicio 6 — Investigar pivot_root vs chroot

```bash
# Crear dos rootfs para comparar
mkdir -p /tmp/pivot-test/rootfs /tmp/chroot-test/rootfs
docker export $(docker create alpine) | sudo tar -xf - -C /tmp/pivot-test/rootfs
docker export $(docker create alpine) | sudo tar -xf - -C /tmp/chroot-test/rootfs

# OPCIONAL: Investigar pivot_root requiere unshare --mount
# Esto es peligroso de probar en un sistema real
# Demostramos la diferencia teórica

echo "=== Teoría ===
chroot:    Solo cambia la referencia de /
            Vieja raíz accesible via /proc/PID/root o fds
pivot_root: Intercambia la raíz del mount namespace
            Vieja raíz se mueve a put_old y puede desmontarse"

# Ver en /proc/self/root qué usa un contenedor Docker real:
docker run --rm alpine readlink /proc/1/root
# Respuesta: /var/lib/docker/overlay2/... (usa pivot_root internamente)

sudo rm -rf /tmp/pivot-test /tmp/chroot-test
```
