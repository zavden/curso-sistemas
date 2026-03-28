# Lab — Memory leaks

## Objetivo

Provocar memory leaks intencionales, detectarlos con Valgrind, interpretar
las categorias de perdida (definitely lost, indirectly lost, still reachable),
y aplicar las correcciones correspondientes. Al finalizar, sabras identificar
y corregir los patrones de leak mas comunes en C.

## Prerequisitos

- GCC y Valgrind instalados
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `simple_leaks.c` | Tres leaks clasicos: simple, reasignacion, error path |
| `valgrind_categories.c` | Leaks que producen cada categoria de Valgrind |
| `ownership.c` | Leaks por ownership incorrecto y destructor malo |
| `fixed_leaks.c` | Versiones corregidas de todos los leaks |

---

## Parte 1 — Tipos de leaks

**Objetivo**: Reconocer los tres patrones de leak mas comunes: leak simple,
leak por reasignacion del puntero, y leak en un error path.

### Paso 1.1 — Verificar el entorno

```bash
gcc --version
valgrind --version
```

Confirma que GCC y Valgrind estan instalados. Anota las versiones.

### Paso 1.2 — Examinar el codigo fuente

```bash
cat simple_leaks.c
```

Antes de compilar, examina las tres funciones y responde mentalmente:

- En `simple_leak()`, hay algun `free(data)` antes de retornar?
- En `reassignment_leak()`, que pasa con la primera alocacion cuando se
  ejecuta `msg = malloc(64)`?
- En `error_path_leak()`, que pasa con `buffer` cuando `trigger_error` es
  verdadero?

Identifica donde esta el leak en cada funcion antes de continuar.

### Paso 1.3 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -g -O0 simple_leaks.c -o simple_leaks
./simple_leaks
```

Salida esperada:

```
--- Leak 1: simple leak ---
simple_leak: computed 10 squares

--- Leak 2: reassignment leak ---
reassignment_leak: msg = "first message"
reassignment_leak: msg = "second message"

--- Leak 3: error path leak ---
error_path_leak: processing data...
error_path_leak: error detected, returning early

Program finished.
```

El programa funciona sin errores visibles. Los leaks son silenciosos -- el
programa no falla, simplemente pierde memoria que nunca se recupera.

### Paso 1.4 — Detectar leaks con Valgrind

Antes de ejecutar Valgrind, predice:

- Cuantos bloques perdidos reportara? (uno por cada leak)
- Cuantos bytes se pierden en `simple_leak()`? (10 ints de 4 bytes)
- Cuantos bytes se pierden en `error_path_leak()`? (malloc de 256)

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.5 — Verificar la prediccion

```bash
valgrind --leak-check=full ./simple_leaks
```

Observa el LEAK SUMMARY al final:

```
LEAK SUMMARY:
   definitely lost: 328 bytes in 3 blocks
   indirectly lost: 0 bytes in 0 blocks
     possibly lost: 0 bytes in 0 blocks
   still reachable: 0 bytes in 0 blocks
        suppressed: 0 bytes in 0 blocks
```

Tres bloques perdidos, todos "definitely lost":

- 40 bytes en `simple_leak` (10 * sizeof(int) = 40)
- 32 bytes en `reassignment_leak` (primer malloc de 32 bytes)
- 256 bytes en `error_path_leak` (malloc de 256 bytes)

Observa que Valgrind muestra la linea exacta del `malloc` que origino cada
leak. Esto es posible gracias a `-g` (debug info) que se uso al compilar.

### Paso 1.6 — Analizar cada reporte

Revisa la salida de Valgrind linea por linea. Para cada bloque "definitely
lost", Valgrind muestra un stack trace:

```
40 bytes in 1 blocks are definitely lost in loss record 2 of 3
   at 0x...: malloc (vg_replace_malloc.c:...)
   by 0x...: simple_leak (simple_leaks.c:8)
   by 0x...: main (simple_leaks.c:49)
```

El stack trace se lee de abajo hacia arriba: `main` llamo a `simple_leak`,
que llamo a `malloc` en la linea 8. Esos 40 bytes nunca se liberaron.

### Limpieza de la parte 1

```bash
rm -f simple_leaks
```

---

## Parte 2 — Categorias de Valgrind

**Objetivo**: Provocar leaks que Valgrind clasifique en cada una de sus
categorias principales: definitely lost, indirectly lost, y still reachable.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat valgrind_categories.c
```

El programa tiene tres funciones, una por cada categoria:

- `definitely_lost()` — hace `data = NULL` despues del malloc
- `indirectly_lost()` — aloca un struct con miembros alocados, no libera nada
- `still_reachable()` — asigna a un puntero global, nunca hace free

