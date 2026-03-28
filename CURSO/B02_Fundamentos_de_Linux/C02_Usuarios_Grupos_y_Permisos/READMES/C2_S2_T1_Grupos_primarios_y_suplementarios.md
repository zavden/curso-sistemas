# T01 — Grupos primarios y suplementarios

## Dos tipos de membresía

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

Con `-N` se puede desactivar UPG para un usuario específico:

```bash
# No crear grupo privado — usa el grupo por defecto (GID 100, "users")
sudo useradd -m -N testuser
id -gn testuser
# users
```

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

### Cambiar el grupo primario (permanente)

```bash
# Cambiar el GID en /etc/passwd
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

### Agregar y quitar de grupos

Hay dos herramientas para gestionar membresías suplementarias:

```bash
# Agregar con usermod
sudo usermod -aG docker dev      # -a es CRUCIAL (sin ella, REEMPLAZA)

# Agregar con gpasswd
sudo gpasswd -a dev docker       # equivalente a -aG

# Quitar con gpasswd (la única forma limpia)
sudo gpasswd -d dev docker

# Quitar con deluser (solo Debian)
sudo deluser dev docker

# NO existe "usermod -rG" ni forma de quitar un solo grupo con usermod.
# Para quitar con usermod hay que reconstruir la lista completa:
sudo usermod -G sudo,www-data dev    # quita docker al no incluirlo
```

### Cómo verifica permisos el kernel

Cuando un proceso intenta acceder a un archivo, el kernel verifica en orden
estricto:

1. ¿El UID del proceso coincide con el propietario del archivo? → usa permisos de **owner**
2. ¿El GID primario o algún GID suplementario coincide con el GID del archivo? → usa permisos de **group**
3. Ninguno coincide → usa permisos de **others**

**El kernel usa la primera coincidencia y se detiene.** No acumula permisos
de varias categorías. Esto tiene una consecuencia contra-intuitiva:

```bash
# Archivo donde owner tiene MENOS permisos que group
chmod 040 /tmp/weird
chown dev:staff /tmp/weird
ls -la /tmp/weird
# ----r----- 1 dev staff ... /tmp/weird

# dev ES el owner → usa permisos de owner (---) → DENEGADO
# Aunque dev esté en el grupo staff (que tiene r--), no importa.
# El kernel ya encontró coincidencia con owner y se detuvo.
```

### Ejemplo práctico: docker.sock

```bash
ls -la /var/run/docker.sock
# srw-rw---- 1 root docker 0 ... /var/run/docker.sock

# dev (UID 1000) no es root → no coincide owner
# dev tiene docker como grupo suplementario → SÍ coincide group → acceso rw
docker ps
# (funciona porque dev está en el grupo docker)
```

### Miembros primarios no aparecen en `/etc/group`

```bash
getent group dev
# dev:x:1000:
# (campo 4 vacío — dev es miembro IMPLÍCITO por su GID primario)

# Solo los miembros suplementarios aparecen en el campo 4:
getent group docker
# docker:x:999:dev,alice
```

Para listar TODOS los miembros de un grupo (primarios + suplementarios),
hay que combinar ambas fuentes:

```bash
# Miembros suplementarios de "staff"
getent group staff | cut -d: -f4

# Miembros primarios de "staff" (GID en /etc/passwd)
awk -F: -v gid=$(getent group staff | cut -d: -f3) '$4==gid {print $1}' /etc/passwd
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
# (si no tiene contraseña, se rechaza el acceso)
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
id -g dev     # Solo GID primario (numérico)
id -G dev     # Todos los GIDs (numéricos)
id -gn dev    # Solo grupo primario (nombre)
id -Gn dev    # Todos los grupos (nombres)

# groups — lista de grupos de un usuario
groups dev
# dev : dev sudo docker

# Sin argumento: grupos del usuario actual
groups
# dev sudo docker

# getent — consultar un grupo con sus miembros (suplementarios)
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
sesión. Cambios con `usermod -aG` **no afectan sesiones existentes**:

```bash
# Agregar al grupo www-data
sudo usermod -aG www-data dev

# En la sesión actual, el grupo NO aparece todavía
id
# uid=1000(dev) gid=1000(dev) groups=1000(dev),27(sudo),999(docker)
# (www-data no aparece)

# Opciones para activarlo:
# 1. Cerrar sesión y volver a entrar (más limpio)
# 2. newgrp www-data (abre shell nueva con www-data como primario)
# 3. exec su -l $USER (reemplaza la shell — pide contraseña)
```

## Tabla resumen: comandos de gestión de grupos

