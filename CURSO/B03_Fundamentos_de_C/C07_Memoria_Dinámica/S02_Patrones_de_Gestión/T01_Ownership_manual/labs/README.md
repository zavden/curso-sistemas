# Lab — Ownership manual

## Objetivo

Practicar las tres convenciones principales de ownership en C (caller owns,
callee owns, transfer), implementar el patron create/destroy, y usar Valgrind
para detectar errores de ownership. Al finalizar, sabras decidir y documentar
quien es responsable de liberar cada bloque de memoria.

## Prerequisitos

- GCC y Valgrind instalados
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `caller_owns.c` | Funciones que retornan memoria alocada, el caller libera |
| `callee_owns.c` | Struct Person con constructor y destructor |
| `transfer.c` | Stack que toma y transfiere ownership de datos |
| `ownership_errors.c` | Programa con 4 errores intencionales de ownership |

---

## Parte 1 — Caller owns

**Objetivo**: Observar el patron donde una funcion retorna memoria alocada y el
caller es responsable de liberarla.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat caller_owns.c
```

Observa:

- `string_duplicate()` usa `malloc` y `strcpy` para crear una copia
- `make_greeting()` usa `snprintf` dos veces: una para medir, otra para escribir
- Ambas funciones tienen un comentario `@return` que documenta el ownership
- `main()` llama a `free()` para cada puntero recibido

Antes de compilar, predice:

- Cuantas llamadas a `malloc` hay en total durante la ejecucion?
- Cuantas llamadas a `free` hay en `main`?
- Valgrind reportara leaks?

### Paso 1.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic caller_owns.c -o caller_owns
./caller_owns
```

Salida esperada:

```
s1 = "ownership in C"
s1 address = <address>
greeting = "Hello, Alice! You are 30 years old."
All memory freed by caller.
```

Las direcciones varian entre ejecuciones. Lo importante es que cada puntero
recibido se libera en `main`.

### Paso 1.3 — Verificar con Valgrind

```bash
valgrind --leak-check=full ./caller_owns
```

Busca estas lineas al final de la salida:

```
All heap blocks were freed -- no leaks are possible
ERROR SUMMARY: 0 errors from 0 contexts
```

Hay 2 `malloc` en las funciones (uno en `string_duplicate`, otro en
`make_greeting`) y 2 `free` en `main`. Valgrind tambien cuenta la alocacion
interna de `printf` (el buffer de stdout), por eso el total de allocs puede ser
3 en vez de 2.

### Limpieza de la parte 1

```bash
rm -f caller_owns
```

---

## Parte 2 — Callee owns (create/destroy)

**Objetivo**: Implementar el patron constructor/destructor donde el struct es
dueno de sus datos internos y el destructor libera todo.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat callee_owns.c
```

Observa la estructura:

- `struct Person` tiene dos campos `char *` (alocados dinamicamente)
- `person_create()` aloca el struct Y sus campos internos
- `person_destroy()` libera los campos internos Y el struct
- En `main()` no se hace `free(alice->name)` — eso es responsabilidad de `person_destroy`

Antes de compilar, predice:

- Cuantas llamadas a `malloc` ocurren al crear una Person? (pista: el struct
  mas cada campo string)
- Si `person_create` falla a mitad de camino (por ejemplo, el segundo `malloc`
  falla), se produce un leak?

### Paso 2.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic callee_owns.c -o callee_owns
./callee_owns
```

Salida esperada:

```
Person { name="Alice", email="alice@example.com", age=30 }
Person { name="Bob", email="bob@example.com", age=25 }
All persons destroyed correctly.
```

### Paso 2.3 — Verificar con Valgrind

```bash
valgrind --leak-check=full ./callee_owns
```

Busca:

```
All heap blocks were freed -- no leaks are possible
ERROR SUMMARY: 0 errors from 0 contexts
```

Cada `person_create` hace 3 alocaciones (struct + name + email). Con 2 personas,
son 6 alocaciones. Mas el buffer de stdout, el total sera 7 allocs y 7 frees.

### Paso 2.4 — Analizar el manejo de errores

Vuelve a leer `person_create` en el codigo fuente. Observa que si el segundo
`malloc` falla:

1. Se inicializan `p->name` y `p->email` a `NULL` antes de asignarlos
2. Si alguno falla, se hace `free` de ambos (y `free(NULL)` es seguro en C)
3. Se libera el struct `p` tambien

