# T02 — Valgrind

## Qué es Valgrind

Valgrind es un framework de instrumentación que ejecuta tu programa
en una CPU virtual, interceptando cada acceso a memoria. Su
herramienta principal, **Memcheck**, detecta errores de memoria
que el compilador no puede ver:

```bash
# Compilar con -g (debug info) y sin optimización:
gcc -g -O0 main.c -o main

# Ejecutar bajo Valgrind:
valgrind ./main

# Valgrind ejecuta el programa ~20-30x más lento
# pero detecta errores que de otra forma son invisibles
```

```
Valgrind detecta:
- Lecturas/escrituras fuera de límites (heap)
- Uso de memoria sin inicializar
- Memory leaks (malloc sin free)
- Double free
- Use-after-free
- Mismatched alloc/dealloc (malloc/delete, new/free)
- Overlapping memcpy

Valgrind NO detecta:
- Errores en stack (buffer overflow en arrays locales)
  → para eso usar AddressSanitizer
- Errores en tiempo de compilación
- Errores lógicos
```

## Memcheck — La herramienta principal

### Acceso inválido a memoria

```c
// invalid_read.c
#include <stdlib.h>
#include <stdio.h>

int main(void) {
    int *arr = malloc(5 * sizeof(int));
    for (int i = 0; i < 5; i++) {
        arr[i] = i * 10;
    }

    printf("%d\n", arr[5]);    // lectura fuera de límites

    free(arr);
    return 0;
}
```

```bash
valgrind ./invalid_read
```

```
==12345== Invalid read of size 4
==12345==    at 0x401180: main (invalid_read.c:10)
==12345==  Address 0x4a47054 is 0 bytes after a block of size 20 alloc'd
==12345==    at 0x4843828: malloc (vg_replace_malloc.c:381)
==12345==    by 0x401150: main (invalid_read.c:5)
```

```
Cómo leer el reporte:
- "Invalid read of size 4" → leyó 4 bytes de memoria inválida
- "at 0x401180: main (invalid_read.c:10)" → ocurrió en la línea 10
- "0 bytes after a block of size 20" → justo después del bloque
  de 20 bytes (5 ints × 4 bytes)
- El stack trace muestra dónde se asignó la memoria (línea 5)
```

### Escritura fuera de límites

```c
// invalid_write.c
#include <stdlib.h>

int main(void) {
    int *arr = malloc(3 * sizeof(int));
    arr[3] = 42;               // escritura fuera de límites
    free(arr);
    return 0;
}
```

```
==12345== Invalid write of size 4
==12345==    at 0x401160: main (invalid_write.c:5)
==12345==  Address 0x4a4704c is 0 bytes after a block of size 12 alloc'd
```

### Uso de memoria sin inicializar

```c
// uninit.c
#include <stdlib.h>
#include <stdio.h>

int main(void) {
    int *arr = malloc(5 * sizeof(int));
    // No inicializamos arr[2]
    arr[0] = 10;
    arr[1] = 20;

    if (arr[2] > 0) {          // uso de valor sin inicializar
        printf("positive\n");
    }

    free(arr);
    return 0;
}
```

```
==12345== Conditional jump or move depends on uninitialised value(s)
==12345==    at 0x401170: main (uninit.c:9)
```

```
"Conditional jump depends on uninitialised value" significa
que una decisión (if, while, etc.) depende de memoria que
nunca fue escrita. El resultado es impredecible.

Valgrind distingue:
- El valor fue allocado con malloc (no inicializado)
- vs allocado con calloc (inicializado a cero)
```

## Memory leaks

### Detección de leaks

```c
// leak.c
#include <stdlib.h>

void create_data(void) {
    int *p = malloc(100 * sizeof(int));
    // se pierde p al salir de la función — nunca se hace free
}

int main(void) {
    create_data();
    create_data();
    create_data();

    int *q = malloc(50 * sizeof(int));
    // q tampoco se libera

    return 0;
}
```

```bash
valgrind --leak-check=full ./leak
```

```
==12345== HEAP SUMMARY:
==12345==     in use at exit: 1,400 bytes in 4 blocks
==12345==   total heap usage: 4 allocs, 0 frees, 1,400 bytes allocated
==12345==
==12345== 200 bytes in 1 blocks are definitely lost in loss record 1 of 2
==12345==    at 0x4843828: malloc (vg_replace_malloc.c:381)
==12345==    by 0x401190: main (leak.c:10)
==12345==
==12345== 1,200 bytes in 3 blocks are definitely lost in loss record 2 of 2
==12345==    at 0x4843828: malloc (vg_replace_malloc.c:381)
==12345==    by 0x401150: create_data (leak.c:4)
==12345==    by 0x401170: main (leak.c:7)
==12345==
==12345== LEAK SUMMARY:
==12345==    definitely lost: 1,400 bytes in 4 blocks
==12345==    indirectly lost: 0 bytes in 0 blocks
==12345==      possibly lost: 0 bytes in 0 blocks
==12345==    still reachable: 0 bytes in 0 blocks
==12345==         suppressed: 0 bytes in 0 blocks
```