| Tarea | Comando | Notas |
|---|---|---|
| Ver grupos de usuario | `id user`, `groups user` | |
| Agregar a grupo suplementario | `usermod -aG group user` | `-a` es crucial |
| Agregar a grupo (alternativa) | `gpasswd -a user group` | Orden de argumentos diferente |
| Quitar de un grupo | `gpasswd -d user group` | Única forma de quitar uno solo |
| Quitar de un grupo (Debian) | `deluser user group` | Wrapper de gpasswd |
| Cambiar grupo primario | `usermod -g group user` | Permanente, requiere sudo |
| Cambiar grupo efectivo temporal | `newgrp group` | Nueva shell |
| Ejecutar con grupo diferente | `sg group -c "cmd"` | Sin nueva shell permanente |

---

## Labs

### Parte 1 — Inspeccionar grupos

#### Paso 1.1: Ver todos los grupos

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

#### Paso 1.2: Grupo primario en /etc/passwd

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
resuelve a un nombre a través de `/etc/group`.

#### Paso 1.3: Suplementarios en /etc/group

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
Los miembros primarios no aparecen ahí — hay que usar `id`.

#### Paso 1.4: User Private Group (UPG)

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

#### Paso 1.5: Comparar con AlmaLinux

```bash
echo "=== Debian ==="
docker compose exec debian-dev id dev

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev id dev
```

Ambas distribuciones usan UPG por defecto. Las diferencias están
en los GIDs asignados y los nombres de ciertos grupos (sudo vs wheel).

### Parte 2 — Efecto en archivos

#### Paso 2.1: Crear archivos como dev

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

#### Paso 2.2: Crear con grupo diferente

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

#### Paso 2.3: Grupos en procesos

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

Los grupos se fijan al iniciar la sesión. Cambios con `usermod -aG`
requieren re-login para activarse.

### Parte 3 — Permisos y membresía

#### Paso 3.1: Verificación de permisos

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

El kernel **no acumula permisos**. Usa la primera categoría que
coincide y se detiene ahí. Si eres owner, recibes permisos de
owner aunque los de group sean más amplios.

#### Paso 3.2: Agregar y quitar de grupos

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
`usermod -aG` también agrega. Para quitar, usar `gpasswd -d`
(usermod no tiene opción para quitar de un solo grupo).

#### Paso 3.3: Ver miembros de un grupo

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

### Limpieza

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

## Ejercicios

### Ejercicio 1 — Desglosar la salida de `id`

```bash
docker compose exec debian-dev bash -c '
useradd -m -s /bin/bash -G sudo alice
echo "=== Salida completa ==="
id alice
'
```

**Pregunta**: En la salida de `id alice`, identifica: (a) el grupo primario,
(b) los grupos suplementarios, (c) el GID numérico del grupo primario.

<details><summary>Predicción</summary>

La salida será algo como:
```
uid=1001(alice) gid=1001(alice) groups=1001(alice),27(sudo)
```

- (a) Grupo primario: `alice` (aparece en `gid=` y primero en `groups=`)
- (b) Grupos suplementarios: `sudo` (GID 27)
- (c) GID numérico del primario: 1001

El grupo primario siempre aparece dos veces: una en `gid=` y otra como primer
elemento de `groups=`.

</details>

```bash
docker compose exec debian-dev userdel -r alice
```

### Ejercicio 2 — Contar grupos suplementarios

```bash
docker compose exec debian-dev bash -c '
useradd -m -s /bin/bash -G sudo testcount
echo "=== Todos los grupos ==="
id -Gn testcount

echo ""
echo "=== Conteo total de grupos ==="
echo "Total (incluyendo primario): $(id -Gn testcount | wc -w)"
'
```

**Pregunta**: `id -Gn` muestra TODOS los grupos (primario + suplementarios).
Si el total es 2, ¿cuántos suplementarios tiene el usuario?

<details><summary>Predicción</summary>

Tiene **1 grupo suplementario**. `id -Gn` incluye siempre el grupo primario
en la lista, así que hay que restar 1 al total para obtener solo los
suplementarios:

```
testcount sudo     ← 2 grupos totales
^              ^
primario    suplementario
```

Suplementarios = total - 1 = 2 - 1 = **1** (solo `sudo`).

Para contar solo suplementarios en un script:
```bash
TOTAL=$(id -Gn testcount | wc -w)
echo "Suplementarios: $((TOTAL - 1))"
```

</details>

```bash
docker compose exec debian-dev userdel -r testcount
```

### Ejercicio 3 — UPG vs grupo compartido

