# T01 — Directorios raíz

## Objetivo

Explorar la jerarquía del Filesystem Hierarchy Standard (FHS) en Linux:
identificar el propósito de cada directorio raíz, localizar software instalado,
verificar permisos especiales, y comparar la estructura entre Debian y
AlmaLinux.

---

## Errores corregidos respecto al material original

1. **`/lib` en AlmaLinux apunta a `/usr/lib`, no a `/usr/lib64`** — La tabla
   original dice que `/lib` en AlmaLinux es symlink a `/usr/lib64`. Esto es
   incorrecto. En AlmaLinux/RHEL, `/lib` → `/usr/lib` y `/lib64` → `/usr/lib64`.
   Son dos symlinks distintos para dos directorios distintos.

---

## El Filesystem Hierarchy Standard (FHS)

El **FHS** es la especificación que define la estructura de directorios en
sistemas Linux. Todas las distribuciones la siguen (con variaciones menores).
El objetivo es que administradores y programas sepan dónde encontrar cada tipo
de archivo sin importar la distribución.

La versión actual es FHS 3.0 (junio 2015), mantenida por la Linux Foundation.

## El directorio raíz: `/`

Todo en Linux parte de `/`. No existe el concepto de "unidades" como en Windows
(C:, D:). Todos los dispositivos, particiones y sistemas de archivos remotos se
**montan** en algún punto del árbol que nace en `/`.

```bash
ls /
# bin  boot  dev  etc  home  lib  lib64  media  mnt  opt  proc  root  run  sbin  srv  sys  tmp  usr  var
```

Incluso un disco USB montado en `/mnt/usb` es parte del mismo árbol.

## Directorios principales

### `/bin` — Binarios esenciales

Históricamente contenía los comandos fundamentales necesarios para arrancar el
sistema y operar en modo single-user (sin `/usr` montado):

```bash
ls /bin/
# bash, cat, cp, echo, grep, ls, mkdir, mount, mv, rm, sh, ...
```

> **Nota**: En distribuciones modernas con `/usr merge` (ver T02), `/bin` es un
> symlink a `/usr/bin`. La separación física entre `/bin` y `/usr/bin` ya no
> existe, pero los mismos binarios esenciales siguen presentes.

### `/sbin` — Binarios de administración

Comandos de administración del sistema, típicamente ejecutados por root:

```bash
ls /sbin/
# fdisk, fsck, ifconfig, init, iptables, mkfs, mount, reboot, shutdown, ...
```

La diferencia con `/bin` no es de permisos (cualquier usuario puede ejecutar
algunos), sino de propósito: `/sbin` contiene herramientas de administración
del sistema, no comandos de uso general.

```bash
# Cualquier usuario puede ver la versión de fdisk
/sbin/fdisk --version

# Pero solo root puede ejecutar operaciones de disco
/sbin/fdisk /dev/sda
# fdisk: cannot open /dev/sda: Permission denied
```

> Con `/usr merge`, `/sbin` es symlink a `/usr/sbin`.

### `/usr` — Programas y recursos del sistema

El directorio más grande. Contiene la mayoría del software instalado:

```
/usr/
├── bin/       ← Binarios de usuario (gcc, python, vim, git...)
├── sbin/      ← Binarios de administración adicionales
├── lib/       ← Bibliotecas compartidas
├── lib64/     ← Bibliotecas de 64 bits (en RHEL/AlmaLinux)
├── include/   ← Headers de C/C++ para desarrollo
├── share/     ← Datos independientes de arquitectura (man pages, docs, locales)
├── local/     ← Software instalado localmente (no por el gestor de paquetes)
└── src/       ← Código fuente (headers del kernel en algunas distros)
```

`/usr` se puede montar como solo lectura en producción, ya que su contenido
no cambia durante la operación normal del sistema.

#### `/usr/local/`

Software instalado manualmente por el administrador (compilado desde fuente,
scripts custom). Tiene la misma estructura que `/usr/`:

```
/usr/local/
├── bin/       ← Binarios locales
├── lib/       ← Bibliotecas locales
├── include/   ← Headers locales
└── share/     ← Datos locales
```

