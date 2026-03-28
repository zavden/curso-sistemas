# T02 — groupadd, groupmod, groupdel

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
| `-f` | No error si el grupo ya existe; con `-g`, elige otro GID si hay conflicto | `groupadd -f developers` |

```bash
# Grupo de sistema (GID en rango 100-999 en Debian, 201-999 en RHEL)
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

### Renombrar un grupo

Renombrar es **seguro** — no afecta la propiedad de archivos porque el GID
numérico no cambia:

```bash
sudo groupmod -n dev-team developers

# Los archivos siguen funcionando — mismo GID, diferente nombre
ls -la /srv/web
# drwxrwsr-x 2 root dev-team ... /srv/web
```

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
requiere planificación. Es análogo a `usermod -u` que tampoco re-indexa
archivos fuera del home.

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

`groupdel` no busca ni limpia archivos del grupo eliminado. Hay que usar
`find / -nogroup` después de eliminar, igual que con `userdel` y `find / -nouser`.

## gpasswd — Gestión de membresía y administración

`gpasswd` es la herramienta específica para gestionar miembros y
administradores de grupos:

```bash
# Agregar usuario a un grupo
sudo gpasswd -a alice developers
# Adding user alice to group developers

# Quitar usuario de un grupo
sudo gpasswd -d alice developers
# Removing user alice from group developers

# Establecer la lista completa de miembros (REEMPLAZA todos los existentes)
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

### gpasswd -A y /etc/gshadow

El administrador de grupo se registra en `/etc/gshadow`:

```bash
sudo grep '^developers:' /etc/gshadow
# developers:!:alice:alice,bob,charlie
#                ^         ^
#           admin(s)    miembros
```

El administrador del grupo puede ejecutar `gpasswd -a` y `gpasswd -d` sin
sudo. Esto es útil para delegar la gestión de membresías a un líder de equipo.

### gpasswd vs usermod para membresías

| Operación | gpasswd | usermod |
|---|---|---|
| Agregar a un grupo | `gpasswd -a user group` | `usermod -aG group user` |
| Quitar de un grupo | `gpasswd -d user group` | No puede (solo reemplazar toda la lista) |
| Establecer miembros | `gpasswd -M u1,u2 group` | No aplica |
| Asignar admin | `gpasswd -A user group` | No aplica |

Nota: el orden de argumentos es diferente: `gpasswd -a USER GROUP` vs
`usermod -aG GROUP USER`.

## grpck — Verificar consistencia

`grpck` verifica la integridad de `/etc/group` y `/etc/gshadow`:

```bash
sudo grpck
# (sin salida = todo correcto)

# En modo solo-lectura (no repara)
sudo grpck -r
```

Detecta: entradas duplicadas, GIDs inválidos, campos malformados, y
referencias a usuarios que no existen.

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

# alice crea un archivo:
touch /srv/project-x/design.txt
ls -la /srv/project-x/design.txt
# -rw-r--r-- 1 alice project-x ... design.txt
#                     ^^^^^^^^^
# Gracias al SGID, el grupo es project-x (no el grupo primario de alice)
```

Sin SGID (chmod 775), los archivos se crearían con el grupo primario de cada
usuario, rompiendo la colaboración. El SGID en directorios se explica en
detalle en S03 (Permisos).

## Diferencias entre distribuciones

| Aspecto | Debian/Ubuntu | AlmaLinux/RHEL |
|---|---|---|
| Rango GID de sistema | 100-999 | 201-999 |
| Grupo de sudo | `sudo` (GID 27) | `wheel` (GID 10) |
| GID de nobody | `nogroup` (65534) | `nobody` (65534) |
| `addgroup` | Existe (script Perl del paquete `adduser`) | No existe |
| `delgroup` | Existe (script Perl del paquete `adduser`) | No existe |

```bash
# Debian: addgroup es un wrapper interactivo
sudo addgroup --system myservice
# Adding group `myservice' (GID 998) ...

