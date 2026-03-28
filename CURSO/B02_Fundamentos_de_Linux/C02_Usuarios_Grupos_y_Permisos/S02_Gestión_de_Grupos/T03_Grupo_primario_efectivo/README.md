# T03 — Grupo primario efectivo

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
dentro heredan el grupo del directorio **en vez del grupo efectivo del
proceso**:

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

---

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
