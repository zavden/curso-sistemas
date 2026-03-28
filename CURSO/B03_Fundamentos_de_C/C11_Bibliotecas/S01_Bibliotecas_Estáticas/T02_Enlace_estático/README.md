# T02 — Enlace estático

## Cómo funciona el linker

El linker (`ld`, invocado por `gcc`) resuelve **símbolos** —
conecta las llamadas a funciones con sus definiciones:

```
main.o                  libvec3.a (contiene vec3.o)
┌──────────────┐        ┌────────────────────┐
│ main()       │        │ vec3_create()  [T]  │
│  call vec3_create [U] │ vec3_add()     [T]  │
│  call vec3_print  [U] │ vec3_print()   [T]  │
│  call printf      [U] │  call printf   [U]  │
└──────────────┘        └────────────────────┘
       │                         │
       └────── LINKER ───────────┘
               │
         ┌─────┴──────┐
         │  programa   │
         │  main()     │
         │  vec3_create│  ← copiado de vec3.o
         │  vec3_add   │  ← copiado de vec3.o
         │  vec3_print │  ← copiado de vec3.o
         │  call printf│  ← resuelto por libc
         └─────────────┘
```

```bash
# El proceso completo:
gcc main.c -L./lib -lvec3 -lm -o program

# Lo que gcc hace internamente:
# 1. gcc compila main.c → main.o (temporal)
# 2. gcc invoca el linker:
#    ld main.o -L./lib -lvec3 -lm -lc -o program
#    (simplificado — en realidad agrega crt0.o, crti.o, etc.)
```

## -l y -L: buscar bibliotecas

```bash
# -l<name> — enlazar con lib<name>.a (o lib<name>.so):
gcc main.c -lvec3 -lm -o program

# El linker busca:
# 1. libvec3.so (dinámica, preferida por defecto)
# 2. libvec3.a (estática, si no encuentra .so)
# en los directorios de búsqueda.

# -L<dir> — agregar un directorio de búsqueda:
gcc main.c -L./lib -L/opt/mylibs/lib -lvec3 -o program

# Directorios de búsqueda por defecto:
# /usr/lib/
# /usr/lib/x86_64-linux-gnu/   (Debian/Ubuntu)
# /usr/local/lib/
# Directorios en /etc/ld.so.conf

# Ver los directorios de búsqueda del linker:
ld --verbose | grep SEARCH_DIR
```

```bash
# Formas equivalentes de enlazar:

# Con -l (busca automáticamente):
gcc main.c -L./lib -lvec3 -o program

# Con ruta completa (no necesita -L):
gcc main.c ./lib/libvec3.a -o program

# La diferencia: -l busca .so primero.
# Con ruta completa, se usa exactamente ese archivo.
```

## El orden de los argumentos importa

Este es el concepto más importante del enlace estático.
El linker procesa los archivos **de izquierda a derecha**:

```bash
# El linker mantiene dos listas:
# - Símbolos DEFINIDOS (ya resueltos)
# - Símbolos UNDEFINED (pendientes de resolver)

# Proceso:
# 1. Lee main.o → agrega main a definidos, vec3_create a undefined
# 2. Lee libvec3.a → busca vec3_create → lo encuentra en vec3.o
#    → incluye vec3.o → agrega vec3_create a definidos, printf a undefined
# 3. Lee libm.a → busca sqrt → lo encuentra → incluye
# 4. Lee libc.a → busca printf → lo encuentra → incluye
# 5. Si quedan undefined → error "undefined reference"
```

```bash
# CORRECTO — dependencias después de quien las usa:
gcc main.o -lvec3 -lm -o program
# main.o necesita vec3 → vec3 viene después
# vec3 necesita sqrt (libm) → -lm viene después

# INCORRECTO — biblioteca antes de quien la necesita:
gcc -lvec3 main.o -lm -o program
# El linker lee libvec3.a primero.
# En ese momento no hay undefined → no incluye nada de vec3.
# Luego lee main.o → vec3_create es undefined → ERROR
# undefined reference to `vec3_create'
```

```bash
# REGLA: poner las bibliotecas DESPUÉS de los .o/.c que las usan.
# Las dependencias van de izquierda a derecha:
# main.o → usa libA → usa libB → usa libC
gcc main.o -lA -lB -lC -o program

# Si libA y libB se usan mutuamente (dependencia circular):
gcc main.o -lA -lB -lA -o program
# Listar la misma biblioteca dos veces resuelve la circularidad.

# O usar group (GNU ld):
gcc main.o -Wl,--start-group -lA -lB -Wl,--end-group -o program
# El linker itera el grupo hasta resolver todo.
```

### Demostración del problema de orden

```c
// --- greet.h / greet.c ---
// libgreet.a contiene greet() que llama a format_name()

// --- format.h / format.c ---
// libformat.a contiene format_name()

// --- main.c ---
// main() llama a greet()
```

```bash
# CORRECTO:
gcc main.o -lgreet -lformat -o program
# main.o → necesita greet → -lgreet lo provee
# greet → necesita format_name → -lformat lo provee

