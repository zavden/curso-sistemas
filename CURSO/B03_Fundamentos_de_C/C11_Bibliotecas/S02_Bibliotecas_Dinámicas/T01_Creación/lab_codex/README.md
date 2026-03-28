# Lab — Creacion de bibliotecas dinamicas

## Objetivo

Construir una biblioteca compartida real, inspeccionar sus simbolos exportados
y comparar una build con visibilidad por defecto contra otra que oculta helpers
internos.

## Prerequisitos

- GCC instalado
- `nm`, `ldd`, `file` y `readelf` disponibles
- Estar en el directorio `lab_codex/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `stats.h` | API publica y macro `STATS_API` |
| `stats.c` | Implementacion de la biblioteca |
| `main.c` | Programa cliente |
| `Makefile` | Builds por defecto, ocultas y limpieza |

---

## Parte 1 — Construir una biblioteca compartida

**Objetivo**: Generar un `.so` y comprobar que es un objeto compartido real.

### Paso 1.1 — Examinar la API publica

```bash
cat stats.h
cat stats.c
cat main.c
```

Observa:

- `stats_sum`, `stats_mean` y `stats_print_summary` forman la API publica.
- `accumulate_internal` es un helper que no deberia exponerse idealmente.
- El programa cliente solo conoce el header, no la implementacion.

### Paso 1.2 — Compilar el objeto PIC

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC -DBUILDING_STATS -c stats.c -o stats_default.o
```

`-fPIC` genera codigo independiente de posicion, apto para cargar el `.so` en
distintas direcciones de memoria.

### Paso 1.3 — Crear la biblioteca compartida

```bash
gcc -shared -o libstats_default.so stats_default.o
```

### Paso 1.4 — Inspeccionar el archivo creado

```bash
file libstats_default.so
ldd libstats_default.so
nm -D libstats_default.so
```

Salida esperada aproximada:

```text
libstats_default.so: ELF 64-bit LSB shared object, x86-64, ...
...
T stats_sum
T stats_mean
T stats_print_summary
T accumulate_internal
```

Observa que `accumulate_internal` aparece exportado. Ese es justamente el
problema de la visibilidad por defecto: todo simbolo global termina visible.

---

## Parte 2 — Enlazar y ejecutar el cliente

**Objetivo**: Ver la diferencia entre link time y runtime.

### Paso 2.1 — Compilar el ejecutable cliente

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 main.c -L. -lstats_default -o demo_default
```

### Paso 2.2 — Verificar dependencias del binario

```bash
ldd demo_default
```

Salida esperada aproximada:

```text
libstats_default.so => not found
libc.so.6 => /lib/.../libc.so.6
...
```

Interpretacion:

- El linker encontro `libstats_default.so` durante la compilacion.
- El loader aun no sabe donde encontrarlo en runtime porque el directorio
  actual no es un path estandar.

### Paso 2.3 — Intentar ejecutar sin path extra

```bash
./demo_default
```

Salida esperada aproximada:

```text
./demo_default: error while loading shared libraries: libstats_default.so: cannot open shared object file: No such file or directory
```

### Paso 2.4 — Ejecutar con `LD_LIBRARY_PATH`

```bash
LD_LIBRARY_PATH=. ./demo_default
```

Salida esperada:

```text
sum: 45
mean: 9.00
summary: count=5 sum=45 mean=9.00
```

### Paso 2.5 — Confirmar que el ejecutable NO contiene el codigo

```bash
nm demo_default | grep stats_
```

No deberias ver definiciones `T stats_*` dentro del ejecutable. El codigo vive
en el `.so`, no en el binario cliente.

---

## Parte 3 — Ocultar simbolos internos

**Objetivo**: Compilar una version del `.so` que exporta solo la API publica.

### Paso 3.1 — Compilar con visibilidad oculta por defecto

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC -fvisibility=hidden -DBUILDING_STATS -c stats.c -o stats_hidden.o
gcc -shared -o libstats_hidden.so stats_hidden.o
```

### Paso 3.2 — Comparar exports dinamicos

```bash
echo '== default =='
nm -D libstats_default.so

echo '== hidden =='
nm -D libstats_hidden.so
```

La diferencia clave debe ser:

- `libstats_default.so` exporta `accumulate_internal`
- `libstats_hidden.so` solo exporta la API marcada con `STATS_API`

### Paso 3.3 — Compilar y ejecutar el cliente contra la version oculta

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 main.c -L. -lstats_hidden -o demo_hidden
LD_LIBRARY_PATH=. ./demo_hidden
```

La salida funcional debe ser la misma que con `demo_default`.

### Paso 3.4 — Interpretar el resultado

Ocultar helpers internos sirve para:

- reducir la superficie publica del `.so`
- evitar colisiones de nombres
- dejar libertad para cambiar detalles internos sin romper consumidores

---

## Parte 4 — Automatizar con `make`

**Objetivo**: Repetir el flujo sin escribir todos los comandos a mano.

### Paso 4.1 — Examinar el Makefile

```bash
cat Makefile
```

### Paso 4.2 — Construir todo

```bash
make clean
make
```

El Makefile genera:

- `libstats_default.so`
- `libstats_hidden.so`
- `demo_default`
- `demo_hidden`

### Paso 4.3 — Probar los dos ejecutables

```bash
LD_LIBRARY_PATH=. ./demo_default
LD_LIBRARY_PATH=. ./demo_hidden
```

### Paso 4.4 — Limpiar

```bash
make clean
```

---

## Limpieza final

```bash
make clean
rm -f stats_default.o stats_hidden.o main.o
rm -f libstats_default.so libstats_hidden.so demo_default demo_hidden
```

---

## Conceptos reforzados

1. Un `.so` se construye a partir de objetos PIC usando `-shared`.
2. El ejecutable dinamico guarda referencias al `.so`, no una copia del codigo.
3. Un `.so` local necesita `LD_LIBRARY_PATH`, `RUNPATH` o instalacion real para
   ser encontrado en runtime.
4. `nm -D` muestra solo los simbolos dinamicos exportados.
5. La visibilidad por defecto expone helpers internos.
6. `-fvisibility=hidden` combinado con atributos de exportacion reduce la API
   publica al minimo necesario.

