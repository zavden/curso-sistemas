# T01 — rwx y notación octal

## El modelo de permisos Unix

Cada archivo y directorio en Linux tiene tres conjuntos de permisos, uno para
cada categoría de usuario:

```
-rwxr-xr-- 1 alice developers 4096 ... script.sh
 ^^^                                     → owner (alice)
    ^^^                                  → group (developers)
       ^^^                               → others (todos los demás)
```

El primer carácter indica el tipo de archivo, no es un permiso:

| Carácter | Tipo |
|---|---|
| `-` | Archivo regular |
| `d` | Directorio |
| `l` | Symlink |
| `b` | Dispositivo de bloque |
| `c` | Dispositivo de carácter |
| `p` | Named pipe (FIFO) |
| `s` | Socket |

## Los tres permisos básicos

### En archivos

| Permiso | Letra | Octal | Significado |
|---|---|---|---|
| Read | `r` | 4 | Leer el contenido del archivo |
| Write | `w` | 2 | Modificar el contenido del archivo |
| Execute | `x` | 1 | Ejecutar el archivo como programa |

```bash
# Archivo con rwx para owner, rx para group, r para others
ls -la script.sh
# -rwxr-xr-- 1 alice developers ... script.sh

# alice puede: leer, escribir, ejecutar
# miembro de developers puede: leer, ejecutar (no escribir)
# cualquier otro puede: leer (no escribir ni ejecutar)
```

### En directorios

Los permisos significan algo **diferente** en directorios:

| Permiso | En archivo | En directorio |
|---|---|---|
| `r` (read) | Leer contenido | Listar nombres de archivos (`ls`) |
| `w` (write) | Modificar contenido | Crear, eliminar, renombrar archivos dentro |
| `x` (execute) | Ejecutar como programa | Acceder al directorio (`cd`), resolver rutas de archivos dentro |

`x` en directorios es el permiso más importante — sin él, **nada funciona**:

```bash
# Directorio con r pero sin x: puedes listar nombres pero no acceder
chmod 644 testdir      # rw-r--r-- (r sin x para group y others)
ls testdir/
# (muestra nombres pero con errores al intentar leer atributos)
cat testdir/file.txt
# Permission denied (no puedes resolver la ruta sin x)

# Directorio con x pero sin r: puedes acceder si conoces el nombre
chmod 711 testdir      # rwx--x--x
ls testdir/
# Permission denied (no puedes listar)
cat testdir/file.txt
# (funciona si conoces el nombre exacto)
```

### w requiere x en directorios

`w` sin `x` en un directorio es **inútil**. Para crear, eliminar o renombrar
archivos se necesitan ambos (`wx`). Sin `x`, el kernel no puede resolver la
ruta dentro del directorio:

```bash
chmod 200 testdir      # -w-------
touch testdir/new.txt
# Permission denied (w sin x no permite nada)

chmod 300 testdir      # -wx------
touch testdir/new.txt
# (funciona — w+x permite crear)
```

### La combinación más contra-intuitiva

`w` en un directorio permite **eliminar cualquier archivo dentro**, incluso
archivos que no te pertenecen y sobre los que no tienes permiso de escritura:

```bash
# Directorio con permisos rwx para others
chmod 777 /tmp/shared/

# alice crea un archivo con permisos restrictivos
# (como alice)
touch /tmp/shared/secret.txt
chmod 600 /tmp/shared/secret.txt

# bob NO puede leer el archivo
# (como bob)
cat /tmp/shared/secret.txt
# Permission denied

# Pero bob SÍ puede ELIMINARLO (porque tiene w+x en el directorio)
rm /tmp/shared/secret.txt
# (eliminado — el permiso de eliminar viene del directorio, no del archivo)
```

Esto es exactamente el problema que resuelve el **sticky bit** (cubierto en
T03).

## Notación octal

Cada permiso tiene un valor numérico. Se suman para obtener el permiso total
de cada categoría:

```
r = 4
w = 2
x = 1
```

