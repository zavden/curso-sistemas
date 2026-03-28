# T01 — `const` y punteros

> **Fuente**: `README.md`, `LABS.md`, `labs/README.md`, `four_combos.c`, `read_right_to_left.c`, `const_errors.c`, `const_api.c`, `cast_away.c`

## Erratas detectadas

| Ubicación | Error | Corrección |
|-----------|-------|------------|
| `README.md` líneas 80-92 | El código usa `y` sin declarar. Además, tras `p = &y` dice que `printf("%d\n", *p)` imprime `20`, pero `p` ya apunta a `y`, no a `x` | Son dos ejemplos mezclados en un bloque. (1) `p = &y` demuestra que redirigir es legal. (2) Para mostrar que `x` cambia y `*p` refleja el cambio, `p` debe seguir apuntando a `x`. Corregido abajo como dos ejemplos separados |
| `labs/const_api.c` línea 63 | `printf("found 'W' at position %ld:`, usando `%ld` para `found - msg` que es `ptrdiff_t` | El formato correcto es `%td`. En 64-bit `%ld` funciona por coincidencia, pero no es portable |

---

## 1. Las 4 combinaciones de `const` con punteros

Un puntero tiene dos cosas que pueden ser `const` o no: **el puntero mismo** (la dirección almacenada) y **el dato apuntado** (lo que hay en esa dirección). Esto produce 4 combinaciones:

```c
// 1. Todo mutable:
int *p;
// p puede cambiar, *p puede cambiar

// 2. Dato protegido, puntero mutable:
const int *p;
// p puede cambiar, *p NO puede cambiar

// 3. Puntero fijo, dato mutable:
int *const p = &x;
// p NO puede cambiar, *p puede cambiar

// 4. Todo protegido:
const int *const p = &x;
// p NO puede cambiar, *p NO puede cambiar
```

Tabla resumen:

| Declaración | `*p = valor` | `p = &otra` |
|-------------|:---:|:---:|
| `int *p` | Sí | Sí |
| `const int *p` | No | Sí |
| `int *const p` | Sí | No |
| `const int *const p` | No | No |

---

## 2. Regla de lectura derecha a izquierda

Para interpretar cualquier declaración con `const` y punteros, léela **de derecha a izquierda**:

```
const int *p
          ←──
p es un puntero (*) a int que es const
→ el dato apuntado es const

int *const p
      ←────
p es un const puntero (*) a int
→ el puntero es const

const int *const p
            ←────
p es un const puntero (*) a int que es const
→ ambos son const
```

La clave: la posición de `const` respecto al `*` determina qué se protege:
- `const` a la **izquierda** de `*` → protege el **dato**
- `const` a la **derecha** de `*` → protege el **puntero**

### `const int *` e `int const *` son equivalentes

```c
const int *p;    // más común en la práctica
int const *p;    // equivalente — lee: "p is a pointer to const int"
```

La posición de `const` respecto al **tipo** (`int`) no importa — solo importa su posición respecto al `*`. Ambas formas ponen `const` a la izquierda del `*`, así que ambas protegen el dato.

---

## 3. `const int *p` — dato protegido

Es la combinación más importante y más usada. Promete **no modificar** el dato apuntado:

```c
int x = 10;
const int *p = &x;

printf("%d\n", *p);    // OK — leer
// *p = 20;            // ERROR: assignment of read-only location
p = &y;                // OK — el puntero sí se puede redirigir
```

**Nota importante**: `const int *p` no hace que `x` sea constante. Solo prohíbe modificar `x` **a través de `p`**. La variable `x` se puede seguir modificando por su nombre:

```c
int x = 10;
const int *p = &x;
x = 20;                // OK — x no es const, solo la vista a través de p
printf("%d\n", *p);    // 20 — p sigue apuntando a x, que ahora vale 20
```

### Uso en funciones — el caso más común

`const` en parámetros puntero documenta que la función **solo lee**:

```c
// strlen no modifica el string:
size_t strlen(const char *s);

// strcpy modifica dest pero no src:
char *strcpy(char *dest, const char *src);

// sum no modifica el array:
int sum(const int *arr, int n) {
    int total = 0;
    for (int i = 0; i < n; i++) {
        total += arr[i];
        // arr[i] = 0;    // ERROR — protegido por const
    }
    return total;
}
```

El compilador verifica la promesa: si intentas modificar un parámetro `const`, obtienes error.

---

## 4. `int *const p` — puntero fijo

