# T04 — Punteros colgantes y wild pointers

> **Fuente**: `README.md`, `LABS.md`, `labs/README.md`, `dangling_uaf.c`, `dangling_local.c`, `wild_pointer.c`, `safe_free.c`
>
> Sin erratas detectadas en el material fuente.

---

## 1. Dangling pointer — qué es

Un **dangling pointer** (puntero colgante) apunta a memoria que **ya fue liberada** o **ya no es válida**. El puntero conserva la dirección antigua, pero esa dirección ya no nos pertenece. Desreferenciarlo es **UB**:

```c
int *p = malloc(sizeof(int));
*p = 42;
free(p);
// p sigue conteniendo la dirección anterior
// Pero la memoria ya no es nuestra

printf("%d\n", *p);    // UB — use-after-free (lectura)
*p = 100;              // UB — write-after-free (escritura)
```

Lo peligroso: a menudo **parece funcionar**. El valor 42 puede seguir ahí si nadie reutilizó esa memoria. El bug pasa desapercibido hasta que otro `malloc` reutiliza ese bloque y los datos se corrompen silenciosamente.

---

## 2. Causa 1: `free` sin `NULL` (use-after-free)

`free()` libera la memoria pero **no modifica el puntero** — sigue conteniendo la dirección antigua:

```c
int *p = malloc(sizeof(int));
*p = 42;
free(p);

printf("%p\n", (void *)p);   // imprime la dirección (no crashea)
*p = 100;                     // UB — la memoria puede haber sido
                               // reasignada a otro malloc
```

Solución: poner a `NULL` inmediatamente después de `free`:

```c
free(p);
p = NULL;
// Ahora *p = 100 da segfault (bug detectable inmediatamente)
```

Use-after-free es una de las vulnerabilidades de seguridad más explotadas — permite ejecución de código arbitrario si el atacante controla lo que se aloca en esa dirección después del `free`.

---

## 3. Causa 2: retornar dirección de variable local

Cuando una función retorna, su **stack frame se destruye**. Un puntero a una variable local de esa función se vuelve dangling:

```c
int *bad_function(void) {
    int x = 42;
    return &x;    // WARNING: returning address of local variable
}

int main(void) {
    int *p = bad_function();
    // x fue destruida al salir de bad_function
    // p apunta a un frame de stack que ya no existe

    printf("%d\n", *p);    // UB — puede imprimir 42, basura, o crashear
}
```

El valor puede parecer correcto la primera vez (42), pero cualquier llamada a otra función **sobrescribe** ese espacio del stack:

```c
int *p = bad_function();          // *p "parece" 42
overwrite_stack();                // otra función usa el mismo stack
printf("%d\n", *p);              // basura — el stack fue reutilizado
```

El mismo problema con arrays y strings:

```c
char *bad_string(void) {
    char buf[100] = "hello";
    return buf;    // WARNING — buf es local
}
```

### Soluciones para retornar datos

```c
// 1. String literal (vive en .rodata, siempre válido):
const char *good(void) {
    return "hello";    // lifetime estático — siempre válido
}

// 2. Heap (el caller hace free):
char *good2(void) {
    return strdup("hello");    // alocado en heap — vive hasta free()
}

// 3. Buffer del caller:
void good3(char *buf, size_t size) {
    snprintf(buf, size, "hello");
}

// 4. static (cuidado: no es thread-safe, se sobrescribe en cada llamada):
const char *good4(void) {
    static char buf[100] = "hello";
    return buf;    // lifetime del programa
}
```

---

## 4. Causa 3: scope terminado y `realloc`

### Scope terminado

```c
int *p;
{
    int x = 42;
    p = &x;
}
// x ya no existe — p es dangling
printf("%d\n", *p);    // UB
```

Caso sutil con loops:

```c
int *ptrs[10];
for (int i = 0; i < 10; i++) {
    int val = i * 10;
    ptrs[i] = &val;    // TODOS apuntan a la misma ubicación del stack
}
// Después del loop: todas las entradas son dangling
// Durante el loop: ptrs[0..i-1] apuntan a val que se sobrescribe cada iteración
```

