# T02 — chmod y chown

## chmod — Cambiar permisos

`chmod` modifica los permisos de archivos y directorios. Tiene dos sintaxis:
octal y simbólica.

### Sintaxis octal

```bash
chmod 755 script.sh    # rwxr-xr-x
chmod 644 config.txt   # rw-r--r--
chmod 600 id_rsa       # rw-------
chmod 700 .ssh/        # rwx------
```

Es concisa y establece los tres conjuntos de permisos a la vez. Es la forma
preferida cuando se conoce el permiso final exacto.

### Sintaxis simbólica

```
chmod [quién][operador][permisos] archivo
```

**Quién**:

| Letra | Significado |
|---|---|
| `u` | User (owner) |
| `g` | Group |
| `o` | Others |
| `a` | All (u+g+o) |

**Operador**:

| Símbolo | Significado |
|---|---|
| `+` | Agregar permiso |
| `-` | Quitar permiso |
| `=` | Establecer exactamente |

```bash
# Agregar ejecución para el owner
chmod u+x script.sh

# Quitar escritura para group y others
chmod go-w file.txt

# Establecer permisos exactos para group
chmod g=rx script.sh

# Agregar lectura para todos (explícitamente)
chmod a+r document.txt

# Múltiples cambios separados por coma
chmod u+x,g=rx,o-w script.sh

# Quitar todos los permisos de others
chmod o= file.txt
```

### `chmod +x` vs `chmod a+x`

Sin especificar quién (`chmod +x`), el resultado se filtra por la **umask**:

```bash
# Con umask 022:
chmod +x file    # Equivale a a+x (porque umask no bloquea x)

# Con umask 077:
chmod +x file    # Solo da x al owner (umask bloquea g+x y o+x)
chmod a+x file   # Da x a todos (a ignora la umask)
```

Para portabilidad, siempre especificar quién: `chmod a+x` o `chmod u+x`.

### Cuándo usar cada sintaxis

| Situación | Preferir |
|---|---|
| Establecer permisos desde cero | Octal: `chmod 755` |
| Agregar/quitar un permiso específico | Simbólica: `chmod g+w` |
| Scripts de automatización | Octal (predecible, no depende del estado actual) |
| Ajustes rápidos | Simbólica (más expresiva) |

### chmod -R — Recursivo

```bash
# Aplicar recursivamente a un directorio y todo su contenido
chmod -R 750 /srv/project/
```

El problema con `-R` es que aplica los **mismos permisos** a archivos y
directorios. Pero los directorios necesitan `x` para ser accesibles, mientras
que los archivos generalmente no necesitan `x`:

```bash
# INCORRECTO — da x a todos los archivos
chmod -R 755 /srv/project/

# CORRECTO — permisos distintos para archivos y directorios
find /srv/project -type d -exec chmod 755 {} \;
find /srv/project -type f -exec chmod 644 {} \;
```

### chmod con X mayúscula

`X` (mayúscula) agrega ejecución **solo a directorios y archivos que ya tienen
algún bit x**:

```bash
# Agrega x a directorios pero NO a archivos regulares
chmod -R a+X /srv/project/

# Combinar: legible por todos, x solo en directorios
chmod -R u=rwX,g=rX,o=rX /srv/project/
```

`X` es la forma idiomática de hacer `chmod -R` sin dar ejecución accidental a
archivos regulares.

## chown — Cambiar propietario y grupo

```bash
# Cambiar owner
sudo chown alice file.txt

# Cambiar owner y grupo
sudo chown alice:developers file.txt

# Cambiar solo el grupo (equivalente a chgrp)
sudo chown :developers file.txt

# Cambiar owner, grupo al grupo primario del nuevo owner
sudo chown alice: file.txt
# (el grupo se cambia al grupo primario de alice)
```

### chown -R — Recursivo

```bash
# Cambiar owner y grupo recursivamente
sudo chown -R www-data:www-data /var/www/site/
```

### Solo root puede cambiar el owner

Un usuario regular no puede dar la propiedad de sus archivos a otro usuario
— esto es una medida de seguridad para evitar que un usuario ocupe la cuota
de disco de otro:

```bash
# Como usuario regular
chown bob myfile.txt
# chown: changing ownership of 'myfile.txt': Operation not permitted

# Solo root puede cambiar el owner
sudo chown bob myfile.txt
```

### chgrp — Cambiar solo el grupo

