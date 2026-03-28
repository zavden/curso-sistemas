# Lab — chmod y chown

## Objetivo

Cambiar permisos con chmod (sintaxis octal y simbolica) y
propietarios con chown/chgrp. Practicar permisos recursivos
correctos con find y la X mayuscula.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — chmod octal y simbolica

### Objetivo

Practicar ambas sintaxis y entender cuando usar cada una.

### Paso 1.1: Sintaxis octal

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

### Paso 1.2: Sintaxis simbolica

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
'
```

La sintaxis simbolica es expresiva para cambios incrementales.
`+` agrega, `-` quita, `=` establece exactamente.

### Paso 1.3: Agregar para todos

```bash
docker compose exec debian-dev bash -c '
chmod 000 /tmp/chmod-test

echo "=== Agregar lectura para todos ==="
chmod a+r /tmp/chmod-test && stat -c "%a %A" /tmp/chmod-test

echo ""
echo "=== +x sin especificar quien = a+x ==="
chmod +x /tmp/chmod-test && stat -c "%a %A" /tmp/chmod-test

rm -f /tmp/chmod-test
'
```

`a` = all (u+g+o). Omitir la letra tambien aplica a todos.

---

## Parte 2 — chown y chgrp

### Objetivo

Cambiar propietario y grupo, entender las restricciones de
usuarios regulares.

### Paso 2.1: Cambiar owner y grupo

```bash
docker compose exec debian-dev bash -c '
useradd -m chowntest 2>/dev/null
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
echo "=== Cambiar solo grupo (equivalente a chgrp) ==="
chown :chowntest /tmp/chown-file
ls -la /tmp/chown-file

rm -f /tmp/chown-file
userdel -r chowntest 2>/dev/null
'
```

`chown usuario:grupo` cambia ambos. `chown :grupo` cambia solo
el grupo. `chown usuario:` cambia owner y grupo al grupo primario
del nuevo owner.

### Paso 2.2: chgrp

```bash
docker compose exec debian-dev bash -c '
touch /tmp/chgrp-test

echo "=== chgrp es equivalente a chown :grupo ==="
chgrp root /tmp/chgrp-test
ls -la /tmp/chgrp-test

rm -f /tmp/chgrp-test
'
```

### Paso 2.3: Restricciones de usuarios regulares

```bash
docker compose exec debian-dev bash -c '
echo "=== Solo root puede cambiar el owner ==="
echo "(un usuario regular no puede dar sus archivos a otro)"
echo "Razon: evitar que alguien ocupe la cuota de disco de otro"

echo ""
echo "=== Un usuario puede cambiar el grupo de SUS archivos ==="
echo "(pero solo a un grupo al que pertenezca)"
echo ""
echo "Grupos de dev: $(id -Gn dev)"
'
```

---

## Parte 3 — Recursivo y edge cases

### Objetivo

Aplicar permisos recursivamente de forma correcta y entender
peculiaridades de symlinks y hard links.

### Paso 3.1: El problema de chmod -R

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

`chmod -R` aplica los mismos permisos a archivos y directorios.
Pero los directorios necesitan `x` y los archivos generalmente no.

### Paso 3.2: find + chmod (correcto)

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

`find -type d` para directorios, `find -type f` para archivos.
Este es el patron correcto para aplicar permisos recursivamente.

### Paso 3.3: chmod con X mayuscula

```bash
docker compose exec debian-dev bash -c '
echo "=== Resetear todo a 600 ==="
chmod -R 600 /tmp/webdir
find /tmp/webdir -exec stat -c "%a %n" {} \;
echo "(todo 600 — los directorios no son accesibles)"

echo ""
echo "=== chmod -R a+X (solo agrega x a directorios) ==="
chmod -R u=rwX,g=rX,o=rX /tmp/webdir

echo ""
echo "=== Verificar ==="
find /tmp/webdir -exec stat -c "%a %A %n" {} \;
echo ""
echo "(directorios tienen x, archivos NO)"
'
```

`X` (mayuscula) agrega ejecucion **solo a directorios y archivos
que ya tengan algun bit x**. Es la forma idiomatica de aplicar
permisos recursivos sin dar x accidental a archivos.

### Paso 3.4: Permisos de symlinks

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear symlink ==="
echo "test" > /tmp/webdir/target.txt
chmod 644 /tmp/webdir/target.txt
ln -s /tmp/webdir/target.txt /tmp/webdir/link.txt

echo ""
echo "=== Permisos del symlink (siempre 777) ==="
ls -la /tmp/webdir/link.txt
echo "(los permisos del symlink no importan)"

echo ""
echo "=== chmod sobre symlink modifica el TARGET ==="
chmod 600 /tmp/webdir/link.txt
stat -c "%a %n" /tmp/webdir/target.txt
echo "(el target cambio a 600)"

rm -rf /tmp/webdir
'
```

Los permisos de un symlink no se usan. El kernel sigue el link y
aplica los permisos del archivo destino. `chmod` sobre un symlink
modifica el target.

### Paso 3.5: Hard links comparten permisos

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear hard link ==="
echo "test" > /tmp/hl-original
ln /tmp/hl-original /tmp/hl-link

echo ""
echo "=== Cambiar permisos del link ==="
chmod 600 /tmp/hl-link

echo ""
echo "=== Ambos cambiaron (mismo inodo) ==="
stat -c "%a %i %n" /tmp/hl-original /tmp/hl-link
echo "(mismo inodo = mismo archivo = mismos permisos)"

rm -f /tmp/hl-original /tmp/hl-link
'
```

Los hard links apuntan al mismo inodo, asi que comparten permisos.
Cambiar uno afecta a todos.

---

## Limpieza final

```bash
docker compose exec debian-dev bash -c '
rm -rf /tmp/webdir /tmp/chmod-test /tmp/chown-file /tmp/chgrp-test 2>/dev/null
rm -f /tmp/hl-original /tmp/hl-link 2>/dev/null
echo "Limpieza completada"
'
```

---

## Conceptos reforzados

1. chmod tiene dos sintaxis: **octal** (predecible, para establecer
   desde cero) y **simbolica** (expresiva, para cambios incrementales).

2. Solo root puede cambiar el **owner** de un archivo. Un usuario
   regular puede cambiar el **grupo** solo a un grupo al que
   pertenezca.

3. `chmod -R` aplica los mismos permisos a todo. Usar `find -type d`
   y `find -type f` para diferenciar directorios y archivos.

4. `X` mayuscula agrega ejecucion solo a directorios (y archivos
   que ya tengan x). Es la forma correcta de `chmod -R`.

5. Los permisos de **symlinks** no importan (el kernel usa los del
   target). Los **hard links** comparten permisos (mismo inodo).
