# T02 — Enlace estático

> Sin erratas detectadas en el material base.

---

## 1. Cómo funciona el linker: resolución de símbolos

El linker (`ld`, invocado internamente por `gcc`) conecta llamadas a
funciones con sus definiciones. Mantiene dos conjuntos mientras procesa
archivos de izquierda a derecha:

- **Definidos** (`T`): símbolos cuya implementación ya se encontró.
- **Indefinidos** (`U`): símbolos referenciados pero sin implementación aún.

```
main.o                    libgreet.a (contiene greet.o)
┌───────────────────┐     ┌───────────────────────┐
│ main()        [T] │     │ greet_person()    [T] │
│ greet_person  [U] │     │ format_name       [U] │
│ printf        [U] │     │ printf            [U] │
└───────────────────┘     └───────────────────────┘
                                    │
                          libformat.a (contiene format.o)
                          ┌───────────────────────┐
                          │ format_name()     [T] │
                          │ snprintf          [U] │
                          └───────────────────────┘
         │                          │
         └──────── LINKER ──────────┘
                     │
               ┌─────┴──────────┐
               │   ejecutable    │
               │   main()        │
               │   greet_person  │ ← copiado de greet.o
               │   format_name   │ ← copiado de format.o
               │   printf        │ ← resuelto por libc
               │   snprintf      │ ← resuelto por libc
               └────────────────┘
```

Lo que `gcc` hace internamente:

```bash
gcc main.c -L./lib -lgreet -lformat -o program

# Internamente:
# 1. Compila main.c → main.o (archivo temporal)
# 2. Invoca el linker:
#    ld main.o -L./lib -lgreet -lformat -lc -o program
#    (simplificado — también agrega crt0.o, crti.o, etc.)
```

El linker no sabe nada de C ni de tipos — solo ve **nombres de símbolos**
(cadenas de texto). Por eso pueden ocurrir errores de enlace que el
compilador no detectó (como pasar `int` donde se esperaba `double`).

---

## 2. `-l` y `-L`: buscar bibliotecas

### `-l<nombre>`: enlazar con una biblioteca

```bash
gcc main.c -lvec3 -lm -o program
```

El linker busca (en este orden):
1. `libvec3.so` (dinámica — **preferida** por defecto)
2. `libvec3.a` (estática — usada si no hay `.so`)

El prefijo `lib` y la extensión (`.so` o `.a`) son implícitos.

### `-L<dir>`: agregar directorio de búsqueda

```bash
gcc main.c -L./lib -L/opt/mylibs/lib -lgreet -o program
```

Directorios de búsqueda por defecto:
- `/usr/lib64/` (Fedora/RHEL)
- `/usr/lib/` (general)
- `/usr/local/lib/`
- Directorios en `/etc/ld.so.conf` (más para dinámicas)

```bash
# Ver los directorios de búsqueda del linker:
ld --verbose | grep SEARCH_DIR
```

### Ruta directa: alternativa a `-L`/`-l`

```bash
# Equivalentes:
gcc main.c -L./lib -lgreet -o program     # busca .so primero
gcc main.c ./lib/libgreet.a -o program     # usa exactamente este .a
```

La diferencia: con `-l` el linker prefiere `.so` sobre `.a`. Con ruta
directa, se usa exactamente el archivo indicado — es una forma de forzar
enlace estático para una biblioteca específica.

---

## 3. El orden de los argumentos importa

Este es el concepto más importante del enlace estático. El linker procesa
los argumentos **de izquierda a derecha**, manteniendo la lista de símbolos
indefinidos:

```bash
# Cadena de dependencias:
# main.o → necesita greet_person (de libgreet)
# libgreet → necesita format_name (de libformat)
# libformat → necesita snprintf (de libc)
```

### Regla: dependencias después de quien las usa

```bash
# CORRECTO — cada biblioteca viene después de quien la necesita:
gcc main.o -L./lib -lgreet -lformat -o program
# 1. Lee main.o → greet_person queda UNDEFINED
# 2. Lee libgreet.a → encuentra greet_person → lo incluye
#    → format_name queda UNDEFINED
# 3. Lee libformat.a → encuentra format_name → lo incluye
#    → snprintf queda UNDEFINED
# 4. Lee libc → resuelve snprintf y printf
# 5. Sin UNDEFINED restantes → éxito
```