# RHEL: addgroup no existe — usar groupadd directamente
sudo groupadd -r myservice
```

> **Nota**: En RHEL, `adduser` es symlink a `useradd`, pero `addgroup` y
> `delgroup` simplemente **no existen**. Para scripts portables, usar siempre
> `groupadd`/`groupdel`.

## Tabla: ciclo de vida de un grupo

| Operación | Comando | Restricción |
|---|---|---|
| Crear | `groupadd name` | Nombre único |
| Renombrar | `groupmod -n new old` | GID no cambia (seguro) |
| Cambiar GID | `groupmod -g N name` | No re-indexa archivos (peligroso) |
| Agregar miembro | `gpasswd -a user group` o `usermod -aG group user` | |
| Quitar miembro | `gpasswd -d user group` | |
| Verificar integridad | `grpck` | |
| Eliminar | `groupdel name` | No debe ser primario de nadie |

---

## Labs

### Parte 1 — groupadd y groupmod

#### Paso 1.1: Crear grupo regular

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

#### Paso 1.2: Crear grupo de sistema

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

#### Paso 1.3: Crear con GID específico

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear con GID 5000 ==="
groupadd -g 5000 shared-data

echo ""
getent group shared-data
'
```

#### Paso 1.4: Renombrar grupo

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

Renombrar es seguro: el GID numérico no cambia, así que los archivos
que pertenecen a este grupo no se ven afectados.

#### Paso 1.5: Cambiar GID (peligroso)

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

Cambiar el GID es peligroso: los archivos quedan huérfanos con el
GID viejo. Hay que buscarlos y actualizarlos manualmente.

### Parte 2 — Directorio compartido

#### Paso 2.1: Configurar directorio con SGID

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

#### Paso 2.2: Probar herencia de grupo

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

#### Paso 2.3: Subdirectorios heredan SGID

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear subdirectorio ==="
mkdir /srv/project/docs
ls -ld /srv/project/docs
echo "(el subdirectorio tambien tiene SGID = s en group)"
'
```

Los subdirectorios creados dentro de un directorio con SGID
heredan automáticamente el bit SGID.

### Parte 3 — groupdel y gpasswd

#### Paso 3.1: gpasswd para gestión de miembros

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

`gpasswd -d` es la única forma de quitar un usuario de un solo
grupo sin afectar sus otras membresías.

#### Paso 3.2: groupdel y restricciones

```bash
docker compose exec debian-dev bash -c '
echo "=== Intentar eliminar grupo primario de alice ==="
PRIMARY=$(id -gn alice)
groupdel "$PRIMARY" 2>&1 || true
echo "(no se puede eliminar un grupo primario activo)"

echo ""
echo "=== Eliminar grupo que NO es primario de nadie ==="
groupdel svcgroup
echo "svcgroup eliminado: $(getent group svcgroup 2>/dev/null || echo no existe)"
'
```

#### Paso 3.3: Archivos huérfanos

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

#### Paso 3.4: Comparar Debian vs AlmaLinux

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

### Limpieza

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

## Ejercicios

### Ejercicio 1 — Ciclo de vida de un grupo

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear grupo ==="
groupadd labteam
getent group labteam

echo ""
echo "=== Agregar miembro ==="
useradd -m -s /bin/bash labmember
gpasswd -a labmember labteam
getent group labteam

echo ""
echo "=== Renombrar ==="
groupmod -n lab-team labteam
getent group lab-team
'
```

**Pregunta**: Después de renombrar `labteam` a `lab-team`, ¿cambia el GID?
¿Los archivos que pertenecían a `labteam` siguen accesibles?

<details><summary>Predicción</summary>

El GID **no cambia**. `groupmod -n` solo modifica el nombre en `/etc/group`,
el GID numérico se mantiene idéntico. Como el filesystem almacena GIDs
numéricos (no nombres), todos los archivos siguen funcionando sin
ningún cambio.

```
Antes: labteam:x:1001:labmember
Después: lab-team:x:1001:labmember
                   ^^^^
                   mismo GID
```

Renombrar es la operación más segura de `groupmod`. Cambiar el GID
(`groupmod -g`) sí dejaría archivos huérfanos.

</details>

```bash
docker compose exec debian-dev bash -c '
userdel -r labmember
groupdel lab-team
'
```

### Ejercicio 2 — Cambiar GID y archivos huérfanos

```bash
docker compose exec debian-dev bash -c '
groupadd -g 5000 testgid
touch /tmp/gid-file
chown root:testgid /tmp/gid-file
echo "=== Antes del cambio ==="
ls -la /tmp/gid-file

echo ""
echo "=== Cambiar GID de 5000 a 6000 ==="
groupmod -g 6000 testgid

echo ""
echo "=== Después del cambio ==="
ls -la /tmp/gid-file
'
```

