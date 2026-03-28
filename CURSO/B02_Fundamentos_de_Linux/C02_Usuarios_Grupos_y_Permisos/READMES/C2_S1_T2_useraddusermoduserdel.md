# T02 — useradd, usermod, userdel

## Las herramientas de gestión

Linux tiene dos niveles de herramientas para gestionar usuarios:

| Herramienta | Tipo | Disponible en |
|---|---|---|
| `useradd`, `usermod`, `userdel` | Bajo nivel (paquete `shadow-utils`) | Todas las distros |
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

---

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
| `-N` | NO crear grupo con el mismo nombre | `useradd -N user` |

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

---

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

---

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

---

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

# Equivalente portable (funciona en ambas familias):
sudo gpasswd -d dev docker
```

---

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

> **Nota importante**: `usermod -L` solo bloquea la autenticación por contraseña
> (antepone `!` al hash). SSH con claves públicas **sigue funcionando** porque
> no usa el hash de `/etc/shadow`. Para un bloqueo completo, cambiar la shell a
> nologin o usar `chage -E 0` para expirar la cuenta.

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

## Tabla resumen de operaciones

| Operación | useradd | usermod | userdel |
|---|---|---|---|
| Crear | `useradd -m user` | — | — |
| Modificar shell | `-s` | `-s` | — |
| Modificar home | `-d` | `-d -m` | — |
| Agregar grupo suplementario | `-G g1,g2` | `-aG g` | — |
| Cambiar grupo primario | `-g` | `-g` | — |
| Bloquear | — | `-L` | — |
| Desbloquear | — | `-U` | — |
| Eliminar | — | — | `userdel` / `-r` (con home) |

> **Nota**: `usermod` **no tiene** flag `-r`. El flag `-r` es exclusivo de
> `useradd` (crea usuario de sistema). No confundir los flags entre herramientas.

---

## Lab — useradd, usermod, userdel

### Objetivo

Crear, modificar y eliminar usuarios con las herramientas de bajo
nivel. Explorar los defaults de useradd, el directorio skeleton, y
practicar las operaciones más comunes incluyendo el error clásico
de `-G` sin `-a`.

### Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

### Parte 1 — useradd

#### Paso 1.1: Defaults de useradd

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c '
useradd -D
echo ""
echo "--- /etc/login.defs (extracto) ---"
grep -E "^(UID_MIN|UID_MAX|SYS_UID|CREATE_HOME|USERGROUPS|ENCRYPT_METHOD|UMASK)" /etc/login.defs
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c '
useradd -D
echo ""
grep -E "^(UID_MIN|UID_MAX|SYS_UID|CREATE_HOME|USERGROUPS|ENCRYPT_METHOD|UMASK)" /etc/login.defs
'
```

Diferencia crucial: en Debian, `CREATE_HOME` es `no` — sin `-m` no
se crea el home. En AlmaLinux, `CREATE_HOME` es `yes`.

#### Paso 1.2: Crear usuario mínimo

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear usuario sin flags ==="
useradd testmin

echo ""
echo "--- Entrada en /etc/passwd ---"
getent passwd testmin

echo ""
echo "--- Home existe? ---"
ls -ld /home/testmin 2>/dev/null || echo "NO existe (Debian no crea home sin -m)"

echo ""
echo "--- Limpiar ---"
userdel testmin
'
```

En Debian, `useradd` sin `-m` no crea el home. Siempre usar `-m`
explícitamente para portabilidad.

#### Paso 1.3: Crear usuario completo

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear usuario con todas las opciones ==="
useradd -m -s /bin/bash -c "Lab User" -u 2001 labuser

echo ""
echo "--- Verificar ---"
getent passwd labuser
id labuser
ls -la /home/labuser/

echo ""
echo "--- Contenido del home (copiado de /etc/skel) ---"
ls -la /home/labuser/
'
```

El contenido del home se copia de `/etc/skel/`. Contiene los archivos
de perfil iniciales (.bashrc, .profile, etc.).

#### Paso 1.4: El directorio skeleton

```bash
docker compose exec debian-dev bash -c '
echo "=== /etc/skel ==="
ls -la /etc/skel/

echo ""
echo "=== Comparar con el home creado ==="
diff <(ls -a /etc/skel/) <(ls -a /home/labuser/) || true
'
```