```
rwx = 4+2+1 = 7
rw- = 4+2+0 = 6
r-x = 4+0+1 = 5
r-- = 4+0+0 = 4
--- = 0+0+0 = 0
```

El permiso completo se expresa con tres dígitos octales (owner, group, others):

```
-rwxr-xr--  →  754
 ^^^            7 (owner: rwx = 4+2+1)
    ^^^         5 (group: r-x = 4+0+1)
       ^^^      4 (others: r-- = 4+0+0)
```

### Permisos comunes

| Octal | Simbólico | Uso típico |
|---|---|---|
| `755` | `rwxr-xr-x` | Ejecutables, directorios públicos |
| `644` | `rw-r--r--` | Archivos de texto, configuraciones legibles |
| `700` | `rwx------` | Home de root, directorios privados |
| `600` | `rw-------` | Claves SSH, archivos sensibles |
| `750` | `rwxr-x---` | Directorios de grupo |
| `640` | `rw-r-----` | Configuraciones con lectura de grupo |
| `775` | `rwxrwxr-x` | Directorios compartidos por grupo |
| `664` | `rw-rw-r--` | Archivos compartidos por grupo |
| `777` | `rwxrwxrwx` | Evitar — todo el mundo tiene acceso total |
| `000` | `---------` | Solo root puede acceder |

## Ver permisos

```bash
# ls -la (formato largo)
ls -la /etc/passwd
# -rw-r--r-- 1 root root 2345 ... /etc/passwd

# stat (más detalle, incluye octal)
stat /etc/passwd
# Access: (0644/-rw-r--r--)  Uid: (    0/    root)   Gid: (    0/    root)

# Solo octal con stat
stat -c '%a' /etc/passwd
# 644

# Solo simbólico con stat
stat -c '%A' /etc/passwd
# -rw-r--r--

# Formato personalizado
stat -c 'Permisos: %a (%A) Owner: %U Group: %G' /etc/passwd
# Permisos: 644 (-rw-r--r--) Owner: root Group: root
```

## Cómo evalúa el kernel los permisos

El kernel evalúa los permisos en orden estricto y se detiene en la **primera
coincidencia**:

1. Si UID del proceso == UID del archivo → aplica permisos de **owner**
2. Si GID del proceso (primario o suplementario) == GID del archivo → aplica permisos de **group**
3. Ninguno coincide → aplica permisos de **others**

Esto significa que un propietario **puede tener menos permisos que el grupo**:

```bash
# alice es owner, developers es group
chmod 074 file.txt
ls -la file.txt
# ----rwxr-- 1 alice developers ... file.txt

# alice (owner) → permisos 0 (---) → NO puede leer
# bob (en developers) → permisos 7 (rwx) → puede todo
# charlie (others) → permisos 4 (r--) → puede leer
```

Esto es raro en la práctica pero importante para entender el modelo: el kernel
**no acumula permisos**, usa la primera categoría que coincide.

### Excepción: root (UID 0)

Root ignora los permisos de lectura y escritura en todos los casos:

```bash
# Archivo sin permisos para nadie
chmod 000 secret.txt

# Un usuario normal no puede acceder
cat secret.txt
# Permission denied

# root puede leerlo y escribirlo
sudo cat secret.txt
# (funciona)
```

Para ejecución, root tiene una regla diferente: puede ejecutar un archivo si
**cualquiera** de los tres bits `x` está activado (owner, group, u others).
Solo si ningún bit `x` está activo, root no puede ejecutar:

```bash
chmod 100 script.sh    # solo owner tiene x → root puede ejecutar
chmod 010 script.sh    # solo group tiene x → root puede ejecutar
chmod 001 script.sh    # solo others tiene x → root puede ejecutar
chmod 000 script.sh    # ningún x → root NO puede ejecutar
```

## Permisos por defecto y umask

Los archivos y directorios nuevos se crean con permisos que dependen de la
**umask** (cubierta en detalle en T04):

```bash
# Con umask 022 (default):
touch newfile
ls -la newfile
# -rw-r--r--  (644)

mkdir newdir
ls -ld newdir
# drwxr-xr-x  (755)
```