### Categorías de leaks

```
definitely lost:
  Memoria que nadie puede liberar. No hay ningún puntero
  apuntando a ella. Es un leak real.

indirectly lost:
  Memoria apuntada por un bloque que está definitely lost.
  Si liberas el bloque padre, estos se liberarían también.
  Ejemplo: una lista enlazada donde se pierde el nodo raíz.

possibly lost:
  Hay un puntero que apunta al interior del bloque (no al
  inicio). Puede ser un leak o puede ser intencional.

still reachable:
  Hay punteros válidos a esta memoria, pero nunca se hizo free.
  Común en globales que se liberan al salir del programa.
  Generalmente no es un problema.
```

```bash
# Opciones de leak-check:
valgrind --leak-check=full ./main      # reporte detallado (recomendado)
valgrind --leak-check=summary ./main   # solo el resumen
valgrind --leak-check=no ./main        # no verificar leaks

# Mostrar dónde se alloqueó cada bloque perdido:
valgrind --leak-check=full --track-origins=yes ./main

# Mostrar leaks del tipo "still reachable":
valgrind --leak-check=full --show-reachable=yes ./main
```

## Double free y use-after-free

### Double free

```c
// double_free.c
#include <stdlib.h>

int main(void) {
    int *p = malloc(sizeof(int));
    *p = 42;
    free(p);
    free(p);       // double free
    return 0;
}
```

```
==12345== Invalid free() / delete / delete[] / realloc()
==12345==    at 0x484412F: free (vg_replace_malloc.c:872)
==12345==    by 0x401180: main (double_free.c:7)
==12345==  Address 0x4a47040 is 0 bytes inside a block of size 4 free'd
==12345==    at 0x484412F: free (vg_replace_malloc.c:872)
==12345==    by 0x401170: main (double_free.c:6)
==12345==  Block was alloc'd at
==12345==    at 0x4843828: malloc (vg_replace_malloc.c:381)
==12345==    by 0x401150: main (double_free.c:4)
```

```
El reporte muestra:
1. Dónde ocurrió el free inválido (línea 7)
2. Dónde se hizo el primer free (línea 6)
3. Dónde se allocó la memoria (línea 4)

Tres stack traces en un solo error — toda la información
para entender el problema.
```

### Use-after-free

```c
// use_after_free.c
#include <stdlib.h>
#include <stdio.h>

int main(void) {
    int *p = malloc(10 * sizeof(int));
    p[0] = 42;
    free(p);

    printf("%d\n", p[0]);     // use-after-free
    return 0;
}
```

```
==12345== Invalid read of size 4
==12345==    at 0x401180: main (use_after_free.c:9)
==12345==  Address 0x4a47040 is 0 bytes inside a block of size 40 free'd
==12345==    at 0x484412F: free (vg_replace_malloc.c:872)
==12345==    by 0x401170: main (use_after_free.c:7)
==12345==  Block was alloc'd at
==12345==    at 0x4843828: malloc (vg_replace_malloc.c:381)
==12345==    by 0x401150: main (use_after_free.c:5)
```

## Opciones útiles de Valgrind

```bash
# Reporte completo con orígenes de valores sin inicializar:
valgrind --leak-check=full --track-origins=yes ./main

# Generar suppressions (para falsos positivos de bibliotecas):
valgrind --gen-suppressions=all ./main

# Usar un archivo de suppressions:
valgrind --suppressions=my.supp ./main

# Logging a archivo:
valgrind --log-file=valgrind.log ./main

# Limitar la profundidad del stack trace:
valgrind --num-callers=20 ./main   # default: 12
```

```bash
# Verificar todos los accesos a memoria, no solo los de heap:
valgrind --track-origins=yes ./main

# track-origins rastrea de dónde viene cada byte sin inicializar.
# Es más lento, pero muestra exactamente qué malloc/variable
# produjo el valor problemático.
```

## Valgrind con GDB

Valgrind puede integrarse con GDB para depurar errores de
memoria interactivamente:

```bash
# Terminal 1 — Iniciar Valgrind en modo servidor de GDB:
valgrind --vgdb=yes --vgdb-error=0 ./main

# Terminal 2 — Conectar GDB:
gdb ./main
(gdb) target remote | vgdb

# Ahora puedes usar GDB normalmente, pero con la detección
# de errores de Valgrind. Cuando Valgrind detecta un error,
# GDB para en ese punto.
```