Agregar archivos a `/etc/skel/` personaliza el entorno inicial de
todos los usuarios nuevos. Los usuarios existentes no se ven afectados.

#### Paso 1.5: Crear usuario con grupos suplementarios

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear usuario con grupos suplementarios ==="
groupadd -f labgroup
useradd -m -s /bin/bash -G labgroup labuser2

echo ""
echo "--- Verificar ---"
id labuser2

echo ""
echo "--- En /etc/group ---"
getent group labgroup
echo "(labuser2 aparece como miembro suplementario)"
'
```

El flag `-G` asigna grupos suplementarios al crear el usuario.

#### Paso 1.6: Diferencia adduser vs useradd

```bash
echo "=== Debian: adduser es un script Perl ==="
docker compose exec debian-dev file /usr/sbin/adduser 2>/dev/null || echo "adduser no instalado"

echo ""
echo "=== AlmaLinux: adduser es symlink a useradd ==="
docker compose exec alma-dev file /usr/sbin/adduser 2>/dev/null || echo "adduser no encontrado"
```

En Debian, `adduser` es un wrapper interactivo. En RHEL, es un
symlink a `useradd`. Para scripts, usar siempre `useradd`.

---

### Parte 2 — usermod

#### Paso 2.1: Cambiar shell

```bash
docker compose exec debian-dev bash -c '
echo "=== Shell actual de labuser ==="
getent passwd labuser | cut -d: -f7

echo ""
echo "=== Cambiar a /bin/sh ==="
usermod -s /bin/sh labuser
getent passwd labuser | cut -d: -f7

echo ""
echo "=== Restaurar a /bin/bash ==="
usermod -s /bin/bash labuser
getent passwd labuser | cut -d: -f7
'
```

#### Paso 2.2: Agregar a grupos — el error clásico

```bash
docker compose exec debian-dev bash -c '
echo "=== Grupos actuales de labuser ==="
id labuser

echo ""
echo "=== CORRECTO: -aG agrega SIN quitar ==="
usermod -aG labgroup labuser
id labuser
echo "(ahora tiene labgroup además de los anteriores)"

echo ""
echo "=== INCORRECTO: -G sin -a REEMPLAZA todos los grupos ==="
echo "(no ejecutar en producción — solo demostración)"
usermod -G labgroup labuser
id labuser
echo "(perdió todos los grupos suplementarios excepto labgroup)"

echo ""
echo "=== Restaurar ==="
usermod -aG labgroup labuser
id labuser
'
```

`-G` sin `-a` **reemplaza** todos los grupos suplementarios. Es uno
de los errores más frecuentes. Siempre usar `-aG` para agregar.

#### Paso 2.3: Bloquear y desbloquear

```bash
docker compose exec debian-dev bash -c '
echo "=== Estado actual de labuser en shadow ==="
grep "^labuser:" /etc/shadow | cut -d: -f2 | head -c 5
echo "..."

echo ""
echo "=== Bloquear (antepone ! al hash) ==="
usermod -L labuser
grep "^labuser:" /etc/shadow | cut -d: -f2 | head -c 5
echo "..."
echo "(empieza con ! = bloqueado)"

echo ""
echo "=== Desbloquear (quita el !) ==="
usermod -U labuser
grep "^labuser:" /etc/shadow | cut -d: -f2 | head -c 5
echo "..."
echo "(el ! desapareció)"
'
```

`-L` bloquea anteponiendo `!` al hash. `-U` desbloquea quitándolo.
La contraseña original se preserva.

#### Paso 2.4: Cambiar UID y su efecto en archivos

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear archivo como labuser ==="
touch /tmp/labuser-file && chown labuser /tmp/labuser-file
ls -ln /tmp/labuser-file

echo ""
echo "=== Cambiar UID ==="
OLD_UID=$(id -u labuser)
usermod -u 3001 labuser
echo "UID cambiado de $OLD_UID a $(id -u labuser)"

echo ""
echo "=== El archivo en /tmp sigue con el UID viejo ==="
ls -ln /tmp/labuser-file
echo "(muestra el UID numérico porque ya no coincide)"

echo ""
echo "=== Archivos en home SÍ se actualizan ==="
ls -ln /home/labuser/ | head -3

rm -f /tmp/labuser-file
'
```

