# Lab — rwx y notación octal

## Objetivo

Interpretar permisos en formato simbolico y octal, convertir entre
ambas notaciones, entender la diferencia entre permisos en archivos
y directorios, y verificar como el kernel evalua los permisos.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Interpretar permisos

### Objetivo

Leer permisos con ls -la y stat, e identificar el tipo de archivo.

### Paso 1.1: Leer permisos con ls -la

```bash
docker compose exec debian-dev bash -c '
echo "=== Archivos del sistema ==="
ls -la /etc/passwd
ls -la /etc/shadow
ls -la /usr/bin/passwd
ls -la /tmp

echo ""
echo "=== Primer caracter = tipo de archivo ==="
echo "- = archivo regular"
echo "d = directorio"
echo "l = symlink"
echo "c = dispositivo de caracter"
echo "b = dispositivo de bloque"
'
```

El primer caracter no es un permiso sino el tipo de archivo. Los
9 caracteres siguientes son los permisos: owner, group, others.

### Paso 1.2: stat para detalle

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

`stat -c '%a'` muestra solo el octal. `stat -c '%A'` muestra solo
el simbolico. `stat` sin flags muestra todo el detalle.

### Paso 1.3: Diferentes tipos de archivos

```bash
docker compose exec debian-dev bash -c '
echo "=== Archivo regular ==="
ls -la /etc/passwd | head -c 1 && echo " = archivo regular"

echo ""
echo "=== Directorio ==="
ls -ld /etc | head -c 1 && echo " = directorio"

echo ""
echo "=== Symlink ==="
ls -la /usr/bin/awk | head -c 1 && echo " = symlink"

echo ""
echo "=== Dispositivo de caracter ==="
ls -la /dev/null | head -c 1 && echo " = dispositivo de caracter"
'
```

### Paso 1.4: Permisos de archivos del sistema

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

---

## Parte 2 — Notacion octal

### Objetivo

Convertir entre notacion simbolica y octal, y reconocer los
permisos mas comunes.

### Paso 2.1: Tabla de conversion

```bash
docker compose exec debian-dev bash -c '
echo "=== Valores ==="
echo "r = 4"
echo "w = 2"
echo "x = 1"
echo ""
echo "=== Combinaciones ==="
echo "rwx = 4+2+1 = 7"
echo "rw- = 4+2+0 = 6"
echo "r-x = 4+0+1 = 5"
echo "r-- = 4+0+0 = 4"
echo "--- = 0+0+0 = 0"
'
```

### Paso 2.2: Practicar conversiones

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
echo "=== Limpiar ==="
rm -f /tmp/perm-*
'
```

Antes de verificar cada uno, predice mentalmente el resultado
simbolico.

### Paso 2.3: Permisos comunes y su uso

```bash
docker compose exec debian-dev bash -c '
echo "=== Permisos comunes ==="
echo "755 (rwxr-xr-x) = ejecutables, directorios publicos"
echo "644 (rw-r--r--) = archivos de texto, configs legibles"
echo "700 (rwx------) = home de root, directorios privados"
echo "600 (rw-------) = claves SSH, archivos sensibles"
echo "750 (rwxr-x---) = directorios de grupo"
echo "640 (rw-r-----) = configs con lectura de grupo"