```bash
docker compose exec debian-dev bash -c '
echo "=== Usuario con UPG (default) ==="
useradd -m -s /bin/bash upg_user
getent passwd upg_user | cut -d: -f1,4
getent group upg_user

echo ""
echo "=== Usuario SIN UPG (-N) ==="
useradd -m -s /bin/bash -N shared_user
getent passwd shared_user | cut -d: -f1,4
getent group shared_user 2>/dev/null || echo "No existe grupo shared_user"
'
```

**Pregunta**: ¿Qué grupo primario tendrá `shared_user`? ¿Se creará un grupo
con su nombre?

<details><summary>Predicción</summary>

- `upg_user`: grupo primario = `upg_user` (GID creado automáticamente con UPG).
  `getent group upg_user` mostrará `upg_user:x:XXXX:` (grupo existe).

- `shared_user`: grupo primario = `users` (GID 100, el default de
  `/etc/default/useradd`). **No se crea grupo `shared_user`** porque `-N`
  desactiva UPG para ese usuario.

```
upg_user:1001       ← GID propio
shared_user:100     ← GID del grupo "users"
```

UPG aísla archivos por usuario. Sin UPG, todos comparten el mismo grupo
primario, lo que puede exponer archivos si la umask es permisiva.

</details>

```bash
docker compose exec debian-dev bash -c '
userdel -r upg_user
userdel -r shared_user
'
```

### Ejercicio 4 — Grupo primario y creación de archivos

```bash
docker compose exec debian-dev bash -c '
groupadd -f devteam
useradd -m -s /bin/bash filetest

echo "=== Grupo primario inicial ==="
id -gn filetest

echo ""
echo "=== Crear archivo con grupo primario ==="
su -c "touch /tmp/file1.txt" filetest
ls -la /tmp/file1.txt

echo ""
echo "=== Cambiar grupo primario ==="
usermod -g devteam filetest
echo "Nuevo grupo primario: $(id -gn filetest)"

echo ""
echo "=== Crear archivo con nuevo grupo primario ==="
su -c "touch /tmp/file2.txt" filetest
ls -la /tmp/file2.txt
'
```

**Pregunta**: ¿Qué grupo tendrá `file1.txt`? ¿Y `file2.txt`? ¿Cambió
`file1.txt` al cambiar el grupo primario?

<details><summary>Predicción</summary>

- `file1.txt` → grupo `filetest` (el grupo primario original al momento de crear)
- `file2.txt` → grupo `devteam` (el nuevo grupo primario después del cambio)

`file1.txt` **no cambió** de grupo. Cambiar el grupo primario con `usermod -g`
solo afecta archivos **futuros**. Los archivos existentes conservan el grupo
que tenían al ser creados. Para actualizar archivos existentes hay que usar
`chgrp` o `find ... -exec chgrp`.

</details>

```bash
docker compose exec debian-dev bash -c '
rm -f /tmp/file1.txt /tmp/file2.txt
userdel -r filetest
groupdel devteam
'
```

### Ejercicio 5 — El kernel no acumula permisos

```bash
docker compose exec debian-dev bash -c '
groupadd -f staff
useradd -m -s /bin/bash -G staff permtest

echo "=== Crear archivo con permisos invertidos ==="
touch /tmp/trap.txt
echo "contenido secreto" > /tmp/trap.txt
chown permtest:staff /tmp/trap.txt
chmod 040 /tmp/trap.txt
ls -la /tmp/trap.txt

echo ""
echo "=== permtest intenta leer (es owner con permisos ---) ==="
su -c "cat /tmp/trap.txt" permtest 2>&1 || true

echo ""
echo "=== otro usuario en staff intenta leer (permisos r--) ==="
useradd -m -s /bin/bash -G staff other
su -c "cat /tmp/trap.txt" other 2>&1 || true
'
```

**Pregunta**: `permtest` es el owner Y está en el grupo `staff`. El archivo
tiene permisos `040` (owner=---, group=r--, others=---). ¿Puede `permtest`
leer el archivo?

<details><summary>Predicción</summary>

**No.** `permtest` **no puede** leer el archivo, a pesar de estar en el grupo
`staff` que tiene permisos de lectura.

El kernel evalúa en orden estricto:
1. ¿`permtest` es el owner? → **Sí** → usa permisos de owner (`---`) → **DENEGADO**

El kernel **se detiene en la primera coincidencia**. Nunca llega a evaluar
los permisos de group. Esto es contra-intuitivo pero es el comportamiento
correcto del modelo de permisos UNIX.