```bash
chgrp developers file.txt

# Equivalente a:
chown :developers file.txt

# Recursivo
chgrp -R developers /srv/project/
```

Un usuario regular puede cambiar el grupo de sus archivos, pero solo a un
grupo al que pertenezca:

```bash
# dev pertenece a: dev, sudo, docker
chgrp docker myfile.txt     # OK (dev es miembro de docker)
chgrp www-data myfile.txt   # Permission denied (dev no es miembro de www-data)
```

## Peculiaridades y edge cases

### Permisos de symlinks

Los permisos de symlinks no se usan — el kernel siempre sigue el link y
aplica los permisos del archivo destino:

```bash
ls -la link -> target
# lrwxrwxrwx 1 dev dev ... link -> target
# (siempre muestra 777 pero no importa — se usan los permisos de target)

# chmod sobre un symlink modifica el TARGET, no el link
chmod 600 link
# Cambia los permisos de target, no de link
```

`chmod` en Linux **no puede** cambiar los permisos de un symlink en sí — los
symlinks siempre tienen permisos `lrwxrwxrwx`. Para cambiar el **owner** del
symlink (sin seguir al target):

```bash
# -h flag en chown: opera sobre el symlink, no sobre el target
chown -h alice link    # Cambia el owner del symlink
# (chmod no tiene equivalente -h en Linux)
```

### Permisos y hard links

Todos los hard links a un mismo archivo comparten los mismos permisos (porque
apuntan al mismo inodo):

```bash
echo "test" > original
ln original hardlink

chmod 600 hardlink
ls -la original hardlink
# -rw------- 2 dev dev ... original
# -rw------- 2 dev dev ... hardlink
# (ambos cambiaron porque son el mismo archivo)
```

### Archivos inmutables (chattr)

Los permisos se pueden reforzar con atributos extendidos:

```bash
# Hacer un archivo inmutable (ni root puede modificar sin quitar el flag)
sudo chattr +i /etc/resolv.conf

# Verificar
lsattr /etc/resolv.conf
# ----i--------e-- /etc/resolv.conf

# Ni root puede modificar (la redirección requiere sh -c para que sudo aplique)
sudo sh -c 'echo "nameserver 8.8.8.8" >> /etc/resolv.conf'
# Operation not permitted

# Quitar el flag
sudo chattr -i /etc/resolv.conf
```

Los atributos de `chattr` operan a nivel de filesystem (ext4, xfs) y son
independientes de los permisos Unix.

## Buenas prácticas

```bash
# Claves SSH: 600 o 400 obligatorio (ssh rechaza permisos laxos)
chmod 600 ~/.ssh/id_rsa
chmod 644 ~/.ssh/id_rsa.pub
chmod 700 ~/.ssh/

# Directorios web
sudo chown -R www-data:www-data /var/www/site
sudo find /var/www/site -type d -exec chmod 755 {} \;
sudo find /var/www/site -type f -exec chmod 644 {} \;

# Scripts
chmod 755 script.sh    # Ejecutable por todos, editable solo por owner
chmod 700 admin.sh     # Solo el owner puede ejecutar

# Configuraciones con secretos
chmod 640 /etc/app/config.yml
chown root:appgroup /etc/app/config.yml
```

## Tabla resumen

| Comando | Cambia owner | Cambia grupo | Recursivo | Notas |
|---|---|---|---|---|
| `chmod` | No | No | Sí (`-R`) | Solo permisos |
| `chown` | Sí | Opcional | Sí (`-R`) | Solo root puede cambiar owner |
| `chgrp` | No | Sí | Sí (`-R`) | Solo grupos a los que perteneces |
| `chattr` | No | No | No | Atributos de filesystem, independientes de permisos |

---

## Labs

### Parte 1 — chmod octal y simbólica

#### Paso 1.1: Sintaxis octal

```bash
docker compose exec debian-dev bash -c '
touch /tmp/chmod-test

echo "=== Establecer permisos con octal ==="
chmod 644 /tmp/chmod-test && stat -c "%a %A" /tmp/chmod-test
chmod 755 /tmp/chmod-test && stat -c "%a %A" /tmp/chmod-test
chmod 600 /tmp/chmod-test && stat -c "%a %A" /tmp/chmod-test
chmod 700 /tmp/chmod-test && stat -c "%a %A" /tmp/chmod-test
'
```

La sintaxis octal establece los tres conjuntos a la vez. Es
predecible y no depende del estado actual.

