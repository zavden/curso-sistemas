# T03 — Macros peligrosas

## Erratas detectadas

| Ubicacion | Error | Correccion |
|-----------|-------|------------|
| `labs/README.md:510` | El mensaje de error esperado dice `error: implicit declaration of function 'typeof'`, pero `stmt_expr.c` usa `__typeof__` (doble guion bajo), que GCC reconoce en todos los modos de compilacion. Con `-std=c11 -Wpedantic` se producen **warnings** sobre las statement expressions, no el error indicado | El resultado real seria solo warnings: `ISO C forbids braced-groups within expressions [-Wpedantic]`. Para obtener el error de `typeof`, el codigo tendria que usar `typeof` sin guiones bajos |

---

## 1 — Precedencia sin parentesis

Las macros son sustitucion de texto. El preprocesador no sabe nada de
operadores ni precedencia — pega el texto tal cual. El resultado:

```c
#define DOUBLE_BAD(x)  x + x
#define SQUARE_BAD(x)  x * x

/* Parece correcto... hasta que lo usas en una expresion: */
int a = SQUARE_BAD(2 + 3);
/* Se expande a: 2 + 3 * 2 + 3
   * tiene mayor precedencia que +
   = 2 + 6 + 3 = 11   (no 25) */

int b = 10 - DOUBLE_BAD(3);
/* Se expande a: 10 - 3 + 3
   Izquierda a derecha: (10 - 3) + 3 = 10   (no 4) */

int c = 100 / SQUARE_BAD(2 + 3);
/* Se expande a: 100 / 2 + 3 * 2 + 3
   = 50 + 6 + 3 = 59   (no 4) */
```

La herramienta para diagnosticar: `gcc -E` muestra el codigo despues del
preprocesador, antes de compilar. Ahi se ve exactamente a que se expande cada
macro:

```bash
gcc -std=c11 -E file.c 2>/dev/null | tail -30
```

---

## 2 — Parentesis: la regla completa

La solucion tiene dos partes — ambas son necesarias:

```c
/* Regla 1: envolver CADA parametro en parentesis */
#define SQUARE(x)  (x) * (x)     /* insuficiente */

/* Regla 2: envolver la EXPRESION COMPLETA en parentesis */
#define SQUARE(x)  ((x) * (x))   /* correcto */
```

Por que ambas son necesarias:

```c
/* Solo regla 1 (parametros con parens, expresion sin): */
#define DOUBLE(x) (x) + (x)

int a = 2 * DOUBLE(5);
/* Expande: 2 * (5) + (5) = 10 + 5 = 15   (no 20) */

/* Con ambas reglas: */
#define DOUBLE(x) ((x) + (x))

int b = 2 * DOUBLE(5);
/* Expande: 2 * ((5) + (5)) = 2 * 10 = 20   (correcto) */
```

Caso especial — operadores bitwise tienen precedencia muy baja:

```c
#define IS_ODD_BAD(n)  n % 2 == 1

int r = IS_ODD_BAD(4 | 1);
/* Expande: 4 | 1 % 2 == 1
   Precedencia: % > == > |
   = 4 | ((1 % 2) == 1) = 4 | 1 = 5   (no 1) */

#define IS_ODD_GOOD(n)  (((n) % 2) == 1)

int s = IS_ODD_GOOD(4 | 1);
/* Expande: (((4 | 1) % 2) == 1) = ((5 % 2) == 1) = 1 */
```

---

## 3 — Evaluacion multiple

El problema mas comun y peligroso de las macros: los argumentos se evaluan
**cada vez** que aparecen en la expansion:

```c
#define MAX(a, b) ((a) > (b) ? (a) : (b))

int x = 5, y = 3;
int m = MAX(x++, y++);
/* Se expande a: ((x++) > (y++) ? (x++) : (y++))
   Paso 1: (5) > (3) → true, post-increment: x=6, y=4
   Paso 2: rama true → (x++) retorna 6, x=7
   Resultado: m=6, x=7, y=4
   x se incremento DOS veces */
```

El operador ternario tiene un sequence point entre la condicion y la rama
seleccionada. Por eso el comportamiento es definido para MAX, pero `x++`
aparece dos veces: una en la comparacion y otra en la rama seleccionada.

