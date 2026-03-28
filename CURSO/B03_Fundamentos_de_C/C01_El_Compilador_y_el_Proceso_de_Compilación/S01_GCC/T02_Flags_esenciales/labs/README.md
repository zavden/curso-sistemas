# Lab -- Flags esenciales

## Objetivo

Experimentar con los flags mas importantes de GCC para entender que detecta
cada nivel de warnings, como funciona la seleccion de estandar, que efecto
tienen los flags de debug y optimizacion, y que combinaciones usar en
desarrollo vs produccion.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `warnings.c` | Programa con problemas deliberados: variable no usada, parametro no usado, variable sin inicializar, formato de printf incorrecto, comparacion signed/unsigned, inicializador incompleto de struct |
| `standard.c` | Programa con extensiones GNU: zero-length array, statement expression, binary literal |
| `optimize_cmp.c` | Loop que suma de 1 a N para comparar assembly con diferentes niveles de optimizacion |

---

## Parte 1 -- Niveles de warnings

**Objetivo**: Observar como cada nivel de warnings (-Wall, -Wextra, -Wpedantic)
detecta problemas distintos, y como -Werror y -Wno-xxx controlan el
comportamiento.

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat warnings.c
```

Observa los problemas deliberados:

- `unused_param` en la firma de `process()` -- nunca se usa
- `y` se declara sin inicializar y se usa en el return
- `unused_var` se declara pero nunca se usa
- `printf("%s\n", x)` usa `%s` para un `int`
- `s < u` compara signed con unsigned
- `struct Config cfg = { 640, 480 }` omite el campo `depth`

### Paso 1.2 -- Compilacion sin flags de warning

```bash
gcc warnings.c -o warnings_none 2>&1
```

Antes de ejecutar, predice: cuantos warnings aparecen al compilar sin
ningun flag de warning explcito?

### Paso 1.3 -- Verificar la prediccion

GCC sin flags no es completamente silencioso: algunos warnings criticos se
emiten por defecto (como formato de printf incorrecto). Anota cuantos
warnings aparecen.

### Paso 1.4 -- Compilacion con -Wall

```bash
gcc -Wall warnings.c -o warnings_wall 2>&1
```

Antes de mirar la salida, predice: cuantos warnings nuevos agrega -Wall
respecto a la compilacion sin flags?

### Paso 1.5 -- Verificar -Wall

Compara con el paso anterior. -Wall agrega warnings como:

- Variable no usada (`unused_var`)
- Variable usada sin inicializar (`y`)
- Formato de printf incorrecto (`%s` con un `int`)

Cuenta el total de warnings.

### Paso 1.6 -- Compilacion con -Wall -Wextra

```bash
gcc -Wall -Wextra warnings.c -o warnings_wextra 2>&1
```

Predice: que warnings nuevos aparecen con -Wextra que -Wall no detectaba?

### Paso 1.7 -- Verificar -Wextra

-Wextra agrega warnings adicionales como:

- Parametro no usado (`unused_param`)
- Comparacion signed/unsigned (`s < u`)
- Missing initializer para campo de struct (`depth` en `Config`)

Cuenta el total y comparalo con el paso anterior.

### Paso 1.8 -- Compilacion con -Wall -Wextra -Wpedantic

```bash
gcc -Wall -Wextra -Wpedantic -std=c11 warnings.c -o warnings_wpedantic 2>&1
```

-Wpedantic advierte sobre extensiones de GCC que no son parte del estandar C.
En este archivo no hay extensiones GNU, asi que predice: cambiara el numero
de warnings respecto al paso anterior?

### Paso 1.9 -- Verificar -Wpedantic

En este caso, -Wpedantic no agrega warnings nuevos porque `warnings.c` no
usa extensiones GNU. El numero de warnings es el mismo que con -Wall -Wextra.
En la parte 2 veras el efecto real de -Wpedantic con extensiones.

### Paso 1.10 -- -Werror convierte warnings en errores

```bash
gcc -Wall -Wextra -Werror warnings.c -o warnings_werror 2>&1
```

Observa: la compilacion falla. Cada warning se convierte en un error. Esto
es util en CI/CD para forzar codigo limpio. Ningun binario se genera.

```bash
ls warnings_werror 2>&1
```

Salida esperada:

```
ls: cannot access 'warnings_werror': No such file or directory
```

### Paso 1.11 -- Suprimir un warning especifico

```bash
gcc -Wall -Wextra -Wno-unused-variable warnings.c -o warnings_suppress 2>&1
```

Compara con el paso 1.7. El warning de `unused_var` desaparecio, pero todos
los demas siguen. `-Wno-unused-variable` desactiva ese warning especifico
sin afectar los demas.

### Limpieza de la parte 1

```bash
rm -f warnings_none warnings_wall warnings_wextra warnings_wpedantic warnings_suppress
```

---

## Parte 2 -- Seleccion del estandar

**Objetivo**: Entender la diferencia entre -std=gnu11 (C11 + extensiones GNU)
y -std=c11 (C11 estricto), y como -Wpedantic expone las extensiones.

### Paso 2.1 -- Examinar el codigo fuente

```bash
cat standard.c
```

Este archivo usa tres extensiones GNU:

- `int data[0]` -- zero-length array (ISO C lo prohibe)
- `({ int tmp = x * 2; tmp + 1; })` -- statement expression
- `0b11110000` -- literal binario

### Paso 2.2 -- Compilar con -std=gnu11

```bash
gcc -std=gnu11 -Wall -Wextra standard.c -o standard_gnu 2>&1
./standard_gnu
```

Salida esperada:

```
size of flex = 4
y    = 201
mask = 0xF0
```

Predice antes de continuar: que pasara si compilamos con -std=c11 -Wpedantic
en lugar de -std=gnu11?

### Paso 2.3 -- Compilar con -std=c11 -Wpedantic

```bash
gcc -std=c11 -Wpedantic -Wall -Wextra standard.c -o standard_strict 2>&1
```

Observa los warnings. Cada uno identifica una extension GNU que no es parte
del estandar C11:

- Los zero-length arrays estan prohibidos por ISO C
- Las statement expressions (braced-groups) son una extension GNU
- Los literales binarios son una extension GNU (estandarizados en C23)

El programa **compila** (son warnings, no errores), pero los warnings
indican que este codigo no es portable a otros compiladores.

### Paso 2.4 -- Verificar que el programa funciona igual

```bash
./standard_strict
```

Salida esperada:

```
size of flex = 4
y    = 201
mask = 0xF0
```

El comportamiento es identico. Las extensiones GNU producen el mismo resultado
-- simplemente no son parte del estandar C11.

### Paso 2.5 -- Reflexion

La diferencia clave:

- `-std=gnu11` (default de GCC): acepta extensiones sin quejarse
- `-std=c11 -Wpedantic`: advierte sobre extensiones, exponiendo dependencias
  del compilador

Para escribir C portable, usar `-std=c11 -Wpedantic` ayuda a detectar
codigo que no compilara con otros compiladores (Clang los soporta tambien,
pero MSVC no).

### Limpieza de la parte 2

```bash
rm -f standard_gnu standard_strict
```

---

## Parte 3 -- Flags de debug y optimizacion

**Objetivo**: Ver el efecto de -g (debug info) y los niveles de optimizacion
(-O0, -O2) en el tamano del binario y el assembly generado.

### Paso 3.1 -- Compilar con -g -O0 (desarrollo)

```bash
gcc -g -O0 optimize_cmp.c -o optimize_debug
```

### Paso 3.2 -- Compilar con -O2 (produccion, sin debug)

```bash
gcc -O2 optimize_cmp.c -o optimize_fast
```

### Paso 3.3 -- Comparar tamanos

Antes de comparar, predice: cual binario sera mas grande, el de debug (-g)
o el optimizado (-O2)?

```bash
ls -l optimize_debug optimize_fast
```

El binario con -g es significativamente mas grande. -g incluye tablas de
simbolos, mapeo de lineas de codigo, nombres de variables, y otra
informacion que GDB necesita para depurar.

### Paso 3.4 -- Verificar la informacion de debug con file

```bash
file optimize_debug
file optimize_fast
```

Observa que `optimize_debug` dice "with debug_info" y `optimize_fast` no.
Esa informacion de debug es lo que agrega el tamano extra.

### Paso 3.5 -- Generar assembly con -O0

```bash
gcc -S -O0 optimize_cmp.c -o optimize_O0.s
```

### Paso 3.6 -- Generar assembly con -O2

```bash
gcc -S -O2 optimize_cmp.c -o optimize_O2.s
```

### Paso 3.7 -- Comparar cantidad de lineas

Predice: cuantas veces mas grande sera el assembly de -O0 comparado con -O2?

```bash
wc -l optimize_O0.s optimize_O2.s
```

Con -O0, el compilador traduce cada linea de C literalmente: carga variables
de la pila, opera, guarda el resultado en la pila. Con -O2, el compilador
puede usar registros, eliminar accesos redundantes a memoria, y hasta
transformar el loop en una formula cerrada.

### Paso 3.8 -- Examinar las diferencias clave

Busca la funcion `sum_to_n` en ambos assemblies:

```bash
echo "=== -O0: sum_to_n ==="
grep -A 30 "sum_to_n:" optimize_O0.s | head -35

