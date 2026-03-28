# T02 — /usr merge

## Objetivo

Entender qué es el `/usr merge`, por qué se implementó, cómo verificarlo, y
sus implicaciones prácticas para administradores y desarrolladores.

---

## El problema histórico

Tradicionalmente, Linux separaba los binarios en dos niveles:

```
/bin, /sbin, /lib        ← Binarios esenciales para arranque y single-user
/usr/bin, /usr/sbin, /usr/lib  ← Resto del software instalado
```

La razón original era técnica: en los primeros sistemas Unix (Bell Labs, ~1971),
los discos eran pequeños y `/usr` podía estar en una partición separada
(posiblemente montada por red). Los binarios en `/bin` y `/sbin` debían estar
disponibles antes de montar `/usr`.

## Por qué ya no tiene sentido

En sistemas modernos:

1. **initramfs** monta `/usr` antes de que el sistema necesite cualquier binario
   del espacio de usuario
2. **systemd** requiere `/usr` montado desde el inicio para funcionar
3. La separación causaba más problemas que los que resolvía:
   - ¿`ip` va en `/sbin` o `/usr/sbin`? Dependía de la distro
   - Scripts hardcodeaban paths que variaban entre distribuciones
   - Las herramientas modernas (`ip`, `ss`) reemplazaron a las legacy
     (`ifconfig`, `netstat`) pero iban en directorios diferentes

```bash
# Ejemplo del problema: ¿dónde está ip?
# En Debian antiguo:  /sbin/ip
# En Fedora:          /usr/sbin/ip
# Un script con /sbin/ip falla en Fedora y viceversa
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
| openSUSE | Sí | 2012 |
| AlmaLinux/RHEL | Sí | RHEL 7 (2014) |
| Ubuntu | Sí | 23.10 (2023) |
| Debian | Sí | Debian 12 bookworm (2023) |

A partir de ~2023, todas las distribuciones principales tienen /usr merge. Es
el estándar de facto.

## Implicaciones prácticas

### Para administradores

- Los paths `/bin/bash`, `/sbin/fdisk`, etc. siguen funcionando (los symlinks
  garantizan compatibilidad)
- No hay impacto operativo visible — es transparente
- Los scripts que usan paths absolutos como `/bin/sh` no se rompen

### Para desarrolladores

Usar `PATH` en vez de paths absolutos para localizar binarios:

```bash
# BIEN: usa PATH
which gcc
gcc --version

# EVITAR: path absoluto hardcodeado
/usr/bin/gcc --version
```

Si necesitas el path absoluto, usar `which` o `command -v`:

```bash
GCC=$(command -v gcc)
echo "gcc está en: $GCC"
```

### Shebangs en scripts

```bash
#!/bin/bash          ← Funciona en todas las distros (symlink o real)
#!/usr/bin/env bash  ← Más portable (busca bash en PATH)
```

`#!/usr/bin/env` es la opción más robusta porque no depende de ningún path
específico. Es la forma recomendada para scripts portátiles.

## La excepción: `/usr/local`

`/usr/local` **no** participa del merge. Sigue siendo un directorio separado
para software instalado manualmente por el administrador:

```
/usr/local/bin     ← Binarios locales (no mergeados)
/usr/local/lib     ← Bibliotecas locales
/usr/local/include ← Headers locales
```

Esto es intencional: `/usr/local` es territorio del administrador, mientras
que `/usr/bin` y `/usr/lib` son del gestor de paquetes. La separación previene
conflictos.

### Prioridad en PATH

`/usr/local/bin` aparece **antes** que `/usr/bin` en `$PATH` por defecto. Si
instalas un binario en `/usr/local/bin/` con el mismo nombre que uno del
sistema, el local toma precedencia:

```bash
echo "$PATH" | tr ':' '\n'
# /usr/local/sbin
# /usr/local/bin      ← primero
# /usr/sbin
# /usr/bin             ← después
```

## Historia del debate

El /usr merge fue propuesto por Lennart Poettering (autor de systemd) en un
blog post de enero 2012. Generó debate en la comunidad:

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

Todos los comandos se ejecutan dentro de los contenedores del curso.

### Ejercicio 1 — Verificar symlinks del merge en ambas distros

Comprueba que `/bin`, `/sbin`, `/lib` son symlinks en Debian y AlmaLinux.