El puntero no se puede redirigir, pero el dato sí se puede modificar:

```c
int x = 10;
int *const p = &x;    // DEBE inicializarse en la declaración

*p = 100;     // OK — modificar el dato
// p = &y;    // ERROR: assignment of read-only variable 'p'
```

Es como una referencia en C++: siempre apunta al mismo lugar. Uso poco frecuente en la práctica. Un ejemplo: proteger un buffer de ser reasignado accidentalmente:

```c
void fill_buffer(int *const buf, int n) {
    for (int i = 0; i < n; i++) {
        buf[i] = i;        // OK — modifica el dato
    }
    // buf = NULL;          // ERROR — no se puede redirigir
}
```

---

## 5. `const int *const p` — todo protegido

```c
int x = 42;
const int *const p = &x;

// *p = 100;    // ERROR — dato protegido
// p = &y;      // ERROR — puntero protegido
printf("%d\n", *p);    // OK — solo lectura
```

Uso real: tablas de lookup constantes:

```c
static const char *const error_messages[] = {
    "Success",
    "File not found",
    "Permission denied",
    "Out of memory",
};
```

Lee de derecha a izquierda: `error_messages` es un array de `const` punteros a `const char`. No se pueden modificar los punteros (redirigir a otro string) ni los caracteres de los strings.

---

## 6. Conversión implícita: agregar `const` es seguro

```c
int x = 42;
int *p = &x;

// Agregar const es seguro — conversión implícita:
const int *cp = p;    // OK — int* → const int*

// Quitar const NO es implícito:
// int *q = cp;       // WARNING: discards 'const' qualifier
```

La lógica: **agregar restricciones** es seguro (no puedes hacer nada nuevo peligroso), **quitar restricciones** es peligroso (podrías modificar algo que debería ser read-only).

Esto permite pasar `int *` a funciones que esperan `const int *` sin cast:

```c
int data[] = {10, 20, 30};
int total = sum(data, 3);    // int[] → int* → const int* — implícito
```

### Trampa con doble puntero

La conversión `int **` → `const int **` **no** es segura (a diferencia de `int *` → `const int *`):

```c
const int x = 42;
int *p;
const int **pp = &p;     // si esto fuera legal...
*pp = &x;                // pp modifica p para apuntar a x (const)
*p = 100;                // p modifica x — ¡pero x es const! → UB
```

El compilador da warning en `const int **pp = &p` porque `int **` y `const int **` son tipos incompatibles. La conversión segura es `int **` → `int *const *` (agregar `const` al puntero intermedio, no al dato).

---

## 7. `const` en APIs de funciones

Regla práctica: **poner `const` en todo parámetro puntero que no necesites modificar**. Esto:
1. Documenta la intención (solo lectura)
2. El compilador verifica la promesa
3. Permite pasar datos `const` sin cast

```c
// Parámetros — const donde no se modifica:
int find(const char *haystack, const char *needle);
void copy(char *dest, const char *src, size_t n);
int sum(const int *arr, size_t n);

// Retorno — const para strings literales:
const char *get_error_message(int code) {
    static const char *const messages[] = {
        "Success", "File not found", "Permission denied"
    };
    if (code < 0 || code > 2) return "Unknown error";
    return messages[code];
}
// El const en el retorno indica que el caller NO debe modificar el string.
// Sin const, el caller podría intentar msg[0] = 'x' sobre un string literal → UB.

// Variables locales — const si no cambian:
const int max_retries = 5;
const char *filename = argv[1];   // puntero mutable a chars const
```

---

## 8. Cast away `const` — cuándo es UB

Quitar `const` con cast es técnicamente posible, pero su legalidad depende del **objeto original**:

### Caso 1 — objeto original ES `const` → UB

```c
const int x = 42;
int *bad = (int *)&x;    // cast quita const
*bad = 100;              // UNDEFINED BEHAVIOR
```

¿Por qué UB? El compilador, al ver `const int x = 42`, puede:
- Poner `x` en memoria de solo lectura → segfault
- Reemplazar `x` por `42` en todo el código (constant folding) → `x` sigue mostrando 42 pero `*bad` muestra 100
- Cualquier otra cosa — es UB

Sin optimización (`-O0`), `x` podría cambiar a 100 (parece funcionar). Con `-O2`, el compilador hace constant folding y `x` sigue mostrando 42. **El resultado cambia según el nivel de optimización** — señal clara de UB.

### Caso 2 — objeto original NO es `const` → válido