**Pregunta**: ¿Qué mostrará `ls -la` para el grupo de `/tmp/gid-file`
después de cambiar el GID del grupo? ¿El nombre `testgid` o un número?

<details><summary>Predicción</summary>

Mostrará el **número `5000`** en lugar del nombre `testgid`:

```
Antes:   -rw-r--r-- 1 root testgid ... /tmp/gid-file
Después: -rw-r--r-- 1 root 5000    ... /tmp/gid-file
```

El archivo sigue teniendo GID 5000 en su inode, pero ya no existe ningún grupo
con GID 5000 (ahora `testgid` tiene GID 6000). Cuando `ls` no puede resolver
un GID a un nombre, muestra el número.

Para corregir: `find /tmp -gid 5000 -exec chgrp testgid {} \;`

</details>

```bash
docker compose exec debian-dev bash -c '
rm -f /tmp/gid-file
groupdel testgid
'
```

### Ejercicio 3 — groupdel y grupo primario

```bash
docker compose exec debian-dev bash -c '
groupadd cantdelete
useradd -m -s /bin/bash -g cantdelete blockuser

echo "=== blockuser tiene cantdelete como grupo primario ==="
id blockuser

echo ""
echo "=== Intentar eliminar el grupo ==="
groupdel cantdelete 2>&1 || true
'
```

**Pregunta**: ¿Se puede eliminar `cantdelete`? ¿Qué pasos hay que seguir
para poder eliminarlo?

<details><summary>Predicción</summary>

**No se puede eliminar** directamente. `groupdel` rechaza la operación:

```
groupdel: cannot remove the primary group of user 'blockuser'
```

Para poder eliminar el grupo hay dos caminos:
1. Eliminar el usuario primero: `userdel -r blockuser`, luego `groupdel cantdelete`
2. Cambiar el grupo primario del usuario: `usermod -g users blockuser`, luego `groupdel cantdelete`

`groupdel` protege contra dejar usuarios sin grupo primario, lo cual
corrompería su configuración.

</details>

```bash
docker compose exec debian-dev bash -c '
userdel -r blockuser
groupdel cantdelete 2>/dev/null
'
```

### Ejercicio 4 — gpasswd -M reemplaza miembros

```bash
docker compose exec debian-dev bash -c '
groupadd settest
useradd -m -s /bin/bash u1
useradd -m -s /bin/bash u2
useradd -m -s /bin/bash u3

echo "=== Agregar u1 y u2 con gpasswd -a ==="
gpasswd -a u1 settest
gpasswd -a u2 settest
getent group settest

echo ""
echo "=== Usar gpasswd -M u2,u3 ==="
gpasswd -M u2,u3 settest
getent group settest
'
```

**Pregunta**: Después de `gpasswd -M u2,u3`, ¿quién sigue en el grupo?
¿Qué pasó con `u1`?

<details><summary>Predicción</summary>

El grupo tendrá **solo u2 y u3**. `u1` fue **eliminado** del grupo.

```
Antes de -M:   settest:x:XXXX:u1,u2
Después de -M: settest:x:XXXX:u2,u3
```

`gpasswd -M` **reemplaza** la lista completa de miembros, no agrega. Es el
equivalente grupal de `usermod -G` sin `-a`. Útil para establecer la membresía
exacta de un grupo de una sola vez, pero peligroso si se olvida incluir
miembros existentes.

</details>

```bash
docker compose exec debian-dev bash -c '
userdel -r u1; userdel -r u2; userdel -r u3
groupdel settest
'
```

### Ejercicio 5 — Delegación con gpasswd -A

```bash
docker compose exec debian-dev bash -c '
groupadd delegated
useradd -m -s /bin/bash teamlead
useradd -m -s /bin/bash newmember

echo "=== Asignar teamlead como admin del grupo ==="
gpasswd -A teamlead delegated

echo ""
echo "=== Ver /etc/gshadow ==="
grep "^delegated:" /etc/gshadow

echo ""
echo "=== teamlead agrega a newmember SIN sudo ==="
su -c "gpasswd -a newmember delegated" teamlead
getent group delegated
'
```

**Pregunta**: ¿Cómo puede `teamlead` ejecutar `gpasswd -a` sin ser root?
¿Dónde se registra esta delegación?

<details><summary>Predicción</summary>

`teamlead` puede administrar el grupo porque fue asignado como **administrador
del grupo** con `gpasswd -A`. Esta información se registra en `/etc/gshadow`:

```
delegated:!:teamlead:newmember
              ^          ^
         admin(s)    miembros
```

El campo 3 de `/etc/gshadow` lista los administradores del grupo. Un admin
puede:
- Agregar miembros: `gpasswd -a user group`
- Quitar miembros: `gpasswd -d user group`
- Establecer contraseña de grupo: `gpasswd group`

**No** puede renombrar, cambiar GID, ni eliminar el grupo (esas operaciones
requieren root).

</details>

```bash
docker compose exec debian-dev bash -c '
userdel -r teamlead; userdel -r newmember
groupdel delegated
'
```

### Ejercicio 6 — SGID vs sin SGID

```bash
docker compose exec debian-dev bash -c '
groupadd proj1
useradd -m -s /bin/bash -G proj1 projuser

echo "=== Directorio SIN SGID ==="
mkdir /tmp/no_sgid
chown root:proj1 /tmp/no_sgid
chmod 775 /tmp/no_sgid
su -c "touch /tmp/no_sgid/file1.txt" projuser
ls -la /tmp/no_sgid/file1.txt

echo ""
echo "=== Directorio CON SGID ==="
mkdir /tmp/with_sgid
chown root:proj1 /tmp/with_sgid
chmod 2775 /tmp/with_sgid
su -c "touch /tmp/with_sgid/file2.txt" projuser
ls -la /tmp/with_sgid/file2.txt
'
```

**Pregunta**: ¿Qué grupo tendrá `file1.txt` (sin SGID) vs `file2.txt`
(con SGID)?

<details><summary>Predicción</summary>

- `file1.txt` (sin SGID): grupo = **`projuser`** (el grupo primario del usuario)
- `file2.txt` (con SGID): grupo = **`proj1`** (heredado del directorio)

```
/tmp/no_sgid/file1.txt    → -rw-r--r-- 1 projuser projuser ...
/tmp/with_sgid/file2.txt  → -rw-r--r-- 1 projuser proj1    ...
```

Sin SGID, el grupo del archivo es siempre el grupo primario del creador.
Con SGID, el archivo hereda el grupo del directorio padre. Por eso el
patrón estándar para directorios compartidos es `chmod 2775` — garantiza
que todos los archivos pertenezcan al grupo del proyecto, no al grupo
personal de cada usuario.

</details>

```bash
docker compose exec debian-dev bash -c '
rm -rf /tmp/no_sgid /tmp/with_sgid
userdel -r projuser
groupdel proj1
'
```

### Ejercicio 7 — grpck para verificar integridad

```bash
docker compose exec debian-dev bash -c '
echo "=== Verificar integridad de /etc/group ==="
grpck -r 2>&1 || true

echo ""
echo "=== Ver entradas con formato sospechoso ==="
awk -F: "NF!=4 {print NR\": \"\$0}" /etc/group
echo "(NF!=4 detecta lineas que no tienen exactamente 4 campos)"
'
```

**Pregunta**: ¿Cuántos campos debe tener cada línea de `/etc/group`?
¿Qué verifica `grpck`?

<details><summary>Predicción</summary>

Cada línea de `/etc/group` debe tener exactamente **4 campos** separados
por `:`:

```
nombre:contraseña:GID:miembros
```

`grpck` verifica:
- Que cada línea tenga exactamente 4 campos
- Que los nombres de grupo sean únicos
- Que los GIDs sean válidos (numéricos)
- Que los usuarios listados como miembros existan en `/etc/passwd`
- Consistencia entre `/etc/group` y `/etc/gshadow`

Con `-r` (read-only) solo reporta problemas sin corregirlos. Sin `-r`,
`grpck` ofrece corregir interactivamente los problemas encontrados.

Si no hay problemas, la salida estará vacía (exit 0).

</details>

### Ejercicio 8 — groupadd -f y duplicados

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear grupo ==="
groupadd duptest
getent group duptest

echo ""
echo "=== Intentar crear de nuevo SIN -f ==="
groupadd duptest 2>&1 || true

echo ""
echo "=== Intentar crear de nuevo CON -f ==="
groupadd -f duptest 2>&1 || true
echo "Exit code: $?"