# INCORRECTO:
gcc main.o -lformat -lgreet -o program
# main.o → necesita greet → -lformat no lo tiene
# -lgreet lo provee → pero necesita format_name
# → format_name ya pasó (libformat fue procesada antes) → ERROR

# TAMBIÉN INCORRECTO:
gcc -lgreet -lformat main.o -o program
# Las bibliotecas se leen antes de main.o
# No hay undefined → no se incluye nada → ERROR
```

## Forzar enlace estático

```bash
# Por defecto, gcc prefiere .so sobre .a.
# Para forzar estático:

# 1. -static — TODO estático (incluyendo libc):
gcc main.c -static -lvec3 -lm -o program
# Binario ~1 MB+ (incluye toda libc).
# No necesita NADA instalado en el sistema para ejecutar.

# 2. Selectivo con -Wl,-Bstatic / -Wl,-Bdynamic:
gcc main.c -Wl,-Bstatic -lvec3 -Wl,-Bdynamic -lm -o program
# libvec3: estática (copiada en el binario)
# libm: dinámica (necesita libm.so en runtime)

# 3. Ruta completa al .a:
gcc main.c ./lib/libvec3.a -lm -o program
# Fuerza usar el .a específico.
```

```bash
# Verificar qué bibliotecas usa un binario:
ldd program
# linux-vdso.so.1 => ...
# libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6
# libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6
# Si libvec3 no aparece → fue enlazada estáticamente.

# Con -static:
ldd program
# not a dynamic executable
# No depende de NADA externo.

# Ver los símbolos del binario final:
nm program | grep vec3
# 0000000000401234 T vec3_create   ← copiado dentro del binario
```

## Errores comunes y diagnóstico

```bash
# 1. "undefined reference to `func_name'"
# Causa: el linker no encontró la definición.
# Diagnóstico:
nm -A lib*.a | grep func_name
# Si aparece con T → la biblioteca SÍ lo tiene → problema de orden.
# Si no aparece → falta compilar o agregar al .a.

# 2. "cannot find -lname"
# Causa: no existe libname.a ni libname.so en los paths de búsqueda.
# Diagnóstico:
find /usr/lib -name "libname*"
# Si no existe → instalar la dependencia o agregar -L.

# 3. "multiple definition of `func_name'"
# Causa: la función está definida en más de un .o.
# Diagnóstico:
nm -A *.o | grep "T func_name"
# Si aparece en dos .o → mover la definición a uno solo,
# o marcarla como static si es interna.
```

```bash
# Ver exactamente qué hace el linker:
gcc -v main.c -L./lib -lvec3 -lm -o program 2>&1
# Muestra el comando completo del linker con todos los paths.

# Ver qué .o extrae el linker del .a:
gcc -Wl,--trace main.c -L./lib -lvec3 -lm -o program
# Imprime cada archivo que el linker procesa.

# Verbose del linker:
gcc -Wl,--verbose main.c -L./lib -lvec3 -o program
```

## pkg-config

```bash
# pkg-config automatiza -I, -L, -l para bibliotecas instaladas:

# Ver flags de compilación:
pkg-config --cflags libpng
# -I/usr/include/libpng16

# Ver flags de enlace:
pkg-config --libs libpng
# -lpng16 -lz

# Usar directamente en gcc:
gcc main.c $(pkg-config --cflags --libs libpng) -o program

# Listar todas las bibliotecas disponibles:
pkg-config --list-all

# Los archivos .pc están en /usr/lib/pkgconfig/ o similar.
# Se pueden crear .pc propios para proyectos internos.
```

---

## Ejercicios

### Ejercicio 1 — Demostrar el orden importa

```bash
# 1. Crear libA.a con func_a() que llama a func_b()
# 2. Crear libB.a con func_b()
# 3. Crear main.c que llama a func_a()
# 4. Intentar compilar en estos órdenes y anotar cuáles fallan:
#    a) gcc main.c -lA -lB
#    b) gcc main.c -lB -lA
#    c) gcc -lA -lB main.c
#    d) gcc main.c -lA
# Explicar por qué cada uno funciona o falla.
```

### Ejercicio 2 — Enlace mixto

```bash
# 1. Crear libcalc.a con funciones add, sub, mul, div_safe
# 2. Compilar main.c que use todas las funciones
# 3. Verificar con ldd que el binario usa libc dinámica
# 4. Recompilar con -static y comparar tamaños (ls -l)
# 5. Verificar con ldd que el estático no tiene dependencias
# 6. Ejecutar ambos y comparar comportamiento
```

### Ejercicio 3 — Diagnóstico de errores

```bash
# Crear una biblioteca libdata.a con parse_csv() y write_json().
# Introducir intencionalmente estos errores y diagnosticar cada uno:
# 1. Compilar con -ldata antes de main.c
# 2. No poner -L para el directorio de la biblioteca
# 3. Definir parse_csv en dos .o diferentes
# 4. Olvidar agregar un .o al .a (función undefined)
# Para cada error: anotar el mensaje, explicar la causa, y corregir.
```
