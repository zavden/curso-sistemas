# T03 — chroot y pivot_root

## chroot — Cambiar el directorio raíz

`chroot` cambia el directorio raíz (`/`) de un proceso. Después de un chroot,
el proceso ve un directorio como si fuera `/` y no puede acceder a nada fuera
de él (en teoría):

```bash
# Sintaxis
sudo chroot /path/to/new-root [command]

# El proceso ve /path/to/new-root como /
# No puede hacer cd .. más allá de ese punto
# Todos sus hijos heredan el chroot
```

### Preparar un rootfs mínimo

Para hacer chroot se necesita un directorio con al menos un shell y sus
dependencias. Las formas más prácticas:

```bash
# Opción 1: debootstrap (Debian) — crea un rootfs Debian mínimo
sudo apt install debootstrap
sudo debootstrap bookworm /tmp/chroot-debian http://deb.debian.org/debian

# Opción 2: extraer una imagen Docker de Alpine (muy ligera)
mkdir -p /tmp/chroot-alpine
docker export $(docker create alpine) | sudo tar -xf - -C /tmp/chroot-alpine

# Opción 3: rootfs mínimo manual (solo busybox)
mkdir -p /tmp/chroot-minimal/{bin,lib,lib64,proc,sys,dev,tmp}
cp /bin/busybox /tmp/chroot-minimal/bin/
# busybox necesita ser estático o copiar las libs necesarias
# Con busybox estático no se necesitan libs
```

### Usar chroot

```bash
# Entrar al chroot con Alpine
sudo chroot /tmp/chroot-alpine /bin/sh

# Dentro del chroot:
ls /
# bin  dev  etc  home  lib  media  mnt  opt  proc  root
# run  sbin srv  sys   tmp  usr    var

pwd
# /

cat /etc/os-release
# Alpine Linux

# No puedes ver el filesystem del host
ls /home/dev
# ls: /home/dev: No such file or directory

# Pero /proc y /sys están vacíos (no montados)
ls /proc
# (vacío)

exit
```

### Montar /proc y /dev dentro del chroot

```bash
# Para que el chroot sea funcional, montar los filesystems virtuales:
sudo mount --bind /proc /tmp/chroot-alpine/proc
sudo mount --bind /sys /tmp/chroot-alpine/sys
sudo mount --bind /dev /tmp/chroot-alpine/dev

sudo chroot /tmp/chroot-alpine /bin/sh

# Ahora sí funciona
ps aux       # muestra procesos (¡del HOST! — no hay aislamiento de PID)
hostname     # muestra el hostname del HOST

# Limpiar al salir
exit
sudo umount /tmp/chroot-alpine/proc
sudo umount /tmp/chroot-alpine/sys
sudo umount /tmp/chroot-alpine/dev
```

Nota cómo `ps aux` dentro del chroot muestra **todos los procesos del host**.
chroot solo cambia la raíz del filesystem — no aísla PIDs, red, usuarios ni
ningún otro recurso. No es un contenedor.

## Limitaciones de chroot

### No es aislamiento real

chroot solo afecta la resolución de rutas del filesystem. Un proceso con
privilegios suficientes puede escapar:

```c
// Escape de chroot (requiere root dentro del chroot):
// 1. Crear un directorio temporal
mkdir("escape", 0755);
// 2. Abrir un fd al directorio actual (dentro del chroot)
int fd = open(".", O_RDONLY);
// 3. Hacer chroot al directorio temporal (cambia la raíz)
chroot("escape");
// 4. Usar fchdir para volver al fd abierto (que está fuera del nuevo chroot)
fchdir(fd);
// 5. Hacer cd .. repetidamente hasta llegar a la raíz real
for (int i = 0; i < 100; i++) chdir("..");
// 6. Chroot a la raíz real
chroot(".");
// Ahora estamos fuera del chroot original
```

```
Por qué chroot no es seguro:
  ✗ Un proceso root puede escapar (demostrado arriba)
  ✗ No aísla PIDs — puede ver y señalizar procesos del host
  ✗ No aísla red — comparte interfaces, puertos, rutas
  ✗ No aísla usuarios — UID 0 dentro es UID 0 fuera
  ✗ No aísla IPC — puede acceder a shared memory del host
  ✗ Puede montar filesystems si tiene CAP_SYS_ADMIN
  ✗ Puede acceder a dispositivos via /dev si está montado
```

### Para qué sí sirve chroot

```
Usos legítimos de chroot:
  ✓ Reparación del sistema: arrancar desde USB, chroot al disco para
    arreglar GRUB, fstab, o paquetes rotos
  ✓ Build environments: compilar software en un rootfs limpio
  ✓ Aislamiento superficial de servicios legacy (sin garantía de seguridad)
  ✓ Pruebas de distribuciones diferentes en el mismo host

# Ejemplo clásico: reparar un sistema
sudo mount /dev/sda2 /mnt
sudo mount --bind /proc /mnt/proc
sudo mount --bind /sys /mnt/sys
sudo mount --bind /dev /mnt/dev
sudo chroot /mnt
# Ahora estás "dentro" del sistema del disco, puedes reinstalar GRUB, etc.
```

## pivot_root — La alternativa segura