Antes de continuar, predice para `indirectly_lost()`:

- Cuantos bloques se pierden en total? (el struct + name + scores = 3)
- Cual es "definitely lost" y cuales son "indirectly lost"?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.2 — Compilar y ejecutar con Valgrind

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -g -O0 valgrind_categories.c -o valgrind_categories
valgrind --leak-check=full --show-leak-kinds=all ./valgrind_categories
```

Observa el flag `--show-leak-kinds=all`. Sin este flag, Valgrind no muestra
los bloques "still reachable". Con `all`, muestra las cuatro categorias.

### Paso 2.3 — Interpretar el LEAK SUMMARY

```
LEAK SUMMARY:
   definitely lost: 88 bytes in 2 blocks
   indirectly lost: 52 bytes in 2 blocks
     possibly lost: 0 bytes in 0 blocks
   still reachable: 128 bytes in 1 blocks
        suppressed: 0 bytes in 0 blocks
```

Desglose:

**Definitely lost (88 bytes, 2 bloques)**:
- 64 bytes de `definitely_lost()` -- el puntero se sobreescribio con NULL
- 24 bytes del struct `Record` en `indirectly_lost()` -- el puntero `rec` se
  perdio al salir del scope sin free

**Indirectly lost (52 bytes, 2 bloques)**:
- 32 bytes de `rec->name` (malloc de 32)
- 20 bytes de `rec->scores` (5 * sizeof(int) = 20)
- Estos bloques solo eran accesibles a traves de `rec`, que se perdio. Al
  arreglar el leak de `rec`, automaticamente se resolverian estos.

**Still reachable (128 bytes, 1 bloque)**:
- 128 bytes de `global_config` en `still_reachable()`
- El puntero `global_config` todavia existe al terminar el programa
- Valgrind puede "verlo", pero nunca se hizo free
- Generalmente no es un bug -- es memoria que vive toda la vida del programa

### Paso 2.4 — Confirmar la diferencia con y sin --show-leak-kinds

```bash
valgrind --leak-check=full ./valgrind_categories 2>&1 | grep -A1 "still reachable"
```

Sin `--show-leak-kinds=all`, los 128 bytes aparecen en el resumen pero
Valgrind no muestra el stack trace detallado. El flag `all` es necesario para
ver de donde viene cada tipo de leak.

### Limpieza de la parte 2

```bash
rm -f valgrind_categories
```

---

## Parte 3 — Leak en programa real

**Objetivo**: Entender leaks que ocurren cuando una funcion aloca y retorna
memoria, y cuando un destructor no libera todos los miembros de un struct.

### Paso 3.1 — Examinar el codigo fuente

```bash
cat ownership.c
```

El programa tiene dos escenarios:

- **Part A**: `create_greeting()` aloca y retorna un string. El caller es
  responsable de hacer free. En `main`, se crean dos greetings pero solo se
  libera uno.

- **Part B**: `student_create()` aloca un struct con miembros alocados.
  `student_free_wrong()` solo hace free del struct, sin liberar sus miembros.

Antes de continuar, predice:

- En Part A, cuantos bytes se pierden? (largo de "Hello, Alice!" + 1)
- En Part B, que miembros del struct se pierden? (name y grades)
- Valgrind reportara los miembros como "definitely lost" o "indirectly lost"?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.2 — Compilar y ejecutar con Valgrind

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -g -O0 ownership.c -o ownership
valgrind --leak-check=full --show-leak-kinds=all ./ownership
```

### Paso 3.3 — Analizar los resultados

Observa los leaks reportados:

- `create_greeting` en `main` linea donde se crea g1: "definitely lost" --
  el caller nunca hizo free de g1

- `student_free_wrong` causa que `name` y `grades` se pierdan como
  "indirectly lost" -- solo son accesibles a traves del struct que se libero
  pero sus punteros internos apuntan a memoria huerfana

Fijate en el stack trace de cada leak. Valgrind apunta al `malloc` original,
no al lugar donde se perdio la referencia. Esto es lo esperable -- Valgrind
sabe donde se aloco la memoria, no donde "deberia" haberse liberado.

### Paso 3.4 — El problema de ownership

El patron "funcion que aloca y retorna" es muy comun en C. El problema es
que no hay nada en el lenguaje que obligue al caller a hacer free. Es pura
convencion:

```bash
grep -n "create_greeting\|free" ownership.c
```

Observa que `g2` si se libera pero `g1` no. El compilador no da ninguna
advertencia sobre esto. Solo Valgrind (o herramientas similares) pueden
detectarlo en runtime.

