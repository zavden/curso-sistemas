# T02 — Enlace dinámico

> Sin erratas detectadas en el material base.

---

## 1. Cómo funciona el enlace dinámico

A diferencia del enlace estático (que copia código), el enlace dinámico
deja **referencias** que se resuelven en runtime:

```
Link time (compilación):
  El linker verifica que los símbolos EXISTEN en la .so,
  pero NO copia el código. Solo graba:
  - El nombre de la .so necesaria
  - Stubs PLT para cada función externa

Runtime (ejecución):
  ld-linux.so (el dynamic linker):
  1. Lee el binario → encuentra qué .so necesita
  2. Busca cada .so en los paths configurados
  3. Carga las .so en memoria (mmap)
  4. Resuelve los símbolos (llena la GOT)
  5. Llama a main()
```

**Dos fases con configuraciones independientes**:
- **Link time** (`gcc`): usa `-L` para encontrar la `.so` durante la
  compilación. Esta información **no se guarda** en el ejecutable.
- **Runtime** (`ld-linux.so`): usa RUNPATH, `LD_LIBRARY_PATH`, ldconfig
  cache, o directorios por defecto. No sabe nada de `-L`.

Esto explica por qué un programa puede **compilar sin errores** pero
**fallar al ejecutar** si el dynamic linker no encuentra la `.so`.

---

## 2. El dynamic linker: `ld-linux.so`

`ld-linux.so` es el programa que carga bibliotecas dinámicas. Es lo
primero que el kernel ejecuta cuando lanzas un binario dinámico:

```bash
file /usr/bin/ls
# ELF 64-bit LSB pie executable, ...
# interpreter /lib64/ld-linux-x86-64.so.2
```

El "interpreter" es `ld-linux.so`. El kernel lo ejecuta primero, y
`ld-linux.so` se encarga de:

1. Leer el binario y encontrar qué `.so` necesita (`NEEDED`)
2. Buscar cada `.so` en los paths configurados
3. Cargarlas en memoria con `mmap` (pages compartidas, read-only)
4. Resolver los símbolos (llenar la GOT)
5. Ejecutar constructores (`__attribute__((constructor))`)
6. Llamar a `main()`

```bash
# Ejecutar ld-linux.so directamente:
/lib64/ld-linux-x86-64.so.2 --list /usr/bin/ls
# Equivale a: ldd /usr/bin/ls
```

---

## 3. PLT y GOT: resolución de símbolos en runtime

### PLT (Procedure Linkage Table)

Tabla de "stubs" — funciones pequeñas que redireccionan a la dirección
real de la función externa.

### GOT (Global Offset Table)

Tabla de punteros que contiene las direcciones reales de las funciones
y variables de las `.so` cargadas.

### Lazy binding (primera llamada)

```
Primera llamada a vec3_create():
1. main() → call PLT[vec3_create]
2. PLT stub lee GOT[vec3_create]
3. GOT aún NO tiene la dirección → llama a ld-linux.so
4. ld-linux.so busca vec3_create en libvec3.so
5. Escribe la dirección real en GOT[vec3_create]
6. Salta a vec3_create()

Segunda llamada (y todas las siguientes):
1. main() → call PLT[vec3_create]
2. PLT stub lee GOT[vec3_create]
3. GOT YA tiene la dirección → salta directamente
→ Sin overhead de resolución
```

La primera llamada tiene overhead (buscar el símbolo), pero las
siguientes son casi tan rápidas como una llamada directa.

```bash
# Ver los stubs PLT de un binario:
objdump -d program | grep "@plt"
# <addr> <vec3_create@plt>:
# <addr> <printf@plt>:
```

---

## 4. Orden de búsqueda del dynamic linker

El dynamic linker busca `.so` en este orden:

```
1. RPATH    (embebido en el ELF — deprecated)
2. LD_LIBRARY_PATH  (variable de entorno)
3. RUNPATH  (embebido en el ELF — recomendado)
4. Cache de ldconfig  (/etc/ld.so.cache)
5. Directorios por defecto  (/lib64, /usr/lib64)
```