### `realloc` invalida alias

```c
int *p = malloc(10 * sizeof(int));
int *alias = p;

p = realloc(p, 100 * sizeof(int));
// realloc puede mover el bloque a otra dirección:
//   Si lo movió: alias es dangling
//   Si no lo movió: alias sigue siendo válido
// NO se puede saber — alias es potencialmente dangling

// SOLUCIÓN: no mantener alias a memoria que puede realocar.
// Usar solo p después de realloc. O actualizar todos los alias.
```

Nota adicional sobre `realloc`: si falla, retorna `NULL` pero **no libera** el bloque original. Un patrón común pero peligroso:

```c
// MAL — pierde el puntero original si realloc falla:
p = realloc(p, new_size);    // si retorna NULL, se pierde p → memory leak

// BIEN — preserva el puntero original:
int *tmp = realloc(p, new_size);
if (tmp) {
    p = tmp;
} else {
    // p sigue siendo válido con el tamaño anterior
    // manejar el error
}
```

---

## 5. Wild pointer — puntero no inicializado

Un **wild pointer** es un puntero que **nunca fue inicializado**. Contiene un valor **indeterminado** — puede apuntar a cualquier dirección:

```c
int *p;          // wild pointer — valor basura (indeterminado)
*p = 42;         // UB — escribe en dirección aleatoria
// Puede: crashear, corromper datos silenciosamente, o "funcionar"
```

En C, las variables locales **no se inicializan**:

```c
void foo(void) {
    int *p;       // indeterminado — wild pointer
    int x;        // indeterminado
    int arr[10];  // todos los elementos indeterminados

    // Variables static/global SÍ se inicializan a 0:
    static int *sp;   // NULL (= 0)
    static int sy;    // 0
}
```

Nota técnica: incluso **leer** el valor de un puntero indeterminado (sin desreferenciarlo) es UB en C11, porque los punteros pueden tener **trap representations** — valores que causan comportamiento indefinido al ser leídos.

---

## 6. NULL vs wild: predictibilidad

| Tipo | `*p = 42;` | Resultado |
|------|-----------|-----------|
| Wild pointer | UB | Impredecible: crash, corrupción silenciosa, o "funciona" |
| NULL pointer | UB | Segfault **garantizado** en la práctica (detectable) |

Un segfault es **mejor** que corrupción silenciosa. El segfault termina el programa inmediatamente con un mensaje de error; la corrupción pasa desapercibida y se manifiesta como bugs imposibles de rastrear en otro lugar del código.

**Regla**: si no sabes a dónde apuntar, inicializa a `NULL`:

```c
int *p = NULL;           // si no tienes dirección aún
int *p = malloc(...);    // si vas a alocar
int *p = &x;             // si sabes a dónde apuntar
```

---

## 7. Detección: compilador, Valgrind, ASan

### En compilación: `-Wall -Wextra`

```c
// -Wreturn-local-addr (incluido en -Wall):
int *foo(void) {
    int x = 42;
    return &x;
    // warning: function returns address of local variable
}

// -Wuse-after-free (incluido en -Wall):
free(p);
*p = 42;
// warning: pointer 'p' used after 'free'

// -Wuninitialized (incluido en -Wall):
int *p;
if (*p) {}
// warning: 'p' is used uninitialized
// (no siempre lo detecta — depende del flujo de control)
```

### En runtime: Valgrind

```bash
valgrind ./prog
```

Detecta use-after-free en heap:

```
Invalid read of size 4
  at 0x...: main (prog.c:21)
Address 0x... is 0 bytes inside a block of size 4 free'd
  at 0x...: free
  at 0x...: main (prog.c:13)
```

**Limitación**: Valgrind no detecta bien los dangling pointers a variables locales (stack), solo los de heap (malloc/free).

### En runtime: ASan

