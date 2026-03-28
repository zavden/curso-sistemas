# Lab — Punteros colgantes y wild pointers

## Objetivo

Provocar dangling pointers y wild pointers de forma controlada, observar su
comportamiento indefinido, y aprender a detectarlos con advertencias del
compilador y herramientas de runtime (Valgrind). Al finalizar, sabras
reconocer las causas mas comunes de estos bugs y aplicar patrones de
prevencion como SAFE_FREE.

## Prerequisitos

- GCC instalado
- Valgrind instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `dangling_uaf.c` | Programa que provoca use-after-free |
| `dangling_local.c` | Programa que retorna direccion de variable local |
| `wild_pointer.c` | Programa con puntero no inicializado |
| `safe_free.c` | Patron SAFE_FREE y verificacion antes de uso |

---

## Parte 1 — Dangling pointer: use-after-free

**Objetivo**: Observar que ocurre al acceder a memoria ya liberada con `free()`,
y como el compilador y Valgrind detectan este bug.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat dangling_uaf.c
```

Observa la estructura:

- Se aloca un `int` con `malloc`, se le asigna 42
- Se libera con `free(p)`
- Despues del `free`, se lee y se escribe a traves de `p`

Antes de compilar, predice:

- El compilador producira warnings con `-Wall -Wextra`?
- El programa crasheara al ejecutarse, o imprimira valores?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.2 — Compilar con warnings

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic dangling_uaf.c -o dangling_uaf
```

Salida esperada (warnings):

```
dangling_uaf.c: warning: pointer 'p' used after 'free' [-Wuse-after-free]
```

El compilador detecta multiples accesos a `p` despues de `free()`. Cada warning
indica la linea del acceso y la linea donde se llamo `free`. El flag
responsable es `-Wuse-after-free`, que esta incluido en `-Wall`.

### Paso 1.3 — Ejecutar sin sanitizers

```bash
./dangling_uaf
```

Salida esperada (puede variar -- es UB):

```
Before free: *p = 42, p = 0x<addr>
After free:  p = 0x<addr>  (address unchanged, but memory invalid)
After free:  *p = 0  (UB -- may print 42, garbage, or crash)
After write: *p = 100  (UB -- wrote to freed memory)
```

Observa:

- La direccion `p` no cambia despues de `free` -- sigue apuntando al mismo
  sitio, pero la memoria ya no es nuestra
- La lectura puede devolver 42, 0, o basura. El resultado depende de la
  implementacion de `malloc` y del estado del heap
- La escritura "funciona" silenciosamente -- este es el peor caso porque
  el bug pasa desapercibido

### Paso 1.4 — Detectar con Valgrind

```bash
valgrind ./dangling_uaf
```

Salida esperada (fragmento relevante):

```
==<pid>== Invalid read of size 4
==<pid>==    at 0x...: main (dangling_uaf.c:~21)
==<pid>==  Address 0x... is 0 bytes inside a block of size 4 free'd
==<pid>==    at 0x...: free
==<pid>==    at 0x...: main (dangling_uaf.c:~13)
...
==<pid>== Invalid write of size 4
==<pid>==    at 0x...: main (dangling_uaf.c:~25)
```

Valgrind detecta tanto la lectura como la escritura invalida. Reporta:

- El tamano del acceso (4 bytes = `sizeof(int)`)
- La linea exacta del acceso invalido
- Donde se libero el bloque y su tamano original
- El resumen final con el conteo de errores

### Limpieza de la parte 1

```bash
rm -f dangling_uaf
```

---

## Parte 2 — Dangling pointer: retornar direccion de variable local

**Objetivo**: Observar que ocurre cuando una funcion retorna un puntero a una
variable local cuyo stack frame ya fue destruido.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat dangling_local.c
```

Observa:

- `return_local_address()` retorna `&x`, donde `x` es una variable local
- `overwrite_stack()` crea un array local que ocupa el mismo espacio del stack
- `main()` usa el puntero antes y despues de llamar a `overwrite_stack()`

Antes de compilar, predice:

- El compilador producira un warning sobre retornar `&x`?
- El valor `*p` sera 42 antes de la segunda llamada?
- El valor `*p` cambiara despues de llamar a `overwrite_stack()`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.2 — Compilar y observar el warning

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic dangling_local.c -o dangling_local
```

Salida esperada:

```
dangling_local.c: In function 'return_local_address':
dangling_local.c: warning: function returns address of local variable [-Wreturn-local-addr]
```