### Limpieza de la parte 3

```bash
rm -f ownership
```

---

## Parte 4 — Corregir leaks

**Objetivo**: Aplicar las correcciones a cada tipo de leak y verificar con
Valgrind que el programa queda libre de perdidas.

### Paso 4.1 — Examinar las correcciones

```bash
cat fixed_leaks.c
```

Compara mentalmente cada funcion con su version con leak:

- `simple_no_leak()` vs `simple_leak()` — se agrego `free(data)` al final
- `reassignment_no_leak()` vs `reassignment_leak()` — se agrego `free(msg)`
  antes de reasignar
- `error_path_no_leak()` vs `error_path_leak()` — se usa el patron goto
  cleanup
- `student_free()` vs `student_free_wrong()` — se liberan los miembros antes
  del struct
- En `main`, se hace `free(g1)` y `free(g2)` despues de usarlos

### Paso 4.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -g -O0 fixed_leaks.c -o fixed_leaks
./fixed_leaks
```

Salida esperada:

```
--- Fix 1: simple -- free before return ---
simple_no_leak: computed 10 squares

--- Fix 2: reassignment -- free before reassigning ---
reassignment_no_leak: msg = "first message"
reassignment_no_leak: msg = "second message"

--- Fix 3: error path -- goto cleanup ---
error_path_no_leak: processing data...
error_path_no_leak: error detected, cleaning up

--- Fix 4: proper destructor ---
Student: Carlos, grades: 85, 92, 78

--- Fix 5: caller frees returned pointer ---
g1: Hello, Alice!
g2: Hello, Bob!

All memory freed. Program finished.
```

### Paso 4.3 — Verificar con Valgrind

Antes de ejecutar, predice: cuantos leaks reportara Valgrind?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.4 — Confirmar cero leaks

```bash
valgrind --leak-check=full ./fixed_leaks
```

Salida esperada al final:

```
HEAP SUMMARY:
    in use at exit: 0 bytes in 0 blocks
  total heap usage: 10 allocs, 10 frees, 4,557 bytes allocated

All heap blocks were freed -- no leaks are possible
```

10 alocaciones, 10 liberaciones. Cero bytes en uso al terminar. Este es el
resultado ideal: cada `malloc` tiene su `free` correspondiente.

### Paso 4.5 — Resumen de correcciones aplicadas

Revisa las cinco correcciones aplicadas:

| Patron de leak | Correccion |
|----------------|------------|
| Olvidar free | Agregar `free()` antes de retornar |
| Reasignar puntero | `free()` antes de reasignar |
| Return temprano | Patron `goto cleanup` |
| Destructor incompleto | Liberar miembros antes del struct |
| Funcion que retorna memoria | El caller hace `free()` del resultado |

### Limpieza de la parte 4

```bash
rm -f fixed_leaks
```

---

## Limpieza final

```bash
rm -f simple_leaks valgrind_categories ownership fixed_leaks
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  fixed_leaks.c  ownership.c  simple_leaks.c  valgrind_categories.c
```

---

## Conceptos reforzados

1. Un memory leak ocurre cuando se pierde la ultima referencia a memoria
   alocada -- el programa no falla, pero la memoria queda reservada sin
   posibilidad de liberarse hasta que el proceso termina.

2. Los tres patrones de leak mas comunes son: olvidar `free()`, sobreescribir
   el unico puntero a la memoria, y retornar temprano sin liberar recursos.

3. Valgrind clasifica los leaks en categorias: **definitely lost** (la
   referencia se perdio completamente), **indirectly lost** (accesible solo
   a traves de otro bloque perdido), y **still reachable** (accesible al
   terminar pero nunca liberado).

4. Los bloques "indirectly lost" se resuelven automaticamente al arreglar
   el "definitely lost" del que dependen -- no necesitan correccion separada.

5. El flag `-g` al compilar es necesario para que Valgrind muestre los
   numeros de linea en los stack traces. Sin `-g`, solo se ven direcciones
   de memoria.

6. El patron `goto cleanup` centraliza la liberacion de recursos en un solo
   punto, eliminando leaks en paths de error con returns tempranos.

7. Un destructor de struct debe liberar todos los miembros alocados
   dinamicamente antes de liberar el struct mismo. Olvidar un miembro
   causa un "indirectly lost".

8. Cuando una funcion aloca y retorna memoria, el caller es responsable
   de liberarla. C no tiene mecanismo para forzar esto -- es pura convencion
   y disciplina, reforzada con herramientas como Valgrind.
