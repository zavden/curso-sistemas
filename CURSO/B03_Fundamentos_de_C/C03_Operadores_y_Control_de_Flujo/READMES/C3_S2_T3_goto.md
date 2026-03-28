# T03 — goto

> **Fuentes**: `README.md`, `LABS.md`, `labs/README.md`, 4 archivos `.c` de laboratorio.
> Aplicada **Regla 3** (no existe `.max.md`).

---

Sin erratas detectadas en el material fuente.

---

## 1. Qué es `goto`

`goto` transfiere el control incondicionalmente a un **label** dentro de la
misma función. Es el mecanismo de salto más primitivo de C:

```c
goto label_name;       // salta al label

// ... este código se salta ...

label_name:            // el label es un identificador seguido de ':'
    // ejecución continúa aquí
```

```c
#include <stdio.h>

int main(void) {
    printf("A\n");
    goto skip;
    printf("B\n");     // nunca se ejecuta
skip:
    printf("C\n");
    return 0;
}
// Imprime: A, C
```

### Restricciones del estándar

**1. Solo dentro de la misma función** — no se puede saltar a un label de otra función:

```c
void foo(void) {
    goto bar_label;      // ERROR de compilación
}
void bar(void) {
bar_label:
    return;
}
```

**2. Los labels tienen function scope** — son visibles en toda la función,
sin importar en qué bloque `{ }` se definan. Esto es diferente de las variables
(que tienen block scope):

```c
void example(void) {
    {
        inner:                     // definido dentro de un bloque
        printf("inner\n");
    }
    goto inner;     // OK: el label es visible en toda la función
}
```

**3. No se puede saltar sobre declaraciones de VLA** (C99+):

```c
goto skip;
int arr[n];        // ERROR: goto no puede saltar sobre la declaración de un VLA
skip:
```

El estándar prohíbe esto porque el VLA requiere que se ejecute su declaración
para reservar espacio en el stack. Saltar sobre ella dejaría un estado inconsistente.

---

## 2. El patrón goto-cleanup — uso legítimo

El uso más importante (y a menudo el único legítimo) de `goto` en C moderno
es la **limpieza centralizada de recursos**. Este patrón es estándar en el
kernel de Linux.

### El problema: cleanup repetido

Sin `goto`, cada punto de error necesita repetir toda la limpieza de los
recursos adquiridos hasta ese momento:

```c
int process_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (f == NULL) return -1;

    char *buf = malloc(1024);
    if (buf == NULL) {
        fclose(f);               // cleanup: f
        return -1;
    }

    int *data = malloc(100 * sizeof(int));
    if (data == NULL) {
        free(buf);               // cleanup: buf
        fclose(f);               // cleanup: f
        return -1;
    }

    if (some_error) {
        free(data);              // cleanup: data
        free(buf);               // cleanup: buf
        fclose(f);               // cleanup: f
        return -1;
    }

    // ... camino feliz ...

    free(data);                  // cleanup normal (duplicado)
    free(buf);
    fclose(f);
    return 0;
}
```

Cada nuevo recurso agrega una línea a **todos** los puntos de error anteriores.
Con 5 recursos y 4 puntos de error, son 20 líneas de cleanup repartidas por
toda la función. Es fácil olvidar una.

### La solución: cleanup centralizado

```c
int process_file(const char *path) {
    int ret = -1;               // asume error por defecto
    FILE *f = NULL;             // CLAVE: inicializar todo a NULL
    char *buf = NULL;
    int *data = NULL;

    f = fopen(path, "r");
    if (f == NULL) goto cleanup;

    buf = malloc(1024);
    if (buf == NULL) goto cleanup;

    data = malloc(100 * sizeof(int));
    if (data == NULL) goto cleanup;

    if (some_error) goto cleanup;

    // ... usar f, buf, data ...

    ret = 0;                    // éxito: sobreescribe el -1 por defecto

cleanup:
    free(data);                 // free(NULL) es un no-op seguro
    free(buf);                  // free(NULL) es un no-op seguro
    if (f != NULL) fclose(f);   // fclose(NULL) es UB — verificar
    return ret;
}
```

