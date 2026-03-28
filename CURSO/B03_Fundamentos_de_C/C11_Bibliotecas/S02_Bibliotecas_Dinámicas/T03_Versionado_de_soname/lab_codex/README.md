# Lab — Versionado de soname

## Objetivo

Construir bibliotecas compartidas versionadas, inspeccionar `SONAME` y `NEEDED`
y demostrar por que Linux puede mantener varias versiones mayores del mismo
`.so` sin romper clientes antiguos.

## Prerequisitos

- GCC instalado
- `readelf`, `objdump`, `ldd` y `ln` disponibles
- Estar en el directorio `lab_codex/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `calc_v1.h` | Header de la ABI v1 |
| `calc_v1.c` | Implementacion v1.0.0 |
| `calc_v1_1.c` | Implementacion v1.1.0 compatible |
| `calc_v2.h` | Header de la ABI v2 |
| `calc_v2.c` | Implementacion v2.0.0 incompatible |
| `program_v1.c` | Cliente compilado contra la ABI v1 |
| `program_v2.c` | Cliente compilado contra la ABI v2 |
| `Makefile` | Build local de librerias y clientes |

---

## Parte 1 — Crear v1.0.0 con su soname

**Objetivo**: Generar una biblioteca versionada y sus symlinks.

### Paso 1.1 — Examinar las fuentes de v1

```bash
cat calc_v1.h
cat calc_v1.c
cat program_v1.c
```

Observa:

- `program_v1.c` usa `add`, `mul` y `calc_build`
- `calc_build` existe para que puedas comprobar que una version nueva realmente
  se cargo sin recompilar el programa

### Paso 1.2 — Construir la biblioteca v1.0.0

```bash
mkdir -p runtime/lib runtime/bin
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC -c calc_v1.c -o calc_v1.o
gcc -shared -Wl,-soname,libcalc.so.1 -o runtime/lib/libcalc.so.1.0.0 calc_v1.o
ln -sf libcalc.so.1.0.0 runtime/lib/libcalc.so.1
ln -sf libcalc.so.1 runtime/lib/libcalc.so
```

### Paso 1.3 — Verificar el trío de nombres

```bash
ls -l runtime/lib/libcalc.so*
readelf -d runtime/lib/libcalc.so.1.0.0 | grep SONAME
```

Salida esperada aproximada:

```text
libcalc.so -> libcalc.so.1
libcalc.so.1 -> libcalc.so.1.0.0
libcalc.so.1.0.0

(SONAME) Library soname: [libcalc.so.1]
```

---

## Parte 2 — Compilar y ejecutar el cliente v1

**Objetivo**: Ver que el programa busca el `soname`, no el archivo fisico.

### Paso 2.1 — Compilar `program_v1`

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 \
    program_v1.c -I. -Lruntime/lib -lcalc \
    -Wl,-rpath,'$ORIGIN/../lib' \
    -o runtime/bin/program_v1
```

### Paso 2.2 — Ver que el ELF pide `libcalc.so.1`

```bash
readelf -d runtime/bin/program_v1 | grep NEEDED
```

Salida esperada aproximada:

```text
(NEEDED) Shared library: [libcalc.so.1]
```

### Paso 2.3 — Ejecutar el cliente

```bash
./runtime/bin/program_v1
```

Salida esperada:

```text
build: calc v1.0.0
add(4, 5) = 9
mul(4, 5) = 20
```

---

## Parte 3 — Actualizacion compatible: 1.0.0 -> 1.1.0

**Objetivo**: Reemplazar la implementacion menor sin recompilar el cliente.

### Paso 3.1 — Examinar la implementacion compatible nueva

```bash
cat calc_v1_1.c
```

Observa:

- Mantiene el mismo `SONAME` esperado: `libcalc.so.1`
- Agrega una funcion nueva (`sub`) sin romper la ABI existente
- Cambia el texto de `calc_build` para que puedas notar la actualizacion

