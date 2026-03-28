# T01 — Creación de bibliotecas estáticas

## Erratas detectadas en el material base

| Archivo | Línea | Error | Corrección |
|---------|-------|-------|------------|
| `README.md` | 180–196 | El ejemplo inline de `main.c` usa `printf` sin `#include <stdio.h>`. Con `-std=c11 -Wall -Wextra -Wpedantic`, esto produce error: en C99+ la declaración implícita de funciones es una violación de constraint. | Agregar `#include <stdio.h>` después de `#include "vec3.h"`. El archivo real del lab (`labs/main.c`) sí lo incluye correctamente. |

---

## 1. ¿Qué es una biblioteca estática?

Una biblioteca estática (`.a` = archive) es un **contenedor** que empaqueta
uno o más archivos objeto (`.o`) en un solo archivo. Durante el enlace, el
linker **copia** el código necesario directamente dentro del ejecutable:

```
Compilación:
  vec3.c       → gcc -c → vec3.o       ─┐
  vec3_print.c → gcc -c → vec3_print.o ─┼→ ar rcs libvec3.a vec3.o vec3_print.o
                                         │
Enlace:                                  │
  main.o + libvec3.a → gcc → programa   │
  (el linker copia solo los .o que main.o necesita)
```

### Convención de nombres

```
lib<nombre>.a

lib     — prefijo obligatorio (GCC lo asume con -l)
nombre  — nombre de la biblioteca
.a      — extensión (archive)

Ejemplos: libmath.a, libparser.a, libvec3.a
```

Cuando le dices a GCC `-lvec3`, busca un archivo llamado `libvec3.a` (o
`libvec3.so` para dinámica). El prefijo `lib` y la extensión son implícitos.

### Diferencia clave con bibliotecas dinámicas

- **Estática** (`.a`): el código se copia al ejecutable en tiempo de enlace.
  El ejecutable final es independiente — no necesita el `.a` para ejecutarse.
- **Dinámica** (`.so`): el código se carga en tiempo de ejecución. El
  ejecutable necesita el `.so` presente en el sistema para funcionar.

---

## 2. Compilación a archivos objeto (`gcc -c`)

El flag `-c` le dice a GCC que compile **sin enlazar**: produce un archivo
objeto (`.o`) en lugar de un ejecutable.

```bash
# Compilar un solo archivo:
gcc -std=c11 -Wall -Wextra -Wpedantic -c vec3.c -o vec3.o

# Compilar varios (cada uno produce su .o):
gcc -std=c11 -Wall -Wextra -Wpedantic -c vec3.c vec3_print.c
# Produce: vec3.o  vec3_print.o
```

El archivo `.o` contiene:
- **Código máquina** de las funciones definidas en el `.c`
- **Tabla de símbolos**: qué funciones define y cuáles necesita de otros módulos
- **Información de reubicación**: las direcciones de memoria no son finales;
  se resuelven durante el enlace

Un `.o` no es ejecutable por sí solo — necesita ser enlazado (con otros `.o`
o bibliotecas) para resolver los símbolos externos como `printf`, `sqrt`, etc.

---

## 3. Inspección de archivos objeto (`file`, `nm`)

### `file` — tipo de archivo

```bash
file vec3.o
# vec3.o: ELF 64-bit LSB relocatable, x86-64, version 1 (SYSV), not stripped
```

La palabra clave es **relocatable**: las direcciones de memoria no son finales.
Compara con un ejecutable (`executable`) o una biblioteca dinámica
(`shared object`).

### `nm` — tabla de símbolos

```bash
nm vec3.o
```

```
                 U sqrt
0000000000000046 T vec3_add
0000000000000000 T vec3_create
00000000000000ef T vec3_dot
0000000000000127 T vec3_length
000000000000009c T vec3_scale
```

Los códigos de tipo más comunes:

