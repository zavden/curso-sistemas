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
| `x` (execute) | Ejecutar como programa | Acceder al directorio (`cd`), acceder a archivos dentro |

Esto tiene implicaciones no obvias:

```bash
# Directorio con r pero sin x: puedes listar nombres pero no acceder
mkdir testdir && chmod 640 testdir
ls testdir/
# (muestra nombres de archivos, pero con errores al intentar leer atributos)
cat testdir/file.txt
# Permission denied (no puedes acceder sin x)

# Directorio con x pero sin r: puedes acceder si conoces el nombre
chmod 710 testdir
ls testdir/
# Permission denied (no puedes listar)
cat testdir/file.txt
# (funciona si conoces el nombre exacto)
```

### La combinación más contraintuitiva

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

# Pero bob SÍ puede ELIMINARLO (porque tiene w en el directorio)
rm /tmp/shared/secret.txt
# (eliminado)
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
no acumula permisos, usa la primera categoría que coincide.

### Excepción: root

El UID 0 (root) ignora los permisos de lectura y escritura:

```bash
# Archivo sin permisos para nadie
chmod 000 secret.txt

# Un usuario normal no puede acceder
cat secret.txt
# Permission denied

# root puede leerlo
sudo cat secret.txt
# (funciona)
```

Root respeta el bit de ejecución solo parcialmente: no puede ejecutar un
archivo sin ningún bit `x`, pero puede leer y escribir cualquier archivo
independientemente de sus permisos.

## Permisos por defecto

Los archivos y directorios nuevos se crean con permisos que dependen de la
**umask** (cubierta en T04):

```bash
# Con umask 022 (default):
touch newfile
ls -la newfile
# -rw-r--r--  (666 - 022 = 644)

mkdir newdir
ls -ld newdir
# drwxr-xr-x  (777 - 022 = 755)
```

Los archivos nunca se crean con bit de ejecución — hay que agregarlo
explícitamente con `chmod +x`.

---

## Ejercicios

### Ejercicio 1 — Interpretar permisos

```bash
# ¿Qué significan estos permisos?
ls -la /etc/shadow
# ¿Quién puede leer? ¿Quién puede escribir?

ls -la /tmp
# ¿Qué significa la 't' al final?

ls -la /usr/bin/passwd
# ¿Qué significa la 's' en la posición de x del owner?
```

### Ejercicio 2 — Convertir entre notaciones

```bash
# Practica mentalmente:
# rwxr-x--- = ?
# rw-rw-r-- = ?
# rwx--x--x = ?

# Verificar con stat
touch /tmp/perm-test
chmod 750 /tmp/perm-test && stat -c '%a %A' /tmp/perm-test
chmod 664 /tmp/perm-test && stat -c '%a %A' /tmp/perm-test
chmod 711 /tmp/perm-test && stat -c '%a %A' /tmp/perm-test
rm /tmp/perm-test
```

### Ejercicio 3 — Permisos en directorios

```bash
# Crear directorio de prueba
mkdir /tmp/perm-dir
echo "test" > /tmp/perm-dir/file.txt

# Quitar x del directorio — ¿puedes leer el archivo?
chmod 660 /tmp/perm-dir
cat /tmp/perm-dir/file.txt

# Quitar r, dejar x — ¿puedes listar? ¿puedes leer el archivo?
chmod 710 /tmp/perm-dir
ls /tmp/perm-dir/
cat /tmp/perm-dir/file.txt

# Restaurar y limpiar
chmod 755 /tmp/perm-dir
rm -rf /tmp/perm-dir
```