```c
/* Con una funcion inline NO hay problema: */
static inline int max_int(int a, int b) {
    return a > b ? a : b;
}

int x = 5, y = 3;
int m = max_int(x++, y++);
/* Los argumentos se evaluan UNA vez antes de entrar a la funcion.
   m=5, x=6, y=4   (correcto) */
```

---

## 4 — Side effects en argumentos de macros

La regla fundamental: **nunca pasar expresiones con side effects a macros**.
Side effects incluyen `++`, `--`, llamadas a funciones que modifican estado,
I/O.

```c
#define SQUARE(x) ((x) * (x))

/* MAL: x++ se evalua dos veces */
int a = SQUARE(x++);
/* Expande: ((x++) * (x++))
   UNDEFINED BEHAVIOR: dos modificaciones de x sin
   sequence point entre ellas */

/* MAL: funcion llamada dos (o tres) veces */
int b = MAX(read_sensor(), threshold);
/* read_sensor() se llama 2 o 3 veces segun la rama
   del ternario. Si retorna valores distintos cada vez
   → resultado impredecible */
```

Nota sobre undefined behavior: `SQUARE(x++)` expande a `((x++) * (x++))`.
El operador `*` **no** es un sequence point (a diferencia del ternario `?:`).
Dos modificaciones de la misma variable sin sequence point = UB. GCC detecta
esto:

```
warning: operation on 'x' may be undefined [-Wsequence-point]
```

Soluciones:

```c
/* 1. Guardar en variable temporal: */
int val = read_sensor();
int b = MAX(val, threshold);

/* 2. Usar funcion inline (solucion definitiva): */
static inline int square(int x) { return x * x; }
int a = square(x++);   /* x++ se evalua una vez */
```

---

## 5 — El patron do { ... } while (0)

Las macros con multiples sentencias necesitan un wrapper especial. Hay tres
intentos posibles:

**Intento 1 — sentencias sueltas (falla):**

```c
#define LOG(msg) printf("[LOG] "); printf("%s\n", msg);

if (error)
    LOG("fail");
else
    printf("ok\n");

/* Expande a:
   if (error)
       printf("[LOG] ");
   printf("%s\n", "fail");    ← FUERA del if
   ;                           ← sentencia vacia
   else                        ← error: else sin if
       printf("ok\n");
*/
```

**Intento 2 — llaves (tambien falla):**

```c
#define LOG(msg) { printf("[LOG] "); printf("%s\n", msg); }

if (error)
    LOG("fail");
else
    printf("ok\n");

/* Expande a:
   if (error)
       { printf("[LOG] "); printf("%s\n", "fail"); };
                                                    ^ este ; cierra el if
   else            ← error: else sin if
       printf("ok\n");
*/
```

El problema es el `;` que el caller pone despues de `LOG("fail")`. Con llaves,
el `}` ya cerro el bloque, y el `;` adicional crea una sentencia vacia que
desconecta el else.

**Solucion — do { ... } while (0):**

```c
#define LOG(msg) do { \
    printf("[LOG] "); \
    printf("%s\n", msg); \
} while (0)

if (error)
    LOG("fail");
else
    printf("ok\n");

/* Expande a:
   if (error)
       do { printf("[LOG] "); printf("%s\n", "fail"); } while (0);
   else
       printf("ok\n");
   /* ; es parte del do-while → todo correcto */
*/
```

`do { ... } while (0)` es una sentencia sintacticamente completa que:
1. Requiere `;` al final (como cualquier sentencia)
2. Se ejecuta exactamente una vez
3. No interfiere con if/else

Ejemplos practicos:

```c
#define SAFE_FREE(p) do { free(p); (p) = NULL; } while (0)

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Check failed: %s at %s:%d\n", \
                #cond, __FILE__, __LINE__); \
        return -1; \
    } \
} while (0)
```

---

## 6 — Variables temporales y colision de nombres

Si un macro necesita variables temporales, estas pueden colisionar con variables
del caller:

```c
#define SWAP(a, b) do { \
    int tmp = (a); \
    (a) = (b); \
    (b) = tmp; \
} while (0)

int tmp = 10, x = 20;
SWAP(tmp, x);
/* Expande a:
   do {
       int tmp = (tmp);    ← tmp local se inicializa consigo mismo
       (tmp) = (x);
       (x) = tmp;
   } while (0)
   El tmp interno hace shadowing del externo. Resultado incorrecto. */
```