La diferencia entre RPATH y RUNPATH es importante:
- **RPATH** (legacy): se busca **antes** de `LD_LIBRARY_PATH` → el
  usuario no puede hacer override
- **RUNPATH** (moderno): se busca **después** de `LD_LIBRARY_PATH` →
  el usuario puede hacer override (más flexible)

GCC moderno genera RUNPATH por defecto al usar `-Wl,-rpath`.

---

## 5. `LD_LIBRARY_PATH`

Variable de entorno que agrega directorios de búsqueda para el dynamic
linker:

```bash
# Para un solo comando:
LD_LIBRARY_PATH=./lib ./program

# O exportar para toda la sesión:
export LD_LIBRARY_PATH=/opt/mylibs/lib:$LD_LIBRARY_PATH
./program
```

### Cuándo usarlo

- **Desarrollo local**: compilar y probar rápidamente sin instalar
- **Testing**: probar una versión diferente de una biblioteca

### Por qué NO usarlo en producción

- **Frágil**: si se mueve la `.so`, hay que actualizar la variable
- **Global**: afecta a **todos** los programas de la sesión
- **Seguridad**: se ignora en binarios setuid/setgid (por diseño)
- **No persistente**: se pierde al cerrar la terminal
- **Puede cargar `.so` incorrectos** de directorios inesperados

---

## 6. RUNPATH con `-rpath` y `$ORIGIN`

La forma recomendada de indicar dónde buscar `.so` es embeber la ruta
**dentro del binario**:

```bash
gcc main.c -L./lib -lmathops -Wl,-rpath,'$ORIGIN/../lib' -o app/bin/program
```

`$ORIGIN` es una variable especial que el dynamic linker reemplaza por
el directorio del ejecutable en runtime.

```bash
# Verificar el RUNPATH embebido:
readelf -d app/bin/program | grep RUNPATH
# (RUNPATH)  Library runpath: [$ORIGIN/../lib]
```

### Ejemplos de RUNPATH

```bash
-Wl,-rpath,/opt/myapp/lib       # Path absoluto (no portable)
-Wl,-rpath,'$ORIGIN/lib'        # Relativo al ejecutable
-Wl,-rpath,'$ORIGIN/../lib'     # Un nivel arriba del ejecutable
```

### Portabilidad con `$ORIGIN`

Si la estructura relativa entre el ejecutable y la `.so` se mantiene,
se puede mover todo el directorio sin romper nada:

```bash
# Estructura original:
app/bin/program     ← ejecutable con RUNPATH=$ORIGIN/../lib
app/lib/libfoo.so   ← biblioteca

# Mover todo:
mv app /tmp/app_moved
/tmp/app_moved/bin/program    # ¡Funciona!
# $ORIGIN se resuelve a /tmp/app_moved/bin/
# ../lib → /tmp/app_moved/lib/ → encuentra libfoo.so
```

**Nota**: `$ORIGIN` debe estar entre comillas simples para que el shell
no lo interprete como variable de shell.

---

## 7. `ldconfig` y `/etc/ld.so.conf`

`ldconfig` es la forma estándar de instalar bibliotecas para que todos
los programas las encuentren. Gestiona el **cache** del dynamic linker:

```bash
# 1. Copiar la biblioteca al sistema:
sudo cp libmathops.so /usr/local/lib/

# 2. Agregar el directorio al config (si no está):
echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/mathops.conf

# 3. Actualizar el cache:
sudo ldconfig

# 4. Verificar que está en el cache:
ldconfig -p | grep mathops
# libmathops.so (libc6,x86-64) => /usr/local/lib/libmathops.so
```

Lo que hace `ldconfig`:
1. Lee `/etc/ld.so.conf` y `/etc/ld.so.conf.d/*.conf`
2. Escanea los directorios listados
3. Crea symlinks de soname (p.ej. `libfoo.so.1` → `libfoo.so.1.2.3`)
4. Actualiza el cache binario `/etc/ld.so.cache`

Requiere root. Es la opción correcta para producción — la biblioteca
queda disponible globalmente sin variables de entorno ni RUNPATH.

---

## 8. Diagnóstico: `ldd` y `LD_DEBUG`