### Errores por orden incorrecto

```bash
# FALLA — libformat antes de libgreet:
gcc main.o -L./lib -lformat -lgreet -o program
# 1. Lee main.o → greet_person UNDEFINED
# 2. Lee libformat.a → no hay nada que resolver → NO incluye nada
# 3. Lee libgreet.a → incluye greet.o → format_name UNDEFINED
# 4. libc no tiene format_name → ERROR: undefined reference to 'format_name'
```

```bash
# FALLA — bibliotecas antes del .o:
gcc -L./lib -lgreet -lformat main.o -o program
# 1. Lee libgreet.a → sin UNDEFINED aún → no incluye nada
# 2. Lee libformat.a → sin UNDEFINED → no incluye nada
# 3. Lee main.o → greet_person UNDEFINED → pero las bibliotecas ya pasaron
# → ERROR: undefined reference to 'greet_person'
```

---

## 4. Dependencias circulares y `--start-group`

Si dos bibliotecas se referencian mutuamente, ningún orden funciona:

```bash
# libA necesita func_b (de libB)
# libB necesita func_a (de libA)

# Ni este:
gcc main.o -lA -lB -o program   # libB no resuelve func_a (ya pasó)
# Ni este:
gcc main.o -lB -lA -o program   # libA no resuelve func_b (ya pasó)
```

### Solución 1: repetir la biblioteca

```bash
gcc main.o -lA -lB -lA -o program
# El linker la procesa dos veces — resuelve ambas direcciones.
```

### Solución 2: `--start-group` / `--end-group`

```bash
gcc main.o -Wl,--start-group -lA -lB -Wl,--end-group -o program
# El linker itera sobre el grupo hasta que no quedan UNDEFINED.
```

`--start-group` es más lento en proyectos grandes (itera), pero es más
robusto. Las dependencias circulares entre bibliotecas son poco comunes;
si ocurren frecuentemente, puede indicar que la separación en bibliotecas
es incorrecta.

---

## 5. Forzar enlace estático

Por defecto, `gcc` prefiere `.so` (dinámica) sobre `.a`. Tres formas de
forzar enlace estático:

### 1. `-static`: todo estático

```bash
gcc main.c -static -lvec3 -lm -o program
# Incluye todo estáticamente: libvec3, libm, libc.
# Binario grande (~800KB+), pero sin dependencias externas.
# Requiere glibc-static instalado (sudo dnf install glibc-static).
```

### 2. Selectivo con `-Wl,-Bstatic` / `-Wl,-Bdynamic`

```bash
gcc main.c -Wl,-Bstatic -lvec3 -Wl,-Bdynamic -lm -o program
# libvec3: estática (copiada al binario)
# libm: dinámica (cargada en runtime)
```

`-Wl,` pasa el flag directamente al linker. `-Bstatic` activa modo estático
y `-Bdynamic` vuelve al modo dinámico para las bibliotecas que siguen.

### 3. Ruta directa al `.a`

```bash
gcc main.c ./lib/libvec3.a -lm -o program
# Fuerza usar el .a específico, sin buscar .so.
```

---

## 6. Verificar dependencias con `ldd`

`ldd` muestra las bibliotecas dinámicas que necesita un ejecutable:

```bash
# Enlace normal (libc dinámica, libcalc estática):
ldd calc_dynamic
```

```
linux-vdso.so.1 (0x...)
libc.so.6 => /lib64/libc.so.6 (0x...)
/lib64/ld-linux-x86-64.so.2 (0x...)
```

- `libc.so.6` aparece → es dependencia dinámica.
- `libcalc` no aparece → fue enlazada estáticamente (copiada al binario).

```bash
# Enlace completamente estático:
ldd calc_static
```

```
not a dynamic executable
```

No tiene dependencias externas. Se puede copiar a cualquier sistema Linux
de la misma arquitectura y ejecutar sin instalar nada.

### Confirmar con `nm`