El gestor de paquetes (apt, dnf) **nunca** toca `/usr/local/`. Esto evita
conflictos entre software del sistema y software instalado manualmente.

```bash
# Software del sistema
which gcc
# /usr/bin/gcc

# Software compilado manualmente
which my-custom-tool
# /usr/local/bin/my-custom-tool
```

### `/etc` — Configuración del sistema

Archivos de configuración de todo el sistema. Es el directorio que más
frecuentemente edita un administrador:

```bash
ls /etc/
# apt/  bash.bashrc  crontab  fstab  group  hostname  hosts
# network/  nginx/  passwd  shadow  ssh/  sudoers  sysctl.conf  ...
```

Todos los archivos en `/etc` son **específicos del host** — configuran esta
máquina en particular. El contenido de `/etc` es lo que diferencia a un
servidor de otro con el mismo software instalado.

Cubierto en profundidad en T04.

### `/var` — Datos variables

Datos que cambian durante la operación normal del sistema:

```
/var/
├── log/     ← Logs del sistema y aplicaciones
├── cache/   ← Caches de aplicaciones (apt, dnf, man-db)
├── spool/   ← Colas de trabajo (impresión, correo, cron)
├── lib/     ← Estado persistente de aplicaciones (bases de datos, gestores de paquetes)
├── tmp/     ← Temporales que sobreviven reboot (a diferencia de /tmp)
└── mail/    ← Buzones de correo local
```

> `/var/run` es un symlink a `/run` en distribuciones modernas.

Cubierto en profundidad en T03.

### `/tmp` — Archivos temporales

Cualquier usuario puede escribir aquí. Los archivos se eliminan periódicamente
(por `systemd-tmpfiles` o al reiniciar):

```bash
# Sticky bit: cualquiera puede crear, pero solo el dueño puede borrar
ls -ld /tmp
# drwxrwxrwt 15 root root 4096 ... /tmp

# La "t" al final indica sticky bit activo
```

En muchas distribuciones modernas, `/tmp` es un **tmpfs** (montado en RAM):

```bash
df -h /tmp
# Filesystem      Size  Used Avail Use% Mounted on
# tmpfs           2.0G   12K  2.0G   1% /tmp
```

Los archivos en tmpfs no se escriben a disco — son más rápidos pero se pierden
al reiniciar y consumen RAM.

#### `/tmp` vs `/var/tmp`

| Aspecto | `/tmp` | `/var/tmp` |
|---|---|---|
| Sticky bit | Sí (`1777`) | Sí (`1777`) |
| Limpieza automática | Al reiniciar o cada ~10 días | Cada ~30 días |
| Sobrevive reboot | No (si es tmpfs) | Sí |
| Uso | Temporales efímeros | Temporales que deben persistir |

### `/home` — Directorios personales

Cada usuario regular tiene su directorio en `/home/<usuario>`:

```bash
ls -la /home/
# drwxr-x--- 5 alice alice 4096 ... alice
# drwxr-x--- 3 bob   bob   4096 ... bob
```

Contiene archivos personales, configuraciones de aplicaciones (dotfiles),
shells, etc. Los dotfiles (`.bashrc`, `.ssh/`, `.config/`) configuran el
entorno del usuario:

```bash
ls -a ~
# .  ..  .bash_history  .bashrc  .config  .local  .profile  .ssh  Documents  ...
```

### `/root` — Home de root

El directorio personal del usuario root. No está en `/home/root` sino en
`/root` para que esté disponible incluso si `/home` está en una partición
separada que no se pudo montar. Permisos `700` — solo root puede acceder.

### `/opt` — Software de terceros

Software autocontenido que no sigue la convención `/usr/bin` + `/usr/lib`:

```
/opt/some-app/
├── bin/
├── lib/
├── etc/
└── share/
```

Cada aplicación tiene su propio subdirectorio. Usado típicamente por software
comercial, IDEs, y aplicaciones distribuidas como "bundles" (Chrome, VS Code).

### `/boot` — Arranque del sistema

Contiene todo lo necesario para arrancar: kernel, initramfs, configuración del
bootloader:

