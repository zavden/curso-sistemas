# T04 — Operador coma y ternario

## Erratas detectadas

| Ubicación | Error | Corrección |
|---|---|---|
| README.md líneas 165-166 | La macro `SWAP` usa `((type) _tmp = (a), ...)` — no es posible declarar variables dentro de una expresión con operador coma | La forma con coma no funciona. Usar `do { type _tmp = (a); (a) = (b); (b) = _tmp; } while(0)` o una statement expression de GCC: `({ type _tmp = (a); (a) = (b); (b) = _tmp; })` |
| labs/README.md líneas 377-379 | Dice que `flags & 0x01 ? "set" : "clear"` se parsea mal porque `?:` (nivel 13) tiene "mayor precedencia" que `&` (nivel 8) | Incorrecto: nivel menor = mayor precedencia. `&` (8) tiene **mayor** precedencia que `?:` (13), así que la expresión se parsea correctamente como `(flags & 0x01) ? "set" : "clear"`. El verdadero problema de precedencia con `&` es contra `==`: `flags & 0x01 == 1` se parsea como `flags & (0x01 == 1)` porque `==` (7) > `&` (8) |

---

## 1. Operador ternario (`?:`)

El ternario es la única **expresión condicional** de C. Evalúa una condición y retorna uno de dos valores:

```c
// Sintaxis: condición ? valor_si_true : valor_si_false

int max = (a > b) ? a : b;

// Equivalente a:
int max;
if (a > b) {
    max = a;
} else {
    max = b;
}
```

La diferencia clave: el ternario es una **expresión** (retorna un valor), no un statement. Puede usarse donde se espera un valor:

```c
printf("x es %s\n", (x > 0) ? "positivo" : "no positivo");

int abs_val = (x >= 0) ? x : -x;

// Inicialización de const — imposible con if/else:
const char *label = (error) ? "FAIL" : "OK";

// Retorno condicional:
return (x >= 0) ? x : -x;
```

### Usos legítimos

```c
// 1. Asignación condicional simple:
int min = (a < b) ? a : b;

// 2. Argumentos condicionales (plurales):
printf("Found %d item%s\n", n, (n == 1) ? "" : "s");

// 3. Inicialización de const:
const int mode = (debug) ? MODE_DEBUG : MODE_RELEASE;

// 4. Macros simples:
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ABS(x)    ((x) >= 0 ? (x) : -(x))
// CUIDADO: estas macros evalúan los argumentos DOS VECES
// MIN(i++, j++) tiene side effects duplicados
```

### Cuándo NO usar el ternario

```c
// NO anidar ternarios — ilegible:
int result = (a > b) ? (a > c ? a : c) : (b > c ? b : c);

// Mejor:
int result = a;
if (b > result) result = b;
if (c > result) result = c;

// NO usar para ejecutar acciones (usar if/else):
// Mal: (condition) ? do_something() : do_other();
// Bien: if (condition) { do_something(); } else { do_other(); }
```

### Tipo del resultado

El tipo sigue las conversiones aritméticas usuales — siempre el tipo más amplio de ambas ramas, independientemente de cuál se seleccione:

```c
int i = 5;
double d = 3.14;
// (1) ? i : d → el resultado es double (i se promueve)
// sizeof(condition ? i : d) == 8 (sizeof(double))
```

### Asociatividad

El ternario asocia de **derecha a izquierda**:

```c
a ? b : c ? d : e
// se parsea como: a ? b : (c ? d : e)
// NO como: (a ? b : c) ? d : e

// Permite encadenar (aunque un switch es más claro):
const char *s = (n == 1) ? "one"
              : (n == 2) ? "two"
              : (n == 3) ? "three"
              :            "other";
```

---

## 2. Operador coma (`,`)

El operador coma evalúa expresiones de izquierda a derecha y retorna el valor de la **última**:

```c
int x = (1, 2, 3);    // x = 3
// Evalúa 1 (descarta), evalúa 2 (descarta), retorna 3
```

### Coma como operador vs como separador

La coma tiene **dos roles** en C:

```c
// 1. SEPARADOR (no es un operador):
int a, b, c;                    // separa declaraciones
foo(1, 2, 3);                   // separa argumentos
int arr[] = {1, 2, 3};          // separa inicializadores

// 2. OPERADOR (evalúa izq→der, retorna el último):
int x = (a = 1, b = 2, a + b); // x = 3

// Para usar coma como operador donde se espera un separador → paréntesis:
foo((a++, b));    // incrementa a, pasa b como argumento (1 arg)
// Sin paréntesis: foo(a++, b) — dos argumentos separados
```

### Usos legítimos

