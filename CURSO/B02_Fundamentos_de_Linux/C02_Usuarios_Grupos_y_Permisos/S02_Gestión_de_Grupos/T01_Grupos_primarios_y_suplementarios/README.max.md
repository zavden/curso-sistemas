# T01 — Grupos primarios y suplementarios (Enhanced)

## dos tipos de membresía

Cada usuario en Linux pertenece a grupos de dos formas distintas:

- **Grupo primario**: exactamente uno. Definido en el campo GID de `/etc/passwd`.
  Determina el grupo propietario de los archivos que el usuario crea.
- **Grupos suplementarios**: cero o más. Listados en el campo 4 de `/etc/group`.
  Otorgan permisos adicionales sin cambiar el grupo por defecto de archivos nuevos.

```bash
id dev
# uid=1000(dev) gid=1000(dev) groups=1000(dev),27(sudo),999(docker)
#                    ^                   ^         ^         ^
#               primario          primario   suplementario  suplementario
#              (gid=)           (en groups=, siempre aparece primero)
```

## Grupo primario

### Cómo se asigna

Cuando se crea un usuario con `useradd`, el comportamiento por defecto depende
de `USERGROUPS_ENAB` en `/etc/login.defs`:

```bash
grep USERGROUPS_ENAB /etc/login.defs
# USERGROUPS_ENAB yes
```

Con `USERGROUPS_ENAB yes` (default en Debian y RHEL):

```bash
sudo useradd -m alice
# Se crea el grupo "alice" (mismo nombre que el usuario)
# Se asigna como grupo primario de alice

getent passwd alice
# alice:x:1001:1001::/home/alice:/bin/sh
#                ^
#           GID 1001 → grupo "alice"

getent group alice
# alice:x:1001:
```

Este esquema se llama **User Private Group (UPG)** — cada usuario tiene su
propio grupo privado. Es el estándar en distribuciones modernas.

### Efecto en la creación de archivos

El grupo primario determina el grupo propietario de todo archivo o directorio
que el usuario crea:

```bash
# Como usuario dev (grupo primario: dev)
touch /tmp/test-file
ls -la /tmp/test-file
# -rw-r--r-- 1 dev dev ... /tmp/test-file
#                  ^
#             grupo primario de dev

mkdir /tmp/test-dir
ls -ld /tmp/test-dir
# drwxr-xr-x 2 dev dev ... /tmp/test-dir
```

### Cambiar el grupo primario

```bash
# Permanente — cambiar el GID en /etc/passwd
sudo usermod -g developers dev

# Verificar
id dev
# uid=1000(dev) gid=1002(developers) groups=1002(developers),27(sudo)

# Ahora los archivos nuevos se crean con grupo "developers"
touch /tmp/new-file
ls -la /tmp/new-file
# -rw-r--r-- 1 dev developers ... /tmp/new-file
```

## Grupos suplementarios

Los grupos suplementarios otorgan permisos adicionales. Un usuario puede
pertenecer a múltiples grupos suplementarios simultáneamente:

```bash
# Agregar al grupo docker (suplementario)
sudo usermod -aG docker dev

# Agregar a múltiples grupos
sudo usermod -aG sudo,docker,www-data dev

# Ver todos los grupos
id dev
# uid=1000(dev) gid=1000(dev) groups=1000(dev),27(sudo),33(www-data),999(docker)

groups dev
# dev : dev sudo www-data docker
```

### Cómo funcionan los permisos con grupos

Cuando un proceso intenta acceder a un archivo, el kernel verifica:

1. ¿El UID del proceso coincide con el propietario del archivo? → usa permisos de **owner**
2. ¿El GID primario o algún GID suplementario coincide con el GID del archivo? → usa permisos de **group**
3. Ninguno coincide → usa permisos de **others**

```bash
# Archivo propiedad de root:docker con permisos 660
ls -la /var/run/docker.sock
# srw-rw---- 1 root docker 0 ... /var/run/docker.sock

# dev (UID 1000) no es root → no coincide owner
# dev tiene docker como grupo suplementario → SÍ coincide group → acceso rw
docker ps
# (funciona porque dev está en el grupo docker)
```

### Listar y gestionar membresías

```bash
# Ver grupos de un usuario
groups dev
# dev : dev sudo docker

# Ver miembros de un grupo
getent group docker
# docker:x:999:dev,alice,bob

# Quitar un usuario de un grupo suplementario
# Método 1: gpasswd
sudo gpasswd -d dev docker

# Método 2: deluser (solo Debian)
sudo deluser dev docker

# No existe "usermod -rG" — para quitar un grupo específico hay que usar
# gpasswd -d o reconstruir la lista con usermod -G
```

## newgrp — Cambiar grupo primario temporal

`newgrp` inicia una **nueva shell** con un grupo primario diferente. Es
temporal — solo afecta a esa shell y sus procesos hijos:

```bash
# Grupo primario actual
id -gn
# dev

# Cambiar a docker como grupo primario (temporalmente)
newgrp docker

# Verificar
id -gn
# docker

# Los archivos creados ahora tendrán grupo "docker"
touch /tmp/docker-file
ls -la /tmp/docker-file
# -rw-r--r-- 1 dev docker ... /tmp/docker-file

# Salir de la shell de newgrp (volver al grupo original)
exit
id -gn
# dev
```

### newgrp vs usermod -g

| Aspecto | `newgrp grupo` | `usermod -g grupo user` |
|---|---|---|
| Alcance | Solo la shell actual | Permanente, todas las sesiones |
| Requiere logout | No | Sí (o `newgrp` para la sesión actual) |
| Requiere root | No (si el usuario es miembro) | Sí |
| Modifica `/etc/passwd` | No | Sí |

### newgrp con grupos no propios

Si el usuario no es miembro del grupo, `newgrp` pedirá la contraseña del grupo
(si tiene una configurada en `/etc/gshadow`):

```bash
# dev no es miembro de "staff"
newgrp staff
# Password:
# (si el grupo tiene contraseña en /etc/gshadow, se puede entrar con ella)
# (si no tiene, se rechaza)
```

En la práctica, las contraseñas de grupo casi nunca se usan.

## sg — Ejecutar un comando con otro grupo

`sg` es similar a `newgrp` pero ejecuta un solo comando en vez de abrir una
shell:

```bash
# Ejecutar un comando con grupo primario "docker"
sg docker -c "touch /tmp/sg-test"
ls -la /tmp/sg-test
# -rw-r--r-- 1 dev docker ... /tmp/sg-test

# No cambia la shell actual
id -gn
# dev  (sigue siendo dev)
```

## Herramientas de consulta

```bash
# id — información completa de un usuario
id dev
# uid=1000(dev) gid=1000(dev) groups=1000(dev),27(sudo),999(docker)

id -u dev     # Solo UID
id -g dev     # Solo GID primario
id -G dev     # Todos los GIDs (numéricos)
id -Gn dev    # Todos los grupos (nombres)
id -gn dev    # Solo grupo primario (nombre)

# groups — lista de grupos de un usuario
groups dev
# dev : dev sudo docker

# Sin argumento: grupos del usuario actual
groups
# dev sudo docker

# getent — consultar el grupo con sus miembros
getent group docker
# docker:x:999:dev,alice
```

## Grupos en procesos

Cada proceso hereda los grupos del usuario que lo lanzó. Se pueden ver en
`/proc/[pid]/status`:

```bash
cat /proc/self/status | grep -i groups
# Groups: 1000 27 999
#          ^    ^   ^
#         dev  sudo docker
```

Los grupos se establecen al hacer login y permanecen fijos durante toda la
sesión. Cambios con `usermod -aG` no afectan sesiones existentes:

```bash
# Agregar al grupo www-data
sudo usermod -aG www-data dev

# En la sesión actual, el grupo NO aparece todavía
id
# uid=1000(dev) gid=1000(dev) groups=1000(dev),27(sudo),999(docker)
# (www-data no aparece)

# Opciones para activarlo:
# 1. Cerrar sesión y volver a entrar
# 2. Usar newgrp www-data (abre shell nueva)
# 3. Ejecutar: exec su -l $USER (reemplaza la shell actual)
```

## Tabla resumen: comandos de gestión de grupos

| Tarea | Comando | Notas |
|---|---|---|
| Ver grupos de usuario | `id user`, `groups user` | |
| Agregar grupo suplementario | `usermod -aG group user` | -a es crucial |
| Quitar de grupo | `gpasswd -d user group` | |
| Cambiar grupo primario | `usermod -g group user` | Requiere sudo |
| Cambiar grupo efectivo temporal | `newgrp group` | Nueva shell |
| Ejecutar con grupo diferente | `sg group -c "cmd"` | Sin nueva shell permanente |

## Ejercicios

### Ejercicio 1 — Inspeccionar grupos

```bash
# Ver todos tus grupos
id
groups

# ¿Cuál es tu grupo primario?
id -gn

# ¿Cuántos grupos suplementarios tienes?
id -Gn | wc -w
```

### Ejercicio 2 — Crear y asignar grupos

```bash
# Crear un grupo
sudo groupadd testgroup

# Crear un usuario y asignarlo al grupo como suplementario
sudo useradd -m -s /bin/bash -G testgroup testuser

# Verificar
id testuser
getent group testgroup

# Limpiar
sudo userdel -r testuser
sudo groupdel testgroup
```

### Ejercicio 3 — newgrp y creación de archivos