En cambio, `other` (que NO es owner pero SÍ está en `staff`) **sí puede**
leer: coincide con group (`r--`).

</details>

```bash
docker compose exec debian-dev bash -c '
rm -f /tmp/trap.txt
userdel -r permtest
userdel -r other
groupdel staff
'
```

### Ejercicio 6 — Directorio compartido con SGID

```bash
docker compose exec debian-dev bash -c '
groupadd -f projteam
useradd -m -s /bin/bash -G projteam alice
useradd -m -s /bin/bash bob

echo "=== Crear directorio compartido con SGID ==="
mkdir -p /tmp/shared_proj
chown root:projteam /tmp/shared_proj
chmod 2770 /tmp/shared_proj
ls -ld /tmp/shared_proj

echo ""
echo "=== alice (en projteam) crea archivo ==="
su -c "touch /tmp/shared_proj/alice.txt" alice 2>&1 || true
ls -la /tmp/shared_proj/alice.txt 2>/dev/null || echo "No pudo crear"

echo ""
echo "=== bob (NO en projteam) crea archivo ==="
su -c "touch /tmp/shared_proj/bob.txt" bob 2>&1 || true
ls -la /tmp/shared_proj/bob.txt 2>/dev/null || echo "No pudo crear"
'
```

**Pregunta**: ¿Por qué alice puede escribir en el directorio pero bob no?
¿Qué grupo tendrá `alice.txt`?

<details><summary>Predicción</summary>

- **alice puede escribir** porque pertenece al grupo `projteam` (suplementario).
  El directorio tiene permisos `2770`: owner=rwx, group=rwx, others=---. Alice
  coincide con group → permisos rwx → puede crear archivos.

- **bob NO puede escribir** porque no pertenece a `projteam`. No es owner ni
  está en el grupo → permisos de others (---) → denegado.

- `alice.txt` tendrá grupo **`projteam`** (no `alice`). El bit **SGID** (el `2`
  en `2770`) hace que los archivos creados dentro del directorio hereden el
  grupo del directorio, no el grupo primario del usuario. Esto es clave para
  directorios compartidos por equipos.

</details>

```bash
docker compose exec debian-dev bash -c '
rm -rf /tmp/shared_proj
userdel -r alice
userdel -r bob
groupdel projteam
'
```

### Ejercicio 7 — gpasswd -a vs gpasswd -d

```bash
docker compose exec debian-dev bash -c '
groupadd -f grpA
groupadd -f grpB
groupadd -f grpC
useradd -m -s /bin/bash -G grpA,grpB,grpC multiuser

echo "=== Grupos iniciales ==="
id -Gn multiuser

echo ""
echo "=== Quitar grpB con gpasswd -d ==="
gpasswd -d multiuser grpB
id -Gn multiuser

echo ""
echo "=== Verificar que grpA y grpC siguen ==="
getent group grpA
getent group grpC
'
```

**Pregunta**: Después de `gpasswd -d multiuser grpB`, ¿qué grupos
suplementarios le quedan a `multiuser`?

<details><summary>Predicción</summary>

Le quedan **grpA y grpC**. `gpasswd -d` quita al usuario de **un solo grupo**
sin afectar los demás.

```
Antes:  multiuser grpA grpB grpC
Después: multiuser grpA grpC
```

Esto es diferente a `usermod -G grpA multiuser` (sin `-a`), que **reemplazaría**
todos los grupos suplementarios dejando SOLO `grpA`. `gpasswd -d` es la forma
segura de quitar un grupo específico.

En `getent group grpB`, `multiuser` ya no aparecerá en el campo 4.

</details>

```bash
docker compose exec debian-dev bash -c '
userdel -r multiuser
groupdel grpA; groupdel grpB; groupdel grpC
'
```

### Ejercicio 8 — Grupos en /proc

```bash
docker compose exec debian-dev bash -c '
echo "=== GIDs del proceso actual (root en el container) ==="
cat /proc/self/status | grep -E "^(Uid|Gid|Groups)"

echo ""
echo "=== Crear usuario con grupos y mostrar sus GIDs ==="
groupadd -f testproc
useradd -m -s /bin/bash -G testproc procuser
echo "id de procuser: $(id procuser)"

echo ""
echo "=== Shell como procuser ==="
su -c "cat /proc/self/status | grep -E \"^(Uid|Gid|Groups)\"" procuser
'
```

**Pregunta**: La línea `Gid:` en `/proc/self/status` muestra 4 valores.
¿Qué representa cada uno? ¿Y la línea `Groups:`?

<details><summary>Predicción</summary>