`usermod -u` actualiza los archivos dentro del home pero NO en otros
directorios. Hay que buscar y corregir manualmente con `find / -uid OLD_UID`.

---

### Parte 3 — userdel y comparación

#### Paso 3.1: userdel sin -r

```bash
docker compose exec debian-dev bash -c '
echo "=== Eliminar labuser2 sin -r ==="
userdel labuser2

echo ""
echo "--- El home sigue existiendo ---"
ls -ld /home/labuser2 2>/dev/null && echo "(home persiste)" || echo "no existe"

echo ""
echo "--- Limpiar manualmente ---"
rm -rf /home/labuser2
'
```

Sin `-r`, `userdel` elimina la entrada de los archivos pero no
borra el home ni el mail spool.

#### Paso 3.2: userdel -r

```bash
docker compose exec debian-dev bash -c '
echo "=== Eliminar labuser con -r ==="
userdel -r labuser

echo ""
echo "--- Home eliminado ---"
ls -ld /home/labuser 2>/dev/null || echo "home ya no existe"

echo ""
echo "--- Buscar archivos huérfanos ---"
find /tmp -nouser 2>/dev/null | head -5 || echo "(ningún archivo huérfano en /tmp)"
'
```

`-r` elimina el home y el mail spool. Pero NO busca archivos del
usuario en otros directorios. Usar `find / -nouser` después de eliminar.

#### Paso 3.3: Comparar defaults entre distribuciones

```bash
echo "=== Comparar CREATE_HOME ==="
echo "Debian:"
docker compose exec debian-dev grep CREATE_HOME /etc/login.defs 2>/dev/null || echo "(no definido = no)"
echo "AlmaLinux:"
docker compose exec alma-dev grep CREATE_HOME /etc/login.defs 2>/dev/null || echo "(no definido)"

echo ""
echo "=== Comparar ENCRYPT_METHOD ==="
echo "Debian:"
docker compose exec debian-dev grep ENCRYPT_METHOD /etc/login.defs
echo "AlmaLinux:"
docker compose exec alma-dev grep ENCRYPT_METHOD /etc/login.defs

echo ""
echo "=== Comparar SYS_UID_MIN ==="
echo "Debian:"
docker compose exec debian-dev grep SYS_UID_MIN /etc/login.defs
echo "AlmaLinux:"
docker compose exec alma-dev grep SYS_UID_MIN /etc/login.defs
```

---

### Limpieza final

```bash
docker compose exec debian-dev bash -c '
userdel -r labuser 2>/dev/null
userdel -r labuser2 2>/dev/null
userdel -r testmin 2>/dev/null
groupdel labgroup 2>/dev/null
echo "Limpieza completada"
'
```

---

## Ejercicios

### Ejercicio 1 — Crear usuario con home y verificar skel

```bash
docker compose exec debian-dev bash -c '
useradd -m -s /bin/bash -c "Test User" testlab
echo "=== passwd ==="
getent passwd testlab
echo "=== id ==="
id testlab
echo "=== home ==="
ls -la /home/testlab/
echo "=== skel ==="
ls -la /etc/skel/
'
```

**Predicción**: ¿Los archivos en `/home/testlab/` serán idénticos a los de `/etc/skel/`?

<details><summary>Respuesta</summary>

Sí, los archivos ocultos serán los mismos (`.bashrc`, `.profile`, `.bash_logout`).
`useradd -m` copia todo el contenido de `/etc/skel/` al nuevo home. La única
diferencia es que el home tendrá `.` y `..` como entradas de directorio, y
el propietario será `testlab` (no root como en `/etc/skel/`).

</details>

### Ejercicio 2 — El error de -G sin -a

```bash
docker compose exec debian-dev bash -c '
groupadd -f testgrp1
groupadd -f testgrp2
useradd -m -s /bin/bash -G testgrp1 testmod

echo "=== Estado inicial ==="
id testmod

echo ""
echo "=== Agregar testgrp2 con -aG ==="
usermod -aG testgrp2 testmod
id testmod

echo ""
echo "=== Ahora usar -G sin -a ==="
usermod -G testgrp1 testmod
id testmod
'
```