#### Paso 1.2: Sintaxis simbólica

```bash
docker compose exec debian-dev bash -c '
chmod 644 /tmp/chmod-test
echo "=== Inicio: $(stat -c "%a %A" /tmp/chmod-test) ==="

echo ""
echo "--- Agregar x para owner ---"
chmod u+x /tmp/chmod-test && stat -c "%a %A" /tmp/chmod-test

echo ""
echo "--- Quitar r para others ---"
chmod o-r /tmp/chmod-test && stat -c "%a %A" /tmp/chmod-test

echo ""
echo "--- Establecer group = exactamente rx ---"
chmod g=rx /tmp/chmod-test && stat -c "%a %A" /tmp/chmod-test

echo ""
echo "--- Quitar todos los permisos de others ---"
chmod o= /tmp/chmod-test && stat -c "%a %A" /tmp/chmod-test

echo ""
echo "--- Multiples cambios ---"
chmod u=rwx,g=rx,o=r /tmp/chmod-test && stat -c "%a %A" /tmp/chmod-test

rm -f /tmp/chmod-test
'
```

### Parte 2 — chown y chgrp

#### Paso 2.1: Cambiar owner y grupo

```bash
docker compose exec debian-dev bash -c '
useradd -m -s /bin/bash chowntest 2>/dev/null
touch /tmp/chown-file
echo "=== Archivo original ==="
ls -la /tmp/chown-file

echo ""
echo "=== Cambiar owner ==="
chown chowntest /tmp/chown-file
ls -la /tmp/chown-file

echo ""
echo "=== Cambiar owner y grupo ==="
chown root:root /tmp/chown-file
ls -la /tmp/chown-file

echo ""
echo "=== Cambiar solo grupo ==="
chown :chowntest /tmp/chown-file
ls -la /tmp/chown-file

echo ""
echo "=== chown usuario: (grupo = primario del nuevo owner) ==="
chown chowntest: /tmp/chown-file
ls -la /tmp/chown-file

rm -f /tmp/chown-file
userdel -r chowntest 2>/dev/null
'
```

#### Paso 2.2: Restricciones de usuarios regulares

```bash
docker compose exec debian-dev bash -c '
useradd -m -s /bin/bash reguser
touch /tmp/reg-file
chown reguser:reguser /tmp/reg-file

echo "=== reguser intenta cambiar owner (falla) ==="
su -c "chown root /tmp/reg-file" reguser 2>&1 || true

echo ""
echo "=== reguser cambia grupo a uno propio (funciona) ==="
usermod -aG root reguser
su -c "chgrp root /tmp/reg-file" reguser 2>&1 || true
ls -la /tmp/reg-file

rm -f /tmp/reg-file
userdel -r reguser
'
```

### Parte 3 — Recursivo y edge cases

#### Paso 3.1: El problema de chmod -R

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear estructura de prueba ==="
mkdir -p /tmp/webdir/css /tmp/webdir/js
touch /tmp/webdir/index.html /tmp/webdir/css/style.css /tmp/webdir/js/app.js

echo ""
echo "=== chmod -R 755 (INCORRECTO para archivos) ==="
chmod -R 755 /tmp/webdir
find /tmp/webdir -exec stat -c "%a %A %n" {} \;
echo ""
echo "(los archivos tienen x — no deberian ser ejecutables)"
'
```

#### Paso 3.2: find + chmod (correcto)

```bash
docker compose exec debian-dev bash -c '
echo "=== Permisos diferenciados ==="
find /tmp/webdir -type d -exec chmod 755 {} \;
find /tmp/webdir -type f -exec chmod 644 {} \;

echo ""
echo "=== Verificar ==="
find /tmp/webdir -exec stat -c "%a %A %n" {} \;
echo ""
echo "(directorios 755, archivos 644 — correcto)"
'
```

#### Paso 3.3: chmod con X mayúscula

```bash
docker compose exec debian-dev bash -c '
echo "=== Resetear todo a 600 ==="
chmod -R 600 /tmp/webdir
find /tmp/webdir -exec stat -c "%a %n" {} \;
echo "(todo 600 — los directorios no son accesibles)"

echo ""
echo "=== chmod -R u=rwX,g=rX,o=rX ==="
chmod -R u=rwX,g=rX,o=rX /tmp/webdir