```bash
ls /boot/
# vmlinuz-6.1.0-18-amd64           ← Kernel comprimido
# initrd.img-6.1.0-18-amd64        ← Initial RAM disk
# config-6.1.0-18-amd64            ← Configuración del kernel
# System.map-6.1.0-18-amd64        ← Mapa de símbolos del kernel
# grub/                             ← Configuración de GRUB
```

En sistemas UEFI, también existe `/boot/efi/` con la partición EFI System.

### `/media` y `/mnt` — Puntos de montaje

- `/media/` — montaje automático de medios removibles (USB, CD)
- `/mnt/` — montaje temporal manual por el administrador

La convención: `/media` para automount, `/mnt` para montajes manuales
temporales.

### `/srv` — Datos de servicios

Datos servidos por el sistema (web, FTP, repositorios). En la práctica, muchas
distribuciones dejan `/srv` vacío y usan `/var/www` para web. El FHS lo define
pero su uso es inconsistente.

### `/run` — Datos de runtime

Datos volátiles de runtime (creados después del arranque):

```bash
ls /run/
# docker.sock  systemd/  user/  lock/  sshd.pid  ...
```

Es un **tmpfs** — se borra al reiniciar. Contiene PIDs, sockets, locks y
otros archivos que solo tienen sentido mientras el sistema está corriendo.
`/var/run` es un symlink a `/run` en distribuciones modernas.

### Directorios virtuales: `/dev`, `/proc`, `/sys`

Estos directorios no contienen archivos reales en disco. Son interfaces al
kernel:

| Directorio | Tipo | Contenido |
|---|---|---|
| `/dev` | devtmpfs | Dispositivos (discos, terminales, null, random) |
| `/proc` | procfs | Información de procesos y del kernel |
| `/sys` | sysfs | Dispositivos de hardware y drivers |

Cubiertos en profundidad en S02 (Sistemas de Archivos Virtuales).

---

## Diferencias entre Debian y AlmaLinux

| Directorio | Debian 12 (bookworm) | AlmaLinux 9 |
|---|---|---|
| `/bin` | Symlink → `/usr/bin` | Symlink → `/usr/bin` |
| `/sbin` | Symlink → `/usr/sbin` | Symlink → `/usr/sbin` |
| `/lib` | Symlink → `/usr/lib` | Symlink → `/usr/lib` |
| `/lib64` | Symlink → `/usr/lib64` (si existe) | Symlink → `/usr/lib64` |
| `/var/run` | Symlink → `/run` | Symlink → `/run` |
| Bibliotecas 64-bit | `/usr/lib/x86_64-linux-gnu/` (multiarch) | `/usr/lib64/` |
| Config de paquetes | `/etc/apt/` | `/etc/yum.repos.d/` |
| Web root por defecto | `/var/www/html` | `/var/www/html` |

Ambas familias convergen gracias al `/usr merge` (T02). La diferencia principal
en la organización de bibliotecas es que Debian usa **multiarch** (rutas con
tripleta como `x86_64-linux-gnu`) que permite instalar bibliotecas de múltiples
arquitecturas simultáneamente, mientras AlmaLinux usa `/lib64/` directamente.

---

## Comando útil: `tree`

```bash
# Ver la estructura de primer nivel
tree -L 1 /

# Ver la estructura de /usr con 2 niveles
tree -L 2 /usr

# Solo directorios, sin archivos
tree -d -L 2 /
```

---

## Ejercicios

Todos los comandos se ejecutan dentro de los contenedores del curso.

### Ejercicio 1 — Explorar la raíz y sus symlinks

Examina la estructura de primer nivel e identifica cuáles directorios son
symlinks del `/usr merge`.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Estructura raíz ==="
ls -la /
echo ""
echo "=== Symlinks del /usr merge ==="
for d in /bin /sbin /lib /lib64; do
    if [ -L "$d" ]; then
        echo "$d -> $(readlink "$d") (symlink)"
    else
        echo "$d (directorio real)"
    fi