El mecanismo real es una operación **AND NOT** bitwise, no una sustracción.
Para umask `022` el resultado es igual (`666 & ~022 = 644`), pero para umask
`033` la sustracción daría `633` (incorrecto) mientras el AND NOT da `644`
(correcto). Más detalles en T04.

Los archivos nunca se crean con bit de ejecución — hay que agregarlo
explícitamente con `chmod +x`.

---

## Labs

### Parte 1 — Interpretar permisos

#### Paso 1.1: Leer permisos con ls -la

```bash
docker compose exec debian-dev bash -c '
echo "=== Archivos del sistema ==="
ls -la /etc/passwd
ls -la /etc/shadow
ls -la /usr/bin/passwd
ls -ld /tmp

echo ""
echo "=== Primer caracter = tipo de archivo ==="
echo "- = archivo regular"
echo "d = directorio"
echo "l = symlink"
echo "c = dispositivo de caracter"
echo "b = dispositivo de bloque"
'
```

El primer carácter no es un permiso sino el tipo de archivo. Los
9 caracteres siguientes son los permisos: owner, group, others.

#### Paso 1.2: stat para detalle

```bash
docker compose exec debian-dev bash -c '
echo "=== stat muestra octal y simbolico ==="
stat /etc/passwd
echo ""
echo "=== Solo octal ==="
stat -c "%a" /etc/passwd
echo ""
echo "=== Solo simbolico ==="
stat -c "%A" /etc/passwd
echo ""
echo "=== Formato personalizado ==="
stat -c "Permisos: %a (%A) Owner: %U Group: %G" /etc/passwd
'
```

#### Paso 1.3: Permisos de archivos críticos

```bash
docker compose exec debian-dev bash -c '
echo "=== Archivos criticos del sistema ==="
for f in /etc/passwd /etc/shadow /etc/group /etc/gshadow; do
    stat -c "%a %A %U:%G %n" "$f"
done

echo ""
echo "=== /etc/passwd: legible por todos (644) ==="
echo "=== /etc/shadow: solo root (640 o 000) ==="
'
```

#### Paso 1.4: Tipos de archivos especiales

```bash
docker compose exec debian-dev bash -c '
echo "=== Archivo regular ==="
stat -c "%F" /etc/passwd

echo ""
echo "=== Directorio ==="
stat -c "%F" /etc

echo ""
echo "=== Dispositivo de caracter ==="
stat -c "%F" /dev/null

echo ""
echo "=== Symlink (si existe) ==="
ls -la /usr/bin/awk 2>/dev/null | head -1 || echo "sin symlink"
'
```

### Parte 2 — Notación octal

#### Paso 2.1: Practicar conversiones

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear archivos con diferentes permisos ==="
for perm in 755 644 700 600 750 640 775 664; do
    touch "/tmp/perm-$perm"
    chmod "$perm" "/tmp/perm-$perm"
done

echo ""
echo "=== Verificar ==="
for perm in 755 644 700 600 750 640 775 664; do
    stat -c "%a %A %n" "/tmp/perm-$perm"
done

echo ""
rm -f /tmp/perm-*
'
```

Antes de verificar cada uno, predice mentalmente el resultado
simbólico.

### Parte 3 — Permisos en directorios

> **Nota**: Estos pasos se ejecutan como un usuario no-root para
> demostrar las restricciones. Root bypasea lectura/escritura.

#### Paso 3.1: Preparar usuario de prueba

```bash
docker compose exec debian-dev bash -c '
useradd -m -s /bin/bash permtester
mkdir -p /tmp/perm-dir
echo "contenido" > /tmp/perm-dir/file.txt
chmod 644 /tmp/perm-dir/file.txt
chown -R permtester:permtester /tmp/perm-dir
'
```

#### Paso 3.2: r sin x en directorio

```bash
docker compose exec debian-dev bash -c '
echo "=== Permisos: r-- para owner ==="
chmod 400 /tmp/perm-dir
ls -ld /tmp/perm-dir