Por qué funciona:

- Todas las variables se inicializan a `NULL` al inicio.
- `free(NULL)` es un no-op definido por el estándar (C99 §7.20.3.2).
- No importa en qué paso falle: el mismo bloque cleanup libera solo los recursos
  que fueron adquiridos (los que no se adquirieron siguen en `NULL`).
- **Punto crítico**: `fclose(NULL)` es **UB** — siempre verificar `f != NULL`.

### Variante con múltiples labels (undo inverso)

Cuando los recursos se adquieren en secuencia y deben liberarse en orden inverso:

```c
int init_system(void) {
    if (init_a() < 0)
        goto fail_a;
    if (init_b() < 0)
        goto fail_b;
    if (init_c() < 0)
        goto fail_c;

    return 0;         // éxito: todo inicializado

fail_c:               // si init_c falló, deshace b y a
    cleanup_b();
fail_b:               // si init_b falló, deshace solo a
    cleanup_a();
fail_a:               // si init_a falló, no hay nada que deshacer
    return -1;
}
```

Los labels aprovechan el **fall-through**: `fail_c` ejecuta `cleanup_b()` y luego
cae a `fail_b` que ejecuta `cleanup_a()`. Mismo principio que el fall-through
de `switch`, pero aplicado a cleanup. Este patrón es muy común en drivers del
kernel de Linux.

### Ejemplo estilo kernel de Linux

```c
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

Convenciones del kernel:
- Labels con prefijo `err_` describiendo qué falló.
- Los labels se apilan de abajo hacia arriba, deshaciendo en orden inverso.
- Se retorna el código de error original (`err`), no un -1 genérico.

---

## 3. Cuándo NO usar `goto`

### Nunca para crear loops

```c
// MAL — loop disfrazado:
int i = 0;
retry:
    printf("%d\n", i);
    i++;
    if (i < 10) goto retry;

// BIEN — usar for/while:
for (int i = 0; i < 10; i++) {
    printf("%d\n", i);
}
```

### Nunca como jump table

```c
// MAL — switch disfrazado:
if (cond_a) goto handle_a;
if (cond_b) goto handle_b;
goto handle_default;

handle_a: /* ... */ goto done;
handle_b: /* ... */ goto done;
handle_default: /* ... */
done:
    return;

// BIEN — usar if/else o switch:
if (cond_a) { /* ... */ }
else if (cond_b) { /* ... */ }
else { /* ... */ }
```

### Caso debatible: salir de loops anidados

```c
// Opción 1: goto (directo pero debatible)
for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
        if (matrix[i][j] == target)
            goto found;
    }
}
printf("Not found\n");
goto done;
found:
    printf("Found!\n");
done:
    // ...

// Opción 2: flag (sin goto, pero contamina las condiciones)
int found = 0;
for (int i = 0; i < rows && !found; i++) {
    for (int j = 0; j < cols && !found; j++) {
        if (matrix[i][j] == target)
            found = 1;
    }
}

// Opción 3: función + return (generalmente la mejor)
int find_in_matrix(int mat[][COLS], int rows, int target) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < COLS; j++) {
            if (mat[i][j] == target) return 1;
        }
    }
    return 0;
}
```

La opción 3 (extraer a función) suele ser la más limpia: `return` sale de
todos los loops de golpe, y la función tiene un nombre descriptivo.

---

## 4. Reglas de estilo

```
Regla                              Ejemplo
────────────────────────────────── ──────────────────────
Solo saltar HACIA ADELANTE         goto cleanup;  ✓
                                   goto retry;    ✗ (loop disfrazado)

Solo para cleanup/error handling   if (!ptr) goto cleanup;  ✓