done
'
```

<details>
<summary>Predicción</summary>

`ls -la /` muestra todos los directorios de primer nivel. Los que son symlinks
aparecen con `->` indicando su destino.

En Debian 12 (bookworm) con `/usr merge` activo:
- `/bin` → `usr/bin` (symlink)
- `/sbin` → `usr/sbin` (symlink)
- `/lib` → `usr/lib` (symlink)
- `/lib64` → `usr/lib64` (symlink, si existe)

Los demás directorios (`/etc`, `/home`, `/opt`, `/root`, `/tmp`, `/usr`,
`/var`, etc.) son directorios reales.

El loop usa `readlink` para mostrar el destino de cada symlink. `-L` en el
test verifica si la ruta es un symlink (a diferencia de `-d` que verificaría
si es un directorio, lo cual también sería verdadero para un symlink a
directorio).

</details>

---

### Ejercicio 2 — Medir el tamaño de cada directorio raíz

Identifica qué directorios ocupan más espacio.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Tamaño de cada directorio raíz ==="
du -sh /* 2>/dev/null | sort -rh | head -15
'
```

<details>
<summary>Predicción</summary>

`/usr` es con diferencia el directorio más grande (probablemente >500 MB)
porque contiene todo el software instalado: gcc, gdb, valgrind, rustc, etc.

Los directorios virtuales (`/proc`, `/sys`) aparecen con 0 porque `du` no
puede medir su contenido real (es generado dinámicamente por el kernel). `/dev`
aparece con un tamaño mínimo.

`2>/dev/null` suprime errores de permisos (algunos directorios no son legibles).
`sort -rh` ordena por tamaño descendente en formato "human-readable" (entiende
sufijos como M, G). `head -15` limita la salida a los 15 más grandes.

En un contenedor, `/home` es pequeño (o vacío) porque no hay usuarios regulares.
`/var` tiene un tamaño moderado (logs, cache de paquetes).

</details>

---

### Ejercicio 3 — Localizar binarios, headers y man pages

Aprende a encontrar dónde vive el software instalado en el filesystem.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Localizar binarios ==="
echo "gcc:  $(which gcc)"
echo "make: $(which make)"
echo "bash: $(which bash)"
echo "rustc: $(which rustc)"

echo ""
echo "=== Headers de C ==="
ls /usr/include/stdio.h
echo "Total headers: $(ls /usr/include/ | wc -l) archivos en /usr/include/"

echo ""
echo "=== Man pages ==="
echo "gcc man page: $(man -w gcc 2>/dev/null || echo "no encontrada")"
echo "printf man page: $(man -w printf 2>/dev/null || echo "no encontrada")"
ls /usr/share/man/ | head -5
'
```

<details>
<summary>Predicción</summary>

Los binarios aparecen en `/usr/bin/`:
- `gcc` → `/usr/bin/gcc`
- `make` → `/usr/bin/make`
- `bash` → `/usr/bin/bash`
- `rustc` → depende de cómo se instaló Rust (puede estar en `/usr/local/bin/`
  si se instaló con `rustup`, o en `~/.cargo/bin/` si fue instalación local)

Los headers de C estándar están en `/usr/include/`. `stdio.h` es uno de los
fundamentales. El número total de archivos puede ser considerable (cientos)
si los paquetes de desarrollo (`-dev`) están instalados.

Las man pages están en `/usr/share/man/`, organizadas en secciones:
- `man1/` → comandos de usuario
- `man2/` → llamadas al sistema (syscalls)
- `man3/` → funciones de biblioteca C
- `man5/` → formatos de archivos
- `man8/` → comandos de administración

</details>

---

### Ejercicio 4 — Examinar `/usr/local/`

Verifica el directorio de software instalado manualmente y su separación
del software del sistema.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Estructura de /usr/local/ ==="
tree -L 1 /usr/local/ 2>/dev/null || ls /usr/local/

echo ""
echo "=== Contenido de /usr/local/bin/ ==="
ls /usr/local/bin/ 2>/dev/null || echo "(vacío)"

echo ""
echo "=== Comparación: gcc del sistema vs local ==="
echo "Sistema: $(which gcc)"
echo "Local:   $(ls /usr/local/bin/gcc 2>/dev/null || echo "no existe")"
'
```

<details>
<summary>Predicción</summary>

