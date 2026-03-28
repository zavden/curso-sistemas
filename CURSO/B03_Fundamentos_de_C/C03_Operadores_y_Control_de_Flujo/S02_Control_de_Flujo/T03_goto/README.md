# T03 — goto

## Qué es goto

`goto` salta incondicionalmente a un label dentro de la misma
función. Es el mecanismo de control de flujo más primitivo de C:

```c
goto label_name;

// ...

label_name:
    // ejecución continúa aquí
```

```c
#include <stdio.h>

int main(void) {
    printf("A\n");
    goto skip;
    printf("B\n");      // nunca se ejecuta
skip:
    printf("C\n");
    return 0;
}
// Imprime: A, C
```

### Restricciones

```c
// 1. Solo puede saltar dentro de la MISMA función:
void foo(void) {
    goto bar_label;     // ERROR: bar_label está en otra función
}
void bar(void) {
bar_label:
    return;
}

// 2. Los labels tienen function scope (visibles en toda la función):
void example(void) {
    {
        // label definido dentro de un bloque...
        inner:
        printf("inner\n");
    }
    goto inner;    // ...pero visible en toda la función
}

// 3. No se puede saltar al interior de un VLA o bloque
//    que declara variables con inicialización (desde C99):
// goto skip;
// int arr[n];        // skip no puede saltar sobre esto
// skip:
```

## Cuándo goto es legítimo — Cleanup en C

El uso legítimo más importante de goto en C es la **limpieza
de recursos al salir de una función**. Este patrón es usado
extensivamente en el kernel de Linux:

### El problema

```c
// Sin goto — cleanup anidado (feo y propenso a errores):
int process_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        return -1;
    }

    char *buffer = malloc(1024);
    if (buffer == NULL) {
        fclose(f);              // cerrar f antes de salir
        return -1;
    }

    int *data = malloc(100 * sizeof(int));
    if (data == NULL) {
        free(buffer);           // liberar buffer
        fclose(f);              // Y cerrar f
        return -1;
    }

    // ... usar f, buffer, data ...

    if (parse_error) {
        free(data);             // liberar data
        free(buffer);           // Y buffer
        fclose(f);              // Y cerrar f
        return -1;
    }

    // ... más código ...

    free(data);                 // cleanup normal
    free(buffer);
    fclose(f);
    return 0;
}

// Cada punto de error necesita repetir el cleanup.
// Es fácil olvidar uno. Es difícil de mantener.
```

### La solución con goto

```c
// Con goto — cleanup centralizado (limpio y mantenible):
int process_file(const char *path) {
    int ret = -1;
    FILE *f = NULL;
    char *buffer = NULL;
    int *data = NULL;

    f = fopen(path, "r");
    if (f == NULL) {
        goto cleanup;
    }

    buffer = malloc(1024);
    if (buffer == NULL) {
        goto cleanup;
    }

    data = malloc(100 * sizeof(int));
    if (data == NULL) {
        goto cleanup;
    }

    // ... usar f, buffer, data ...

    if (parse_error) {
        goto cleanup;
    }

    // ... más código ...

    ret = 0;    // éxito

cleanup:
    free(data);       // free(NULL) es seguro (no hace nada)
    free(buffer);     // free(NULL) es seguro
    if (f != NULL) {
        fclose(f);
    }
    return ret;
}
```

```c
// Ventajas del patrón goto-cleanup:
// - Un solo punto de limpieza (DRY)
// - Imposible olvidar un recurso
// - Fácil agregar nuevos recursos
// - free(NULL) es un no-op — no necesita verificación
// - fclose(NULL) SÍ es UB — necesita verificación
```

### Variante con múltiples labels

```c
// Cuando los recursos se adquieren en orden y necesitan
// liberarse en orden inverso:

int init_system(void) {
    if (init_a() < 0) {
        goto fail_a;
    }
    if (init_b() < 0) {
        goto fail_b;
    }
    if (init_c() < 0) {
        goto fail_c;
    }

    return 0;    // éxito — todo inicializado

fail_c:
    cleanup_b();
fail_b:
    cleanup_a();
fail_a:
    return -1;
}

// Este patrón es muy común en drivers del kernel de Linux.
// Los labels van de abajo hacia arriba, deshaciendo en orden inverso.
```

### Ejemplo del kernel de Linux

```c
// Patrón real simplificado del kernel:
static int my_driver_init(void) {
    int err;

    err = alloc_resources();
    if (err)
        goto err_alloc;

    err = register_device();
    if (err)
        goto err_register;

    err = create_sysfs();
    if (err)
        goto err_sysfs;

    return 0;

err_sysfs:
    unregister_device();
err_register:
    free_resources();
err_alloc:
    return err;
}
```

## Cuándo NO usar goto

### Saltar hacia atrás (loop improvisado)

```c
// NUNCA usar goto para crear loops:
int i = 0;
loop_start:
    printf("%d\n", i);
    i++;
    if (i < 10)
        goto loop_start;    // NO — usar for/while

// Usar for:
for (int i = 0; i < 10; i++) {
    printf("%d\n", i);
}
```

### Saltar entre secciones de código