echo ""
echo "=== Verificar ==="
find /tmp/webdir -exec stat -c "%a %A %n" {} \;
echo "(directorios tienen x, archivos NO)"
'
```

#### Paso 3.4: Symlinks y hard links

```bash
docker compose exec debian-dev bash -c '
echo "=== Symlink: chmod modifica el TARGET ==="
echo "test" > /tmp/webdir/target.txt
chmod 644 /tmp/webdir/target.txt
ln -s /tmp/webdir/target.txt /tmp/webdir/link.txt

chmod 600 /tmp/webdir/link.txt
echo "Symlink: $(stat -c "%A" /tmp/webdir/link.txt) (siempre 777)"
echo "Target:  $(stat -c "%a %A" /tmp/webdir/target.txt) (cambio a 600)"

echo ""
echo "=== Hard link: comparten permisos (mismo inodo) ==="
echo "test" > /tmp/hl-original
ln /tmp/hl-original /tmp/hl-link
chmod 600 /tmp/hl-link
stat -c "%a %i %n" /tmp/hl-original /tmp/hl-link
echo "(mismo inodo = mismos permisos)"

rm -rf /tmp/webdir /tmp/hl-original /tmp/hl-link
'
```

### Limpieza

```bash
docker compose exec debian-dev bash -c '
rm -rf /tmp/webdir /tmp/chmod-test /tmp/chown-file /tmp/hl-* 2>/dev/null
echo "Limpieza completada"
'
```

---

## Ejercicios

### Ejercicio 1 — Sintaxis simbólica paso a paso

```bash
docker compose exec debian-dev bash -c '
touch /tmp/sym-test
chmod 644 /tmp/sym-test
echo "Inicio: $(stat -c "%a %A" /tmp/sym-test)"

chmod u+x /tmp/sym-test
echo "u+x:   $(stat -c "%a %A" /tmp/sym-test)"

chmod g+w /tmp/sym-test
echo "g+w:   $(stat -c "%a %A" /tmp/sym-test)"

chmod o-r /tmp/sym-test
echo "o-r:   $(stat -c "%a %A" /tmp/sym-test)"
'
```

**Pregunta**: Partiendo de 644, predice el octal después de cada operación:
`u+x`, luego `g+w`, luego `o-r`.

<details><summary>Predicción</summary>

```
Inicio: 644 (-rw-r--r--)
u+x:    744 (-rwxr--r--)    → owner gana x: 6+1=7
g+w:    764 (-rwxrw-r--)    → group gana w: 4+2=6
o-r:    760 (-rwxrw-----)   → others pierde r: 4-4=0
```

La sintaxis simbólica modifica **incrementalmente**: `+` agrega bits, `-`
quita bits, sin alterar los demás. Esto es diferente de la octal que
establece TODO de una vez.

</details>

```bash
docker compose exec debian-dev bash -c 'rm -f /tmp/sym-test'
```

### Ejercicio 2 — chmod -R vs find (permisos diferenciados)

```bash
docker compose exec debian-dev bash -c '
mkdir -p /tmp/project/src /tmp/project/docs
touch /tmp/project/src/main.c /tmp/project/docs/readme.txt
echo "#!/bin/bash" > /tmp/project/src/build.sh

echo "=== chmod -R 755 (todo con x) ==="
chmod -R 755 /tmp/project
find /tmp/project -exec stat -c "%a %A %n" {} \;
'
```

**Pregunta**: Después de `chmod -R 755`, ¿qué problema tienen `main.c` y
`readme.txt`? ¿Cómo se corrige sin afectar directorios ni `build.sh`?

<details><summary>Predicción</summary>

`main.c` y `readme.txt` tienen el bit `x` activado (`755 = rwxr-xr-x`),
lo que los marca como ejecutables cuando no lo son. Esto es un riesgo de
seguridad y una mala práctica.

Corrección con `find`:
```bash
find /tmp/project -type d -exec chmod 755 {} \;
find /tmp/project -type f -exec chmod 644 {} \;
chmod 755 /tmp/project/src/build.sh   # restaurar x solo al script
```

O con `X` mayúscula (más elegante):
```bash
chmod -R u=rwX,g=rX,o=rX /tmp/project
chmod u+x /tmp/project/src/build.sh
```

`X` da `x` a directorios pero no a archivos que no lo tenían. Sin embargo,
como `chmod -R 755` ya puso `x` en todos los archivos, `X` los mantendría.
Hay que quitar primero los `x` con `chmod -R 644` y luego aplicar `X`.

</details>

```bash
docker compose exec debian-dev bash -c 'rm -rf /tmp/project'
```

### Ejercicio 3 — X mayúscula

```bash
docker compose exec debian-dev bash -c '
mkdir -p /tmp/xtest/sub
touch /tmp/xtest/file.txt /tmp/xtest/sub/data.csv
chmod -R 600 /tmp/xtest