`/usr/local/` tiene la misma estructura que `/usr/` (`bin/`, `lib/`, `include/`,
`share/`), pero la mayoría de subdirectorios estarán vacíos en un contenedor
de desarrollo porque todo se instaló con el gestor de paquetes.

Si Rust se instaló con `rustup` al directorio del sistema, podría haber
binarios en `/usr/local/bin/`. Si no, estará vacío.

La separación `/usr/bin` vs `/usr/local/bin` es importante: el gestor de
paquetes **nunca** toca `/usr/local/`. Esto permite instalar una versión
diferente de un programa sin conflictos:
- `apt install gcc` → `/usr/bin/gcc`
- Compilar gcc desde fuente → `/usr/local/bin/gcc`
- `which gcc` devuelve `/usr/local/bin/gcc` si `$PATH` lo antepone

</details>

---

### Ejercicio 5 — Investigar `/tmp`: sticky bit y filesystem

Examina los permisos especiales de `/tmp` y su tipo de filesystem.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Permisos de /tmp ==="
ls -ld /tmp
stat -c "Octal: %a  Texto: %A" /tmp

echo ""
echo "=== Filesystem de /tmp ==="
df -h /tmp
mount | grep /tmp || echo "/tmp no es un mount separado en este contenedor"

echo ""
echo "=== Comparación /tmp vs /var/tmp ==="
echo "/tmp:     $(stat -c "%a %A" /tmp)"
echo "/var/tmp: $(stat -c "%a %A" /var/tmp)"
'
```

<details>
<summary>Predicción</summary>

Los permisos de `/tmp` son `1777` (`drwxrwxrwt`):
- `777` = lectura+escritura+ejecución para todos
- `1` (sticky bit) = solo el dueño puede borrar sus archivos (la `t` al final)

Sin el sticky bit, cualquier usuario podría borrar archivos de otros usuarios
en `/tmp`. El sticky bit lo impide.

Dentro de un contenedor, `/tmp` generalmente NO es un mount separado — forma
parte del filesystem overlay del contenedor. En el **host**, `/tmp` suele ser
un tmpfs (montado en RAM). `mount | grep /tmp` no encontrará nada en el
contenedor, de ahí el mensaje "no es un mount separado".

`/var/tmp` también tiene `1777` pero su política de limpieza es diferente:
systemd-tmpfiles limpia `/tmp` cada ~10 días y `/var/tmp` cada ~30 días.

</details>

---

### Ejercicio 6 — Crear y verificar archivos temporales

Prueba el comportamiento del sticky bit creando archivos como diferentes
contextos.

```bash
docker compose exec -T debian-dev bash -c '
# Crear un archivo temporal
echo "test data" > /tmp/mytest.txt
ls -la /tmp/mytest.txt

# Verificar que se puede leer
cat /tmp/mytest.txt

# Verificar que el sticky bit funciona
echo ""
echo "=== Sticky bit en acción ==="
echo "Dueño del archivo: $(stat -c %U /tmp/mytest.txt)"
echo "Permisos del directorio: $(stat -c %A /tmp)"

# Limpiar
rm /tmp/mytest.txt
echo "Archivo eliminado correctamente"
'
```

<details>
<summary>Predicción</summary>

El archivo se crea exitosamente. `ls -la` muestra que el dueño es `root` (en
la mayoría de contenedores Docker, `exec` ejecuta como root por defecto).

El sticky bit se observa en el permiso del **directorio** `/tmp` (la `t` en
`drwxrwxrwt`), no en el archivo individual. Su efecto: un usuario puede crear
archivos en `/tmp` porque los permisos del directorio lo permiten (`777`), pero
solo puede borrar archivos de los que es dueño.

En este ejercicio, como ejecutamos como root, podemos borrar cualquier archivo.
La verdadera prueba del sticky bit requiere dos usuarios diferentes, lo cual es
difícil en un contenedor single-user.

`rm` funciona porque somos el dueño del archivo (y además root puede borrar
cualquier cosa).

</details>

---

### Ejercicio 7 — Comparar estructura raíz entre distros

Verifica que ambas distribuciones siguen el FHS con diferencias menores.

```bash
echo "=== Debian: symlinks del /usr merge ==="
docker compose exec -T debian-dev bash -c '
for d in /bin /sbin /lib /lib64; do
    if [ -L "$d" ]; then
        echo "  $d -> $(readlink "$d")"
    else
        [ -d "$d" ] && echo "  $d (directorio real)" || echo "  $d (no existe)"
    fi