```bash
gcc -fsanitize=address -g prog.c -o prog
./prog
```

Detecta use-after-free con reporte detallado:

```
ERROR: AddressSanitizer: heap-use-after-free on address 0x...
WRITE of size 4 at 0x...
freed by thread T0 here:
    #0 ... in free
    #1 ... in main prog.c:13
```

Para detectar stack-use-after-return:

```bash
ASAN_OPTIONS=detect_stack_use_after_return=1 ./prog
```

### Comparación de herramientas

| Herramienta | Qué detecta | Cuándo | Overhead |
|-------------|-------------|--------|----------|
| `-Wall -Wextra` | Retornar local, use-after-free, no inicializado | Compilación | Ninguno |
| Valgrind | Use-after-free en heap, lecturas no inicializadas | Runtime | ~20x más lento |
| ASan (`-fsanitize=address`) | Use-after-free, stack-use-after-return | Runtime | ~2x más lento |

Las herramientas se complementan. El compilador atrapa los patrones estáticos obvios. Valgrind y ASan atrapan los bugs dinámicos que dependen del flujo de ejecución.

---

## 8. Prevención: patrones seguros

### 1. Inicializar siempre

```c
int *p = NULL;                    // si no tienes dirección aún
int *data = malloc(n * sizeof(int));   // si vas a alocar
int *elem = &array[0];           // si sabes a dónde apuntar
```

### 2. `NULL` después de `free`

```c
free(p);
p = NULL;
```

### 3. No retornar direcciones locales

Verificar siempre: ¿la variable sigue viva cuando el caller usa el puntero?

```c
// MAL:
int *create(void) {
    int arr[10] = {0};
    return arr;               // arr muere al retornar
}

// BIEN — heap:
int *create(int n) {
    return calloc(n, sizeof(int));   // vive hasta free()
}

// BIEN — caller provee buffer:
void create(int *buf, int n) {
    for (int i = 0; i < n; i++) buf[i] = 0;
}
```

### 4. Limitar el scope de punteros

```c
// Cuanto menor sea el scope, menor la probabilidad de dangling:
{
    int *p = malloc(sizeof(int));
    *p = 42;
    process(*p);
    free(p);
}
// p no existe fuera de las llaves — imposible usar después de free
```

---

## 9. `SAFE_FREE` y ownership

### Macro `SAFE_FREE`

```c
#define SAFE_FREE(p) do { free(p); (p) = NULL; } while (0)

int *data = malloc(100 * sizeof(int));
// ... usar data ...
SAFE_FREE(data);    // free + NULL en una operación
// data es NULL ahora — *data da segfault (detecta bug)

// Idempotente — free(NULL) es un no-op (C estándar):
SAFE_FREE(data);    // OK — no crashea
```

El `do { ... } while (0)` asegura que la macro funcione correctamente en todas las posiciones sintácticas (después de `if` sin llaves, etc.).

**Limitación**: si el puntero tiene alias, `SAFE_FREE` solo pone a NULL **un** puntero — los alias siguen siendo dangling:

```c
int *p = malloc(sizeof(int));
int *alias = p;
SAFE_FREE(p);       // p = NULL
// alias sigue apuntando a la memoria liberada — dangling
```

### Ownership claro

En C no hay mecanismo del lenguaje para gestionar ownership — solo disciplina y documentación:

```c
// El caller es dueño — la función aloca, el caller libera:
// @return: caller must free() the returned pointer
char *read_file(const char *path);

// La función es dueña — el caller no debe liberar:
// @return: pointer to internal buffer, do not free
const char *get_name(int id);

// Transferencia de ownership:
// @param data: this function takes ownership, caller must not free
void process_and_free(int *data);
```

---

## 10. Comparación con Rust

Rust previene dangling pointers y use-after-free **en compilación**:

```rust
// Use-after-free — error de compilación:
let p = Box::new(42);
drop(p);              // equivalente a free()
// println!("{}", *p);  // ERROR: use of moved value 'p'

// Retornar referencia a local — error de compilación:
fn bad() -> &i32 {
    let x = 42;
    &x       // ERROR: cannot return reference to local variable
}

// Dangling por scope — error de compilación:
let p;
{
    let x = 42;
    p = &x;
}              // ERROR: 'x' does not live long enough
```

| Problema | C | Rust |
|----------|---|------|
| Use-after-free | UB silencioso | Error de compilación |
| Retornar local | Warning (`-Wall`) | Error de compilación |
| Wild pointer | UB silencioso | No existe (todas las variables se inicializan o el compilador exige inicialización) |
| Double free | UB silencioso | Error de compilación (moved value) |
| `realloc` alias | Dangling silencioso | `Vec::resize` invalida automáticamente los borrows |

C requiere disciplina del programador + herramientas externas (Valgrind, ASan). Rust lo previene por diseño: el borrow checker + ownership verifican en compilación que ninguna referencia sobreviva al dato al que apunta. El precio es la curva de aprendizaje del borrow checker.

---

## Ejercicios

### Ejercicio 1 — Identificar dangling pointers

```c
// Identifica todos los dangling pointers en este código y explica la causa:
char *get_greeting(const char *name) {
    char buf[100];
    snprintf(buf, sizeof(buf), "Hello, %s!", name);
    return buf;
}

int *create_sequence(int n) {
    int arr[n];
    for (int i = 0; i < n; i++) arr[i] = i + 1;
    return arr;
}

void example(void) {
    int *p = malloc(sizeof(int));
    int *q = p;
    *p = 42;
    free(p);
    printf("%d\n", *q);
}
```

<details><summary>Predicción</summary>

Tres dangling pointers: (1) `get_greeting` retorna `buf`, un array local — se destruye al retornar. Solución: usar `strdup` o recibir buffer del caller. (2) `create_sequence` retorna `arr`, un VLA local — mismo problema. Solución: usar `malloc(n * sizeof(int))`. (3) `example`: `q` es alias de `p`. Después de `free(p)`, `q` es dangling. `*q` es use-after-free. Solución: poner `q = NULL` o no mantener alias, o hacer `p = NULL; q = NULL;` después del free.

</details>

### Ejercicio 2 — Use-after-free observable

```c
// Escribe un programa que:
// a) Aloque un int con malloc, asigne 42, imprima *p
// b) Haga free(p), imprima *p (UB — ¿qué valor obtienes?)
// c) Aloque otro int con malloc, asigne 99, imprima *p de nuevo
// ¿El segundo malloc reutilizó la misma dirección?
// ¿El valor de *p cambió entre (b) y (c)?
// Ejecuta varias veces — ¿el comportamiento es consistente?
```

<details><summary>Predicción</summary>

(b) Después de `free(p)`, `*p` probablemente muestra 0 o 42 (depende de la implementación de malloc — glibc suele poner metadata del allocator en los primeros bytes). (c) El segundo `malloc(sizeof(int))` probablemente retorna la **misma dirección** (glibc reutiliza bloques libres del mismo tamaño). Si sí, `*p` ahora vale 99 — el dangling pointer de (b) accidentalmente "funciona" porque apunta al nuevo bloque. Esto es exactamente lo que hace use-after-free peligroso: el dato puede cambiar silenciosamente cuando alguien más aloca esa memoria.

</details>

### Ejercicio 3 — Stack overwrite visible

```c
// Escribe return_local() que retorne &x donde x=42.
// Escribe overwrite() que declare int arr[4] = {0xAA, 0xBB, 0xCC, 0xDD}.
// En main:
//   int *p = return_local();
//   printf("*p = %d\n", *p);       // ¿42?
//   overwrite();
//   printf("*p = %d\n", *p);       // ¿qué valor ahora?
// ¿El segundo printf muestra 42, 0xAA (170), u otro valor?
// ¿Por qué cambia?
```

<details><summary>Predicción</summary>