El problema: en C, la variable entra en scope en el momento de su declaracion.
Asi que `int tmp = (tmp);` usa el `tmp` recien declarado (sin inicializar), no
el externo.

Soluciones:

```c
/* 1. Prefijo descriptivo (convencion): */
#define SWAP(a, b) do { \
    __typeof__(a) _swap_tmp = (a); \
    (a) = (b); \
    (b) = _swap_tmp; \
} while (0)
/* _swap_tmp es poco probable que colisione */

/* 2. Nombre unico con __LINE__: */
#define CONCAT_(a, b) a##b
#define CONCAT(a, b)  CONCAT_(a, b)
#define UNIQUE(prefix) CONCAT(prefix, __LINE__)
/* UNIQUE(tmp) en linea 42 genera tmp42 */

/* 3. Statement expression (GCC, ver seccion 8): */
#define SWAP(a, b) ({         \
    __typeof__(a) _t = (a);   \
    (a) = (b);                \
    (b) = _t;                 \
})
```

Nota: `__typeof__` (con doble guion bajo) funciona en todos los modos de GCC
(`-std=c11` incluido). La forma `typeof` (sin guiones bajos) requiere
`-std=gnu11` o C23.

---

## 7 — Ausencia de verificacion de tipos

Las macros aceptan cualquier tipo sin verificacion. Esto permite bugs silenciosos:

```c
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* Caso peligroso: signed vs unsigned */
int neg = -1;
unsigned int pos = 1;
int m = MAX(neg, pos);
/* La comparacion se hace en unsigned (reglas de promotion de C).
   -1 como unsigned = UINT_MAX (4294967295).
   UINT_MAX > 1 → true → m = -1.
   La macro dice que -1 es mayor que 1. */
```

La regla de promotion: cuando se mezclan `int` y `unsigned int` en una
operacion, el `int` se convierte a `unsigned int`. `-1` como unsigned es
`UINT_MAX` porque la representacion es la misma (complemento a dos) pero la
interpretacion cambia.

```c
/* Caso absurdo: compara int con puntero */
MAX(42, "hello");
/* Compila (con warning), pero compara un entero con
   la direccion de un string literal. Basura. */

/* Con funcion inline, el compilador rechaza tipos incompatibles: */
static inline int max_int(int a, int b) {
    return a > b ? a : b;
}
max_int(42, "hello");   /* ERROR de compilacion */
max_int(-1, 1);         /* Retorna 1 (correcto, ambos int) */
```

Para multiples tipos, se puede usar `_Generic` (C11):

```c
#define MAX(a, b) _Generic((a), \
    int:    max_int,            \
    double: max_double          \
)(a, b)
```

---

## 8 — Statement expressions de GCC

Las **statement expressions** (`({ ... })`) son una extension de GCC y Clang
que permiten usar un bloque de sentencias como una expresion. El valor de la
ultima sentencia (sin `;` final) es el valor de la expresion:

```c
#define MAX_SAFE(a, b) ({       \
    __typeof__(a) _a = (a);     \
    __typeof__(b) _b = (b);     \
    _a > _b ? _a : _b;         \
})
```

Resuelve la evaluacion multiple: cada argumento se evalua una sola vez y se
guarda en una variable temporal. `__typeof__` preserva el tipo original, asi que
la macro sigue siendo generica:

```c
int x = 5, y = 3;
int m = MAX_SAFE(x++, y++);
/* _a = x++ (x pasa a 6, _a = 5)
   _b = y++ (y pasa a 4, _b = 3)
   5 > 3 → _a → m = 5
   Resultado: m=5, x=6, y=4   (correcto) */

/* Funciona con double tambien: */
double d = MAX_SAFE(3.14, 2.71);   /* d = 3.14 */
```

Ejemplo practico — CLAMP:

```c
#define CLAMP(val, lo, hi) ({        \
    __typeof__(val) _val = (val);    \
    __typeof__(lo)  _lo  = (lo);     \
    __typeof__(hi)  _hi  = (hi);     \
    _val < _lo ? _lo :               \
    _val > _hi ? _hi : _val;         \
})

CLAMP(15, 0, 10)    /* → 10 (clamped al maximo) */
CLAMP(-5, 0, 10)    /* →  0 (clamped al minimo) */
CLAMP(7,  0, 10)    /* →  7 (dentro del rango) */
```

