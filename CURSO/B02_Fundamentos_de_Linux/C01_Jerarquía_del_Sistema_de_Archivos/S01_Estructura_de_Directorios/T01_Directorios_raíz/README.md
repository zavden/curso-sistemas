# T01 — Directorios raíz

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

Contiene comandos fundamentales necesarios para arrancar el sistema y operar en
modo single-user (sin red, sin /usr montado):

```bash
ls /bin/
# bash, cat, cp, echo, grep, ls, mkdir, mount, mv, rm, sh, ...
```

Estos son los comandos que deben estar disponibles incluso si `/usr` está en
una partición separada que aún no se montó.

> **Nota**: En distribuciones con `/usr merge` (ver T02), `/bin` es un symlink
> a `/usr/bin`. El concepto es el mismo pero la separación física desapareció.

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

### `/usr` — Programas y recursos del sistema

El directorio más grande. Contiene la mayoría del software instalado:

```
/usr/
├── bin/       ← Binarios de usuario (gcc, python, vim, git...)
├── sbin/      ← Binarios de administración adicionales
├── lib/       ← Bibliotecas compartidas
├── lib64/     ← Bibliotecas de 64 bits (en algunas distros)
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
├── run/     ← Datos de runtime (PIDs, sockets) — en muchas distros es symlink a /run
└── mail/    ← Buzones de correo local
```

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

mount | grep /tmp
# tmpfs on /tmp type tmpfs (rw,nosuid,nodev,size=2097152k)
```

Esto significa que los archivos en `/tmp` no se escriben a disco — son más
rápidos pero se pierden al reiniciar y consumen RAM.

### `/home` — Directorios personales

Cada usuario regular tiene su directorio en `/home/<usuario>`:

```bash
ls -la /home/
# drwxr-x--- 5 alice alice 4096 ... alice
# drwxr-x--- 3 bob   bob   4096 ... bob
# drwxr-x--- 8 dev   dev   4096 ... dev
```

Contiene archivos personales, configuraciones de aplicaciones (dotfiles),
shells, etc. En servidores, puede estar en una partición separada o montado
desde NFS/NAS.

Los dotfiles (`.bashrc`, `.ssh/`, `.config/`) configuran el entorno del
usuario:

```bash
ls -a ~
# .  ..  .bash_history  .bashrc  .config  .local  .profile  .ssh  Documents  ...
```

### `/root` — Home de root

El directorio personal del usuario root. No está en `/home/root` sino en
`/root` para que esté disponible incluso si `/home` está en una partición
separada que no se pudo montar.

```bash
ls -la /root/
# -rw------- 1 root root  500 ... .bash_history
# -rw-r--r-- 1 root root  571 ... .bashrc
```

Los permisos son `700` — solo root puede acceder.

### `/opt` — Software de terceros

Software autocontenido que no sigue la convención `/usr/bin` + `/usr/lib`:

```bash
ls /opt/
# google/  containerd/  some-enterprise-app/
```

Cada aplicación tiene su propio subdirectorio con todo lo que necesita:

```
/opt/some-app/
├── bin/
├── lib/
├── etc/
└── share/
```

Usado típicamente por software comercial, IDEs, y aplicaciones que se
distribuyen como "bundles" (Chrome, VS Code, etc.).

### `/boot` — Arranque del sistema

Contiene todo lo necesario para arrancar: kernel, initramfs, configuración
del bootloader:

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

- `/media/` — montaje automático de medios removibles (USB, CD):
  ```bash
  ls /media/alice/
  # USB-DRIVE/  CDROM/
  ```

- `/mnt/` — montaje temporal manual por el administrador:
  ```bash
  sudo mount /dev/sdb1 /mnt
  ls /mnt/
  ```

La convención es: `/media` para automount, `/mnt` para montajes manuales
temporales.

### `/srv` — Datos de servicios

Datos servidos por el sistema (web, FTP, repositorios):

```bash
ls /srv/
# www/   ← Archivos web (alternativa a /var/www)
# ftp/   ← Archivos FTP
```

En la práctica, muchas distribuciones dejan `/srv` vacío y usan `/var/www`
para web. El FHS lo define pero su uso es inconsistente.

### `/run` — Datos de runtime

Datos volátiles de runtime (creados después del arranque):

```bash
ls /run/
# docker.sock  systemd/  user/  lock/  sshd.pid  ...
```

Es un **tmpfs** — se borra al reiniciar. Contiene PIDs, sockets, locks y
otros archivos que solo tienen sentido mientras el sistema está corriendo.

En distribuciones modernas, `/var/run` es un symlink a `/run`.

## Diferencias entre Debian y RHEL

| Directorio | Debian/Ubuntu | AlmaLinux/RHEL |
|---|---|---|
| `/bin` | Symlink a `/usr/bin` (Debian 12+) | Symlink a `/usr/bin` |
| `/sbin` | Symlink a `/usr/sbin` (Debian 12+) | Symlink a `/usr/sbin` |
| `/lib` | Symlink a `/usr/lib` (Debian 12+) | Symlink a `/usr/lib64` |
| `/var/run` | Symlink a `/run` | Symlink a `/run` |
| `/srv` | Existe, vacío | Existe, vacío |
| Web root por defecto | `/var/www/html` | `/var/www/html` |

Ambas familias convergen cada vez más gracias al `/usr merge` (T02).

## Comando útil: `tree`

```bash
# Ver la estructura de primer nivel
tree -L 1 /

# Ver la estructura de /usr con 2 niveles de profundidad
tree -L 2 /usr

# Solo directorios, sin archivos
tree -d -L 2 /
```

---

## Ejercicios

### Ejercicio 1 — Explorar la raíz

```bash
# Listar todos los directorios de primer nivel
ls -la /

# Identificar cuáles son symlinks (en distros con /usr merge)
ls -la /bin /sbin /lib

# Verificar el tamaño de cada directorio de primer nivel
du -sh /* 2>/dev/null | sort -rh | head -15
```

### Ejercicio 2 — Localizar software

```bash
# ¿Dónde está gcc?
which gcc
# ¿En /usr/bin o en /usr/local/bin?

# ¿Dónde están sus headers?
ls /usr/include/stdio.h

# ¿Dónde están sus man pages?
man -w gcc
```

### Ejercicio 3 — Verificar /tmp

```bash
# ¿Es tmpfs o está en disco?
df -h /tmp
mount | grep /tmp

# Verificar sticky bit
ls -ld /tmp
stat /tmp

# Crear un archivo como usuario regular
echo "test" > /tmp/mytest.txt
ls -la /tmp/mytest.txt
```