echo ""
echo "=== GID no cambio ==="
getent group duptest
'
```

**Pregunta**: ¿Qué diferencia hay entre `groupadd duptest` (sin `-f`) y
`groupadd -f duptest` cuando el grupo ya existe?

<details><summary>Predicción</summary>

- **Sin `-f`**: error con exit code 9 y mensaje:
  ```
  groupadd: group 'duptest' already exists
  ```

- **Con `-f`**: **sin error**, exit code 0. El grupo existente no se modifica.

`-f` (force) es útil en scripts donde quieres garantizar que el grupo exista
sin que el script falle si ya fue creado. Es idempotente — ejecutar
`groupadd -f duptest` múltiples veces siempre da exit 0 sin efecto.

Además, si se usa `-f` con `-g GID` y el GID ya está en uso por otro grupo,
`-f` elige automáticamente otro GID libre en vez de fallar.

</details>

```bash
docker compose exec debian-dev bash -c 'groupdel duptest'
```

### Ejercicio 9 — Buscar archivos sin grupo válido

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear grupo, archivo, y eliminar grupo ==="
groupadd ephemeral
touch /tmp/ephemeral-file
chown root:ephemeral /tmp/ephemeral-file
ls -la /tmp/ephemeral-file
groupdel ephemeral

echo ""
echo "=== Buscar archivos sin grupo valido ==="
find /tmp -nogroup -ls 2>/dev/null

echo ""
echo "=== Reasignar a root ==="
find /tmp -nogroup -exec chgrp root {} \; 2>/dev/null
ls -la /tmp/ephemeral-file
'
```

**Pregunta**: ¿Por qué es importante ejecutar `find / -nogroup` después de
eliminar un grupo? ¿Qué muestra `ls` para un archivo sin grupo válido?

<details><summary>Predicción</summary>

Después de `groupdel`, `ls` muestra el **GID numérico** en vez del nombre:

```
Antes:   -rw-r--r-- 1 root ephemeral ... /tmp/ephemeral-file
Después: -rw-r--r-- 1 root 1001      ... /tmp/ephemeral-file
```

Es importante buscar archivos huérfanos porque:
1. **Seguridad**: si se crea otro grupo que reciba el mismo GID reciclado,
   ese nuevo grupo heredaría acceso a todos los archivos del grupo eliminado.
2. **Auditoría**: archivos con GIDs numéricos dificultan saber quién tiene
   acceso.
3. **Limpieza**: pueden quedar datos que deberían haberse eliminado.

`find / -nogroup` encuentra todos los archivos cuyo GID no corresponde a
ningún grupo en `/etc/group`. La corrección es reasignarlos (`chgrp`) o
eliminarlos según corresponda.

</details>

```bash
docker compose exec debian-dev bash -c 'rm -f /tmp/ephemeral-file'
```

### Ejercicio 10 — Rangos de GID: sistema vs regular

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear grupo de sistema ==="
groupadd -r sysgrp
echo "sysgrp GID: $(getent group sysgrp | cut -d: -f3)"

echo ""
echo "=== Crear grupo regular ==="
groupadd reggrp
echo "reggrp GID: $(getent group reggrp | cut -d: -f3)"

echo ""
echo "=== Rangos configurados ==="
grep -E "^(SYS_GID_MIN|SYS_GID_MAX|GID_MIN|GID_MAX)" /etc/login.defs
'
```

**Pregunta**: ¿En qué rango caerá el GID de `sysgrp` y en cuál el de `reggrp`?
¿De dónde vienen estos rangos?

<details><summary>Predicción</summary>

- `sysgrp` (con `-r`): GID en el rango **100-999** (Debian) o **201-999** (RHEL)
- `reggrp` (sin `-r`): GID en el rango **1000-60000**

Los rangos se configuran en `/etc/login.defs`:

```
SYS_GID_MIN   100     # (Debian) o 201 (RHEL)
SYS_GID_MAX   999
GID_MIN        1000
GID_MAX        60000
```

Estos son los mismos rangos que para UIDs (`SYS_UID_MIN`/`UID_MIN`). La
distinción existe para separar grupos de servicio/sistema de grupos de
usuarios reales, facilitando la auditoría y evitando conflictos de GID.

`groupadd -r` asigna GIDs decrementando desde `SYS_GID_MAX` (999 hacia
abajo), mientras que `groupadd` regular incrementa desde `GID_MIN` (1000
hacia arriba).

</details>

```bash
docker compose exec debian-dev bash -c '
groupdel sysgrp
groupdel reggrp
'
```