```bash
nm calc_dynamic | grep calc_
# <addr> T calc_add      ← copiado dentro del binario
# <addr> T calc_sub      ← copiado dentro del binario
```

Los símbolos de `libcalc` tienen `T` (definidos) en el ejecutable — fueron
copiados durante el enlace estático.

---

## 7. Diagnóstico de errores del linker

### Error 1: `undefined reference`

```
/usr/bin/ld: greet.o: undefined reference to 'format_name'
```

**Causa**: el linker no encontró la definición del símbolo.
**Diagnóstico**:

```bash
nm -A lib/lib*.a | grep format_name
# lib/libformat.a:format.o:... T format_name  ← existe, con T
# lib/libgreet.a:greet.o:      U format_name  ← la necesita
```

Si aparece con `T` → el símbolo existe pero hay **problema de orden**.
Si no aparece → falta compilar o agregar al `.a`.

### Error 2: `cannot find -l`

```
/usr/bin/ld: cannot find -lgreet: No such file or directory
```

**Causa**: el linker no encuentra `libgreet.a` ni `libgreet.so` en las
rutas de búsqueda.
**Solución**: agregar `-L<directorio>` o verificar que la biblioteca existe.

### Error 3: `multiple definition`

```
/usr/bin/ld: greet_duplicate.o: multiple definition of 'greet_person'
```

**Causa**: el mismo símbolo está definido en dos archivos objeto.
**Diagnóstico**:

```bash
nm -A greet.o greet_duplicate.o | grep "T greet_person"
# greet.o:...           T greet_person
# greet_duplicate.o:... T greet_person   ← duplicado
```

**Solución**: eliminar la definición duplicada, o marcar una como `static`
si es interna al módulo.

---

## 8. Herramientas de inspección del linker

### `gcc -v`: ver el comando completo del linker

```bash
gcc -v main.c -L./lib -lgreet -lformat -o program 2>&1
# Muestra el comando ld completo con todos los paths y archivos.
```

### `gcc -Wl,--trace`: ver qué archivos procesa el linker

```bash
gcc main.o -L./lib -lgreet -lformat -o program -Wl,--trace
```

```
main.o
(./lib/libgreet.a)greet.o
(./lib/libformat.a)format.o
```

Muestra exactamente qué `.o` extrajo de cada `.a` — la resolución selectiva
en acción. Si una biblioteca no aparece, significa que el linker no necesitó
extraer nada de ella.

### `gcc -Wl,--verbose`: información completa del linker

```bash
gcc -Wl,--verbose main.c -L./lib -lvec3 -o program
# Muestra rutas de búsqueda, scripts del linker, decisiones de resolución.
```

---

## 9. `pkg-config`: automatizar flags de enlace

Para bibliotecas instaladas en el sistema, `pkg-config` genera
automáticamente los flags `-I`, `-L` y `-l` correctos:

```bash
# Ver flags de compilación (includes):
pkg-config --cflags libpng
# -I/usr/include/libpng16

# Ver flags de enlace:
pkg-config --libs libpng
# -lpng16 -lz

# Usar directamente en gcc:
gcc main.c $(pkg-config --cflags --libs libpng) -o program

# Listar todas las bibliotecas disponibles:
pkg-config --list-all

# Verificar si una biblioteca está instalada:
pkg-config --exists libpng && echo "OK" || echo "Not found"
```

Los archivos `.pc` (configuración de `pkg-config`) están típicamente en
`/usr/lib64/pkgconfig/` o `/usr/share/pkgconfig/`. Se pueden crear `.pc`
propios para proyectos internos.

`pkg-config` resuelve el problema de recordar qué flags necesita cada
biblioteca — especialmente útil cuando una biblioteca requiere varios
`-l` en un orden específico.

---

## 10. Comparación con Rust

| Aspecto | C (enlace estático) | Rust (cargo) |
|---------|-------------------|-------------|
| Dependencias | Manual: `-l`, `-L`, orden correcto | Automático: `Cargo.toml` |
| Orden | Importa (izquierda a derecha) | Cargo lo resuelve solo |
| Circular | Repetir lib o `--start-group` | No ocurre (grafo dirigido acíclico) |
| Diagnóstico | `nm`, `ldd`, `--trace` | `cargo build --verbose` |
| Estático/dinámico | `-static`, `-Wl,-Bstatic` | `crate-type = ["staticlib"]` |
| Flags | `pkg-config` o manual | `build.rs` + `pkg-config` crate |

