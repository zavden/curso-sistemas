# T02 — Notación posfija (RPN)

## Índice

1. [Tres notaciones](#1-tres-notaciones)
2. [Qué es la notación posfija](#2-qué-es-la-notación-posfija)
3. [Por qué no necesita precedencia ni paréntesis](#3-por-qué-no-necesita-precedencia-ni-paréntesis)
4. [Algoritmo de evaluación con pila](#4-algoritmo-de-evaluación-con-pila)
5. [Trazas detalladas](#5-trazas-detalladas)
6. [Implementación en C](#6-implementación-en-c)
7. [Implementación en Rust](#7-implementación-en-rust)
8. [Manejo de errores](#8-manejo-de-errores)
9. [Análisis de complejidad](#9-análisis-de-complejidad)
10. [Ejercicios](#10-ejercicios)

---

## 1. Tres notaciones

Toda expresión aritmética con operadores binarios puede escribirse en tres
formas equivalentes:

| Notación | Nombre formal | Orden | Ejemplo |
|----------|---------------|-------|---------|
| **Infija** | — | operando **operador** operando | `3 + 4` |
| **Prefija** | Notación polaca | **operador** operando operando | `+ 3 4` |
| **Posfija** | Notación polaca inversa (RPN) | operando operando **operador** | `3 4 +` |

La notación infija es la que usamos habitualmente. Es natural para humanos
pero problemática para máquinas porque requiere reglas de precedencia y
paréntesis para eliminar ambigüedades.

### Ejemplo con expresión compleja

La expresión infija `(3 + 4) * 2 - 5` en las tres notaciones:

```
Infija:    (3 + 4) * 2 - 5
Prefija:   - * + 3 4 2 5
Posfija:   3 4 + 2 * 5 -
```

### Origen histórico

La notación prefija fue inventada por el lógico polaco Jan Łukasiewicz en
1924, de ahí el nombre "notación polaca". La forma posfija (inversa) fue
popularizada por las calculadoras HP en los años 1960-70. Los ingenieros y
científicos de esa era aprendían a pensar en RPN porque era más eficiente
en dispositivos con memoria limitada: no se necesitan paréntesis ni tecla
`=`, y cada operación se ejecuta inmediatamente.

---

## 2. Qué es la notación posfija

En notación posfija (**postfix**), cada operador aparece **después** de sus
operandos:

```
Infija:    a + b          Posfija:   a b +
Infija:    a + b * c      Posfija:   a b c * +
Infija:    (a + b) * c    Posfija:   a b + c *
```

### Propiedad fundamental

En una expresión posfija bien formada:

- Los operandos se leen de izquierda a derecha **en el mismo orden** que en
  la expresión infija.
- Los operadores se reordenan según su **orden real de ejecución**, no
  según cómo aparecen textualmente.

Por ejemplo, en `a + b * c`, la multiplicación se ejecuta primero. La
posfija `a b c * +` refleja este orden: `*` aparece antes que `+`.

### Propiedades formales

Una expresión posfija válida sobre operadores binarios cumple:

1. **Conteo**: si hay $n$ operandos, hay exactamente $n - 1$ operadores.
2. **Prefijo válido**: para todo prefijo de la expresión, el número de
   operandos vistos es siempre **estrictamente mayor** que el número de
   operadores vistos.
3. **Total**: al final, la diferencia operandos $-$ operadores $= 1$
   (queda exactamente un valor en la pila).

La propiedad 2 es equivalente a decir que la pila nunca se vacía en medio
de la evaluación (nunca se intenta pop sin suficientes operandos).

---

## 3. Por qué no necesita precedencia ni paréntesis

### El problema de la infija

La expresión infija `3 + 4 * 2` es ambigua sin convenciones:

- ¿Es `(3 + 4) * 2 = 14`?
- ¿Es `3 + (4 * 2) = 11`?

Necesitamos **reglas externas** (precedencia: `*` antes que `+`) para
desambiguar. Y cuando queremos un orden diferente, necesitamos
**paréntesis** explícitos: `(3 + 4) * 2`.

### La posfija es autocontenida

En posfija, el orden de evaluación está **codificado en la posición** de
los operadores:

```
3 4 2 * +    → primero 4*2, luego 3+8  → resultado 11
3 4 + 2 *    → primero 3+4, luego 7*2  → resultado 14
```

No hay ambigüedad posible. Cada expresión tiene una única interpretación
determinada por la secuencia de tokens. No existen reglas de precedencia
externas ni paréntesis — toda la información está en el orden.

### Demostración por contraste

Todas estas expresiones infijas **diferentes** (gracias a paréntesis y
precedencia) se mapean a expresiones posfijas **distintas**:

| Infija | Posfija | Resultado |
|--------|---------|:---------:|
| `2 + 3 * 4` | `2 3 4 * +` | 14 |
| `(2 + 3) * 4` | `2 3 + 4 *` | 20 |
| `2 * 3 + 4` | `2 3 * 4 +` | 10 |
| `2 * (3 + 4)` | `2 3 4 + *` | 14 |

Los paréntesis de la infija desaparecen: su efecto queda capturado en la
posición relativa operando-operador.

### Ventaja para máquinas

Un evaluador de posfija es un bucle trivial con una pila. Un evaluador de
infija requiere:

- Tabla de precedencia
- Manejo de asociatividad (izquierda o derecha)
- Procesamiento recursivo o algoritmo de dos pasadas
- Manejo de paréntesis como modificadores de precedencia

Por esto, los compiladores convierten internamente las expresiones infijas a
una representación similar a la posfija (código de tres direcciones, bytecode
de pila) para evaluarlas.

---

## 4. Algoritmo de evaluación con pila

El algoritmo es elegante en su simplicidad:

```
FUNCIÓN evaluate_rpn(tokens):
    stack ← pila vacía

    PARA CADA token EN tokens:
        SI token es un número:
            push(stack, token)
        SI token es un operador op:
            b ← pop(stack)        // segundo operando (primero en salir)
            a ← pop(stack)        // primer operando
            resultado ← a op b
            push(stack, resultado)

    RETORNAR pop(stack)           // único valor restante
```

### Detalle crucial: orden de operandos

Al hacer pop, el **segundo operando sale primero** (LIFO). Para operaciones
conmutativas (`+`, `*`) esto no importa, pero para operaciones no
conmutativas (`-`, `/`) el orden es esencial:

```
Expresión:    8 3 -
              push 8, push 3
              pop → b=3, pop → a=8
              resultado = a - b = 8 - 3 = 5    ✓ (no 3 - 8)
```

El primer operando (`a`) se empujó primero a la pila, por lo que sale
segundo. La convención es: `a` es el operando izquierdo, `b` el derecho.

---

## 5. Trazas detalladas

### Traza 1: `3 4 + 2 *` (equivale a `(3+4)*2`)

```
Token    Acción                 Pila (top →)
──────────────────────────────────────────────
  3      push 3                 3
  4      push 4                 3  4
  +      pop 4, pop 3 → 3+4=7  7
  2      push 2                 7  2
  *      pop 2, pop 7 → 7*2=14 14
  FIN    pop → 14               ∅
```

Resultado: **14**

### Traza 2: `5 1 2 + 4 * + 3 -` (equivale a `5+(1+2)*4-3`)

```
Token    Acción                   Pila (top →)
────────────────────────────────────────────────
  5      push 5                   5
  1      push 1                   5  1
  2      push 2                   5  1  2
  +      pop 2, pop 1 → 1+2=3    5  3
  4      push 4                   5  3  4
  *      pop 4, pop 3 → 3*4=12   5  12
  +      pop 12, pop 5 → 5+12=17 17
  3      push 3                   17  3
  -      pop 3, pop 17 → 17-3=14 14
  FIN    pop → 14                 ∅
```

Resultado: **14**

### Traza 3: `15 7 1 1 + - / 3 * 2 1 1 + + -`

Equivale a `15 / (7 - (1+1)) * 3 - (2 + (1+1))`:

```
Token    Acción                     Pila (top →)
──────────────────────────────────────────────────
  15     push 15                    15
  7      push 7                     15  7
  1      push 1                     15  7  1
  1      push 1                     15  7  1  1
  +      pop 1, pop 1 → 1+1=2      15  7  2
  -      pop 2, pop 7 → 7-2=5      15  5
  /      pop 5, pop 15 → 15/5=3    3
  3      push 3                     3  3
  *      pop 3, pop 3 → 3*3=9      9
  2      push 2                     9  2
  1      push 1                     9  2  1
  1      push 1                     9  2  1  1
  +      pop 1, pop 1 → 1+1=2      9  2  2
  +      pop 2, pop 2 → 2+2=4      9  4
  -      pop 4, pop 9 → 9-4=5      5
  FIN    pop → 5                    ∅
```

Resultado: **5**

---

## 6. Implementación en C

### Pila de doubles

Para expresiones aritméticas usamos `double` en lugar de `int`, lo que
permite manejar divisiones con decimales:

```c
/* rpn.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* --- Double stack ----------------------------------------- */

typedef struct {
    double *data;
    int     top;
    int     capacity;
} DblStack;

DblStack *dblstack_create(int capacity) {
    DblStack *s = malloc(sizeof(DblStack));
    if (!s) return NULL;
    s->data = malloc(capacity * sizeof(double));
    if (!s->data) { free(s); return NULL; }
    s->top = 0;
    s->capacity = capacity;
    return s;
}

void dblstack_destroy(DblStack *s) {
    if (s) { free(s->data); free(s); }
}

int dblstack_push(DblStack *s, double val) {
    if (s->top >= s->capacity) return 0;
    s->data[s->top++] = val;
    return 1;
}

int dblstack_pop(DblStack *s, double *out) {
    if (s->top == 0) return 0;
    *out = s->data[--s->top];
    return 1;
}

int dblstack_size(const DblStack *s) {
    return s->top;
}

/* --- Evaluator -------------------------------------------- */

int is_operator(const char *token) {
    return (strlen(token) == 1) &&
           (token[0] == '+' || token[0] == '-' ||
            token[0] == '*' || token[0] == '/');
}

typedef enum {
    RPN_OK,
    RPN_TOO_FEW_OPERANDS,    /* operator without enough operands */
    RPN_TOO_MANY_OPERANDS,   /* leftover values on stack */
    RPN_DIVISION_BY_ZERO,
    RPN_INVALID_TOKEN,
} RpnError;

typedef struct {
    RpnError error;
    double   value;           /* valid only if error == RPN_OK */
    char     token[32];       /* token that caused the error */
} RpnResult;

RpnResult evaluate_rpn(const char *expr) {
    RpnResult result = { .error = RPN_OK, .value = 0.0, .token = "" };

    /* copy expr to tokenize (strtok modifies the string) */
    char *buf = strdup(expr);
    if (!buf) {
        result.error = RPN_INVALID_TOKEN;
        return result;
    }

    /* worst case: all tokens are numbers */
    int max_tokens = (strlen(expr) / 2) + 1;
    DblStack *stack = dblstack_create(max_tokens);

    char *token = strtok(buf, " \t\n");
    while (token != NULL) {
        if (is_operator(token)) {
            double b, a;
            if (!dblstack_pop(stack, &b) || !dblstack_pop(stack, &a)) {
                result.error = RPN_TOO_FEW_OPERANDS;
                strncpy(result.token, token, 31);
                goto cleanup;
            }

            double r;
            switch (token[0]) {
                case '+': r = a + b; break;
                case '-': r = a - b; break;
                case '*': r = a * b; break;
                case '/':
                    if (b == 0.0) {
                        result.error = RPN_DIVISION_BY_ZERO;
                        strncpy(result.token, token, 31);
                        goto cleanup;
                    }
                    r = a / b;
                    break;
                default: r = 0; break;
            }
            dblstack_push(stack, r);
        } else {
            /* try to parse as number */
            char *end;
            double val = strtod(token, &end);
            if (*end != '\0') {
                result.error = RPN_INVALID_TOKEN;
                strncpy(result.token, token, 31);
                goto cleanup;
            }
            dblstack_push(stack, val);
        }

        token = strtok(NULL, " \t\n");
    }

    /* should have exactly one value left */
    if (dblstack_size(stack) != 1) {
        result.error = RPN_TOO_MANY_OPERANDS;
        goto cleanup;
    }

    dblstack_pop(stack, &result.value);

cleanup:
    dblstack_destroy(stack);
    free(buf);
    return result;
}

/* --- Main ------------------------------------------------- */

int main(void) {
    const char *tests[] = {
        "3 4 +",                          /* 7 */
        "3 4 + 2 *",                      /* 14 */
        "5 1 2 + 4 * + 3 -",             /* 14 */
        "15 7 1 1 + - / 3 * 2 1 1 + + -", /* 5 */
        "2 3 /",                          /* 0.666... */
        "10 0 /",                         /* div by zero */
        "+",                              /* too few operands */
        "1 2 3 +",                        /* too many operands */
        "hello",                          /* invalid token */
    };
    int n = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < n; i++) {
        RpnResult r = evaluate_rpn(tests[i]);
        printf("%-40s -> ", tests[i]);
        switch (r.error) {
            case RPN_OK:
                printf("%.4f\n", r.value);
                break;
            case RPN_TOO_FEW_OPERANDS:
                printf("ERROR: too few operands for '%s'\n", r.token);
                break;
            case RPN_TOO_MANY_OPERANDS:
                printf("ERROR: too many operands left\n");
                break;
            case RPN_DIVISION_BY_ZERO:
                printf("ERROR: division by zero\n");
                break;
            case RPN_INVALID_TOKEN:
                printf("ERROR: invalid token '%s'\n", r.token);
                break;
        }
    }

    return 0;
}
```

Salida esperada:

```
3 4 +                                    -> 7.0000
3 4 + 2 *                                -> 14.0000
5 1 2 + 4 * + 3 -                        -> 14.0000
15 7 1 1 + - / 3 * 2 1 1 + + -           -> 5.0000
2 3 /                                    -> 0.6667
10 0 /                                   -> ERROR: division by zero
+                                        -> ERROR: too few operands for '+'
1 2 3 +                                  -> ERROR: too many operands left
hello                                    -> ERROR: invalid token 'hello'
```

### Puntos de diseño en C

**`strtod` para parsing**: Convierte cadenas a `double`, manejando enteros,
decimales y notación científica. El puntero `end` indica dónde paró el
parsing: si `*end != '\0'`, el token no es un número válido.

**`strdup` + `strtok`**: `strtok` modifica la cadena original (inserta `\0`),
por eso copiamos con `strdup`. Olvidar esto causa undefined behavior si
`expr` apunta a un string literal (que reside en memoria de solo lectura).

**`goto cleanup`**: Patrón estándar en C para centralizar liberación de
recursos. Cada error salta al mismo punto que ejecuta `free(buf)` y
`dblstack_destroy(stack)`.

### Compilación

```bash
gcc -std=c11 -Wall -Wextra -o rpn rpn.c -lm
./rpn
```

El flag `-lm` enlaza `libm` (necesario si se añaden funciones como `pow`,
`sqrt` más adelante, aunque `strtod` no lo requiere).

---

## 7. Implementación en Rust

```rust
// rpn.rs

#[derive(Debug, PartialEq)]
enum RpnError {
    TooFewOperands { operator: char },
    TooManyOperands { remaining: usize },
    DivisionByZero,
    InvalidToken(String),
}

fn evaluate_rpn(expr: &str) -> Result<f64, RpnError> {
    let mut stack: Vec<f64> = Vec::new();

    for token in expr.split_whitespace() {
        match token {
            "+" | "-" | "*" | "/" => {
                let b = stack.pop().ok_or(RpnError::TooFewOperands {
                    operator: token.chars().next().unwrap(),
                })?;
                let a = stack.pop().ok_or(RpnError::TooFewOperands {
                    operator: token.chars().next().unwrap(),
                })?;

                let result = match token {
                    "+" => a + b,
                    "-" => a - b,
                    "*" => a * b,
                    "/" => {
                        if b == 0.0 {
                            return Err(RpnError::DivisionByZero);
                        }
                        a / b
                    }
                    _ => unreachable!(),
                };

                stack.push(result);
            }
            _ => {
                let val: f64 = token
                    .parse()
                    .map_err(|_| RpnError::InvalidToken(token.to_string()))?;
                stack.push(val);
            }
        }
    }

    if stack.len() != 1 {
        return Err(RpnError::TooManyOperands {
            remaining: stack.len(),
        });
    }

    Ok(stack.pop().unwrap())
}

fn main() {
    let tests = [
        "3 4 +",
        "3 4 + 2 *",
        "5 1 2 + 4 * + 3 -",
        "15 7 1 1 + - / 3 * 2 1 1 + + -",
        "2 3 /",
        "10 0 /",
        "+",
        "1 2 3 +",
        "hello",
    ];

    for expr in &tests {
        print!("{:<40} -> ", expr);
        match evaluate_rpn(expr) {
            Ok(val) => println!("{:.4}", val),
            Err(e) => println!("ERROR: {:?}", e),
        }
    }
}
```

### Comparación C vs Rust

| Aspecto | C | Rust |
|---------|---|------|
| Pila | `DblStack` manual | `Vec<f64>` |
| Tokenización | `strdup` + `strtok` (mutable) | `split_whitespace` (inmutable) |
| Parsing | `strtod` + puntero `end` | `.parse::<f64>()` → `Result` |
| Errores | enum + struct retorno | `Result<f64, RpnError>` + `?` |
| Limpieza | `goto cleanup` con 2 `free` | Automática |
| Líneas | ~120 | ~55 |

El operador `?` de Rust reemplaza todo el patrón `goto cleanup`. Cada
operación que puede fallar (`pop`, `parse`) retorna un `Result`, y `?`
propaga el error automáticamente, ejecutando los destructores de todos los
valores en el scope actual.

### Nota sobre `split_whitespace`

A diferencia de `strtok` en C, `split_whitespace` no modifica la cadena
original. Retorna un iterador de `&str` (slices) que apuntan a las
subcadenas separadas por espacios. No hay copia, no hay mutación, no hay
`strdup`.

---

## 8. Manejo de errores

Una implementación robusta debe detectar tres categorías de errores:

### 8.1 Errores de estructura

| Error | Causa | Detección |
|-------|-------|-----------|
| Operandos insuficientes | Operador sin 2 valores en pila | Pop falla |
| Operandos sobrantes | Más valores que operadores | `stack.len() != 1` al final |
| Expresión vacía | Cadena sin tokens | Pila vacía al final |

### 8.2 Errores aritméticos

| Error | Causa | Detección |
|-------|-------|-----------|
| División por cero | Divisor es 0 | Verificar `b == 0` antes de dividir |
| Overflow | Resultado excede el rango | Verificar `isinf()` / `is_infinite()` |

### 8.3 Errores léxicos

| Error | Causa | Detección |
|-------|-------|-----------|
| Token inválido | No es número ni operador | Parsing falla |

### Números negativos como tokens

Un caso sutil: ¿es `-3` un número negativo o el operador `-` seguido de
`3`? La convención estándar en RPN es que `-3` es un número negativo
(token único). `strtod` y `parse::<f64>()` manejan esto correctamente —
leen el `-` como parte del número si es seguido inmediatamente por dígitos.

Esto significa que el token `-` solo (un carácter) es un operador, mientras
que `-3.14` (múltiples caracteres empezando con `-`) es un número. La
distinción la hace la longitud del token o el resultado de intentar el
parsing numérico primero.

---

## 9. Análisis de complejidad

### Tiempo

Cada token se procesa exactamente una vez. Cada operación (push, pop,
aritmética) es $O(1)$:

$$T(n) = O(n)$$

donde $n$ es el número de tokens en la expresión.

### Espacio

La pila almacena como máximo los operandos no consumidos. En el peor caso
(todos los operandos seguidos de todos los operadores), la pila tiene $k$
elementos donde $k$ es el número de operandos:

$$S(n) = O(n)$$

En la práctica, para expresiones balanceadas, la profundidad máxima de la
pila es proporcional a la profundidad de anidamiento, no al número total de
tokens.

### Comparación con evaluación infija

| Aspecto | Evaluación posfija | Evaluación infija directa |
|---------|:------------------:|:-------------------------:|
| Tiempo | $O(n)$ | $O(n)$ |
| Espacio | $O(n)$ | $O(n)$ (dos pilas) |
| Complejidad del código | Baja (un bucle) | Alta (precedencia, recursión) |
| Paréntesis | No existen | Hay que procesarlos |

La complejidad asintótica es la misma, pero la **complejidad del código**
es dramáticamente menor para la posfija. Un evaluador de posfija se escribe
en 20 líneas; un evaluador de infija correcto requiere 100+.

---

## 10. Ejercicios

---

### Ejercicio 1 — Traducción manual infija a posfija

Convierte estas expresiones infijas a posfija **manualmente** (sin algoritmo
formal — eso se verá en T03). Pista: identifica qué operación se ejecuta
primero y coloca ese operador después de sus operandos.

```
a)  2 + 3
b)  2 + 3 * 4
c)  (2 + 3) * 4
d)  a * b + c * d
e)  (a + b) * (c - d)
```

<details>
<summary>¿En cuáles cambia el orden de los operandos respecto a la infija?</summary>

```
a)  2 3 +
b)  2 3 4 * +          (* tiene mayor precedencia, se ejecuta primero)
c)  2 3 + 4 *          (los paréntesis fuerzan + primero)
d)  a b * c d * +      (dos multiplicaciones, luego la suma)
e)  a b + c d - *      (sumas/restas dentro, multiplicación al final)
```

Los **operandos nunca cambian de orden**. En todas las expresiones, los
operandos aparecen en la misma secuencia izquierda-a-derecha que en la
infija. Solo los **operadores** se reposicionan. Esta es una propiedad
fundamental de la conversión infija→posfija.

</details>

---

### Ejercicio 2 — Traza de evaluación

Realiza la traza completa (token, acción, estado de la pila) para:

```
8 2 5 * + 1 3 2 * + 4 - /
```

<details>
<summary>¿Cuál es el resultado? ¿Cuál es la profundidad máxima de la pila?</summary>

```
Token  Acción                      Pila (top →)
──────────────────────────────────────────────────
  8    push 8                      8
  2    push 2                      8  2
  5    push 5                      8  2  5
  *    pop 5, pop 2 → 2*5=10      8  10
  +    pop 10, pop 8 → 8+10=18    18
  1    push 1                      18  1
  3    push 3                      18  1  3
  2    push 2                      18  1  3  2
  *    pop 2, pop 3 → 3*2=6       18  1  6
  +    pop 6, pop 1 → 1+6=7       18  7
  4    push 4                      18  7  4
  -    pop 4, pop 7 → 7-4=3       18  3
  /    pop 3, pop 18 → 18/3=6     6
```

Resultado: **6**. Profundidad máxima: **4** (cuando la pila contiene
`18 1 3 2`). La expresión infija equivalente es `(8 + 2*5) / (1 + 3*2 - 4)`.

</details>

---

### Ejercicio 3 — Detectar expresiones inválidas

Para cada expresión, predice si es válida o qué error se produce:

```
a)  3 +
b)  3 4 + *
c)  3 4 5
d)  3 4 + 5 - 2 *
e)  * 3 4
```

<details>
<summary>¿Cuáles producen "too few operands" y cuáles "too many operands"?</summary>

```
a)  "3 +"      → TooFewOperands para '+': push 3, luego + necesita 2 valores
                  pero solo hay 1.
b)  "3 4 + *"  → TooFewOperands para '*': push 3, push 4, + → 7,
                  luego * necesita 2 valores pero solo hay 1 (el 7).
c)  "3 4 5"    → TooManyOperands: tres valores, cero operadores.
                  La pila tiene 3 elementos al final.
d)  "3 4 + 5 - 2 *"  → OK: 3+4=7, 7-5=2, 2*2=4. Resultado 4.
e)  "* 3 4"    → TooFewOperands para '*': primer token es *, pila vacía.
```

TooFewOperands: (a), (b), (e). TooManyOperands: (c). Válida: (d).

Nota: (e) sería válida en notación **prefija** (`* 3 4 = 12`), pero no en
posfija.

</details>

---

### Ejercicio 4 — Operador de potencia

Extiende la implementación en C para soportar el operador `^` (potencia).
Evalúa: `"2 3 ^"` (→ 8), `"2 3 ^ 2 ^"` (→ 64), `"2 0.5 ^"` (→ $\sqrt{2} \approx 1.4142$).

<details>
<summary>¿Qué función de la librería matemática necesitas y qué flag de compilación?</summary>

Necesitas `pow(a, b)` de `<math.h>` y compilar con `-lm`:

```c
int is_operator(const char *token) {
    return (strlen(token) == 1) &&
           (token[0] == '+' || token[0] == '-' ||
            token[0] == '*' || token[0] == '/' ||
            token[0] == '^');
}

/* inside the switch: */
case '^': r = pow(a, b); break;
```

En Rust, se usa `f64::powf`:

```rust
"^" => a.powf(b),
```

Los resultados:
- `"2 3 ^"` → `pow(2, 3) = 8`
- `"2 3 ^ 2 ^"` → `pow(pow(2, 3), 2) = pow(8, 2) = 64`
- `"2 0.5 ^"` → `pow(2, 0.5) = 1.4142...`

En posfija, la asociatividad de `^` no importa porque el orden está
explícito. La expresión `"2 3 ^ 2 ^"` siempre significa $(2^3)^2$. Si se
quisiera $2^{(3^2)} = 512$, sería `"2 3 2 ^ ^"`.

</details>

---

### Ejercicio 5 — Operadores unarios

Implementa en Rust el operador unario `neg` (negación) que tome un solo
operando de la pila. Evalúa: `"5 neg"` (→ -5), `"3 4 + neg"` (→ -7).

<details>
<summary>¿Cómo se distingue un operador unario de uno binario en el algoritmo?</summary>

Se distingue por el nombre del token (no por un solo carácter). Los
operadores unarios hacen un solo `pop`:

```rust
"neg" => {
    let a = stack.pop().ok_or(RpnError::TooFewOperands {
        operator: '~',  // or use a String instead of char
    })?;
    stack.push(-a);
}
"sqrt" => {
    let a = stack.pop().ok_or(/* ... */)?;
    stack.push(a.sqrt());
}
```

La convención en RPN es que los operadores unarios consumen 1 valor y
producen 1 valor (la pila no cambia de tamaño). Los binarios consumen 2 y
producen 1 (la pila decrece en 1). Un operador `n`-ario consumiría $n$ y
produciría 1.

La propiedad de conteo se generaliza: si hay $k$ operandos, $b$ operadores
binarios y $u$ operadores unarios, la expresión es válida si $k - b - 0 \cdot u = 1$,
es decir, $k = b + 1$ (los unarios no cambian la cuenta neta).

</details>

---

### Ejercicio 6 — Cadena vacía y un solo número

¿Qué debería retornar el evaluador para estas entradas?

```
a)  "" (cadena vacía)
b)  "42"
c)  "  " (solo espacios)
```

Implementa el comportamiento correcto en ambos lenguajes.

<details>
<summary>¿Cuál es un caso especial legítimo y cuál un error?</summary>

```
a)  "" → Error (TooManyOperands con remaining=0, o un error específico
          EmptyExpression). No hay ningún valor que retornar.
b)  "42" → Ok(42.0). Un solo número sin operadores es una expresión posfija
           válida: la pila tiene exactamente 1 elemento al final.
c)  "  " → Igual que "": split_whitespace/strtok no produce tokens.
```

El caso (b) es el caso base legítimo. Si la expresión tiene un solo operando
y cero operadores, cumple la propiedad $k - b = 1 - 0 = 1$.

Para manejar (a) y (c), verifica antes del return final:

```rust
if stack.is_empty() {
    return Err(RpnError::TooManyOperands { remaining: 0 });
    // or a dedicated EmptyExpression error
}
```

La implementación actual ya lo maneja: `stack.len() != 1` captura tanto
`len() == 0` (vacía) como `len() > 1` (sobrantes).

</details>

---

### Ejercicio 7 — Verificar propiedad de conteo

Escribe una función en Rust que, **sin evaluar** la expresión, determine si
una cadena posfija es **estructuralmente válida** (tiene el número correcto
de operandos y operadores, y nunca intenta pop de pila vacía).

<details>
<summary>¿Basta contar operandos y operadores, o necesitas simular la pila?</summary>

Contar $k$ operandos y $b$ operadores y verificar $k = b + 1$ es necesario
pero **no suficiente**. La expresión `"+ 3 4"` tiene $k=2, b=1$, cumple
$2 = 1+1$, pero es inválida (el `+` se encuentra con la pila vacía).

Necesitas **simular el tamaño de la pila**:

```rust
fn is_structurally_valid(expr: &str) -> bool {
    let mut depth: i32 = 0;

    for token in expr.split_whitespace() {
        match token {
            "+" | "-" | "*" | "/" | "^" => {
                depth -= 1;  // consume 2, produce 1 → net -1
                if depth < 0 {
                    return false;  // underflow
                }
            }
            _ => {
                // assume it's an operand (don't validate number format)
                depth += 1;
            }
        }
    }

    depth == 1
}
```

Un operando incrementa `depth` en 1 (push). Un operador binario decrementa
en 1 (pop 2, push 1 = neto $-1$). Si `depth` cae por debajo de 0, hubo
underflow. Al final, `depth == 1` confirma que queda exactamente un resultado.

Esto es equivalente a la propiedad 2 de la sección 2: "para todo prefijo,
operandos > operadores".

</details>

---

### Ejercicio 8 — RPN con enteros en C (sin doubles)

Reimplementa el evaluador en C usando `int` en lugar de `double`. La
división debe ser **entera** (truncada hacia cero como hace C). Evalúa:
`"7 2 /"` (→ 3), `"7 -2 /"` (→ -3).

<details>
<summary>¿Qué pasa con la división entera de negativos en C?</summary>

Desde C99, la división entera trunca hacia cero:

- `7 / 2 = 3` (no 3.5)
- `7 / -2 = -3` (no -4)
- `-7 / 2 = -3` (no -4)

El módulo sigue la regla: `a == (a/b)*b + a%b`.

La implementación cambia `double` por `int` y `strtod` por `strtol`:

```c
typedef struct {
    int *data;
    int  top;
    int  capacity;
} IntStack;

/* In the evaluator: */
long val = strtol(token, &end, 10);
if (*end != '\0') { /* invalid token */ }
intstack_push(stack, (int)val);
```

Con `strtol`, el tercer argumento es la base (10 para decimal). Se puede
extender a bases 16 (`0x1A`) u 8 (`017`) cambiando la base o usando 0
para detección automática.

</details>

---

### Ejercicio 9 — Múltiples expresiones desde stdin

Escribe un programa en C que lea expresiones RPN línea por línea desde
`stdin` (con `fgets`), evalúe cada una, e imprima el resultado. Termina
con EOF (Ctrl+D).

<details>
<summary>¿Qué precaución necesitas con `fgets` y el `\n` final?</summary>

`fgets` incluye el `\n` en el buffer. Hay que eliminarlo antes de evaluar,
porque `strtok` con delimitador `" \t\n"` ya lo maneja (el `\n` es un
separador más), pero si el token final es `\n` solo, se ignorará
correctamente.

```c
int main(void) {
    char line[1024];

    printf("RPN Calculator (Ctrl+D to exit)\n");
    printf("> ");

    while (fgets(line, sizeof(line), stdin) != NULL) {
        /* remove trailing newline for clean display */
        line[strcspn(line, "\n")] = '\0';

        if (line[0] == '\0') {
            printf("> ");
            continue;
        }

        RpnResult r = evaluate_rpn(line);
        if (r.error == RPN_OK) {
            printf("= %.4f\n", r.value);
        } else {
            printf("ERROR\n");
        }

        printf("> ");
    }

    printf("\n");
    return 0;
}
```

`strcspn(line, "\n")` retorna el índice del primer `\n` (o la longitud si
no hay `\n`). Al colocar `'\0'` ahí, se elimina el salto de línea
limpiamente. Esta técnica es más robusta que `line[strlen(line)-1] = '\0'`
porque no falla si la cadena está vacía.

</details>

---

### Ejercicio 10 — Verificación infija-posfija

Dada la expresión infija `((a + b) * c - d) / (e + f)`, escribe la posfija
equivalente. Luego sustituye: $a=2, b=3, c=4, d=10, e=1, f=3$. Evalúa
**tanto la infija como la posfija** y verifica que dan el mismo resultado.

<details>
<summary>¿Cuál es la posfija y cuál es el resultado numérico?</summary>

Conversión paso a paso (de adentro hacia afuera):

```
((a + b) * c - d) / (e + f)

1. a + b         →  a b +
2. resultado * c →  a b + c *
3. resultado - d →  a b + c * d -
4. e + f         →  e f +
5. resultado / resultado → a b + c * d - e f + /
```

Posfija: **`a b + c * d - e f + /`**

Sustituyendo: **`2 3 + 4 * 10 - 1 3 + /`**

Evaluación infija:
- $(2 + 3) = 5$
- $5 \times 4 = 20$
- $20 - 10 = 10$
- $(1 + 3) = 4$
- $10 / 4 = 2.5$

Evaluación posfija:
```
2 3 + → 5
5 4 * → 20
20 10 - → 10
1 3 + → 4
10 4 / → 2.5
```

Resultado: **2.5** (coinciden).

</details>