echo ""
echo "=== Intentar listar (r = parcial, muestra nombres con errores) ==="
su -c "ls /tmp/perm-dir/" permtester 2>&1 || true

echo ""
echo "=== Intentar leer archivo dentro (sin x = no) ==="
su -c "cat /tmp/perm-dir/file.txt" permtester 2>&1 || true
echo "(sin x no puedes resolver la ruta dentro del directorio)"

chmod 755 /tmp/perm-dir
'
```

`r` en un directorio permite listar nombres de entradas. `x` permite
resolver rutas (acceder al inode). Sin `x`, puedes ver nombres pero
no acceder a los archivos.

#### Paso 3.3: x sin r en directorio

```bash
docker compose exec debian-dev bash -c '
echo "=== Permisos: --x para owner ==="
chmod 100 /tmp/perm-dir
ls -ld /tmp/perm-dir

echo ""
echo "=== Intentar listar (sin r = no) ==="
su -c "ls /tmp/perm-dir/" permtester 2>&1 || true

echo ""
echo "=== Acceder si conoces el nombre (con x = si) ==="
su -c "cat /tmp/perm-dir/file.txt" permtester 2>&1 || true
echo "(funciona porque x permite resolver la ruta)"

chmod 755 /tmp/perm-dir
'
```

#### Paso 3.4: w necesita x

```bash
docker compose exec debian-dev bash -c '
echo "=== Solo w (chmod 200) ==="
chmod 200 /tmp/perm-dir
su -c "touch /tmp/perm-dir/new.txt" permtester 2>&1 || true
echo "(falla — w sin x es inutil)"

echo ""
echo "=== w+x (chmod 300) ==="
chmod 300 /tmp/perm-dir
su -c "touch /tmp/perm-dir/new.txt" permtester 2>&1 || true
echo "(funciona — w+x permite crear)"

chmod 755 /tmp/perm-dir
rm -f /tmp/perm-dir/new.txt
'
```

#### Paso 3.5: El kernel no acumula permisos

```bash
docker compose exec debian-dev bash -c '
echo "=== Owner con MENOS permisos que group ==="
touch /tmp/trap-perm
chown permtester:permtester /tmp/trap-perm
chmod 074 /tmp/trap-perm
stat -c "%a %A" /tmp/trap-perm

echo ""
echo "=== permtester es owner (permisos 0 = ---) ==="
su -c "cat /tmp/trap-perm" permtester 2>&1 || true
echo "(DENEGADO — owner tiene --- aunque group tiene rwx)"

rm -f /tmp/trap-perm
'
```

### Limpieza

```bash
docker compose exec debian-dev bash -c '
rm -rf /tmp/perm-dir /tmp/perm-*
userdel -r permtester 2>/dev/null
echo "Limpieza completada"
'
```

---

## Ejercicios

### Ejercicio 1 — Interpretar permisos del sistema

```bash
docker compose exec debian-dev bash -c '
echo "=== Permisos de archivos criticos ==="
for f in /etc/passwd /etc/shadow /usr/bin/passwd /tmp; do
    stat -c "%a %A %n" "$f"
done
'
```

**Pregunta**: ¿Por qué `/etc/passwd` es legible por todos pero `/etc/shadow`
no? ¿Qué significan la `s` en `/usr/bin/passwd` y la `t` en `/tmp`?

<details><summary>Predicción</summary>

```
644 -rw-r--r-- /etc/passwd      → todos pueden leer (nombres, UIDs, shells)
640 -rw-r----- /etc/shadow      → solo root y grupo shadow (contiene hashes)
4755 -rwsr-xr-x /usr/bin/passwd → SUID: se ejecuta como root
1777 drwxrwxrwt /tmp            → sticky bit: solo el dueño puede borrar sus archivos
```

- `/etc/passwd` es legible porque los programas necesitan resolver UID→nombre.
  No contiene contraseñas (están en shadow).
- `/etc/shadow` es restrictivo porque contiene hashes de contraseñas.
- La `s` en `/usr/bin/passwd` es el **SUID bit** — permite a usuarios normales
  cambiar su propia contraseña ejecutando el binario como root.
- La `t` en `/tmp` es el **sticky bit** — evita que un usuario borre archivos
  de otro (cubierto en T03).

</details>

### Ejercicio 2 — Convertir entre notaciones

```bash
docker compose exec debian-dev bash -c '
echo "=== Convierte mentalmente antes de verificar ==="
echo ""

