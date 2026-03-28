# Lab — groupadd, groupmod, groupdel

## Objetivo

Practicar el ciclo de vida completo de grupos: crear con groupadd,
modificar con groupmod (renombrar, cambiar GID), configurar
directorios compartidos con SGID, gestionar miembros con gpasswd,
y eliminar con groupdel.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — groupadd y groupmod

### Objetivo

Crear grupos regulares y de sistema, renombrar, y entender las
implicaciones de cambiar el GID.

### Paso 1.1: Crear grupo regular

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear grupo ==="
groupadd webteam

echo ""
echo "--- Verificar ---"
getent group webteam
echo "GID asignado: $(getent group webteam | cut -d: -f3)"
'
```

Sin flags, `groupadd` asigna el siguiente GID disponible en el
rango regular (>= 1000).

### Paso 1.2: Crear grupo de sistema

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear grupo de sistema ==="
groupadd -r svcgroup

echo ""
echo "--- Verificar ---"
getent group svcgroup
echo "GID asignado: $(getent group svcgroup | cut -d: -f3)"
echo "(debe ser < 1000)"
'
```

`-r` crea un grupo de sistema con GID en el rango bajo (< 1000).

### Paso 1.3: Crear con GID especifico

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear con GID 5000 ==="
groupadd -g 5000 shared-data

echo ""
getent group shared-data
'
```

### Paso 1.4: Renombrar grupo

```bash
docker compose exec debian-dev bash -c '
echo "=== Renombrar webteam → dev-team ==="
groupmod -n dev-team webteam

echo ""
echo "--- Verificar ---"
getent group dev-team
echo ""
echo "El GID no cambio — los archivos siguen funcionando"
'
```

Renombrar es seguro: el GID numerico no cambia, asi que los archivos
que pertenecen a este grupo no se ven afectados.

### Paso 1.5: Cambiar GID (peligroso)

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear archivo con grupo shared-data ==="
touch /tmp/gid-test
chown root:shared-data /tmp/gid-test
ls -la /tmp/gid-test

echo ""
echo "=== Cambiar GID de shared-data ==="
OLD_GID=$(getent group shared-data | cut -d: -f3)
groupmod -g 6000 shared-data
echo "GID cambiado de $OLD_GID a $(getent group shared-data | cut -d: -f3)"

echo ""
echo "=== El archivo ahora muestra el GID numerico viejo ==="
ls -la /tmp/gid-test
echo "(ya no hay grupo con GID $OLD_GID)"

echo ""
echo "=== Corregir: actualizar archivos con el GID viejo ==="
find /tmp -gid $OLD_GID -exec chgrp shared-data {} \; 2>/dev/null
ls -la /tmp/gid-test

rm -f /tmp/gid-test
'
```

Cambiar el GID es peligroso: los archivos quedan huerfanos con el
GID viejo. Hay que buscarlos y actualizarlos manualmente.

---

## Parte 2 — Directorio compartido

### Objetivo

Configurar un directorio de trabajo compartido usando grupo + SGID.

### Paso 2.1: Configurar directorio con SGID

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear usuarios ==="
useradd -m -s /bin/bash alice
useradd -m -s /bin/bash bob

echo ""
echo "=== Agregar al grupo ==="
usermod -aG dev-team alice
usermod -aG dev-team bob

echo ""
echo "=== Crear directorio compartido ==="
mkdir -p /srv/project
chown root:dev-team /srv/project
chmod 2775 /srv/project

echo ""
echo "--- Verificar ---"
ls -ld /srv/project
echo "(la s en group indica SGID activo)"
'
```

El SGID (2 en 2775) hace que los archivos creados dentro hereden
el grupo del directorio, no el grupo primario del usuario.

### Paso 2.2: Probar herencia de grupo

```bash
docker compose exec debian-dev bash -c '
echo "=== alice crea un archivo ==="
su -c "touch /srv/project/design.txt" alice 2>/dev/null || { touch /srv/project/design.txt; chown alice /srv/project/design.txt; }
ls -la /srv/project/design.txt
echo "(grupo = dev-team gracias al SGID)"

echo ""
echo "=== bob crea un archivo ==="
su -c "touch /srv/project/code.py" bob 2>/dev/null || { touch /srv/project/code.py; chown bob /srv/project/code.py; }
ls -la /srv/project/code.py
echo "(grupo = dev-team, no el grupo primario de bob)"
'
```

### Paso 2.3: Subdirectorios heredan SGID

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear subdirectorio ==="
mkdir /srv/project/docs
ls -ld /srv/project/docs
echo "(el subdirectorio tambien tiene SGID = s en group)"
'
```