```c
// 1. En for — múltiples variables o incrementos:
for (int i = 0, j = 10; i < j; i++, j--) {
    // i++ y j-- son operador coma (se ejecutan ambos)
    // "int i = 0, j = 10" es separador de declaración
}

// 2. En condiciones while (leer + verificar):
while (c = getchar(), c != EOF && c != '\n') {
    // lee un char Y verifica que no es EOF ni newline
}
```

### Cuándo NO usar la coma

```c
// En casi todos los demás casos, preferir statements separados:

// Mal estilo:
x = 1, y = 2, z = 3;

// Bien:
x = 1;
y = 2;
z = 3;
```

---

## 3. Precedencia de la coma

La coma tiene la **menor precedencia** de todos los operadores (nivel 15). La asignación (nivel 14) tiene mayor precedencia:

```c
x = 1, y = 2;     // se parsea como (x = 1), (y = 2) — dos expresiones
x = (1, y = 2);   // se parsea como x = ((1), (y = 2)) → x = 2
```

```c
// Cadena completa de precedencia:
// aritmética > comparación > ternario > asignación > coma

r = val > 5 ? val * 2 : val + 1, printf("side effect\n");
// se parsea como:
// (r = (val > 5 ? val * 2 : val + 1)), (printf("side effect\n"))
// r recibe el resultado del ternario; printf se ejecuta como side effect
```

---

## 4. Tabla de precedencia completa

```
Precedencia  Operador                         Asociatividad
──────────────────────────────────────────────────────────────
1 (máxima)   () [] -> .  postfix ++ --       izquierda → derecha
2            prefix ++ --  + - ! ~           derecha → izquierda
             (type) * & sizeof
3            * / %                           izquierda → derecha
4            + -                             izquierda → derecha
5            << >>                           izquierda → derecha
6            < <= > >=                       izquierda → derecha
7            == !=                           izquierda → derecha
8            &                               izquierda → derecha
9            ^                               izquierda → derecha
10           |                               izquierda → derecha
11           &&                              izquierda → derecha
12           ||                              izquierda → derecha
13           ?:                              derecha → izquierda
14           = += -= *= /= %= etc.           derecha → izquierda
15 (mínima)  ,                               izquierda → derecha
```

**Trampas de precedencia con `&`:**

`&` (nivel 8) tiene **menor** precedencia que `==` (nivel 7), pero **mayor** que `?:` (nivel 13):

```c
// TRAMPA: & vs ==
flags & MASK == 1       // se parsea como: flags & (MASK == 1)
// Correcto: (flags & MASK) == 1

// OK: & vs ?:
flags & 0x01 ? "yes" : "no"  // se parsea como: (flags & 0x01) ? "yes" : "no"
// Los paréntesis son opcionales aquí, pero recomendados para claridad
```

---

## Ejercicios

### Ejercicio 1 — Ternario básico

```c
#include <stdio.h>

int main(void) {
    int a = 7, b = 3;
    int max = (a > b) ? a : b;
    int min = (a < b) ? a : b;

    printf("max(%d, %d) = %d\n", a, b, max);
    printf("min(%d, %d) = %d\n", a, b, min);

    int x = -5;
    int abs_x = (x >= 0) ? x : -x;
    printf("|%d| = %d\n", x, abs_x);

    for (int n = 0; n <= 3; n++) {
        printf("%d item%s\n", n, (n == 1) ? "" : "s");
    }

    return 0;
}
```

**¿Qué imprime cada línea?**

<details>
<summary>Predicción</summary>

```
max(7, 3) = 7      // 7 > 3 → true → retorna a (7)
min(7, 3) = 3      // 7 < 3 → false → retorna b (3)
|-5| = 5            // -5 >= 0 → false → retorna -(-5) = 5
0 items             // 0 == 1 → false → "s"
1 item              // 1 == 1 → true → ""
2 items             // 2 == 1 → false → "s"
3 items             // 3 == 1 → false → "s"
```

</details>

---

### Ejercicio 2 — Ternario y tipo del resultado

```c
#include <stdio.h>

int main(void) {
    int i = 5;
    double d = 3.14;

    printf("sizeof(1 ? i : d) = %zu\n", sizeof(1 ? i : d));
    printf("sizeof(0 ? i : d) = %zu\n", sizeof(0 ? i : d));
    printf("value = %f\n", 1 ? i : d);

    return 0;
}
```

**¿Los dos `sizeof` dan el mismo resultado? ¿El valor impreso es 5 o 5.000000?**

<details>
<summary>Predicción</summary>

```
sizeof(1 ? i : d) = 8    // tipo del resultado = double (el más amplio)
sizeof(0 ? i : d) = 8    // MISMO tipo — no depende de la condición
value = 5.000000          // selecciona i (5), pero promovido a double
```

Ambos `sizeof` dan 8 porque el tipo de la expresión ternaria se determina en **compilación** por las conversiones aritméticas usuales (int + double → double), sin importar qué rama se seleccione en runtime.

</details>

---

