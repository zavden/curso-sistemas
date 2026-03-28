# Lab — ACLs

## Objetivo

Usar Access Control Lists para dar permisos granulares por usuario
y grupo: setfacl, getfacl, ACLs por defecto en directorios, y la
mascara efectiva.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — ACLs basicas

### Objetivo

Establecer y verificar ACLs en archivos individuales.

### Paso 1.1: Archivo sin ACLs

```bash
docker compose exec debian-dev bash -c '
touch /tmp/acl-test
chmod 640 /tmp/acl-test

echo "=== Sin ACLs ==="
getfacl /tmp/acl-test

echo ""
echo "=== ls -la (sin +) ==="
ls -la /tmp/acl-test
'
```

Sin ACLs, `getfacl` muestra solo los permisos Unix tradicionales.
No hay `+` al final de los permisos en `ls -la`.

### Paso 1.2: Agregar ACL de usuario

```bash
docker compose exec debian-dev bash -c '
echo "=== Agregar lectura para nobody ==="
setfacl -m u:nobody:r /tmp/acl-test

echo ""
echo "=== getfacl muestra la ACL ==="
getfacl /tmp/acl-test

echo ""
echo "=== ls -la muestra el + ==="
ls -la /tmp/acl-test
echo "(el + al final indica ACLs extendidas)"
'
```

`setfacl -m u:usuario:permisos` agrega una ACL de usuario. El `+`
en `ls -la` indica que el archivo tiene ACLs.

### Paso 1.3: Agregar ACL de grupo

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear grupo de prueba ==="
groupadd -f aclteam

echo ""
echo "=== Agregar ACL de grupo ==="
setfacl -m g:aclteam:rw /tmp/acl-test

echo ""
echo "=== Verificar ==="
getfacl /tmp/acl-test
'
```

`setfacl -m g:grupo:permisos` agrega una ACL de grupo. Se pueden
tener multiples ACLs de usuario y grupo en un mismo archivo.

### Paso 1.4: Multiples ACLs a la vez

```bash
docker compose exec debian-dev bash -c '
echo "=== Establecer varias ACLs ==="
useradd -m acluser1 2>/dev/null
useradd -m acluser2 2>/dev/null

setfacl -m u:acluser1:r,u:acluser2:rw /tmp/acl-test

echo ""
echo "=== Todas las ACLs ==="
getfacl /tmp/acl-test
'
```

Las ACLs separadas por coma se aplican en un solo comando.

### Paso 1.5: Eliminar ACLs

```bash
docker compose exec debian-dev bash -c '
echo "=== Eliminar ACL de un usuario ==="
setfacl -x u:nobody /tmp/acl-test
getfacl /tmp/acl-test
echo "(nobody ya no aparece)"

echo ""
echo "=== Eliminar TODAS las ACLs ==="
setfacl -b /tmp/acl-test
getfacl /tmp/acl-test
echo "(solo quedan permisos Unix)"

echo ""
echo "=== El + desaparecio ==="
ls -la /tmp/acl-test
'
```

`setfacl -x` elimina una ACL especifica. `setfacl -b` elimina todas.

---

## Parte 2 — ACLs por defecto

### Objetivo

Configurar ACLs por defecto en directorios para herencia automatica.

### Paso 2.1: Crear directorio con ACL por defecto

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear directorio ==="
mkdir /tmp/acl-dir
chmod 750 /tmp/acl-dir

echo ""
echo "=== Establecer ACL por defecto ==="
setfacl -d -m u:nobody:r /tmp/acl-dir
setfacl -d -m g:aclteam:rw /tmp/acl-dir

echo ""
echo "=== Verificar ==="
getfacl /tmp/acl-dir
echo "(las lineas default: son las ACLs por defecto)"
'
```

`setfacl -d -m` establece ACLs por defecto. Se aplican
automaticamente a archivos y subdirectorios nuevos.

### Paso 2.2: Verificar herencia en archivos

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear archivo dentro ==="
touch /tmp/acl-dir/file1.txt

echo ""
echo "=== ACLs heredadas ==="
getfacl /tmp/acl-dir/file1.txt
echo "(heredo las ACLs por defecto del directorio padre)"
'
```

### Paso 2.3: Herencia en subdirectorios

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear subdirectorio ==="
mkdir /tmp/acl-dir/subdir

echo ""
echo "=== ACLs del subdirectorio ==="
getfacl /tmp/acl-dir/subdir
echo "(heredo ACLs regulares Y por defecto — la herencia se propaga)"

echo ""
echo "=== Archivo en el subdirectorio ==="
touch /tmp/acl-dir/subdir/file2.txt
getfacl /tmp/acl-dir/subdir/file2.txt
echo "(tambien heredo las ACLs)"
'
```