echo ""
echo "=== -O2: sum_to_n ==="
grep -A 30 "sum_to_n:" optimize_O2.s | head -35
```

Con -O0, veras un loop completo con accesos a la pila en cada iteracion.
Con -O2, el compilador puede haber optimizado el loop drasticamente o
incluso reemplazado toda la funcion con la formula n*(n+1)/2.

### Paso 3.9 -- Efecto de -g sin optimizacion vs con optimizacion

```bash
gcc -g -O2 optimize_cmp.c -o optimize_debug_fast
ls -l optimize_debug optimize_debug_fast optimize_fast
```

Observa: -g y -O2 son compatibles. Se puede tener informacion de debug con
codigo optimizado, pero la experiencia de depuracion puede ser confusa
(variables eliminadas, lineas reordenadas).

### Limpieza de la parte 3

```bash
rm -f optimize_debug optimize_fast optimize_debug_fast
rm -f optimize_O0.s optimize_O2.s
```

---

## Parte 4 -- Sets recomendados

**Objetivo**: Practicar los dos sets de flags mas importantes: desarrollo
(maxima deteccion de errores) y produccion (optimizado, sin debug).

### Paso 4.1 -- Set de desarrollo

```bash
gcc -Wall -Wextra -Wpedantic -std=c11 -g -O0 \
    -fsanitize=address,undefined \
    optimize_cmp.c -o optimize_dev