```c
int y = 42;
const int *view = &y;    // agregar const
int *mp = (int *)view;   // quitar const — OK porque y no es const
*mp = 200;               // válido — y nunca fue const
```

Esto es legal porque `y` fue declarado como `int`, no `const int`. El `const` en `view` era solo una vista restringida, no una propiedad del objeto.

---

## 9. `const` correctness — errores comunes

### Error 1: olvidar `const` en parámetros de solo lectura

```c
// Mal — no comunica que arr no se modifica:
int sum(int *arr, int n);

// Bien — const documenta y el compilador verifica:
int sum(const int *arr, int n);
```

Sin `const`, el caller no puede pasar `const int *` sin cast. Peor: la función podría accidentalmente modificar el array sin que el compilador lo detecte.

### Error 2: retornar `char *` a un string literal

```c
// Mal — el caller podría modificar el literal → UB:
char *get_greeting(void) {
    return "Hello, World!";   // string literal es read-only
}

// Bien — const previene la modificación:
const char *get_greeting(void) {
    return "Hello, World!";
}
```

### Error 3: confundir qué protege `const`

```c
const char *msg = "hello";
msg = "world";     // OK — el puntero no es const
// msg[0] = 'W';   // ERROR — los chars son const

char *const buf = buffer;
buf[0] = 'X';     // OK — los chars no son const
// buf = other;    // ERROR — el puntero es const
```

---

## 10. Comparación con Rust: `&T` vs `&mut T`

En Rust, la inmutabilidad es el **default**:

```rust
let x = 42;          // inmutable por default
let mut y = 42;       // hay que pedir mutabilidad explícitamente

fn sum(arr: &[i32]) -> i32 { ... }        // &T — referencia inmutable (como const int *)
fn fill(arr: &mut [i32], val: i32) { ... } // &mut T — referencia mutable (como int *)
```

| Concepto | C | Rust |
|----------|---|------|
| Dato no modificable vía puntero | `const int *p` | `&i32` (default) |
| Dato modificable vía puntero | `int *p` | `&mut i32` (explícito) |
| Puntero fijo | `int *const p` | No aplica — las referencias no se "redirigen" |
| Todo protegido | `const int *const p` | `&i32` (ya lo es por default) |
| Quitar protección | `(int *)p` — compila, posible UB | No existe `unsafe` cast equivalente directo |

La diferencia fundamental: en C, `const` es opt-in (debes recordar ponerlo). En Rust, la inmutabilidad es opt-out (debes pedir `mut`). El diseño de Rust hace que el camino más fácil sea el más seguro.

Además, Rust garantiza **en compilación** que no existan una `&mut T` y una `&T` al mismo dato simultáneamente (borrow checker). En C, puedes tener `const int *cp` y `int *mp` apuntando al mismo dato — el compilador no lo previene.

---

## Ejercicios

### Ejercicio 1 — Clasificar las 4 combinaciones

```c
// Declara las 4 combinaciones de const con punteros a int.
// Para cada una, intenta:
//   a) *p = 99;     (modificar dato)
//   b) p = &otra;   (redirigir puntero)
// Compila con -Wall -Wextra y anota cuáles dan error y el mensaje exacto.
// ¿El mensaje dice "read-only location" o "read-only variable"?
// ¿A qué corresponde cada uno?
```

<details><summary>Predicción</summary>

`int *p`: ambas operaciones compilan. `const int *p`: (a) da error "read-only location" (el dato), (b) compila. `int *const p`: (a) compila, (b) da error "read-only variable" (el puntero). `const int *const p`: ambas dan error — (a) "read-only location" y (b) "read-only variable". "read-only location" = dato protegido (`const` a la izquierda de `*`). "read-only variable" = puntero protegido (`const` a la derecha de `*`).

</details>

### Ejercicio 2 — Regla derecha a izquierda

```c
// Sin compilar, lee cada declaración de derecha a izquierda y predice:
//   a) const char *msg;
//   b) char *const buf;
//   c) const char *const label;
//   d) const double **pp;
//   e) double *const *pp2;
// Para cada una: ¿qué es const? ¿Se puede redirigir? ¿Se puede modificar el dato?
// Después escribe un programa que verifique tus predicciones.
```

<details><summary>Predicción</summary>