**Predicción**: Después de `usermod -G testgrp1 testmod`, ¿seguirá en `testgrp2`?

<details><summary>Respuesta</summary>

**No**. `-G` sin `-a` **reemplaza** toda la lista de grupos suplementarios.
El usuario `testmod` perderá `testgrp2` y quedará solo en `testgrp1`.

Este es el error más peligroso con `usermod`: si un usuario estaba en `sudo`
o `wheel` y ejecutas `usermod -G docker user`, pierde acceso de sudo
inmediatamente. Siempre usar `usermod -aG`.

</details>

### Ejercicio 3 — CREATE_HOME: Debian vs AlmaLinux

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c '
useradd testnohome
ls -ld /home/testnohome 2>/dev/null || echo "Home NO existe"
userdel testnohome
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c '
useradd testnohome
ls -ld /home/testnohome 2>/dev/null || echo "Home NO existe"
userdel -r testnohome 2>/dev/null; userdel testnohome 2>/dev/null
'
```

**Predicción**: ¿En cuál de los dos se creará el home sin usar `-m`?

<details><summary>Respuesta</summary>

Solo en **AlmaLinux**. La diferencia está en `/etc/login.defs`:
- Debian: `CREATE_HOME no` (o no definido) — sin `-m` no se crea
- AlmaLinux: `CREATE_HOME yes` — se crea automáticamente

Por portabilidad, **siempre** usar `-m` explícitamente para crear el home.
Nunca depender del default de la distribución.

</details>

### Ejercicio 4 — Bloquear y verificar el hash

```bash
docker compose exec debian-dev bash -c '
useradd -m -s /bin/bash lockdemo
echo "password123" | passwd --stdin lockdemo 2>/dev/null || echo "password123" | chpasswd <<< "lockdemo:password123"

echo "=== Antes de bloquear ==="
grep "^lockdemo:" /etc/shadow | cut -d: -f2 | cut -c1-6
passwd -S lockdemo

echo ""
echo "=== Después de usermod -L ==="
usermod -L lockdemo
grep "^lockdemo:" /etc/shadow | cut -d: -f2 | cut -c1-6
passwd -S lockdemo

echo ""
echo "=== Después de usermod -U ==="
usermod -U lockdemo
grep "^lockdemo:" /etc/shadow | cut -d: -f2 | cut -c1-6
passwd -S lockdemo

userdel -r lockdemo
'
```

**Predicción**: ¿Qué carácter aparecerá al inicio del hash después de `usermod -L`?

<details><summary>Respuesta</summary>

Un `!` al inicio del hash. `usermod -L` **antepone** `!` al hash existente
(por ejemplo, `!$y$j9T$...`). Esto significa que ninguna contraseña coincidirá
jamás, pero el hash original se preserva. `usermod -U` simplemente quita ese
`!`, restaurando la contraseña original sin necesidad de restablecerla.

`passwd -S` mostrará `L` (locked) después de bloquear y `P` (password set)
después de desbloquear.

</details>

### Ejercicio 5 — useradd con shell inexistente

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear usuario con shell que no existe ==="
useradd -m -s /bin/inventada shelltest
echo "Exit code: $?"

echo ""
echo "=== Verificar ==="
getent passwd shelltest

echo ""
echo "=== ¿La shell existe en el sistema? ==="
ls -la /bin/inventada 2>/dev/null || echo "/bin/inventada NO existe"

echo ""
echo "=== ¿Está en /etc/shells? ==="
grep inventada /etc/shells || echo "NO está en /etc/shells"

userdel -r shelltest
'
```

**Predicción**: ¿`useradd` fallará con exit code distinto de 0?

<details><summary>Respuesta</summary>

**No**, `useradd` **no valida** que la shell exista. El usuario se creará
exitosamente (exit code 0) con `/bin/inventada` como shell. Sin embargo:
- El usuario no podrá iniciar sesión interactivamente (la shell no existe)
- `chsh` rechazaría esa shell porque no está en `/etc/shells`

`useradd` confía en que root sabe lo que hace. La validación de shells
solo ocurre en `chsh` (que consulta `/etc/shells`) y en el proceso de login
(que intenta ejecutar la shell).

</details>

### Ejercicio 6 — USERGROUPS_ENAB y flag -N

