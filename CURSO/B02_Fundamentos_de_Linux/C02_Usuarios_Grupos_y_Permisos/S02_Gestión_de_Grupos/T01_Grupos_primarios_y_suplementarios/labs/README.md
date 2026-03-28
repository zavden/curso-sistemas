# Lab — Grupos primarios y suplementarios

## Objetivo

Entender la diferencia entre grupo primario y suplementarios: como
se asignan, como afectan la creacion de archivos, y como el kernel
usa los grupos para verificar permisos.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Inspeccionar grupos

### Objetivo

Identificar el grupo primario y los suplementarios de un usuario.

### Paso 1.1: Ver todos los grupos

```bash
docker compose exec debian-dev bash -c '
echo "=== id (informacion completa) ==="
id dev

echo ""
echo "=== Desglose ==="
echo "UID:             $(id -u dev)"
echo "GID primario:    $(id -g dev) ($(id -gn dev))"
echo "Todos los GIDs:  $(id -G dev)"
echo "Todos los nombres: $(id -Gn dev)"
'
```

`gid=` muestra el grupo primario. `groups=` lista el primario
(siempre primero) y los suplementarios.

### Paso 1.2: Grupo primario en /etc/passwd

```bash
docker compose exec debian-dev bash -c '
echo "=== Campo GID en /etc/passwd ==="
getent passwd dev
echo ""
GID=$(getent passwd dev | cut -d: -f4)
echo "GID primario: $GID"
echo "Nombre del grupo: $(getent group $GID | cut -d: -f1)"
'
```

El grupo primario se define en el campo 4 de `/etc/passwd`. Se
resuelve a un nombre a traves de `/etc/group`.

### Paso 1.3: Suplementarios en /etc/group

```bash
docker compose exec debian-dev bash -c '
echo "=== Buscar dev en /etc/group ==="
grep "dev" /etc/group

echo ""
echo "=== Observa: dev NO aparece en su propio grupo ==="
getent group dev
echo "(campo 4 vacio — dev es miembro implicito por su GID primario)"

echo ""
echo "=== Pero SI aparece en los suplementarios ==="
grep "dev" /etc/group | grep -v "^dev:"
'
```

El campo 4 de `/etc/group` solo lista miembros **suplementarios**.
Los miembros primarios no aparecen ahi — hay que usar `id`.

### Paso 1.4: User Private Group (UPG)

```bash
docker compose exec debian-dev bash -c '
echo "=== USERGROUPS_ENAB ==="
grep USERGROUPS_ENAB /etc/login.defs

echo ""
echo "=== Al crear un usuario, se crea un grupo con su nombre ==="
useradd -m testupg
echo "Usuario: $(getent passwd testupg | cut -d: -f1)"
echo "GID primario: $(id -g testupg)"
echo "Grupo: $(id -gn testupg)"
echo "(mismo nombre que el usuario = User Private Group)"

userdel -r testupg
'
```

UPG (User Private Group) es el esquema por defecto: cada usuario
tiene su propio grupo privado como grupo primario.

### Paso 1.5: Comparar con AlmaLinux

```bash
echo "=== Debian ==="
docker compose exec debian-dev id dev

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev id dev
```

Ambas distribuciones usan UPG por defecto. Las diferencias estan
en los GIDs asignados y los nombres de ciertos grupos (sudo vs wheel).

---

## Parte 2 — Efecto en archivos

### Objetivo

Demostrar como el grupo primario determina el grupo propietario
de los archivos que un usuario crea.

### Paso 2.1: Crear archivos como dev

```bash
docker compose exec debian-dev bash -c '
echo "=== Grupo primario actual ==="
id -gn dev

echo ""
echo "=== Crear archivo ==="
su -c "touch /tmp/group-test" dev 2>/dev/null || { touch /tmp/group-test; chown dev /tmp/group-test; }
ls -la /tmp/group-test
echo ""
echo "(el grupo propietario es el grupo primario de dev)"

rm -f /tmp/group-test
'
```

El grupo propietario del archivo es siempre el grupo primario
efectivo del usuario que lo crea.

### Paso 2.2: Crear con grupo diferente

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear grupo de prueba ==="
groupadd -f labteam
usermod -aG labteam dev

echo ""
echo "=== Archivo normal (grupo primario) ==="
touch /tmp/normal-file
chown dev:dev /tmp/normal-file
ls -la /tmp/normal-file

echo ""
echo "=== Archivo con chgrp (grupo suplementario) ==="
touch /tmp/team-file
chown dev:labteam /tmp/team-file
ls -la /tmp/team-file