echo "rwxr-x--- = ?"
touch /tmp/conv1 && chmod 750 /tmp/conv1
stat -c "Respuesta: %a" /tmp/conv1

echo ""
echo "rw-rw-r-- = ?"
chmod 664 /tmp/conv1
stat -c "Respuesta: %a" /tmp/conv1

echo ""
echo "rwx--x--x = ?"
chmod 711 /tmp/conv1
stat -c "Respuesta: %a" /tmp/conv1

echo ""
echo "r-------- = ?"
chmod 400 /tmp/conv1
stat -c "Respuesta: %a" /tmp/conv1

rm -f /tmp/conv1
'
```

**Pregunta**: Convierte mentalmente cada permiso simbólico a octal antes de
ver la respuesta.

<details><summary>Predicción</summary>

```
rwxr-x--- = 750  (rwx=4+2+1=7, r-x=4+0+1=5, ---=0)
rw-rw-r-- = 664  (rw-=4+2+0=6, rw-=4+2+0=6, r--=4+0+0=4)
rwx--x--x = 711  (rwx=4+2+1=7, --x=0+0+1=1, --x=0+0+1=1)
r-------- = 400  (r--=4+0+0=4, ---=0, ---=0)
```

Truco: memorizar los valores clave (7=rwx, 6=rw-, 5=r-x, 4=r--, 0=---) y
los demás se derivan. Los más comunes en la práctica: 755, 644, 700, 600.

</details>

### Ejercicio 3 — r sin x en directorio

```bash
docker compose exec debian-dev bash -c '
useradd -m -s /bin/bash ex3user
mkdir /tmp/rx-test
echo "secreto" > /tmp/rx-test/data.txt
chown -R ex3user:ex3user /tmp/rx-test

echo "=== chmod 400 (r-- para owner) ==="
chmod 400 /tmp/rx-test

echo ""
echo "=== Intentar listar ==="
su -c "ls /tmp/rx-test/" ex3user 2>&1 || true

echo ""
echo "=== Intentar leer archivo ==="
su -c "cat /tmp/rx-test/data.txt" ex3user 2>&1 || true
'
```

**Pregunta**: Con permisos `r--` en el directorio, ¿puede `ex3user` listar
el directorio? ¿Puede leer `data.txt`?

<details><summary>Predicción</summary>

- **Listar**: **parcialmente**. `ls` muestra los nombres de las entradas
  pero con errores al intentar leer atributos (tamaño, permisos, fecha):
  ```
  ls: cannot access '/tmp/rx-test/data.txt': Permission denied
  data.txt
  ```
  Los nombres se leen del directorio (requiere `r`), pero los atributos
  están en el inode de cada archivo (requiere `x` para acceder al inode).

- **Leer `data.txt`**: **No**. `cat` necesita resolver la ruta
  `rx-test/data.txt`, lo cual requiere `x` en el directorio. Sin `x`,
  no puede acceder al inode del archivo → "Permission denied".

`r` sin `x` en un directorio es una combinación casi inútil en la práctica.

</details>

```bash
docker compose exec debian-dev bash -c '
chmod 755 /tmp/rx-test; rm -rf /tmp/rx-test
userdel -r ex3user
'
```

### Ejercicio 4 — x sin r en directorio

```bash
docker compose exec debian-dev bash -c '
useradd -m -s /bin/bash ex4user
mkdir /tmp/xr-test
echo "contenido" > /tmp/xr-test/known.txt
chmod 644 /tmp/xr-test/known.txt
chown -R ex4user:ex4user /tmp/xr-test

echo "=== chmod 100 (--x para owner) ==="
chmod 100 /tmp/xr-test

echo ""
echo "=== Intentar listar ==="
su -c "ls /tmp/xr-test/" ex4user 2>&1 || true