```bash
docker compose exec debian-dev bash -c '
echo "USERGROUPS_ENAB = $(grep USERGROUPS_ENAB /etc/login.defs | awk "{print \$2}")"

echo ""
echo "=== Con grupo privado (default) ==="
useradd -m -s /bin/bash testgrp1
echo "passwd: $(getent passwd testgrp1 | cut -d: -f4)"
echo "group:  $(getent group testgrp1)"

echo ""
echo "=== Sin grupo privado (-N) ==="
useradd -m -s /bin/bash -N testgrp2
echo "passwd: $(getent passwd testgrp2 | cut -d: -f4)"
echo "group:  $(getent group testgrp2 2>/dev/null || echo "NO existe")"

userdel -r testgrp1
userdel -r testgrp2
'
```

**Predicción**: ¿Qué GID tendrá `testgrp2` creado con `-N`?

<details><summary>Respuesta</summary>

Con `USERGROUPS_ENAB yes` (default en ambas familias), `useradd` crea un grupo
con el mismo nombre que el usuario. El flag `-N` desactiva esto, y el usuario
recibe el GID del grupo por defecto (`GROUP=100` en `useradd -D`, que
típicamente corresponde al grupo `users`).

- `testgrp1`: GID propio (ej. 2002) con grupo `testgrp1` creado
- `testgrp2`: GID 100 (grupo `users`), sin grupo `testgrp2`

</details>

### Ejercicio 7 — Cambiar UID y encontrar archivos huérfanos

```bash
docker compose exec debian-dev bash -c '
useradd -m -s /bin/bash uidtest
touch /tmp/uidtest-outside && chown uidtest /tmp/uidtest-outside
touch /home/uidtest/inside-home && chown uidtest /home/uidtest/inside-home

echo "=== Antes del cambio ==="
echo "UID: $(id -u uidtest)"
ls -ln /tmp/uidtest-outside
ls -ln /home/uidtest/inside-home

echo ""
OLD_UID=$(id -u uidtest)
usermod -u 5555 uidtest

echo "=== Después del cambio ==="
echo "UID: $(id -u uidtest)"
ls -ln /tmp/uidtest-outside
ls -ln /home/uidtest/inside-home

echo ""
echo "=== Buscar archivos con UID viejo ==="
find / -uid "$OLD_UID" 2>/dev/null

rm -f /tmp/uidtest-outside
userdel -r uidtest
'
```

**Predicción**: ¿`usermod -u` actualizará el propietario del archivo en `/tmp/`?

<details><summary>Respuesta</summary>

**No**. `usermod -u` solo actualiza archivos dentro del directorio home del
usuario. El archivo en `/tmp/` conservará el UID viejo y aparecerá como
número sin nombre en `ls -ln` (porque ya no hay usuario con ese UID).

Por eso es importante ejecutar `find / -uid OLD_UID` después de cambiar un
UID, para encontrar y corregir archivos huérfanos con
`find / -uid OLD_UID -exec chown NEW_UID {} \;`.

</details>

### Ejercicio 8 — userdel con y sin -r

```bash
docker compose exec debian-dev bash -c '
useradd -m -s /bin/bash deltest1
useradd -m -s /bin/bash deltest2
touch /home/deltest1/important.txt
touch /home/deltest2/important.txt

echo "=== userdel SIN -r ==="
userdel deltest1
echo "passwd: $(getent passwd deltest1 2>/dev/null || echo "eliminado")"
echo "home:   $(ls -ld /home/deltest1 2>/dev/null && echo "EXISTE" || echo "no existe")"

echo ""
echo "=== userdel CON -r ==="
userdel -r deltest2
echo "passwd: $(getent passwd deltest2 2>/dev/null || echo "eliminado")"
echo "home:   $(ls -ld /home/deltest2 2>/dev/null && echo "EXISTE" || echo "no existe")"

rm -rf /home/deltest1
'
```

**Predicción**: ¿Qué método dejará archivos residuales?

<details><summary>Respuesta</summary>

`userdel` sin `-r` eliminará la entrada de `/etc/passwd` y `/etc/shadow`,
pero el directorio `/home/deltest1/` y sus archivos **persistirán**. Ahora
son archivos "huérfanos" — pertenecen a un UID que ya no existe.