### Ejercicio 3 — Ternarios anidados y asociatividad

```c
#include <stdio.h>

int main(void) {
    int n = 3;

    const char *s = (n == 1) ? "one"
                  : (n == 2) ? "two"
                  : (n == 3) ? "three"
                  :            "other";

    printf("n=%d -> \"%s\"\n", n, s);

    // Equivalente explícito:
    int a = 0, b = 10, c = 1, d = 20, e = 30;
    int r1 = a ? b : c ? d : e;
    int r2 = a ? b : (c ? d : e);

    printf("r1 = %d\n", r1);
    printf("r2 = %d\n", r2);

    return 0;
}
```

**¿`r1` y `r2` son iguales? ¿Qué valor tienen?**

<details>
<summary>Predicción</summary>

```
n=3 -> "three"
r1 = 20
r2 = 20
```

`r1` y `r2` son iguales porque el ternario asocia de **derecha a izquierda**. `a ? b : c ? d : e` se parsea como `a ? b : (c ? d : e)`.

Evaluación: `a=0` → false → evalúa `c ? d : e`. `c=1` → true → retorna `d=20`.

Si se parseara como `(a ? b : c) ? d : e`: `a=0` → retorna `c=1`, luego `1 ? d : e` → retorna `d=20`. Casualidad: mismo resultado. Pero con `a=1, b=0`: la diferencia aparece.

</details>

---

### Ejercicio 4 — Operador coma: valor de retorno

```c
#include <stdio.h>

int main(void) {
    int a = (10, 20, 30);
    int b, c;
    int d = (b = 5, c = 10, b + c);

    printf("a = %d\n", a);
    printf("d = %d, b = %d, c = %d\n", d, b, c);

    return 0;
}
```

**¿Qué valores tienen `a`, `b`, `c` y `d`?**

<details>
<summary>Predicción</summary>

```
a = 30               // (10, 20, 30) → retorna 30
d = 15, b = 5, c = 10   // (b=5, c=10, b+c) → b=5, c=10, retorna 15
```

La coma evalúa de izquierda a derecha y retorna el último valor. Los valores intermedios se descartan, pero los side effects (asignaciones a `b` y `c`) sí ocurren.

</details>

---

### Ejercicio 5 — Coma: precedencia vs asignación

```c
#include <stdio.h>

int main(void) {
    int x, y;

    x = 1, y = 2;
    printf("A: x=%d, y=%d\n", x, y);

    x = (1, y = 99);
    printf("B: x=%d, y=%d\n", x, y);

    x = (1, 2, 3);
    printf("C: x=%d\n", x);

    return 0;
}
```

**¿Cómo se parsea cada línea? ¿Qué valores resultan?**

<details>
<summary>Predicción</summary>

```
A: x=1, y=2
B: x=99, y=99
C: x=3
```

- **A:** `x = 1, y = 2` → asignación (nivel 14) tiene mayor precedencia que coma (nivel 15) → se parsea como `(x = 1), (y = 2)`. Dos asignaciones independientes.
- **B:** `x = (1, y = 99)` → los paréntesis fuerzan la coma como operador. Evalúa `1` (descarta), evalúa `y = 99` (y=99, retorna 99), asigna 99 a x.
- **C:** `x = (1, 2, 3)` → evalúa 1 (descarta), 2 (descarta), retorna 3. x=3.

</details>

---

### Ejercicio 6 — Coma en for loop

```c
#include <stdio.h>

int main(void) {
    printf("Countdown:\n");
    for (int i = 0, j = 9; i < j; i++, j--) {
        printf("  i=%d, j=%d\n", i, j);
    }

    return 0;
}
```

**¿Cuántas iteraciones? ¿Cuáles son los valores de `i` y `j` en cada una?**

<details>
<summary>Predicción</summary>

```
Countdown:
  i=0, j=9
  i=1, j=8
  i=2, j=7
  i=3, j=6
  i=4, j=5
```

5 iteraciones. La condición `i < j` se cumple mientras i < j. Cuando `i=5, j=4`, `5 < 4` es false y el loop termina.

Nota: `int i = 0, j = 9` usa la coma como **separador** de declaraciones. `i++, j--` usa la coma como **operador** — ambos incrementos se ejecutan en cada iteración.

</details>

---

### Ejercicio 7 — Coma como operador en argumento

```c
#include <stdio.h>

int add(int a, int b) {
    return a + b;
}

int main(void) {
    int x = 1, y = 2;

    int r1 = add(x, y);
    int r2 = add((x++, y), 100);

    printf("r1 = %d\n", r1);
    printf("r2 = %d, x = %d\n", r2, x);

    return 0;
}
```

**¿Cuántos argumentos recibe `add` en la segunda llamada? ¿Qué valor tiene `x` después?**

<details>
<summary>Predicción</summary>