```bash
# Crear un grupo de prueba y agregar tu usuario
sudo groupadd labgroup
sudo usermod -aG labgroup $(whoami)

# Usar newgrp para activar el grupo sin re-login
newgrp labgroup

# Crear un archivo — ¿qué grupo tiene?
touch /tmp/labgroup-test
ls -la /tmp/labgroup-test

# Salir de la shell de newgrp
exit

# Crear otro archivo — ¿qué grupo tiene ahora?
touch /tmp/original-test
ls -la /tmp/original-test

# Limpiar
rm /tmp/labgroup-test /tmp/original-test
sudo groupdel labgroup
```

### Ejercicio 4 — Diferencia entre grupos suplementarios y primarios

```bash
#!/bin/bash
# Crear escenario donde se ve la diferencia

sudo groupadd projteam

# Crear dos usuarios
sudo useradd -m -s /bin/bash -G projteam alice
sudo useradd -m -s /bin/bash bob

# Crear directorio compartido
sudo mkdir /tmp/shared_proj
sudo chown root:projteam /tmp/shared_proj
sudo chmod 2775 /tmp/shared_proj

echo "=== Permisos del directorio ==="
ls -ld /tmp/shared_proj

echo ""
echo "=== alice (en projteam) crea archivo ==="
sudo -u alice sh -c 'cd /tmp/shared_proj && touch alice_file.txt'
sudo -u alice ls -la /tmp/shared_proj/alice_file.txt

echo ""
echo "=== bob (NO en projteam) crea archivo ==="
sudo -u bob sh -c 'cd /tmp/shared_proj && touch bob_file.txt'
sudo -u bob ls -la /tmp/shared_proj/bob_file.txt

echo ""
echo "¿Por qué alice puede escribir pero bob no?"

# Limpiar
rm -rf /tmp/shared_proj
sudo userdel -r alice
sudo userdel -r bob
sudo groupdel projteam
```

### Ejercicio 5 — gpasswd vs usermod

```bash
#!/bin/bash
# Comparar métodos para quitar un usuario de un grupo

sudo groupadd testgrp
sudo useradd -m -s /bin/bash -G testgrp,uucp,news demouser

echo "=== Grupos iniciales ==="
id demouser

echo ""
echo "=== Quitar testgrp con gpasswd -d ==="
sudo gpasswd -d demouser testgrp
id demouser

echo ""
echo "=== Agregar testgrp de nuevo ==="
sudo usermod -aG testgrp demouser
id demouser

echo ""
echo "=== gpasswd -d otra vez ==="
sudo gpasswd -d demouser testgrp

# Limpiar
sudo userdel -r demouser
sudo groupdel testgrp
```

### Ejercicio 6 — Permisos effective vs real

```bash
#!/bin/bash
# Demostrar que los grupos suplementarios afectan permisos pero no el GID efectivo

echo "Grupos actuales (incluye suplementarios):"
id

echo ""
echo "GID efectivo (grupo primario):"
id -gn

echo ""
echo "GIDs numéricos (todos):"
id -G

echo ""
echo "4 GIDs del proceso desde /proc:"
cat /proc/self/status | grep ^Gid
```

### Ejercicio 7 — Grupo primario vs suplementario en permisos

```bash
#!/bin/bash
# Crear archivo donde el grupo primario del usuario NO tiene acceso
# pero el grupo suplementario SÍ lo tiene

sudo groupadd securegroup
sudo useradd -m -s /bin/bash -G securegroup secuser

# Archivo con permisos solo para grupo y others
sudo touch /tmp/secfile.txt
sudo chown secuser:securegroup /tmp/secfile.txt
sudo chmod 760 /tmp/secfile.txt  # rwxrw---- (grupo: rw, others: nada)

echo "=== Permisos del archivo ==="
ls -la /tmp/secfile.txt

echo ""
echo "=== secuser (pertenece a securegroup como suplementario) ==="
sudo -u secuser cat /tmp/secfile.txt && echo "LECTURA OK" || echo "LECTURA DENEGADA"

echo ""
echo "=== Cambiar a grupo primario ==="
sudo -u secuser newgrp securegroup
sudo -u secuser cat /tmp/secfile.txt && echo "LECTURA OK" || echo "LECTURA DENEGADA"

# Limpiar
rm /tmp/secfile.txt
sudo userdel -r secuser
sudo groupdel securegroup
```

### Ejercicio 8 — UPG en práctica

```bash
#!/bin/bash
# Ver cómo UPG funciona con umask 002 vs 022

sudo useradd -m -s /bin/bash upguser1
sudo useradd -m -s /bin/bash -g users upguser2

echo "=== upguser1 (UPG: grupo propio) ==="
getent passwd upguser1 | cut -d: -f4
getent group upguser1 | cut -d: -f3

echo ""
echo "=== upguser2 (grupo primario: users) ==="
getent passwd upguser2 | cut -d: -f4
getent group users | cut -d: -f3 || echo "Grupo users no existe"

# Limpiar
sudo userdel -r upguser1
sudo userdel -r upguser2 2>/dev/null
```