**Limitaciones:**
- No son estandar C — solo GCC y Clang
- Requieren `-std=gnu11` (no `-std=c11`) si se usa `typeof` sin guiones bajos
- `-Wpedantic` produce warnings
- No aparecen en el debugger (se expanden como texto)

Compilacion:

```bash
# Con statement expressions:
gcc -std=gnu11 -Wall -Wextra file.c -o file

# Con -std=c11 -Wpedantic → warnings sobre braced-groups
gcc -std=c11 -Wall -Wextra -Wpedantic file.c -o file
```

---

## 9 — Macros que rompen el flujo de control

Las macros que contienen `return`, `break`, `continue` o `goto` son
particularmente peligrosas porque el flujo de control es invisible para quien
lee el codigo:

```c
/* MAL: return oculto */
#define CHECK_NULL(p) if (!(p)) return NULL

void *process(void *data) {
    CHECK_NULL(data);    /* return oculto — no se ve al leer */
    /* ... */
}

/* Si se usa en una funcion que retorna int: */
int foo(void *p) {
    CHECK_NULL(p);    /* return NULL en funcion que retorna int */
    return 0;         /* → warning: incompatible return type */
}
```

Problemas:
1. El `return` es invisible — el lector ve `CHECK_NULL(p);` y piensa que es
   una verificacion, no una salida de la funcion
2. El tipo de retorno puede no coincidir con la funcion
3. El macro sin do-while rompe if/else (problema de la seccion 5)

Si realmente necesitas un macro con return (por ejemplo, para logging antes de
salir), al menos hazlo visible:

```c
#define CHECK_NULL(p) do { \
    if (!(p)) { \
        fprintf(stderr, "%s:%d: null pointer: %s\n", \
                __FILE__, __LINE__, #p); \
        return NULL; \
    } \
} while (0)
```

La regla general: evitar control de flujo en macros. Si necesitas un patron
repetitivo de verificacion, una funcion inline + un `if` explicito es mas claro:

```c
static inline int is_null(const void *p, const char *file, int line) {
    if (!p) fprintf(stderr, "%s:%d: null pointer\n", file, line);
    return !p;
}

/* Uso — el return es visible: */
if (is_null(data, __FILE__, __LINE__)) return NULL;
```

---

## 10 — Resumen: cuando usar macros vs alternativas

| Necesidad | Solucion | Por que |
|-----------|----------|---------|
| Valor constante | `const` o `enum` | Tienen tipo, scope, y aparecen en debugger |
| Funcion simple | `static inline` | Evaluacion unica, type-safe, debuggable |
| Genericidad de tipos | `_Generic` (C11) | Type-safe dispatch sin evaluacion multiple |
| Stringify (`#`) | Macro | Solo el preprocesador puede hacer `#param` |
| Token pasting (`##`) | Macro | Solo el preprocesador puede concatenar tokens |
| `__FILE__`/`__LINE__` del caller | Macro | inline obtiene su propia ubicacion, no la del caller |
| Compilacion condicional | `#ifdef` | Solo el preprocesador puede excluir codigo |

**Comparacion con Rust:**

Rust elimina todas estas trampas por diseno:

```rust
// Rust no tiene preprocesador de texto.
// macro_rules! es higienico: las variables del macro
// NO colisionan con variables del caller.
macro_rules! max {
    ($a:expr, $b:expr) => {{
        let a = $a;  // Evaluacion unica
        let b = $b;  // Evaluacion unica
        if a > b { a } else { b }
    }};
}

// Las variables 'a' y 'b' internas son invisibles
// para el codigo que llama al macro — higiene.
let a = 10;
let m = max!(a, 20);  // Sin colision de nombres
```

Diferencias clave:
- **Higiene**: las variables del macro no colisionan con las del caller
- **Evaluacion unica**: `let` evalua la expresion una vez
- **Pattern matching**: los macros de Rust operan sobre el AST, no sobre texto
- **Tipo verificado**: el compilador verifica tipos despues de la expansion
- **Sin precedencia**: la expansion produce un bloque `{{ }}`, no texto suelto

---

## Ejercicios

