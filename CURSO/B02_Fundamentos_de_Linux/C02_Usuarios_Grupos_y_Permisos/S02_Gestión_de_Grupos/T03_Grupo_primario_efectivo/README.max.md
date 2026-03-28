# T03 — Grupo primario efectivo (Enhanced)

## Qué es el grupo primario efectivo

El **grupo primario efectivo** es el GID que el kernel usa para determinar el
grupo propietario de los archivos que un proceso crea. Normalmente coincide con
el grupo primario definido en `/etc/passwd`, pero puede cambiarse en runtime
con `newgrp` o `sg`.

```bash
# Grupo primario (de /etc/passwd)
id -gn
# dev

# Grupo primario efectivo del proceso actual (de /proc)
cat /proc/self/status | grep ^Gid
# Gid:    1000    1000    1000    1000
#          ^       ^       ^       ^
#        real  efectivo  saved   fs
```

Los cuatro GIDs del proceso son:

| GID | Descripción |
|---|---|
| Real | GID original del usuario al hacer login |
| Efectivo | GID que se usa para verificar permisos y asignar grupo a archivos nuevos |
| Saved | Copia del efectivo antes de un cambio (permite restaurar) |
| Filesystem | GID usado para operaciones de filesystem (normalmente = efectivo) |

En condiciones normales, los cuatro son iguales. Divergen cuando se usan
mecanismos como SGID o `newgrp`.

## Impacto en la creación de archivos

Todo archivo o directorio creado hereda:
- **Owner**: el UID efectivo del proceso
- **Group**: el GID efectivo del proceso (salvo que el directorio padre tenga SGID)

```bash
# Grupo primario efectivo: dev
id -gn
# dev

touch /tmp/file-a
ls -la /tmp/file-a
# -rw-r--r-- 1 dev dev ... file-a

# Cambiar grupo primario efectivo con newgrp
newgrp docker

id -gn
# docker

touch /tmp/file-b
ls -la /tmp/file-b
# -rw-r--r-- 1 dev docker ... file-b
#                  ^^^^^^
#                  cambió porque el GID efectivo ahora es docker

exit  # volver al grupo original
```

## newgrp en detalle

`newgrp` crea una **nueva shell** con el GID efectivo cambiado. No modifica
la sesión actual — crea una subshell:

```bash
# Shell actual
echo $SHLVL
# 1

id -gn
# dev

newgrp docker
echo $SHLVL
# 2  (nueva shell, un nivel más profundo)

id -gn
# docker

exit  # vuelve a la shell anterior
echo $SHLVL
# 1

id -gn
# dev
```

### Qué cambia y qué no con newgrp

| Aspecto | ¿Cambia? |
|---|---|
| GID efectivo del proceso | Sí |
| Grupo propietario de archivos nuevos | Sí |
| `/etc/passwd` | No |
| Grupos suplementarios | Se mantienen |
| Variables de entorno | Se heredan |
| Directorio de trabajo | Se mantiene |

### newgrp sin ser miembro

Si el usuario no es miembro del grupo y el grupo tiene contraseña en
`/etc/gshadow`, se le pide la contraseña. Si no tiene contraseña, se rechaza:

```bash
newgrp staff
# Password: (si tiene contraseña en gshadow)
# o
# newgrp: failed to crypt password with previous salt: Invalid argument
```

## SGID en directorios — override del grupo efectivo

Cuando un directorio tiene el bit **SGID** activado, los archivos creados
dentro heredan el **grupo del directorio** en vez del grupo efectivo del
proceso:

```bash
# Sin SGID: el grupo del archivo es el GID efectivo del usuario
mkdir /tmp/normal-dir
touch /tmp/normal-dir/file
ls -la /tmp/normal-dir/file
# -rw-r--r-- 1 dev dev ... file

# Con SGID: el grupo del archivo es el del directorio
sudo mkdir /tmp/sgid-dir
sudo chown root:docker /tmp/sgid-dir
sudo chmod 2775 /tmp/sgid-dir

touch /tmp/sgid-dir/file
ls -la /tmp/sgid-dir/file
# -rw-r--r-- 1 dev docker ... file
#                  ^^^^^^
#          heredó "docker" del directorio, no "dev" del usuario

# Los subdirectorios también heredan el SGID
mkdir /tmp/sgid-dir/subdir
ls -ld /tmp/sgid-dir/subdir
# drwxrwsr-x 2 dev docker ... subdir
#       ^
#       s = SGID propagado al subdirectorio
```

SGID en directorios es la solución estándar para directorios de trabajo
compartidos. Sin SGID, cada archivo tendría el grupo primario de quien lo
creó, y los otros miembros no tendrían acceso por grupo.