echo "=== Todo en 600 ==="
find /tmp/xtest -exec stat -c "%a %A %n" {} \;

echo ""
echo "=== Aplicar u=rwX,go=rX ==="
chmod -R u=rwX,go=rX /tmp/xtest
find /tmp/xtest -exec stat -c "%a %A %n" {} \;
'
```

**Pregunta**: Después de `chmod -R u=rwX,go=rX`, ¿qué permisos tendrán los
directorios vs los archivos?

<details><summary>Predicción</summary>

```
Directorios: 755 (drwxr-xr-x) — X les da x porque son directorios
Archivos:    644 (-rw-r--r--) — X NO les da x porque no tenían ningún bit x
```

`X` (mayúscula) solo agrega `x` a:
1. **Directorios** (siempre)
2. **Archivos que ya tienen algún bit x** (cualquiera de los tres)

Como los archivos estaban en 600 (sin ningún `x`), `X` los deja sin `x`.
Los directorios siempre reciben `x` con `X`.

Este es el patrón correcto para `chmod -R`: una sola operación que diferencia
automáticamente archivos y directorios.

</details>

```bash
docker compose exec debian-dev bash -c 'rm -rf /tmp/xtest'
```

### Ejercicio 4 — chown user: (grupo implícito)

```bash
docker compose exec debian-dev bash -c '
useradd -m -s /bin/bash alice4
groupadd -f devteam
usermod -aG devteam alice4

touch /tmp/colon-test
chown root:root /tmp/colon-test

echo "=== Antes ==="
ls -la /tmp/colon-test

echo ""
echo "=== chown alice4: (con dos puntos al final) ==="
chown alice4: /tmp/colon-test
ls -la /tmp/colon-test
'
```

**Pregunta**: `chown alice4:` (con `:` pero sin grupo explícito) cambia el
owner a alice4. ¿A qué grupo cambia?

<details><summary>Predicción</summary>

El grupo cambia al **grupo primario de alice4** (que es `alice4` por UPG):

```
Antes:   -rw-r--r-- 1 root   root   ... colon-test
Después: -rw-r--r-- 1 alice4 alice4 ... colon-test
```

La sintaxis `chown user:` (con `:` al final) significa "cambiar owner a `user`
y grupo al grupo primario de `user`". Es diferente de:
- `chown alice4` (sin `:`): solo cambia owner, grupo no cambia
- `chown alice4:devteam`: cambia owner y grupo explícitamente
- `chown :devteam`: solo cambia grupo

</details>

```bash
docker compose exec debian-dev bash -c '
rm -f /tmp/colon-test
userdel -r alice4; groupdel devteam 2>/dev/null
'
```

### Ejercicio 5 — chgrp solo a grupos propios

```bash
docker compose exec debian-dev bash -c '
groupadd -f allowed
groupadd -f forbidden
useradd -m -s /bin/bash grpchanger
usermod -aG allowed grpchanger

touch /tmp/grp-test
chown grpchanger:grpchanger /tmp/grp-test

echo "=== Cambiar a grupo propio (allowed) ==="
su -c "chgrp allowed /tmp/grp-test" grpchanger 2>&1
ls -la /tmp/grp-test

echo ""
echo "=== Cambiar a grupo ajeno (forbidden) ==="
su -c "chgrp forbidden /tmp/grp-test" grpchanger 2>&1 || true
'
```

**Pregunta**: ¿Por qué el segundo `chgrp` falla?

<details><summary>Predicción</summary>

El segundo `chgrp` falla con "Operation not permitted" porque `grpchanger`
**no es miembro** del grupo `forbidden`:

```
chgrp allowed:   OK (grpchanger es miembro)
chgrp forbidden: Operation not permitted
```

Un usuario regular solo puede cambiar el grupo de sus archivos a un grupo
al que **pertenezca** (primario o suplementario). Esto evita que un usuario
asigne archivos a grupos arbitrarios, lo que podría:
- Dar acceso no autorizado a otros miembros del grupo
- Evadir cuotas de grupo

Solo root puede asignar cualquier grupo sin restricción.

</details>

```bash
docker compose exec debian-dev bash -c '
rm -f /tmp/grp-test
userdel -r grpchanger
groupdel allowed; groupdel forbidden
'
```

### Ejercicio 6 — Symlink: chmod modifica el target

```bash
docker compose exec debian-dev bash -c '
echo "contenido" > /tmp/real-file.txt
chmod 644 /tmp/real-file.txt
ln -s /tmp/real-file.txt /tmp/my-link

