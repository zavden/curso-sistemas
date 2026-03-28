# Lab — Directorios raiz

## Objetivo

Explorar la jerarquia del Filesystem Hierarchy Standard (FHS) en Linux:
identificar el proposito de cada directorio raiz, localizar software
instalado, verificar /tmp, y comparar la estructura entre Debian y
AlmaLinux.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)
- Los comandos se ejecutan dentro de los contenedores

---

## Parte 1 — Estructura raiz

### Objetivo

Explorar los directorios de primer nivel y entender el proposito de
cada uno.

### Paso 1.1: Listar la raiz

```bash
docker compose exec debian-dev ls -la /
```

Identifica cada directorio antes de continuar. Algunos son symlinks
(parte del /usr merge, cubierto en T02).

### Paso 1.2: Identificar symlinks

```bash
docker compose exec debian-dev bash -c '
for d in /bin /sbin /lib /lib64; do
    if [ -L "$d" ]; then
        echo "$d -> $(readlink $d) (symlink)"
    else
        echo "$d (directorio real)"
    fi
done
'
```

En Debian bookworm (con /usr merge), `/bin`, `/sbin` y `/lib` son
symlinks a sus equivalentes en `/usr/`.

### Paso 1.3: Tamano de cada directorio

```bash
docker compose exec debian-dev bash -c 'du -sh /* 2>/dev/null | sort -rh | head -15'
```

Predice: cual sera el directorio mas grande?

`/usr` suele ser el mayor porque contiene todo el software instalado.
`/home` sera grande si tiene datos del usuario.

### Paso 1.4: Arbol de primer nivel

```bash
docker compose exec debian-dev tree -L 1 /
```

### Paso 1.5: Estructura de /usr

```bash
docker compose exec debian-dev tree -L 1 /usr
```

`/usr` contiene: `bin/` (binarios), `lib/` (librerias), `include/`
(headers), `share/` (datos independientes de arquitectura), `local/`
(software instalado manualmente).

---

## Parte 2 — Localizar software

### Objetivo

Aprender a localizar binarios, headers y man pages en el sistema.

### Paso 2.1: Donde estan los binarios

```bash
docker compose exec debian-dev bash -c '
echo "=== gcc ==="
which gcc
ls -la $(which gcc)

echo ""
echo "=== make ==="
which make
ls -la $(which make)

echo ""
echo "=== bash ==="
which bash
ls -la $(which bash)
'
```

Todos estan en `/usr/bin/` (o en `/bin/` que es symlink a `/usr/bin/`).

### Paso 2.2: Headers de C

```bash
docker compose exec debian-dev bash -c '
echo "=== stdio.h ==="
ls /usr/include/stdio.h

echo ""
echo "=== Headers del sistema ==="
ls /usr/include/ | head -10
echo "..."
ls /usr/include/ | wc -l
echo "archivos en /usr/include/"
'
```

Los headers estan en `/usr/include/`. Son necesarios para compilar
programas C que usan las librerias del sistema.

### Paso 2.3: Man pages

```bash
docker compose exec debian-dev bash -c '
echo "=== Donde estan las man pages ==="
man -w gcc
man -w printf

echo ""
echo "=== Estructura ==="
ls /usr/share/man/
'
```

Las man pages estan en `/usr/share/man/`, organizadas por seccion
(man1 = comandos, man2 = syscalls, man3 = funciones C).

### Paso 2.4: /usr/local/ — software manual

```bash
docker compose exec debian-dev bash -c '
echo "=== /usr/local/ ==="
ls /usr/local/
echo ""
echo "=== /usr/local/bin/ ==="
ls /usr/local/bin/ 2>/dev/null || echo "(vacio)"
'
```

`/usr/local/` es para software instalado manualmente por el
administrador. El gestor de paquetes nunca toca este directorio.

---

## Parte 3 — /tmp y permisos especiales

### Objetivo

Examinar /tmp: sticky bit, tipo de filesystem, y comportamiento.

### Paso 3.1: Permisos de /tmp