Labels descriptivos                cleanup:       ✓
                                   err_alloc:     ✓
                                   out:           ✓ (estilo kernel)
                                   loop:          ✗

Un solo label cuando sea posible   cleanup: (prefiere 1 sobre N)
```

---

## 5. Function scope de labels vs block scope de variables

Los labels son el **único identificador en C con function scope**. Todo lo demás
(variables, typedefs, enums) tiene block scope o file scope:

```c
void demo(void) {
    {
        int x = 42;      // block scope: solo visible dentro de { }
        inner:            // function scope: visible en TODA la función
        printf("x=%d\n", x);
    }

    // printf("%d\n", x);  // ERROR: x no existe fuera del bloque
    goto inner;            // OK: inner es visible aquí
}
```

Esto significa que no puedes tener dos labels con el mismo nombre en la misma función,
aunque estén en bloques diferentes — colisionarían.

---

## 6. Comparación con Rust — RAII vs goto

C no tiene destructores automáticos. El programador es responsable de liberar
cada recurso manualmente, y `goto` es la herramienta para centralizar esa
responsabilidad.

Rust resuelve esto con **RAII** (Resource Acquisition Is Initialization):
los recursos se liberan automáticamente al salir del scope, gracias al trait `Drop`:

```c
// C — cleanup manual con goto:
int process(void) {
    int ret = -1;
    int *data = NULL;

    data = malloc(100 * sizeof(int));
    if (!data) goto cleanup;

    // ... usar data ...

    ret = 0;
cleanup:
    free(data);
    return ret;
}
```

```rust
// Rust — RAII automático:
fn process() -> Result<(), Error> {
    let data = vec![0i32; 100];  // se libera automáticamente
    if error { return Err(...); }  // data se libera aquí (Drop)
    // ...
    Ok(())
}  // data se libera aquí también (Drop)
```

C no puede hacer esto porque no tiene destructores, RAII, ni borrow checker.
`goto` es la alternativa pragmática. La necesidad del patrón goto-cleanup
es una de las motivaciones principales para el sistema de ownership de Rust.

### `__attribute__((cleanup))` — RAII simulado en GCC

GCC ofrece una extensión no estándar que simula RAII:

```c
void free_int(int **p) { free(*p); }

void process(void) {
    __attribute__((cleanup(free_int))) int *data = malloc(100 * sizeof(int));

    if (error) return;    // data se libera automáticamente
    // ...
}   // data se libera automáticamente al salir del scope
```

- **No es C estándar** — solo funciona con GCC y Clang.
- El kernel de Linux lo usa a través de macros como `__cleanup()`.
- Muestra que incluso en C se buscan alternativas al cleanup manual.

---

## Ejercicios

### Ejercicio 1 — goto básico: predecir el flujo

```c
#include <stdio.h>

int main(void) {
    int x = 1;

    printf("A: x=%d\n", x);
    if (x > 0) goto positive;
    printf("B: negative\n");
    goto done;

positive:
    printf("C: positive\n");
    x = -x;
    printf("D: x=%d\n", x);

done:
    printf("E: done\n");
    return 0;
}
```

<details><summary>Predicción</summary>

```
A: x=1
C: positive
D: x=-1
E: done
```

- `x = 1 > 0` → salta a `positive:`, saltando "B".
- Imprime C, cambia `x` a -1, imprime D.
- La ejecución cae naturalmente a `done:` (no hay goto entre `positive:` y `done:`).
- Imprime E.
- "B: negative" nunca se ejecuta porque el `goto positive` lo salta, y no hay
  ningún camino que llegue a él sin pasar por el `goto`.

</details>

---

### Ejercicio 2 — Cleanup con un solo label

```c
// Completa la función para que use el patrón goto-cleanup.
// Debe abrir un archivo, leer una línea, e imprimir su longitud.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int read_first_line_length(const char *path) {
    int ret = -1;
    FILE *f = NULL;
    char *buf = NULL;

    f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "Cannot open '%s'\n", path);
        goto cleanup;
    }

    buf = malloc(256);
    if (buf == NULL) {
        fprintf(stderr, "malloc failed\n");
        goto cleanup;
    }

    if (fgets(buf, 256, f) == NULL) {
        fprintf(stderr, "File is empty\n");
        goto cleanup;
    }

    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        len--;   // no contar el newline
    }
    printf("First line of '%s': %zu chars\n", path, len);
    ret = (int)len;