El compilador detecta el patron con `-Wreturn-local-addr` (incluido en `-Wall`).
Este warning es claro: la variable local `x` deja de existir cuando la funcion
retorna.

### Paso 2.3 — Ejecutar y observar el UB

```bash
./dangling_local
```

Salida esperada (puede variar -- es UB):

```
Before another call: *p = 42  (UB -- may still show 42)
After another call:  *p = 170  (UB -- stack was overwritten)
```

Observa:

- La primera lectura puede mostrar 42 porque el stack aun no fue reutilizado.
  Esto es lo peligroso: el bug parece no existir
- Despues de llamar a `overwrite_stack()`, esa funcion uso el mismo espacio
  del stack para su array local `{0xAA, 0xBB, 0xCC, 0xDD}`. El valor 170
  es `0xAA` en decimal -- el stack fue sobrescrito
- El valor exacto depende del layout del stack del compilador. Puede ser
  cualquiera de `0xAA` (170), `0xBB` (187), `0xCC` (204), `0xDD` (221),
  o algo completamente diferente

### Paso 2.4 — Detectar con Valgrind

```bash
valgrind ./dangling_local
```

Observa la salida de Valgrind. En muchos casos, Valgrind no reporta error aqui
porque el acceso es a memoria del stack del proceso (no al heap). Valgrind es
mas efectivo detectando errores de heap (malloc/free) que de stack.

Esto hace que los dangling pointers a variables locales sean especialmente
peligrosos: pueden pasar desapercibidos tanto en ejecucion normal como con
Valgrind. La mejor defensa es el warning del compilador.

### Limpieza de la parte 2

```bash
rm -f dangling_local
```

---

## Parte 3 — Wild pointer: puntero no inicializado

**Objetivo**: Observar el comportamiento de un puntero no inicializado y como
el compilador lo detecta.

### Paso 3.1 — Examinar el codigo fuente

```bash
cat wild_pointer.c
```

Observa:

- `int *p;` se declara sin inicializar -- contiene basura
- Se imprime el valor del puntero (la direccion basura)
- La desreferencia esta comentada (descomentarla causaria un crash o
  corrupcion silenciosa)
- Luego se muestra la alternativa segura: inicializar a NULL

Antes de compilar, predice:

- El compilador detectara que `p` se usa sin inicializar?
- Que flag sera responsable del warning?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.2 — Compilar y observar el warning

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic wild_pointer.c -o wild_pointer
```

Salida esperada:

```
wild_pointer.c: In function 'main':
wild_pointer.c: warning: 'p' is used uninitialized [-Wuninitialized]
```

El flag `-Wuninitialized` (incluido en `-Wall`) detecta que `p` se usa antes
de recibir un valor. Sin embargo, este analisis no es perfecto -- en flujos
de control complejos (con ifs, loops), el compilador puede no detectar todos
los casos.

### Paso 3.3 — Ejecutar

```bash
./wild_pointer
```

Salida esperada:

```
Wild pointer value: p = 0x<addr>  (garbage address)
Initialized to NULL: safe = (nil)
```

Observa:

- `p` contiene una direccion aleatoria. Si la desreferencias, puede pasar
  cualquier cosa: segfault, corrupcion de datos, o funcionar (el peor caso)
- `safe` es `(nil)` (NULL). Si la desreferencias, siempre da segfault, lo que
  hace el bug inmediatamente detectable

### Paso 3.4 — Diferencia entre wild pointer y NULL

La ventaja de inicializar a NULL es la predictibilidad:

| Tipo | Desreferenciar | Resultado |
|------|---------------|-----------|
| Wild pointer | `*p = 42;` | Impredecible: crash, corrupcion, o "funciona" |
| NULL pointer | `*p = 42;` | Segfault garantizado (detectable) |

Un segfault es mejor que corrupcion silenciosa. Por eso la regla es: si no
sabes a donde apuntar, inicializa a NULL.

### Limpieza de la parte 3

```bash
rm -f wild_pointer
```

---

## Parte 4 — Prevencion: patron SAFE_FREE y verificacion

**Objetivo**: Implementar y probar el patron `SAFE_FREE` que combina `free()`
con la asignacion a NULL, y verificar que el puntero queda en un estado
detectable despues de liberar.

### Paso 4.1 — Examinar el codigo fuente

```bash
cat safe_free.c
```

Observa:

- El macro `SAFE_FREE(p)` hace `free(p); (p) = NULL;` en una sola operacion
- `use_pointer()` verifica si el puntero es NULL antes de desreferenciarlo
- Se comparan dos escenarios: `free()` directo vs `SAFE_FREE()`

Antes de ejecutar, predice:

- Despues de `free(a)` sin SAFE_FREE, `a` sera NULL o conservara la direccion?
- Despues de `SAFE_FREE(b)`, `b` sera NULL?
- Que pasa si llamas `SAFE_FREE` dos veces sobre el mismo puntero?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.2 — Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic safe_free.c -o safe_free
```

