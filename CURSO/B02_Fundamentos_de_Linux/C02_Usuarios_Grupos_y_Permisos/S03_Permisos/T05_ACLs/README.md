# T05 — ACLs

## Limitaciones del modelo Unix

Los permisos tradicionales Unix (owner/group/others) tienen una limitación
fundamental: solo permiten **un owner y un grupo** por archivo. Si se necesita
dar acceso a dos grupos diferentes, o a un usuario específico sin cambiar la
propiedad, el modelo no alcanza.

```
Problema: alice (grupo dev) y bob (grupo finance) necesitan acceso
a report.csv, pero con diferentes permisos.

Con permisos Unix:
- owner = alice → solo alice tiene permisos de owner
- group = dev → solo dev tiene permisos de grupo
- bob necesitaría estar en dev O tener permisos via others
- Dar permisos via others los da a TODO el sistema
```

Las **ACLs** (Access Control Lists) resuelven esto añadiendo entradas de
permisos por usuario o por grupo adicionales.

## Soporte de ACLs

Las ACLs son una extensión del filesystem. Los filesystems principales las
soportan:

```bash
# Verificar si el filesystem soporta ACLs
mount | grep acl
# /dev/sda2 on / type ext4 (rw,...,acl,...)

# En ext4 y XFS, ACLs están habilitadas por defecto
# En otros filesystems, puede necesitar mount -o acl
```

Las herramientas necesarias vienen en el paquete `acl`:

```bash
# Debian
sudo apt install acl

# RHEL (normalmente ya instalado)
sudo dnf install acl
```

## getfacl — Ver ACLs

```bash
# Archivo sin ACLs (solo permisos Unix)
getfacl file.txt
# # file: file.txt
# # owner: alice
# # group: dev
# user::rw-       ← permisos del owner
# group::r--      ← permisos del grupo
# other::r--      ← permisos de others

# Archivo con ACLs
getfacl report.csv
# # file: report.csv
# # owner: alice
# # group: dev
# user::rw-           ← owner (alice)
# user:bob:rw-        ← ACL: bob tiene rw
# group::r--          ← grupo propietario (dev)
# group:finance:r--   ← ACL: grupo finance tiene r
# mask::rw-           ← máscara efectiva
# other::---          ← others
```

### Detectar archivos con ACLs

```bash
# El + al final de los permisos indica ACLs presentes
ls -la report.csv
# -rw-rw-r--+ 1 alice dev ... report.csv
#           ^
#           + = tiene ACLs extendidas
```

## setfacl — Establecer ACLs

### Agregar permisos para un usuario

```bash
# Dar permisos rw a bob
setfacl -m u:bob:rw report.csv

# Verificar
getfacl report.csv
# user:bob:rw-     ← nueva entrada

# Dar solo lectura a charlie
setfacl -m u:charlie:r report.csv
```

### Agregar permisos para un grupo

```bash
# Dar lectura al grupo finance
setfacl -m g:finance:r report.csv

# Dar rw al grupo editors
setfacl -m g:editors:rw report.csv
```

### Sintaxis de setfacl

```
setfacl -m TIPO:NOMBRE:PERMISOS archivo
```

| Tipo | Descripción | Ejemplo |
|---|---|---|
| `u:nombre:perms` | ACL de usuario | `u:bob:rw` |
| `g:nombre:perms` | ACL de grupo | `g:finance:r` |
| `o::perms` | Others | `o::---` |
| `m::perms` | Máscara | `m::rw` |

### Múltiples entradas

```bash
# Establecer varias ACLs a la vez
setfacl -m u:bob:rw,g:finance:r,g:editors:rw report.csv
```

### Eliminar ACLs

```bash
# Eliminar la ACL de un usuario específico
setfacl -x u:bob report.csv

# Eliminar la ACL de un grupo específico
setfacl -x g:finance report.csv

# Eliminar TODAS las ACLs (volver a permisos Unix puros)
setfacl -b report.csv

# Verificar que el + desapareció
ls -la report.csv
# -rw-r--r-- 1 alice dev ... report.csv  (sin +)
```

## La máscara efectiva

La **mask** es el límite máximo de permisos para todas las entradas ACL
(excepto owner y others). Funciona como un techo:

```bash
getfacl file.txt
# user::rw-
# user:bob:rw-         #effective:r--
# group::rw-           #effective:r--
# group:finance:rw-    #effective:r--
# mask::r--            ← la máscara limita a solo lectura
# other::---
```

Aunque bob tiene `rw-` en su ACL, la máscara `r--` limita su acceso efectivo
a solo lectura.

### Cómo se establece la máscara

```bash
# La máscara se recalcula automáticamente al agregar ACLs
setfacl -m u:bob:rw file.txt
# mask se ajusta a rw (para permitir el rw de bob)

# Establecer máscara manualmente
setfacl -m m::r file.txt
# Ahora todas las ACLs se limitan a lectura máximo

# chmod sobre el grupo también modifica la máscara
chmod g=r file.txt
# Equivale a setfacl -m m::r
```