### Paso 3.2 — Construir la version 1.1.0 y actualizar el symlink

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC -c calc_v1_1.c -o calc_v1_1.o
gcc -shared -Wl,-soname,libcalc.so.1 -o runtime/lib/libcalc.so.1.1.0 calc_v1_1.o
ln -sf libcalc.so.1.1.0 runtime/lib/libcalc.so.1
ln -sf libcalc.so.1 runtime/lib/libcalc.so
```

### Paso 3.3 — Ejecutar el MISMO binario sin recompilar

```bash
./runtime/bin/program_v1
```

Salida esperada:

```text
build: calc v1.1.0
add(4, 5) = 9
mul(4, 5) = 20
```

La linea `build:` cambio, pero el ejecutable es el mismo. Eso demuestra que
`program_v1` sigue cargando cualquier biblioteca compatible con `libcalc.so.1`.

### Paso 3.4 — Confirmar la version resuelta

```bash
ldd runtime/bin/program_v1 | grep libcalc
```

Debe apuntar a `runtime/lib/libcalc.so.1.1.0`.

---

## Parte 4 — Coexistencia de majors

**Objetivo**: Mantener v1 y v2 al mismo tiempo para clientes distintos.

### Paso 4.1 — Examinar la ABI v2

```bash
cat calc_v2.h
cat calc_v2.c
cat program_v2.c
```

La incompatibilidad esta en `add`: ahora recibe tres parametros. Eso amerita
un major nuevo y por tanto un `soname` distinto.

### Paso 4.2 — Construir v2.0.0

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC -c calc_v2.c -o calc_v2.o
gcc -shared -Wl,-soname,libcalc.so.2 -o runtime/lib/libcalc.so.2.0.0 calc_v2.o
ln -sf libcalc.so.2.0.0 runtime/lib/libcalc.so.2
ln -sf libcalc.so.2 runtime/lib/libcalc.so
```

Aqui el `linker name` (`libcalc.so`) pasa a apuntar a la version nueva para
nuevas compilaciones, pero `libcalc.so.1` sigue existiendo para clientes viejos.

### Paso 4.3 — Compilar el cliente v2

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 \
    program_v2.c -I. -Lruntime/lib -lcalc \
    -Wl,-rpath,'$ORIGIN/../lib' \
    -o runtime/bin/program_v2
```

### Paso 4.4 — Ejecutar ambos clientes

```bash
./runtime/bin/program_v1
./runtime/bin/program_v2
```

Salida esperada:

```text
build: calc v1.1.0
add(4, 5) = 9
mul(4, 5) = 20

build: calc v2.0.0
add(4, 5, 6) = 15
```

### Paso 4.5 — Verificar las dependencias exactas

```bash
readelf -d runtime/bin/program_v1 | grep NEEDED
readelf -d runtime/bin/program_v2 | grep NEEDED
```

Debes ver:

- `program_v1` necesita `libcalc.so.1`
- `program_v2` necesita `libcalc.so.2`

Eso es lo que hace posible la coexistencia segura.

---

## Parte 5 — Automatizar con `make`

**Objetivo**: Repetir el flujo con targets dedicados.

### Paso 5.1 — Examinar el Makefile

```bash
cat Makefile
```

### Paso 5.2 — Build rapido

```bash
make clean
make v1_0
make program_v1
./runtime/bin/program_v1

make v1_1
./runtime/bin/program_v1

make v2_0
make program_v2
./runtime/bin/program_v2
```

### Paso 5.3 — Limpiar

```bash
make clean
```

---

## Limpieza final

```bash
make clean
rm -rf runtime
rm -f *.o
```

---

## Conceptos reforzados

1. `real name`, `soname` y `linker name` cumplen roles distintos.
2. El ejecutable guarda en `NEEDED` el `soname`, no el archivo fisico exacto.
3. Una actualizacion compatible cambia el archivo real, pero mantiene el mismo
   `soname`.
4. Un cambio de ABI requiere `major` nuevo y por tanto un `soname` nuevo.
5. Dos versiones mayores pueden convivir sin conflicto si tienen `soname`
   diferentes.
6. `readelf -d` y `ldd` permiten comprobar exactamente que version se resolvio.