cleanup:
    free(buf);
    if (f != NULL) fclose(f);
    return ret;
}

int main(void) {
    // Crear archivo de prueba
    FILE *tmp = fopen("/tmp/goto_ex2.txt", "w");
    if (tmp) { fprintf(tmp, "Hello, goto!\n"); fclose(tmp); }

    int r1 = read_first_line_length("/tmp/goto_ex2.txt");
    printf("Result: %d\n\n", r1);

    int r2 = read_first_line_length("/tmp/nonexistent.txt");
    printf("Result: %d\n", r2);

    remove("/tmp/goto_ex2.txt");
    return 0;
}
```

<details><summary>Predicción</summary>

```
First line of '/tmp/goto_ex2.txt': 12 chars
Result: 12

Cannot open '/tmp/nonexistent.txt'
Result: -1
```

- Test 1: el archivo contiene "Hello, goto!\n" (12 chars sin el newline). Se abren
  f y buf exitosamente, se lee la línea, se descuenta el `\n`. Cleanup libera todo.
  Retorna 12.
- Test 2: `fopen` falla → `goto cleanup`. `buf` es NULL → `free(NULL)` es no-op.
  `f` es NULL → se salta `fclose`. Retorna -1.
- El mismo bloque `cleanup:` funciona para ambos casos gracias a las inicializaciones a NULL.

</details>

---

### Ejercicio 3 — Múltiples labels (undo inverso)

```c
#include <stdio.h>

int init_a(void) { printf("  init_a\n"); return 0; }
int init_b(void) { printf("  init_b\n"); return 0; }
int init_c(void) { printf("  init_c\n"); return -1; } // falla

void cleanup_a(void) { printf("  cleanup_a\n"); }
void cleanup_b(void) { printf("  cleanup_b\n"); }
void cleanup_c(void) { printf("  cleanup_c\n"); }

int init_system(void) {
    printf("Initializing...\n");

    if (init_a() < 0) goto fail_a;
    if (init_b() < 0) goto fail_b;
    if (init_c() < 0) goto fail_c;

    return 0;

fail_c:
    cleanup_b();
fail_b:
    cleanup_a();
fail_a:
    return -1;
}

int main(void) {
    int result = init_system();
    printf("init_system returned: %d\n", result);
    return 0;
}
```

<details><summary>Predicción</summary>

```
Initializing...
  init_a
  init_b
  init_c
  cleanup_b
  cleanup_a
init_system returned: -1
```

- `init_a()` → 0 (éxito).
- `init_b()` → 0 (éxito).
- `init_c()` → -1 (falla) → `goto fail_c`.
- `fail_c:` ejecuta `cleanup_b()` (deshace b).
- Fall-through a `fail_b:` ejecuta `cleanup_a()` (deshace a).
- Fall-through a `fail_a:` retorna -1.

Nota: no se llama `cleanup_c()` porque `init_c` falló — el recurso c nunca se
adquirió, así que no hay nada que limpiar. Los labels desharán en orden inverso
solo lo que se inicializó exitosamente.

</details>

---

### Ejercicio 4 — Refactorizar: con goto vs sin goto

```c
// Versión CON goto:
#include <stdio.h>
#include <stdlib.h>