Este patron evita leaks en caso de error parcial. Es un patron comun en codigo
C de produccion.

### Limpieza de la parte 2

```bash
rm -f callee_owns
```

---

## Parte 3 — Transfer ownership

**Objetivo**: Observar como la responsabilidad de un puntero puede moverse de
una funcion a otra, y que el codigo debe documentar claramente quien es dueno
en cada momento.

### Paso 3.1 — Examinar el codigo fuente

```bash
cat transfer.c
```

Observa:

- `stack_push()` dice `TAKES OWNERSHIP` — despues de llamarla, el caller no
  debe usar ni liberar el puntero
- `stack_pop()` dice `TRANSFERS OWNERSHIP to caller` — el caller recibe el
  puntero y debe hacer `free`
- `stack_destroy()` libera todos los datos que aun quedan en el stack

Antes de compilar, predice la secuencia de operaciones:

- Se pushean 4 strings. Despues de cada push, el caller ya no es dueno
- Se popean 2 strings. El caller recibe ownership y hace `free`
- Se destruye el stack. Los 2 strings restantes se liberan internamente

Cuantos allocs y frees habra en total?

### Paso 3.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic transfer.c -o transfer
./transfer
```

Salida esperada:

```
--- Pushing 4 strings onto stack ---
Pushing "Alice" (address <addr>)
Pushing "Bob" (address <addr>)
Pushing "Charlie" (address <addr>)
Pushing "Diana" (address <addr>)
Stack count: 4

--- Popping 2 items (we receive ownership) ---
Popped: "Diana" (address <addr>)
Popped: "Charlie" (address <addr>)
Stack count: 2

--- Destroying stack (frees remaining items) ---
Stack destroyed. Count: 0
```

Observa que el stack es LIFO: Diana (la ultima en entrar) es la primera en salir.

### Paso 3.3 — Verificar con Valgrind

```bash
valgrind --leak-check=full ./transfer
```

Busca:

```
All heap blocks were freed -- no leaks are possible
ERROR SUMMARY: 0 errors from 0 contexts
```

Hay 8 alocaciones en total: 4 strings (`malloc` en el loop) + 4 nodos
(`malloc` en `stack_push`). Los frees se distribuyen asi:

- 2 strings liberados por `main` (despues de `stack_pop`)
- 2 nodos liberados por `stack_pop` (al extraer)
- 2 strings + 2 nodos liberados por `stack_destroy`

Total: 8 allocs, 8 frees. Mas el buffer de stdout = 9 allocs, 9 frees.

### Paso 3.4 — Observar las direcciones

Ejecuta de nuevo sin Valgrind:

```bash
./transfer
```

Compara las direcciones de push y pop. La direccion de "Diana" en el push debe
ser la misma que en el pop. El puntero no se copio — se movio. Esto es
transferencia de ownership: el mismo bloque de memoria cambio de dueno.

### Limpieza de la parte 3

```bash
rm -f transfer
```

---

## Parte 4 — Errores de ownership

**Objetivo**: Provocar errores clasicos de ownership y observar como Valgrind
los detecta. Este programa tiene errores **intencionales**.

### Paso 4.1 — Examinar el codigo fuente

```bash
cat ownership_errors.c
```

Observa las cuatro funciones de error:

1. `error_double_free` — llama a `free` dos veces sobre el mismo puntero
2. `error_use_after_free` — lee memoria despues de liberarla
3. `error_memory_leak` — sobrescribe un puntero sin liberar la memoria anterior
4. `error_use_after_transfer` — usa un puntero despues de que el "dueno" lo libero

### Paso 4.2 — Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ownership_errors.c -o ownership_errors
```

Observa los warnings del compilador. GCC detecta algunos de estos errores:

```
warning: pointer 's' used after 'free' [-Wuse-after-free]
```

El compilador advierte sobre use-after-free, pero **no impide la compilacion**.
En C, estos errores son warnings, no errores fatales. Rust los rechazaria en
compilacion.

### Paso 4.3 — Error 3: memory leak

Empezamos por el error mas seguro (no causa undefined behavior en runtime):

```bash
valgrind --leak-check=full ./ownership_errors 3
```

Salida esperada (relevante):