echo "=== Antes ==="
echo "Link:   $(stat -c "%A" /tmp/my-link)"
echo "Target: $(stat -c "%a %A" /tmp/real-file.txt)"

echo ""
echo "=== chmod 600 sobre el link ==="
chmod 600 /tmp/my-link
echo "Link:   $(stat -c "%A" /tmp/my-link)"
echo "Target: $(stat -c "%a %A" /tmp/real-file.txt)"
'
```

**Pregunta**: Al hacer `chmod 600 /tmp/my-link`, ¿cambian los permisos del
symlink o del archivo real?

<details><summary>Predicción</summary>

Cambian los permisos del **archivo real** (target), no del symlink:

```
Antes:
  Link:   lrwxrwxrwx  (siempre 777)
  Target: 644 -rw-r--r--

Después:
  Link:   lrwxrwxrwx  (sigue siendo 777 — no se puede cambiar)
  Target: 600 -rw-------  (ESTE cambió)
```

`chmod` sigue el symlink y modifica el target. Los permisos del symlink
en sí son siempre `lrwxrwxrwx` y **no se pueden cambiar** en Linux.

`chown` tiene la opción `-h` para operar sobre el symlink en sí (cambiar
su owner), pero `chmod` no tiene equivalente porque Linux no usa los
permisos del symlink.

</details>

```bash
docker compose exec debian-dev bash -c 'rm -f /tmp/real-file.txt /tmp/my-link'
```

### Ejercicio 7 — go= vs o=

```bash
docker compose exec debian-dev bash -c '
touch /tmp/go1 /tmp/go2
chmod 776 /tmp/go1 /tmp/go2

echo "=== Inicio: ambos en 776 ==="
stat -c "%a %A %n" /tmp/go1 /tmp/go2

echo ""
chmod go= /tmp/go1
chmod o= /tmp/go2
echo "=== go= vs o= ==="
stat -c "%a %A %n" /tmp/go1 /tmp/go2
'
```

**Pregunta**: Partiendo de 776, ¿qué permisos quedan con `go=` vs `o=`?

<details><summary>Predicción</summary>

```
go=: 700 (-rwx------)  → group Y others = nada
o=:  770 (-rwxrwx---)  → solo others = nada, group no cambia
```

- `go=` actúa sobre **group y others**: ambos quedan sin permisos
- `o=` actúa solo sobre **others**: group mantiene sus permisos (7=rwx)

`=` sin nada a la derecha establece los permisos a **cero** para las
categorías especificadas. Es equivalente a `-rwx` para esas categorías.

</details>

```bash
docker compose exec debian-dev bash -c 'rm -f /tmp/go1 /tmp/go2'
```

### Ejercicio 8 — Hard links comparten todo

```bash
docker compose exec debian-dev bash -c '
echo "original" > /tmp/hl-orig
ln /tmp/hl-orig /tmp/hl-copy

echo "=== Inodos ==="
stat -c "%i %n" /tmp/hl-orig /tmp/hl-copy

echo ""
echo "=== Cambiar permisos via el hard link ==="
chmod 600 /tmp/hl-copy
stat -c "%a %A %n" /tmp/hl-orig /tmp/hl-copy

echo ""
echo "=== Cambiar contenido via el hard link ==="
echo "modificado" > /tmp/hl-copy
cat /tmp/hl-orig
'
```

**Pregunta**: Al cambiar permisos o contenido del hard link, ¿qué pasa con
el original?

<details><summary>Predicción</summary>

**Ambos cambian** porque son el mismo archivo (mismo inodo):

```
Inodos: 12345 /tmp/hl-orig    ← mismo número
        12345 /tmp/hl-copy    ← mismo número

Permisos: 600 -rw------- /tmp/hl-orig    ← cambió
          600 -rw------- /tmp/hl-copy    ← cambió