`userdel -r` elimina la entrada Y el home. Pero incluso con `-r`, archivos
en otros directorios (como `/tmp/`, `/var/`) no se tocan. Por eso el paso
final de un deprovisioning de usuario debería incluir `find / -nouser`.

</details>

### Ejercicio 9 — Usuario de servicio con -r

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear usuario de servicio ==="
useradd -r -s /usr/sbin/nologin -c "MyApp Service" myappsvc

echo ""
echo "=== Verificar ==="
getent passwd myappsvc
echo "UID: $(id -u myappsvc)"
echo "Shell: $(getent passwd myappsvc | cut -d: -f7)"
echo "Home existe: $(ls -ld /home/myappsvc 2>/dev/null && echo SÍ || echo NO)"

echo ""
echo "=== Rango de UID ==="
echo "SYS_UID_MIN=$(grep SYS_UID_MIN /etc/login.defs | awk "{print \$2}")"
echo "SYS_UID_MAX=$(grep SYS_UID_MAX /etc/login.defs | awk "{print \$2}")"

userdel myappsvc
'
```

**Predicción**: ¿El UID asignado estará en el rango 1000+ o por debajo?

<details><summary>Respuesta</summary>

Estará **por debajo de 1000** (en el rango de sistema). El flag `-r` indica
a `useradd` que cree un usuario de sistema, cuyo UID se asigna del rango
`SYS_UID_MIN`–`SYS_UID_MAX` (100-999 en Debian, 201-999 en AlmaLinux).

Además, `-r` no crea home por defecto (a diferencia del comportamiento
normal en RHEL con `CREATE_HOME yes`) y no crea grupo privado en algunas
configuraciones. Los usuarios de sistema son para daemons y servicios.

</details>

### Ejercicio 10 — deluser para quitar de grupo (Debian)

```bash
docker compose exec debian-dev bash -c '
useradd -m -s /bin/bash -G sudo grptest 2>/dev/null
echo "=== Antes ==="
id grptest

echo ""
echo "=== Quitar de sudo con deluser (Debian) ==="
deluser grptest sudo 2>/dev/null || echo "deluser no disponible"
echo "Después: $(id grptest)"

echo ""
echo "=== Alternativa portable: gpasswd -d ==="
usermod -aG sudo grptest
echo "Re-agregado: $(id grptest)"
gpasswd -d grptest sudo
echo "Quitado con gpasswd: $(id grptest)"

userdel -r grptest
'
```

**Predicción**: ¿`deluser grptest sudo` eliminará al usuario o solo lo quitará del grupo?

<details><summary>Respuesta</summary>

Solo lo **quitará del grupo** `sudo`. En Debian, `deluser` tiene doble
función:
- `deluser username` → elimina el usuario (wrapper de `userdel`)
- `deluser username groupname` → quita al usuario del grupo (NO lo elimina)

La alternativa portable (funciona en ambas familias) es `gpasswd -d user group`.
No hay forma de quitar un usuario de un grupo con `usermod` — `usermod -G` solo
puede *establecer* la lista completa, no quitar un grupo individual.

</details>

---

## Conceptos reforzados

1. `useradd -m -s /bin/bash` es la forma portable de crear usuarios.
   Sin `-m`, Debian no crea el home. Siempre especificar explícitamente.

2. `usermod -aG` agrega a un grupo SIN quitar los existentes.
   `-G` sin `-a` **reemplaza** todos los grupos suplementarios.

3. `userdel -r` elimina home y mail spool, pero hay que buscar
   archivos huérfanos en otros directorios con `find / -nouser`.

4. `usermod` **no tiene** flag `-r`. No confundir con `useradd -r`
   (crear usuario de sistema).

5. `useradd` no valida que la shell exista. La validación ocurre en
   `chsh` (consulta `/etc/shells`) y en el proceso de login.

6. Los defaults de `useradd` se configuran en `/etc/default/useradd`
   y `/etc/login.defs`. Difieren entre Debian y RHEL.

7. El directorio `/etc/skel/` define el contenido inicial del home
   de nuevos usuarios. Los usuarios existentes no se ven afectados.

8. Para quitar un usuario de un grupo individual, usar `gpasswd -d user group`
   (portable) o `deluser user group` (solo Debian).