| Letra | Significado |
|-------|-------------|
| `T` | **Text** — función definida en este archivo (code section) |
| `U` | **Undefined** — símbolo externo, debe resolverse al enlazar |
| `D` | **Data** — variable global inicializada |
| `B` | **BSS** — variable global sin inicializar |
| `R` | **Read-only** — constante global |

Las letras minúsculas (`t`, `d`, `b`) indican símbolos **locales** (marcados
con `static` en C), que no son visibles fuera del archivo objeto.

En el ejemplo, `sqrt` aparece como `U` porque `vec3.c` la usa (`vec3_length`)
pero la implementación está en `libm`. Se resolverá al enlazar con `-lm`.

---

## 4. Creación del archivo `.a` con `ar`

`ar` (archiver) empaqueta archivos objeto en un archivo `.a`:

```bash
ar rcs libvec3.a vec3.o vec3_print.o
```

### Flags de `ar rcs`

| Flag | Significado |
|------|-------------|
| `r` | **Replace** — insertar archivos (reemplazar si ya existen) |
| `c` | **Create** — crear el archivo si no existe (suprime warning) |
| `s` | **Symbol index** — crear índice de símbolos (equivale a `ranlib`) |

El flag `s` (o ejecutar `ranlib` por separado) crea un índice que permite
al linker encontrar rápidamente qué `.o` contiene cada símbolo. Sin el
índice, el linker tendría que escanear todos los `.o` secuencialmente.

### Otras operaciones de `ar`

```bash
ar t  libvec3.a            # Listar miembros del archivo
ar d  libvec3.a vec3.o     # Eliminar un miembro
ar x  libvec3.a            # Extraer todos los miembros
ar x  libvec3.a vec3.o     # Extraer un miembro específico
```

### Actualizar un miembro

Si modificas `vec3.c` y recompilas a `vec3.o`, puedes actualizar solo ese
miembro:

```bash
gcc -c vec3.c -o vec3.o
ar rcs libvec3.a vec3.o    # reemplaza solo vec3.o, mantiene vec3_print.o
```

---

## 5. Verificación del contenido (`ar t`, `nm` sobre `.a`)

### Listar miembros

```bash
ar t libvec3.a
```

```
vec3.o
vec3_print.o
```

### Listar todos los símbolos

```bash
nm libvec3.a
```

```
vec3.o:
                 U sqrt
0000000000000046 T vec3_add
0000000000000000 T vec3_create
00000000000000ef T vec3_dot
0000000000000127 T vec3_length
000000000000009c T vec3_scale

vec3_print.o:
                 U printf
0000000000000000 T vec3_print
0000000000000031 T vec3_report
```

`nm` sobre un `.a` agrupa los símbolos por archivo objeto. El linker usa el
índice de símbolos (`s` de `ar rcs`) para saber que si un programa necesita
`vec3_add`, debe extraer `vec3.o`.

### `file` sobre la biblioteca

```bash
file libvec3.a
# libvec3.a: current ar archive
```

No es un ELF — es un formato de archivo (ar archive) que contiene ELFs dentro.

---

## 6. Enlazar con la biblioteca (`-L`, `-l`)

```bash
gcc -std=c11 main.c -L. -lvec3 -lm -o program
```

| Flag | Significado |
|------|-------------|
| `-L.` | Agregar `.` (directorio actual) a la ruta de búsqueda de bibliotecas |
| `-lvec3` | Buscar `libvec3.a` (o `libvec3.so`) en las rutas de búsqueda |
| `-lm` | Enlazar con `libm` (para `sqrt` que usa `vec3_length`) |

Equivalente sin `-l` (ruta completa):

```bash
gcc -std=c11 main.c ./libvec3.a -lm -o program
```

### Orden de los argumentos importa

El linker procesa los argumentos **de izquierda a derecha**. Cuando encuentra
un `.o` o `main.c`, registra los símbolos que necesita. Cuando encuentra una
biblioteca (`-l`), busca en ella solo los símbolos **pendientes en ese
momento**.