### Ejercicio 1 — Diagnosticar bugs de precedencia con gcc -E

```c
// Dado este macro:
#define AVERAGE(a, b)  a + b / 2

// 1. Predice que retorna AVERAGE(10, 20)
// 2. Predice que retorna 100 * AVERAGE(10, 20)
// 3. Usa gcc -E para verificar tus predicciones
// 4. Corrige el macro con la regla de parentesis completa
```

<details><summary>Prediccion</summary>

1. `AVERAGE(10, 20)` → `10 + 20 / 2` → `10 + 10` → `20` (correcto por casualidad)
2. `100 * AVERAGE(10, 20)` → `100 * 10 + 20 / 2` → `1000 + 10` → `1010` (esperado: 1500)
3. `gcc -E` muestra la expansion literal
4. Correccion: `#define AVERAGE(a, b) (((a) + (b)) / 2)`

</details>

### Ejercicio 2 — Demostrar evaluacion multiple con contador

```c
// Crear una funcion que cuente cuantas veces fue llamada:
// static int calls = 0;
// static int get_value(int v) { calls++; return v; }
//
// Usar MAX(get_value(10), get_value(20)) y verificar
// que calls == 3 (no 2).
// Comparar con max_inline(get_value(10), get_value(20))
// donde calls == 2.
```

<details><summary>Prediccion</summary>

Con `MAX(get_value(10), get_value(20))`:
- Expansion: `((get_value(10)) > (get_value(20)) ? (get_value(10)) : (get_value(20)))`
- Comparacion: llama 2 veces (una por argumento)
- Rama tomada: llama 1 vez mas (el ganador)
- Total: 3 llamadas, `calls == 3`

Con `max_inline(get_value(10), get_value(20))`:
- Los argumentos se evaluan una vez cada uno antes de la llamada
- Total: 2 llamadas, `calls == 2`

</details>

### Ejercicio 3 — do-while(0) con ASSERT

```c
// Crear un macro ASSERT(cond, msg) con do-while(0) que:
// - Verifique cond
// - Si falla: imprima archivo, linea, condicion (#cond) y msg a stderr
// - Llame a abort()
// Probar que funciona dentro de if/else sin llaves.
// Pista: #include <stdlib.h> para abort().
```

<details><summary>Prediccion</summary>

```c
#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: ASSERT(%s) failed: %s\n", \
                __FILE__, __LINE__, #cond, msg); \
        abort(); \
    } \
} while (0)

// Dentro de if/else:
if (x > 0)
    ASSERT(x < 100, "x out of range");
else
    printf("x is negative\n");

// Expande correctamente — el do-while(0) no rompe el else.
```

</details>

### Ejercicio 4 — Colision de nombres con SWAP

```c
// 1. Implementar SWAP con variable temporal llamada "tmp"
// 2. Demostrar que SWAP(tmp, x) falla (tmp colisiona)
// 3. Corregir usando un prefijo como _swap_tmp
// 4. Verificar que SWAP(tmp, x) ahora funciona
// Pista: imprimir los valores antes y despues del swap.
```

<details><summary>Prediccion</summary>

Con `tmp` como nombre de variable temporal:
```c
int tmp = 10, x = 20;
SWAP(tmp, x);
// Expande a: int tmp = (tmp);  ← se inicializa consigo mismo (basura)
// Resultado: tmp toma valor indeterminado, x recibe basura
```

Con `_swap_tmp`:
```c
int tmp = 10, x = 20;
SWAP(tmp, x);
// Expande a: __typeof__(tmp) _swap_tmp = (tmp);  ← usa el tmp externo
// Resultado: tmp=20, x=10  (correcto)
```

</details>

### Ejercicio 5 — signed vs unsigned en macro MAX

```c
// 1. Usar MAX(-1, 1u) e imprimir el resultado como %d y como %u
// 2. Explicar por que MAX dice que -1 > 1u
// 3. Comparar con max_int(-1, 1) que retorna 1 correctamente
// 4. Bonus: que retorna MAX(-1, (unsigned int)0)?
```

<details><summary>Prediccion</summary>