cat hl-orig: "modificado"    ← el contenido también cambió
```

Un hard link NO es una copia. Es otro nombre para el mismo archivo (mismo
inodo en el filesystem). Permisos, contenido, timestamps — todo se comparte.
Cambiar uno es cambiar el otro porque son **el mismo archivo**.

Esto es diferente de un symlink, que es solo un puntero al nombre del archivo.

</details>

```bash
docker compose exec debian-dev bash -c 'rm -f /tmp/hl-orig /tmp/hl-copy'
```

### Ejercicio 9 — chattr +i (inmutable)

```bash
docker compose exec debian-dev bash -c '
echo "protegido" > /tmp/immutable-test
chmod 644 /tmp/immutable-test

echo "=== Atributos antes ==="
lsattr /tmp/immutable-test

echo ""
echo "=== Activar inmutable ==="
chattr +i /tmp/immutable-test
lsattr /tmp/immutable-test

echo ""
echo "=== Root intenta escribir ==="
sh -c "echo nuevo >> /tmp/immutable-test" 2>&1 || true

echo ""
echo "=== Root intenta chmod ==="
chmod 777 /tmp/immutable-test 2>&1 || true

echo ""
echo "=== Root intenta rm ==="
rm /tmp/immutable-test 2>&1 || true

echo ""
echo "=== Quitar inmutable y limpiar ==="
chattr -i /tmp/immutable-test
rm /tmp/immutable-test
'
```

**Pregunta**: Con `chattr +i`, ¿puede root modificar, cambiar permisos, o
eliminar el archivo?

<details><summary>Predicción</summary>

**No a todo.** Con el atributo inmutable (`+i`):

```
Escribir:         Operation not permitted
chmod:            Operation not permitted
rm:               Operation not permitted
```

`chattr +i` es **más fuerte que los permisos Unix**. Ni siquiera root puede:
- Modificar el contenido
- Cambiar permisos (`chmod`)
- Cambiar owner (`chown`)
- Eliminar el archivo (`rm`)
- Renombrar el archivo (`mv`)

Lo único que root puede hacer es quitar el atributo con `chattr -i` y luego
realizar la operación. Este atributo opera a nivel del filesystem (ext4, xfs)
y es independiente del modelo de permisos Unix.

Se usa para proteger archivos críticos contra modificación accidental, incluso
por root (ej: `/etc/resolv.conf` que NetworkManager podría sobreescribir).

</details>

### Ejercicio 10 — Permisos recomendados para SSH

```bash
docker compose exec debian-dev bash -c '
useradd -m -s /bin/bash sshuser
mkdir -p /home/sshuser/.ssh
touch /home/sshuser/.ssh/id_rsa
touch /home/sshuser/.ssh/id_rsa.pub
touch /home/sshuser/.ssh/authorized_keys
chown -R sshuser:sshuser /home/sshuser/.ssh

echo "=== Permisos correctos para SSH ==="
chmod 700 /home/sshuser/.ssh
chmod 600 /home/sshuser/.ssh/id_rsa
chmod 644 /home/sshuser/.ssh/id_rsa.pub
chmod 600 /home/sshuser/.ssh/authorized_keys

find /home/sshuser/.ssh -exec stat -c "%a %A %n" {} \;
'
```

**Pregunta**: ¿Por qué SSH rechaza la conexión si `id_rsa` tiene permisos
644 en vez de 600?

<details><summary>Predicción</summary>

SSH verifica permisos estrictamente antes de usar claves privadas:

```
.ssh/          → 700 (drwx------) — solo owner accede al directorio
id_rsa         → 600 (rw-------) — solo owner lee la clave privada
id_rsa.pub     → 644 (rw-r--r--) — la clave pública puede ser legible
authorized_keys → 600 (rw-------) — solo owner modifica
```

Si `id_rsa` tiene 644, el cliente SSH muestra:
```
WARNING: UNPROTECTED PRIVATE KEY FILE!
Permissions 0644 for '/home/user/.ssh/id_rsa' are too open.
```

Y **rechaza usar la clave**. Esto es una medida de seguridad: si otros
usuarios pueden leer la clave privada, ya no es privada. SSH prefiere
fallar ruidosamente a usar una clave comprometida.

El servidor también verifica permisos de `authorized_keys` y `.ssh/` —
si son demasiado permisivos, ignora el archivo.

</details>

```bash
docker compose exec debian-dev bash -c 'userdel -r sshuser'
```
