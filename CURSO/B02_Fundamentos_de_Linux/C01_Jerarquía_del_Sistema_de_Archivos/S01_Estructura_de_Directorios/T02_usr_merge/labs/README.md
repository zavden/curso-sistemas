# Lab — /usr merge

## Objetivo

Verificar el estado del /usr merge en Debian y AlmaLinux, demostrar
que los paths tradicionales funcionan via symlinks, y confirmar que
/usr/local permanece separado del merge.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Verificar el merge

### Objetivo

Comprobar que /bin, /sbin y /lib son symlinks a /usr/ en ambas
distribuciones.

### Paso 1.1: Symlinks en Debian

```bash
docker compose exec debian-dev bash -c '
echo "=== Symlinks del merge ==="
for d in /bin /sbin /lib /lib64; do
    if [ -L "$d" ]; then
        echo "$d -> $(readlink $d)"
    else
        echo "$d (directorio real - NO mergeado)"
    fi
done
'
```

Todos deben mostrar symlinks. `/bin -> usr/bin`, `/sbin -> usr/sbin`,
etc.

### Paso 1.2: Symlinks en AlmaLinux

```bash
docker compose exec alma-dev bash -c '
for d in /bin /sbin /lib /lib64; do
    if [ -L "$d" ]; then
        echo "$d -> $(readlink $d)"
    else
        echo "$d (directorio real - NO mergeado)"
    fi
done
'
```

Ambas distribuciones tienen el merge activo.

### Paso 1.3: Mismo archivo, diferentes paths

```bash
docker compose exec debian-dev bash -c '
echo "=== inodo de /bin/bash ==="
ls -i /bin/bash
echo ""
echo "=== inodo de /usr/bin/bash ==="
ls -i /usr/bin/bash
'
```

Antes de ejecutar, predice: tendran el mismo numero de inodo?

Si. Ambos paths apuntan al **mismo archivo** en disco. El inodo es
identico porque `/bin` es un symlink a `/usr/bin`.

### Paso 1.4: Verificar con stat

```bash
docker compose exec debian-dev bash -c '
echo "=== /bin/ls ==="
stat /bin/ls | grep -E "Inode|File"
echo ""
echo "=== /usr/bin/ls ==="
stat /usr/bin/ls | grep -E "Inode|File"
'
```

El campo "File" mostrara la misma ruta resuelta y el mismo inodo.

---

## Parte 2 — Compatibilidad

### Objetivo

Demostrar que los paths tradicionales siguen funcionando y que los
scripts no se rompen con el merge.

### Paso 2.1: Ejecutar con paths tradicionales

```bash
docker compose exec debian-dev bash -c '
echo "=== Paths tradicionales ==="
/bin/echo "funciona desde /bin/echo"
/usr/bin/echo "funciona desde /usr/bin/echo"
/bin/ls --version | head -1
/usr/bin/ls --version | head -1
'
```

Ambos paths funcionan identicamente.

### Paso 2.2: Shebangs

```bash
docker compose exec debian-dev bash -c '
echo "=== Shebang clasico ==="
echo "#!/bin/bash
echo shebang /bin/bash funciona" > /tmp/test1.sh
chmod +x /tmp/test1.sh
/tmp/test1.sh

echo ""
echo "=== Shebang portátil ==="
echo "#!/usr/bin/env bash
echo shebang /usr/bin/env funciona" > /tmp/test2.sh
chmod +x /tmp/test2.sh
/tmp/test2.sh

rm /tmp/test1.sh /tmp/test2.sh
'
```

`#!/bin/bash` funciona gracias al symlink. `#!/usr/bin/env bash` es
la forma mas portable porque busca bash en PATH.

### Paso 2.3: which y command -v

```bash
docker compose exec debian-dev bash -c '
echo "=== which ==="
which gcc
which bash
which ls

echo ""
echo "=== command -v (POSIX) ==="
command -v gcc
command -v bash
command -v ls
'
```

`command -v` es la forma POSIX de localizar binarios. Es preferible
a `which` en scripts portátiles.

---

## Parte 3 — /usr/local separado

### Objetivo

Confirmar que /usr/local no participa del merge y entender por que.

### Paso 3.1: /usr/local es un directorio real

```bash
docker compose exec debian-dev bash -c '
echo "=== /usr/local ==="
file /usr/local
file /usr/local/bin
ls -la /usr/local/
'
```

`/usr/local` es un directorio real (no un symlink). Tiene la misma
estructura que `/usr`: `bin/`, `lib/`, `include/`, `share/`.

### Paso 3.2: Contenido de /usr/local

```bash
docker compose exec debian-dev bash -c '
echo "=== /usr/local/bin ==="
ls /usr/local/bin/ 2>/dev/null || echo "(vacio)"

echo ""
echo "=== Comparar con /usr/bin ==="
echo "Binarios en /usr/bin: $(ls /usr/bin/ | wc -l)"
echo "Binarios en /usr/local/bin: $(ls /usr/local/bin/ 2>/dev/null | wc -l)"
'
```

`/usr/local/bin/` esta vacio en una imagen Docker limpia porque no se
ha instalado software manualmente. El gestor de paquetes instala en
`/usr/bin/`, no en `/usr/local/bin/`.

### Paso 3.3: Prioridad en PATH

```bash
docker compose exec debian-dev bash -c '
echo "$PATH" | tr ":" "\n"
'
```

`/usr/local/bin` aparece **antes** que `/usr/bin` en PATH. Si
instalas un binario en `/usr/local/bin/` con el mismo nombre que uno
del sistema, el local toma precedencia.

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. El /usr merge convierte `/bin`, `/sbin` y `/lib` en **symlinks** a
   `/usr/bin`, `/usr/sbin` y `/usr/lib`. Los paths tradicionales
   siguen funcionando.

2. Ambos paths (`/bin/bash` y `/usr/bin/bash`) apuntan al **mismo
   inodo** — es literalmente el mismo archivo.

3. `#!/usr/bin/env bash` es el shebang mas portable porque busca bash
   en PATH en vez de depender de un path absoluto.

4. `/usr/local` **no participa** del merge. Es territorio del
   administrador, separado del software del gestor de paquetes.

5. `/usr/local/bin` tiene **prioridad** sobre `/usr/bin` en PATH,
   permitiendo sobrescribir comandos del sistema con versiones locales.