echo ""
echo "=== Acceder por nombre exacto ==="
su -c "cat /tmp/xr-test/known.txt" ex4user 2>&1 || true
'
```

**Pregunta**: Con permisos `--x` (solo execute) en el directorio, ¿puede
`ex4user` listar? ¿Puede leer `known.txt` si conoce el nombre?

<details><summary>Predicción</summary>

- **Listar**: **No**. `ls` requiere `r` para leer las entradas del directorio.
  ```
  ls: cannot open directory '/tmp/xr-test/': Permission denied
  ```

- **Leer `known.txt`**: **Sí** (si conoce el nombre exacto). `x` permite
  resolver la ruta, y el archivo tiene permisos `644` (legible por owner):
  ```
  contenido
  ```

Este patrón (`x` sin `r`) se usa a veces como seguridad por oscuridad: los
archivos son accesibles pero no se pueden descubrir sin conocer su nombre.
Ejemplo: `/home` en algunos sistemas tiene `711` — puedes acceder a
`/home/alice` si sabes que existe, pero no listar todos los usuarios.

</details>

```bash
docker compose exec debian-dev bash -c '
chmod 755 /tmp/xr-test; rm -rf /tmp/xr-test
userdel -r ex4user
'
```

### Ejercicio 5 — w necesita x para crear archivos

```bash
docker compose exec debian-dev bash -c '
useradd -m -s /bin/bash ex5user
mkdir /tmp/wx-test
chown ex5user:ex5user /tmp/wx-test

echo "=== Solo w (chmod 200) ==="
chmod 200 /tmp/wx-test
su -c "touch /tmp/wx-test/file1.txt" ex5user 2>&1 || true

echo ""
echo "=== w+x (chmod 300) ==="
chmod 300 /tmp/wx-test
su -c "touch /tmp/wx-test/file2.txt" ex5user 2>&1 || true
su -c "ls -la /tmp/wx-test/file2.txt" ex5user 2>&1 || true
'
```

**Pregunta**: ¿Por qué `touch` falla con `chmod 200` (solo `w`) pero
funciona con `chmod 300` (`wx`)?

<details><summary>Predicción</summary>

- Con `200` (`-w-------`): **falla**. El `touch` necesita resolver la ruta
  `/tmp/wx-test/file1.txt`, lo cual requiere `x` (traverse) en el directorio.
  Sin `x`, no puede ni entrar al directorio:
  ```
  touch: cannot touch '/tmp/wx-test/file1.txt': Permission denied
  ```

- Con `300` (`-wx------`): **funciona**. `x` permite resolver la ruta, y `w`
  permite crear la entrada en el directorio.

**Regla**: `w` en un directorio es inútil sin `x`. Para cualquier operación
de modificación (crear, eliminar, renombrar) dentro de un directorio,
se necesitan **ambos** `w` y `x`.

El `ls` del archivo también fallaría porque `300` no tiene `r` (no se puede
listar), pero si accedes por nombre funciona.

</details>

```bash
docker compose exec debian-dev bash -c '
chmod 755 /tmp/wx-test; rm -rf /tmp/wx-test
userdel -r ex5user
'
```

### Ejercicio 6 — Owner con menos permisos que group

```bash
docker compose exec debian-dev bash -c '
groupadd -f devgroup
useradd -m -s /bin/bash -G devgroup owneruser
useradd -m -s /bin/bash -G devgroup groupuser

touch /tmp/weird.txt
chown owneruser:devgroup /tmp/weird.txt
chmod 074 /tmp/weird.txt

echo "=== Permisos ==="
stat -c "%a %A %U:%G" /tmp/weird.txt

echo ""
echo "=== owneruser (owner, permisos ---) lee ==="
su -c "cat /tmp/weird.txt" owneruser 2>&1 || true