int allocate_three(void) {
    int ret = -1;
    int *a = NULL, *b = NULL, *c = NULL;

    a = malloc(100 * sizeof(int));
    if (!a) goto cleanup;
    printf("a allocated\n");

    b = malloc(200 * sizeof(int));
    if (!b) goto cleanup;
    printf("b allocated\n");

    c = malloc(300 * sizeof(int));
    if (!c) goto cleanup;
    printf("c allocated\n");

    a[0] = 1; b[0] = 2; c[0] = 3;
    printf("a[0]=%d, b[0]=%d, c[0]=%d\n", a[0], b[0], c[0]);
    ret = 0;

cleanup:
    free(c);
    free(b);
    free(a);
    return ret;
}

int main(void) {
    int r = allocate_three();
    printf("Returned: %d\n", r);
    return 0;
}
```

<details><summary>Predicción</summary>

```
a allocated
b allocated
c allocated
a[0]=1, b[0]=2, c[0]=3
Returned: 0
```

En condiciones normales (sin fallo de malloc), los tres mallocs tienen éxito,
se usan los arrays, `ret` se pone a 0, y cleanup libera todo.

Versión sin goto equivalente: necesitaría `free(a)` en el error de b,
`free(b); free(a)` en el error de c, y `free(c); free(b); free(a)` al final.
Con 3 recursos es manejable; con 10 recursos la versión sin goto sería mucho
más repetitiva y propensa a errores.

</details>

---

### Ejercicio 5 — Salir de loops anidados: 3 versiones

```c
#include <stdio.h>

// Versión 1: goto
void search_goto(int mat[3][4], int target) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            if (mat[i][j] == target) {
                printf("goto:  found %d at [%d][%d]\n", target, i, j);
                goto done;
            }
        }
    }
    printf("goto:  %d not found\n", target);
done:;
}

// Versión 2: flag
void search_flag(int mat[3][4], int target) {
    int found = 0;
    for (int i = 0; i < 3 && !found; i++) {
        for (int j = 0; j < 4 && !found; j++) {
            if (mat[i][j] == target) {
                printf("flag:  found %d at [%d][%d]\n", target, i, j);
                found = 1;
            }
        }
    }
    if (!found) printf("flag:  %d not found\n", target);
}

// Versión 3: función + return
int find_pos(int mat[3][4], int target, int *ri, int *rj) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            if (mat[i][j] == target) {
                *ri = i; *rj = j;
                return 1;
            }
        }
    }
    return 0;
}

int main(void) {
    int mat[3][4] = {
        {1, 2, 3, 4},
        {5, 6, 7, 8},
        {9, 10, 11, 12}
    };

    search_goto(mat, 7);
    search_flag(mat, 7);

    int r, c;
    if (find_pos(mat, 7, &r, &c))
        printf("func:  found 7 at [%d][%d]\n", r, c);

    printf("\n");
    search_goto(mat, 99);
    search_flag(mat, 99);

    if (!find_pos(mat, 99, &r, &c))
        printf("func:  99 not found\n");

    return 0;
}
```

<details><summary>Predicción</summary>

```
goto:  found 7 at [1][2]
flag:  found 7 at [1][2]
func:  found 7 at [1][2]

goto:  99 not found
flag:  99 not found
func:  99 not found
```

- 7 está en `mat[1][2]` (segunda fila, tercera columna).
- 99 no está en la matriz.
- Las tres versiones producen el mismo resultado.
- La versión con función es la más reutilizable y testeable.
- La versión con `goto` es la más directa pero menos estructurada.
- La versión con flag contamina las condiciones de ambos loops con `&& !found`.

</details>

---

### Ejercicio 6 — function scope de labels

```c
#include <stdio.h>