Los subdirectorios creados dentro de un directorio con SGID
heredan automaticamente el bit SGID.

---

## Parte 3 — groupdel y gpasswd

### Objetivo

Gestionar miembros con gpasswd, entender las restricciones de
groupdel, y comparar entre distribuciones.

### Paso 3.1: gpasswd para gestion de miembros

```bash
docker compose exec debian-dev bash -c '
echo "=== Agregar con gpasswd -a ==="
gpasswd -a alice dev-team
echo "Miembros: $(getent group dev-team | cut -d: -f4)"

echo ""
echo "=== Quitar con gpasswd -d ==="
gpasswd -d alice dev-team
echo "Miembros: $(getent group dev-team | cut -d: -f4)"

echo ""
echo "=== Establecer lista completa con -M ==="
gpasswd -M alice,bob dev-team
echo "Miembros: $(getent group dev-team | cut -d: -f4)"
'
```

`gpasswd -d` es la unica forma de quitar un usuario de un solo
grupo sin afectar sus otras membresías. `usermod` no tiene esa
capacidad.

### Paso 3.2: groupdel y restricciones

```bash
docker compose exec debian-dev bash -c '
echo "=== Intentar eliminar grupo primario de alice ==="
PRIMARY=$(id -gn alice)
groupdel "$PRIMARY" 2>&1 || true
echo "(no se puede eliminar un grupo primario activo)"

echo ""
echo "=== Eliminar grupo que NO es primario de nadie ==="
groupdel svcgroup
echo "svcgroup eliminado: $(getent group svcgroup 2>/dev/null || echo 'no existe')"
'
```

`groupdel` se niega a eliminar un grupo si es el grupo primario
de algun usuario. Hay que cambiar el grupo primario primero.

### Paso 3.3: Archivos huerfanos

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear archivo con grupo shared-data ==="
touch /tmp/orphan-test
chown root:shared-data /tmp/orphan-test
ls -la /tmp/orphan-test

echo ""
echo "=== Eliminar el grupo ==="
groupdel shared-data

echo ""
echo "=== El archivo muestra GID numerico ==="
ls -la /tmp/orphan-test

echo ""
echo "=== Buscar archivos sin grupo valido ==="
find /tmp -nogroup 2>/dev/null | head -5

rm -f /tmp/orphan-test
'
```

Al eliminar un grupo, los archivos que le pertenecian muestran el
GID numerico en lugar del nombre.

### Paso 3.4: Comparar Debian vs AlmaLinux

```bash
echo "=== Grupo de sudo ==="
echo "Debian:"
docker compose exec debian-dev bash -c 'getent group sudo 2>/dev/null || echo "no existe"'
echo "AlmaLinux:"
docker compose exec alma-dev bash -c 'getent group wheel 2>/dev/null || echo "no existe"'

echo ""
echo "=== Grupo de nobody ==="
echo "Debian:"
docker compose exec debian-dev bash -c 'getent group nogroup 2>/dev/null || echo "no existe"'
echo "AlmaLinux:"
docker compose exec alma-dev bash -c 'getent group nobody 2>/dev/null || echo "no existe"'
```

Debian usa `sudo` (GID 27) y `nogroup` (GID 65534). AlmaLinux usa
`wheel` (GID 10) y `nobody` (GID 65534).

---

## Limpieza final

```bash
docker compose exec debian-dev bash -c '
userdel -r alice 2>/dev/null
userdel -r bob 2>/dev/null
groupdel dev-team 2>/dev/null
groupdel shared-data 2>/dev/null
groupdel svcgroup 2>/dev/null
rm -rf /srv/project 2>/dev/null
echo "Limpieza completada"
'
```

---

## Conceptos reforzados

1. `groupadd` crea grupos. `-r` para sistema (GID bajo), `-g` para
   GID especifico.

2. `groupmod -n` renombra (seguro, no afecta archivos). `groupmod -g`
   cambia GID (peligroso, archivos quedan huerfanos).

3. SGID en directorios (chmod 2775) hace que archivos nuevos hereden
   el grupo del directorio. Patron estandar para trabajo compartido.

4. `gpasswd -d` quita un usuario de un solo grupo. `gpasswd -M`
   establece la lista completa de miembros.

5. `groupdel` no puede eliminar un grupo primario activo. Los
   archivos del grupo eliminado muestran el GID numerico.