echo ""
echo "=== groupuser (en devgroup, permisos rwx) lee ==="
su -c "cat /tmp/weird.txt" groupuser 2>&1 || true
'
```

**Pregunta**: `owneruser` es el dueño del archivo y está en `devgroup`.
¿Puede leer el archivo?

<details><summary>Predicción</summary>

**No.** `owneruser` no puede leer el archivo, a pesar de estar en `devgroup`
que tiene permisos `rwx`.

```
owneruser: Permission denied
groupuser: (puede leer — tiene permisos de group rwx)
```

El kernel evalúa en orden: owner → group → others. `owneruser` coincide
como **owner** → aplica permisos de owner (`---` = nada) → **DENEGADO**.
El kernel nunca llega a evaluar los permisos de group.

Aunque `owneruser` también está en `devgroup`, el kernel se detiene en la
primera coincidencia. **No acumula permisos** de varias categorías.

`owneruser` puede arreglarlo con `chmod u+r /tmp/weird.txt` porque sí tiene
derecho a cambiar permisos de archivos que posee.

</details>

```bash
docker compose exec debian-dev bash -c '
rm -f /tmp/weird.txt
userdel -r owneruser; userdel -r groupuser
groupdel devgroup
'
```

### Ejercicio 7 — stat con formato personalizado

```bash
docker compose exec debian-dev bash -c '
echo "=== Formato personalizado de stat ==="
for f in /etc/passwd /etc/shadow /etc/group; do
    stat -c "%-20n %4a %A %U:%G" "$f"
done

echo ""
echo "=== Solo tipo de archivo ==="
for f in /etc/passwd /etc /dev/null /usr/bin/awk; do
    echo "$f → $(stat -c "%F" "$f" 2>/dev/null || echo "no existe")"
done
'
```

**Pregunta**: ¿Qué formato usa `stat -c` para cada campo? ¿Qué muestra `%F`?

<details><summary>Predicción</summary>

Campos de `stat -c`:
- `%n` = nombre del archivo
- `%a` = permisos en octal (ej: 644)
- `%A` = permisos en simbólico (ej: -rw-r--r--)
- `%U` = nombre del owner
- `%G` = nombre del grupo
- `%F` = tipo de archivo en texto legible

Salida esperada:
```
/etc/passwd           644 -rw-r--r-- root:root
/etc/shadow           640 -rw-r----- root:shadow
/etc/group            644 -rw-r--r-- root:root

/etc/passwd → regular file
/etc → directory
/dev/null → character special file
/usr/bin/awk → symbolic link (o regular file si no es symlink)
```

Otros campos útiles: `%u`/`%g` (UID/GID numéricos), `%s` (tamaño),
`%y` (última modificación).

</details>

### Ejercicio 8 — Eliminación en directorios ajenos

```bash
docker compose exec debian-dev bash -c '
useradd -m -s /bin/bash alice8
useradd -m -s /bin/bash bob8

mkdir /tmp/shared8
chmod 777 /tmp/shared8

echo "=== alice crea archivo protegido ==="
su -c "touch /tmp/shared8/alice-secret.txt && chmod 600 /tmp/shared8/alice-secret.txt" alice8
ls -la /tmp/shared8/alice-secret.txt

echo ""
echo "=== bob intenta leer ==="
su -c "cat /tmp/shared8/alice-secret.txt" bob8 2>&1 || true

echo ""
echo "=== bob intenta eliminar ==="
su -c "rm /tmp/shared8/alice-secret.txt" bob8 2>&1 || true
ls /tmp/shared8/ 2>/dev/null || echo "(directorio vacio)"
'
```

**Pregunta**: bob no puede leer el archivo de alice (`chmod 600`). ¿Puede
eliminarlo? ¿Por qué?

<details><summary>Predicción</summary>

**Sí, bob puede eliminarlo.** El permiso de eliminar un archivo depende
del **directorio**, no del archivo:

- ¿bob puede leer `alice-secret.txt`? → **No** (permisos `600`, bob es others
  = `---`)
- ¿bob puede eliminar `alice-secret.txt`? → **Sí** (directorio tiene `777`,
  bob tiene `w+x` en el directorio)

```
bob: cat → Permission denied
bob: rm → (eliminado exitosamente)
```

La operación `rm` modifica la **entrada del directorio** (quita el link al
inode), no modifica el archivo en sí. El permiso `w` en el directorio es lo
que controla esto.

Para evitar este problema, se usa el **sticky bit** (`chmod +t` o `1777`),
que restringe la eliminación: solo el owner del archivo, el owner del
directorio, o root pueden eliminar.

</details>

```bash
docker compose exec debian-dev bash -c '
rm -rf /tmp/shared8
userdel -r alice8; userdel -r bob8
'
```

### Ejercicio 9 — Root y el bit de ejecución

```bash
docker compose exec debian-dev bash -c '
echo "#!/bin/bash" > /tmp/rootx-test.sh
echo "echo ejecutado" >> /tmp/rootx-test.sh