rm -f /tmp/normal-file /tmp/team-file
'
```

Para crear un archivo con un grupo diferente al primario, se puede
usar `chgrp`, `chown :grupo`, o `newgrp` (cubierto en T03).

### Paso 2.3: Grupos en procesos

```bash
docker compose exec debian-dev bash -c '
echo "=== GIDs del proceso actual ==="
cat /proc/self/status | grep -i groups

echo ""
echo "=== Explicacion ==="
echo "El campo Groups muestra los GIDs del proceso (primario + suplementarios)"
echo "Estos se establecen al hacer login y no cambian durante la sesion"
'
```

Los grupos se fijan al iniciar la sesion. Cambios con `usermod -aG`
requieren re-login para activarse.

---

## Parte 3 — Permisos y membresia

### Objetivo

Demostrar como el kernel usa los grupos para verificar permisos
y como gestionar membresias.

### Paso 3.1: Verificacion de permisos

```bash
docker compose exec debian-dev bash -c '
echo "=== Orden de verificacion del kernel ==="
echo "1. UID del proceso == UID del archivo → permisos de owner"
echo "2. GID (primario o suplementario) == GID del archivo → permisos de group"
echo "3. Ninguno coincide → permisos de others"

echo ""
echo "=== Ejemplo practico ==="
groupadd -f testaccess
useradd -m -s /bin/bash -G testaccess member1
useradd -m -s /bin/bash outsider1

echo "--- Archivo con grupo testaccess ---"
touch /tmp/access-test
chown root:testaccess /tmp/access-test
chmod 640 /tmp/access-test
ls -la /tmp/access-test

echo ""
echo "member1 (en testaccess): tiene permisos de group (r--)"
echo "outsider1 (no en testaccess): tiene permisos de others (---)"

rm -f /tmp/access-test
userdel -r member1
userdel -r outsider1
groupdel testaccess
'
```

El kernel no acumula permisos. Usa la **primera** categoria que
coincide y se detiene ahi.

### Paso 3.2: Agregar y quitar de grupos

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear grupo y usuario ==="
groupadd -f labdev
useradd -m -s /bin/bash grptest

echo ""
echo "=== Agregar con gpasswd -a ==="
gpasswd -a grptest labdev
echo "Grupos: $(id -Gn grptest)"

echo ""
echo "=== Quitar con gpasswd -d ==="
gpasswd -d grptest labdev
echo "Grupos: $(id -Gn grptest)"

echo ""
echo "=== Agregar con usermod -aG ==="
usermod -aG labdev grptest
echo "Grupos: $(id -Gn grptest)"

userdel -r grptest
groupdel labdev
'
```

`gpasswd -a` agrega a un grupo, `gpasswd -d` quita de un grupo.
`usermod -aG` tambien agrega. Para quitar, usar `gpasswd -d`
(usermod no tiene opcion para quitar de un solo grupo).

### Paso 3.3: Ver miembros de un grupo

```bash
docker compose exec debian-dev bash -c '
echo "=== Miembros de un grupo ==="
for g in root $(getent group | cut -d: -f1 | head -10); do
    members=$(getent group "$g" | cut -d: -f4)
    [ -n "$members" ] && echo "$g: $members"
done

echo ""
echo "(solo muestra miembros suplementarios — los primarios no aparecen)"
'
```

---

## Limpieza final

```bash
docker compose exec debian-dev bash -c '
groupdel labteam 2>/dev/null
groupdel labdev 2>/dev/null
groupdel testaccess 2>/dev/null
userdel -r testupg 2>/dev/null
userdel -r grptest 2>/dev/null
userdel -r member1 2>/dev/null
userdel -r outsider1 2>/dev/null
echo "Limpieza completada"
'
```

---

## Conceptos reforzados

1. Cada usuario tiene exactamente **un grupo primario** (campo GID
   de /etc/passwd) y cero o mas **grupos suplementarios** (campo 4
   de /etc/group).

2. El grupo primario determina el grupo propietario de los archivos
   que el usuario crea.

3. El kernel verifica permisos en orden: owner → group → others.
   Usa la primera coincidencia y se detiene. Los grupos suplementarios
   se verifican junto con el primario.

4. `gpasswd -d` es la forma correcta de quitar un usuario de un
   solo grupo sin afectar el resto de sus membresías.

5. Los cambios en grupos no se aplican a sesiones activas. Se
   requiere re-login o `newgrp` para activarlos.