```bash
# Correcto: main.c primero, luego las bibliotecas que resuelven sus símbolos:
gcc main.c -L. -lvec3 -lm -o program

# Incorrecto: la biblioteca antes de quien la necesita:
gcc -L. -lvec3 main.c -lm -o program
# Puede fallar: cuando el linker procesa -lvec3, aún no sabe qué
# símbolos necesita main.c, así que no extrae nada.
```

### El ejecutable es independiente

Después del enlace, el código de la biblioteca queda **copiado** dentro del
ejecutable:

```bash
./program          # funciona
rm libvec3.a       # eliminar la biblioteca
./program          # sigue funcionando — no la necesita en runtime
```

---

## 7. Enlace selectivo: granularidad por archivo objeto

Este es el concepto más importante del enlace con bibliotecas estáticas.
Cuando el linker procesa un `.a`, **no incluye todos los `.o`** — solo
extrae los archivos objeto que contienen símbolos necesarios.

Pero la granularidad es el **archivo objeto completo**, no funciones
individuales:

```bash
# main_minimal.c solo usa vec3_create y vec3_add
gcc main_minimal.c -L. -lvec3 -lm -o demo_minimal

nm demo_minimal | grep vec3
```

```
<addr> T vec3_add        ← necesario (usado por main)
<addr> T vec3_create     ← necesario (usado por main)
<addr> T vec3_dot        ← incluido (está en vec3.o, igual que add/create)
<addr> T vec3_length     ← incluido (está en vec3.o)
<addr> T vec3_scale      ← incluido (está en vec3.o)
```

**No aparecen** `vec3_print` ni `vec3_report` — están en `vec3_print.o`,
y ningún símbolo de ese archivo fue solicitado, así que el linker no lo
extrajo.

**Sí aparecen** `vec3_dot`, `vec3_length`, `vec3_scale` aunque el programa
no las llama — están en `vec3.o`, el mismo archivo que contiene
`vec3_create` y `vec3_add`.

### Implicación para diseño

Por eso es buena práctica **separar funciones independientes en archivos
`.c` distintos**: cuanto más granulares sean los `.o`, más selectivo puede
ser el linker. Si todas las funciones estuvieran en un solo `vec3_all.c`,
el linker tendría que incluir todo o nada.

---

## 8. Enlace directo con `.o` vs enlace con `.a`

Cuando pasas archivos `.o` directamente en la línea de comandos, el linker
los incluye **incondicionalmente** — no hay selección:

```bash
# Enlace con .o directos — todo se incluye:
gcc main_minimal.c vec3.o vec3_print.o -lm -o demo_direct

# Enlace con .a — enlace selectivo:
gcc main_minimal.c -L. -lvec3 -lm -o demo_minimal
```

```bash
nm demo_direct | grep vec3
# vec3_add, vec3_create, vec3_dot, vec3_length, vec3_scale,
# vec3_print, vec3_report  ← TODOS incluidos

nm demo_minimal | grep vec3
# vec3_add, vec3_create, vec3_dot, vec3_length, vec3_scale
# (sin vec3_print ni vec3_report)
```

`demo_direct` es ligeramente más grande porque incluye código que nunca se
ejecuta. En este ejemplo trivial la diferencia es mínima (~100 bytes), pero
en proyectos con decenas de módulos el enlace selectivo del `.a` reduce
significativamente el tamaño del ejecutable.

---

## 9. Organización de proyecto y Makefile

### Estructura típica

```
proyecto/
├── include/
│   └── vec3.h              ← header público
├── src/
│   ├── vec3.c              ← implementación matemática
│   └── vec3_print.c        ← implementación de impresión
├── lib/
│   └── libvec3.a           ← biblioteca compilada
├── main.c
└── Makefile
```

### Makefile

```makefile
CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude
AR     = ar rcs

SRCS   = src/vec3.c src/vec3_print.c
OBJS   = $(SRCS:.c=.o)

# Target principal
all: program

# Crear la biblioteca
lib/libvec3.a: $(OBJS)
	$(AR) $@ $^

# Regla genérica .c → .o
src/%.o: src/%.c include/vec3.h
	$(CC) $(CFLAGS) -c $< -o $@

# Compilar el programa enlazando con la biblioteca
program: main.c lib/libvec3.a
	$(CC) $(CFLAGS) main.c -Llib -lvec3 -lm -o $@

clean:
	rm -f src/*.o lib/*.a program

.PHONY: all clean
```