1. `MAX(-1, 1u)` retorna `-1` como `%d`, o `4294967295` como `%u`
2. La comparacion `(int)-1 > (unsigned)1u` se hace en unsigned. `-1` se convierte a `UINT_MAX` (4294967295). `UINT_MAX > 1` → true → retorna el primer argumento (`-1`)
3. `max_int(-1, 1)` → comparacion en int → `-1 > 1` es false → retorna `1`
4. `MAX(-1, 0u)` → mismo efecto: `-1` como unsigned es `UINT_MAX > 0` → retorna `-1`

</details>

### Ejercicio 6 — Tres versiones de LOG en if/else

```c
// Implementar tres versiones de un macro TRACE(msg):
// 1. LOG_NAKED: dos printf separados (sin wrapper)
// 2. LOG_BRACES: envuelto en { }
// 3. LOG_SAFE: envuelto en do { } while (0)
//
// Probar cada uno dentro de: if (cond) TRACE("x"); else TRACE("y");
// Solo LOG_SAFE compilara correctamente.
// Nota: LOG_NAKED y LOG_BRACES deben probarse standalone para verificar
// que funcionan fuera de if/else.
```

<details><summary>Prediccion</summary>

- `LOG_NAKED` no compila en if/else: el segundo printf queda fuera del if, el `;` crea una sentencia vacia, y el else queda sin if
- `LOG_BRACES` no compila en if/else: `{ ... };` — el `;` despues del `}` cierra el if, dejando el else huerfano
- `LOG_SAFE` compila y funciona: `do { ... } while (0);` es una sentencia completa que absorbe el `;`
- Las tres funcionan si el caller usa llaves: `if (cond) { TRACE("x"); }`

</details>

### Ejercicio 7 — Statement expression para SQUARE seguro

```c
// 1. Compilar con -std=gnu11:
//    #define SQUARE_SAFE(x) ({          \
//        __typeof__(x) _x = (x);       \
//        _x * _x;                      \
//    })
//
// 2. Verificar que SQUARE_SAFE(x++) da result=16, x=5 (con x=4)
// 3. Comparar con SQUARE_UNSAFE(x++) = ((x++) * (x++))
// 4. Intentar compilar con -std=c11 -Wpedantic y observar los warnings
```

<details><summary>Prediccion</summary>

- `SQUARE_SAFE(x++)` con x=4: `_x = x++` → `_x = 4`, `x = 5`. Luego `_x * _x` = 16. Resultado: `result=16, x=5` (correcto)
- `SQUARE_UNSAFE(x++)` con x=4: `((x++) * (x++))` → undefined behavior (dos modificaciones sin sequence point). GCC warning: `operation on 'x' may be undefined [-Wsequence-point]`. Resultado tipico: `result=20, x=6` pero no garantizado
- Con `-std=c11 -Wpedantic`: warning `ISO C forbids braced-groups within expressions`, pero compila si se usa `__typeof__` (doble guion bajo)

</details>

### Ejercicio 8 — Macro con return oculto

```c
// 1. Definir:
//    #define RETURN_IF_NULL(p) if (!(p)) return -1
//
// 2. Usar en una funcion int process(int *data)
//    que retorna 0 en caso de exito.
// 3. Demostrar que funciona para la funcionalidad basica.
// 4. Luego mostrar el problema: usar dentro de
//    if (verbose) RETURN_IF_NULL(ptr); else do_something();
//    ¿Compila? ¿Que pasa con el else?
// 5. Reescribir con do-while(0) y fprintf para hacer
//    visible el return.
```

<details><summary>Prediccion</summary>

Sin do-while(0), `RETURN_IF_NULL(ptr)` en if/else:
```c
if (verbose)
    if (!(ptr)) return -1;    // el macro expande su propio if
else                          // este else se asocia al if INTERNO
    do_something();           // no al if (verbose) externo
```
El else se asocia al if mas cercano ("dangling else") — el del macro, no el de `verbose`. El codigo compila pero la logica es incorrecta.

Con do-while(0): `do { if (!(ptr)) { fprintf(...); return -1; } } while (0);` — funciona correctamente en cualquier contexto.

</details>

### Ejercicio 9 — CLAMP generico con statement expression

```c
// 1. Implementar CLAMP(val, lo, hi) con statement expression
//    que funcione con int y double
// 2. Probar: CLAMP(15, 0, 10) → 10
//            CLAMP(-5, 0, 10) → 0
//            CLAMP(7, 0, 10) → 7
//            CLAMP(3.14, 0.0, 1.0) → 1.0
// 3. Implementar clamp_int como funcion inline
// 4. Verificar que clamp_int NO funciona con double
//    (trunca o da warning)
// Compilar con: gcc -std=gnu11 -Wall -Wextra
```