Salida esperada (un warning esperado):

```
safe_free.c: warning: pointer 'a' used after 'free' [-Wuse-after-free]
```

El warning solo aparece para `a` (el caso sin SAFE_FREE), porque se imprime
su valor despues de `free()`. Para `b` no hay warning: despues de `SAFE_FREE`,
el puntero es NULL y se verifica antes de usar.

### Paso 4.3 — Ejecutar y verificar

```bash
./safe_free
```

Salida esperada:

```
=== Without SAFE_FREE ===
Before free:
  *p = 42
After free (dangling, UB if dereferenced):
  a = 0x<addr>  (non-NULL, but invalid)

=== With SAFE_FREE ===
Before SAFE_FREE:
  *p = 99
After SAFE_FREE:
  b = (nil)  (NULL -- safe)
  Pointer is NULL -- safe to detect, no silent corruption.

Second SAFE_FREE (idempotent):
  b = (nil)  (still NULL, no crash)
```

Observa:

- Sin SAFE_FREE: `a` retiene la direccion despues de `free()`. Cualquier
  acceso a `*a` seria UB silencioso
- Con SAFE_FREE: `b` es NULL. La funcion `use_pointer()` lo detecta y
  evita la desreferencia
- `SAFE_FREE` es idempotente: `free(NULL)` es un no-op definido por el
  estandar, asi que llamarlo dos veces es seguro

### Paso 4.4 — Detectar con Valgrind la version sin SAFE_FREE

```bash
valgrind ./safe_free
```

Observa que Valgrind reporta el acceso a `a` despues de `free()` como
"Invalid read". Para `b`, no hay errores porque `SAFE_FREE` puso el puntero
a NULL y `use_pointer()` verifica antes de desreferenciar.

### Paso 4.5 — Resumen de herramientas de deteccion

| Herramienta | Que detecta | Cuando |
|-------------|-------------|--------|
| `-Wall -Wextra` | Use-after-free, retornar local, no inicializado | Compilacion |
| Valgrind | Use-after-free en heap, lecturas no inicializadas | Runtime |
| ASan (`-fsanitize=address`) | Use-after-free, stack-use-after-return | Runtime |

Nota: ASan requiere que `libasan` este instalada. Si no esta disponible,
Valgrind cubre la mayoria de los casos de heap.

### Limpieza de la parte 4

```bash
rm -f safe_free
```

---

## Limpieza final

```bash
rm -f dangling_uaf dangling_local wild_pointer safe_free
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  dangling_local.c  dangling_uaf.c  safe_free.c  wild_pointer.c
```

---

## Conceptos reforzados

1. Un dangling pointer apunta a memoria que fue liberada o ya no es valida.
   Desreferenciarlo es UB: puede funcionar, devolver basura, o crashear. El
   caso mas peligroso es cuando "funciona" porque el bug pasa desapercibido.

2. `free()` no modifica el valor del puntero -- sigue conteniendo la direccion
   anterior. Por eso es posible hacer use-after-free sin que el programa
   crashee inmediatamente.

3. Retornar la direccion de una variable local crea un dangling pointer porque
   el stack frame se destruye al retornar. El compilador lo detecta con
   `-Wreturn-local-addr`, pero Valgrind generalmente no lo detecta porque es
   acceso a memoria del stack, no del heap.

4. Un wild pointer es un puntero no inicializado que contiene una direccion
   aleatoria. Desreferenciarlo puede corromper memoria de forma silenciosa.
   Inicializar a NULL convierte el bug en un segfault determinista y detectable.

5. El macro `SAFE_FREE(p)` combina `free(p)` con `p = NULL` para eliminar
   dangling pointers inmediatamente despues de liberar. Es idempotente porque
   `free(NULL)` es un no-op segun el estandar C.

6. Las herramientas de deteccion se complementan: el compilador detecta
   patrones en codigo estatico (`-Wall`), Valgrind detecta accesos invalidos
   al heap en runtime, y ASan detecta use-after-free y stack-use-after-return
   con instrumentacion del compilador.