Puntos clave:
- `$@` = target, `$^` = todas las dependencias, `$<` = primera dependencia
- `$(SRCS:.c=.o)` sustituye la extensión: `src/vec3.c` → `src/vec3.o`
- `make` solo recompila lo necesario: si solo cambia `vec3.c`, solo se
  regenera `vec3.o` y se actualiza la biblioteca

---

## 10. Bibliotecas estáticas del sistema y comparación con Rust

### Bibliotecas del sistema

```bash
# En Fedora/RHEL, las bibliotecas estáticas están en:
ls /usr/lib64/lib*.a
# libc.a, libm.a, libpthread.a, etc.
# (En Debian/Ubuntu: /usr/lib/x86_64-linux-gnu/)

# Enlazar todo estáticamente:
gcc -static main.c -o program
# El binario incluye todo el código necesario de libc, libm, etc.
# Resultado: binario mucho más grande pero totalmente independiente.

# Mezclar estático y dinámico:
gcc main.c -Wl,-Bstatic -lvec3 -Wl,-Bdynamic -lm -o program
# libvec3 estática, libm dinámica
```

### Comparación con Rust

En Rust, el sistema de bibliotecas es fundamentalmente diferente:

```rust
// Rust usa crates en lugar de archivos .a
// Cargo.toml declara dependencias:
// [dependencies]
// nalgebra = "0.32"

// Rust enlaza estáticamente por defecto (rlib)
// No necesitas ar, nm, ni -L/-l — cargo lo maneja todo

// El equivalente de separar funciones en .c distintos para
// enlace selectivo no existe en Rust: el compilador elimina
// código muerto automáticamente (dead code elimination)
// sin importar cómo organices los módulos.
```

| Aspecto | C (estático) | Rust |
|---------|-------------|------|
| Formato | `.a` (archive de `.o`) | `.rlib` (crate compilado) |
| Herramienta | `ar`, `gcc -l` | `cargo` (automatizado) |
| Selección | Por archivo `.o` | Por función (dead code elimination) |
| Headers | `.h` separado manualmente | Integrado en el crate |
| Dependencias | Manual (`-l`, `-L`, orden) | Automático (`Cargo.toml`) |

---

## Ejercicios

### Ejercicio 1 — Compilar a `.o` e inspeccionar

Compila `vec3.c` y `vec3_print.c` a archivos objeto. Usa `file` para
verificar que son "relocatable" y `nm` para listar sus símbolos.

```bash
cd labs/
gcc -std=c11 -Wall -Wextra -Wpedantic -c vec3.c -o vec3.o
gcc -std=c11 -Wall -Wextra -Wpedantic -c vec3_print.c -o vec3_print.o

file vec3.o
file vec3_print.o

nm vec3.o
nm vec3_print.o
```

<details><summary>Predicción</summary>

`file` para ambos:
```
vec3.o: ELF 64-bit LSB relocatable, x86-64, version 1 (SYSV), not stripped
vec3_print.o: ELF 64-bit LSB relocatable, x86-64, version 1 (SYSV), not stripped
```

`nm vec3.o`:
```
                 U sqrt
0000000000000... T vec3_add
0000000000000... T vec3_create
0000000000000... T vec3_dot
0000000000000... T vec3_length
0000000000000... T vec3_scale
```

- 5 funciones con `T` (definidas), 1 símbolo `U` (`sqrt` de libm).
- Las direcciones empiezan en 0 porque son relativas — se resolverán al enlazar.

`nm vec3_print.o`:
```
                 U printf
0000000000000... T vec3_print
0000000000000... T vec3_report
```