`pivot_root` es una syscall y comando que **intercambia** la raíz del
filesystem: el nuevo rootfs se convierte en `/` y el antiguo rootfs se mueve a
un subdirectorio. A diferencia de chroot, es un cambio a nivel de mount
namespace, no solo una redirección de rutas:

```
chroot:
  Solo cambia la referencia de "/" para el proceso
  El viejo rootfs sigue accesible (via fds abiertos, escape)

pivot_root:
  Intercambia la raíz del mount namespace completo
  El viejo rootfs se mueve a un punto de montaje específico
  Después se puede desmontar y desaparecer completamente
```

### Cómo funciona

```bash
# Sintaxis
pivot_root new_root put_old

# new_root: el directorio que será la nueva raíz (/)
# put_old:  directorio DENTRO de new_root donde se moverá la raíz antigua

# Requisitos:
# - new_root y put_old deben ser mount points
# - new_root no puede ser el rootfs actual
# - Debe ejecutarse en un mount namespace aislado
```

### Ejemplo con pivot_root

```bash
# Preparar el nuevo rootfs (usando Alpine como ejemplo)
mkdir -p /tmp/newroot
docker export $(docker create alpine) | sudo tar -xf - -C /tmp/newroot
mkdir -p /tmp/newroot/oldroot

# Crear un mount namespace aislado
sudo unshare --mount bash

# Hacer que el nuevo rootfs sea un mount point
mount --bind /tmp/newroot /tmp/newroot

# pivot_root: intercambiar raíces
cd /tmp/newroot
pivot_root . oldroot

# Ahora / es Alpine y /oldroot es el host
ls /
# bin  dev  etc  home  lib  media  mnt  oldroot  opt  proc  ...

# Montar /proc para que funcione ps
mount -t proc proc /proc

# Desmontar el viejo rootfs (ya no lo necesitamos)
umount -l /oldroot
rmdir /oldroot

# Ahora NO hay forma de volver al host
# El filesystem del host ya no está accesible
ls /oldroot
# ls: /oldroot: No such file or directory

exit
```

### Por qué Docker usa pivot_root

```
chroot:
  ✗ Escape posible con root
  ✗ El viejo rootfs sigue accesible

pivot_root:
  ✓ El viejo rootfs se puede desmontar completamente
  ✓ Una vez desmontado, no hay forma de acceder al filesystem del host
  ✓ Funciona con mount namespaces para aislamiento completo
  ✓ Es el mecanismo que usan Docker, Podman, y todos los runtimes OCI
```

El flujo de un runtime de contenedor:

```
1. clone() con CLONE_NEWNS (nuevo mount namespace)
2. Montar el rootfs de la imagen
3. pivot_root al rootfs de la imagen
4. Desmontar el viejo rootfs
5. Montar /proc, /sys, /dev dentro del nuevo rootfs
6. Ejecutar el entrypoint
```

## Comparación

| Aspecto | chroot | pivot_root |
|---|---|---|
| Mecanismo | Cambia la referencia de "/" | Intercambia el rootfs del mount namespace |
| Escape con root | Posible | No (si se desmonta el viejo rootfs) |
| Requiere mount namespace | No | Sí (para ser útil) |
| Viejo rootfs | Sigue accesible | Se puede desmontar completamente |
| Complejidad | Baja (un comando) | Mayor (necesita namespaces + montajes) |
| Uso moderno | Reparación, builds | Contenedores (Docker, Podman, runc) |
| Seguridad | Baja (no es aislamiento real) | Alta (combinado con namespaces) |

---

## Ejercicios

### Ejercicio 1 — chroot básico

```bash
# Crear un rootfs de Alpine
mkdir -p /tmp/chroot-test
docker export $(docker create alpine) | sudo tar -xf - -C /tmp/chroot-test

# Entrar al chroot
sudo chroot /tmp/chroot-test /bin/sh

# ¿Qué distribución ves?
cat /etc/os-release

# ¿Puedes ver los procesos del host?
# (necesitas montar /proc primero)
mount -t proc proc /proc
ps aux | wc -l
# ¿Son los del host o los del chroot?

exit
sudo umount /tmp/chroot-test/proc 2>/dev/null
```

### Ejercicio 2 — Verificar que chroot no aísla PIDs

```bash
# Dentro del chroot, montar /proc del host
sudo mount --bind /proc /tmp/chroot-test/proc
sudo chroot /tmp/chroot-test /bin/sh

# ¿Cuántos procesos ves?
ps aux | wc -l
# Todos los del host — chroot NO aísla PIDs

# ¿Puedes enviar señales a procesos del host?
# kill -0 1    # testear si puedes señalizar PID 1
# (Sí — no hay aislamiento)

exit
sudo umount /tmp/chroot-test/proc
```

### Ejercicio 3 — Comparar chroot vs unshare

```bash
# chroot sin namespace: ve todos los procesos
sudo mount --bind /proc /tmp/chroot-test/proc
sudo chroot /tmp/chroot-test sh -c 'ps aux | wc -l'
sudo umount /tmp/chroot-test/proc

# unshare con PID namespace + chroot: ve solo los suyos
sudo unshare --pid --fork --mount-proc=/tmp/chroot-test/proc \
    chroot /tmp/chroot-test sh -c 'ps aux | wc -l'
# Debería mostrar 2 (sh + ps)

# ¿Cuál se parece más a un contenedor?

# Limpiar
sudo rm -rf /tmp/chroot-test
```