(a) `msg` es puntero a `const char` — puede redirigir, no puede modificar chars. (b) `buf` es `const` puntero a `char` — no puede redirigir, puede modificar chars. (c) `label` es `const` puntero a `const char` — ni redirigir ni modificar. (d) `pp` es puntero a puntero a `const double` — `pp` se puede redirigir, `*pp` se puede redirigir, `**pp` no se puede modificar. (e) `pp2` es puntero a `const` puntero a `double` — `pp2` se puede redirigir, `*pp2` no se puede redirigir, `**pp2` sí se puede modificar.

</details>

### Ejercicio 3 — Conversión implícita

```c
// Declara int x y las siguientes asignaciones.
// Predice cuáles compilan sin warning, cuáles con warning, cuáles con error:
//   a) const int *cp = &x;
//   b) int *mp = cp;
//   c) const int cx = 10; int *p = &cx;
//   d) const int *cp2 = &cx;
//   e) int **pp; const int **cpp = pp;
// Compila con -Wall -Wextra -Wpedantic y verifica.
```

<details><summary>Predicción</summary>

(a) Compila sin warning — `int *` → `const int *` es agregar restricciones (implícito). (b) Warning "discards const qualifier" — `const int *` → `int *` quita restricciones. (c) Warning "discards const qualifier" — `const int *` → `int *`. (d) Sin warning — `const int *` a `const int *`, compatible. (e) Warning "incompatible pointer types" — `int **` → `const int **` no es seguro (como se explicó en la trampa del doble puntero).

</details>

### Ejercicio 4 — Firmas con `const` correctness

```c
// Escribe las firmas correctas (con const donde corresponda) para:
// 1. int count(???) — cuenta apariciones de 'target' en un array de n ints
// 2. void swap(???) — intercambia dos ints
// 3. const char *greeting(???) — retorna "Good morning/afternoon/evening" según hora
// 4. char *find_first(???) — busca la primera aparición de char c en un string,
//    retorna puntero a la posición encontrada (mutable) o NULL
// Implementa cada función y verifica que compila sin warnings.
```

<details><summary>Predicción</summary>

1. `int count(const int *arr, int n, int target)` — solo lee el array. 2. `void swap(int *a, int *b)` — modifica ambos, no puede ser `const`. 3. `const char *greeting(int hour)` — retorna puntero a string literal (read-only). 4. `char *find_first(char *str, char c)` — el string puede ser mutable (la firma sin `const` permite modificar a través del puntero retornado). Nota: la función estándar `strchr` tiene firma `char *strchr(const char *s, int c)` en C, lo cual es un problema de diseño conocido — retorna `char *` mutable desde un `const char *`.

</details>

### Ejercicio 5 — `const` correctness en código existente

```c
// Agrega const donde sea apropiado en este código:
void process(int *data, int n, int *result) {
    int sum = 0;
    for (int i = 0; i < n; i++) {
        sum += data[i];
    }
    *result = sum;
}

char *get_greeting(void) {
    return "Hello, World!";
}

void print_matrix(int **matrix, int rows, int cols) {
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            printf("%d ", matrix[r][c]);
        }
        printf("\n");
    }
}
```

<details><summary>Predicción</summary>

`process`: `data` → `const int *data` (solo se lee), `n` queda `int`, `result` queda `int *` (se escribe). `get_greeting`: retorno → `const char *` (string literal es read-only). `print_matrix`: `matrix` → `const int *const *matrix` (no se modifican los punteros a filas ni los ints), `rows` y `cols` quedan `int`. La firma de `print_matrix` es sutil: `const int **` no bastaría — necesitamos que los punteros intermedios también sean const para que sea correcto (`const int *const *`).

</details>

### Ejercicio 6 — Cast away const: UB vs válido

```c
// Escribe un programa con dos casos:
// Caso 1: const int x = 42; quita const con cast, modifica, imprime x y *ptr.
//         Compila con -O0 y con -O2. ¿Cambia el resultado?
// Caso 2: int y = 42; agrégale const con const int *view = &y;
//         quita const con cast, modifica, imprime y.
// ¿Cuál es UB y cuál es válido? ¿Por qué?
```

<details><summary>Predicción</summary>

Caso 1 es UB porque `x` fue declarado `const`. Con `-O0`, `x` probablemente muestre 100 (la escritura modificó la memoria). Con `-O2`, `x` muestra 42 (constant folding — el compilador reemplazó `x` por 42) pero `*ptr` muestra 100. Caso 2 es válido porque `y` no es `const` — el `const` en `view` era solo una restricción de la vista. `y` muestra el nuevo valor en ambos niveles de optimización.

</details>

### Ejercicio 7 — Tabla de lookup con doble const