El primer `printf` probablemente muestra 42 — el stack aún no fue reutilizado. El segundo `printf` muestra un valor diferente (probablemente 170 = 0xAA, o 187 = 0xBB, u otro byte del array de `overwrite`). `overwrite()` usó el mismo espacio del stack que `return_local()`, sobrescribiendo donde estaba `x`. El valor exacto depende del layout del stack del compilador. Con `-O0` y la misma versión de GCC, el resultado suele ser reproducible. Con `-O2`, el compilador puede optimizar de formas impredecibles.

</details>

### Ejercicio 4 — Implementar SAFE_FREE

```c
// Implementa el macro SAFE_FREE(p).
// Escribe un programa que:
// a) Aloque memoria, use SAFE_FREE, verifique que p == NULL
// b) Llame SAFE_FREE(p) dos veces — ¿crashea?
// c) Tenga una función use_if_valid(const int *p) que verifique NULL antes de usar
// d) Muestre que sin SAFE_FREE, p retiene la dirección después de free
```

<details><summary>Predicción</summary>

(a) Después de `SAFE_FREE(p)`, `p == NULL` es true. (b) No crashea — `free(NULL)` es un no-op según C99. (c) `use_if_valid(p)` compara `p == NULL` y retorna sin desreferenciar si es NULL. (d) `free(p)` sin `p = NULL` deja `p` con la dirección vieja — `p == NULL` es false, y `use_if_valid(p)` intenta desreferenciar → UB.

</details>

### Ejercicio 5 — Wild pointer vs NULL

```c
// Declara int *wild; (sin inicializar) e int *safe = NULL;
// Imprime ambos con %p.
// ¿wild muestra (nil) o una dirección basura?
// ¿safe muestra (nil)?
// Comenta sobre qué pasaría al desreferenciar cada uno.
// Compila con -Wall — ¿qué warning obtienes para wild?
```

<details><summary>Predicción</summary>

`wild` muestra una dirección basura (indeterminada). `safe` muestra `(nil)`. Desreferenciar `wild` es UB impredecible — puede crashear, corromper datos, o "funcionar" silenciosamente. Desreferenciar `safe` da segfault garantizado (la dirección 0 no es mapeable en Linux). El compilador da `-Wuninitialized` para `wild`. Nota técnica: incluso imprimir `wild` con `%p` es UB en C11 porque leer un puntero indeterminado puede ser trap representation.

</details>

### Ejercicio 6 — Detección con Valgrind

```c
// Escribe un programa con tres bugs:
//   a) Use-after-free: malloc, free, luego leer
//   b) Write-after-free: malloc, free, luego escribir
//   c) Double free: malloc, free, free
// Ejecuta con valgrind y documenta el reporte para cada bug.
// ¿Valgrind identifica la línea exacta de cada error?
// ¿Identifica dónde se hizo el free original?
```

<details><summary>Predicción</summary>

Valgrind detecta los tres: (a) "Invalid read of size N" con la línea de la lectura y la línea del free. (b) "Invalid write of size N" con la línea de la escritura y la línea del free. (c) "Invalid free() / delete / delete[] / realloc()" — el segundo free es inválido. Para cada error, Valgrind muestra: el tipo de acceso, el tamaño, la dirección, dónde se alocó el bloque, y dónde se liberó. El programa termina con "ERROR SUMMARY: N errors from N contexts".

</details>

### Ejercicio 7 — Detección con ASan

```c
// Toma el mismo programa del ejercicio 6.
// Compila con gcc -fsanitize=address -g.
// Ejecuta y compara el reporte con el de Valgrind.
// ¿ASan da más o menos detalle? ¿Es más rápido?
// ¿Detecta el primer error y aborta, o reporta todos?
```

<details><summary>Predicción</summary>

ASan detecta el **primer** error y aborta inmediatamente (a diferencia de Valgrind que puede seguir ejecutando). El reporte es detallado: tipo de error, dirección, tamaño del acceso, stack trace de la alocación, stack trace del free, stack trace del acceso inválido, con colores. ASan es significativamente más rápido que Valgrind (~2x overhead vs ~20x). Para ver los tres errores, hay que arreglar cada uno y recompilar. ASan y Valgrind no deben usarse juntos (interfieren).