```bash
echo "=== Debian ==="
docker compose exec -T debian-dev bash -c '
for d in /bin /sbin /lib /lib64; do
    if [ -L "$d" ]; then
        echo "  $d -> $(readlink "$d") (merged)"
    else
        [ -d "$d" ] && echo "  $d (directorio real)" || echo "  $d (no existe)"
    fi
done
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec -T alma-dev bash -c '
for d in /bin /sbin /lib /lib64; do
    if [ -L "$d" ]; then
        echo "  $d -> $(readlink "$d") (merged)"
    else
        [ -d "$d" ] && echo "  $d (directorio real)" || echo "  $d (no existe)"
    fi
done
'
```

<details>
<summary>Predicción</summary>

Ambas distribuciones muestran los mismos symlinks:
- `/bin` → `usr/bin` (merged)
- `/sbin` → `usr/sbin` (merged)
- `/lib` → `usr/lib` (merged)
- `/lib64` → `usr/lib64` (merged)

El target de `readlink` es relativo (`usr/bin`, no `/usr/bin`) porque así se
almacena el symlink. Funciona porque cuando el kernel resuelve `/bin`, ve que
es un symlink a `usr/bin` y lo interpreta como `/usr/bin` (relativo al
directorio que contiene el symlink, que es `/`).

AlmaLinux/RHEL adoptó el merge desde RHEL 7 (2014). Debian lo completó mucho
más tarde, en Debian 12 bookworm (2023).

</details>

---

### Ejercicio 2 — Demostrar que ambos paths son el mismo archivo

Compara los números de inodo para confirmar que `/bin/bash` y `/usr/bin/bash`
son literalmente el mismo archivo.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Inodos ==="
echo "/bin/bash:     $(ls -i /bin/bash | awk "{print \$1}")"
echo "/usr/bin/bash: $(ls -i /usr/bin/bash | awk "{print \$1}")"

echo ""
echo "=== Verificación con stat ==="
echo "--- /bin/bash ---"
stat /bin/bash | grep -E "Inode|File"
echo "--- /usr/bin/bash ---"
stat /usr/bin/bash | grep -E "Inode|File"

echo ""
echo "=== Son el mismo archivo? ==="
if [ /bin/bash -ef /usr/bin/bash ]; then
    echo "SÍ: mismo dispositivo e inodo"
else
    echo "NO: archivos diferentes"
fi
'
```

<details>
<summary>Predicción</summary>

Ambos paths muestran el **mismo número de inodo**. No son dos copias del
archivo — son dos rutas que llegan al mismo archivo en disco.

El operador `-ef` en bash ("equal file") compara si dos paths apuntan al mismo
dispositivo e inodo. Es la forma más robusta de verificar que son el mismo
archivo.

El mecanismo: `/bin` es un symlink a `usr/bin`. Cuando accedes a `/bin/bash`,
el kernel:
1. Lee `/bin` → ve que es symlink a `usr/bin`
2. Resuelve a `/usr/bin`
3. Busca `bash` en `/usr/bin/`
4. Devuelve el mismo inodo que `/usr/bin/bash`

No hay doble resolución ni copia. Es una indirección a nivel de filesystem.

</details>

---

### Ejercicio 3 — Probar shebangs con ambos estilos

Crea scripts con diferentes shebangs y verifica que ambos funcionan.

```bash
docker compose exec -T debian-dev bash -c '
# Shebang clásico
cat > /tmp/test_classic.sh << "SCRIPT"
#!/bin/bash
echo "Shebang clásico (/bin/bash) funciona"
echo "PID: $$, Shell: $BASH"
SCRIPT
chmod +x /tmp/test_classic.sh

# Shebang portable
cat > /tmp/test_env.sh << "SCRIPT"
#!/usr/bin/env bash
echo "Shebang portable (/usr/bin/env) funciona"
echo "PID: $$, Shell: $BASH"
SCRIPT
chmod +x /tmp/test_env.sh

echo "=== Shebang clásico ==="
/tmp/test_classic.sh

echo ""
echo "=== Shebang portable ==="
/tmp/test_env.sh

# Limpiar
rm /tmp/test_classic.sh /tmp/test_env.sh
'
```

<details>
<summary>Predicción</summary>

Ambos scripts se ejecutan correctamente y muestran su mensaje. La variable
`$BASH` muestra la ruta resuelta del intérprete (probablemente `/usr/bin/bash`
en ambos casos, ya que `/bin/bash` se resuelve al mismo binario).

La diferencia entre ambos shebangs:
- `#!/bin/bash` — el kernel busca directamente `/bin/bash`. Funciona gracias
  al symlink `/bin` → `/usr/bin`
- `#!/usr/bin/env bash` — el kernel ejecuta `/usr/bin/env`, que busca `bash`
  en `$PATH`. Es más portable porque no depende de la ubicación de bash

