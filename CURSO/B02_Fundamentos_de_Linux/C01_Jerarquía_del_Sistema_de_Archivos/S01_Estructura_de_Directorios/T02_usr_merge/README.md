# T02 — /usr merge

## El problema histórico

Tradicionalmente, Linux separaba los binarios en dos niveles:

```
/bin, /sbin, /lib        ← Binarios esenciales para arranque y single-user
/usr/bin, /usr/sbin, /usr/lib  ← Resto del software instalado
```

La razón original era técnica: en los años 70-80, los discos eran pequeños y
`/usr` podía estar en una partición separada (posiblemente montada por red).
Los binarios en `/bin` y `/sbin` debían estar disponibles antes de montar
`/usr`.

## Por qué ya no tiene sentido

En sistemas modernos:

1. **initramfs** se encarga del arranque inicial — monta `/usr` antes de que el
   sistema necesite cualquier binario del espacio de usuario
2. **systemd** requiere `/usr` montado desde el inicio para funcionar
3. La separación causa más problemas que los que resuelve:
   - ¿`ip` va en `/sbin` o `/usr/sbin`? Depende de la distro
   - Scripts hardcodean paths que varían entre distribuciones
   - Las herramientas modernas (`ip`, `ss`) reemplazaron a las legacy
     (`ifconfig`, `netstat`) pero van en directorios diferentes

```bash
# Ejemplo del problema: ¿dónde está ip?
# En Debian antiguo:  /sbin/ip
# En Fedora:          /usr/sbin/ip
# Script que usa /sbin/ip falla en Fedora y viceversa
```

## Qué es /usr merge

El **usrmerge** elimina la separación moviendo todo a `/usr` y dejando
symlinks en la raíz:

```
/bin  → /usr/bin       (symlink)
/sbin → /usr/sbin      (symlink)
/lib  → /usr/lib       (symlink)
/lib64 → /usr/lib64    (symlink, en sistemas 64-bit)
```

Después del merge, los paths tradicionales siguen funcionando:

```bash
# Ambos paths funcionan — apuntan al mismo archivo
/bin/ls
/usr/bin/ls

ls -la /bin
# lrwxrwxrwx 1 root root 7 ... /bin -> usr/bin
```

## Estado por distribución

| Distribución | /usr merge | Desde cuándo |
|---|---|---|
| Fedora | Sí | Fedora 17 (2012) — pionera |
| Arch Linux | Sí | 2012 |
| Ubuntu | Sí | 23.10 (2023) — tardía |
| Debian | Sí | Debian 12 bookworm (2023) |
| AlmaLinux/RHEL | Sí | RHEL 7 (2014) |
| openSUSE | Sí | 2012 |

A partir de ~2023, todas las distribuciones principales tienen /usr merge.
Es el estándar de facto.

## Verificar el estado en tu sistema

```bash
# Verificar si /bin es un symlink
ls -la /bin

# Si muestra "lrwxrwxrwx ... /bin -> usr/bin" → usrmerge activo
# Si muestra "drwxr-xr-x ... /bin"            → separados (sistema legacy)

# Verificar todos los symlinks del merge
for d in /bin /sbin /lib /lib64; do
    if [ -L "$d" ]; then
        echo "$d -> $(readlink "$d") (merged)"
    else
        echo "$d (separado)"
    fi
done
```

## Implicaciones prácticas

### Para administradores

- Los paths `/bin/bash`, `/sbin/fdisk`, etc. siguen funcionando (los
  symlinks garantizan compatibilidad)
- No hay impacto operativo visible — es transparente
- Los scripts que usan paths absolutos como `/bin/sh` no se rompen

### Para desarrolladores

- Usar `PATH` en vez de paths absolutos para localizar binarios:
  ```bash
  # BIEN: usa PATH
  which gcc
  gcc --version

  # EVITAR: path absoluto hardcodeado
  /usr/bin/gcc --version
  ```

- Si necesitas el path absoluto, usar `which` o `command -v`:
  ```bash
  GCC=$(command -v gcc)
  echo "gcc está en: $GCC"
  ```

### Para shebangs

Los shebangs en scripts deben usar paths estándar:

```bash
#!/bin/bash          ← Funciona en todas las distros (symlink o real)
#!/usr/bin/env bash  ← Más portable (busca bash en PATH)
```

`#!/usr/bin/env` es la opción más robusta porque no depende de ningún path
específico.

### Para el curso

Tanto la imagen Debian como la imagen AlmaLinux del laboratorio tienen /usr
merge activo:

```bash
# Dentro de debian-dev
ls -la /bin
# lrwxrwxrwx 1 root root 7 ... /bin -> usr/bin

# Dentro de alma-dev
ls -la /bin
# lrwxrwxrwx 1 root root 7 ... /bin -> usr/bin
```

## La excepción: `/usr/local`

`/usr/local` **no** participa del merge. Sigue siendo un directorio separado
para software instalado manualmente por el administrador:

```
/usr/local/bin     ← Binarios locales (no mergeados)
/usr/local/lib     ← Bibliotecas locales
/usr/local/include ← Headers locales
```

Esto es intencional: `/usr/local` es territorio del administrador, mientras
que `/usr/bin` y `/usr/lib` son del gestor de paquetes.

## Historia del debate

El /usr merge fue propuesto por Lennart Poettering (autor de systemd) en 2011
y generó debate en la comunidad:

**A favor**:
- Simplifica la estructura del sistema
- Elimina ambigüedad de paths entre distribuciones
- Facilita snapshots de solo lectura del sistema raíz
- systemd lo requiere para funcionar correctamente

**En contra**:
- Rompe la tradición Unix de separar `/` y `/usr`
- Potencial rotura de scripts legacy con paths hardcodeados
- Complejidad durante la migración

El debate se resolvió por la vía práctica: todas las distribuciones principales
migraron. En 2025, es estándar universal.

---

## Ejercicios

### Ejercicio 1 — Verificar el merge

```bash
# En ambos contenedores del lab
docker compose exec debian-dev bash -c '
    for d in /bin /sbin /lib; do
        ls -la "$d" | head -1
    done'

docker compose exec alma-dev bash -c '
    for d in /bin /sbin /lib; do
        ls -la "$d" | head -1
    done'
```

### Ejercicio 2 — Paths equivalentes

```bash
# Verificar que ambos paths apuntan al mismo binario
docker compose exec debian-dev bash -c '
    ls -li /bin/bash /usr/bin/bash
    # El número de inodo es el mismo — es el mismo archivo'
```

### Ejercicio 3 — /usr/local sigue separado

```bash
docker compose exec debian-dev bash -c '
    ls -la /usr/local/
    # No es un symlink — es un directorio real

    file /usr/local/bin
    # /usr/local/bin: directory'
```