int main(void) {
    int n = 0;

    {
        start:
        n++;
        printf("n=%d\n", n);
    }

    if (n < 3) goto start;

    printf("Final: n=%d\n", n);
    return 0;
}
```

<details><summary>Predicción</summary>

```
n=1
n=2
n=3
Final: n=3
```

- `start:` está dentro de un bloque `{ }`, pero los labels tienen **function scope**:
  el `goto start` fuera del bloque puede alcanzarlo.
- Primera iteración: n=0 → n=1, imprime "n=1". 1 < 3 → goto start.
- Segunda: n=1 → n=2, imprime "n=2". 2 < 3 → goto start.
- Tercera: n=2 → n=3, imprime "n=3". 3 < 3 es falso → no salta.
- Imprime "Final: n=3".

Esto funciona como un loop, pero es un anti-patrón. Un `while` sería más claro:
```c
int n = 1;
while (n <= 3) { printf("n=%d\n", n); n++; }
```

</details>

---

### Ejercicio 7 — `free(NULL)` vs `fclose(NULL)`

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    // free(NULL) es seguro — el estándar lo define como no-op:
    int *p = NULL;
    free(p);
    printf("free(NULL): OK (no-op)\n");

    free(NULL);
    printf("free(NULL) direct: OK (no-op)\n");

    // fclose(NULL) es UB — NO ejecutar esto:
    // fclose(NULL);   // CRASH probable (segfault)

    // Por eso el patrón de cleanup siempre verifica:
    FILE *f = NULL;
    if (f != NULL) {
        fclose(f);
    }
    printf("fclose(NULL): skipped safely\n");

    return 0;
}
```

<details><summary>Predicción</summary>

```
free(NULL): OK (no-op)
free(NULL) direct: OK (no-op)
fclose(NULL): skipped safely
```

- `free(NULL)` está definido por el estándar C como un no-op. Puedes llamarlo
  sin verificar y nunca hará nada malo.
- `fclose(NULL)` es **undefined behavior** porque el estándar dice que el argumento
  debe ser un puntero a un `FILE` válido. En la práctica, suele causar segfault
  porque intenta acceder a campos del struct `FILE` a través de un puntero nulo.
- Por esto, en el patrón goto-cleanup, `free()` se llama sin verificar pero
  `fclose()` **siempre** necesita `if (f != NULL)`.

</details>

---

### Ejercicio 8 — Cleanup parcial: ¿qué se libera?

```c
#include <stdio.h>
#include <stdlib.h>

int demo(int fail_at) {
    int ret = -1;
    char *a = NULL;
    char *b = NULL;
    char *c = NULL;

    a = malloc(10);
    if (!a) goto cleanup;
    printf("a OK\n");

    if (fail_at == 1) goto cleanup;

    b = malloc(20);
    if (!b) goto cleanup;
    printf("b OK\n");

    if (fail_at == 2) goto cleanup;

    c = malloc(30);
    if (!c) goto cleanup;
    printf("c OK\n");

    ret = 0;

cleanup:
    printf("cleanup: free(a=%s) free(b=%s) free(c=%s)\n",
           a ? "ptr" : "NULL", b ? "ptr" : "NULL", c ? "ptr" : "NULL");
    free(c);
    free(b);
    free(a);
    return ret;
}

int main(void) {
    printf("--- fail_at=0 (no fail) ---\n");
    printf("ret=%d\n\n", demo(0));

    printf("--- fail_at=1 ---\n");
    printf("ret=%d\n\n", demo(1));

    printf("--- fail_at=2 ---\n");
    printf("ret=%d\n", demo(2));

    return 0;
}
```

<details><summary>Predicción</summary>

```
--- fail_at=0 (no fail) ---
a OK
b OK
c OK
cleanup: free(a=ptr) free(b=ptr) free(c=ptr)
ret=0

--- fail_at=1 ---
a OK
cleanup: free(a=ptr) free(b=NULL) free(c=NULL)
ret=-1

--- fail_at=2 ---
a OK
b OK
cleanup: free(a=ptr) free(b=ptr) free(c=NULL)
ret=-1
```

- `fail_at=0`: todo se adquiere, cleanup libera todo, ret=0.
- `fail_at=1`: solo a se adquirió. b y c son NULL → `free(NULL)` es no-op.
- `fail_at=2`: a y b se adquirieron. c es NULL → `free(NULL)` es no-op.