</details>

### Ejercicio 8 — `realloc` y dangling alias

```c
// a) Aloca int *p = malloc(5 * sizeof(int)); llena con {1,2,3,4,5}.
// b) Crea int *alias = p + 2; (apunta al tercer elemento).
// c) Haz p = realloc(p, 100 * sizeof(int));
// d) ¿alias sigue siendo válido? Imprime *alias — ¿qué obtienes?
// e) Escribe el patrón seguro con tmp para realloc.
// f) ¿Cómo actualizarías alias después del realloc?
```

<details><summary>Predicción</summary>

(d) `alias` es **potencialmente dangling**. Si `realloc` movió el bloque, `alias` apunta a la dirección vieja (memoria liberada). `*alias` es UB — puede dar 3, basura, o crashear. Si `realloc` no movió el bloque, `alias` sigue siendo válido por coincidencia. (e) Patrón seguro: `int *tmp = realloc(p, new_size); if (tmp) { p = tmp; } else { /* p sigue válido */ }`. (f) Actualizar: `alias = p + 2` después del realloc exitoso. Mejor aún: no mantener alias — usar siempre `p[2]` en lugar de `alias`.

</details>

### Ejercicio 9 — Ownership documentado

```c
// Implementa tres funciones con ownership diferente:
// a) int *create_array(int n);
//    → aloca en heap, retorna ownership al caller (caller hace free)
// b) const char *get_status(int code);
//    → retorna puntero a string literal (caller NO debe hacer free)
// c) void process_and_free(int *data, int n);
//    → recibe ownership, procesa, y hace free internamente
// Escribe main que use las tres correctamente.
// Documenta la ownership de cada puntero en comentarios.
```

<details><summary>Predicción</summary>

(a) `create_array` usa `malloc(n * sizeof(int))`, el caller debe hacer `free(arr)`. (b) `get_status` retorna string literal (`"OK"`, `"Error"`, etc.) — el caller no debe hacer `free` (los string literals están en `.rodata`). (c) `process_and_free` recibe el puntero, lo procesa (suma, imprime, etc.), hace `free(data)` al final. En `main`: `int *arr = create_array(5); /* caller owns */ ... const char *s = get_status(0); /* no free */ ... process_and_free(arr, 5); /* ownership transferred, arr is now dangling */ arr = NULL;`.

</details>

### Ejercicio 10 — Programa sin dangling pointers

```c
// Refactoriza este programa para eliminar TODOS los dangling/wild pointers:
//
// int *data;                    // wild
// int *get_data(int n) {
//     int arr[n];
//     for (int i = 0; i < n; i++) arr[i] = i;
//     return arr;               // retorna local
// }
// void cleanup(int *p) {
//     free(p);                  // no pone a NULL
// }
// int main(void) {
//     data = get_data(5);
//     int *copy = data;
//     cleanup(data);
//     printf("%d\n", *copy);    // use-after-free
// }
//
// Usa: inicialización a NULL, malloc en get_data, SAFE_FREE, eliminar alias.
// Compila con -Wall -Wextra -fsanitize=address y verifica cero errores.
```

<details><summary>Predicción</summary>

Correcciones: (1) `int *data = NULL;` — elimina wild pointer. (2) `get_data` usa `malloc(n * sizeof(int))` en lugar de array local. (3) `cleanup` usa `SAFE_FREE(p)` — pero como recibe por valor, solo pone a NULL la copia local. Mejor: `cleanup(int **pp) { SAFE_FREE(*pp); }` o hacer `data = NULL` en main después de cleanup. (4) Eliminar `copy` — o ponerlo a NULL después de cleanup. (5) No imprimir `*copy` después del free. Programa corregido compila sin warnings y ejecuta sin errores con ASan.

</details>