- 2 funciones con `T`, 1 símbolo `U` (`printf` de libc).
- `printf` se resuelve automáticamente porque GCC enlaza libc por defecto.

</details>

---

### Ejercicio 2 — Crear la biblioteca con `ar rcs`

Empaqueta los dos `.o` en `libvec3.a`. Verifica el contenido con `ar t` y
`nm`.

```bash
ar rcs libvec3.a vec3.o vec3_print.o

file libvec3.a
ar t libvec3.a
nm libvec3.a
```

<details><summary>Predicción</summary>

```
libvec3.a: current ar archive
```

`ar t`:
```
vec3.o
vec3_print.o
```

`nm libvec3.a` — muestra los símbolos agrupados por archivo objeto:
```
vec3.o:
                 U sqrt
0000000000000... T vec3_add
0000000000000... T vec3_create
0000000000000... T vec3_dot
0000000000000... T vec3_length
0000000000000... T vec3_scale

vec3_print.o:
                 U printf
0000000000000... T vec3_print
0000000000000... T vec3_report
```

- `file` identifica el `.a` como "ar archive", no como ELF — es un contenedor.
- `ar t` lista los miembros (los `.o` empaquetados).
- `nm` sobre el `.a` es idéntico a ejecutar `nm` sobre cada `.o` por separado.

</details>

---

### Ejercicio 3 — Enlazar y ejecutar el programa completo

Compila `main.c` enlazando con `libvec3.a` y `libm`. Ejecuta el programa.

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic main.c -L. -lvec3 -lm -o demo_vec3
./demo_vec3
```

<details><summary>Predicción</summary>

```
a + b: (5.00, 7.00, 9.00)
a * 2.5: (2.50, 5.00, 7.50)
dot(a, b): 32.00
length(a): 3.74
(1.00, 2.00, 3.00)
(4.00, 5.00, 6.00)
```

- `vec3_add({1,2,3}, {4,5,6})` = `(5, 7, 9)`.
- `vec3_scale({1,2,3}, 2.5)` = `(2.5, 5.0, 7.5)`.
- `vec3_dot({1,2,3}, {4,5,6})` = 1\*4 + 2\*5 + 3\*6 = 4 + 10 + 18 = 32.
- `vec3_length({1,2,3})` = sqrt(1 + 4 + 9) = sqrt(14) ≈ 3.74.
- Las dos últimas líneas son `vec3_print(a)` y `vec3_print(b)`.
- Si omites `-lm`, el enlace falla: `undefined reference to 'sqrt'`.
- Si omites `-L.`, el enlace falla: `cannot find -lvec3`.

</details>

---

### Ejercicio 4 — Verificar que el ejecutable no necesita la biblioteca

Elimina `libvec3.a` y verifica que el programa sigue funcionando. Luego
recrea la biblioteca para los ejercicios siguientes.

```bash
rm libvec3.a
./demo_vec3
echo "Exit code: $?"

# Recrear:
ar rcs libvec3.a vec3.o vec3_print.o
```

<details><summary>Predicción</summary>

```
a + b: (5.00, 7.00, 9.00)
a * 2.5: (2.50, 5.00, 7.50)
dot(a, b): 32.00
length(a): 3.74
(1.00, 2.00, 3.00)
(4.00, 5.00, 6.00)
Exit code: 0
```

- El programa funciona perfectamente sin `libvec3.a` — el código de la
  biblioteca fue **copiado** dentro del ejecutable durante el enlace.
- Esto es la diferencia fundamental con bibliotecas dinámicas (`.so`),
  donde el ejecutable sí necesita el `.so` presente en runtime.

</details>

---

### Ejercicio 5 — Enlace selectivo: programa minimal

Compila `main_minimal.c` (que solo usa `vec3_create` y `vec3_add`) enlazando
con la biblioteca. Usa `nm` para verificar qué símbolos se incluyeron.

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic main_minimal.c -L. -lvec3 -lm -o demo_minimal
nm demo_minimal | grep vec3
```

<details><summary>Predicción</summary>