### `ldd` — listar dependencias

```bash
ldd program
# libmathops.so => not found           ← problema
# libc.so.6 => /lib64/libc.so.6        ← OK
```

`not found` indica que el dynamic linker no puede encontrar la `.so`.

### `LD_DEBUG` — trazar la búsqueda paso a paso

```bash
# Ver dónde busca las bibliotecas:
LD_DEBUG=libs LD_LIBRARY_PATH=. ./program 2>&1 | head -20
# find library=libmathops.so [0]; searching
# search path=./tls/haswell:./tls:./haswell:. (LD_LIBRARY_PATH)
# trying file=./libmathops.so
# → found

# Ver la resolución de símbolos:
LD_DEBUG=bindings LD_LIBRARY_PATH=. ./program 2>&1 | grep mathops
# binding file ./program [0] to ./libmathops.so [0]: normal symbol `add'
# binding file ./program [0] to ./libmathops.so [0]: normal symbol `mul'

# Ver TODO (verbose):
LD_DEBUG=all LD_LIBRARY_PATH=. ./program 2>&1 | less
```

`LD_DEBUG` es la herramienta definitiva para diagnosticar problemas de
enlace dinámico. Los valores más útiles: `libs` (búsqueda de archivos),
`bindings` (resolución de símbolos), `all` (todo).

### Diagnóstico de errores comunes

| Error | Causa | Solución |
|-------|-------|----------|
| `cannot open shared object file` | `.so` no está en ningún path | `LD_LIBRARY_PATH`, `-rpath`, o `ldconfig` |
| `symbol lookup error: undefined symbol` | `.so` incorrecta (versión) | `nm -D libfoo.so \| grep func` |
| `version GLIBC_2.XX not found` | libc del sistema demasiado vieja | Compilar en un sistema con libc más vieja |

---

## 9. `LD_PRELOAD`: interceptar funciones

`LD_PRELOAD` carga una biblioteca **antes** que todas las demás. Los
símbolos de esa biblioteca tienen prioridad — pueden **reemplazar**
funciones de cualquier otra `.so`:

```c
// override_time.c — hacer que time() siempre retorne 0:
#include <time.h>
time_t time(time_t *t) {
    if (t) *t = 0;
    return 0;
}
```

```bash
gcc -shared -fPIC override_time.c -o liboverride.so
LD_PRELOAD=./liboverride.so date
# Thu Jan  1 00:00:00 UTC 1970
```

### Usos legítimos

- **Debugging**: interceptar `malloc`/`free` para detectar leaks
- **Testing**: mock de funciones del sistema (faketime, libfaketime)
- **Herramientas**: `tsocks` (transparently SOCKSify), `libeatmydata`
  (skip fsync for speed)

### Seguridad

`LD_PRELOAD` se **ignora** en binarios setuid/setgid (por diseño del
kernel). Si funcionara, cualquier usuario podría reemplazar funciones
de programas con privilegios elevados.

---

## 10. Lazy binding vs immediate binding

### Lazy binding (por defecto)

Los símbolos se resuelven **la primera vez que se llaman**:

```
main() se ejecuta
  → llama a add()
    → PLT → GOT vacío → ld-linux resuelve → escribe GOT → ejecuta add()
  → imprime resultado
  → llama a mul()
    → PLT → GOT vacío → ld-linux resuelve → escribe GOT → ejecuta mul()
```

Si `mul()` nunca se llamara, nunca se resolvería. Un símbolo faltante
solo causa error cuando se intenta usar.

### Immediate binding (`LD_BIND_NOW` o `-z now`)

**Todos** los símbolos se resuelven al inicio, antes de ejecutar `main()`:

```bash
# Por variable de entorno:
LD_BIND_NOW=1 ./program