En Rust, `cargo` maneja automáticamente el orden de dependencias, la
resolución de versiones y los flags de enlace. El equivalente de los errores
de orden en C simplemente no existe — `cargo` genera el comando de enlace
correcto.

Sin embargo, cuando Rust enlaza con bibliotecas C (FFI), los mismos
conceptos de `-l`, `-L` y orden aplican en el `build.rs`:

```rust
// build.rs — indica a cargo cómo enlazar con una biblioteca C
fn main() {
    println!("cargo:rustc-link-search=native=./lib");
    println!("cargo:rustc-link-lib=static=vec3");
    // Equivalente a: gcc -L./lib -lvec3
}
```

---

## Ejercicios

### Ejercicio 1 — Preparar las bibliotecas con dependencia

Compila `format.c` y `greet.c` a archivos objeto, crea `libformat.a` y
`libgreet.a`, y verifica los símbolos de cada una con `nm`.

```bash
cd labs/
gcc -std=c11 -Wall -Wextra -Wpedantic -c format.c -o format.o
gcc -std=c11 -Wall -Wextra -Wpedantic -c greet.c -o greet.o
gcc -std=c11 -Wall -Wextra -Wpedantic -c main.c -o main.o

ar rcs libformat.a format.o
ar rcs libgreet.a greet.o

mkdir -p lib
mv libformat.a libgreet.a lib/

nm lib/libformat.a
nm lib/libgreet.a
```

<details><summary>Predicción</summary>

`nm lib/libformat.a`:
```
format.o:
0000000000000... d buf.0
0000000000000... T format_name
                 U snprintf
```

`nm lib/libgreet.a`:
```
greet.o:
                 U format_name
0000000000000... T greet_person
                 U printf
```

- `libformat.a` define `format_name` (`T`) y necesita `snprintf` (`U`).
- `libgreet.a` define `greet_person` (`T`) y necesita `format_name` (`U`)
  y `printf` (`U`).
- `buf.0` con `d` (data, local) es el buffer `static char buf[256]` de
  `format.c` — minúscula porque es `static`.
- La cadena de dependencias: `main.o` → `libgreet` → `libformat` → `libc`.

</details>

---

### Ejercicio 2 — Enlazar en el orden correcto

Enlaza `main.o` con las dos bibliotecas en el orden correcto. Ejecuta
el programa.

```bash
gcc main.o -L./lib -lgreet -lformat -o greet_demo
./greet_demo
```

<details><summary>Predicción</summary>

```
=== Static linking demo ===
Hello, [Alice]!
Hello, [Bob]!
```

- El orden `-lgreet -lformat` es correcto: primero la que `main.o` necesita
  (`libgreet`), después la que `libgreet` necesita (`libformat`).
- `format_name("Alice")` produce `"[Alice]"`, y `greet_person` imprime
  `"Hello, [Alice]!"`.
- `snprintf` y `printf` se resuelven automáticamente con `libc`.

</details>

---

### Ejercicio 3 — Provocar error: orden invertido de bibliotecas

Intenta enlazar con `-lformat` antes de `-lgreet`.

```bash
gcc main.o -L./lib -lformat -lgreet -o greet_demo 2>&1
```

<details><summary>Predicción</summary>

```
/usr/bin/ld: ./lib/libgreet.a(greet.o): in function `greet_person':
greet.c:(.text+0x...): undefined reference to `format_name'
collect2: error: ld returned 1 exit status
```

- El linker procesa `libformat.a` primero. En ese momento no hay símbolos
  indefinidos que format pueda resolver → no incluye nada.
- Después procesa `libgreet.a`, incluye `greet.o` (porque `main.o`
  necesita `greet_person`), pero `greet.o` necesita `format_name`.
- `libformat.a` ya fue procesada → `format_name` queda sin resolver → error.
- La solución: invertir el orden a `-lgreet -lformat`.

