# T01 — Paréntesis balanceados

## Índice

1. [El problema](#1-el-problema)
2. [Por qué una pila](#2-por-qué-una-pila)
3. [Algoritmo para un solo tipo de delimitador](#3-algoritmo-para-un-solo-tipo-de-delimitador)
4. [Extensión a múltiples delimitadores](#4-extensión-a-múltiples-delimitadores)
5. [Implementación en C](#5-implementación-en-c)
6. [Implementación en Rust](#6-implementación-en-rust)
7. [Reporte de errores con posición](#7-reporte-de-errores-con-posición)
8. [Análisis de complejidad](#8-análisis-de-complejidad)
9. [Aplicaciones](#9-aplicaciones)
10. [Ejercicios](#10-ejercicios)

---

## 1. El problema

Una cadena de delimitadores está **balanceada** si y solo si:

1. Cada delimitador de apertura tiene exactamente un delimitador de cierre correspondiente.
2. Los pares están **correctamente anidados**: no se cruzan entre sí.
3. No hay delimitadores de cierre sin su apertura correspondiente.

Ejemplos:

| Cadena | Balanceada | Razón |
|--------|:----------:|-------|
| `()` | Sí | Par simple |
| `([{}])` | Sí | Anidamiento correcto |
| `()[]{}` | Sí | Pares adyacentes |
| `([{}()])` | Sí | Anidamiento mixto |
| `(]` | No | Tipo de cierre incorrecto |
| `([)]` | No | Cruce: `[` se cierra después de `)` |
| `(` | No | Apertura sin cierre |
| `)` | No | Cierre sin apertura |
| `` (vacía) | Sí | Caso base: cero delimitadores |

Este problema aparece constantemente en compiladores, editores de texto, validadores
de HTML/XML, y cualquier sistema que procese estructuras anidadas.

### Definición formal

Sea $s$ una cadena sobre el alfabeto $\{(, ), [, ], \{, \}\}$. Definimos $s$ como
balanceada recursivamente:

- La cadena vacía $\varepsilon$ es balanceada.
- Si $s$ es balanceada, entonces $(s)$, $[s]$ y $\{s\}$ son balanceadas.
- Si $s_1$ y $s_2$ son balanceadas, entonces $s_1 s_2$ es balanceada.
- Ninguna otra cadena es balanceada.

---

## 2. Por qué una pila

El anidamiento tiene una propiedad fundamental: **el último delimitador abierto es
el primero que debe cerrarse** — exactamente el principio LIFO.

Traza visual con la cadena `([{}])`:

```
Carácter    Acción          Pila (top →)
─────────────────────────────────────────
   (        push '('        (
   [        push '['        (  [
   {        push '{'        (  [  {
   }        pop '{', match   (  [
   ]        pop '[', match   (
   )        pop '(', match   ∅
   FIN      pila vacía → ✓ balanceada
```

Traza con cadena incorrecta `([)]`:

```
Carácter    Acción              Pila (top →)
──────────────────────────────────────────────
   (        push '('            (
   [        push '['            (  [
   )        pop '[', ¿match?    '[' ≠ ')' → ✗ ERROR
```

El tercer carácter `)` necesita cerrar un `(`, pero el tope de la pila es `[`.
La pila detecta el cruce de forma inmediata.

### ¿Por qué no basta un contador?

Para un solo tipo de delimitador (solo paréntesis), un simple contador funciona:
incrementar al ver `(`, decrementar al ver `)`, verificar que nunca sea negativo
y que termine en cero.

Pero con múltiples tipos de delimitadores, un contador por tipo **no detecta
cruces**. La cadena `([)]` tendría contadores correctos (cada tipo suma cero),
pero está mal anidada. Solo una pila preserva el **orden** de apertura.

---

## 3. Algoritmo para un solo tipo de delimitador

Antes de generalizar, conviene entender la versión simple con solo `(` y `)`:

```
FUNCIÓN is_balanced_parens(s):
    count ← 0
    PARA CADA c EN s:
        SI c = '(':
            count ← count + 1
        SI c = ')':
            SI count = 0:
                RETORNAR falso      // cierre sin apertura
            count ← count - 1
    RETORNAR count = 0              // aperturas sin cierre
```

Esta versión usa $O(1)$ de memoria porque solo necesita rastrear la profundidad,
no qué tipo de delimitador está abierto. Es un caso especial donde la pila se
degrada a un contador.

---

## 4. Extensión a múltiples delimitadores

Con `()`, `[]` y `{}`, necesitamos la pila completa:

```
FUNCIÓN is_balanced(s):
    stack ← pila vacía
    PARA CADA c EN s:
        SI c ∈ {'(', '[', '{'}:
            push(stack, c)
        SI c ∈ {')', ']', '}'}:
            SI is_empty(stack):
                RETORNAR falso          // cierre sin apertura
            open ← pop(stack)
            SI NOT matches(open, c):
                RETORNAR falso          // tipo incorrecto
    RETORNAR is_empty(stack)            // ¿quedan aperturas?

FUNCIÓN matches(open, close):
    RETORNAR (open='(' AND close=')')
        OR  (open='[' AND close=']')
        OR  (open='{' AND close='}')
```

### Casos límite

| Caso | Resultado | Razón |
|------|:---------:|-------|
| Cadena vacía | Balanceada | Cero delimitadores, cero errores |
| Solo aperturas `(((` | No balanceada | Pila no vacía al final |
| Solo cierres `)))` | No balanceada | Pop en pila vacía |
| Caracteres mixtos `a*(b+c)` | Depende | Ignorar no-delimitadores |
| Un solo par `()` | Balanceada | Caso mínimo |
| Profundidad extrema | Balanceada | Limitado por memoria de la pila |

El algoritmo puede **ignorar caracteres que no son delimitadores** simplemente
no haciendo nada con ellos en el bucle. Esto permite verificar expresiones
completas como `func(arr[i], map{key: val})`.

---

## 5. Implementación en C

### Pila de caracteres simple

Para este problema no necesitamos una pila genérica; una pila especializada
para `char` es más directa:

```c
/* balanced.c */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* --- Char stack ------------------------------------------- */

typedef struct {
    char *data;
    int   top;
    int   capacity;
} CharStack;

CharStack *charstack_create(int capacity) {
    CharStack *s = malloc(sizeof(CharStack));
    if (!s) return NULL;
    s->data = malloc(capacity * sizeof(char));
    if (!s->data) { free(s); return NULL; }
    s->top = 0;
    s->capacity = capacity;
    return s;
}

void charstack_destroy(CharStack *s) {
    if (s) {
        free(s->data);
        free(s);
    }
}

bool charstack_push(CharStack *s, char c) {
    if (s->top >= s->capacity) return false;
    s->data[s->top++] = c;
    return true;
}

bool charstack_pop(CharStack *s, char *out) {
    if (s->top == 0) return false;
    *out = s->data[--s->top];
    return true;
}

bool charstack_is_empty(const CharStack *s) {
    return s->top == 0;
}

/* --- Balanced check --------------------------------------- */

static bool is_opening(char c) {
    return c == '(' || c == '[' || c == '{';
}

static bool is_closing(char c) {
    return c == ')' || c == ']' || c == '}';
}

static char matching_open(char close) {
    switch (close) {
        case ')': return '(';
        case ']': return '[';
        case '}': return '{';
        default:  return '\0';
    }
}

bool is_balanced(const char *expr) {
    int len = strlen(expr);
    CharStack *stack = charstack_create(len);  /* peor caso: todo aperturas */
    if (!stack) return false;

    for (int i = 0; expr[i] != '\0'; i++) {
        char c = expr[i];

        if (is_opening(c)) {
            charstack_push(stack, c);
        } else if (is_closing(c)) {
            char top;
            if (!charstack_pop(stack, &top)) {
                /* pila vacía: cierre sin apertura */
                charstack_destroy(stack);
                return false;
            }
            if (top != matching_open(c)) {
                /* tipo incorrecto */
                charstack_destroy(stack);
                return false;
            }
        }
        /* caracteres no-delimitadores: ignorar */
    }

    bool result = charstack_is_empty(stack);
    charstack_destroy(stack);
    return result;
}

/* --- Main ------------------------------------------------- */

int main(void) {
    const char *tests[] = {
        "()",       "([{}])",   "()[]{}",
        "(]",       "([)]",     "(",
        ")",        "",         "a*(b+c)",
        "{[()]}",   "((()))",   "({[}])",
    };
    int n = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < n; i++) {
        printf("%-15s -> %s\n",
               tests[i][0] ? tests[i] : "(empty)",
               is_balanced(tests[i]) ? "balanced" : "NOT balanced");
    }

    return 0;
}
```

Salida esperada:

```
()              -> balanced
([{}])          -> balanced
()[]{}          -> balanced
(]              -> NOT balanced
([)]            -> NOT balanced
(               -> NOT balanced
)               -> NOT balanced
(empty)         -> balanced
a*(b+c)         -> balanced
{[()]}          -> balanced
((()))          -> balanced
({[}])          -> NOT balanced
```

### Anatomía del flujo de `is_balanced`

```
is_balanced("([)]")
│
├─ i=0, c='(' → push '('         pila: (
├─ i=1, c='[' → push '['         pila: ( [
├─ i=2, c=')' → pop → '['
│               matching_open(')') = '('
│               '[' ≠ '(' → return false ✗
│
└─ charstack_destroy (limpieza antes del return)
```

Punto clave: **cada camino de retorno temprano debe llamar a
`charstack_destroy`**. Olvidar esto es el error de memoria más común en esta
implementación. En C no hay destructores automáticos; cada `return` necesita
su `free`.

### Compilación

```bash
gcc -std=c11 -Wall -Wextra -o balanced balanced.c
./balanced
```

---

## 6. Implementación en Rust

En Rust, `Vec<char>` actúa como pila perfecta y no hay riesgo de fugas de
memoria:

```rust
// balanced.rs

fn is_balanced(expr: &str) -> bool {
    let mut stack: Vec<char> = Vec::new();

    for c in expr.chars() {
        match c {
            '(' | '[' | '{' => stack.push(c),
            ')' | ']' | '}' => {
                match stack.pop() {
                    None => return false,           // close without open
                    Some(open) => {
                        if !matches(open, c) {
                            return false;           // wrong type
                        }
                    }
                }
            }
            _ => {}                                 // ignore non-delimiters
        }
    }

    stack.is_empty()
}

fn matches(open: char, close: char) -> bool {
    match (open, close) {
        ('(', ')') | ('[', ']') | ('{', '}') => true,
        _ => false,
    }
}

fn main() {
    let tests = [
        "()",       "([{}])",   "()[]{}",
        "(]",       "([)]",     "(",
        ")",        "",         "a*(b+c)",
        "{[()]}",   "((()))",   "({[}])",
    ];

    for expr in &tests {
        let label = if expr.is_empty() { "(empty)" } else { expr };
        let result = if is_balanced(expr) { "balanced" } else { "NOT balanced" };
        println!("{:<15} -> {}", label, result);
    }
}
```

### Comparación C vs Rust

| Aspecto | C | Rust |
|---------|---|------|
| Pila | `CharStack` manual (malloc/free) | `Vec<char>` estándar |
| Pop vacío | Retorno `bool` + puntero `out` | `Option<char>` (None) |
| Limpieza | `destroy` en cada `return` | Automática (Drop) |
| Return temprano | Riesgo de leak si se olvida `destroy` | Seguro siempre |
| Matching | `switch` con `default` | `match` exhaustivo |
| Caracteres | `char` (1 byte, ASCII) | `char` (4 bytes, Unicode) |
| Líneas | ~80 | ~35 |

La función `matches` en Rust usa un patrón de tupla `(open, close)` que es
más expresivo que el `switch` de C. El compilador verifica exhaustividad:
si se añade un delimitador nuevo y no se actualiza el `match`, el código no
compila.

### Nota sobre Unicode

La versión Rust maneja correctamente caracteres Unicode como delimitadores
CJK (「」, 【】) si se extiende el `match`. La versión C, al operar sobre
`char` de 1 byte, requeriría manejar codificaciones multibyte para lo mismo.

---

## 7. Reporte de errores con posición

Una función que solo retorna `true`/`false` es útil para validación, pero
un compilador o editor necesita **dónde** está el error. Podemos devolver
información más rica:

### En C

```c
typedef enum {
    BALANCE_OK,
    BALANCE_UNEXPECTED_CLOSE,   /* cierre sin apertura */
    BALANCE_MISMATCH,           /* tipo incorrecto */
    BALANCE_UNCLOSED,           /* apertura sin cierre */
} BalanceError;

typedef struct {
    BalanceError error;
    int          position;      /* índice del carácter problemático */
    char         expected;      /* qué se esperaba (para MISMATCH) */
    char         found;         /* qué se encontró */
} BalanceResult;

BalanceResult check_balanced(const char *expr) {
    int len = strlen(expr);
    CharStack *stack = charstack_create(len);
    /* también necesitamos guardar posiciones */
    int *positions = malloc(len * sizeof(int));
    BalanceResult result = { .error = BALANCE_OK, .position = -1 };

    for (int i = 0; expr[i] != '\0'; i++) {
        char c = expr[i];

        if (is_opening(c)) {
            charstack_push(stack, c);
            positions[stack->top - 1] = i;
        } else if (is_closing(c)) {
            char top;
            if (!charstack_pop(stack, &top)) {
                result = (BalanceResult){
                    BALANCE_UNEXPECTED_CLOSE, i, '\0', c
                };
                goto cleanup;
            }
            char expected_close = matching_close(top);
            if (c != expected_close) {
                result = (BalanceResult){
                    BALANCE_MISMATCH, i, expected_close, c
                };
                goto cleanup;
            }
        }
    }

    if (!charstack_is_empty(stack)) {
        /* la apertura sin cerrar está en positions[top-1] */
        result = (BalanceResult){
            BALANCE_UNCLOSED,
            positions[stack->top - 1],
            '\0',
            stack->data[stack->top - 1]
        };
    }

cleanup:
    charstack_destroy(stack);
    free(positions);
    return result;
}
```

Aquí el `goto cleanup` es un patrón estándar en C para centralizar la
liberación de recursos. Es preferible a duplicar `free` en cada punto de
retorno.

### En Rust

```rust
#[derive(Debug, PartialEq)]
enum BalanceError {
    UnexpectedClose { position: usize, found: char },
    Mismatch { position: usize, expected: char, found: char },
    Unclosed { position: usize, opener: char },
}

fn check_balanced(expr: &str) -> Result<(), BalanceError> {
    let mut stack: Vec<(char, usize)> = Vec::new(); // (delimiter, position)

    for (i, c) in expr.chars().enumerate() {
        match c {
            '(' | '[' | '{' => stack.push((c, i)),
            ')' | ']' | '}' => {
                match stack.pop() {
                    None => {
                        return Err(BalanceError::UnexpectedClose {
                            position: i,
                            found: c,
                        });
                    }
                    Some((open, _pos)) => {
                        let expected = matching_close(open);
                        if c != expected {
                            return Err(BalanceError::Mismatch {
                                position: i,
                                expected,
                                found: c,
                            });
                        }
                    }
                }
            }
            _ => {}
        }
    }

    match stack.last() {
        Some(&(opener, position)) => {
            Err(BalanceError::Unclosed { position, opener })
        }
        None => Ok(()),
    }
}

fn matching_close(open: char) -> char {
    match open {
        '(' => ')',
        '[' => ']',
        '{' => '}',
        _ => unreachable!(),
    }
}
```

La versión Rust usa `Result<(), BalanceError>` — el tipo idiomático para
operaciones que pueden fallar. No hay `goto`, no hay `free`, no hay array
auxiliar de posiciones separado (la posición viaja junto al delimitador en
la tupla).

Ejemplo de uso:

```rust
match check_balanced("foo(bar[baz)qux]") {
    Ok(()) => println!("Balanced"),
    Err(e) => println!("Error: {:?}", e),
}
// Error: Mismatch { position: 11, expected: ']', found: ')' }
```

El mensaje indica exactamente: en la posición 11, se esperaba `]` pero se
encontró `)`.

---

## 8. Análisis de complejidad

### Tiempo

Cada carácter de la cadena se procesa exactamente una vez. Cada carácter
provoca a lo sumo una operación de pila (push o pop), que es $O(1)$.

$$T(n) = O(n)$$

donde $n$ es la longitud de la cadena. No hay forma de hacerlo más rápido:
hay que leer cada carácter al menos una vez ($\Omega(n)$), así que el
algoritmo es **óptimo**.

### Espacio

En el peor caso (todos aperturas, como `((((...`), la pila almacena $n$
elementos:

$$S(n) = O(n)$$

En el caso promedio para código real, la profundidad máxima de anidamiento
es mucho menor que $n$. Para código fuente típico, la profundidad rara vez
supera 10-20 niveles, así que en la práctica el uso de memoria es
efectivamente constante.

### Tabla resumen

| Operación | Tiempo | Espacio |
|-----------|:------:|:-------:|
| Verificación completa | $O(n)$ | $O(n)$ peor caso |
| Push/pop individual | $O(1)$ | $O(1)$ |
| Caso promedio (código real) | $O(n)$ | $O(d)$ con $d \ll n$ |

Donde $d$ es la profundidad máxima de anidamiento.

---

## 9. Aplicaciones

### Compiladores y parsers

La verificación de delimitadores es una de las primeras fases del análisis
léxico/sintáctico. Un compilador la ejecuta antes de intentar construir el
árbol de sintaxis abstracta (AST). Si los delimitadores no están balanceados,
el parser no puede producir un árbol válido.

### Editores de texto e IDEs

Los editores modernos resaltan el par correspondiente cuando el cursor está
sobre un delimitador. Internamente usan una variante del algoritmo de pila
para encontrar la pareja. También detectan errores en tiempo real conforme
el usuario escribe.

### Validación de HTML/XML

Las etiquetas de apertura (`<div>`) y cierre (`</div>`) siguen el mismo
principio de anidamiento. La extensión del algoritmo reemplaza caracteres
por nombres de etiquetas:

```
push("div") → push("p") → pop espera "p" → pop espera "div"
```

### Expresiones matemáticas

Antes de evaluar `3 * (2 + (4 - 1))`, hay que verificar que los paréntesis
estén balanceados. Esto es un paso previo a la conversión a notación posfija
que se verá en los siguientes tópicos.

---

## 10. Ejercicios

Todos los ejercicios usan las implementaciones mostradas en las secciones 5
y 6 como punto de partida.

---

### Ejercicio 1 — Traza manual con pila

Realiza la traza del algoritmo para la cadena `{[()][({})]}`:

Para cada carácter, anota: el carácter, la acción (push/pop/match), y el
estado de la pila después de la acción.

<details>
<summary>¿Cuántos elementos tendrá la pila como máximo durante la traza?</summary>

Traza completa:

```
Carácter    Acción            Pila (top →)
──────────────────────────────────────────
   {        push '{'          {
   [        push '['          { [
   (        push '('          { [ (
   )        pop '(', match    { [
   ]        pop '[', match    {
   [        push '['          { [
   (        push '('          { [ (
   {        push '{'          { [ ( {
   }        pop '{', match    { [ (
   )        pop '(', match    { [
   ]        pop '[', match    {
   }        pop '{', match    ∅
```

La pila alcanza un máximo de **4 elementos** (en la posición del `{` interno:
`{ [ ( {`). La cadena está balanceada porque la pila termina vacía.

</details>

---

### Ejercicio 2 — Identificar el error

Para cada cadena, predice **qué tipo de error** se detecta y **en qué posición**
(índice base 0):

```
a)  "([)]"
b)  "((("
c)  "}"
d)  "(hello]"
```

<details>
<summary>¿Qué cadena produce un error de tipo Mismatch y cuál un Unclosed?</summary>

```
a)  "([)]"    → Mismatch en posición 2: se esperaba ']', se encontró ')'
               (la pila tiene '(' '[', se hace pop '[', no coincide con ')')
b)  "((("     → Unclosed en posición 2: el '(' en posición 2 nunca se cierra
               (la pila termina con 3 elementos)
c)  "}"       → UnexpectedClose en posición 0: cierre sin apertura
               (la pila está vacía cuando se encuentra '}')
d)  "(hello]" → Mismatch en posición 6: se esperaba ')', se encontró ']'
               (la pila tiene '(', se hace pop '(', no coincide con ']')
```

Mismatch: cadenas (a) y (d). Unclosed: cadena (b). UnexpectedClose: cadena (c).

</details>

---

### Ejercicio 3 — Solo paréntesis con contador

Implementa en C una función `is_balanced_parens` que solo verifique `(` y `)`
usando un **contador entero**, sin pila. Verifica con las cadenas: `"(())"`,
`"())("`, `"((()"`, `""`.

<details>
<summary>¿Puede el contador detectar el caso "())("? ¿Por qué?</summary>

Sí. La función:

```c
bool is_balanced_parens(const char *expr) {
    int count = 0;
    for (int i = 0; expr[i] != '\0'; i++) {
        if (expr[i] == '(') count++;
        else if (expr[i] == ')') {
            if (count == 0) return false;  // key check
            count--;
        }
    }
    return count == 0;
}
```

Con `"())("`: al procesar el segundo `)` (posición 2), `count` ya es 0 →
retorna `false` inmediatamente. Sin la verificación `count == 0`, el
contador llegaría a $-1$ y luego volvería a 0 al final, dando un falso
positivo. La clave es que el contador **nunca debe volverse negativo**.

</details>

---

### Ejercicio 4 — Extensión a ángulos

Extiende la implementación en Rust para que también reconozca `<` y `>` como
delimitadores. Prueba con: `"Vec<Option<i32>>"`, `"a < b && c > d"`,
`"<([])>"`.

<details>
<summary>¿Qué problema tiene usar `<` y `>` como delimitadores en código real?</summary>

El problema es la **ambigüedad**: `<` y `>` también son operadores de
comparación. La cadena `"a < b && c > d"` no contiene delimitadores anidados
sino dos comparaciones, pero el algoritmo ingenuo la trataría como
delimitadores.

Extensión:

```rust
fn matches(open: char, close: char) -> bool {
    match (open, close) {
        ('(', ')') | ('[', ']') | ('{', '}') | ('<', '>') => true,
        _ => false,
    }
}
```

Con los tests:
- `"Vec<Option<i32>>"` → balanceada (si se tratan como delimitadores)
- `"a < b && c > d"` → **no balanceada** (falso negativo: interpreta `>` como
  cierre de `<`, pero el `d` intermedio no es un delimitador)
- `"<([])>"` → balanceada

Los compiladores reales de C++ y Rust resuelven esto con **análisis contextual**:
`<` solo es delimitador en contextos de tipo/genéricos, no en expresiones. Esto
requiere un parser completo, no solo un verificador de paréntesis.

</details>

---

### Ejercicio 5 — Profundidad máxima

Implementa en Rust una función `max_depth(expr: &str) -> usize` que retorne
la profundidad máxima de anidamiento. Por ejemplo: `"(())"` → 2, `"()()"` → 1,
`"((()))"` → 3, `""` → 0.

<details>
<summary>¿Necesitas almacenar los delimitadores o basta un contador?</summary>

Si solo necesitas la profundidad (sin verificar tipos), basta un contador:

```rust
fn max_depth(expr: &str) -> usize {
    let mut depth: usize = 0;
    let mut max: usize = 0;
    for c in expr.chars() {
        match c {
            '(' | '[' | '{' => {
                depth += 1;
                if depth > max {
                    max = depth;
                }
            }
            ')' | ']' | '}' => {
                depth = depth.saturating_sub(1);
            }
            _ => {}
        }
    }
    max
}
```

No necesitas la pila porque la profundidad solo depende de cuántos
delimitadores están abiertos simultáneamente, no de cuáles son. El
`saturating_sub` evita underflow si la cadena está desbalanceada (un cierre
extra no produce panic).

Si también necesitas verificar correctitud, combina esta función con
`check_balanced` — primero verifica, luego mide profundidad.

</details>

---

### Ejercicio 6 — Posición del error en C

Modifica la implementación C de `check_balanced` (sección 7) para que
imprima un mensaje de error visual con un indicador `^` debajo del carácter
problemático:

```
Error en: foo(bar[baz)qux]
                    ^
Esperado: ']', encontrado: ')'
```

<details>
<summary>¿Cómo calculas la cantidad de espacios antes del `^`?</summary>

El campo `position` de `BalanceResult` da el índice del carácter. Para
imprimir el indicador:

```c
void print_error(const char *expr, BalanceResult res) {
    if (res.error == BALANCE_OK) {
        printf("OK: %s\n", expr);
        return;
    }

    printf("Error en: %s\n", expr);
    /* "Error en: " tiene 10 caracteres */
    printf("          ");
    for (int i = 0; i < res.position; i++) {
        printf(" ");
    }
    printf("^\n");

    switch (res.error) {
        case BALANCE_UNEXPECTED_CLOSE:
            printf("Cierre '%c' sin apertura\n", res.found);
            break;
        case BALANCE_MISMATCH:
            printf("Esperado: '%c', encontrado: '%c'\n",
                   res.expected, res.found);
            break;
        case BALANCE_UNCLOSED:
            printf("'%c' en posicion %d nunca se cierra\n",
                   res.found, res.position);
            break;
        default:
            break;
    }
}
```

La cantidad de espacios es exactamente `res.position`. El prefijo
`"Error en: "` tiene longitud fija (10 caracteres) y se compensa con los
10 espacios iniciales de la segunda línea.

</details>

---

### Ejercicio 7 — Eliminar delimitadores mínimos

Dada una cadena no balanceada, calcula el **número mínimo de delimitadores
que hay que eliminar** para que sea balanceada. Ejemplo: `"(])()"` → 2
(eliminar `]` en posición 1 y `)` en posición 2, o `(` en posición 0 y
`]` en posición 1).

<details>
<summary>¿Es el resultado siempre = delimitadores sin pareja?</summary>

Sí. El número mínimo de eliminaciones es exactamente el número de
delimitadores que quedan sin pareja. El algoritmo:

```rust
fn min_removals(expr: &str) -> usize {
    let mut stack: Vec<char> = Vec::new();
    let mut removals: usize = 0;

    for c in expr.chars() {
        match c {
            '(' | '[' | '{' => stack.push(c),
            ')' | ']' | '}' => {
                if let Some(&top) = stack.last() {
                    if matches_delim(top, c) {
                        stack.pop();    // valid pair, consume it
                    } else {
                        removals += 1;  // mismatch: must remove close
                    }
                } else {
                    removals += 1;      // no opener: must remove close
                }
            }
            _ => {}
        }
    }

    removals + stack.len()  // remaining openers also need removal
}
```

Para `"(])()"`:
- `(` → push. Pila: `(`
- `]` → top es `(`, no coincide → removals=1
- `)` → top es `(`, coincide → pop. Pila: vacía
- `(` → push. Pila: `(`
- `)` → top es `(`, coincide → pop. Pila: vacía
- Resultado: removals(1) + pila(0) = **1**

Nota: una implementación alternativa podría dar 2 dependiendo de la
estrategia de emparejamiento. El problema general de mínimas eliminaciones
para múltiples tipos de delimitadores es más complejo que para un solo tipo
y puede requerir programación dinámica para la solución óptima.

</details>

---

### Ejercicio 8 — Verificador de archivos en C

Escribe un programa en C que lea un archivo fuente desde `argv[1]` carácter
por carácter (`fgetc`), cuente las líneas, y reporte errores de balanceo
indicando **línea y columna** en lugar de posición absoluta.

<details>
<summary>¿Qué necesitas rastrear además del delimitador en la pila?</summary>

Necesitas almacenar una estructura con tres campos por cada apertura:

```c
typedef struct {
    char delimiter;
    int  line;
    int  column;
} StackEntry;
```

La pila almacena `StackEntry` en lugar de `char`. Cada vez que ves `\n`,
incrementas `line` y reseteas `column` a 0. Cuando haces push, guardas la
línea y columna actuales junto con el delimitador.

```c
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <archivo>\n", argv[0]);
        return 1;
    }
    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    /* pila de StackEntry, línea y columna */
    int line = 1, col = 0;
    int c;
    while ((c = fgetc(fp)) != EOF) {
        col++;
        if (c == '\n') { line++; col = 0; continue; }

        if (is_opening(c)) {
            /* push {c, line, col} */
        } else if (is_closing(c)) {
            /* pop y verificar, reportar line:col en caso de error */
        }
    }
    /* verificar pila no vacía: reportar line:col de la apertura */

    fclose(fp);
    return 0;
}
```

El reporte se ve así: `"error: '}' expected at 15:22 to match '{' opened at 3:5"`.
Esto es exactamente lo que hacen compiladores como GCC y Clang en sus
mensajes de error.

</details>

---

### Ejercicio 9 — Generar cadenas balanceadas

Implementa en Rust una función `generate_balanced(n: usize) -> Vec<String>`
que genere **todas** las cadenas balanceadas de longitud $2n$ usando solo
`(` y `)`. Para $n=3$, las cadenas son: `((()))`, `(()())`, `(())()`,
`()(())`, `()()()`.

<details>
<summary>¿Cuántas cadenas hay para $n$ pares? ¿Qué técnica de generación usarías?</summary>

El número de cadenas balanceadas de $n$ pares es el **número de Catalan**:

$$C_n = \frac{1}{n+1}\binom{2n}{n} = \frac{(2n)!}{(n+1)!\,n!}$$

Valores: $C_0=1$, $C_1=1$, $C_2=2$, $C_3=5$, $C_4=14$, $C_5=42$.

La técnica es **backtracking**:

```rust
fn generate_balanced(n: usize) -> Vec<String> {
    let mut results = Vec::new();
    let mut current = String::with_capacity(2 * n);
    backtrack(n, 0, 0, &mut current, &mut results);
    results
}

fn backtrack(
    n: usize,
    open: usize,
    close: usize,
    current: &mut String,
    results: &mut Vec<String>,
) {
    if current.len() == 2 * n {
        results.push(current.clone());
        return;
    }
    if open < n {
        current.push('(');
        backtrack(n, open + 1, close, current, results);
        current.pop();
    }
    if close < open {
        current.push(')');
        backtrack(n, open, close + 1, current, results);
        current.pop();
    }
}
```

Las dos restricciones clave son: (1) solo puedes añadir `(` si no has usado
las $n$ disponibles, y (2) solo puedes añadir `)` si hay un `(` sin cerrar
(`close < open`). Esto garantiza que solo se generan cadenas válidas, sin
necesidad de filtrar.

</details>

---

### Ejercicio 10 — Propiedad de pila en tests

Escribe un test en Rust que verifique esta propiedad: **para toda cadena
generada por `generate_balanced(n)`, `is_balanced` retorna `true`; y para
toda cadena generada que se modifique eliminando un carácter,
`is_balanced` retorna `false`**.

<details>
<summary>¿La segunda propiedad (eliminar un carácter) siempre produce una cadena no balanceada?</summary>

Sí, siempre. Una cadena balanceada de longitud $2n$ tiene exactamente $n$
aperturas y $n$ cierres. Al eliminar un carácter:

- Si eliminas una apertura: quedan $n-1$ aperturas y $n$ cierres → no balanceada.
- Si eliminas un cierre: quedan $n$ aperturas y $n-1$ cierres → no balanceada.

La longitud resultante es impar ($2n - 1$), que nunca puede ser balanceada
(los pares requieren longitud par).

```rust
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn generated_are_balanced() {
        for n in 0..=5 {
            let strings = generate_balanced(n);
            for s in &strings {
                assert!(
                    is_balanced(s),
                    "Should be balanced: {}", s
                );
            }
        }
    }

    #[test]
    fn removing_one_char_breaks_balance() {
        for n in 1..=4 {
            let strings = generate_balanced(n);
            for s in &strings {
                for i in 0..s.len() {
                    let mut modified = s.clone();
                    modified.remove(i);
                    assert!(
                        !is_balanced(&modified),
                        "Should NOT be balanced: {} (from {} removing pos {})",
                        modified, s, i
                    );
                }
            }
        }
    }
}
```

Este es un ejemplo de **property-based testing**: en lugar de verificar
casos específicos, se prueba una propiedad universal. El test verifica
$\sum_{n=1}^{4} C_n \times 2n = 1 \times 2 + 2 \times 4 + 5 \times 6 + 14 \times 8 = 154$
variantes, cubriendo mucho más que un puñado de casos manuales.

</details>