# Embebido en el binario:
gcc main.c -L. -lmathops -Wl,-z,now -o program
```

```bash
# Verificar:
readelf -d program | grep BIND_NOW
# (BIND_NOW)
```

Ventajas del immediate binding:
- Detecta símbolos faltantes **antes** de ejecutar código
- Más seguro: no hay sorpresas a mitad de ejecución
- Permite que la sección GOT se marque read-only (`RELRO`)

Desventaja: ligeramente más lento al inicio (resuelve todo de una vez).

### Comparación con Rust

Rust enlaza estáticamente los crates nativos (sin PLT/GOT). Para FFI con
bibliotecas C dinámicas, usa el mismo mecanismo PLT/GOT de Linux. No hay
equivalente directo de `LD_PRELOAD` en Rust — la interposición es un
concepto de enlace dinámico de C.

---

## Ejercicios

### Ejercicio 1 — Compilar contra `.so` y observar el error

Crea `libmathops.so` y compila un programa contra ella. Intenta ejecutar
sin indicar dónde está la `.so`.

```bash
cd labs/

gcc -std=c11 -Wall -Wextra -Wpedantic -fPIC -c mathops.c -o mathops.o
gcc -shared mathops.o -o libmathops.so

gcc -std=c11 -Wall -Wextra -Wpedantic main_dynlink.c -L. -lmathops -o main_dynlink
./main_dynlink
```

<details><summary>Predicción</summary>

```
./main_dynlink: error while loading shared libraries: libmathops.so:
cannot open shared object file: No such file or directory
```

- La compilación tiene éxito — el linker encontró `libmathops.so` con `-L.`.
- La ejecución falla — el dynamic linker no sabe dónde buscar la `.so`.
- `-L.` es información para el **linker en compilación**, no para el
  **dynamic linker en runtime**.
- Son dos programas diferentes con configuraciones independientes.

</details>

---

### Ejercicio 2 — Resolver con `LD_LIBRARY_PATH`

Ejecuta el programa indicando el directorio de la `.so`.

```bash
LD_LIBRARY_PATH=. ./main_dynlink
```

<details><summary>Predicción</summary>

```
Dynamic linking demo
  [libmathops] add(3, 5) called
add(3, 5) = 8
  [libmathops] mul(4, 7) called
mul(4, 7) = 28
  [libmathops] mul(2, 3) called
  [libmathops] add(10, 6) called
add(10, mul(2, 3)) = 16
```

- Los mensajes `[libmathops]` confirman que el código de la `.so` se ejecuta.
- En `add(10, mul(2, 3))`: primero se evalúa `mul(2,3)` = 6 (argumento),
  luego `add(10, 6)` = 16.
- `LD_LIBRARY_PATH=.` funciona solo para este comando. Sin ella, el
  siguiente `./main_dynlink` fallaría otra vez.

</details>

---

### Ejercicio 3 — `ldd`: inspeccionar dependencias

Compara las dependencias con y sin `LD_LIBRARY_PATH`.

```bash
echo "=== Sin LD_LIBRARY_PATH ==="
ldd main_dynlink

echo ""
echo "=== Con LD_LIBRARY_PATH ==="
LD_LIBRARY_PATH=. ldd main_dynlink
```

<details><summary>Predicción</summary>

Sin `LD_LIBRARY_PATH`:
```
linux-vdso.so.1 (0x...)
libmathops.so => not found
libc.so.6 => /lib64/libc.so.6 (0x...)
/lib64/ld-linux-x86-64.so.2 (0x...)
```

Con `LD_LIBRARY_PATH=.`:
```
linux-vdso.so.1 (0x...)
libmathops.so => ./libmathops.so (0x...)
libc.so.6 => /lib64/libc.so.6 (0x...)
/lib64/ld-linux-x86-64.so.2 (0x...)
```

- `not found` → el dynamic linker no puede encontrar `libmathops.so`.
- `=> ./libmathops.so` → encontrada en el directorio actual.
- `libc.so.6` siempre se encuentra porque está en `/lib64/` (ruta del
  sistema).
- `linux-vdso.so.1` es una interfaz virtual del kernel (siempre presente).

</details>

---

### Ejercicio 4 — RUNPATH con `$ORIGIN`

Organiza el proyecto en `app/bin/` y `app/lib/`. Compila con `-rpath`
para que no necesite `LD_LIBRARY_PATH`.

```bash
mkdir -p app/bin app/lib
cp libmathops.so app/lib/

