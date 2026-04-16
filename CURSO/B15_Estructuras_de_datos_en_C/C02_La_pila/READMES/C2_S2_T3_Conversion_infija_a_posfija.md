# T03 — Conversión infija a posfija

## Índice

1. [El problema de la notación infija](#1-el-problema-de-la-notación-infija)
2. [Precedencia y asociatividad](#2-precedencia-y-asociatividad)
3. [Algoritmo de Shunting-Yard](#3-algoritmo-de-shunting-yard)
4. [Trazas detalladas](#4-trazas-detalladas)
5. [Implementación en C](#5-implementación-en-c)
6. [Implementación en Rust](#6-implementación-en-rust)
7. [Extensiones del algoritmo](#7-extensiones-del-algoritmo)
8. [Errores comunes](#8-errores-comunes)
9. [Análisis de complejidad](#9-análisis-de-complejidad)
10. [Ejercicios](#10-ejercicios)

---

## 1. El problema de la notación infija

La notación infija que usamos habitualmente (`3 + 4 * 2`) es ambigua sin
reglas adicionales. Para que un programa la evalúe correctamente, necesita
saber:

1. **Precedencia**: `*` se ejecuta antes que `+`.
2. **Asociatividad**: `2 ^ 3 ^ 4` es `2 ^ (3 ^ 4)` (derecha), no
   `(2 ^ 3) ^ 4` (izquierda).
3. **Paréntesis**: `(3 + 4) * 2` anula la precedencia normal.

En T02 vimos que la notación posfija elimina estas ambigüedades. El
problema ahora es: **¿cómo convertir automáticamente de infija a posfija?**

La respuesta es el **algoritmo de Shunting-Yard**, inventado por Edsger
Dijkstra en 1961. El nombre ("patio de maniobras") viene de la analogía
con un patio ferroviario donde los vagones se reordenan usando una vía
lateral — que es exactamente lo que hace la pila de operadores.

---

## 2. Precedencia y asociatividad

### Tabla de precedencia

Para los operadores aritméticos estándar:

| Precedencia | Operadores | Asociatividad | Ejemplo |
|:-----------:|:----------:|:-------------:|---------|
| 4 (alta) | `^` | Derecha | `2^3^4` = `2^(3^4)` |
| 3 | `*` `/` | Izquierda | `6/3*2` = `(6/3)*2` |
| 2 | `+` `-` | Izquierda | `5-3+1` = `(5-3)+1` |

### Asociatividad

La asociatividad determina cómo se agrupan operadores con la **misma
precedencia**:

- **Asociatividad izquierda**: se evalúa de izquierda a derecha.
  `a - b - c` = `(a - b) - c`
- **Asociatividad derecha**: se evalúa de derecha a izquierda.
  `a ^ b ^ c` = `a ^ (b ^ c)`

Esto importa para operadores no conmutativos. `10 - 3 - 2`:
- Izquierda (correcto): $(10 - 3) - 2 = 5$
- Derecha (incorrecto): $10 - (3 - 2) = 9$

### Regla para el algoritmo

La asociatividad se traduce en una regla simple al comparar el operador
actual con el del tope de la pila:

- **Izquierda**: sacar si la precedencia del tope es **mayor o igual**.
- **Derecha**: sacar solo si la precedencia del tope es **estrictamente mayor**.

Esto se formaliza como:

```
DEBE_SACAR(op_tope, op_actual):
    SI op_actual es asociativo-izquierda:
        RETORNAR precedencia(op_tope) >= precedencia(op_actual)
    SI op_actual es asociativo-derecha:
        RETORNAR precedencia(op_tope) > precedencia(op_actual)
```

---

## 3. Algoritmo de Shunting-Yard

El algoritmo usa dos estructuras:

- **Cola de salida** (output): donde se construye la expresión posfija.
- **Pila de operadores** (op_stack): almacenamiento temporal de operadores.

```
FUNCIÓN shunting_yard(tokens):
    output ← cola vacía
    op_stack ← pila vacía

    PARA CADA token EN tokens:
        SI token es un número:
            agregar token a output

        SI token es un operador o1:
            MIENTRAS op_stack no vacía
                  Y tope o2 no es '('
                  Y DEBE_SACAR(o2, o1):
                sacar o2 de op_stack
                agregar o2 a output
            push o1 a op_stack

        SI token es '(':
            push '(' a op_stack

        SI token es ')':
            MIENTRAS tope de op_stack ≠ '(':
                sacar operador de op_stack
                agregar a output
            sacar '(' de op_stack (y descartarlo)

    MIENTRAS op_stack no vacía:
        sacar operador de op_stack
        agregar a output

    RETORNAR output
```

### Analogía del patio ferroviario

```
                    ┌─────────┐
  Entrada ───────►  │  PILA   │  ───────► Salida
  (infija)          │  (vía   │           (posfija)
                    │ lateral)│
                    └─────────┘
```

Los **operandos** pasan directamente de la entrada a la salida (como vagones
que van directo). Los **operadores** entran en la vía lateral (pila) y
esperan hasta que sea su turno de salir. Un operador sale de la pila cuando
llega otro operador con menor o igual precedencia, o cuando se cierra un
paréntesis.

### Por qué funciona

El algoritmo mantiene un invariante: **la pila de operadores siempre tiene
precedencia estrictamente creciente de abajo hacia arriba** (para operadores
de asociatividad izquierda). Cuando llega un operador nuevo:

- Si tiene mayor precedencia que el tope, se apila (todavía no debe
  ejecutarse).
- Si tiene menor o igual precedencia, los operadores de mayor precedencia
  en el tope ya deberían haberse ejecutado antes — se sacan a la salida.

Los paréntesis rompen temporalmente este invariante: el `(` actúa como
barrera. Nada puede sacarse más allá de un `(`. Cuando llega `)`, se
vacía la pila hasta el `(` correspondiente.

---

## 4. Trazas detalladas

### Traza 1: `3 + 4 * 2`

Precedencia: `*` (3) > `+` (2).

```
Token   Acción                          Output        Op Stack (top →)
─────────────────────────────────────────────────────────────────────────
  3     número → output                 3
  +     pila vacía → push               3             +
  4     número → output                 3 4           +
  *     prec(*) > prec(+) → push        3 4           + *
  2     número → output                 3 4 2         + *
  FIN   vaciar pila → output            3 4 2 * +
```

Resultado: **`3 4 2 * +`**

Verificación: $3 + (4 \times 2) = 3 + 8 = 11$. Evaluando `3 4 2 * +`:
push 3, push 4, push 2, `*` → 8, `+` → 11. Correcto.

### Traza 2: `(3 + 4) * 2`

Los paréntesis fuerzan la suma primero.

```
Token   Acción                          Output        Op Stack (top →)
─────────────────────────────────────────────────────────────────────────
  (     push '('                                      (
  3     número → output                 3             (
  +     tope es '(' → push              3             ( +
  4     número → output                 3 4           ( +
  )     sacar hasta '(':
        sacar '+' → output              3 4 +         (
        descartar '('                   3 4 +
  *     pila vacía → push               3 4 +         *
  2     número → output                 3 4 + 2       *
  FIN   vaciar pila → output            3 4 + 2 *
```

Resultado: **`3 4 + 2 *`**

### Traza 3: `a + b * c - d / e`

Dos operadores de misma precedencia (`+`, `-`) y dos de mayor precedencia
(`*`, `/`).

```
Token   Acción                          Output            Op Stack
─────────────────────────────────────────────────────────────────────────
  a     número → output                 a
  +     pila vacía → push               a                 +
  b     número → output                 a b               +
  *     prec(*) > prec(+) → push        a b               + *
  c     número → output                 a b c             + *
  -     prec(-) <= prec(*) → sacar *    a b c *           +
        prec(-) <= prec(+) → sacar +    a b c * +
        push -                          a b c * +         -
  d     número → output                 a b c * + d       -
  /     prec(/) > prec(-) → push        a b c * + d       - /
  e     número → output                 a b c * + d e     - /
  FIN   vaciar pila                     a b c * + d e / -
```

Resultado: **`a b c * + d e / -`**

Equivale a: $(a + (b \times c)) - (d / e)$.

### Traza 4: `2 ^ 3 ^ 4` (asociatividad derecha)

```
Token   Acción                          Output        Op Stack
──────────────────────────────────────────────────────────────────
  2     número → output                 2
  ^     pila vacía → push               2             ^
  3     número → output                 2 3           ^
  ^     prec(^) NO > prec(^) (derecha,
        necesita estrictamente mayor)
        → push                          2 3           ^ ^
  4     número → output                 2 3 4         ^ ^
  FIN   vaciar pila                     2 3 4 ^ ^
```

Resultado: **`2 3 4 ^ ^`**

Evaluando: push 2, push 3, push 4, `^` → $3^4 = 81$, `^` → $2^{81}$.
Esto es correcto: $2^{3^4} = 2^{81}$, no $(2^3)^4 = 8^4 = 4096$.

Si `^` fuera asociativo-izquierda, al llegar el segundo `^`, como
$\text{prec}(\hat{}) \geq \text{prec}(\hat{})$, se sacaría el primero,
dando `2 3 ^ 4 ^` = $(2^3)^4$. La diferencia entre `>=` y `>` en la
condición de sacar es lo que distingue asociatividad izquierda de derecha.

### Traza 5: `(a + b) * (c - d)`

```
Token   Acción                          Output        Op Stack
──────────────────────────────────────────────────────────────────
  (     push '('                                      (
  a     número → output                 a             (
  +     tope es '(' → push              a             ( +
  b     número → output                 a b           ( +
  )     sacar hasta '(': sacar +        a b +
  *     pila vacía → push               a b +         *
  (     push '('                        a b +         * (
  c     número → output                 a b + c       * (
  -     tope es '(' → push              a b + c       * ( -
  d     número → output                 a b + c d     * ( -
  )     sacar hasta '(': sacar -        a b + c d -   *
  FIN   vaciar pila                     a b + c d - *
```

Resultado: **`a b + c d - *`**

---

## 5. Implementación en C

```c
/* shunting_yard.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* --- Operator properties ---------------------------------- */

typedef enum { ASSOC_LEFT, ASSOC_RIGHT } Assoc;

typedef struct {
    char  symbol;
    int   precedence;
    Assoc associativity;
} Operator;

static const Operator operators[] = {
    { '+', 2, ASSOC_LEFT  },
    { '-', 2, ASSOC_LEFT  },
    { '*', 3, ASSOC_LEFT  },
    { '/', 3, ASSOC_LEFT  },
    { '^', 4, ASSOC_RIGHT },
};
static const int num_operators = sizeof(operators) / sizeof(operators[0]);

static const Operator *find_operator(char c) {
    for (int i = 0; i < num_operators; i++) {
        if (operators[i].symbol == c) return &operators[i];
    }
    return NULL;
}

/* --- String stack for tokens ------------------------------ */

typedef struct {
    char **data;
    int    top;
    int    capacity;
} StrStack;

StrStack *strstack_create(int capacity) {
    StrStack *s = malloc(sizeof(StrStack));
    if (!s) return NULL;
    s->data = malloc(capacity * sizeof(char *));
    if (!s->data) { free(s); return NULL; }
    s->top = 0;
    s->capacity = capacity;
    return s;
}

void strstack_destroy(StrStack *s) {
    if (s) { free(s->data); free(s); }
}

void strstack_push(StrStack *s, char *token) {
    if (s->top < s->capacity) s->data[s->top++] = token;
}

char *strstack_pop(StrStack *s) {
    if (s->top == 0) return NULL;
    return s->data[--s->top];
}

char *strstack_peek(const StrStack *s) {
    if (s->top == 0) return NULL;
    return s->data[s->top - 1];
}

bool strstack_is_empty(const StrStack *s) {
    return s->top == 0;
}

/* --- Shunting-Yard ---------------------------------------- */

typedef enum {
    SY_OK,
    SY_MISMATCHED_PAREN,  /* '(' without ')' or vice versa */
    SY_INVALID_TOKEN,
} SyError;

typedef struct {
    char  **tokens;       /* output token array (pointers into buf) */
    int     count;
    SyError error;
} SyResult;

static bool should_pop(const Operator *stack_op, const Operator *current_op) {
    if (current_op->associativity == ASSOC_LEFT) {
        return stack_op->precedence >= current_op->precedence;
    } else {
        return stack_op->precedence > current_op->precedence;
    }
}

SyResult shunting_yard(char *expr) {
    int len = strlen(expr);
    int max_tokens = len + 1;

    SyResult result;
    result.tokens = malloc(max_tokens * sizeof(char *));
    result.count = 0;
    result.error = SY_OK;

    StrStack *ops = strstack_create(max_tokens);

    char *token = strtok(expr, " \t\n");
    while (token != NULL) {
        const Operator *op = NULL;

        if (strlen(token) == 1 && (op = find_operator(token[0])) != NULL) {
            /* operator */
            while (!strstack_is_empty(ops)) {
                char *top = strstack_peek(ops);
                if (top[0] == '(') break;
                const Operator *top_op = find_operator(top[0]);
                if (!top_op || !should_pop(top_op, op)) break;
                result.tokens[result.count++] = strstack_pop(ops);
            }
            strstack_push(ops, token);

        } else if (strlen(token) == 1 && token[0] == '(') {
            strstack_push(ops, token);

        } else if (strlen(token) == 1 && token[0] == ')') {
            while (!strstack_is_empty(ops) &&
                   strstack_peek(ops)[0] != '(') {
                result.tokens[result.count++] = strstack_pop(ops);
            }
            if (strstack_is_empty(ops)) {
                result.error = SY_MISMATCHED_PAREN;
                goto cleanup;
            }
            strstack_pop(ops);  /* discard '(' */

        } else {
            /* assume number/operand — goes directly to output */
            result.tokens[result.count++] = token;
        }

        token = strtok(NULL, " \t\n");
    }

    /* flush remaining operators */
    while (!strstack_is_empty(ops)) {
        char *top = strstack_pop(ops);
        if (top[0] == '(') {
            result.error = SY_MISMATCHED_PAREN;
            goto cleanup;
        }
        result.tokens[result.count++] = top;
    }

cleanup:
    strstack_destroy(ops);
    return result;
}

/* --- Main ------------------------------------------------- */

void print_result(const char *infix, SyResult *r) {
    printf("Infija:  %s\n", infix);
    if (r->error != SY_OK) {
        printf("Posfija: ERROR (%s)\n\n",
               r->error == SY_MISMATCHED_PAREN
               ? "mismatched parentheses" : "invalid token");
        return;
    }
    printf("Posfija: ");
    for (int i = 0; i < r->count; i++) {
        printf("%s ", r->tokens[i]);
    }
    printf("\n\n");
}

int main(void) {
    /* strtok modifies the string, so we need mutable copies */
    char e1[] = "3 + 4 * 2";
    char e2[] = "( 3 + 4 ) * 2";
    char e3[] = "a + b * c - d / e";
    char e4[] = "2 ^ 3 ^ 4";
    char e5[] = "( a + b ) * ( c - d )";
    char e6[] = "3 + 4 * 2 / ( 1 - 5 ) ^ 2 ^ 3";
    char e7[] = "( ( 1 + 2 )";    /* mismatched */
    char e8[] = "1 + 2 ) * 3";    /* mismatched */

    /* save originals for display (strtok destroys them) */
    const char *labels[] = {
        "3 + 4 * 2",
        "( 3 + 4 ) * 2",
        "a + b * c - d / e",
        "2 ^ 3 ^ 4",
        "( a + b ) * ( c - d )",
        "3 + 4 * 2 / ( 1 - 5 ) ^ 2 ^ 3",
        "( ( 1 + 2 )",
        "1 + 2 ) * 3",
    };
    char *exprs[] = { e1, e2, e3, e4, e5, e6, e7, e8 };
    int n = sizeof(exprs) / sizeof(exprs[0]);

    for (int i = 0; i < n; i++) {
        SyResult r = shunting_yard(exprs[i]);
        print_result(labels[i], &r);
        free(r.tokens);
    }

    return 0;
}
```

Salida esperada:

```
Infija:  3 + 4 * 2
Posfija: 3 4 2 * +

Infija:  ( 3 + 4 ) * 2
Posfija: 3 4 + 2 *

Infija:  a + b * c - d / e
Posfija: a b c * + d e / -

Infija:  2 ^ 3 ^ 4
Posfija: 2 3 4 ^ ^

Infija:  ( a + b ) * ( c - d )
Posfija: a b + c d - *

Infija:  3 + 4 * 2 / ( 1 - 5 ) ^ 2 ^ 3
Posfija: 3 4 2 * 1 5 - 2 3 ^ ^ / +

Infija:  ( ( 1 + 2 )
Posfija: ERROR (mismatched parentheses)

Infija:  1 + 2 ) * 3
Posfija: ERROR (mismatched parentheses)
```

### Decisiones de diseño

**Tokens como punteros dentro del buffer**: Los tokens de salida son
punteros `char *` que apuntan a subcadenas dentro del buffer modificado por
`strtok`. No hay copia de strings — eficiente pero implica que el buffer
original debe vivir tanto como el resultado.

**Tokens separados por espacios**: Esta implementación requiere que cada
token (incluidos paréntesis) esté separado por espacios: `( 3 + 4 )` en
lugar de `(3+4)`. La versión sin esta restricción requiere un lexer que
procese carácter por carácter, separando números de operadores incluso
cuando están adyacentes.

**`should_pop` centralizado**: Toda la lógica de precedencia y asociatividad
está en una función. Añadir un operador nuevo solo requiere agregarlo al
array `operators[]`.

### Compilación

```bash
gcc -std=c11 -Wall -Wextra -o shunting shunting_yard.c
./shunting
```

---

## 6. Implementación en Rust

```rust
// shunting_yard.rs

#[derive(Debug, Clone, Copy, PartialEq)]
enum Assoc {
    Left,
    Right,
}

fn precedence(op: char) -> Option<(i32, Assoc)> {
    match op {
        '+' | '-' => Some((2, Assoc::Left)),
        '*' | '/' => Some((3, Assoc::Left)),
        '^'       => Some((4, Assoc::Right)),
        _         => None,
    }
}

fn is_operator(token: &str) -> bool {
    token.len() == 1 && precedence(token.chars().next().unwrap()).is_some()
}

#[derive(Debug, PartialEq)]
enum ShuntingError {
    MismatchedParentheses,
}

fn shunting_yard(expr: &str) -> Result<Vec<String>, ShuntingError> {
    let mut output: Vec<String> = Vec::new();
    let mut ops: Vec<String> = Vec::new();

    for token in expr.split_whitespace() {
        if is_operator(token) {
            let current_char = token.chars().next().unwrap();
            let (current_prec, current_assoc) = precedence(current_char).unwrap();

            while let Some(top) = ops.last() {
                if top == "(" { break; }

                let top_char = top.chars().next().unwrap();
                if let Some((top_prec, _)) = precedence(top_char) {
                    let should_pop = match current_assoc {
                        Assoc::Left  => top_prec >= current_prec,
                        Assoc::Right => top_prec > current_prec,
                    };
                    if should_pop {
                        output.push(ops.pop().unwrap());
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            }

            ops.push(token.to_string());
        } else if token == "(" {
            ops.push(token.to_string());
        } else if token == ")" {
            loop {
                match ops.pop() {
                    None => return Err(ShuntingError::MismatchedParentheses),
                    Some(top) if top == "(" => break,
                    Some(top) => output.push(top),
                }
            }
        } else {
            // operand: goes directly to output
            output.push(token.to_string());
        }
    }

    // flush remaining operators
    while let Some(top) = ops.pop() {
        if top == "(" {
            return Err(ShuntingError::MismatchedParentheses);
        }
        output.push(top);
    }

    Ok(output)
}

fn main() {
    let tests = [
        "3 + 4 * 2",
        "( 3 + 4 ) * 2",
        "a + b * c - d / e",
        "2 ^ 3 ^ 4",
        "( a + b ) * ( c - d )",
        "3 + 4 * 2 / ( 1 - 5 ) ^ 2 ^ 3",
        "( ( 1 + 2 )",
        "1 + 2 ) * 3",
    ];

    for expr in &tests {
        print!("Infija:  {}\n", expr);
        match shunting_yard(expr) {
            Ok(tokens) => println!("Posfija: {}\n", tokens.join(" ")),
            Err(e) => println!("Posfija: ERROR ({:?})\n", e),
        }
    }
}
```

### Comparación C vs Rust

| Aspecto | C | Rust |
|---------|---|------|
| Pila de operadores | `StrStack` manual con `char **` | `Vec<String>` |
| Tabla de precedencia | Array de structs + búsqueda lineal | `match` directo |
| Salida | Array de `char *` (punteros al buffer) | `Vec<String>` (ownership propio) |
| Tokenización | `strtok` (modifica la cadena) | `split_whitespace` (inmutable) |
| Paréntesis sin par | `goto cleanup` | `return Err(...)` |
| Añadir operador | Agregar entrada al array | Agregar rama al `match` |

En Rust, `ops.last()` equivale a peek (observar sin sacar), y el `while let
Some(top) = ops.last()` es un bucle que se detiene cuando la pila está vacía
o la condición de break se cumple.

El patrón `Some(top) if top == "("` dentro del `match` es un **guard**: la
rama solo se toma si `top` es exactamente `"("`. Sin el guard, habría que
usar un `if` adicional dentro del cuerpo.

---

## 7. Extensiones del algoritmo

### 7.1 Operadores unarios

El operador unario `-` (negación) es problemático porque comparte símbolo
con la resta binaria. La solución clásica: si `-` aparece al inicio de la
expresión o después de `(` u otro operador, es unario.

Estrategia de implementación: en la fase de tokenización, convertir el `-`
unario a un token distinto (por ejemplo, `~` o `NEG`) con precedencia
máxima y asociatividad derecha:

```
Entrada:   - 3 + 4
Tokens:    NEG 3 + 4
Posfija:   3 NEG 4 +
```

### 7.2 Funciones

Funciones como `sin`, `max` pueden integrarse al algoritmo. Una función se
empuja a la pila como un operador de precedencia máxima. Cuando se encuentra
su `)` de cierre, se saca la función a la salida:

```
Entrada:   sin ( 3.14 )
Posfija:   3.14 sin

Entrada:   max ( 3 , 4 )
Posfija:   3 4 max
```

La coma (`,`) dentro de argumentos de función actúa como un separador:
vacía la pila de operadores hasta el `(` de la función, similar a `)` pero
sin consumir el paréntesis.

### 7.3 Tokens sin espacios

Un lexer carácter por carácter puede procesar `(3+4)*2` sin espacios:

```c
/* simplified character-by-character lexer */
while (*p != '\0') {
    if (isdigit(*p) || *p == '.') {
        /* read entire number */
        char *start = p;
        while (isdigit(*p) || *p == '.') p++;
        /* token is [start, p) */
    } else if (is_operator_char(*p) || *p == '(' || *p == ')') {
        /* single character token */
        p++;
    } else if (isspace(*p)) {
        p++;  /* skip whitespace */
    }
}
```

Esto separa la fase de **lexing** (dividir en tokens) de la fase de
**parsing** (aplicar Shunting-Yard). Es la arquitectura que usan los
compiladores reales: primero tokenizar, luego parsear.

---

## 8. Errores comunes

### 8.1 Ignorar la asociatividad

Usar siempre `>=` en la condición de sacar (tratar todo como
asociativo-izquierda) produce resultados incorrectos para `^`:

```
2 ^ 3 ^ 4

Con >= (MAL):  2 3 ^ 4 ^  →  (2^3)^4 = 4096
Con  > (BIEN): 2 3 4 ^ ^  →  2^(3^4) = 2^81
```

### 8.2 No detectar paréntesis sin par

Si se olvida verificar que la pila contiene `(` al encontrar `)`, el
algoritmo puede vaciar toda la pila y producir posfija incorrecta sin
señalar error.

Si se olvida verificar `(` sobrantes al final (cuando se vacía la pila de
operadores), la expresión `(3 + 4` no se detectaría como errónea.

### 8.3 Confundir el orden de pop

Al sacar operadores para la salida, se sacan en orden LIFO pero se
**agregan** a la salida en ese mismo orden. Como la salida es una cola (se
agrega al final), el orden relativo se preserva correctamente.

### 8.4 Modificar la cadena original sin copia

En C, `strtok` modifica la cadena insertando `\0`. Si la cadena original
es un literal (`"3 + 4"`), esto causa undefined behavior (los literales
están en memoria de solo lectura). Siempre copiar con `strdup` o usar
arrays `char[]`.

### 8.5 Paréntesis como tokens

Los paréntesis `(` y `)` **nunca deben aparecer en la salida posfija**. Si
aparecen, hay un error en la implementación. El `(` se usa como barrera en
la pila y se descarta al encontrar su `)`.

---

## 9. Análisis de complejidad

### Tiempo

Cada token se procesa una vez. Cada operador se empuja y se saca de la pila
exactamente una vez. El bucle interno (`while` de sacar operadores) no
añade complejidad total porque cada operador participa en él a lo sumo
una vez en toda la ejecución:

$$T(n) = O(n)$$

Este es un ejemplo del **análisis amortizado**: aunque una iteración
individual del bucle externo puede sacar varios operadores, el total de
operaciones de pila sobre toda la ejecución es $\leq 2n$ (cada operador se
empuja una vez y se saca una vez).

### Espacio

La pila de operadores almacena como máximo todos los operadores (sin contar
operandos). En el peor caso (expresión con muchos paréntesis anidados o
con precedencia estrictamente creciente):

$$S(n) = O(n)$$

La salida también es $O(n)$ — los mismos $n$ tokens reordenados.

### Comparación: conversión + evaluación vs evaluación directa

| Enfoque | Pasos | Tiempo total | Espacio |
|---------|-------|:------------:|:-------:|
| Infija → Posfija → Evaluar | 2 pasadas | $O(n) + O(n) = O(n)$ | $O(n)$ |
| Evaluación directa de infija | 1 pasada (compleja) | $O(n)$ | $O(n)$ |

La complejidad asintótica es igual, pero la separación en dos pasos produce
código más simple, más testeable, y más reutilizable. La conversión a posfija
se hace una vez y la evaluación puede repetirse con diferentes valores de
variables.

---

## 10. Ejercicios

---

### Ejercicio 1 — Traza completa

Aplica el algoritmo de Shunting-Yard a: `a * ( b + c ) - d`

Para cada token, anota el token, la acción tomada, el estado de la cola de
salida, y el estado de la pila de operadores.

<details>
<summary>¿En qué momento se saca el `*` de la pila?</summary>

```
Token   Acción                   Output          Op Stack
─────────────────────────────────────────────────────────────
  a     output                   a
  *     push                     a               *
  (     push                     a               * (
  b     output                   a b             * (
  +     tope '(' → push          a b             * ( +
  c     output                   a b c           * ( +
  )     sacar hasta '(':
        sacar '+' → output       a b c +         * (
        descartar '('            a b c +         *
  -     prec(-) <= prec(*) →
        sacar '*' → output       a b c + *
        push '-'                 a b c + *       -
  d     output                   a b c + * d     -
  FIN   vaciar: sacar '-'        a b c + * d -
```

El `*` se saca cuando llega el `-` (posición 7). Aunque `*` tiene mayor
precedencia que `-`, la regla dice "sacar si prec(tope) >= prec(actual)"
para asociatividad izquierda. Como $\text{prec}(*) = 3 \geq 2 = \text{prec}(-)$,
se saca.

Resultado: **`a b c + * d -`** que equivale a `(a * (b + c)) - d`.

</details>

---

### Ejercicio 2 — Asociatividad en acción

Convierte `10 - 3 - 2` usando Shunting-Yard. Luego convierte `2 ^ 3 ^ 2`.
Compara los resultados y explica la diferencia en la condición de sacar.

<details>
<summary>¿Cuáles son las posfijas y por qué difieren en comportamiento?</summary>

**`10 - 3 - 2`** (asociatividad izquierda):

```
Token   Acción                   Output        Op Stack
──────────────────────────────────────────────────────────
  10    output                   10
  -     push                     10            -
  3     output                   10 3          -
  -     prec(-) >= prec(-) →
        sacar '-' → output       10 3 -
        push '-'                 10 3 -        -
  2     output                   10 3 - 2      -
  FIN   sacar '-'                10 3 - 2 -
```

Posfija: `10 3 - 2 -` → $(10-3)-2 = 5$. Correcto.

**`2 ^ 3 ^ 2`** (asociatividad derecha):

```
Token   Acción                   Output        Op Stack
──────────────────────────────────────────────────────────
  2     output                   2
  ^     push                     2             ^
  3     output                   2 3           ^
  ^     prec(^) NOT > prec(^) →
        NO sacar (necesita
        estrictamente mayor)
        push '^'                 2 3           ^ ^
  2     output                   2 3 2         ^ ^
  FIN   sacar ambos              2 3 2 ^ ^
```

Posfija: `2 3 2 ^ ^` → $2^{(3^2)} = 2^9 = 512$. Correcto.

La diferencia: al llegar el segundo operador de igual precedencia:
- Izquierda: `>=` → saca el anterior (se ejecuta primero).
- Derecha: `>` → no saca (el actual se empila encima, se ejecutará primero).

</details>

---

### Ejercicio 3 — Paréntesis anidados

Convierte: `( ( a + b ) * ( c + d ) ) / e`

<details>
<summary>¿Cuántos paréntesis quedan en la pila al mismo tiempo como máximo?</summary>

```
Token   Acción                   Output            Op Stack
──────────────────────────────────────────────────────────────
  (     push                                       (
  (     push                                       ( (
  a     output                   a                 ( (
  +     tope '(' → push          a                 ( ( +
  b     output                   a b               ( ( +
  )     sacar +, descartar (     a b +             (
  *     tope '(' → push          a b +             ( *
  (     push                     a b +             ( * (
  c     output                   a b + c           ( * (
  +     tope '(' → push          a b + c           ( * ( +
  d     output                   a b + c d         ( * ( +
  )     sacar +, descartar (     a b + c d +       ( *
  )     sacar *, descartar (     a b + c d + *
  /     push                     a b + c d + *     /
  e     output                   a b + c d + * e   /
  FIN   sacar /                  a b + c d + * e /
```

Máximo de paréntesis simultáneos en la pila: **2** (cuando se apilan ambos
`(` al inicio). En total la pila alcanza profundidad 4 en `( * ( +`.

Resultado: **`a b + c d + * e /`**.

</details>

---

### Ejercicio 4 — Detectar errores

Predice si cada expresión produce un error y de qué tipo:

```
a)  "3 + ) 4"
b)  "( 3 + 4"
c)  "( 3 + 4 ) )"
d)  "3 + ( ( 4 * 2 )"
e)  "3 + 4 * 2"
```

<details>
<summary>¿Cuáles tienen paréntesis sin par y cuáles son válidas?</summary>

```
a)  "3 + ) 4"       → Error: al encontrar ')' la pila no tiene '('
                       (pila solo tiene '+').
b)  "( 3 + 4"       → Error: al vaciar la pila al final, se encuentra '('
                       sin su ')' correspondiente.
c)  "( 3 + 4 ) )"   → Error: el segundo ')' intenta buscar '(' pero la
                       pila está vacía.
d)  "3 + ( ( 4 * 2 )" → Error: al vaciar la pila al final, queda un '('
                         (se cerró un paréntesis pero el otro no).
e)  "3 + 4 * 2"     → OK: sin paréntesis, todo correcto.
                       Resultado: 3 4 2 * +
```

Errores: (a), (b), (c), (d). Válida: (e).

</details>

---

### Ejercicio 5 — Pipeline completo

Implementa una función en Rust que tome una expresión infija como `&str`,
la convierta a posfija con `shunting_yard`, y luego la evalúe con el
evaluador de T02. Prueba con: `"( 3 + 4 ) * 2"` → 14, `"2 ^ 10"` → 1024.

<details>
<summary>¿Cómo conectas las dos funciones si una produce `Vec<String>` y la otra espera `&str`?</summary>

El evaluador de T02 esperaba un `&str` que tokenizaba internamente con
`split_whitespace`. Para conectar, puedes:

**Opción 1**: unir los tokens y re-parsear (simple pero redundante):

```rust
fn evaluate_infix(expr: &str) -> Result<f64, String> {
    let postfix = shunting_yard(expr)
        .map_err(|e| format!("{:?}", e))?;
    let joined = postfix.join(" ");
    evaluate_rpn(&joined)
        .map_err(|e| format!("{:?}", e))
}
```

**Opción 2**: modificar el evaluador para aceptar `&[String]` directamente
(más eficiente, sin re-tokenización):

```rust
fn evaluate_rpn_tokens(tokens: &[String]) -> Result<f64, RpnError> {
    let mut stack: Vec<f64> = Vec::new();
    for token in tokens {
        // same logic as before, but iterating over &[String]
        // ...
    }
    // ...
}

fn evaluate_infix(expr: &str) -> Result<f64, String> {
    let tokens = shunting_yard(expr)
        .map_err(|e| format!("{:?}", e))?;
    evaluate_rpn_tokens(&tokens)
        .map_err(|e| format!("{:?}", e))
}
```

La opción 2 evita la serialización y re-parsing del paso intermedio.

Tests:
- `"( 3 + 4 ) * 2"` → Shunting-Yard: `["3","4","+","2","*"]` → Evaluar: 14
- `"2 ^ 10"` → Shunting-Yard: `["2","10","^"]` → Evaluar: 1024

</details>

---

### Ejercicio 6 — Operador módulo

Extiende ambas implementaciones (C y Rust) para soportar el operador `%`
(módulo) con la misma precedencia que `*` y `/`, asociatividad izquierda.
Convierte y evalúa: `"10 + 7 % 3"`.

<details>
<summary>¿Cuál es la posfija y el resultado?</summary>

En C, agregar al array de operadores:

```c
{ '%', 3, ASSOC_LEFT },
```

En Rust, agregar al `match` de `precedence`:

```rust
'%' => Some((3, Assoc::Left)),
```

Conversión de `"10 + 7 % 3"`:
- `10` → output: `10`
- `+` → push. Stack: `+`
- `7` → output: `10 7`
- `%` → prec(%) > prec(+) → push. Stack: `+ %`
- `3` → output: `10 7 3`
- FIN → sacar `%`, sacar `+`: `10 7 3 % +`

Posfija: **`10 7 3 % +`**

Evaluación: $7 \% 3 = 1$, $10 + 1 = 11$.

Para la evaluación en C: `case '%': r = fmod(a, b); break;` (con `double`)
o `r = (int)a % (int)b` (con enteros). En Rust: `"%" => a % b`.

</details>

---

### Ejercicio 7 — Tabla de verdad

Extiende Shunting-Yard para operadores lógicos. Define la siguiente tabla:

| Operador | Precedencia | Asociatividad |
|:--------:|:-----------:|:-------------:|
| `OR` | 1 | Izquierda |
| `AND` | 2 | Izquierda |
| `NOT` | 5 (unario) | Derecha |

Convierte: `"a AND b OR c AND d"` y `"NOT a OR b"`.

<details>
<summary>¿Cuáles son las posfijas resultantes?</summary>

**`"a AND b OR c AND d"`**:

```
Token   Acción                       Output            Op Stack
──────────────────────────────────────────────────────────────────
  a     output                       a
  AND   push                         a                 AND
  b     output                       a b               AND
  OR    prec(OR)<prec(AND) → no
        prec(OR)<=prec(AND) → sacar  a b AND
        push OR                      a b AND           OR
  c     output                       a b AND c         OR
  AND   prec(AND)>prec(OR) → push    a b AND c         OR AND
  d     output                       a b AND c d       OR AND
  FIN   sacar AND, sacar OR          a b AND c d AND OR
```

Posfija: **`a b AND c d AND OR`** que equivale a `(a AND b) OR (c AND d)`.

**`"NOT a OR b"`** (NOT es unario):

```
Token   Acción                       Output        Op Stack
──────────────────────────────────────────────────────────────
  NOT   push (unario, prec 5)                      NOT
  a     output                       a             NOT
  OR    prec(OR)<prec(NOT) → sacar   a NOT
        push OR                      a NOT         OR
  b     output                       a NOT b       OR
  FIN   sacar OR                     a NOT b OR
```

Posfija: **`a NOT b OR`** que equivale a `(NOT a) OR b`.

Para implementar tokens multi-carácter, el `match` en Rust cambia de
comparar caracteres a comparar strings:

```rust
match token {
    "AND" | "OR" => { /* binary operator logic */ }
    "NOT"        => { /* unary operator logic */ }
    _            => { /* operand */ }
}
```

</details>

---

### Ejercicio 8 — Conversión inversa: posfija a infija

Escribe una función en Rust que convierta posfija a infija. La técnica: usa
una pila de strings. Al encontrar un operador, saca dos strings, los
combina con paréntesis, y empuja el resultado. Convierte `"a b + c *"`.

<details>
<summary>¿El resultado tiene paréntesis redundantes? ¿Cómo minimizarlos?</summary>

Algoritmo básico:

```rust
fn postfix_to_infix(expr: &str) -> String {
    let mut stack: Vec<String> = Vec::new();

    for token in expr.split_whitespace() {
        match token {
            "+" | "-" | "*" | "/" | "^" => {
                let b = stack.pop().unwrap();
                let a = stack.pop().unwrap();
                let combined = format!("( {} {} {} )", a, token, b);
                stack.push(combined);
            }
            _ => stack.push(token.to_string()),
        }
    }

    stack.pop().unwrap()
}
```

Para `"a b + c *"`:
- `a` → push `"a"`
- `b` → push `"b"`
- `+` → pop `"b"`, pop `"a"` → push `"( a + b )"`
- `c` → push `"c"`
- `*` → pop `"c"`, pop `"( a + b )"` → push `"( ( a + b ) * c )"`

Resultado: `"( ( a + b ) * c )"` — paréntesis **redundantes** en el exterior.

Para minimizar paréntesis, hay que considerar la precedencia: solo añadir
paréntesis cuando la precedencia del operador interno es menor que la del
externo. Esto requiere llevar la precedencia junto con cada string en la
pila, transformándola en una pila de `(String, Option<i32>)`.

</details>

---

### Ejercicio 9 — Verificación con round-trip

Escribe un test que verifique: para cualquier expresión infija válida,
convertirla a posfija y luego de vuelta a infija produce una expresión que,
al evaluarla, da el **mismo resultado**. Prueba con al menos 5 expresiones.

<details>
<summary>¿Producirá el round-trip la misma cadena original?</summary>

No, la cadena textual generalmente **difiere** de la original. La conversión
posfija→infija añade paréntesis explícitos que la original podía no tener
(porque usaba precedencia implícita).

Ejemplo: `"3 + 4 * 2"` → posfija `"3 4 2 * +"` → infija `"( 3 + ( 4 * 2 ) )"`.
La cadena es diferente de la original, pero al evaluarlas ambas dan 11.

El test debe comparar **valores numéricos**, no cadenas:

```rust
#[test]
fn round_trip_preserves_value() {
    let tests = [
        "( 3 + 4 ) * 2",      // 14
        "10 - 3 - 2",          // 5
        "2 ^ 3 ^ 2",           // 512
        "8 / 4 / 2",           // 1
        "( 1 + 2 ) * ( 3 + 4 )", // 21
    ];

    for expr in &tests {
        let postfix = shunting_yard(expr).unwrap();
        let val1 = evaluate_rpn_tokens(&postfix).unwrap();

        let infix_back = postfix_to_infix(&postfix.join(" "));
        let postfix2 = shunting_yard(&infix_back).unwrap();
        let val2 = evaluate_rpn_tokens(&postfix2).unwrap();

        assert!(
            (val1 - val2).abs() < 1e-10,
            "Round-trip failed for '{}': {} vs {}", expr, val1, val2
        );
    }
}
```

Se usa comparación con tolerancia ($\epsilon = 10^{-10}$) por posibles
errores de punto flotante.

</details>

---

### Ejercicio 10 — Traza visual interactiva

Implementa en C un modo "verbose" que imprima la tabla de traza completa
(como las de la sección 4) conforme ejecuta el algoritmo. Usa el formato:

```
Token   Acción                   Output            Op Stack
─────────────────────────────────────────────────────────────
  3     output                   3
  +     push                     3                 +
  ...
```

<details>
<summary>¿Cómo imprimes el contenido actual de la pila y la cola de salida en cada paso?</summary>

Necesitas funciones auxiliares que recorran los arrays internos:

```c
void print_output(char **tokens, int count) {
    for (int i = 0; i < count; i++) {
        printf("%s ", tokens[i]);
    }
}

void print_stack(StrStack *s) {
    for (int i = 0; i < s->top; i++) {
        printf("%s ", s->data[i]);
    }
}

/* inside the main loop, after each action: */
printf("  %-6s %-24s ", token, action_description);
print_output(result.tokens, result.count);
printf("%-20s", "");  /* padding */
print_stack(ops);
printf("\n");
```

La clave es que `StrStack` necesita acceso a sus internos para imprimir
(no solo peek/pop). En la implementación actual, `s->data[0..s->top-1]`
contiene los elementos de abajo hacia arriba.

Para `action_description`, construir un string según la acción tomada:
`"output"`, `"push"`, `"pop * -> output"`, `"pop until ("`, etc. Esto
transforma la herramienta en un recurso pedagógico donde el estudiante puede
verificar sus trazas manuales contra la salida del programa.

</details>