```c
// Crea una tabla static const char *const days[] con los 7 días de la semana.
// Escribe una función const char *day_name(int n) que retorne el nombre.
// Intenta:
//   a) days[0] = "Lunes";    // ¿compila?
//   b) days[0][0] = 'l';     // ¿compila?
//   c) const char *d = day_name(0); d[0] = 'x';  // ¿compila?
// ¿Qué protege cada const en la declaración?
```

<details><summary>Predicción</summary>

(a) Error — los punteros son `const` (el `const` después del `*`): no se puede redirigir `days[0]` a otro string. (b) Error — los chars son `const` (el `const` antes del `*`): no se puede modificar el contenido del string. (c) Error — `d` es `const char *`, no se puede modificar `d[0]`. Los dos `const` protegen cosas distintas: `const char *` protege los caracteres, `* const` protege los punteros del array.

</details>

### Ejercicio 8 — `const` con structs

```c
// Dado:
struct Config {
    char name[32];
    int value;
    int *data;      // puntero a array dinámico
};

// ¿Qué permite y qué prohíbe cada declaración?
//   a) const struct Config *cfg;
//   b) struct Config *const cfg;
//   c) const struct Config cfg_val;
// Para (a): ¿se puede hacer cfg->value = 10? ¿cfg->data[0] = 5? ¿cfg->data = NULL?
// Escribe un programa que verifique.
```

<details><summary>Predicción</summary>

(a) `const struct Config *cfg`: no se puede modificar ningún **miembro directo** — `cfg->value = 10` es error, `cfg->data = NULL` es error. Pero `cfg->data[0] = 5` **sí compila** — `const` protege el puntero `data` (no se puede reasignar), pero no protege los datos **apuntados por** `data`. Es `const` superficial (shallow const). `cfg` sí se puede redirigir. (b) `struct Config *const cfg`: el puntero no se puede redirigir, pero sí se pueden modificar todos los miembros. (c) `const struct Config cfg_val`: la variable completa es const — no se puede modificar ningún miembro directamente. Pero `cfg_val.data[0] = 5` sigue compilando por la misma razón de shallow const.

</details>

### Ejercicio 9 — `const` shallow vs deep

```c
// Demuestra que const en C es "shallow" (superficial):
// a) Declara struct Wrapper { int *ptr; };
// b) Crea const struct Wrapper w = { .ptr = arr };
// c) Intenta w.ptr = other;     // ¿compila?
// d) Intenta w.ptr[0] = 99;     // ¿compila?
// e) Intenta *(w.ptr) = 99;     // ¿compila?
// ¿Por qué (d) y (e) compilan si w es const?
// ¿Cómo se diferencia esto de Rust?
```

<details><summary>Predicción</summary>

(c) Error — `w.ptr` es un miembro de una variable `const`, no se puede reasignar el puntero. (d) y (e) **compilan** — `const` protege el valor del puntero (la dirección almacenada en `w.ptr`), pero no los datos a los que apunta. `w.ptr` es de tipo `int *const` (puntero const a int mutable), no `const int *const`. En Rust, un `&Wrapper` haría que `ptr` sea completamente inmutable (deep immutability), incluyendo los datos apuntados — el borrow checker previene la mutación transitiva.

</details>

### Ejercicio 10 — API completa con const correctness

```c
// Implementa una mini-biblioteca de array de ints con estas funciones:
//
// void    arr_fill(int *arr, size_t n, int value);
// int     arr_sum(const int *arr, size_t n);
// int     arr_max(const int *arr, size_t n);
// int     arr_contains(const int *arr, size_t n, int target);
// void    arr_print(const int *arr, size_t n);
// void    arr_reverse(int *arr, size_t n);
// size_t  arr_count(const int *arr, size_t n, int target);
//
// Para cada función, justifica si el parámetro arr lleva const o no.
// Escribe un main que use todas las funciones y compila sin warnings.
// Pista: las funciones que solo leen llevan const, las que modifican no.
```

<details><summary>Predicción</summary>

`arr_fill` y `arr_reverse` modifican el array → `int *arr` sin const. Las demás solo leen → `const int *arr`. El programa compila sin warnings porque `int[]` se convierte implícitamente a `const int *` al llamar las funciones de solo lectura. `arr_reverse` puede implementarse con dos índices (`left` y `right`) intercambiando elementos hasta cruzarse, o usando un swap genérico. `arr_max` necesita manejar el caso `n == 0` (podría retornar `INT_MIN` o requerir `n >= 1`).

</details>