```

Este set incluye:

- Todos los niveles de warnings (-Wall -Wextra -Wpedantic)
- Estandar estricto (-std=c11)
- Informacion de debug (-g)
- Sin optimizacion (-O0) para debug preciso
- Sanitizers: detectan errores de memoria y comportamiento indefinido en runtime

### Paso 4.2 -- Set de produccion

```bash
gcc -Wall -Wextra -std=c11 -O2 -DNDEBUG \
    optimize_cmp.c -o optimize_prod
```

Este set incluye:

- Warnings principales (-Wall -Wextra) para detectar problemas
- Estandar sin -Wpedantic (no necesario en produccion si ya se verifico)
- Optimizacion de produccion (-O2)
- -DNDEBUG desactiva assert() (equivalente a #define NDEBUG)
- Sin -g (no se quiere debug info en binarios de produccion)
- Sin sanitizers (agregan overhead de rendimiento)

### Paso 4.3 -- Comparar tamanos

Predice: cual binario sera mas grande? El de desarrollo tiene -g y
sanitizers. El de produccion tiene -O2 y nada mas.

```bash
ls -l optimize_dev optimize_prod
```

El binario de desarrollo es mucho mas grande por:

- Informacion de debug (-g)
- Codigo de instrumentacion de los sanitizers (-fsanitize)
- Sin optimizacion (-O0) que no reduce codigo

### Paso 4.4 -- Comparar dependencias

```bash
echo "=== Desarrollo ==="
ldd optimize_dev

echo ""
echo "=== Produccion ==="
ldd optimize_prod
```

El binario de desarrollo depende de bibliotecas adicionales de los sanitizers
(libasan, libubsan). El de produccion solo depende de libc y el linker
dinamico.

### Paso 4.5 -- Verificar que ambos funcionan

```bash
echo "=== Desarrollo ==="
./optimize_dev

echo ""
echo "=== Produccion ==="
./optimize_prod
```

Salida esperada (en ambos casos):

```
sum(1..1000000) = 500000500000
```

Mismo resultado, diferente proposito: desarrollo detecta errores, produccion
maximiza rendimiento.

### Limpieza de la parte 4

```bash
rm -f optimize_dev optimize_prod
```

---

## Limpieza final

```bash
rm -f warnings_* standard_* optimize_dev optimize_prod
rm -f optimize_debug optimize_fast optimize_debug_fast
rm -f optimize_O0.s optimize_O2.s
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  optimize_cmp.c  standard.c  warnings.c
```

---

## Conceptos reforzados

1. `-Wall` no activa todos los warnings. `-Wextra` agrega parametros no
   usados y missing initializers. `-Wpedantic` advierte sobre extensiones
   GNU que no son parte del estandar.

2. `-Werror` convierte warnings en errores de compilacion -- ningun binario
   se genera si hay warnings. `-Wno-xxx` desactiva un warning especifico.

3. `-std=c11` compila en modo C11 estricto. `-std=gnu11` (default de GCC)
   permite extensiones como zero-length arrays, statement expressions, y literales
   binarios. `-Wpedantic` expone las extensiones.

4. `-g` agrega informacion de debug al binario, aumentando su tamano. `file`
   muestra "with debug_info" para binarios compilados con -g.

5. `-O0` traduce cada linea de C literalmente. `-O2` puede reducir un loop
   a una formula cerrada, eliminar accesos redundantes a memoria, e inlinear
   funciones. La diferencia en tamano de assembly es directamente observable.

6. El set de desarrollo (`-Wall -Wextra -Wpedantic -std=c11 -g -O0
   -fsanitize=address,undefined`) maximiza la deteccion de errores. El set
   de produccion (`-Wall -Wextra -std=c11 -O2 -DNDEBUG`) maximiza el
   rendimiento.

7. Los sanitizers (-fsanitize) agregan dependencias de bibliotecas
   adicionales (libasan, libubsan) y overhead de rendimiento. Solo se usan
   en desarrollo, nunca en produccion.
