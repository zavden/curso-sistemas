# T02 — groupadd, groupmod, groupdel (Enhanced)

## groupadd — Crear grupos

```bash
# Crear un grupo regular
sudo groupadd developers

# Verificar
getent group developers
# developers:x:1001:
```

### Flags importantes

| Flag | Descripción | Ejemplo |
|---|---|---|
| (sin flags) | Crear con GID automático | `groupadd developers` |
| `-g GID` | Especificar GID | `groupadd -g 2000 developers` |
| `-r` | Grupo de sistema (GID bajo, < 1000) | `groupadd -r myservice` |
| `-f` | Forzar — no error si el grupo ya existe | `groupadd -f developers` |

```bash
# Grupo de sistema (UID en rango 100-999 o 201-999)
sudo groupadd -r appservice
getent group appservice
# appservice:x:998:

# Con GID específico
sudo groupadd -g 5000 shared-data
getent group shared-data
# shared-data:x:5000:
```

### Cuándo crear grupos

Los casos más comunes:

```bash
# Grupo para un equipo de trabajo
sudo groupadd webteam

# Grupo para acceso a un recurso compartido
sudo groupadd db-access

# Grupo para un servicio
sudo groupadd -r myapp

# Agregar usuarios al grupo
sudo usermod -aG webteam alice
sudo usermod -aG webteam bob

# Dar acceso al grupo sobre un directorio
sudo mkdir /srv/web
sudo chown root:webteam /srv/web
sudo chmod 2775 /srv/web
# (2 = SGID — archivos nuevos heredan el grupo del directorio)
```

## groupmod — Modificar grupos

```bash
# Cambiar nombre del grupo
sudo groupmod -n newname oldname

# Cambiar GID
sudo groupmod -g 3000 developers
```

### Flags

| Flag | Descripción | Ejemplo |
|---|---|---|
| `-n newname` | Renombrar | `groupmod -n dev-team developers` |
| `-g GID` | Cambiar GID | `groupmod -g 3000 developers` |

### Implicaciones de cambiar el GID

Cambiar el GID de un grupo **no actualiza automáticamente los archivos** que
pertenecen al GID anterior:

```bash
# Antes del cambio
ls -la /srv/web
# drwxrwsr-x 2 root developers ... /srv/web
# (grupo developers, GID 1001)

# Cambiar GID
sudo groupmod -g 3000 developers

# Después del cambio
ls -la /srv/web
# drwxrwsr-x 2 root 1001 ... /srv/web
# (muestra el GID numérico porque ya no existe grupo con GID 1001)

# Corregir — buscar y actualizar archivos con el GID viejo
sudo find / -gid 1001 -exec chgrp developers {} \; 2>/dev/null
```

Por esta razón, cambiar GIDs en producción es una operación delicada que
requiere planificación.

### Renombrar un grupo

Renombrar es más seguro que cambiar el GID — no afecta la propiedad de
archivos porque el GID numérico no cambia:

```bash
sudo groupmod -n dev-team developers

# Los archivos siguen funcionando — mismo GID, diferente nombre
ls -la /srv/web
# drwxrwsr-x 2 root dev-team ... /srv/web
```

## groupdel — Eliminar grupos

```bash
# Eliminar un grupo
sudo groupdel testgroup
```

### Restricciones

`groupdel` se niega a eliminar un grupo si es el **grupo primario** de algún
usuario:

```bash
sudo groupdel dev
# groupdel: cannot remove the primary group of user 'dev'
```

Para eliminar el grupo, primero hay que cambiar el grupo primario de todos
los usuarios que lo usen:

```bash
# Cambiar el grupo primario de dev a otro grupo
sudo usermod -g users dev

# Ahora sí se puede eliminar
sudo groupdel dev
```

### Archivos huérfanos

Al eliminar un grupo, los archivos que le pertenecían muestran el GID numérico:

```bash
# Antes
ls -la /tmp/shared-file
# -rw-rw-r-- 1 alice webteam ... /tmp/shared-file

sudo groupdel webteam

# Después
ls -la /tmp/shared-file
# -rw-rw-r-- 1 alice 1005 ... /tmp/shared-file

# Buscar archivos huérfanos
sudo find / -nogroup -ls 2>/dev/null
```

## gpasswd — Gestión avanzada de membresía

`gpasswd` es una herramienta específica para gestionar miembros y
administradores de grupos:

```bash
# Agregar usuario a un grupo
sudo gpasswd -a alice developers
# Adding user alice to group developers

# Quitar usuario de un grupo
sudo gpasswd -d alice developers
# Removing user alice from group developers

# Establecer la lista completa de miembros (reemplaza)
sudo gpasswd -M alice,bob,charlie developers

# Asignar un administrador del grupo (puede agregar/quitar miembros sin sudo)
sudo gpasswd -A alice developers
# Ahora alice puede hacer:
# gpasswd -a bob developers  (sin sudo)

# Establecer contraseña de grupo (raro, casi nunca se usa)
sudo gpasswd developers
# Permite a no-miembros usar "newgrp developers" con contraseña

# Quitar contraseña del grupo
sudo gpasswd -r developers
```

### gpasswd vs usermod para membresías

| Operación | gpasswd | usermod |
|---|---|---|
| Agregar a un grupo | `gpasswd -a user group` | `usermod -aG group user` |
| Quitar de un grupo | `gpasswd -d user group` | No puede (solo reemplazar toda la lista) |
| Establecer miembros | `gpasswd -M u1,u2 group` | No aplica |

`gpasswd -d` es la forma estándar de quitar un usuario de un solo grupo sin
afectar sus otras membresías. Con `usermod` habría que listar todos los grupos
que debe mantener con `-G`.

## Grupos en el sistema de archivos

### Directorios compartidos por grupo

El patrón estándar para directorios compartidos entre miembros de un grupo:

```bash
# Crear grupo y directorio compartido
sudo groupadd project-x
sudo mkdir -p /srv/project-x

# Asignar propiedad
sudo chown root:project-x /srv/project-x

# Permisos: rwx para owner y group, SGID para herencia de grupo
sudo chmod 2775 /srv/project-x

# Verificar
ls -ld /srv/project-x
# drwxrwsr-x 2 root project-x ... /srv/project-x
#       ^
#       s = SGID activo

# Agregar usuarios
sudo usermod -aG project-x alice
sudo usermod -aG project-x bob

# Ahora alice crea un archivo:
touch /srv/project-x/design.txt
ls -la /srv/project-x/design.txt
# -rw-r--r-- 1 alice project-x ... design.txt
#                     ^^^^^^^^^
# Gracias al SGID, el grupo es project-x (no el grupo primario de alice)
```

El SGID en directorios se explica en detalle en S03 (Permisos).

## Diferencias entre distribuciones

| Aspecto | Debian/Ubuntu | AlmaLinux/RHEL |
|---|---|---|
| Rango GID de sistema | 100-999 | 201-999 |
| Grupo de sudo | `sudo` (GID 27) | `wheel` (GID 10) |
| GID de nobody | `nogroup` (65534) | `nobody` (65534) |
| `addgroup` | Existe (wrapper Perl) | Symlink a `groupadd` |
| `delgroup` | Existe (wrapper Perl) | No existe |

```bash
# Debian: addgroup es interactivo
sudo addgroup --system myservice
# Adding group `myservice' (GID 998) ...

# RHEL: addgroup no existe como wrapper
sudo groupadd -r myservice
```

## Tabla: lifecycle de un grupo

| Operación | Comando | Restricción |
|---|---|---|
| Crear | `groupadd name` | Nombre único |
| Renombrar | `groupmod -n new old` | GID no cambia |
| Cambiar GID | `groupmod -g N name` | No re-indexa archivos |
| Agregar miembro | `gpasswd -a user group` o `usermod -aG group user` | |
| Quitar miembro | `gpasswd -d user group` | |
| Eliminar | `groupdel name` | No debe ser primario de nadie |

## Ejercicios

### Ejercicio 1 — Ciclo de vida de un grupo

```bash
# Crear un grupo
sudo groupadd labteam

# Verificar que existe
getent group labteam

# Agregar usuarios
sudo gpasswd -a $(whoami) labteam

# Verificar membresía
getent group labteam

# Renombrar
sudo groupmod -n lab-team labteam

# Verificar que el GID no cambió
getent group lab-team

# Quitar usuario
sudo gpasswd -d $(whoami) lab-team

# Eliminar grupo
sudo groupdel lab-team
```

### Ejercicio 2 — Directorio compartido

```bash
# Crear grupo y directorio compartido
sudo groupadd shared
sudo mkdir /tmp/shared-lab
sudo chown root:shared /tmp/shared-lab
sudo chmod 2775 /tmp/shared-lab

# Agregar tu usuario
sudo usermod -aG shared $(whoami)
newgrp shared

# Crear archivos — ¿qué grupo tienen?
touch /tmp/shared-lab/file1.txt
ls -la /tmp/shared-lab/

# Limpiar
exit  # salir de newgrp
rm -rf /tmp/shared-lab
sudo groupdel shared
```

### Ejercicio 3 — Encontrar inconsistencias

```bash
# Verificar consistencia de /etc/group
sudo grpck