Esto es una interacción sutil: `chmod g=rw` en un archivo con ACLs modifica
la **máscara**, no los permisos del grupo propietario.

## ACLs en directorios

### ACLs regulares

Funcionan igual que en archivos:

```bash
# Dar acceso a bob al directorio
setfacl -m u:bob:rx /srv/project
```

### ACLs por defecto (default ACLs)

Las ACLs por defecto se aplican automáticamente a los archivos y subdirectorios
**nuevos** creados dentro del directorio:

```bash
# Establecer ACL por defecto: bob tendrá rw en todo archivo nuevo
setfacl -d -m u:bob:rw /srv/project

# Verificar
getfacl /srv/project
# # file: srv/project
# # owner: alice
# # group: dev
# user::rwx
# group::r-x
# other::r-x
# default:user::rwx        ← default para owner
# default:user:bob:rw-     ← default para bob
# default:group::r-x       ← default para grupo
# default:mask::rwx         ← default máscara
# default:other::r-x       ← default para others

# Ahora crear un archivo dentro
touch /srv/project/new-file.txt
getfacl /srv/project/new-file.txt
# user:bob:rw-    ← heredó la ACL por defecto
```

### Herencia de defaults

Los subdirectorios nuevos heredan las ACLs por defecto del padre **y también
las establecen como sus propias ACLs por defecto** — la herencia se propaga
recursivamente:

```bash
mkdir /srv/project/subdir
getfacl /srv/project/subdir
# default:user:bob:rw-    ← heredó y propagará
```

### Aplicar ACLs recursivamente

```bash
# Aplicar ACL a todos los archivos y directorios existentes
setfacl -R -m u:bob:rw /srv/project

# Aplicar ACL por defecto a todos los directorios existentes
setfacl -R -d -m u:bob:rw /srv/project
```

## Backup y restauración de ACLs

```bash
# Guardar ACLs a un archivo
getfacl -R /srv/project > acl-backup.txt

# Restaurar ACLs desde el archivo
setfacl --restore=acl-backup.txt
```

Importante: `tar` y `rsync` pueden preservar ACLs con flags específicos:

```bash
# tar con ACLs
tar --acls -czf backup.tar.gz /srv/project

# rsync con ACLs
rsync -a --acls /srv/project/ remote:/srv/project/

# cp con ACLs
cp -a --preserve=all source dest
# o
cp --preserve=xattr source dest
```

## ACLs vs permisos Unix

| Aspecto | Permisos Unix | ACLs |
|---|---|---|
| Granularidad | 1 owner, 1 grupo, others | Múltiples usuarios y grupos |
| Herencia | No (solo SGID para grupo) | Sí (ACLs por defecto) |
| Complejidad | Simple | Más compleja, más difícil de auditar |
| Rendimiento | Mínimo overhead | Ligero overhead en filesystem |
| Compatibilidad | Universal | Requiere soporte de filesystem |

Regla general: usar permisos Unix cuando sean suficientes. Agregar ACLs solo
cuando se necesita granularidad que Unix no puede dar.

---

## Ejercicios

### Ejercicio 1 — ACLs básicas

```bash
# Crear archivo
touch /tmp/acl-test
chmod 640 /tmp/acl-test

# Agregar ACL para un usuario (usa un usuario del sistema como prueba)
setfacl -m u:nobody:r /tmp/acl-test

# Verificar
getfacl /tmp/acl-test
ls -la /tmp/acl-test   # ¿Ves el + ?

# Eliminar la ACL
setfacl -x u:nobody /tmp/acl-test
getfacl /tmp/acl-test

rm /tmp/acl-test
```

### Ejercicio 2 — ACLs por defecto en directorios

```bash
# Crear directorio con ACL por defecto
mkdir /tmp/acl-dir
setfacl -d -m u:nobody:r /tmp/acl-dir

# Crear archivos dentro — ¿heredan la ACL?
touch /tmp/acl-dir/file1
mkdir /tmp/acl-dir/subdir
touch /tmp/acl-dir/subdir/file2

getfacl /tmp/acl-dir/file1
getfacl /tmp/acl-dir/subdir
getfacl /tmp/acl-dir/subdir/file2

# Limpiar
rm -rf /tmp/acl-dir
```

### Ejercicio 3 — La máscara

```bash
touch /tmp/mask-test
setfacl -m u:nobody:rw /tmp/mask-test

# Ver la máscara
getfacl /tmp/mask-test

# Restringir la máscara a solo lectura
setfacl -m m::r /tmp/mask-test

# ¿Cuál es el permiso efectivo de nobody ahora?
getfacl /tmp/mask-test

rm /tmp/mask-test
```