```
<addr> T vec3_add
<addr> T vec3_create
<addr> T vec3_dot
<addr> T vec3_length
<addr> T vec3_scale
```

- `vec3_print` y `vec3_report` **NO aparecen**: el linker no extrajo
  `vec3_print.o` porque ningún símbolo de ese archivo fue necesario.
- `vec3_dot`, `vec3_length` y `vec3_scale` **SÍ aparecen** aunque
  `main_minimal.c` no las usa: están en `vec3.o`, el mismo archivo objeto
  que contiene `vec3_create` y `vec3_add`.
- La granularidad del enlace selectivo es el **archivo objeto completo**.
  El linker decidió que necesitaba `vec3.o` (por `vec3_create`/`vec3_add`)
  y copió todo su contenido.

</details>

---

### Ejercicio 6 — Enlace directo con `.o` vs con `.a`

Enlaza `main_minimal.c` directamente con ambos `.o` (sin biblioteca).
Compara los símbolos y el tamaño con `demo_minimal`.

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic main_minimal.c vec3.o vec3_print.o -lm -o demo_direct

nm demo_direct | grep vec3
echo "---"
nm demo_minimal | grep vec3

echo ""
ls -l demo_direct demo_minimal
```

<details><summary>Predicción</summary>

`demo_direct`:
```
<addr> T vec3_add
<addr> T vec3_create
<addr> T vec3_dot
<addr> T vec3_length
<addr> T vec3_print      ← incluido (aunque no se usa)
<addr> T vec3_report     ← incluido (aunque no se usa)
<addr> T vec3_scale
```

`demo_minimal`:
```
<addr> T vec3_add
<addr> T vec3_create
<addr> T vec3_dot
<addr> T vec3_length
<addr> T vec3_scale
```

- `demo_direct` contiene **todos** los símbolos de ambos `.o` — al pasar
  `.o` directamente, el linker los incluye incondicionalmente.
- `demo_minimal` no tiene `vec3_print` ni `vec3_report` — el enlace con
  `.a` solo extrajo `vec3.o`.
- `demo_direct` es ligeramente más grande (~100 bytes más). En proyectos
  reales con muchos módulos, la diferencia es significativa.

</details>

---

### Ejercicio 7 — Verificar con `file` los tipos de archivo

Usa `file` sobre cada tipo de archivo generado en el proceso de compilación.

```bash
file vec3.c
file vec3.o
file libvec3.a
file demo_vec3
```

<details><summary>Predicción</summary>

```
vec3.c:      C source, ASCII text
vec3.o:      ELF 64-bit LSB relocatable, x86-64, version 1 (SYSV), not stripped
libvec3.a:   current ar archive
demo_vec3:   ELF 64-bit LSB executable, x86-64, version 1 (SYSV), dynamically linked, ..., not stripped
```

Cada tipo de archivo tiene un rol distinto en la cadena:
- `.c` → código fuente (texto)
- `.o` → **relocatable** (código máquina sin direcciones finales)
- `.a` → **ar archive** (contenedor de `.o`, no es ELF)
- ejecutable → **executable** (o `pie executable` con PIE habilitado),
  **dynamically linked** (porque sigue enlazado con libc/libm dinámicamente
  aunque libvec3 es estática)

Nota: "dynamically linked" no contradice que libvec3 sea estática — el
ejecutable usa libc y libm dinámicamente. Para un enlace 100% estático
se usaría `gcc -static`.

</details>

---

### Ejercicio 8 — El orden de `-l` importa

Intenta enlazar con `-lvec3` **antes** de `main.c` y observa qué pasa.

```bash
# Orden incorrecto:
gcc -std=c11 -L. -lvec3 main.c -lm -o test_order 2>&1

# Orden correcto:
gcc -std=c11 main.c -L. -lvec3 -lm -o test_order 2>&1
```

<details><summary>Predicción</summary>

Orden incorrecto — probable error:
```
/usr/bin/ld: /tmp/ccXXXXXX.o: in function `main':
main.c:(.text+0x...): undefined reference to `vec3_create'
main.c:(.text+0x...): undefined reference to `vec3_add'
...
```