done
'

echo ""
echo "=== AlmaLinux: symlinks del /usr merge ==="
docker compose exec -T alma-dev bash -c '
for d in /bin /sbin /lib /lib64; do
    if [ -L "$d" ]; then
        echo "  $d -> $(readlink "$d")"
    else
        [ -d "$d" ] && echo "  $d (directorio real)" || echo "  $d (no existe)"
    fi
done
'
```

<details>
<summary>Predicción</summary>

Ambas distribuciones muestran los mismos symlinks del `/usr merge`:
- `/bin` → `usr/bin`
- `/sbin` → `usr/sbin`
- `/lib` → `usr/lib`
- `/lib64` → `usr/lib64`

La convergencia es casi total. RHEL/AlmaLinux adoptó `/usr merge` desde RHEL 7
(2014). Debian lo adoptó más tarde, completándolo en Debian 12 bookworm (2023).

El resultado práctico: ya no importa si un binario está "en `/bin`" o "en
`/usr/bin`" — son el mismo directorio. El FHS original separaba binarios
esenciales (`/bin`) de los no esenciales (`/usr/bin`) para sistemas donde
`/usr` pudiera estar en una partición separada. Con `/usr merge`, esta
distinción es puramente histórica.

</details>

---

### Ejercicio 8 — Comparar la organización de bibliotecas

Observa la diferencia entre multiarch (Debian) y lib64 (AlmaLinux).

```bash
echo "=== Debian: multiarch ==="
docker compose exec -T debian-dev bash -c '
echo "Ruta de bibliotecas 64-bit:"
ls -d /usr/lib/x86_64-linux-gnu/ 2>/dev/null && echo "  /usr/lib/x86_64-linux-gnu/ (multiarch)"
echo ""
echo "Algunas bibliotecas:"
ls /usr/lib/x86_64-linux-gnu/lib*.so* 2>/dev/null | head -5
'

echo ""
echo "=== AlmaLinux: lib64 ==="
docker compose exec -T alma-dev bash -c '
echo "Ruta de bibliotecas 64-bit:"
ls -d /usr/lib64/ 2>/dev/null && echo "  /usr/lib64/"
echo ""
echo "Algunas bibliotecas:"
ls /usr/lib64/lib*.so* 2>/dev/null | head -5
'
```

<details>
<summary>Predicción</summary>

**Debian** usa el esquema **multiarch**: las bibliotecas de 64 bits están en
`/usr/lib/x86_64-linux-gnu/`. La tripleta (`x86_64-linux-gnu`) identifica la
arquitectura. Esto permite instalar bibliotecas de múltiples arquitecturas
simultáneamente (por ejemplo, `i386` y `amd64` para cross-compilation o
ejecutar software de 32 bits).

**AlmaLinux** usa el esquema tradicional de RHEL: `/usr/lib64/` para 64 bits
y `/usr/lib/` para 32 bits (si están instaladas). Es más simple pero no
soporta múltiples arquitecturas tan limpiamente.

Las bibliotecas que aparecen (`libc.so`, `libm.so`, `libpthread.so`, etc.) son
las mismas en ambas — solo cambia la ruta. `gcc` y `ld` saben dónde buscarlas
en cada distribución.

Esto es relevante para compilación: si compilas un programa con `-l<nombre>`,
el linker busca en la ruta correspondiente de la distribución.

</details>

---

### Ejercicio 9 — Examinar directorios de configuración por distro

Identifica las diferencias en `/etc` entre las dos distribuciones.

```bash
echo "=== Debian: gestión de paquetes ==="
docker compose exec -T debian-dev bash -c '
echo "Config de APT:"
ls /etc/apt/ 2>/dev/null
echo ""
echo "Repositorios:"
cat /etc/apt/sources.list 2>/dev/null | grep -v "^#" | grep -v "^$" | head -5
'