<details><summary>Prediccion</summary>

```c
#define CLAMP(val, lo, hi) ({        \
    __typeof__(val) _v = (val);      \
    __typeof__(lo)  _l = (lo);       \
    __typeof__(hi)  _h = (hi);       \
    _v < _l ? _l : _v > _h ? _h : _v; \
})
```

- `CLAMP(15, 0, 10)` → `_v=15`, `15 < 0` false, `15 > 10` true → `10`
- `CLAMP(-5, 0, 10)` → `_v=-5`, `-5 < 0` true → `0`
- `CLAMP(7, 0, 10)` → `_v=7`, `7 < 0` false, `7 > 10` false → `7`
- `CLAMP(3.14, 0.0, 1.0)` → `_v=3.14` (double), `3.14 > 1.0` true → `1.0`
- `clamp_int(3.14, 0, 1)` → trunca `3.14` a `3`, retorna `1` (con warning de conversion implicit)

La ventaja de la statement expression es la genericidad de tipos sin duplicar funciones.

</details>

### Ejercicio 10 — Corregir macros con bugs

```c
// Cada macro tiene uno o mas bugs. Identificar y corregir:
//
// A) #define DOUBLE(x)  x + x
// B) #define IS_EVEN(n) n % 2 == 0
// C) #define PRINT_VAR(x) printf("x = %d\n", x)
// D) #define SWAP(a, b) int tmp = a; a = b; b = tmp;
// E) #define MIN(a, b) (a) < (b) ? (a) : (b)
//
// Para cada uno:
// 1. Identificar los bugs
// 2. Dar un ejemplo de uso que produzca resultado incorrecto
// 3. Escribir la version corregida
```

<details><summary>Prediccion</summary>

**A)** `DOUBLE(x)  x + x`
- Bug: sin parentesis en parametros ni expresion
- Falla: `2 * DOUBLE(3)` → `2 * 3 + 3` = 9 (no 12)
- Correccion: `#define DOUBLE(x) ((x) + (x))`

**B)** `IS_EVEN(n) n % 2 == 0`
- Bug: sin parentesis — `|` y `&` tienen menor precedencia que `%` y `==`
- Falla: `IS_EVEN(4 | 2)` → `4 | 2 % 2 == 0` → `4 | (0 == 0)` → `4 | 1` = 5 (no 1)
- Correccion: `#define IS_EVEN(n) (((n) % 2) == 0)`

**C)** `PRINT_VAR(x) printf("x = %d\n", x)`
- Bug 1: el string literal `"x = %d\n"` siempre imprime la letra `x`, no el nombre de la variable
- Bug 2: sin do-while(0) (aunque es una sola sentencia, falta envoltura de expresion)
- Correccion: `#define PRINT_VAR(x) do { printf(#x " = %d\n", (x)); } while (0)` — usa stringify (#x) para imprimir el nombre real

**D)** `SWAP(a, b) int tmp = a; a = b; b = tmp;`
- Bug 1: tres sentencias sueltas sin do-while(0) — rompe if/else
- Bug 2: `tmp` colisiona si el caller tiene variable `tmp`
- Bug 3: tipo fijo `int` — no funciona con otros tipos
- Bug 4: sin parentesis en argumentos
- Correccion:
```c
#define SWAP(a, b) do { \
    __typeof__(a) _swap_tmp = (a); \
    (a) = (b); \
    (b) = _swap_tmp; \
} while (0)
```

**E)** `MIN(a, b) (a) < (b) ? (a) : (b)`
- Bug 1: sin parentesis en la expresion completa
- Falla: `int m = 10 + MIN(3, 5)` → `10 + (3) < (5) ? (3) : (5)` → `13 < 5` → false → `5` (no 13)
- Bug 2: evaluacion multiple (a y b aparecen dos veces)
- Correccion (minima): `#define MIN(a, b) ((a) < (b) ? (a) : (b))` — agrega parentesis externos (evaluacion multiple persiste)
- Correccion (completa, GCC): usar statement expression con `__typeof__`

</details>