En la práctica, `#!/bin/bash` funciona en todas las distribuciones modernas
(con o sin merge). `#!/usr/bin/env bash` es preferible en scripts que podrían
ejecutarse en sistemas donde bash está en una ubicación no estándar (como
algunos BSD).

</details>

---

### Ejercicio 4 — Comparar `which` vs `command -v`

Conoce las dos formas de localizar binarios y cuándo usar cada una.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== which ==="
which gcc
which bash
which ls
which rustc

echo ""
echo "=== command -v (POSIX) ==="
command -v gcc
command -v bash
command -v ls
command -v rustc

echo ""
echo "=== Diferencia clave: builtins ==="
echo "which cd:     $(which cd 2>&1)"
echo "command -v cd: $(command -v cd)"
echo ""
echo "which echo:     $(which echo)"
echo "command -v echo: $(command -v echo)"
'
```

<details>
<summary>Predicción</summary>

Para binarios externos (gcc, bash, ls, rustc), ambos devuelven el mismo path
(`/usr/bin/gcc`, etc.).

La diferencia aparece con **shell builtins**:
- `which cd` → falla o muestra nada (cd no es un binario en disco)
- `command -v cd` → muestra `cd` (reconoce que es un builtin de bash)
- `which echo` → `/usr/bin/echo` (encuentra el binario externo)
- `command -v echo` → `echo` (muestra el builtin, que tiene prioridad)

`command -v` es la forma **POSIX** (portable entre shells). `which` es un
comando externo que solo busca binarios en `$PATH`. Para scripts portátiles,
`command -v` es preferible. Para uso interactivo, ambos sirven.

Un patrón común en scripts:
```bash
if command -v gcc >/dev/null 2>&1; then
    echo "gcc disponible"
fi
```

</details>

---

### Ejercicio 5 — Verificar que `/usr/local` no participa del merge

Confirma que `/usr/local` es un directorio real y entiende su propósito.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Tipo de /usr/local ==="
file /usr/local
file /usr/local/bin

echo ""
echo "=== ¿Es symlink? ==="
if [ -L /usr/local ]; then
    echo "/usr/local es symlink -> $(readlink /usr/local)"
else
    echo "/usr/local es directorio real (NO participa del merge)"
fi

echo ""
echo "=== Estructura ==="
ls -la /usr/local/

echo ""
echo "=== Contenido de /usr/local/bin ==="
ls /usr/local/bin/ 2>/dev/null | head -10
[ "$(ls /usr/local/bin/ 2>/dev/null | wc -l)" -eq 0 ] && echo "(vacío)"
'
```

<details>
<summary>Predicción</summary>

`file /usr/local` muestra `directory` (no symlink). `/usr/local` es un
directorio real con la misma estructura que `/usr/`: `bin/`, `lib/`,
`include/`, `share/`, etc.

`/usr/local/bin/` probablemente está vacío en un contenedor donde todo el
software se instaló con el gestor de paquetes. Si Rust se instaló con `rustup`
a nivel de sistema, podría haber binarios aquí.

La razón de no incluir `/usr/local` en el merge: el merge unificó directorios
que contenían software gestionado por el **mismo sistema** (la distribución).
`/usr/local` es intencionalmente separado — es territorio del administrador.
El gestor de paquetes nunca lo toca, evitando conflictos entre software del
sistema y software manual.

</details>

---

### Ejercicio 6 — Analizar la prioridad en PATH

Observa el orden de `$PATH` y entiende qué versión de un comando se ejecuta
cuando hay múltiples.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== PATH completo ==="
echo "$PATH" | tr ":" "\n" | cat -n

echo ""
echo "=== Precedencia para un binario ==="
echo "En PATH, /usr/local/bin aparece antes que /usr/bin"
echo ""
echo "Demostración con echo:"
echo "  which echo:     $(which echo)"
echo "  type echo:      $(type echo)"
echo "  type -a echo:"
type -a echo
'
```

<details>
<summary>Predicción</summary>

`$PATH` muestra los directorios de búsqueda numerados. El orden típico en
Debian es:
1. `/usr/local/sbin`
2. `/usr/local/bin`
3. `/usr/sbin`
4. `/usr/bin`

`/usr/local/bin` aparece **antes** que `/usr/bin`. Si ambos contienen un
binario con el mismo nombre, gana `/usr/local/bin`.

`type -a echo` muestra **todas** las versiones de `echo`:
1. `echo is a shell builtin` — el builtin de bash (tiene prioridad máxima)
2. `echo is /usr/bin/echo` — el binario externo

Los builtins de bash siempre tienen prioridad sobre binarios en `$PATH`. Para
forzar el binario externo: `/usr/bin/echo` o `command echo`.

El orden de resolución de un comando en bash es:
1. Aliases
2. Funciones
3. Builtins
4. Binarios en `$PATH` (primer match)

</details>

---

### Ejercicio 7 — Simular la instalación en `/usr/local`

Crea un "binario" en `/usr/local/bin` y verifica que toma precedencia sobre el
de `/usr/bin`.

```bash
docker compose exec -T debian-dev bash -c '
# Crear un script personalizado que "sobrescribe" un comando
echo "#!/bin/bash
echo \"Esta es MI versión personalizada de hello\"
echo \"Ejecutando desde: \$0\"" > /usr/local/bin/hello
chmod +x /usr/local/bin/hello