```
=== Error: memory leak ===
s = "first allocation"
s = "second allocation" (old allocation leaked!)
Only freed the second allocation. First is leaked.

HEAP SUMMARY:
    in use at exit: 17 bytes in 1 blocks

17 bytes in 1 blocks are definitely lost in loss record 1 of 1
   at 0x...: malloc
   by 0x...: error_memory_leak

LEAK SUMMARY:
   definitely lost: 17 bytes in 1 blocks

ERROR SUMMARY: 1 errors from 1 contexts
```

Valgrind identifica exactamente donde se aloco la memoria perdida. Los 17 bytes
corresponden a `"first allocation"` (16 caracteres + 1 byte nulo).

### Paso 4.4 — Error 2: use after free

```bash
valgrind --leak-check=full ./ownership_errors 2
```

Busca en la salida:

```
Invalid read of size 1
   at 0x...: ...printf...
   by 0x...: error_use_after_free
 Address 0x... is 0 bytes inside a block of size 6 free'd
   at 0x...: free
   by 0x...: error_use_after_free
```

Valgrind detecta que se lee memoria que ya fue liberada. "Invalid read" indica
un acceso a memoria invalida. Los 6 bytes corresponden a `"hello"` (5 + nulo).

### Paso 4.5 — Error 1: double free

```bash
valgrind --leak-check=full ./ownership_errors 1
```

Busca en la salida:

```
Invalid free() / delete / delete[] / realloc()
   at 0x...: free
   by 0x...: error_double_free
 Address 0x... is 0 bytes inside a block of size 6 free'd
   at 0x...: free
   by 0x...: error_double_free
```

Valgrind detecta que se intenta liberar un bloque que ya fue liberado.
"Invalid free" es la senal de un double free.

### Paso 4.6 — Error 4: use after transfer

```bash
valgrind --leak-check=full ./ownership_errors 4
```

Busca en la salida:

```
Invalid read of size 1
   at 0x...: ...printf...
   by 0x...: error_use_after_transfer
 Address 0x... is 0 bytes inside a block of size 15 free'd
```

Este es el mismo tipo de error que use-after-free, pero con un contexto
diferente: el puntero original sigue existiendo como variable, pero la memoria
a la que apunta ya fue liberada por quien recibio el ownership.

En Rust, este error seria un **error de compilacion**: el compilador no
permitiria usar `data` despues de moverlo.

### Paso 4.7 — Reflexion sobre los errores

Observa que los cuatro errores comparten un patron:

- El programador pierde la nocion de **quien es dueno** de la memoria
- El compilador advierte sobre algunos (use-after-free) pero no impide la compilacion
- Solo Valgrind detecta todos los errores en runtime

Por eso documentar ownership con comentarios (`@param`, `@return`) es critico en
C. En Rust, el sistema de tipos hace este trabajo automaticamente.

### Limpieza de la parte 4

```bash
rm -f ownership_errors
```

---

## Limpieza final

```bash
rm -f caller_owns callee_owns transfer ownership_errors
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  callee_owns.c  caller_owns.c  ownership_errors.c  transfer.c
```

---

## Conceptos reforzados

1. En el patron **caller owns**, la funcion retorna un puntero a memoria
   alocada y el caller es responsable de hacer `free`. Este es el patron de
   `strdup`, `malloc`, y la mayoria de funciones que crean recursos.

2. En el patron **callee owns** (create/destroy), el struct encapsula sus
   recursos internos. El destructor libera todo, incluyendo campos alocados
   dinamicamente. El usuario nunca hace `free` de los campos individuales.

3. La **transferencia de ownership** mueve la responsabilidad de un puntero de
   una funcion a otra. Despues de transferir, el puntero original no debe usarse
   ni liberarse. Documentar esto con comentarios es esencial.

4. **Double free** ocurre cuando se llama a `free` dos veces sobre el mismo
   puntero. Valgrind lo reporta como "Invalid free".

5. **Use after free** ocurre cuando se accede a memoria ya liberada. Valgrind
   lo reporta como "Invalid read". GCC lo detecta con `-Wuse-after-free`.

6. **Memory leak** ocurre cuando se pierde la referencia a memoria alocada sin
   liberarla. Valgrind identifica la linea exacta de la alocacion perdida.

7. Documentar ownership con comentarios (`@return: caller must free`,
   `@param: takes ownership`) es la unica herramienta que tiene C para
   comunicar la intencion. Rust resuelve esto a nivel del sistema de tipos.

8. `free(NULL)` es seguro en C (no hace nada). Esto simplifica el manejo de
   errores en destructores que limpian multiples campos.
