# T02 — useradd, usermod, userdel

## Las herramientas de gestión

Linux tiene dos niveles de herramientas para gestionar usuarios:

| Herramienta | Tipo | Disponible en |
|---|---|---|
| `useradd`, `usermod`, `userdel` | Bajo nivel (POSIX) | Todas las distros |
| `adduser`, `deluser` | Alto nivel (wrappers interactivos) | Debian/Ubuntu |

En RHEL/AlmaLinux, `adduser` es simplemente un symlink a `useradd`:

```bash
# Debian
file /usr/sbin/adduser
# /usr/sbin/adduser: Perl script

# RHEL
file /usr/sbin/adduser
# /usr/sbin/adduser: symbolic link to useradd
```

## useradd — Crear usuarios

### Uso básico

```bash
# Forma mínima — crea el usuario pero NO el home ni la contraseña
sudo useradd testuser

# Forma completa recomendada
sudo useradd -m -s /bin/bash -c "Test User" testuser
```

### Flags importantes

| Flag | Significado | Ejemplo |
|---|---|---|
| `-m` | Crear directorio home | `useradd -m user` |
| `-M` | NO crear directorio home | `useradd -M serviceuser` |
| `-d /path` | Especificar directorio home | `useradd -d /srv/app appuser` |
| `-s /shell` | Shell de login | `useradd -s /bin/bash user` |
| `-c "texto"` | Comentario (campo GECOS) | `useradd -c "App Service" appuser` |
| `-u 1500` | UID específico | `useradd -u 1500 user` |
| `-g grupo` | Grupo primario | `useradd -g developers user` |
| `-G g1,g2` | Grupos suplementarios | `useradd -G sudo,docker user` |
| `-e YYYY-MM-DD` | Fecha de expiración de cuenta | `useradd -e 2025-12-31 temp` |
| `-r` | Usuario de sistema (UID bajo, sin home) | `useradd -r -s /usr/sbin/nologin svc` |
| `-k /skel` | Directorio skeleton alternativo | `useradd -m -k /etc/skel-dev user` |

### Comportamiento por defecto

`useradd` sin flags usa los defaults definidos en dos archivos:

```bash
# Ver defaults actuales
useradd -D
# GROUP=100
# HOME=/home
# INACTIVE=-1
# EXPIRE=
# SHELL=/bin/sh         ← Debian: /bin/sh
# SKEL=/etc/skel
# CREATE_MAIL_SPOOL=no

# Archivo de defaults
cat /etc/default/useradd
```

```bash
# Configuración avanzada de defaults
cat /etc/login.defs | grep -E '^[A-Z]' | head -20
```

`/etc/login.defs` controla comportamientos globales:

| Parámetro | Descripción | Debian | RHEL |
|---|---|---|---|
| `UID_MIN` | UID mínimo para usuarios regulares | 1000 | 1000 |
| `UID_MAX` | UID máximo | 60000 | 60000 |
| `SYS_UID_MIN` | UID mínimo para usuarios de sistema | 100 | 201 |
| `SYS_UID_MAX` | UID máximo para usuarios de sistema | 999 | 999 |
| `CREATE_HOME` | Crear home por defecto | no | yes |
| `USERGROUPS_ENAB` | Crear grupo con el mismo nombre | yes | yes |
| `ENCRYPT_METHOD` | Algoritmo de hash | YESCRYPT | SHA512 |
| `UMASK` | umask por defecto para homes | 022 | 022 |

Diferencia crucial: en Debian, `useradd` sin `-m` **no crea el home**. En
RHEL, sí lo crea por defecto (porque `CREATE_HOME yes` en `login.defs`).

### El directorio skeleton

Cuando se crea un home, su contenido inicial se copia de `/etc/skel/`:

```bash
ls -la /etc/skel/
# .bash_logout
# .bashrc
# .profile      [Debian]
# .bash_profile  [RHEL]
```

Para personalizar el entorno inicial de nuevos usuarios, agregar archivos a
`/etc/skel/`:

```bash
# Agregar un .vimrc para todos los usuarios nuevos
sudo cp /path/to/.vimrc /etc/skel/.vimrc

# Los usuarios existentes NO se ven afectados — solo los nuevos
```

## adduser (Debian) — Wrapper interactivo

En Debian/Ubuntu, `adduser` es un script Perl que envuelve `useradd` con una
interfaz interactiva y más amigable:

```bash
sudo adduser newuser
# Adding user `newuser' ...
# Adding new group `newuser' (1001) ...
# Adding new user `newuser' (1001) with group `newuser' ...
# Creating home directory `/home/newuser' ...
# Copying files from `/etc/skel' ...
# New password:
# Retype new password:
# passwd: password updated successfully
# Changing the user information for newuser
# Enter the new value, or press ENTER for the default
#     Full Name []: Juan García
#     Room Number []:
#     Work Phone []:
#     Home Phone []:
#     Other []:
# Is the information correct? [Y/n]
```

Diferencias con `useradd`:

| Aspecto | `useradd` | `adduser` (Debian) |
|---|---|---|
| Crea home | Solo con `-m` | Siempre |
| Copia `/etc/skel` | Solo con `-m` | Siempre |
| Pide contraseña | No | Sí |
| Pide info GECOS | No | Sí |
| Configurable en | `/etc/default/useradd` | `/etc/adduser.conf` |

Para scripts automatizados, usar siempre `useradd` — es predecible, no
interactivo, y funciona igual en todas las distribuciones.

## usermod — Modificar usuarios

### Flags principales

```bash
# Cambiar shell
sudo usermod -s /bin/zsh dev