Se cubre en detalle en S03 T03 (SUID, SGID, Sticky bit).

## Escenarios prácticos

### Desarrollo web compartido

```bash
# Configuración
sudo groupadd webdev
sudo usermod -aG webdev alice
sudo usermod -aG webdev bob

sudo mkdir -p /var/www/project
sudo chown root:webdev /var/www/project
sudo chmod 2775 /var/www/project

# alice crea un archivo
# (como alice)
touch /var/www/project/index.html
ls -la /var/www/project/index.html
# -rw-r--r-- 1 alice webdev ... index.html
#                    ^^^^^^
# Gracias al SGID, bob (también en webdev) puede leer el archivo
# Si se necesita escritura grupal, configurar umask 002 o usar ACLs
```

### Crear archivos con un grupo específico sin newgrp

```bash
# chgrp después de crear (más comandos pero no cambia de shell)
touch /tmp/report.csv
chgrp finance /tmp/report.csv

# install con grupo (crea y asigna en un solo comando)
install -g finance -m 664 /dev/null /tmp/report.csv

# sg — ejecutar un solo comando con grupo diferente
sg finance -c "touch /tmp/report.csv"
```

### Script que necesita crear archivos con grupo específico

```bash
#!/bin/bash
# Script que debe crear archivos con grupo "backup"

# Opción 1: sg para cada comando
sg backup -c "tar czf /backup/data.tar.gz /data"

# Opción 2: newgrp dentro del script (cuidado — abre subshell)
newgrp backup <<EOF
tar czf /backup/data.tar.gz /data
chown root:backup /backup/data.tar.gz
EOF
```

## Verificar el grupo efectivo actual

```bash
# Forma simple
id -gn
# dev

# Forma detallada
id
# uid=1000(dev) gid=1000(dev) groups=1000(dev),27(sudo),999(docker)
#                    ^^^
#               grupo primario efectivo

# Desde /proc (muestra los cuatro GIDs del proceso)
cat /proc/self/status | grep ^Gid
# Gid:    1000    1000    1000    1000

# Procesos hijos heredan el grupo efectivo
bash -c 'id -gn'
# dev (heredó el grupo del padre)
```

## Diferencias entre distribuciones

El comportamiento del grupo primario efectivo es idéntico entre Debian y RHEL
— es funcionalidad del kernel, no de la distribución. Las diferencias están en
los defaults:

| Aspecto | Debian/Ubuntu | AlmaLinux/RHEL |
|---|---|---|
| UPG (User Private Group) | Sí (por defecto) | Sí (por defecto) |
| `USERGROUPS_ENAB` | `yes` | `yes` |
| umask por defecto | `022` | `022` |
| SGID en `/home/user` | No | No |

Ambas familias usan UPG por defecto: cada usuario tiene su propio grupo
privado como grupo primario. Esto hace que la umask `022` sea segura (los
archivos se crean como `user:user`, no como `user:users`).

## Tabla: los 4 GIDs de un proceso

| Campo /proc | Nombre | Cuándo cambia |
|---|---|---|
| Gid: 1er valor | Real | Solo con setuid de root |
| Gid: 2do valor | Efectivo | con newgrp, sg, o binarios SGID |
| Gid: 3er valor | Saved | se guarda antes de cambiar efectivo |
| Gid: 4to valor | Filesystem | con CAP_SETFCAP o FSUGID |

## Ejercicios

### Ejercicio 1 — Observar el grupo efectivo

```bash
# ¿Cuál es tu grupo primario efectivo?
id -gn

# Cambiar temporalmente con newgrp
newgrp $(id -Gn | awk '{print $2}')  # usa tu segundo grupo

# Crear un archivo y verificar su grupo
touch /tmp/test-effective
ls -la /tmp/test-effective

# Volver
exit
```

### Ejercicio 2 — SGID vs grupo efectivo

```bash
# Crear directorio con SGID
sudo mkdir /tmp/sgid-lab
sudo chown root:$(id -gn) /tmp/sgid-lab
sudo chmod 2775 /tmp/sgid-lab

# Cambiar grupo efectivo con newgrp a otro grupo
newgrp $(id -Gn | awk '{print $2}')

# Crear archivo dentro del directorio SGID
touch /tmp/sgid-lab/test

# ¿Qué grupo tiene? ¿El del directorio (SGID) o el efectivo (newgrp)?
ls -la /tmp/sgid-lab/test

# Limpiar
exit
rm -rf /tmp/sgid-lab
```

### Ejercicio 3 — GIDs del proceso