</details>

---

### Ejercicio 4 — Provocar error: bibliotecas antes del `.o`

Intenta enlazar poniendo las bibliotecas antes de `main.o`.

```bash
gcc -L./lib -lgreet -lformat main.o -o greet_demo 2>&1
```

<details><summary>Predicción</summary>

```
/usr/bin/ld: main.o: in function `main':
main.c:(.text+0x...): undefined reference to `greet_person'
collect2: error: ld returned 1 exit status
```

- El linker lee `libgreet.a` y `libformat.a` primero. No hay ningún símbolo
  indefinido todavía → no incluye nada de ninguna.
- Después lee `main.o` → `greet_person` queda indefinido.
- Las bibliotecas ya pasaron → error.
- Nota: el error menciona `greet_person`, no `format_name` — porque el
  linker ni siquiera llegó a incluir `greet.o`, así que nunca descubrió
  la dependencia de `format_name`.

</details>

---

### Ejercicio 5 — Inspeccionar el proceso con `--trace`

Enlaza correctamente y usa `--trace` para ver qué archivos extrae el linker.

```bash
gcc main.o -L./lib -lgreet -lformat -o greet_demo -Wl,--trace 2>&1
```

<details><summary>Predicción</summary>

```
/usr/bin/ld: mode elf_x86_64
/usr/lib/.../crt1.o
/usr/lib/.../crti.o
...
main.o
(./lib/libgreet.a)greet.o
(./lib/libformat.a)format.o
...
/usr/lib/.../libc.so.6
...
```

- `main.o` aparece directamente (incluido incondicionalmente).
- `(./lib/libgreet.a)greet.o` — el linker extrajo `greet.o` de `libgreet.a`.
- `(./lib/libformat.a)format.o` — extrajo `format.o` de `libformat.a`.
- Los archivos `crt1.o`, `crti.o` son el **startup code** que GCC agrega
  automáticamente (inicialización antes de `main`).
- `libc.so.6` aparece como dependencia dinámica (no estática).

</details>

---

### Ejercicio 6 — Enlace con ruta directa al `.a`

Enlaza usando la ruta completa a los `.a` en vez de `-L`/`-l`.

```bash
gcc main.o ./lib/libgreet.a ./lib/libformat.a -o greet_demo_direct
./greet_demo_direct
```

<details><summary>Predicción</summary>

```
=== Static linking demo ===
Hello, [Alice]!
Hello, [Bob]!
```

- El resultado es idéntico al ejercicio 2.
- La diferencia: con ruta directa, no se busca `.so` — se usa exactamente
  el `.a` indicado.
- El orden sigue importando: `libgreet.a` antes de `libformat.a`.
- Este método es más explícito y evita la ambigüedad de si el linker
  elegirá `.so` o `.a`.

</details>

---

### Ejercicio 7 — Enlace completamente estático

Compila `main_calc.c` con `libcalc.a` en modo normal y con `-static`.
Compara tamaños y dependencias.

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -c calc.c -o calc.o
ar rcs libcalc.a calc.o
gcc -std=c11 -Wall -Wextra -Wpedantic -c main_calc.c -o main_calc.o

gcc main_calc.o libcalc.a -o calc_dynamic
gcc -static main_calc.o libcalc.a -o calc_static

ls -lh calc_dynamic calc_static
```

<details><summary>Predicción</summary>

```
-rwxr-xr-x ... ~16K  ... calc_dynamic
-rwxr-xr-x ... ~800K ... calc_static
```

- `calc_static` es ~50x más grande porque incluye toda `libc` copiada
  dentro del binario.
- `calc_dynamic` es pequeño porque solo contiene `calc_add`/`calc_sub` y
  el código de `main`; `printf` y demás se cargan de `libc.so` en runtime.
- Si `-static` falla con `cannot find -lc`, instalar: `sudo dnf install
  glibc-static`.

</details>

---

### Ejercicio 8 — Verificar con `ldd` y `nm`

Compara las dependencias dinámicas y los símbolos de ambos binarios.

```bash
echo "=== calc_dynamic ==="
ldd calc_dynamic
echo ""
nm calc_dynamic | grep -E "calc_|printf"