gcc -std=c11 -Wall -Wextra -Wpedantic main_dynlink.c \
    -L./app/lib -lmathops \
    -Wl,-rpath,'$ORIGIN/../lib' \
    -o app/bin/main_runpath

./app/bin/main_runpath
```

<details><summary>Predicción</summary>

```
Dynamic linking demo
  [libmathops] add(3, 5) called
add(3, 5) = 8
  [libmathops] mul(4, 7) called
mul(4, 7) = 28
  [libmathops] mul(2, 3) called
  [libmathops] add(10, 6) called
add(10, mul(2, 3)) = 16
```

- Funciona **sin** `LD_LIBRARY_PATH`.
- `$ORIGIN` se resuelve al directorio del ejecutable (`app/bin/`).
- `$ORIGIN/../lib` → `app/lib/` → ahí está `libmathops.so`.
- El RUNPATH queda embebido en el binario — siempre funciona mientras
  la estructura relativa se mantenga.

</details>

---

### Ejercicio 5 — Portabilidad: mover el directorio completo

Mueve `app/` a otra ubicación y verifica que sigue funcionando.

```bash
mv app /tmp/app_moved
/tmp/app_moved/bin/main_runpath

# Restaurar:
mv /tmp/app_moved app
```

<details><summary>Predicción</summary>

```
Dynamic linking demo
  [libmathops] add(3, 5) called
add(3, 5) = 8
...
```

- Sigue funcionando porque `$ORIGIN` se resuelve a la **nueva** ubicación
  del ejecutable: `/tmp/app_moved/bin/`.
- `$ORIGIN/../lib` → `/tmp/app_moved/lib/` → ahí está la `.so`.
- Esta es la ventaja de `$ORIGIN` sobre paths absolutos: la estructura
  relativa se mantiene al mover.
- Si la `.so` se hubiera buscado por path absoluto (ej: `/home/user/app/lib`),
  habría fallado.

</details>

---

### Ejercicio 6 — `readelf -d`: verificar RUNPATH embebido

Compara los binarios con y sin RUNPATH.

```bash
echo "=== Sin RUNPATH ==="
readelf -d main_dynlink | grep -iE "runpath|rpath" || echo "(ninguno)"

echo ""
echo "=== Con RUNPATH ==="
readelf -d app/bin/main_runpath | grep -iE "runpath|rpath"
```

<details><summary>Predicción</summary>

Sin RUNPATH:
```
(ninguno)
```

Con RUNPATH:
```
 0x... (RUNPATH)   Library runpath: [$ORIGIN/../lib]
```

- `main_dynlink` no tiene RUNPATH — depende de `LD_LIBRARY_PATH` o
  `ldconfig` para encontrar la `.so`.
- `main_runpath` tiene `$ORIGIN/../lib` embebido — es autocontenido.
- `readelf -d` muestra la sección `.dynamic` del ELF, que incluye
  NEEDED (dependencias) y RUNPATH (rutas de búsqueda).

</details>

---

### Ejercicio 7 — `LD_DEBUG=libs`: trazar la búsqueda

Observa paso a paso cómo el dynamic linker busca `libmathops.so`.

```bash
LD_DEBUG=libs LD_LIBRARY_PATH=. ./main_dynlink 2>&1 | grep -E "find|trying|found" | head -15
```

<details><summary>Predicción</summary>

```
<pid>: find library=libmathops.so [0]; searching
<pid>:  trying file=./tls/haswell/x86_64/libmathops.so
<pid>:  trying file=./tls/haswell/libmathops.so
<pid>:  trying file=./tls/x86_64/libmathops.so
<pid>:  trying file=./tls/libmathops.so
<pid>:  trying file=./haswell/x86_64/libmathops.so
<pid>:  trying file=./haswell/libmathops.so
<pid>:  trying file=./x86_64/libmathops.so
<pid>:  trying file=./libmathops.so
```

- El loader prueba subdirectorios especializados primero (`tls`, `haswell`,
  `x86_64`). Estos permiten tener versiones optimizadas para hardware
  específico.
- Finalmente prueba `./libmathops.so` y la encuentra.
- Si usas `LD_DEBUG=libs` **sin** `LD_LIBRARY_PATH`, verás que busca en
  el cache y en `/lib64/`, `/usr/lib64/` — y falla.

</details>

---

### Ejercicio 8 — `LD_DEBUG=bindings`: resolución de símbolos

Observa cuándo se resuelve cada símbolo con lazy binding.

```bash
LD_DEBUG=bindings LD_LIBRARY_PATH=. ./main_dynlink 2>&1 | grep -E "(binding.*mathops|Dynamic|add\(|mul\()"
```

<details><summary>Predicción</summary>

Con lazy binding (por defecto), los bindings aparecen intercalados con
la salida del programa:
```
Dynamic linking demo
<pid>: binding ... normal symbol `add'
  [libmathops] add(3, 5) called
add(3, 5) = 8
<pid>: binding ... normal symbol `mul'
  [libmathops] mul(4, 7) called