echo "=== Localizar hello ==="
which hello
command -v hello

echo ""
echo "=== Ejecutar ==="
hello

echo ""
echo "=== type -a muestra todas las versiones ==="
type -a hello

# Limpiar
rm /usr/local/bin/hello
echo ""
echo "=== Después de eliminar ==="
which hello 2>&1 || echo "hello ya no existe en PATH"
'
```

<details>
<summary>Predicción</summary>

`which hello` muestra `/usr/local/bin/hello` porque es el primer directorio en
`$PATH` que contiene ese nombre. `hello` se ejecuta y muestra el mensaje
personalizado.

`type -a hello` muestra solo `/usr/local/bin/hello` (no hay otra versión).

Después de `rm`, `which hello` falla porque ya no existe en ningún directorio
de `$PATH`.

Este patrón es exactamente cómo un administrador puede sobrescribir un comando
del sistema sin tocar el paquete. Por ejemplo, compilar una versión más reciente
de `gcc` e instalarla en `/usr/local/bin/gcc` — el sistema usará esa versión
por defecto sin que `apt` sepa nada al respecto.

</details>

---

### Ejercicio 8 — Verificar el merge con paths de bibliotecas

Comprueba que el merge también aplica a `/lib` y no solo a `/bin`.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== /lib es symlink ==="
ls -la /lib

echo ""
echo "=== Buscar libc ==="
echo "Desde /lib:"
ls /lib/x86_64-linux-gnu/libc.so* 2>/dev/null || echo "  (buscar en otro path)"
echo ""
echo "Desde /usr/lib:"
ls /usr/lib/x86_64-linux-gnu/libc.so* 2>/dev/null || echo "  (no encontrado)"

echo ""
echo "=== Mismo archivo? ==="
if [ /lib/x86_64-linux-gnu/libc.so.6 -ef /usr/lib/x86_64-linux-gnu/libc.so.6 ] 2>/dev/null; then
    echo "SÍ: /lib/.../libc.so.6 y /usr/lib/.../libc.so.6 son el mismo archivo"
else
    echo "Verificar paths disponibles"
fi
'
```

<details>
<summary>Predicción</summary>

`/lib` es un symlink a `usr/lib`, igual que `/bin` lo es a `usr/bin`. El merge
aplica a todos los pares:
- `/bin` → `/usr/bin`
- `/sbin` → `/usr/sbin`
- `/lib` → `/usr/lib`
- `/lib64` → `/usr/lib64`

`libc.so.6` (la biblioteca C estándar) se encuentra en
`/usr/lib/x86_64-linux-gnu/` (ruta multiarch de Debian). Acceder via
`/lib/x86_64-linux-gnu/libc.so.6` da el mismo resultado gracias al symlink.

El operador `-ef` confirma que ambas rutas apuntan al mismo inodo. Esto es
crucial para el linker dinámico (`ld-linux`): no importa si un programa
referencia `/lib/...` o `/usr/lib/...` — es el mismo archivo.

</details>

---

### Ejercicio 9 — Comparar el merge entre distros

Verifica que ambas distros del lab tienen el merge y observa una diferencia
sutil.

```bash
echo "=== Debian: resolución de /bin/gcc ==="
docker compose exec -T debian-dev bash -c '
readlink -f /bin/gcc 2>/dev/null || echo "gcc no está en /bin/"
readlink -f /usr/bin/gcc
echo "Tamaño: $(stat -c %s /usr/bin/gcc) bytes"
'

echo ""
echo "=== AlmaLinux: resolución de /bin/gcc ==="
docker compose exec -T alma-dev bash -c '
readlink -f /bin/gcc 2>/dev/null || echo "gcc no está en /bin/"
readlink -f /usr/bin/gcc
echo "Tamaño: $(stat -c %s /usr/bin/gcc) bytes"
'

echo ""
echo "=== Diferencia clave: ld-linux ==="
echo "Debian:"
docker compose exec -T debian-dev bash -c 'file /usr/bin/gcc | grep -o "interpreter [^,]*"'
echo "AlmaLinux:"
docker compose exec -T alma-dev bash -c 'file /usr/bin/gcc | grep -o "interpreter [^,]*"'
```