echo ""
echo "=== calc_static ==="
ldd calc_static
echo ""
nm calc_static | grep -E "calc_|printf" | head -5
```

<details><summary>Predicción</summary>

`calc_dynamic`:
```
linux-vdso.so.1 (0x...)
libc.so.6 => /lib64/libc.so.6 (0x...)
/lib64/ld-linux-x86-64.so.2 (0x...)
```
```
<addr> T calc_add
<addr> T calc_sub
                 U printf@@GLIBC_...
```

`calc_static`:
```
not a dynamic executable
```
```
<addr> T calc_add
<addr> T calc_sub
<addr> T printf
...
```

- En `calc_dynamic`, `printf` aparece con `U` (undefined) — se resuelve
  en runtime cargando `libc.so.6`.
- En `calc_static`, `printf` aparece con `T` (definido) — la implementación
  completa de printf fue copiada de `libc.a` al binario.
- `calc_add`/`calc_sub` tienen `T` en ambos — siempre fueron estáticas.

</details>

---

### Ejercicio 9 — Provocar y diagnosticar `multiple definition`

Compila `greet_duplicate.c` (que define otra versión de `greet_person`) y
intenta enlazar ambos `.o`. Diagnostica con `nm`.

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -c greet_duplicate.c -o greet_duplicate.o

# Provocar el error:
gcc main.o greet.o greet_duplicate.o ./lib/libformat.a -o greet_demo 2>&1

# Diagnosticar:
nm -A greet.o greet_duplicate.o | grep "T greet_person"
```

<details><summary>Predicción</summary>

Error:
```
/usr/bin/ld: greet_duplicate.o: in function `greet_person':
greet_duplicate.c:(.text+0x...): multiple definition of `greet_person'; greet.o:greet.c:(.text+0x...): first defined here
collect2: error: ld returned 1 exit status
```

Diagnóstico con `nm`:
```
greet.o:0000000000000000 T greet_person
greet_duplicate.o:0000000000000000 T greet_person
```

- Ambos `.o` definen `greet_person` con `T` — el linker no puede elegir.
- En C, cada símbolo externo debe tener exactamente **una** definición
  en todo el programa (One Definition Rule).
- Soluciones: eliminar el duplicado, marcar uno como `static` (lo hace
  local al `.o`), o usar `__attribute__((weak))` si se quiere un fallback.

</details>

---

### Ejercicio 10 — Explorar `pkg-config`

Usa `pkg-config` para explorar las bibliotecas instaladas en tu sistema.

```bash
# ¿Cuántas bibliotecas conoce pkg-config?
pkg-config --list-all 2>/dev/null | wc -l

# Ver flags de una biblioteca conocida (zlib suele estar instalada):
pkg-config --cflags zlib 2>/dev/null
pkg-config --libs zlib 2>/dev/null

# Ver los paths donde busca archivos .pc:
pkg-config --variable=pc_path pkg-config 2>/dev/null

# Si tienes libpng instalada:
pkg-config --cflags --libs libpng 2>/dev/null
```

<details><summary>Predicción</summary>

Número de bibliotecas: varía según el sistema. En Fedora con herramientas
de desarrollo instaladas, típicamente 50-200+.

zlib:
```
(cflags vacío — zlib está en rutas estándar de include)
-lz
```

Paths de `.pc`:
```
/usr/lib64/pkgconfig:/usr/share/pkgconfig
```
(En Fedora. En Debian/Ubuntu sería `/usr/lib/x86_64-linux-gnu/pkgconfig`.)

libpng (si está instalada):
```
-I/usr/include/libpng16
-lpng16 -lz
```

- `pkg-config` sabe que `libpng` también necesita `-lz` (zlib).
- Sin `pkg-config`, tendrías que recordar esta dependencia manualmente.
- Si una biblioteca no está instalada, `pkg-config` devuelve error y
  código de salida 1.

</details>

---

### Limpieza

```bash
rm -f *.o libcalc.a
rm -rf lib/
rm -f greet_demo greet_demo_direct calc_dynamic calc_static greet_duplicate.o
```