mul(4, 7) = 28
...
```

- `add` se resuelve justo antes de su primera llamada.
- `mul` se resuelve justo antes de su primera llamada.
- Esto es lazy binding: cada símbolo se resuelve bajo demanda, no al inicio.

</details>

---

### Ejercicio 9 — Lazy vs immediate binding

Compara el orden de resolución con `LD_BIND_NOW=1`.

```bash
echo "=== Lazy (default) ==="
LD_DEBUG=bindings LD_LIBRARY_PATH=. ./main_dynlink 2>&1 | grep -E "(binding.*mathops|Dynamic)" | head -6

echo ""
echo "=== Immediate (BIND_NOW) ==="
LD_BIND_NOW=1 LD_DEBUG=bindings LD_LIBRARY_PATH=. ./main_dynlink 2>&1 | grep -E "(binding.*mathops|Dynamic)" | head -6
```

<details><summary>Predicción</summary>

Lazy:
```
Dynamic linking demo
<pid>: binding ... normal symbol `add'
<pid>: binding ... normal symbol `mul'
```
(bindings intercalados con la ejecución del programa)

Immediate:
```
<pid>: binding ... normal symbol `add'
<pid>: binding ... normal symbol `mul'
Dynamic linking demo
```
(todos los bindings **antes** de "Dynamic linking demo")

- Con `LD_BIND_NOW=1`, todos los símbolos se resuelven antes de ejecutar
  `main()`.
- Si faltara un símbolo, con lazy binding el error ocurriría a mitad de
  ejecución (cuando se llame la función). Con immediate binding, el error
  ocurre inmediatamente al inicio.
- Immediate binding es más seguro pero ligeramente más lento al arrancar.

</details>

---

### Ejercicio 10 — `objdump -d`: ver PLT stubs

Inspecciona los stubs PLT del ejecutable enlazado dinámicamente.

```bash
objdump -d main_dynlink | grep -A3 "@plt>:" | head -20
```

<details><summary>Predicción</summary>

```
<addr> <add@plt>:
  <addr>: ff 25 XX XX XX XX    jmp    *0xXXXX(%rip)   # GOT entry
  <addr>: 68 00 00 00 00       push   $0x0             # PLT index
  <addr>: e9 XX XX XX XX       jmp    <resolver>

<addr> <mul@plt>:
  <addr>: ff 25 XX XX XX XX    jmp    *0xXXXX(%rip)   # GOT entry
  <addr>: 68 01 00 00 00       push   $0x1             # PLT index
  <addr>: e9 XX XX XX XX       jmp    <resolver>

<addr> <printf@plt>:
  ...
```

Cada stub PLT tiene la misma estructura:
1. `jmp *GOT[n]` — saltar a la dirección en la GOT
2. Si la GOT no está llena → `push` el índice y `jmp` al resolver
3. El resolver (`ld-linux.so`) busca el símbolo, llena la GOT, y salta

- En la primera llamada: pasa por el resolver (lento).
- En llamadas siguientes: `jmp *GOT[n]` salta directamente (rápido).
- `printf` también tiene un stub PLT — es de `libc.so`, no del programa.

</details>

---

### Limpieza

```bash
rm -f mathops.o libmathops.so main_dynlink
rm -rf app/ libdir/
```