echo ""
echo "=== Verificar en el sistema ==="
echo "Home de dev:"
stat -c "%a %A" /home/dev
echo "Claves SSH (si existen):"
stat -c "%a %A %n" /home/dev/.ssh/* 2>/dev/null || echo "(no hay claves SSH)"
'
```

---

## Parte 3 — Permisos en directorios

### Objetivo

Entender la diferencia critica entre permisos en archivos y
directorios, especialmente el significado de x en directorios.

### Paso 3.1: r vs x en directorios

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear directorio de prueba ==="
mkdir -p /tmp/perm-dir
echo "contenido" > /tmp/perm-dir/file.txt

echo ""
echo "=== Permisos 755 (normal) ==="
chmod 755 /tmp/perm-dir
ls /tmp/perm-dir/
cat /tmp/perm-dir/file.txt
echo "(todo funciona)"
'
```

### Paso 3.2: Directorio con r sin x

```bash
docker compose exec debian-dev bash -c '
echo "=== Quitar x del directorio ==="
chmod 644 /tmp/perm-dir

echo ""
echo "=== Intentar listar (r = si, pero con errores) ==="
ls /tmp/perm-dir/ 2>&1 || true

echo ""
echo "=== Intentar leer archivo dentro (sin x = no) ==="
cat /tmp/perm-dir/file.txt 2>&1 || true
echo "(sin x no puedes acceder a los archivos dentro)"

chmod 755 /tmp/perm-dir
'
```

`r` en un directorio permite listar nombres. `x` permite acceder
al contenido. Sin `x`, puedes ver los nombres pero no acceder a
los archivos.

### Paso 3.3: Directorio con x sin r

```bash
docker compose exec debian-dev bash -c '
echo "=== Solo x, sin r ==="
chmod 711 /tmp/perm-dir

echo ""
echo "=== Intentar listar (sin r) ==="
ls /tmp/perm-dir/ 2>&1 || true
echo "(no puedes listar)"

echo ""
echo "=== Pero SI puedes acceder si conoces el nombre ==="
cat /tmp/perm-dir/file.txt 2>&1 || true
echo "(funciona porque tienes x)"

chmod 755 /tmp/perm-dir
'
```

Con `x` sin `r`: no puedes listar pero puedes acceder si conoces
el nombre exacto del archivo.

### Paso 3.4: w en directorio permite eliminar

```bash
docker compose exec debian-dev bash -c '
echo "=== w en directorio: puede eliminar archivos de otros ==="
echo "Esto es lo que resuelve el sticky bit (cubierto en T03)"

echo ""
echo "Ejemplo:"
echo "chmod 777 /tmp/shared/"
echo "alice crea secret.txt con chmod 600"
echo "bob NO puede leer secret.txt (permisos del archivo)"
echo "bob SI puede ELIMINARLO (permisos del directorio)"

echo ""
echo "=== Verificar que /tmp tiene sticky bit ==="
stat -c "%a %A" /tmp
echo "(la t al final = sticky bit, protege contra esto)"
'
```

### Paso 3.5: El kernel no acumula permisos

```bash
docker compose exec debian-dev bash -c '
echo "=== Owner puede tener MENOS permisos que group ==="
touch /tmp/perm-test
chmod 074 /tmp/perm-test
stat -c "%a %A" /tmp/perm-test

echo ""
echo "Owner (0 = ---): NO puede leer"
echo "Group (7 = rwx): puede todo"
echo "Others (4 = r--): puede leer"

echo ""
echo "=== Intentar leer como owner ==="
cat /tmp/perm-test 2>&1 || true

chmod 755 /tmp/perm-dir
rm -f /tmp/perm-test
rm -rf /tmp/perm-dir
'
```

El kernel usa la **primera** categoria que coincide. Si eres el
owner, solo se aplican los permisos de owner, incluso si group
tiene mas permisos.

---

## Limpieza final

```bash
docker compose exec debian-dev bash -c '
rm -rf /tmp/perm-dir /tmp/perm-test /tmp/perm-* 2>/dev/null
echo "Limpieza completada"
'
```

---

## Conceptos reforzados

1. Cada archivo tiene tres conjuntos de permisos: **owner** (u),
   **group** (g), **others** (o). Cada uno con r, w, x.

2. En directorios: `r` = listar nombres, `w` = crear/eliminar,
   `x` = acceder. Sin `x`, no se puede acceder a archivos dentro.

3. Notacion octal: r=4, w=2, x=1. Se suman por categoria:
   `rwxr-xr--` = 754.

4. El kernel evalua en orden: owner → group → others. Usa la
   **primera coincidencia** y se detiene. No acumula permisos.

5. `stat -c '%a'` muestra el octal. `stat -c '%A'` muestra el
   simbolico. Usar `stat` para verificar permisos.