Orden correcto — compila sin error.

- El linker procesa argumentos de izquierda a derecha. Cuando ve `-lvec3`
  primero, no hay ningún símbolo pendiente aún (no ha procesado `main.c`),
  así que no extrae nada de la biblioteca.
- Cuando después procesa `main.c`, necesita `vec3_create` etc., pero
  `-lvec3` ya fue procesado y descartado.
- Nota: algunos linkers (como el de GNU con `--as-needed` deshabilitado o
  con ciertos flags) pueden ser más permisivos, pero el comportamiento
  estándar es sensible al orden.

</details>

---

### Ejercicio 9 — Enlace estático completo con `-static`

Compila `main_minimal.c` con enlace completamente estático y compara el
tamaño con el enlace normal.

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic main_minimal.c -L. -lvec3 -lm -o demo_dynamic
gcc -std=c11 -Wall -Wextra -Wpedantic -static main_minimal.c -L. -lvec3 -lm -o demo_static

ls -l demo_dynamic demo_static
file demo_dynamic
file demo_static
```

<details><summary>Predicción</summary>

```
-rwxr-xr-x ... ~16000 ... demo_dynamic
-rwxr-xr-x ... ~800000+ ... demo_static
```

- `demo_static` es **mucho más grande** (≈50-100x) porque incluye todo
  el código de libc y libm copiado dentro del ejecutable.
- `demo_dynamic` es pequeño porque libc/libm se cargan del sistema en runtime.

```
demo_dynamic: ELF 64-bit LSB executable, ..., dynamically linked, ...
demo_static:  ELF 64-bit LSB executable, ..., statically linked, ...
```

- `file` confirma la diferencia: "dynamically linked" vs "statically linked".
- `demo_static` es completamente independiente — puede copiarse a otro
  sistema Linux x86-64 y funcionar sin instalar ninguna biblioteca.
- Nota: en algunos sistemas, la compilación estática puede fallar si no
  están instaladas las versiones estáticas de las bibliotecas del sistema
  (`glibc-static` en Fedora).

</details>

---

### Ejercicio 10 — Inspeccionar macros predefinidas del sistema

Usa `gcc -dM -E` (del tópico anterior) para explorar las bibliotecas
estáticas disponibles en tu sistema.

```bash
# ¿Cuántas bibliotecas estáticas hay en tu sistema?
ls /usr/lib64/lib*.a 2>/dev/null | wc -l

# ¿Está la versión estática de libm?
ls -l /usr/lib64/libm.a 2>/dev/null

# Ver los símbolos de libm.a (solo contar):
nm /usr/lib64/libm.a 2>/dev/null | grep " T " | wc -l

# Ver qué funciones matemáticas define:
nm /usr/lib64/libm.a 2>/dev/null | grep " T " | head -20
```

<details><summary>Predicción</summary>

Número de bibliotecas estáticas: varía según lo instalado. En un sistema
Fedora típico con herramientas de desarrollo, puede haber 10-50+.

`libm.a` existe si tienes `glibc-static` instalado:
```
/usr/lib64/libm.a
```

Número de funciones en libm: cientos (sin, cos, tan, exp, log, sqrt,
pow, ceil, floor, fabs, etc., más sus variantes float/long double).

```
... T acos
... T acosf
... T acosh
... T acoshf
... T asin
...
```

- Cada función matemática tiene variantes: `sin` (double), `sinf` (float),
  `sinl` (long double).
- Si `libm.a` no existe, necesitas instalar: `sudo dnf install glibc-static`.
- La diferencia entre `libm.a` (estática) y `libm.so` (dinámica): ambas
  contienen las mismas funciones, pero `.a` se copia al ejecutable y `.so`
  se carga en runtime.

</details>

---

### Limpieza

```bash
rm -f *.o libvec3.a demo_vec3 demo_minimal demo_direct demo_dynamic demo_static test_order
```
