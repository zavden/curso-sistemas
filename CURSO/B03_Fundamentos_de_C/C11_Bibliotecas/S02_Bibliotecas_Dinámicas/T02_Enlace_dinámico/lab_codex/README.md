# Lab — Enlace dinamico

## Objetivo

Entender como un ejecutable encuentra un `.so` en runtime, por que compilar con
`-L` no resuelve automaticamente la ejecucion, y como `RUNPATH` permite
distribuir una app autocontenida en `bin/` y `lib/`.

## Prerequisitos

- GCC instalado
- `ldd`, `readelf`, `objdump` y `LD_DEBUG` disponibles
- Estar en el directorio `lab_codex/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `app/lib/calc.h` | Header publico de la biblioteca |
| `app/lib/calc.c` | Implementacion de `libcalc.so` |
| `app/bin/main.c` | Programa cliente |
| `Makefile` | Build de biblioteca y dos variantes del cliente |

---

## Parte 1 — Construir la app local

**Objetivo**: Generar una biblioteca en `app/lib` y un binario en `app/bin`.

### Paso 1.1 — Examinar el layout

```bash
find app -maxdepth 2 -type f | sort
cat app/lib/calc.h
cat app/lib/calc.c
cat app/bin/main.c
```

Este layout imita una distribucion simple:

- `app/bin/` para ejecutables
- `app/lib/` para bibliotecas compartidas

### Paso 1.2 — Compilar la biblioteca

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC -shared app/lib/calc.c -o app/lib/libcalc.so
```

### Paso 1.3 — Compilar el ejecutable sin `RUNPATH`

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -Iapp/lib app/bin/main.c -Lapp/lib -lcalc -o app/bin/demo_plain
```

### Paso 1.4 — Comprobar la dependencia

```bash
ldd app/bin/demo_plain
```

Salida esperada aproximada:

```text
libcalc.so => not found
libc.so.6 => /lib/.../libc.so.6
...
```

Compilar con `-Lapp/lib` solo ayuda al linker en build time. El loader aun no
sabe donde esta `libcalc.so`.

---

## Parte 2 — Resolver con `LD_LIBRARY_PATH`

**Objetivo**: Hacer funcionar `demo_plain` usando una variable de entorno.

### Paso 2.1 — Ejecutar sin path adicional

```bash
./app/bin/demo_plain
```

Salida esperada aproximada:

```text
./app/bin/demo_plain: error while loading shared libraries: libcalc.so: cannot open shared object file: No such file or directory
```

### Paso 2.2 — Ejecutar con `LD_LIBRARY_PATH`

```bash
LD_LIBRARY_PATH=./app/lib ./app/bin/demo_plain
```

Salida esperada:

```text
add: 13
mul: 42
signature: calc-lib: add=13 mul=42
```

### Paso 2.3 — Ver la carga real con `LD_DEBUG`

```bash
LD_DEBUG=libs LD_LIBRARY_PATH=./app/lib ./app/bin/demo_plain 2>&1 | head -20
```

Debes ver mensajes donde el loader explora rutas y finalmente encuentra
`./app/lib/libcalc.so`.

---

## Parte 3 — Resolver con `RUNPATH`

**Objetivo**: Hacer que el binario encuentre su `.so` sin variables de entorno.

### Paso 3.1 — Compilar la variante con `RUNPATH`

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 \
    -Iapp/lib app/bin/main.c -Lapp/lib -lcalc \
    -Wl,-rpath,'$ORIGIN/../lib' \
    -o app/bin/demo_runpath
```

`$ORIGIN` se expande al directorio del ejecutable. Como `demo_runpath` vive en
`app/bin`, la ruta relativa `../lib` apunta a `app/lib`.

### Paso 3.2 — Inspeccionar el ELF

```bash
readelf -d app/bin/demo_runpath | grep -E 'RUNPATH|RPATH|NEEDED'
```

Salida esperada aproximada:

```text
0x000000000000001d (RUNPATH)            Library runpath: [$ORIGIN/../lib]
0x0000000000000001 (NEEDED)             Shared library: [libcalc.so]
```

### Paso 3.3 — Ejecutar sin `LD_LIBRARY_PATH`

```bash
./app/bin/demo_runpath
```

Debe funcionar directamente:

```text
add: 13
mul: 42
signature: calc-lib: add=13 mul=42
```

### Paso 3.4 — Confirmar con `ldd`

```bash
ldd app/bin/demo_runpath
```

Ahora `libcalc.so` debe aparecer resuelta a una ruta dentro de `app/lib`.

---

## Parte 4 — Comparar los dos enfoques

**Objetivo**: Ver claramente la diferencia entre una app fragil y una app
autocontenida.

### Paso 4.1 — Resumen lado a lado

```bash
printf '\n== plain ==\n'
ldd app/bin/demo_plain

printf '\n== runpath ==\n'
ldd app/bin/demo_runpath
```

### Paso 4.2 — Interpretar

- `demo_plain` depende del entorno del usuario
- `demo_runpath` lleva la ruta necesaria embebida en el ELF
- `LD_LIBRARY_PATH` es util para desarrollo y pruebas
- `RUNPATH` suele ser mejor para apps distribuidas en carpetas relocatables

### Paso 4.3 — Repetir con `make`

```bash
make clean
make
```

### Paso 4.4 — Probar ambos binarios

```bash
LD_LIBRARY_PATH=./app/lib ./app/bin/demo_plain
./app/bin/demo_runpath
```

---

## Limpieza final

```bash
make clean
rm -f app/lib/libcalc.so app/bin/demo_plain app/bin/demo_runpath
```

---

## Conceptos reforzados

1. `-L` resuelve bibliotecas en build time, no en runtime.
2. `LD_LIBRARY_PATH` modifica el path de busqueda del loader para un proceso.
3. `RUNPATH` embebe una ruta en el ELF y hace el binario mas autocontenido.
4. `ldd` muestra dependencias y si una biblioteca esta resuelta o no.
5. `readelf -d` permite ver `NEEDED`, `RUNPATH` y otras entradas dinamicas.
6. `LD_DEBUG=libs` deja visible el proceso real de busqueda del loader.