```
r1 = 3             // add(1, 2) = 3
r2 = 102, x = 2    // add((x++, y), 100) → add(2, 100) = 102
```

`add` recibe **dos** argumentos. Los paréntesis `(x++, y)` convierten la coma en operador: evalúa `x++` (x pasa de 1 a 2, retorna 1, que se descarta), luego retorna `y` (2). Primer argumento = 2, segundo = 100. `add(2, 100) = 102`.

Sin paréntesis, `add(x++, y, 100)` serían 3 argumentos → error de compilación (add espera 2).

</details>

---

### Ejercicio 8 — Reescribir if/else como ternario

```c
#include <stdio.h>

int main(void) {
    int x = -3;

    // Reescribir como ternario:
    int sign;
    if (x > 0)
        sign = 1;
    else if (x < 0)
        sign = -1;
    else
        sign = 0;

    printf("sign(%d) = %d\n", x, sign);

    // Versión ternaria:
    int sign2 = (x > 0) ? 1 : (x < 0) ? -1 : 0;
    printf("sign2(%d) = %d\n", x, sign2);

    return 0;
}
```

**¿Ambos dan el mismo resultado? ¿Cómo se parsea el ternario anidado?**

<details>
<summary>Predicción</summary>

```
sign(-3) = -1
sign2(-3) = -1
```

Ambos dan -1. El ternario se parsea por asociatividad derecha como:

```
(x > 0) ? 1 : ((x < 0) ? -1 : 0)
```

Evaluación: `x > 0` → `-3 > 0` → false → evalúa `(x < 0) ? -1 : 0` → `-3 < 0` → true → retorna -1.

Es legible para este caso, pero para más ramas un `if/else` o `switch` sería mejor.

</details>

---

### Ejercicio 9 — Macro MIN con doble evaluación

```c
#include <stdio.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

int main(void) {
    int x = 3, y = 5;

    // Caso normal:
    printf("MIN(%d, %d) = %d\n", x, y, MIN(x, y));

    // Caso con side effects:
    x = 3; y = 5;
    int result = MIN(x++, y++);
    printf("MIN(x++, y++): result=%d, x=%d, y=%d\n", result, x, y);

    return 0;
}
```

**¿Qué valor tiene `result`? ¿Cuántas veces se incrementa `x`?**

<details>
<summary>Predicción</summary>

```
MIN(3, 5) = 3
MIN(x++, y++): result=4, x=5, y=6
```

La macro se expande a: `((x++) < (y++) ? (x++) : (y++))`.

Evaluación:
1. `x++ < y++` → `3 < 5` → true (x ahora es 4, y ahora es 6)
2. Como es true, evalúa `(x++)` → retorna 4 (x ahora es 5)
3. `y++` de la rama false NO se evalúa

Resultado: `result=4`, `x=5`, `y=6`. `x` se incrementó **dos veces** (una en la condición, una en la rama true). `y` se incrementó **una vez** (solo en la condición).

Esto es un bug: esperábamos MIN(3,5)=3, pero obtenemos 4. Las macros con ternario evalúan argumentos dos veces.

</details>

---

### Ejercicio 10 — Precedencia completa

```c
#include <stdio.h>

int main(void) {
    int a = 2, b = 3, c = 4;

    // Expresión 1:
    int r1 = a + b > c ? a : b;
    printf("r1 = %d\n", r1);

    // Expresión 2:
    int x = 0, y = 0;
    int r2 = (x = 5, y = 10, x + y);
    printf("r2 = %d\n", r2);

    // Expresión 3:
    int p, q;
    p = 1, q = p + 1;
    printf("p = %d, q = %d\n", p, q);

    // Expresión 4:
    int val = 8;
    int flags = 0x0F;
    int r3 = (flags & 0x08) ? val * 2 : val + 1;
    printf("r3 = %d\n", r3);

    return 0;
}
```

**Parsear cada expresión y predecir el resultado.**

<details>
<summary>Predicción</summary>

```
r1 = 2
r2 = 15
p = 1, q = 2
r3 = 16
```

Desglose:

- **r1:** `a + b > c ? a : b` → `+` (nivel 4) > `>` (nivel 6) > `?:` (nivel 13). Se parsea como `((a + b) > c) ? a : b` = `(5 > 4) ? 2 : 3` = `true ? 2 : 3` = **2**.

- **r2:** `(x = 5, y = 10, x + y)` → evalúa x=5, y=10, retorna x+y = **15**.

- **p, q:** `p = 1, q = p + 1` → asignación (14) > coma (15). Se parsea como `(p = 1), (q = p + 1)`. Primero p=1, luego q = 1 + 1 = **2**.

- **r3:** `(flags & 0x08)` = `0x0F & 0x08` = `0x08` (true). Rama true: `val * 2` = `8 * 2` = **16**.

</details>