```bash
docker compose exec debian-dev bash -c '
ls -ld /tmp
stat -c "%a %A" /tmp
'
```

La `t` al final de los permisos indica el **sticky bit**: cualquier
usuario puede crear archivos en /tmp, pero solo el dueno puede borrar
los suyos.

### Paso 3.2: Tipo de filesystem

```bash
docker compose exec debian-dev bash -c '
df -h /tmp
mount | grep /tmp || echo "/tmp no es un mount separado"
'
```

En contenedores, /tmp suele estar en el filesystem del contenedor
(overlay2). En el host, puede ser un tmpfs (montado en RAM).

### Paso 3.3: Crear y verificar archivos temporales

```bash
docker compose exec debian-dev bash -c '
echo "test from dev" > /tmp/mytest.txt
ls -la /tmp/mytest.txt
cat /tmp/mytest.txt
rm /tmp/mytest.txt
'
```

### Paso 3.4: /tmp vs /var/tmp

```bash
docker compose exec debian-dev bash -c '
echo "=== /tmp ==="
ls -ld /tmp
echo ""
echo "=== /var/tmp ==="
ls -ld /var/tmp
'
```

Ambos tienen sticky bit. La diferencia: `/tmp` se limpia al reiniciar
(o cada 10 dias por systemd-tmpfiles), `/var/tmp` se limpia despues de
30 dias. Usar `/var/tmp` para datos temporales que deben sobrevivir
un reinicio.

---

## Parte 4 — Comparar distros

### Objetivo

Verificar que ambas distribuciones siguen el FHS con diferencias
menores.

### Paso 4.1: Estructura raiz en AlmaLinux

```bash
docker compose exec alma-dev ls -la /
```

Compara con la salida de Debian del paso 1.1. La estructura es
practicamente identica.

### Paso 4.2: Symlinks del merge

```bash
docker compose exec alma-dev bash -c '
for d in /bin /sbin /lib /lib64; do
    if [ -L "$d" ]; then
        echo "$d -> $(readlink $d) (symlink)"
    else
        echo "$d (directorio real)"
    fi
done
'
```

AlmaLinux tambien tiene /usr merge activo.

### Paso 4.3: Directorios de paquetes

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c 'ls /etc/apt/ 2>/dev/null && echo "apt: presente"'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c 'ls /etc/yum.repos.d/ 2>/dev/null && echo "yum.repos.d: presente"'
```

Debian usa `/etc/apt/` para repositorios. AlmaLinux usa
`/etc/yum.repos.d/`.

### Paso 4.4: Multiarch vs lib64

```bash
echo "=== Debian (multiarch) ==="
docker compose exec debian-dev ls /usr/lib/x86_64-linux-gnu/ | head -5

echo ""
echo "=== AlmaLinux (lib64) ==="
docker compose exec alma-dev ls /usr/lib64/ | head -5
```

Debian usa directorios multiarch (`x86_64-linux-gnu/`) que permiten
instalar librerias de multiples arquitecturas. AlmaLinux usa `/lib64/`
directamente.

---

## Limpieza final

No hay recursos que limpiar. Los contenedores del curso siguen
funcionando normalmente.

---

## Conceptos reforzados

1. La estructura del FHS es consistente entre distribuciones: `/usr`
   para software, `/etc` para configuracion, `/var` para datos
   variables, `/tmp` para temporales.

2. En distribuciones modernas, `/bin`, `/sbin` y `/lib` son
   **symlinks** a `/usr/bin`, `/usr/sbin` y `/usr/lib` (usr merge).

3. El software del sistema esta en `/usr/bin/`, el software manual del
   administrador en `/usr/local/bin/`. El gestor de paquetes nunca
   toca `/usr/local/`.

4. `/tmp` tiene el **sticky bit** (`t`): cualquier usuario puede
   crear archivos, pero solo el dueno puede borrar los suyos.

5. Debian usa **multiarch** (`/usr/lib/x86_64-linux-gnu/`) mientras
   AlmaLinux usa `/usr/lib64/` directamente.