```c
// NUNCA usar goto como "jump table" improvisado:
if (condition_a) goto handle_a;
if (condition_b) goto handle_b;
goto handle_default;

handle_a:
    // ...
    goto done;
handle_b:
    // ...
    goto done;
handle_default:
    // ...
done:
    return;

// Usar switch o if/else:
if (condition_a) {
    // ...
} else if (condition_b) {
    // ...
} else {
    // ...
}
```

### Salir de loops profundamente anidados

```c
// Este es un caso DEBATIBLE.
// goto puede ser más claro que flags:

// Con flag:
int found = 0;
for (int i = 0; i < rows && !found; i++) {
    for (int j = 0; j < cols && !found; j++) {
        if (matrix[i][j] == target) {
            found = 1;
        }
    }
}

// Con goto:
for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
        if (matrix[i][j] == target) {
            goto found;
        }
    }
}
found:
// ...

// Con función (la mejor opción generalmente):
int find_in_matrix(int matrix[][COLS], int rows, int target) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < COLS; j++) {
            if (matrix[i][j] == target) {
                return 1;    // return sale de todo
            }
        }
    }
    return 0;
}
```

## Reglas de estilo para goto

```c
// 1. Solo saltar HACIA ADELANTE (nunca hacia atrás):
goto cleanup;    // OK: salta adelante hacia el cleanup
// goto retry;   // NO: salta hacia atrás (es un loop disfrazado)

// 2. Solo para cleanup/error handling:
if (error) goto cleanup;    // OK: patrón de cleanup

// 3. Labels descriptivos:
cleanup:        // OK
error_cleanup:  // OK
out:            // OK (estilo kernel)
err_alloc:      // OK (estilo kernel)
// loop:        // NO: indica un loop, usar for/while

// 4. Un solo label de salida cuando sea posible:
// Mejor un solo cleanup: que cleanup1:, cleanup2:, cleanup3:
// (excepto el patrón de undo en orden inverso)
```

## Comparación con Rust

```c
// C no tiene destructores automáticos.
// En C, el programador es responsable de liberar recursos.
// goto es la herramienta para centralizar esa responsabilidad.

// Rust resuelve esto con RAII (Resource Acquisition Is Initialization):
// Los recursos se liberan automáticamente al salir del scope.
// No necesita goto porque el compilador genera el cleanup.

// C (con goto):
int process(void) {
    int *data = malloc(100);
    if (!data) return -1;

    if (error) goto cleanup;
    // ...

cleanup:
    free(data);
    return -1;
}

// Rust (equivalente):
// fn process() -> Result<(), Error> {
//     let data = vec![0; 100];  // se libera automáticamente
//     if error { return Err(...); }  // data se libera aquí
//     // ...
//     Ok(())
// }  // data se libera aquí también

// C no puede hacer esto porque no tiene:
// - Destructores
// - RAII
// - Borrow checker
// goto es la alternativa pragmática.
```

## __attribute__((cleanup)) — RAII en GCC

```c
// GCC tiene una extensión que simula RAII:
void free_ptr(void *p) {
    free(*(void **)p);
}

void process(void) {
    __attribute__((cleanup(free_ptr))) int *data = malloc(100);

    if (error) return;    // data se libera automáticamente
    // ...
}   // data se libera automáticamente

// Esto es una extensión GNU, no C estándar.
// El kernel de Linux define macros para esto:
// __cleanup(func) = __attribute__((cleanup(func)))
//
// No es portable, pero muestra que incluso en C
// se buscan alternativas a goto para cleanup.
```

## Resumen

```
Usar goto para:
  ✓ Cleanup centralizado de recursos
  ✓ Error handling con múltiples puntos de fallo
  ✓ Salir de loops profundamente anidados (debatible)

NO usar goto para:
  ✗ Loops (usar for, while, do-while)
  ✗ Condicionales (usar if/else, switch)
  ✗ Saltar hacia atrás
  ✗ Saltar entre funciones

Alternativas:
  - return (salir de la función)
  - break (salir de un loop)
  - Extraer a función (return sale de loops anidados)
  - __attribute__((cleanup)) (extensión GCC)
  - Rust: RAII con Drop trait
```

---

## Ejercicios

### Ejercicio 1 — Cleanup con goto

```c
// Escribir una función que abra dos archivos,
// alloce un buffer con malloc, haga alguna operación,
// y use goto para centralizar el cleanup.
// Manejar errores en cada paso.
```

### Ejercicio 2 — Refactorizar sin goto

```c
// Refactorizar este código para que NO use goto.
// ¿Es más claro sin goto o con goto?

int init(void) {
    int *a = malloc(100);
    if (!a) goto fail;

    int *b = malloc(200);
    if (!b) goto fail_a;

    int *c = malloc(300);
    if (!c) goto fail_b;

    // usar a, b, c...

    free(c);
    free(b);
    free(a);
    return 0;

fail_b:
    free(b);
fail_a:
    free(a);
fail:
    return -1;
}
```

### Ejercicio 3 — Salir de loops anidados

```c
// Buscar un valor en una matriz 2D.
// Implementar tres versiones:
// 1. Con goto
// 2. Con una variable flag
// 3. Extrayendo a una función con return
// ¿Cuál prefieres?
```
