# Lab — useradd, usermod, userdel

## Objetivo

Crear, modificar y eliminar usuarios con las herramientas de bajo
nivel. Explorar los defaults de useradd, el directorio skeleton, y
practicar las operaciones mas comunes incluyendo el error clasico
de -G sin -a.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — useradd

### Objetivo

Crear usuarios con diferentes configuraciones y entender los defaults.

### Paso 1.1: Defaults de useradd

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

### Paso 1.2: Crear usuario minimo

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
explicitamente para portabilidad.

### Paso 1.3: Crear usuario completo

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

### Paso 1.4: El directorio skeleton

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

### Paso 1.5: Crear usuario con grupos suplementarios

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

### Paso 1.6: Diferencia adduser vs useradd

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

## Parte 2 — usermod

### Objetivo

Modificar usuarios existentes: shell, grupos, bloqueo, y entender
el error comun de -G sin -a.

### Paso 2.1: Cambiar shell

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

### Paso 2.2: Agregar a grupos — el error clasico

```bash
docker compose exec debian-dev bash -c '
echo "=== Grupos actuales de labuser ==="
id labuser

echo ""
echo "=== CORRECTO: -aG agrega SIN quitar ==="
usermod -aG labgroup labuser
id labuser
echo "(ahora tiene labgroup ademas de los anteriores)"

echo ""
echo "=== INCORRECTO: -G sin -a REEMPLAZA todos los grupos ==="
echo "(no ejecutar en produccion — solo demostracion)"
usermod -G labgroup labuser
id labuser
echo "(perdio todos los grupos suplementarios excepto labgroup)"

echo ""
echo "=== Restaurar ==="
usermod -aG labgroup labuser
id labuser
'
```

`-G` sin `-a` **reemplaza** todos los grupos suplementarios. Es uno
de los errores mas frecuentes en administracion Linux. Siempre usar
`-aG` para agregar.

### Paso 2.3: Bloquear y desbloquear

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
echo "(el ! desaparecio)"
'
```

`-L` bloquea anteponiendo `!` al hash. `-U` desbloquea quitandolo.
La contrasena original se preserva.

### Paso 2.4: Cambiar UID

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear archivo como labuser ==="
su -c "touch /tmp/labuser-file" labuser 2>/dev/null || touch /tmp/labuser-file && chown labuser /tmp/labuser-file
ls -ln /tmp/labuser-file

echo ""
echo "=== Cambiar UID ==="
OLD_UID=$(id -u labuser)
usermod -u 3001 labuser
echo "UID cambiado de $OLD_UID a $(id -u labuser)"

echo ""
echo "=== El archivo en /tmp sigue con el UID viejo ==="
ls -ln /tmp/labuser-file
echo "(muestra el UID numerico porque ya no coincide)"

echo ""
echo "=== Archivos en home SI se actualizan ==="
ls -ln /home/labuser/ | head -3

rm -f /tmp/labuser-file
'
```

`usermod -u` actualiza los archivos dentro del home pero NO en otros
directorios. Hay que buscar y corregir manualmente con `find -uid`.

---

## Parte 3 — userdel y comparacion

### Objetivo

Eliminar usuarios, buscar archivos huerfanos, y comparar el
comportamiento entre Debian y AlmaLinux.

### Paso 3.1: userdel sin -r

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

### Paso 3.2: userdel -r

```bash
docker compose exec debian-dev bash -c '
echo "=== Eliminar labuser con -r ==="
userdel -r labuser

echo ""
echo "--- Home eliminado ---"
ls -ld /home/labuser 2>/dev/null || echo "home ya no existe"

echo ""
echo "--- Buscar archivos huerfanos ---"
find /tmp -nouser 2>/dev/null | head -5 || echo "(ningun archivo huerfano en /tmp)"
'
```

`-r` elimina el home y el mail spool. Pero NO busca archivos del
usuario en otros directorios. Usar `find / -nouser` despues de
eliminar.

### Paso 3.3: Crear y eliminar en AlmaLinux

```bash
docker compose exec alma-dev bash -c '
echo "=== Crear usuario en AlmaLinux ==="
useradd -m -s /bin/bash -c "RHEL Test" rheltest

echo ""
echo "--- Verificar ---"
getent passwd rheltest
id rheltest
ls -ld /home/rheltest

echo ""
echo "=== Eliminar ==="
userdel -r rheltest
echo "Eliminado: $(getent passwd rheltest 2>/dev/null || echo 'no existe')"
'
```

### Paso 3.4: Comparar defaults

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

Las diferencias principales: Debian no crea home por defecto,
usa yescrypt, rango de sistema desde 100. AlmaLinux crea home,
usa SHA-512, rango de sistema desde 201.

---

## Limpieza final

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

## Conceptos reforzados

1. `useradd -m -s /bin/bash` es la forma portable de crear usuarios.
   Sin `-m`, Debian no crea el home. Siempre especificar explicitamente.

2. `usermod -aG` agrega a un grupo SIN quitar los existentes.
   `-G` sin `-a` **reemplaza** todos los grupos suplementarios.

3. `userdel -r` elimina home y mail spool, pero hay que buscar
   archivos huerfanos en otros directorios con `find / -nouser`.

4. Los defaults de `useradd` se configuran en `/etc/default/useradd`
   y `/etc/login.defs`. Difieren entre Debian y RHEL.

5. El directorio `/etc/skel/` define el contenido inicial del home
   de nuevos usuarios.
