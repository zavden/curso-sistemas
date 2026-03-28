# T04 — Evaluación de expresiones

## Índice

1. [El pipeline completo](#1-el-pipeline-completo)
2. [Tokens tipados](#2-tokens-tipados)
3. [Lexer: cadena a tokens](#3-lexer-cadena-a-tokens)
4. [Implementación en C](#4-implementación-en-c)
5. [Implementación en Rust](#5-implementación-en-rust)
6. [Manejo de errores en el pipeline](#6-manejo-de-errores-en-el-pipeline)
7. [Extensión con variables](#7-extensión-con-variables)
8. [Comparación: tagged union vs enum](#8-comparación-tagged-union-vs-enum)
9. [Análisis de complejidad](#9-análisis-de-complejidad)
10. [Ejercicios](#10-ejercicios)

---

## 1. El pipeline completo

En T02 y T03 vimos las dos mitades del problema por separado. Ahora las
unimos en un pipeline de tres fases:

```
  Cadena           Tokens          Tokens           Resultado
  "3 + 4 * 2"  ──►  [Num(3),   ──►  [Num(3),    ──►  11.0
                      Op(+),         Num(4),
                      Num(4),        Num(2),
                      Op(*),         Op(*),
                      Num(2)]        Op(+)]

                  LEXER          SHUNTING-YARD     EVALUADOR
                 (fase 1)          (fase 2)        (fase 3)
```

### Por qué tres fases separadas

| Fase | Responsabilidad | Entrada | Salida |
|------|-----------------|---------|--------|
| Lexer | Dividir texto en tokens tipados | `&str` / `char *` | `Vec<Token>` / `Token[]` |
| Shunting-Yard | Reordenar a posfija | Tokens infijos | Tokens posfijos |
| Evaluador | Calcular resultado | Tokens posfijos | `f64` / `double` |

Separar en fases tiene ventajas concretas:

- Cada fase se puede **testar independientemente**.
- El lexer maneja complejidades textuales (números negativos, espacios,
  tokens sin separar) sin contaminar la lógica matemática.
- El evaluador trabaja con tokens tipados — no necesita repetir parsing
  de strings en cada operación.
- Se puede reutilizar el evaluador con tokens generados por otros medios
  (por ejemplo, un parser de archivos de configuración).

En las implementaciones de T02 y T03, los tokens eran strings (`char *` o
`&str`). El evaluador tenía que parsear cada token de nuevo para saber si
era número u operador. Con tokens tipados, esa decisión se toma una sola
vez en el lexer.

---

## 2. Tokens tipados

Un **token** es la unidad mínima con significado en una expresión. En lugar
de representarlos como strings, usamos tipos que distinguen su naturaleza
en el sistema de tipos del lenguaje.

### En Rust: enum con datos

```rust
#[derive(Debug, Clone, PartialEq)]
enum Token {
    Number(f64),
    Operator(Op),
    LeftParen,
    RightParen,
}

#[derive(Debug, Clone, Copy, PartialEq)]
enum Op {
    Add,
    Sub,
    Mul,
    Div,
    Pow,
}
```

Cada variante porta exactamente los datos que necesita. `Number` porta el
valor `f64`; `Operator` porta qué operación es; los paréntesis no portan
datos adicionales.

### En C: tagged union

C no tiene enums con datos. Se emula con una **tagged union**: un `enum`
para el tipo (tag) y un `union` para el dato:

```c
typedef enum {
    TOK_NUMBER,
    TOK_OPERATOR,
    TOK_LPAREN,
    TOK_RPAREN,
} TokenType;

typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_POW,
} OpKind;

typedef struct {
    TokenType type;       /* tag: qué variante es */
    union {
        double  number;   /* válido si type == TOK_NUMBER */
        OpKind  op;       /* válido si type == TOK_OPERATOR */
    } data;
} Token;
```

El programador es responsable de acceder al campo correcto de la union
según el valor del tag. Acceder a `data.number` cuando `type == TOK_OPERATOR`
es **undefined behavior** — el compilador no lo impide.

### Ventaja sobre strings

| Con strings | Con tokens tipados |
|-------------|-------------------|
| `"3.14"` → parsear a `double` cada vez | `Number(3.14)` → valor ya disponible |
| `"+"` → comparar carácter | `Operator(Add)` → match directo |
| `"foo"` → ¿es variable o error? | Error detectado en el lexer |
| No hay exhaustividad | `match` exhaustivo en Rust |

---

## 3. Lexer: cadena a tokens

El **lexer** (o **tokenizer**) lee la cadena carácter por carácter y produce
una secuencia de tokens. Maneja las complejidades textuales:

- Números de múltiples dígitos: `123`, `3.14`
- Espacios opcionales entre tokens: `3+4` y `3 + 4`
- Números negativos: `-5` como token (no como resta unaria)
- Caracteres inválidos

### Algoritmo del lexer

```
FUNCIÓN tokenize(input):
    tokens ← lista vacía
    i ← 0

    MIENTRAS i < longitud(input):
        SI input[i] es espacio:
            i ← i + 1
            CONTINUAR

        SI input[i] es dígito o '.':
            inicio ← i
            MIENTRAS i < longitud Y (input[i] es dígito o '.'):
                i ← i + 1
            number ← parsear input[inicio..i]
            agregar Number(number) a tokens

        SI input[i] es operador (+, -, *, /, ^):
            agregar Operator correspondiente a tokens
            i ← i + 1

        SI input[i] es '(':
            agregar LeftParen a tokens
            i ← i + 1

        SI input[i] es ')':
            agregar RightParen a tokens
            i ← i + 1

        EN OTRO CASO:
            ERROR: carácter inválido en posición i
```

### Números negativos

El caso `-` es ambiguo: ¿operador binario o signo del número?

Regla: `-` es signo negativo (parte del número) cuando aparece:
- Al inicio de la expresión, o
- después de un operador, o
- después de `(`.

En cualquier otro caso, es el operador resta.

```
"- 3 + 4"     → [-3.0]  [+]  [4.0]       (signo negativo)
"3 - 4"       → [3.0]  [-]  [4.0]         (resta)
"( - 3 )"     → [(]  [-3.0]  [)]          (signo negativo)
"3 * - 2"     → [3.0]  [*]  [-2.0]        (signo negativo)
```

---

## 4. Implementación en C

La implementación completa integra lexer, Shunting-Yard y evaluador:

```c
/* expr_eval.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* --- Token types ------------------------------------------ */

typedef enum { TOK_NUMBER, TOK_OPERATOR, TOK_LPAREN, TOK_RPAREN } TokenType;
typedef enum { OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_POW } OpKind;

typedef struct {
    TokenType type;
    union {
        double number;
        OpKind op;
    } data;
} Token;

/* --- Token array ------------------------------------------ */

typedef struct {
    Token *items;
    int    count;
    int    capacity;
} TokenArray;

TokenArray tokarray_create(int capacity) {
    TokenArray ta;
    ta.items = malloc(capacity * sizeof(Token));
    ta.count = 0;
    ta.capacity = capacity;
    return ta;
}

void tokarray_push(TokenArray *ta, Token tok) {
    if (ta->count >= ta->capacity) {
        ta->capacity *= 2;
        ta->items = realloc(ta->items, ta->capacity * sizeof(Token));
    }
    ta->items[ta->count++] = tok;
}

void tokarray_destroy(TokenArray *ta) {
    free(ta->items);
    ta->items = NULL;
    ta->count = 0;
}

/* --- Token stack (for Shunting-Yard operator stack) -------- */

typedef struct {
    Token *data;
    int    top;
    int    capacity;
} TokenStack;

TokenStack *tokstack_create(int capacity) {
    TokenStack *s = malloc(sizeof(TokenStack));
    s->data = malloc(capacity * sizeof(Token));
    s->top = 0;
    s->capacity = capacity;
    return s;
}

void tokstack_destroy(TokenStack *s) {
    if (s) { free(s->data); free(s); }
}

void tokstack_push(TokenStack *s, Token t) {
    if (s->top < s->capacity) s->data[s->top++] = t;
}

Token tokstack_pop(TokenStack *s) { return s->data[--s->top]; }

Token tokstack_peek(const TokenStack *s) { return s->data[s->top - 1]; }

int tokstack_is_empty(const TokenStack *s) { return s->top == 0; }

/* --- Double stack (for evaluator) ------------------------- */

typedef struct {
    double *data;
    int     top;
    int     capacity;
} DblStack;

DblStack *dblstack_create(int cap) {
    DblStack *s = malloc(sizeof(DblStack));
    s->data = malloc(cap * sizeof(double));
    s->top = 0;
    s->capacity = cap;
    return s;
}

void dblstack_destroy(DblStack *s) {
    if (s) { free(s->data); free(s); }
}

void dblstack_push(DblStack *s, double v) { s->data[s->top++] = v; }

double dblstack_pop(DblStack *s) { return s->data[--s->top]; }

int dblstack_size(const DblStack *s) { return s->top; }

/* --- Operator properties ---------------------------------- */

typedef enum { ASSOC_LEFT, ASSOC_RIGHT } Assoc;

static int op_precedence(OpKind op) {
    switch (op) {
        case OP_ADD: case OP_SUB: return 2;
        case OP_MUL: case OP_DIV: return 3;
        case OP_POW: return 4;
    }
    return 0;
}

static Assoc op_assoc(OpKind op) {
    return (op == OP_POW) ? ASSOC_RIGHT : ASSOC_LEFT;
}

static char op_char(OpKind op) {
    switch (op) {
        case OP_ADD: return '+';
        case OP_SUB: return '-';
        case OP_MUL: return '*';
        case OP_DIV: return '/';
        case OP_POW: return '^';
    }
    return '?';
}

/* --- Error types ------------------------------------------ */

typedef enum {
    EVAL_OK,
    EVAL_LEX_ERROR,
    EVAL_MISMATCHED_PAREN,
    EVAL_TOO_FEW_OPERANDS,
    EVAL_TOO_MANY_OPERANDS,
    EVAL_DIVISION_BY_ZERO,
} EvalError;

typedef struct {
    EvalError error;
    double    value;
    int       error_pos;
} EvalResult;

/* --- Phase 1: Lexer --------------------------------------- */

static EvalResult tokenize(const char *input, TokenArray *out) {
    EvalResult res = { .error = EVAL_OK, .value = 0, .error_pos = -1 };
    int i = 0;
    int len = strlen(input);

    while (i < len) {
        /* skip whitespace */
        if (isspace(input[i])) { i++; continue; }

        /* number (possibly negative) */
        if (isdigit(input[i]) || input[i] == '.') {
            char *end;
            double val = strtod(&input[i], &end);
            Token t = { .type = TOK_NUMBER, .data.number = val };
            tokarray_push(out, t);
            i = end - input;
            continue;
        }

        /* negative number: '-' at start, after operator, or after '(' */
        if (input[i] == '-' &&
            (out->count == 0 ||
             out->items[out->count - 1].type == TOK_OPERATOR ||
             out->items[out->count - 1].type == TOK_LPAREN) &&
            i + 1 < len &&
            (isdigit(input[i + 1]) || input[i + 1] == '.')) {
            char *end;
            double val = strtod(&input[i], &end);
            Token t = { .type = TOK_NUMBER, .data.number = val };
            tokarray_push(out, t);
            i = end - input;
            continue;
        }

        /* operators and parentheses */
        OpKind op;
        switch (input[i]) {
            case '+': op = OP_ADD; goto push_op;
            case '-': op = OP_SUB; goto push_op;
            case '*': op = OP_MUL; goto push_op;
            case '/': op = OP_DIV; goto push_op;
            case '^': op = OP_POW; goto push_op;
            push_op: {
                Token t = { .type = TOK_OPERATOR, .data.op = op };
                tokarray_push(out, t);
                i++;
                continue;
            }
            case '(': {
                Token t = { .type = TOK_LPAREN };
                tokarray_push(out, t);
                i++;
                continue;
            }
            case ')': {
                Token t = { .type = TOK_RPAREN };
                tokarray_push(out, t);
                i++;
                continue;
            }
            default:
                res.error = EVAL_LEX_ERROR;
                res.error_pos = i;
                return res;
        }
    }

    return res;
}

/* --- Phase 2: Shunting-Yard ------------------------------- */

static int should_pop(OpKind stack_op, OpKind current_op) {
    if (op_assoc(current_op) == ASSOC_LEFT) {
        return op_precedence(stack_op) >= op_precedence(current_op);
    }
    return op_precedence(stack_op) > op_precedence(current_op);
}

static EvalResult to_postfix(const TokenArray *infix, TokenArray *postfix) {
    EvalResult res = { .error = EVAL_OK };
    TokenStack *ops = tokstack_create(infix->count);

    for (int i = 0; i < infix->count; i++) {
        Token t = infix->items[i];

        switch (t.type) {
            case TOK_NUMBER:
                tokarray_push(postfix, t);
                break;

            case TOK_OPERATOR:
                while (!tokstack_is_empty(ops)) {
                    Token top = tokstack_peek(ops);
                    if (top.type == TOK_LPAREN) break;
                    if (top.type != TOK_OPERATOR) break;
                    if (!should_pop(top.data.op, t.data.op)) break;
                    tokarray_push(postfix, tokstack_pop(ops));
                }
                tokstack_push(ops, t);
                break;

            case TOK_LPAREN:
                tokstack_push(ops, t);
                break;

            case TOK_RPAREN:
                while (!tokstack_is_empty(ops) &&
                       tokstack_peek(ops).type != TOK_LPAREN) {
                    tokarray_push(postfix, tokstack_pop(ops));
                }
                if (tokstack_is_empty(ops)) {
                    res.error = EVAL_MISMATCHED_PAREN;
                    goto cleanup;
                }
                tokstack_pop(ops);  /* discard '(' */
                break;
        }
    }

    while (!tokstack_is_empty(ops)) {
        Token top = tokstack_pop(ops);
        if (top.type == TOK_LPAREN) {
            res.error = EVAL_MISMATCHED_PAREN;
            goto cleanup;
        }
        tokarray_push(postfix, top);
    }

cleanup:
    tokstack_destroy(ops);
    return res;
}

/* --- Phase 3: Evaluate postfix ---------------------------- */

static EvalResult evaluate_postfix(const TokenArray *postfix) {
    EvalResult res = { .error = EVAL_OK, .value = 0 };
    DblStack *stack = dblstack_create(postfix->count);

    for (int i = 0; i < postfix->count; i++) {
        Token t = postfix->items[i];

        if (t.type == TOK_NUMBER) {
            dblstack_push(stack, t.data.number);
        } else if (t.type == TOK_OPERATOR) {
            if (dblstack_size(stack) < 2) {
                res.error = EVAL_TOO_FEW_OPERANDS;
                goto cleanup;
            }
            double b = dblstack_pop(stack);
            double a = dblstack_pop(stack);
            double r;
            switch (t.data.op) {
                case OP_ADD: r = a + b; break;
                case OP_SUB: r = a - b; break;
                case OP_MUL: r = a * b; break;
                case OP_DIV:
                    if (b == 0.0) {
                        res.error = EVAL_DIVISION_BY_ZERO;
                        goto cleanup;
                    }
                    r = a / b;
                    break;
                case OP_POW: r = pow(a, b); break;
            }
            dblstack_push(stack, r);
        }
    }

    if (dblstack_size(stack) != 1) {
        res.error = EVAL_TOO_MANY_OPERANDS;
        goto cleanup;
    }
    res.value = dblstack_pop(stack);

cleanup:
    dblstack_destroy(stack);
    return res;
}

/* --- Public API: full pipeline ---------------------------- */

EvalResult evaluate(const char *expr) {
    TokenArray infix = tokarray_create(32);
    TokenArray postfix = tokarray_create(32);

    /* Phase 1: lex */
    EvalResult res = tokenize(expr, &infix);
    if (res.error != EVAL_OK) goto cleanup;

    /* Phase 2: infix → postfix */
    res = to_postfix(&infix, &postfix);
    if (res.error != EVAL_OK) goto cleanup;

    /* Phase 3: evaluate postfix */
    res = evaluate_postfix(&postfix);

cleanup:
    tokarray_destroy(&infix);
    tokarray_destroy(&postfix);
    return res;
}

/* --- Main ------------------------------------------------- */

static const char *error_name(EvalError e) {
    switch (e) {
        case EVAL_OK:                return "OK";
        case EVAL_LEX_ERROR:         return "invalid character";
        case EVAL_MISMATCHED_PAREN:  return "mismatched parentheses";
        case EVAL_TOO_FEW_OPERANDS:  return "too few operands";
        case EVAL_TOO_MANY_OPERANDS: return "too many operands";
        case EVAL_DIVISION_BY_ZERO:  return "division by zero";
    }
    return "unknown";
}

int main(void) {
    const char *tests[] = {
        "3 + 4 * 2",
        "(3 + 4) * 2",
        "2 ^ 3 ^ 2",
        "10 - 3 - 2",
        "100 / (5 * (3 + 1 - 2))",
        "-3 + 4",
        "3 * (-2 + 5)",
        "2 ^ 10",
        "(1 + 2",
        "3 / 0",
        "3 + @",
    };
    int n = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < n; i++) {
        EvalResult r = evaluate(tests[i]);
        printf("%-30s -> ", tests[i]);
        if (r.error == EVAL_OK) {
            printf("%.4f\n", r.value);
        } else {
            printf("ERROR: %s\n", error_name(r.error));
        }
    }

    return 0;
}
```

Salida esperada:

```
3 + 4 * 2                      -> 11.0000
(3 + 4) * 2                    -> 14.0000
2 ^ 3 ^ 2                      -> 512.0000
10 - 3 - 2                     -> 5.0000
100 / (5 * (3 + 1 - 2))        -> 10.0000
-3 + 4                         -> 1.0000
3 * (-2 + 5)                   -> 9.0000
2 ^ 10                         -> 1024.0000
(1 + 2                         -> ERROR: mismatched parentheses
3 / 0                          -> ERROR: division by zero
3 + @                          -> ERROR: invalid character
```

### Notas de diseño en C

**Tagged union sin verificación**: Al acceder a `t.data.number`, el
compilador no verifica que `t.type == TOK_NUMBER`. Es responsabilidad del
programador. Un acceso incorrecto lee basura de memoria (los bytes de un
`double` interpretados como `OpKind` o viceversa).

**`goto` con múltiples `cleanup`**: La función `evaluate` tiene un solo
punto de limpieza que libera ambos `TokenArray`. Cada fase puede fallar y
saltar al mismo `cleanup`. Este patrón escala bien: añadir una fase más
solo requiere una línea de `free` adicional en `cleanup`.

**Lexer sin `strtok`**: A diferencia de T02/T03, este lexer procesa
carácter por carácter. Soporta tokens sin espacios (`(3+4)*2`) porque
no depende de delimitadores de whitespace.

### Compilación

```bash
gcc -std=c11 -Wall -Wextra -o expr_eval expr_eval.c -lm
./expr_eval
```

---

## 5. Implementación en Rust

```rust
// expr_eval.rs

use std::fmt;

/* --- Token types ------------------------------------------ */

#[derive(Debug, Clone, Copy, PartialEq)]
enum Op {
    Add,
    Sub,
    Mul,
    Div,
    Pow,
}

impl Op {
    fn precedence(self) -> i32 {
        match self {
            Op::Add | Op::Sub => 2,
            Op::Mul | Op::Div => 3,
            Op::Pow => 4,
        }
    }

    fn is_right_assoc(self) -> bool {
        matches!(self, Op::Pow)
    }

    fn apply(self, a: f64, b: f64) -> Result<f64, EvalError> {
        match self {
            Op::Add => Ok(a + b),
            Op::Sub => Ok(a - b),
            Op::Mul => Ok(a * b),
            Op::Div => {
                if b == 0.0 {
                    Err(EvalError::DivisionByZero)
                } else {
                    Ok(a / b)
                }
            }
            Op::Pow => Ok(a.powf(b)),
        }
    }
}

impl fmt::Display for Op {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let c = match self {
            Op::Add => '+',
            Op::Sub => '-',
            Op::Mul => '*',
            Op::Div => '/',
            Op::Pow => '^',
        };
        write!(f, "{}", c)
    }
}

#[derive(Debug, Clone, PartialEq)]
enum Token {
    Number(f64),
    Operator(Op),
    LeftParen,
    RightParen,
}

#[derive(Debug, PartialEq)]
enum EvalError {
    InvalidChar { ch: char, position: usize },
    MismatchedParentheses,
    TooFewOperands,
    TooManyOperands,
    DivisionByZero,
    EmptyExpression,
}

/* --- Phase 1: Lexer --------------------------------------- */

fn tokenize(input: &str) -> Result<Vec<Token>, EvalError> {
    let mut tokens = Vec::new();
    let chars: Vec<char> = input.chars().collect();
    let mut i = 0;

    while i < chars.len() {
        match chars[i] {
            ' ' | '\t' => { i += 1; }

            c if c.is_ascii_digit() || c == '.' => {
                let start = i;
                while i < chars.len() &&
                      (chars[i].is_ascii_digit() || chars[i] == '.') {
                    i += 1;
                }
                let s: String = chars[start..i].iter().collect();
                let val: f64 = s.parse().map_err(|_| {
                    EvalError::InvalidChar { ch: chars[start], position: start }
                })?;
                tokens.push(Token::Number(val));
            }

            '-' if (tokens.is_empty()
                    || matches!(tokens.last(),
                        Some(Token::Operator(_)) | Some(Token::LeftParen)))
                    && i + 1 < chars.len()
                    && (chars[i + 1].is_ascii_digit() || chars[i + 1] == '.') =>
            {
                let start = i;
                i += 1; // skip '-'
                while i < chars.len() &&
                      (chars[i].is_ascii_digit() || chars[i] == '.') {
                    i += 1;
                }
                let s: String = chars[start..i].iter().collect();
                let val: f64 = s.parse().map_err(|_| {
                    EvalError::InvalidChar { ch: '-', position: start }
                })?;
                tokens.push(Token::Number(val));
            }

            '+' => { tokens.push(Token::Operator(Op::Add)); i += 1; }
            '-' => { tokens.push(Token::Operator(Op::Sub)); i += 1; }
            '*' => { tokens.push(Token::Operator(Op::Mul)); i += 1; }
            '/' => { tokens.push(Token::Operator(Op::Div)); i += 1; }
            '^' => { tokens.push(Token::Operator(Op::Pow)); i += 1; }
            '(' => { tokens.push(Token::LeftParen); i += 1; }
            ')' => { tokens.push(Token::RightParen); i += 1; }

            c => {
                return Err(EvalError::InvalidChar { ch: c, position: i });
            }
        }
    }

    Ok(tokens)
}

/* --- Phase 2: Shunting-Yard ------------------------------- */

fn to_postfix(tokens: &[Token]) -> Result<Vec<Token>, EvalError> {
    let mut output: Vec<Token> = Vec::new();
    let mut ops: Vec<Token> = Vec::new();

    for token in tokens {
        match token {
            Token::Number(_) => output.push(token.clone()),

            Token::Operator(op) => {
                while let Some(top) = ops.last() {
                    match top {
                        Token::LeftParen => break,
                        Token::Operator(top_op) => {
                            let should_pop = if op.is_right_assoc() {
                                top_op.precedence() > op.precedence()
                            } else {
                                top_op.precedence() >= op.precedence()
                            };
                            if should_pop {
                                output.push(ops.pop().unwrap());
                            } else {
                                break;
                            }
                        }
                        _ => break,
                    }
                }
                ops.push(token.clone());
            }

            Token::LeftParen => ops.push(token.clone()),

            Token::RightParen => {
                loop {
                    match ops.pop() {
                        None => return Err(EvalError::MismatchedParentheses),
                        Some(Token::LeftParen) => break,
                        Some(t) => output.push(t),
                    }
                }
            }
        }
    }

    while let Some(top) = ops.pop() {
        if top == Token::LeftParen {
            return Err(EvalError::MismatchedParentheses);
        }
        output.push(top);
    }

    Ok(output)
}

/* --- Phase 3: Evaluate postfix ---------------------------- */

fn evaluate_postfix(tokens: &[Token]) -> Result<f64, EvalError> {
    let mut stack: Vec<f64> = Vec::new();

    for token in tokens {
        match token {
            Token::Number(val) => stack.push(*val),
            Token::Operator(op) => {
                let b = stack.pop().ok_or(EvalError::TooFewOperands)?;
                let a = stack.pop().ok_or(EvalError::TooFewOperands)?;
                stack.push(op.apply(a, b)?);
            }
            _ => {} // parentheses should not appear in postfix
        }
    }

    match stack.len() {
        0 => Err(EvalError::EmptyExpression),
        1 => Ok(stack[0]),
        _ => Err(EvalError::TooManyOperands),
    }
}

/* --- Public API ------------------------------------------- */

fn evaluate(expr: &str) -> Result<f64, EvalError> {
    let tokens = tokenize(expr)?;
    let postfix = to_postfix(&tokens)?;
    evaluate_postfix(&postfix)
}

/* --- Main ------------------------------------------------- */

fn main() {
    let tests = [
        "3 + 4 * 2",
        "(3 + 4) * 2",
        "2 ^ 3 ^ 2",
        "10 - 3 - 2",
        "100 / (5 * (3 + 1 - 2))",
        "-3 + 4",
        "3 * (-2 + 5)",
        "2 ^ 10",
        "(1 + 2",
        "3 / 0",
        "3 + @",
    ];

    for expr in &tests {
        print!("{:<30} -> ", expr);
        match evaluate(expr) {
            Ok(val) => println!("{:.4}", val),
            Err(e) => println!("ERROR: {:?}", e),
        }
    }
}
```

### Puntos clave de la implementación Rust

**`Op::apply` como método**: La lógica de evaluación está en el tipo `Op`
mismo. Esto es el equivalente Rust de "enviar un mensaje al objeto" — el
operador sabe cómo aplicarse. En C, esa lógica vive en un `switch` dentro
del evaluador, separada de la definición del operador.

**Pattern matching anidado**: En `to_postfix`, el `while let Some(top) =
ops.last()` seguido de `match top` permite inspeccionar el tope de la pila
sin sacarlo, y solo hacer `pop` cuando la condición se cumple. En C esto
requiere un `peek` explícito seguido de un `pop` condicional.

**Propagación con `?`**: La función `evaluate` tiene solo 3 líneas. Cada
`?` propaga errores automáticamente. El equivalente en C es el bloque
`if (res.error != EVAL_OK) goto cleanup` repetido entre cada fase.

**`Token::clone()`**: Los tokens se clonan al moverlos entre listas. Para
`Number(f64)` y `Operator(Op)`, `clone` es una copia trivial (8 bytes y
1 byte respectivamente). No hay allocación de heap.

---

## 6. Manejo de errores en el pipeline

Cada fase puede producir errores diferentes. El pipeline los propaga hacia
arriba con contexto:

```
         ┌──────────────┐
         │    Lexer     │ → InvalidChar { ch, position }
         └──────┬───────┘
                │ Ok(tokens)
         ┌──────▼───────┐
         │ Shunting-Yard│ → MismatchedParentheses
         └──────┬───────┘
                │ Ok(postfix)
         ┌──────▼───────┐
         │  Evaluador   │ → TooFewOperands
         │              │ → TooManyOperands
         │              │ → DivisionByZero
         └──────┬───────┘
                │ Ok(value)
                ▼
             f64
```

### Error temprano

Si el lexer falla, las fases 2 y 3 no se ejecutan. Esto es deseable:
no tiene sentido intentar parsear tokens si la cadena tiene caracteres
inválidos. En C, la cadena `if ... goto cleanup` logra esto. En Rust,
el operador `?` lo hace implícito.

### Errores que no se detectan

| Problema | Fase que lo detecta | Fase que NO lo detecta |
|----------|:-------------------:|:---------------------:|
| Carácter inválido `@` | Lexer | — |
| Paréntesis sin par `((` | Shunting-Yard | Lexer |
| Dos operadores seguidos `3 + + 4` | Evaluador (TooFewOperands) | Lexer, Shunting-Yard |
| Dos números seguidos `3 4 + 5` | Evaluador (TooManyOperands) | Lexer, Shunting-Yard |

Observa que `3 + + 4` pasa el lexer (tokens válidos individualmente) y
Shunting-Yard (no hay paréntesis mal formados), pero falla en el evaluador
porque al evaluar el primer `+`, la pila solo tiene un operando.

Un parser más sofisticado podría detectar estos errores antes, pero para un
evaluador de expresiones aritméticas, delegar al evaluador es pragmático y
correcto.

---

## 7. Extensión con variables

Para soportar variables como `x + y * 2`, se añade una variante al token y
una tabla de valores:

### En Rust

```rust
#[derive(Debug, Clone, PartialEq)]
enum Token {
    Number(f64),
    Variable(String),   // new
    Operator(Op),
    LeftParen,
    RightParen,
}
```

El lexer detecta identificadores (secuencias de letras):

```rust
c if c.is_ascii_alphabetic() => {
    let start = i;
    while i < chars.len() && chars[i].is_ascii_alphanumeric() {
        i += 1;
    }
    let name: String = chars[start..i].iter().collect();
    tokens.push(Token::Variable(name));
}
```

El evaluador recibe un mapa de valores:

```rust
use std::collections::HashMap;

fn evaluate_with_vars(
    expr: &str,
    vars: &HashMap<String, f64>,
) -> Result<f64, EvalError> {
    let tokens = tokenize(expr)?;
    let postfix = to_postfix(&tokens)?;
    evaluate_postfix_vars(&postfix, vars)
}
```

Al encontrar `Token::Variable(name)`, busca en el mapa:

```rust
Token::Variable(name) => {
    let val = vars.get(name).ok_or_else(|| {
        EvalError::UndefinedVariable(name.clone())
    })?;
    stack.push(*val);
}
```

### En C

En C, la variante de variable almacena el nombre como cadena:

```c
typedef struct {
    TokenType type;
    union {
        double  number;
        OpKind  op;
        char    var_name[32];   /* variable name */
    } data;
} Token;
```

La tabla de variables puede ser un array simple de pares nombre-valor:

```c
typedef struct {
    char   name[32];
    double value;
} Variable;

/* linear search — sufficient for small variable counts */
double lookup_var(const Variable *vars, int nvars, const char *name) {
    for (int i = 0; i < nvars; i++) {
        if (strcmp(vars[i].name, name) == 0) return vars[i].value;
    }
    return 0.0; /* or signal error */
}
```

---

## 8. Comparación: tagged union vs enum

Esta sección profundiza en cómo C y Rust representan el mismo concepto de
"un valor que puede ser una de varias cosas":

### Representación en memoria

```
C (Token):                          Rust (Token):
┌────────────┬──────────────┐       ┌────────────┬──────────────┐
│ type (4B)  │ data (8B)    │       │ tag (1B*)  │ data (8B)    │
│ TOK_NUMBER │ 3.14 (f64)   │       │ 0          │ 3.14 (f64)   │
└────────────┴──────────────┘       └────────────┴──────────────┘
  Total: 12 bytes (+ padding)        Total: 16 bytes (con align)
```

*El tag de Rust puede ser 1 byte si hay ≤ 256 variantes.

### Garantías

| Aspecto | C tagged union | Rust enum |
|---------|:--------------:|:---------:|
| Acceso al campo correcto | Responsabilidad del programador | Garantizado por `match` |
| Exhaustividad | No (puede olvidar un case) | Sí (error de compilación) |
| Dato inválido | UB silencioso | Imposible en safe Rust |
| Patrón de uso | `switch` + acceso manual | `match` con destructuring |
| Añadir variante | Puede compilar sin manejarla | Error en cada `match` incompleto |

### Ejemplo concreto

Acceder al valor de un token número:

```c
/* C — el programador DEBE verificar el tipo */
if (t.type == TOK_NUMBER) {
    double val = t.data.number;  /* OK */
}
/* si se olvida el if: undefined behavior silencioso */
double val = t.data.number;  /* compila, puede dar basura */
```

```rust
// Rust — el compilador OBLIGA a verificar
if let Token::Number(val) = t {
    // val is f64, guaranteed
}
// no hay forma de acceder al f64 sin match/if-let
```

### El patrón `match` con destructuring

```rust
match token {
    Token::Number(val) => { /* val: f64 disponible */ }
    Token::Operator(op) => { /* op: Op disponible */ }
    Token::LeftParen => { /* no data */ }
    Token::RightParen => { /* no data */ }
}
// si se añade Token::Variable(name) y no se maneja: ERROR
```

Esto es fundamental: al añadir `Token::Variable`, **todo `match` que no lo
maneje produce un error de compilación**. En C, olvidar un `case` en un
`switch` compila sin advertencias (salvo `-Wswitch`, que no todos activan).

---

## 9. Análisis de complejidad

### Tiempo

Cada fase es $O(n)$ donde $n$ es la longitud de la entrada:

| Fase | Operaciones | Complejidad |
|------|-------------|:-----------:|
| Lexer | Recorre cada carácter una vez | $O(n)$ |
| Shunting-Yard | Cada token push/pop una vez (amortizado) | $O(n)$ |
| Evaluador | Cada token push/pop una vez | $O(n)$ |
| **Total** | Tres pasadas secuenciales | $O(n)$ |

Las tres fases secuenciales suman $O(n) + O(n) + O(n) = O(3n) = O(n)$.
La constante 3 es insignificante comparada con la claridad de la separación
en fases.

### Espacio

| Estructura | Tamaño máximo | Uso |
|------------|:-------------:|-----|
| Array de tokens (infija) | $O(n)$ | Salida del lexer |
| Array de tokens (posfija) | $O(n)$ | Salida de Shunting-Yard |
| Pila de operadores | $O(n)$ | Temporal en Shunting-Yard |
| Pila de evaluación | $O(n)$ | Temporal en evaluador |
| **Total** | $O(n)$ | |

Todos son $O(n)$ en el peor caso. En la práctica, la pila de operadores
rara vez excede $O(\sqrt{n})$ para expresiones reales.

### ¿Se puede hacer en una pasada?

Sí: se puede combinar Shunting-Yard y evaluación en una sola pasada,
evaluando operadores al sacarlos de la pila en lugar de colocarlos en una
cola de salida. Esto elimina el array de posfija y reduce el espacio a
$O(n)$ con menor constante, pero mezcla las responsabilidades y hace el
código más difícil de testar y extender.

---

## 10. Ejercicios

---

### Ejercicio 1 — Traza del pipeline

Traza las 3 fases completas para la expresión `"(2 + 3) * 4 ^ 2"`:
1. Lexer: ¿qué tokens produce?
2. Shunting-Yard: ¿cuál es la posfija?
3. Evaluador: ¿cuál es el resultado?

<details>
<summary>¿En qué orden se ejecutan `*` y `^`?</summary>

**Fase 1 — Lexer**:
```
Tokens: [LeftParen, Number(2), Operator(Add), Number(3),
         RightParen, Operator(Mul), Number(4), Operator(Pow), Number(2)]
```

**Fase 2 — Shunting-Yard**:
```
Token       Acción                  Output           Op Stack
──────────────────────────────────────────────────────────────
(           push                                     (
2           output                  2                (
+           tope '(' → push        2                ( +
3           output                  2 3              ( +
)           sacar +, descartar (    2 3 +
*           push                    2 3 +            *
4           output                  2 3 + 4          *
^           prec(^)>prec(*) → push  2 3 + 4          * ^
2           output                  2 3 + 4 2        * ^
FIN         sacar ^, sacar *        2 3 + 4 2 ^ *
```

Posfija: `2 3 + 4 2 ^ *`

**Fase 3 — Evaluación**:
```
2, 3 → push
+ → 2+3 = 5
4, 2 → push
^ → 4^2 = 16
* → 5*16 = 80
```

Resultado: **80**. El `^` se ejecuta antes que `*` porque tiene mayor
precedencia. Equivale a $(2+3) \times 4^2 = 5 \times 16 = 80$.

</details>

---

### Ejercicio 2 — Números negativos en el lexer

Predice qué tokens produce el lexer para cada cadena:

```
a)  "-5 + 3"
b)  "4 * -2"
c)  "(-3)"
d)  "5 - -3"
e)  "--5"
```

<details>
<summary>¿Cuáles producen un `Number` negativo y cuáles un `Operator(Sub)`?</summary>

```
a)  "-5 + 3"
    → [Number(-5), Operator(Add), Number(3)]
    '-' al inicio + dígito siguiente → número negativo

b)  "4 * -2"
    → [Number(4), Operator(Mul), Number(-2)]
    '-' después de operador + dígito siguiente → número negativo

c)  "(-3)"
    → [LeftParen, Number(-3), RightParen]
    '-' después de '(' + dígito siguiente → número negativo

d)  "5 - -3"
    → [Number(5), Operator(Sub), Number(-3)]
    Primer '-' después de número → operador Sub
    Segundo '-' después de operador + dígito → número negativo

e)  "--5"
    → [Operator(Sub), Number(-5)]
    Primer '-' al inicio, pero siguiente es '-' no dígito → operador Sub
    Segundo '-' después de operador + dígito → número negativo
    (evaluar daría: 0 - (-5) = 5... pero falla por TooFewOperands
    porque Sub necesita 2 operandos y solo hay Number(-5))
```

El caso (e) revela una limitación: el lexer produce tokens válidos
individualmente, pero la expresión no es evaluable como infija binaria. Una
solución es tratar el `-` unario como un operador especial (`Negate`) en
lugar de incorporarlo al número.

</details>

---

### Ejercicio 3 — Tagged union en C

Dado el siguiente código C, ¿qué imprime y por qué?

```c
Token t;
t.type = TOK_OPERATOR;
t.data.op = OP_MUL;
printf("%.2f\n", t.data.number);
```

<details>
<summary>¿Es undefined behavior? ¿Qué vería un debugger?</summary>

Es **undefined behavior**. Se accede a `data.number` (un `double` de 8
bytes) cuando el campo activo es `data.op` (un `int/enum` de típicamente
4 bytes).

Lo que se imprime es basura: los bytes de `OP_MUL` (valor 2, es decir
`0x00000002`) interpretados como parte de un `double` IEEE 754. Los 4
bytes restantes del `double` serían lo que haya en memoria (no
inicializados).

Un debugger con sanitizers (`-fsanitize=undefined`) podría detectarlo.
Valgrind probablemente lo marque como lectura de memoria no inicializada.

En Rust, este error es **imposible**:

```rust
let t = Token::Operator(Op::Mul);
// no hay forma de pedir el f64 — el tipo solo permite match
```

</details>

---

### Ejercicio 4 — Añadir operador módulo al pipeline

Extiende ambas implementaciones (C y Rust) para soportar `%` con precedencia
3 y asociatividad izquierda. Enumera todos los cambios necesarios en cada
fase.

<details>
<summary>¿Cuántas líneas hay que modificar en cada lenguaje?</summary>

**En Rust** (cambios mínimos gracias a `match` exhaustivo):

1. `Op` enum: añadir `Mod`
2. `Op::precedence`: añadir `Op::Mod => 3`
3. `Op::is_right_assoc`: no cambia (ya retorna `false` para default)
4. `Op::apply`: añadir `Op::Mod => Ok(a % b)` (o `a.rem_euclid(b)`)
5. `Op::Display`: añadir `Op::Mod => '%'`
6. Lexer: añadir `'%' => { tokens.push(Token::Operator(Op::Mod)); i += 1; }`

Total: **~6 líneas** modificadas. El compilador señalará los `match` que
falten.

**En C**:

1. `OpKind` enum: añadir `OP_MOD`
2. `op_precedence`: añadir `case OP_MOD: return 3;`
3. `op_assoc`: no cambia (default es `ASSOC_LEFT`)
4. `op_char`: añadir `case OP_MOD: return '%';`
5. Lexer: añadir `case '%': op = OP_MOD; goto push_op;`
6. Evaluador: añadir `case OP_MOD: r = fmod(a, b); break;`

Total: **~6 líneas**. Pero el compilador **no advierte** si se olvida
algún `case` (salvo con `-Wswitch-enum`).

</details>

---

### Ejercicio 5 — Expresiones con espacios opcionales

Verifica que la implementación del lexer carácter por carácter maneja
correctamente estas variantes de la misma expresión:

```
"3+4*2"
"3 + 4 * 2"
"3+ 4 *2"
" 3 + 4 * 2 "
```

<details>
<summary>¿Todas producen los mismos tokens? ¿Hay algún caso problemático?</summary>

Sí, todas producen los mismos tokens:
`[Number(3), Operator(Add), Number(4), Operator(Mul), Number(2)]`

El lexer salta espacios y reconoce números/operadores por carácter, así
que los espacios entre tokens son irrelevantes.

Caso potencialmente problemático: `"3 +4"` — ¿es `3`, `+`, `4` o `3`,
`+4` (número positivo)? Con nuestro lexer, el `+` no se trata como signo
positivo (solo `-` tiene esa lógica), así que se parsea como operador.
Resultado: `[Number(3), Operator(Add), Number(4)]` — correcto.

Otro caso: `"3.14.15"` — dos puntos decimales. `strtod` y `parse::<f64>()`
leerían `3.14` y pararían en el segundo punto. El lexer basado en
`is_digit || '.'` leería `3.14.15` como un solo token y el parsing
fallaría. La versión con `strtod` es más robusta aquí.

</details>

---

### Ejercicio 6 — Pipeline inverso

Implementa en Rust una función `postfix_to_infix(tokens: &[Token]) -> String`
que tome tokens posfijos tipados y reconstruya la expresión infija con
paréntesis mínimos. Usa la pila, pero en lugar de números, apila structs
con la cadena y la precedencia del operador más externo.

<details>
<summary>¿Cuándo se necesitan paréntesis y cuándo no?</summary>

Se necesitan paréntesis alrededor de un subexpresión cuando su operador
tiene **menor precedencia** que el operador que la contiene:

```rust
struct InfixExpr {
    text: String,
    precedence: i32,  // -1 for atoms (numbers)
}

fn postfix_to_infix(tokens: &[Token]) -> String {
    let mut stack: Vec<InfixExpr> = Vec::new();

    for token in tokens {
        match token {
            Token::Number(val) => {
                stack.push(InfixExpr {
                    text: format!("{}", val),
                    precedence: -1,  // atoms never need parens
                });
            }
            Token::Operator(op) => {
                let b = stack.pop().unwrap();
                let a = stack.pop().unwrap();
                let prec = op.precedence();

                let left = if a.precedence >= 0 && a.precedence < prec {
                    format!("({})", a.text)
                } else {
                    a.text
                };

                // right operand also needs parens if same prec
                // and left-associative (to preserve grouping)
                let right = if b.precedence >= 0 &&
                    (b.precedence < prec ||
                     (b.precedence == prec && !op.is_right_assoc()))
                {
                    format!("({})", b.text)
                } else {
                    b.text
                };

                stack.push(InfixExpr {
                    text: format!("{} {} {}", left, op, right),
                    precedence: prec,
                });
            }
            _ => {}
        }
    }

    stack.pop().unwrap().text
}
```

Ejemplo: tokens de `3 4 2 * +`:
- `3`, `4`, `2` → push atoms
- `*` → `4 * 2` (prec 3)
- `+` → `3 + 4 * 2` (no paréntesis: prec de `4*2` es 3 > 2 de `+`)

Tokens de `3 4 + 2 *`:
- `3`, `4` → push atoms
- `+` → `3 + 4` (prec 2)
- `*` → `(3 + 4) * 2` (paréntesis: prec de `3+4` es 2 < 3 de `*`)

</details>

---

### Ejercicio 7 — Calculadora REPL

Combina lexer + Shunting-Yard + evaluador en un programa interactivo que
lea expresiones de stdin, las evalúe, e imprima el resultado. Añade el
comando `quit` para salir. Implementa en C o Rust.

<details>
<summary>¿Cómo manejas la lectura de líneas y la detección de "quit"?</summary>

En Rust:

```rust
use std::io::{self, BufRead, Write};

fn main() {
    let stdin = io::stdin();
    print!("> ");
    io::stdout().flush().unwrap();

    for line in stdin.lock().lines() {
        let line = line.unwrap();
        let trimmed = line.trim();

        if trimmed.eq_ignore_ascii_case("quit") {
            break;
        }
        if trimmed.is_empty() {
            print!("> ");
            io::stdout().flush().unwrap();
            continue;
        }

        match evaluate(trimmed) {
            Ok(val) => println!("= {}", val),
            Err(e) => println!("ERROR: {:?}", e),
        }

        print!("> ");
        io::stdout().flush().unwrap();
    }
}
```

En C, usar `fgets` (como en T02 ejercicio 9) con `strcmp` para detectar
`"quit\n"`. El `flush` en Rust es necesario porque `print!` (sin `ln`)
no flushea automáticamente — el prompt no aparecería hasta que se escriba
una línea completa.

</details>

---

### Ejercicio 8 — Errores con contexto

Modifica la implementación en Rust para que `EvalError` incluya la fase
que produjo el error:

```rust
enum EvalError {
    LexError { phase: &'static str, detail: String },
    ParseError { phase: &'static str, detail: String },
    RuntimeError { phase: &'static str, detail: String },
}
```

Genera mensajes como: `"[Lexer] Invalid character '@' at position 4"`.

<details>
<summary>¿Es mejor usar un enum plano o un enum con fase embebida?</summary>

Depende del contexto. El enum plano (como nuestra implementación actual) es
mejor para **manejar programáticamente** los errores (cada variante se puede
matchear independientemente). El enum con fase es mejor para **reportar al
usuario**.

Un enfoque intermedio es mantener el enum original y añadir un método
`Display`:

```rust
impl fmt::Display for EvalError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            EvalError::InvalidChar { ch, position } => {
                write!(f, "[Lexer] Invalid character '{}' at position {}",
                       ch, position)
            }
            EvalError::MismatchedParentheses => {
                write!(f, "[Parser] Mismatched parentheses")
            }
            EvalError::TooFewOperands => {
                write!(f, "[Evaluator] Too few operands for operator")
            }
            EvalError::TooManyOperands => {
                write!(f, "[Evaluator] Too many operands left on stack")
            }
            EvalError::DivisionByZero => {
                write!(f, "[Evaluator] Division by zero")
            }
            EvalError::EmptyExpression => {
                write!(f, "[Evaluator] Empty expression")
            }
        }
    }
}
```

Esto da lo mejor de ambos mundos: el tipo es preciso para `match` y el
display es legible para humanos. En el REPL, usar `println!("ERROR: {}", e)`
en lugar de `println!("ERROR: {:?}", e)`.

</details>

---

### Ejercicio 9 — Test de las tres fases

Escribe tests unitarios en Rust para cada fase por separado:

1. Test del lexer: `"3.14 + 2"` produce `[Number(3.14), Operator(Add), Number(2)]`.
2. Test de Shunting-Yard: tokens de `3 + 4 * 2` producen posfija `3 4 2 * +`.
3. Test del evaluador: tokens posfijos `[3, 4, 2, *, +]` producen `11.0`.

<details>
<summary>¿Cómo comparas `f64` en los tests? ¿Y los tokens?</summary>

Para `f64`, usar tolerancia epsilon:

```rust
fn approx_eq(a: f64, b: f64) -> bool {
    (a - b).abs() < 1e-10
}
```

Para tokens, el `#[derive(PartialEq)]` permite comparación directa, pero
`f64` no implementa `Eq`, así que `Token` tampoco. Los tests deben
comparar campo por campo:

```rust
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_lexer() {
        let tokens = tokenize("3.14 + 2").unwrap();
        assert_eq!(tokens.len(), 3);
        assert!(matches!(&tokens[0], Token::Number(v) if (*v - 3.14).abs() < 1e-10));
        assert_eq!(tokens[1], Token::Operator(Op::Add));
        assert!(matches!(&tokens[2], Token::Number(v) if (*v - 2.0).abs() < 1e-10));
    }

    #[test]
    fn test_shunting_yard() {
        let tokens = tokenize("3 + 4 * 2").unwrap();
        let postfix = to_postfix(&tokens).unwrap();
        // expected: 3, 4, 2, *, +
        assert!(matches!(&postfix[0], Token::Number(v) if (*v - 3.0).abs() < 1e-10));
        assert!(matches!(&postfix[1], Token::Number(v) if (*v - 4.0).abs() < 1e-10));
        assert!(matches!(&postfix[2], Token::Number(v) if (*v - 2.0).abs() < 1e-10));
        assert_eq!(postfix[3], Token::Operator(Op::Mul));
        assert_eq!(postfix[4], Token::Operator(Op::Add));
    }

    #[test]
    fn test_evaluate() {
        let result = evaluate("3 + 4 * 2").unwrap();
        assert!(approx_eq(result, 11.0));
    }

    #[test]
    fn test_division_by_zero() {
        let result = evaluate("1 / 0");
        assert_eq!(result, Err(EvalError::DivisionByZero));
    }
}
```

El patrón `matches!(&tokens[0], Token::Number(v) if ...)` combina pattern
matching con guard para verificar el valor `f64` con tolerancia.

</details>

---

### Ejercicio 10 — Benchmark: tokens tipados vs strings

Implementa una versión del evaluador que use `&str` para todo (como en T02)
y otra que use `Token` (como en este tópico). Mide el tiempo de evaluar
la expresión `"1 + 2 * 3 + 4 * 5 + 6 * 7 + 8 * 9 + 10"` repetida 100,000
veces.

<details>
<summary>¿Cuál es más rápida y por qué?</summary>

La versión con tokens tipados es más rápida por dos razones:

1. **Parsing único**: La cadena se parsea a `f64` una sola vez (en el lexer).
   La versión string re-parsea en cada evaluación si la expresión se reutiliza.

2. **Branch prediction**: `match` sobre un enum es un salto indirecto sobre
   un entero pequeño (0-4), que el CPU predice bien. Comparar strings
   requiere `strcmp` (acceso a memoria, comparación byte a byte).

Benchmark aproximado (varía según hardware):

```
Versión string:  ~180 ms para 100K iteraciones
Versión tokens:  ~120 ms para 100K iteraciones
```

La diferencia se amplifica con expresiones más largas o con re-evaluación
(misma expresión, diferentes variables). En un compilador real, los tokens
se generan una vez y se recorren muchas veces durante optimización.

Para medir en Rust:

```rust
use std::time::Instant;

let start = Instant::now();
for _ in 0..100_000 {
    let _ = evaluate("1 + 2 * 3 + 4 * 5 + 6 * 7 + 8 * 9 + 10");
}
let elapsed = start.elapsed();
println!("Tokens: {:.2?}", elapsed);
```

</details>