<details>
<summary>Predicción</summary>

`readlink -f` resuelve la cadena completa de symlinks hasta el archivo final.
`/bin/gcc` resuelve a `/usr/bin/gcc` (pasando por el symlink `/bin` →
`/usr/bin`). En ambas distros, `gcc` está en `/usr/bin/`.

La diferencia sutil aparece en el **intérprete dinámico** (linker):
- Debian: `interpreter /lib64/ld-linux-x86-64.so.2` — pero `/lib64` es symlink
  a `/usr/lib64`, así que el archivo real está en `/usr/lib64/`
- AlmaLinux: mismo path, misma resolución

El tamaño de `gcc` será diferente entre distros porque son versiones distintas
del compilador (Debian 12 trae gcc-12, AlmaLinux 9 trae gcc-11).

`readlink -f` es diferente de `readlink`: el primero resuelve **toda** la
cadena de symlinks recursivamente, el segundo solo resuelve un nivel.

</details>

---

### Ejercicio 10 — Contar binarios y verificar la unificación

Verifica que no hay binarios "duplicados" — todo está realmente en `/usr`.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Verificación de unificación ==="
echo ""

# Contar binarios
echo "Binarios en /usr/bin:       $(ls /usr/bin/ | wc -l)"
echo "Binarios en /usr/sbin:      $(ls /usr/sbin/ | wc -l)"
echo "Binarios en /usr/local/bin: $(ls /usr/local/bin/ 2>/dev/null | wc -l)"

echo ""
echo "=== /bin y /usr/bin son el mismo directorio ==="
echo "ls /bin/ | wc -l:     $(ls /bin/ | wc -l)"
echo "ls /usr/bin/ | wc -l: $(ls /usr/bin/ | wc -l)"

echo ""
echo "=== Demostración: mismo contenido ==="
diff <(ls /bin/) <(ls /usr/bin/) > /dev/null 2>&1 \
    && echo "Contenido idéntico (son el mismo directorio)" \
    || echo "Diferencias encontradas (inesperado)"

echo ""
echo "=== Resumen del merge ==="
echo "Directorio       Tipo        Destino"
echo "──────────────── ─────────── ──────────────"
for d in /bin /sbin /lib /lib64 /usr/local; do
    if [ -L "$d" ]; then
        printf "%-17s symlink     %s\n" "$d" "$(readlink "$d")"
    else
        printf "%-17s directorio  (real)\n" "$d"
    fi
done
'
```

<details>
<summary>Predicción</summary>

`ls /bin/ | wc -l` y `ls /usr/bin/ | wc -l` muestran el **mismo número**
porque son el mismo directorio (visto a través de un symlink). `diff` no
encuentra diferencias.

El conteo de `/usr/bin/` mostrará cientos de binarios (todo el software
instalado: gcc, gdb, valgrind, make, bash, etc.). `/usr/sbin/` tendrá menos
(herramientas de administración). `/usr/local/bin/` tendrá cero o muy pocos.

La tabla resumen muestra visualmente la situación:
- `/bin`, `/sbin`, `/lib`, `/lib64` → symlinks (merged)
- `/usr/local` → directorio real (no participa del merge)

El merge no duplicó archivos. No hay una copia en `/bin` y otra en `/usr/bin`.
Los symlinks son solo indirecciones — un único conjunto de archivos accesible
desde dos rutas.

</details>

---

## Resumen de conceptos

| Concepto | Detalle clave |
|---|---|
| /usr merge | `/bin`, `/sbin`, `/lib` → symlinks a `/usr/...` |
| Mismo inodo | `/bin/bash` y `/usr/bin/bash` son el mismo archivo |
| Compatibilidad | Paths tradicionales siguen funcionando via symlinks |
| Shebang portable | `#!/usr/bin/env bash` busca en PATH, no depende de path fijo |
| `/usr/local` | No participa del merge; territorio del administrador |
| Prioridad en PATH | `/usr/local/bin` antes que `/usr/bin` |
| `command -v` vs `which` | `command -v` es POSIX y detecta builtins; `which` solo binarios |
| Resolución de comandos | alias → funciones → builtins → `$PATH` |
| `readlink -f` | Resuelve toda la cadena de symlinks recursivamente |
| Estado actual | Todas las distros principales tienen merge (~2023) |