Las inicializaciones a NULL son la pieza clave: permiten que el mismo bloque
cleanup funcione correctamente sin importar en qué punto falló la función.

</details>

---

### Ejercicio 9 — Prohibición de saltar sobre VLA

```c
// Este código NO compila. ¿Por qué?
// (No intentes compilarlo — analiza el error)

#include <stdio.h>

int main(void) {
    int n = 5;
    goto skip;

    int arr[n];   // VLA (Variable Length Array)

skip:
    printf("After skip\n");
    // arr[0] = 42;   // ¿qué pasaría si esto se ejecutara?
    return 0;
}
```

<details><summary>Predicción</summary>

El compilador emite un error como:
```
error: jump into scope of identifier with variably modified type
```

El estándar C (§6.8.6.1) prohíbe saltar sobre la declaración de un VLA con `goto`.
La razón: un VLA necesita que su declaración se ejecute para determinar el tamaño
y reservar espacio en el stack. Si `goto` salta la declaración:

- No se sabe cuánto espacio reservar (`n` podría no haberse evaluado).
- El stack estaría en un estado inconsistente.
- Cualquier acceso a `arr` después del label sería UB catastrófico.

Nota: esta restricción solo aplica a **VLAs** (tamaño variable). Con arrays de
tamaño fijo (`int arr[5]`), el goto es legal porque el compilador conoce el
tamaño en tiempo de compilación.

</details>

---

### Ejercicio 10 — Goto-cleanup en la práctica: copiar archivo

```c
#include <stdio.h>
#include <stdlib.h>

int copy_file(const char *src, const char *dst) {
    int ret = -1;
    FILE *in = NULL;
    FILE *out = NULL;
    char *buf = NULL;

    in = fopen(src, "rb");
    if (!in) { perror(src); goto cleanup; }

    out = fopen(dst, "wb");
    if (!out) { perror(dst); goto cleanup; }

    buf = malloc(4096);
    if (!buf) { perror("malloc"); goto cleanup; }

    size_t n;
    while ((n = fread(buf, 1, 4096, in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            perror("fwrite");
            goto cleanup;
        }
    }

    if (ferror(in)) {
        perror("fread");
        goto cleanup;
    }

    ret = 0;
    printf("Copied '%s' -> '%s'\n", src, dst);

cleanup:
    free(buf);
    if (out) fclose(out);
    if (in) fclose(in);
    return ret;
}

int main(void) {
    // Crear archivo fuente
    FILE *f = fopen("/tmp/goto_src.txt", "w");
    if (f) { fprintf(f, "Line 1\nLine 2\nLine 3\n"); fclose(f); }

    int r1 = copy_file("/tmp/goto_src.txt", "/tmp/goto_dst.txt");
    printf("Result: %d\n\n", r1);

    int r2 = copy_file("/tmp/nonexistent.txt", "/tmp/goto_dst2.txt");
    printf("Result: %d\n", r2);

    remove("/tmp/goto_src.txt");
    remove("/tmp/goto_dst.txt");
    return 0;
}
```

<details><summary>Predicción</summary>

```
Copied '/tmp/goto_src.txt' -> '/tmp/goto_dst.txt'
Result: 0

/tmp/nonexistent.txt: No such file or directory
Result: -1
```

- Test 1: abre src (OK), abre dst (OK), malloc buf (OK), copia 3 líneas con
  `fread`/`fwrite`, no hay errores → ret=0. Cleanup cierra ambos archivos y libera buf.
- Test 2: `fopen` del source falla → `perror` imprime el mensaje del sistema →
  `goto cleanup`. `out` y `buf` son NULL → `fclose` se salta, `free(NULL)` es no-op.

Este es un ejemplo realista del patrón goto-cleanup con 3 recursos (2 archivos + 1 buffer)
y 4 posibles puntos de fallo (fopen×2, malloc, fwrite). Sin goto, el cleanup de
fwrite tendría que cerrar 2 archivos y liberar 1 buffer manualmente.

</details>