```bash
# Ver los 4 GIDs de tu shell
cat /proc/self/status | grep ^Gid

# Comparar con newgrp
newgrp docker 2>/dev/null || newgrp $(id -Gn | awk '{print $NF}')
cat /proc/self/status | grep ^Gid
# ¿Cuáles cambiaron?
exit
```

### Ejercicio 4 — newgrp crea subshell

```bash
#!/bin/bash
# Demostrar que newgrp incrementa SHLVL

echo "SHLVL antes de newgrp: $SHLVL"
echo "GID efectivo antes: $(id -gn)"

newgrp $(id -Gn | awk '{print $NF}') 2>/dev/null

echo "SHLVL dentro de newgrp: $SHLVL"
echo "GID efectivo dentro: $(id -gn)"

# Salir y comparar
exit
echo "SHLVL después de exit: $SHLVL"
echo "GID efectivo después: $(id -gn)"
```

### Ejercicio 5 — sg vs newgrp

```bash
#!/bin/bash
# Comparar sg y newgrp

echo "=== Antes ==="
echo "SHLVL: $SHLVL"
echo "GID efectivo: $(id -gn)"

echo ""
echo "=== Con sg (no crea subshell nueva) ==="
sg $(id -Gn | awk '{print $NF}') -c 'echo "SHLVL: $SHLVL"; echo "GID: $(id -gn)"'
echo "SHLVL tras sg: $SHLVL"

echo ""
echo "=== Con newgrp (crea subshell) ==="
newgrp $(id -Gn | awk '{print $NF}')
echo "SHLVL tras newgrp: $SHLVL"
exit
```

### Ejercicio 6 — GID saved para restore

```bash
#!/bin/bash
# Demostrar el GID saved

echo "=== Estado inicial ==="
cat /proc/self/status | grep ^Gid

# newgrp cambia el efectivo pero guarda el anterior como saved
newgrp docker 2>/dev/null || newgrp $(id -Gn | awk '{print $NF}')

echo ""
echo "=== Dentro de newgrp ==="
cat /proc/self/status | grep ^Gid
# El tercer número (saved) debería seguir siendo el GID original

exit
```

### Ejercicio 7 — install -g para crear con grupo específico

```bash
#!/bin/bash
# Usar install -g para crear archivo con grupo específico

TARGET_GROUP=$(id -Gn | awk '{print $2}')
echo "Grupo objetivo: $TARGET_GROUP"

install -g "$TARGET_GROUP" -m 660 /dev/null /tmp/install-test.txt
ls -la /tmp/install-test.txt

# Comparar con touch
touch /tmp/touch-test.txt
ls -la /tmp/touch-test.txt

rm /tmp/install-test.txt /tmp/touch-test.txt
```

### Ejercicio 8 — umask y grupo efectivo

```bash
#!/bin/bash
# Crear archivo con umask 002 vs 002 y observar el grupo

umask 022
touch /tmp/umask-022.txt
ls -la /tmp/umask-022.txt

umask 002
newgrp $(id -Gn | awk '{print $NF}') 2>/dev/null
touch /tmp/umask-002-in-newgrp.txt
ls -la /tmp/umask-002-in-newgrp.txt

exit
rm /tmp/umask-022.txt /tmp/umask-002-in-newgrp.txt
```

### Ejercicio 9 — SGID hereda incluso con newgrp activo

```bash
#!/bin/bash
# Crear directorio SGID y probar que el grupo del directorio gana

sudo groupadd sgidtest
sudo mkdir /tmp/sgid-effective
sudo chown root:sgidtest /tmp/sgid-effective
sudo chmod 2775 /tmp/sgid-effective

# Activar otro grupo con newgrp
newgrp sgidtest 2>/dev/null || true

# Dentro de newgrp, crear archivo EN el directorio SGID
touch /tmp/sgid-effective/from-newgrp.txt

echo "=== Archivo creado dentro de newgrp, en directorio SGID ==="
ls -la /tmp/sgid-effective/from-newgrp.txt
# El grupo debería ser sgidtest (del directorio SGID, no del efectivo)

exit
rm -rf /tmp/sgid-effective
sudo groupdel sgidtest
```

### Ejercicio 10 — Proceso hijo hereda GID efectivo

```bash
#!/bin/bash
# Verificar que los procesos hijos heredan el GID efectivo

echo "=== Shell actual ==="
cat /proc/self/status | grep ^Gid

echo ""
echo "=== Proceso hijo (bash -c) ==="
bash -c 'cat /proc/self/status | grep ^Gid'

echo ""
echo "=== Con newgrp ==="
newgrp $(id -Gn | awk '{print $NF}') 2>/dev/null || true
bash -c 'cat /proc/self/status | grep ^Gid'
exit
```