# Cambiar home (no mueve los archivos)
sudo usermod -d /new/home dev

# Cambiar home Y mover los archivos
sudo usermod -d /new/home -m dev

# Cambiar nombre de login
sudo usermod -l newname oldname

# Cambiar UID (actualiza archivos en home, NO en otros directorios)
sudo usermod -u 2000 dev

# Cambiar grupo primario
sudo usermod -g newgroup dev

# Agregar a grupos suplementarios (MANTENER los existentes)
sudo usermod -aG docker dev
sudo usermod -aG sudo,docker dev

# Reemplazar grupos suplementarios (QUITA los anteriores)
sudo usermod -G docker dev    # ¡Cuidado! Solo queda en docker

# Bloquear cuenta (antepone ! al hash en /etc/shadow)
sudo usermod -L dev

# Desbloquear cuenta
sudo usermod -U dev

# Establecer fecha de expiración
sudo usermod -e 2025-12-31 dev

# Quitar fecha de expiración
sudo usermod -e "" dev
```

### El error más común: -G sin -a

```bash
# INCORRECTO — reemplaza todos los grupos suplementarios con solo "docker"
sudo usermod -G docker dev
# dev pierde acceso a sudo, y cualquier otro grupo que tuviera

# CORRECTO — agrega docker sin quitar los grupos existentes
sudo usermod -aG docker dev
```

Este es uno de los errores más frecuentes en administración de Linux. Siempre
usar `-aG` (append + Groups) para agregar a un grupo sin quitar los existentes.

### Efecto de los cambios

Los cambios en grupos **no se aplican a sesiones activas**. El usuario debe
cerrar sesión y volver a entrar para que los nuevos grupos se activen:

```bash
# Agregar a docker
sudo usermod -aG docker dev

# Verificar (muestra los grupos de la sesión actual — NO incluye docker aún)
groups
# dev sudo

# Opción 1: cerrar sesión y volver a entrar
# Opción 2: usar newgrp para la sesión actual
newgrp docker
groups
# docker dev sudo
```

## userdel — Eliminar usuarios

```bash
# Eliminar usuario (mantiene su home y archivos)
sudo userdel testuser

# Eliminar usuario Y su directorio home
sudo userdel -r testuser

# Forzar eliminación aunque el usuario tenga procesos activos
sudo userdel -f testuser
```

### Qué no limpia userdel

`userdel -r` elimina el home y el mail spool, pero **no busca archivos del
usuario en otros directorios**:

```bash
# Después de eliminar un usuario, buscar archivos huérfanos
sudo find / -nouser -ls 2>/dev/null
# (muestra archivos que pertenecen a un UID que ya no existe)

# Buscar crontabs del usuario eliminado
ls /var/spool/cron/crontabs/testuser 2>/dev/null    # Debian
ls /var/spool/cron/testuser 2>/dev/null              # RHEL
```

En Debian, `deluser` (wrapper de `userdel`) tiene opciones adicionales:

```bash
# Eliminar usuario de un grupo (no elimina el usuario)
sudo deluser dev docker
# Removing user `dev' from group `docker' ...

# Esto equivale a:
sudo gpasswd -d dev docker
```

## Operaciones comunes

### Crear un usuario para desarrollo

```bash
# Debian
sudo useradd -m -s /bin/bash -c "Developer" -G sudo,docker devuser
sudo passwd devuser

# RHEL
sudo useradd -m -s /bin/bash -c "Developer" -G wheel,docker devuser
sudo passwd devuser
```

### Crear un usuario de servicio

```bash
sudo useradd -r -s /usr/sbin/nologin -d /var/lib/myapp -M myapp
# -r: UID de sistema (bajo)
# -s /usr/sbin/nologin: sin shell interactiva
# -M: no crear home (o crear manualmente el directorio de datos)
```

### Bloquear un usuario temporalmente

```bash
# Bloquear (no puede hacer login, pero SSH con keys puede seguir funcionando)
sudo usermod -L dev

# Bloquear completamente (cambiar shell a nologin)
sudo usermod -L -s /usr/sbin/nologin dev

# Restaurar
sudo usermod -U -s /bin/bash dev
```

### Cambiar la shell de login

```bash
# Como root
sudo usermod -s /bin/zsh dev

# Como el propio usuario (sin sudo)
chsh -s /bin/zsh

# Ver shells válidas
cat /etc/shells
# /bin/sh
# /bin/bash
# /usr/bin/bash
# /bin/zsh
# /usr/bin/zsh
```

`chsh` solo permite shells listadas en `/etc/shells`.

---

## Ejercicios

### Ejercicio 1 — Crear y configurar un usuario

```bash
# Crear un usuario de prueba con home y bash
sudo useradd -m -s /bin/bash -c "Test User" testlab

# Verificar que se creó correctamente
getent passwd testlab
id testlab
ls -la /home/testlab/

# Asignar contraseña
sudo passwd testlab

# Verificar en /etc/shadow
sudo grep '^testlab:' /etc/shadow
```

### Ejercicio 2 — Modificar membresías de grupo

```bash
# Agregar a un grupo sin perder los existentes
sudo usermod -aG docker testlab

# Verificar
id testlab

# ¿Qué pasa si usas -G sin -a?
# (no ejecutar si no quieres perder grupos — solo predecir)
```

### Ejercicio 3 — Limpiar

```bash
# Eliminar el usuario y su home
sudo userdel -r testlab

# Verificar que no quedan archivos huérfanos
sudo find /tmp -user testlab 2>/dev/null
```