# Buscar archivos sin grupo válido
sudo find /tmp -nogroup -ls 2>/dev/null
```

### Ejercicio 4 — groupmod y archivos huérfanos

```bash
#!/bin/bash
# Crear archivos con un grupo, cambiar GID, ver huérfanos

sudo groupadd oldgroup
sudo useradd -m -s /bin/bash -G oldgroup orphanuser

# Crear archivo
sudo -u orphanuser sh -c 'touch /tmp/orphan.txt'
ls -la /tmp/orphan.txt

echo "=== Cambiar GID del grupo ==="
sudo groupmod -g 9999 oldgroup
ls -la /tmp/orphan.txt  # ¿Qué pasa con el grupo?

echo ""
echo "=== Buscar archivos huérfanos ==="
sudo find /tmp -nogroup -ls 2>/dev/null

# Limpiar
rm /tmp/orphan.txt
sudo userdel -r orphanuser
sudo groupdel oldgroup
```

### Ejercicio 5 — GID 0 (grupo root)

```bash
#!/bin/bash
# Intentar crear grupo con GID 0

echo "=== Intentando crear grupo con GID 0 ==="
sudo groupadd -g 0 roottest 2>&1

echo ""
echo "=== Verificar ==="
getent group 0
```

### Ejercicio 6 — gpasswd -M (set masivo)

```bash
#!/bin/bash
# Crear grupo con múltiples miembros usando -M

sudo groupadd team

# Crear usuarios
sudo useradd -m -s /bin/bash -G team user1
sudo useradd -m -s /bin/bash -G team user2
sudo useradd -m -s /bin/bash -G team user3

# Establecer lista completa (reemplaza)
sudo gpasswd -M user1,user2,user3 team

echo "=== Miembros del grupo ==="
getent group team

# Quitar user2 de la lista completa
sudo gpasswd -M user1,user3 team
getent group team

# Limpiar
sudo userdel -r user1 user2 user3
sudo groupdel team
```

### Ejercicio 7 — Grupo como administrador

```bash
#!/bin/bash
# Usar gpasswd -A para dar permisos de administrador de grupo a un usuario

sudo groupadd admingrp
sudo useradd -m -s /bin/bash adminuser
sudo useradd -m -s /bin/bash regularuser

# adminuser puede administrar el grupo sin sudo
sudo gpasswd -A adminuser admingrp
sudo gpasswd -a regularuser admingrp

# Verificar que quedó configurado
getent group admingrp

echo ""
echo "=== Campo admin en /etc/gshadow ==="
sudo grep '^admingrp:' /etc/gshadow

# Limpiar
sudo userdel -r adminuser regularuser
sudo groupdel admingrp
```

### Ejercicio 8 — addgroup vs groupadd (Debian)

```bash
#!/bin/bash
# Comparar comportamiento de adduser/addgroup (Debian) vs useradd/groupadd

# adduser/addgroup en Debian son interactivos, pero con --system actúan como groupadd
echo "=== addgroup --system ==="
sudo addgroup --system sysgrp 2>&1 || echo "addgroup no disponible o error"

getent group sysgrp 2>/dev/null && echo "Grupo creado"

# Limpiar si existe
sudo groupdel sysgrp 2>/dev/null
```

### Ejercicio 9 — Directorio compartido sin SGID (para comparar)

```bash
#!/bin/bash
# Crear directorio compartido SIN SGID y mostrar el problema

sudo groupadd proj1
sudo mkdir /tmp/proj1_no_sgid
sudo chown root:proj1 /tmp/proj1_no_sgid
sudo chmod 775 /tmp/proj1_no_sgid

# Agregar usuarios
sudo useradd -m -s /bin/bash -G proj1 userA
sudo useradd -m -s /bin/bash -G proj1 userB

# userA crea archivo
sudo -u userA sh -c 'touch /tmp/proj1_no_sgid/fileA.txt'
ls -la /tmp/proj1_no_sgid/fileA.txt
# El grupo es... ¿proj1 o el primario de userA?

# Limpiar
rm -rf /tmp/proj1_no_sgid
sudo userdel -r userA userB
sudo groupdel proj1
```

### Ejercicio 10 — Verificar rango de GIDs

```bash
#!/bin/bash
# Mostrar los rangos de GID del sistema

echo "=== GIDs de sistema (100-999) ==="
getent group | awk -F: '$3>=100 && $3<1000 {print $1": "$3}' | sort -t: -k2 -n | head -10

echo ""
echo "=== GIDs regulares (1000+) ==="
getent group | awk -F: '$3>=1000 {print $1": "$3}' | sort -t: -k2 -n | head -10

echo ""
echo "=== Último GID usado ==="
getent group | awk -F: '{print $3}' | sort -n | tail -1
```