echo ""
echo "=== AlmaLinux: gestión de paquetes ==="
docker compose exec -T alma-dev bash -c '
echo "Repos de DNF/YUM:"
ls /etc/yum.repos.d/ 2>/dev/null
echo ""
echo "Primer repo:"
head -5 /etc/yum.repos.d/*.repo 2>/dev/null | head -10
'
```

<details>
<summary>Predicción</summary>

**Debian** configura sus repositorios en `/etc/apt/`:
- `sources.list` → archivo principal con las URLs de los repositorios
- `sources.list.d/` → directorio para repos adicionales (uno por archivo)

**AlmaLinux** configura sus repositorios en `/etc/yum.repos.d/`:
- Cada archivo `.repo` define uno o más repositorios
- Archivos como `almalinux.repo`, `almalinux-baseos.repo`, etc.

La estructura de `/etc` es donde más se notan las diferencias entre familias
de distribuciones: mismos conceptos (repositorios, autenticación, red) pero
diferentes archivos y formatos de configuración.

Archivos comunes a ambas: `/etc/passwd`, `/etc/group`, `/etc/hostname`,
`/etc/hosts`, `/etc/shadow`, `/etc/resolv.conf`. Estos son estándar del FHS.

</details>

---

### Ejercicio 10 — Mapa completo del filesystem

Genera un resumen de la estructura del filesystem con el propósito de cada
directorio.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Mapa del Filesystem (Debian) ==="
echo ""
for d in / /bin /boot /dev /etc /home /lib /media /mnt /opt /proc /root /run /sbin /srv /sys /tmp /usr /var; do
    if [ -L "$d" ]; then
        target=$(readlink "$d")
        printf "%-10s -> %-20s (symlink)\n" "$d" "$target"
    elif [ -d "$d" ]; then
        size=$(du -sh "$d" 2>/dev/null | cut -f1)
        printf "%-10s %-8s (directorio)\n" "$d" "$size"
    fi
done
echo ""
echo "=== Subdirectorios clave de /usr ==="
tree -L 1 /usr 2>/dev/null || ls /usr
echo ""
echo "=== Subdirectorios clave de /var ==="
tree -L 1 /var 2>/dev/null || ls /var
'
```

<details>
<summary>Predicción</summary>

El mapa muestra cada directorio raíz con su tamaño (si es directorio real) o
su destino (si es symlink). Los patrones esperados:

- **Más grandes**: `/usr` (software instalado), `/var` (logs, cache)
- **Symlinks**: `/bin`, `/sbin`, `/lib` (todos apuntan a `/usr/...`)
- **Tamaño 0 o mínimo**: `/proc`, `/sys`, `/dev` (filesystems virtuales; `du`
  no puede medir su contenido real)
- **Vacíos o casi**: `/media`, `/mnt`, `/srv`, `/opt` (en un contenedor)

El loop usa `printf` con formato fijo para alinear las columnas. `du -sh`
muestra el tamaño en formato "human-readable". `cut -f1` extrae solo el tamaño
(descarta la ruta que `du` imprime después del tab).

`tree -L 1` muestra la estructura de primer nivel. Para `/usr`, se ven los
subdirectorios clave (`bin/`, `lib/`, `include/`, `share/`, `local/`). Para
`/var`, se ven `log/`, `cache/`, `lib/`, `spool/`, `tmp/`.

</details>

---

## Resumen de conceptos

| Concepto | Detalle clave |
|---|---|
| FHS | Estructura estándar de directorios Linux (v3.0, 2015) |
| `/` | Raíz del árbol; todo se monta aquí |
| `/usr` | Software del sistema (el más grande) |
| `/usr/local` | Software instalado manualmente; nunca tocado por apt/dnf |
| `/etc` | Configuración específica del host |
| `/var` | Datos variables (logs, cache, estado) |
| `/tmp` | Temporales con sticky bit; puede ser tmpfs |
| `/usr merge` | `/bin`, `/sbin`, `/lib` son symlinks a `/usr/...` |
| Multiarch vs lib64 | Debian: `/usr/lib/x86_64-linux-gnu/`; AlmaLinux: `/usr/lib64/` |
| `/run` | Datos de runtime volátiles (tmpfs); `/var/run` → `/run` |