echo "=== chmod 000 (ningun permiso) ==="
chmod 000 /tmp/rootx-test.sh
/tmp/rootx-test.sh 2>&1 || true
echo "Root no puede ejecutar sin ningun bit x"

echo ""
echo "=== chmod 001 (solo others tiene x) ==="
chmod 001 /tmp/rootx-test.sh
/tmp/rootx-test.sh 2>&1 || true

echo ""
echo "=== Pero root SI puede leer con 000 ==="
chmod 000 /tmp/rootx-test.sh
cat /tmp/rootx-test.sh
'
```

**Pregunta**: ¿Cuál es la diferencia entre cómo root trata r/w vs x?

<details><summary>Predicción</summary>

Root tiene reglas diferentes para lectura/escritura vs ejecución:

- **r y w**: root **siempre** puede leer y escribir, sin importar permisos.
  Incluso con `chmod 000`, root accede sin problema.

- **x**: root puede ejecutar **solo si al menos un bit x está activado**
  (owner, group, o others). Con `chmod 000`, root **no puede ejecutar**.

```
chmod 000: cat → funciona, ./script.sh → Permission denied
chmod 001: cat → funciona, ./script.sh → "ejecutado"
```

La razón: ejecutar código arbitrario es más peligroso que leer datos.
El kernel protege contra ejecución accidental de archivos no marcados
como ejecutables, incluso para root. Pero leer/escribir siempre está
permitido porque root necesita administrar todo el sistema.

</details>

```bash
docker compose exec debian-dev bash -c 'rm -f /tmp/rootx-test.sh'
```

### Ejercicio 10 — Auditoría de permisos inseguros

```bash
docker compose exec debian-dev bash -c '
echo "=== Buscar archivos world-writable en /etc ==="
find /etc -perm -002 -type f 2>/dev/null | head -5 || echo "(ninguno encontrado)"

echo ""
echo "=== Buscar directorios world-writable sin sticky bit ==="
find /tmp -maxdepth 1 -perm -002 -not -perm -1000 -type d 2>/dev/null | head -5 || echo "(ninguno encontrado)"

echo ""
echo "=== Archivos con SUID ==="
find /usr -perm -4000 -type f 2>/dev/null | head -5

echo ""
echo "=== Permisos de /tmp ==="
stat -c "%a %A" /tmp
'
```

**Pregunta**: ¿Qué busca `-perm -002`? ¿Y `-perm -002 -not -perm -1000`?
¿Por qué son preocupantes estos archivos?

<details><summary>Predicción</summary>

- **`-perm -002`**: busca archivos donde `others` tiene permiso de **escritura**
  (el bit `w` de others está activado). El `-` significa "al menos estos bits".

- **`-perm -002 -not -perm -1000`**: busca directorios world-writable que **no
  tienen sticky bit**. `-1000` es el sticky bit en octal.

Son preocupantes porque:
- Archivos world-writable en `/etc` → cualquier usuario puede modificar
  configuración del sistema (escalamiento de privilegios).
- Directorios world-writable sin sticky → cualquier usuario puede eliminar
  archivos de otros (pérdida de datos, DoS).

`/tmp` debería ser `1777` (sticky bit + world-writable). El sticky bit
permite que todos creen archivos pero solo el owner pueda eliminar los suyos.
Esto se cubre en detalle en T03.

</details>