Los subdirectorios heredan las ACLs por defecto del padre y las
establecen como sus propias ACLs por defecto. La herencia se
propaga recursivamente.

### Paso 2.4: Aplicar ACLs recursivamente

```bash
docker compose exec debian-dev bash -c '
echo "=== Aplicar ACL a todo lo existente ==="
setfacl -R -m u:acluser1:r /tmp/acl-dir

echo ""
echo "=== Aplicar ACL por defecto a todos los directorios ==="
setfacl -R -d -m u:acluser1:r /tmp/acl-dir

echo ""
echo "=== Verificar ==="
echo "--- Directorio ---"
getfacl /tmp/acl-dir | grep acluser1
echo "--- Archivo ---"
getfacl /tmp/acl-dir/file1.txt | grep acluser1
echo "--- Subdirectorio ---"
getfacl /tmp/acl-dir/subdir | grep acluser1
'
```

`-R` aplica recursivamente. Combinar con `-d` para establecer
defaults en todos los directorios existentes.

---

## Parte 3 — Mascara y limpieza

### Objetivo

Entender la mascara efectiva y su efecto en los permisos reales.

### Paso 3.1: La mascara

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear archivo con ACLs ==="
touch /tmp/mask-test
setfacl -m u:nobody:rw /tmp/mask-test
setfacl -m g:aclteam:rw /tmp/mask-test

echo ""
echo "=== ACLs actuales ==="
getfacl /tmp/mask-test
echo "(mask::rw- permite rw)"
'
```

La mascara es el limite maximo de permisos para las entradas ACL
(excepto owner y others).

### Paso 3.2: Restringir la mascara

```bash
docker compose exec debian-dev bash -c '
echo "=== Restringir mascara a solo lectura ==="
setfacl -m m::r /tmp/mask-test

echo ""
echo "=== Verificar ==="
getfacl /tmp/mask-test
echo "(los permisos muestran #effective:r-- aunque la ACL dice rw-)"
echo "(la mascara limita el acceso real)"
'
```

La mascara actua como un techo. Aunque la ACL de nobody dice `rw-`,
el acceso efectivo es solo `r--` porque la mascara lo limita.

### Paso 3.3: chmod modifica la mascara

```bash
docker compose exec debian-dev bash -c '
echo "=== chmod g=rw en archivo con ACLs ==="
chmod g=rw /tmp/mask-test

echo ""
echo "=== getfacl ==="
getfacl /tmp/mask-test
echo ""
echo "chmod modifica la MASCARA, no los permisos del grupo propietario"
echo "(interaccion sutil entre chmod y ACLs)"

rm -f /tmp/mask-test
'
```

En archivos con ACLs, `chmod g=...` modifica la mascara, no los
permisos del grupo propietario. Es una interaccion que causa
confusion frecuente.

### Paso 3.4: Backup y restauracion de ACLs

```bash
docker compose exec debian-dev bash -c '
echo "=== Guardar ACLs ==="
getfacl -R /tmp/acl-dir > /tmp/acl-backup.txt
cat /tmp/acl-backup.txt | head -15
echo "..."

echo ""
echo "=== Eliminar todas las ACLs ==="
setfacl -R -b /tmp/acl-dir
echo "ACLs eliminadas. Verificar:"
getfacl /tmp/acl-dir/file1.txt

echo ""
echo "=== Restaurar desde backup ==="
cd /tmp && setfacl --restore=acl-backup.txt
echo "ACLs restauradas. Verificar:"
getfacl /tmp/acl-dir/file1.txt

rm -f /tmp/acl-backup.txt
'
```

`getfacl -R > backup` guarda las ACLs. `setfacl --restore=backup`
las restaura. Util antes de operaciones que puedan perder ACLs.

---

## Limpieza final

```bash
docker compose exec debian-dev bash -c '
rm -rf /tmp/acl-test /tmp/acl-dir /tmp/mask-test /tmp/acl-backup.txt 2>/dev/null
userdel -r acluser1 2>/dev/null
userdel -r acluser2 2>/dev/null
groupdel aclteam 2>/dev/null
echo "Limpieza completada"
'
```

---

## Conceptos reforzados

1. Las ACLs extienden el modelo Unix permitiendo permisos por
   **usuario y grupo adicionales**. El `+` en `ls -la` indica
   ACLs presentes.

2. `setfacl -m` agrega ACLs. `setfacl -x` elimina una ACL especifica.
   `setfacl -b` elimina todas.

3. Las ACLs **por defecto** (`setfacl -d -m`) se heredan
   automaticamente a archivos y subdirectorios nuevos.

4. La **mascara** es el techo de permisos para todas las entradas
   ACL (excepto owner y others). `chmod g=` modifica la mascara,
   no el grupo.

5. `getfacl -R > backup` y `setfacl --restore=backup` permiten
   guardar y restaurar ACLs.