```bash
# --vgdb-error=0  → parar en el primer error
# --vgdb-error=1  → parar después del primer error
# --vgdb-error=N  → parar después de N errores
```

## Ejemplo completo

```c
// buggy.c — programa con múltiples errores de memoria
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char *make_greeting(const char *name) {
    // Bug 1: strlen no cuenta el '\0'
    char *greeting = malloc(strlen(name) + 6);  // "Hello " = 6 chars
    // Debería ser: strlen(name) + 6 + 1

    sprintf(greeting, "Hello %s", name);  // escribe 1 byte de más
    return greeting;
}

void process(void) {
    char *msg = make_greeting("World");
    printf("%s\n", msg);

    // Bug 2: olvidamos free(msg) — memory leak
}

int main(void) {
    process();

    int *data = malloc(5 * sizeof(int));
    // Bug 3: usamos data sin inicializar
    int sum = 0;
    for (int i = 0; i < 5; i++) {
        sum += data[i];
    }
    printf("Sum: %d\n", sum);

    free(data);
    free(data);  // Bug 4: double free

    return 0;
}
```

```bash
gcc -g -O0 buggy.c -o buggy
valgrind --leak-check=full --track-origins=yes ./buggy
```

```
# Valgrind reportará (en este orden de ejecución):

# 1. Invalid write en sprintf (1 byte más allá del bloque)
# 2. Uso de valores sin inicializar (data[0..4] en el loop)
# 3. Invalid free (el double free)
# 4. Memory leak (el greeting que no se liberó)

# Cada error con stack traces completos mostrando:
#   - Dónde ocurrió
#   - Dónde se allocó la memoria
#   - (para errores de free) Dónde se liberó antes
```

## Cuándo usar Valgrind

```
Usar Valgrind cuando:
- Necesitas detectar memory leaks
- No puedes recompilar (tienes solo el binario + debug info)
- Necesitas un reporte detallado con stack traces de alloc/free
- Quieres verificar un programa existente sin modificarlo

NO usar Valgrind cuando:
- Necesitas detectar errores en el stack (usar ASan)
- La velocidad es importante (Valgrind es 20-30x más lento)
- El programa usa threads intensivamente (ASan es mejor)
- Necesitas detección en producción (los sanitizers tienen
  menos overhead — ~2x en lugar de ~20x)
```

## Tabla de mensajes de Valgrind

| Mensaje | Significado | Causa típica |
|---|---|---|
| Invalid read of size N | Lectura fuera de límites | Índice de array incorrecto |
| Invalid write of size N | Escritura fuera de límites | Buffer overflow |
| Conditional jump on uninitialised | Decisión con valor sin inicializar | malloc sin inicializar |
| Invalid free | Free de memoria ya liberada o no allocada | Double free |
| X bytes in Y blocks are definitely lost | Memory leak confirmado | malloc sin free |
| X bytes are indirectly lost | Leak por leak transitivo | Lista enlazada perdida |
| Source and destination overlap | memcpy con regiones superpuestas | Usar memmove |
| Mismatched free | Tipo de free incorrecto | malloc + delete (C++) |

---

## Ejercicios

### Ejercicio 1 — Detectar leaks

```c
// Este programa tiene memory leaks.
// Ejecutar con valgrind --leak-check=full
// ¿Cuántos bytes se pierden? ¿Dónde?

#include <stdlib.h>
#include <string.h>

char *duplicate(const char *s) {
    char *copy = malloc(strlen(s) + 1);
    strcpy(copy, s);
    return copy;
}

int main(void) {
    char *a = duplicate("hello");
    char *b = duplicate("world");
    char *c = duplicate("test");

    free(a);
    // b y c no se liberan
    return 0;
}
```

### Ejercicio 2 — Encontrar el bug

```c
// Este programa crashea a veces. Usar Valgrind para
// encontrar el error exacto.

#include <stdlib.h>
#include <stdio.h>

int main(void) {
    int *arr = malloc(10 * sizeof(int));
    for (int i = 0; i <= 10; i++) {
        arr[i] = i;
    }
    for (int i = 0; i < 10; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
    free(arr);
    return 0;
}
```

### Ejercicio 3 — Programa limpio

```bash
# Escribir un programa que:
# 1. Alloce un array de 100 ints con malloc
# 2. Lo llene con valores
# 3. Encuentre el máximo
# 4. Lo libere
# Ejecutar con valgrind --leak-check=full
# El objetivo: 0 errors, 0 bytes lost
```