La línea `Gid:` muestra 4 valores separados por tabs:
```
Gid:    1001    1001    1001    1001
         ^       ^       ^       ^
        Real  Effective SavedSet  FS
```

- **Real**: el GID del usuario (de `/etc/passwd`)
- **Effective**: el GID activo para verificación de permisos (normalmente igual al real)
- **Saved Set**: GID guardado (para restaurar después de un cambio temporal)
- **FS**: GID usado para operaciones de filesystem (normalmente igual al effective)

La línea `Groups:` muestra **todos los GIDs** (primario + suplementarios)
como lista de números. Para `procuser`, mostrará el GID de `procuser` y el
GID de `testproc`.

</details>

```bash
docker compose exec debian-dev bash -c '
userdel -r procuser
groupdel testproc
'
```

### Ejercicio 9 — Cambios no aplican a sesiones activas

```bash
docker compose exec debian-dev bash -c '
groupadd -f lategroup
useradd -m -s /bin/bash sessiontest

echo "=== Grupos iniciales ==="
id sessiontest

echo ""
echo "=== Agregar a lategroup ==="
usermod -aG lategroup sessiontest

echo ""
echo "=== Verificar con id (lectura de archivos) ==="
id sessiontest

echo ""
echo "=== Simular sesion activa (los GIDs del proceso no cambian) ==="
su -c "
echo \"Grupos en la sesion actual:\"
id -Gn
echo \"\"
echo \"GIDs en /proc:\"
cat /proc/self/status | grep Groups
" sessiontest
'
```

**Pregunta**: Si `sessiontest` ya tiene una sesión abierta cuando
ejecutamos `usermod -aG lategroup`, ¿aparecerá `lategroup` en su sesión?
¿Y en la salida de `id sessiontest` ejecutado como root?

<details><summary>Predicción</summary>

- `id sessiontest` (ejecutado como root, leyendo `/etc/group`) → **SÍ muestra
  `lategroup`**. El comando `id` consulta los archivos del sistema, que ya
  están actualizados.

- Dentro de la sesión activa de `sessiontest` (`su -c "id -Gn"`) → **SÍ
  muestra `lategroup`** en este caso particular, porque `su` inicia una
  **nueva sesión** que lee los grupos actualizados.

En una sesión SSH o terminal ya abierta (sin pasar por `su`), `lategroup`
**NO aparecería** hasta hacer re-login. Los GIDs del proceso se fijan al
iniciar la sesión y no se actualizan dinámicamente. Para activar sin re-login:
`newgrp lategroup` (abre nueva shell con ese grupo como primario).

</details>

```bash
docker compose exec debian-dev bash -c '
userdel -r sessiontest
groupdel lategroup
'
```

### Ejercicio 10 — Auditoría: quién pertenece a un grupo

```bash
docker compose exec debian-dev bash -c '
groupadd -f audit_grp
useradd -m -s /bin/bash -G audit_grp user_a
useradd -m -s /bin/bash user_b
usermod -g audit_grp user_b

echo "=== user_a tiene audit_grp como suplementario ==="
id user_a

echo ""
echo "=== user_b tiene audit_grp como primario ==="
id user_b

echo ""
echo "=== getent group audit_grp ==="
getent group audit_grp

echo ""
echo "=== Buscar miembros primarios con awk ==="
GID=$(getent group audit_grp | cut -d: -f3)
awk -F: -v gid="$GID" "\$4==gid {print \$1}" /etc/passwd
'
```

**Pregunta**: `getent group audit_grp` muestra a `user_a` pero NO a
`user_b`. ¿Por qué? ¿Cómo encontrar a `user_b`?

<details><summary>Predicción</summary>

`getent group audit_grp` mostrará:
```
audit_grp:x:XXXX:user_a
```

`user_b` **no aparece** porque es miembro **primario** del grupo (su GID en
`/etc/passwd` es el de `audit_grp`). El campo 4 de `/etc/group` solo lista
miembros **suplementarios**.

Para encontrar miembros primarios hay que buscar en `/etc/passwd`:
```bash
awk -F: -v gid=XXXX '$4==gid {print $1}' /etc/passwd
```

Esto mostrará `user_b`.

Para una auditoría completa de todos los miembros de un grupo, hay que
combinar ambas fuentes: campo 4 de `/etc/group` (suplementarios) + campo 4
de `/etc/passwd` donde coincide el GID (primarios).

</details>

```bash
docker compose exec debian-dev bash -c '
userdel -r user_a
userdel -r user_b
groupdel audit_grp
'
```
