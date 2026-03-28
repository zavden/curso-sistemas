# T02 — Loops

> **Fuentes**: `README.md`, `LABS.md`, `labs/README.md`, 8 archivos `.c` de laboratorio.
> Aplicada **Regla 3** (no existe `.max.md`).

---

## Erratas detectadas

| Archivo | Línea(s) | Error | Corrección |
|---------|----------|-------|------------|
| `labs/input_validation.c` | 4, 24 | `int number;` sin inicializar. Si `scanf` falla en la primera iteración, `continue` salta a `while (number < 1 \|\| number > 10)` donde `number` se lee sin haber sido escrito — **undefined behavior**. | Inicializar: `int number = 0;`. Así la condición del `while` evalúa un valor determinístico. |

---

## 1. El loop `for`

### Estructura y flujo de ejecución

```c
for (inicialización; condición; actualización) {
    cuerpo;
}
```

Orden de ejecución:
1. **Inicialización** — se ejecuta **una sola vez** antes de todo.
2. **Condición** — se evalúa **antes** de cada iteración. Si es falsa, el loop termina.
3. **Cuerpo** — se ejecuta si la condición fue verdadera.
4. **Actualización** — se ejecuta **después** del cuerpo (incluso con `continue`).
5. Volver al paso 2.

```c
for (int i = 0; i < 5; i++) {
    printf("%d ", i);     // 0 1 2 3 4
}
// Al salir: i ya no existe (scope limitado al for en C99+)
```

Punto clave: la **actualización siempre se ejecuta**, incluso cuando se usa `continue`.
Esto lo diferencia de `while`, donde el incremento manual puede saltarse.

### Variaciones comunes

```c
// Conteo descendente:
for (int i = 9; i >= 0; i--) { ... }

// Paso diferente:
for (int i = 0; i < 100; i += 10) { ... }   // 0, 10, 20, ..., 90

// Dos variables (operador coma):
for (int lo = 0, hi = 9; lo < hi; lo++, hi--) {
    printf("lo=%d hi=%d\n", lo, hi);
}
// 5 iteraciones: lo=0/hi=9, lo=1/hi=8, ..., lo=4/hi=5
// Cuando lo=5 y hi=4, la condición lo < hi es falsa

// Iterar sobre un array:
int arr[] = {10, 20, 30, 40, 50};
size_t len = sizeof(arr) / sizeof(arr[0]);
for (size_t i = 0; i < len; i++) {
    printf("%d\n", arr[i]);
}

// Iterar sobre un string (con índice):
for (int i = 0; str[i] != '\0'; i++) {
    printf("%c", str[i]);
}

// Iterar sobre un string (con puntero — más idiomático):
for (const char *p = str; *p != '\0'; p++) {
    printf("%c", *p);
}
```

### Partes opcionales

Las tres secciones del `for` son independientes y cada una es opcional:

```c
// Sin inicialización (variable declarada fuera):
int i = 10;
for (; i < 15; i++) { ... }         // i accesible después del loop

// Sin actualización (incremento en el cuerpo):
for (int j = 0; j < 10;) {
    printf("%d ", j);
    j += 3;                          // 0, 3, 6, 9
}

// Sin condición = loop infinito:
for (;;) {
    if (should_stop) break;          // equivale a while(1)
}
```

Omitir la condición la hace implícitamente `true`. `for (;;)` es la forma idiomática
de loop infinito en muchos estilos de C.

---

## 2. `while` y `do-while`

### `while` — cero o más ejecuciones

```c
while (condición) {
    cuerpo;
}
```

La condición se evalúa **antes** de la primera iteración. Si es falsa desde el inicio,
el cuerpo **nunca se ejecuta**:

```c
int x = 100;
while (x < 10) {
    printf("%d\n", x);   // nunca se ejecuta
}
```

### `do-while` — una o más ejecuciones

```c
do {
    cuerpo;
} while (condición);    // nota el ; obligatorio
```

El cuerpo se ejecuta **al menos una vez**. La condición se evalúa **después**:

```c
int y = 100;
do {
    printf("%d\n", y);   // se ejecuta una vez (imprime 100)
} while (y < 10);
```

### Cuándo usar cada uno

| Loop | Usar cuando... | Ejemplo |
|------|---------------|---------|
| `for` | Sabes cuántas iteraciones, o tienes init/condición/update claros | Recorrer array, contar de A a B |
| `while` | No sabes cuántas iteraciones, condición depende de algo externo | Leer hasta EOF, buscar en lista |
| `do-while` | Necesitas ejecutar al menos una vez antes de verificar | Validar input, menú interactivo |

```c
// for: número conocido de iteraciones
for (int i = 0; i < n; i++) { ... }

// while: condición externa
while (fgets(line, sizeof(line), f)) { ... }       // hasta EOF
while (current != NULL) { current = current->next; } // lista enlazada

// do-while: al menos una vez
int choice;
do {
    printf("1. Option A\n2. Option B\n0. Exit\n");
    scanf("%d", &choice);
    // procesar choice...
} while (choice != 0);
```

---

## 3. `do-while` en macros

El uso más importante de `do { } while (0)` no es como loop, sino como wrapper
para macros multi-statement:

```c
#define LOG(msg) do { \
    fprintf(stderr, "[%s:%d] ", __FILE__, __LINE__); \
    fprintf(stderr, "%s\n", msg); \
} while (0)
```

**Por qué es necesario**: sin `do-while(0)`, las llaves sueltas rompen `if/else`:

```c
#define LOG_BAD(msg) { fprintf(stderr, msg); fprintf(stderr, "\n"); }

if (error)
    LOG_BAD("fail");    // se expande a { ... };
else                    // ERROR: el ; extra cierra el if, else queda huérfano
    LOG_BAD("ok");
```

Con `do { } while (0)`:

```c
if (error)
    LOG("fail");        // se expande a do { ... } while(0);
else                    // OK: el ; termina el do-while limpiamente
    LOG("ok");
```

La expresión `while(0)` garantiza que el cuerpo se ejecuta exactamente una vez
(no es un loop, es un wrapper sintáctico). El compilador optimiza el `while(0)` por
completo — no genera ninguna instrucción de branch.

---

## 4. `break` y `continue`

### `break` — salir del loop

`break` termina inmediatamente el loop **más interno** que lo contiene:

```c
for (int i = 0; i < 100; i++) {
    if (arr[i] == target) {
        printf("Found at %d\n", i);
        break;                        // sale del for, NO continúa
    }
}
// la ejecución continúa aquí después del break
```

`break` también funciona en `while`, `do-while`, y `switch`. Pero **no sale de un `if`** —
sale del loop o switch que lo contiene.

### `continue` — saltar al siguiente ciclo

`continue` salta el resto del cuerpo y va directamente al **siguiente ciclo**:

```c
for (int i = 0; i < 10; i++) {
    if (i % 2 == 0) continue;    // salta los pares
    printf("%d ", i);             // solo impares: 1 3 5 7 9
}
```

Diferencia crucial entre `for` y `while`:

- En `for`: `continue` salta al **paso de actualización** (`i++`), que **siempre se ejecuta**.
- En `while`: `continue` salta a la **evaluación de la condición**. Si el incremento
  está después del `continue`, **no se ejecuta**.

```c
// PELIGRO: loop infinito con while + continue
int j = 0;
while (j < 10) {
    if (j == 5) {
        continue;    // j++ nunca se ejecuta cuando j == 5
    }                // j queda en 5 para siempre → loop infinito
    j++;
}

// CORRECTO: poner el incremento ANTES del continue
int j = 0;
while (j < 10) {
    j++;
    if (j == 5) {
        continue;    // OK: j ya se incrementó
    }
    printf("%d ", j);
}

// O simplemente usar for (el incremento siempre se ejecuta):
for (int j = 0; j < 10; j++) {
    if (j == 5) continue;    // j++ se ejecuta siempre
    printf("%d ", j);
}
```

---

## 5. `break` en loops anidados

`break` solo sale del loop **más interno**. El loop externo sigue ejecutándose:

```c
for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
        if (matrix[i][j] == target) {
            printf("Found at [%d][%d]\n", i, j);
            break;     // sale del for de j, NO del for de i
        }
    }
    // el for de i continúa aquí normalmente
}
```

### Soluciones para salir de múltiples loops

**1. Variable flag:**

```c
int found = 0;
for (int i = 0; i < rows && !found; i++) {
    for (int j = 0; j < cols && !found; j++) {
        if (matrix[i][j] == target) {
            printf("Found at [%d][%d]\n", i, j);
            found = 1;   // ambas condiciones de loop se vuelven falsas
        }
    }
}
```

**2. `goto` (a veces es la solución más limpia):**

```c
for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
        if (matrix[i][j] == target) {
            printf("Found at [%d][%d]\n", i, j);
            goto done;    // sale de ambos loops directamente
        }
    }
}
done:
// continúa aquí
```

**3. Extraer a una función (return sale de todo):**

```c
int find_in_matrix(int matrix[][COLS], int rows, int cols, int target) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (matrix[i][j] == target) return 1;
        }
    }
    return 0;
}
```

---

## 6. Patrones comunes con loops

### Recorrer un string

Un string en C termina en `'\0'`. El loop continúa mientras el carácter actual
no sea el terminador nulo:

```c
const char *msg = "Hello, World! 123";

// Con índice:
for (int i = 0; msg[i] != '\0'; i++) {
    putchar(msg[i]);
}

// Con puntero (más idiomático en C):
for (const char *p = msg; *p != '\0'; p++) {
    putchar(*p);
}

// Contar tipos de caracteres (ctype.h):
int letters = 0, digits = 0;
for (const char *p = msg; *p; p++) {   // *p es equivalente a *p != '\0'
    if (isalpha((unsigned char)*p)) letters++;
    else if (isdigit((unsigned char)*p)) digits++;
}
```

Nota: el cast a `(unsigned char)` en `isalpha`/`isdigit` es necesario porque estas
funciones esperan un `unsigned char` o `EOF`. Un `char` con signo y valor negativo
sería UB sin el cast.

### Búsqueda con sentinel value

El patrón estándar para "encontrar o reportar que no existe":

```c
int find(const int *arr, int n, int target) {
    for (int i = 0; i < n; i++) {
        if (arr[i] == target) {
            return i;       // encontrado: retorna el índice
        }
    }
    return -1;              // sentinel: -1 no es un índice válido
}

// Uso:
int pos = find(data, len, 42);
if (pos == -1) {
    printf("Not found\n");
} else {
    printf("Found at index %d\n", pos);
}
```

### Validación de input con do-while

```c
int number = 0;   // inicializar para evitar UB si scanf falla
int attempts = 0;

do {
    printf("Enter a number (1-10): ");
    if (scanf("%d", &number) != 1) {
        // Limpiar input inválido del buffer
        int c;
        while ((c = getchar()) != '\n' && c != EOF) { /* discard */ }
        printf("Invalid input. Try again.\n");
        attempts++;
        continue;   // vuelve al while(condición)
    }
    attempts++;
    if (number < 1 || number > 10) {
        printf("Out of range. Try again.\n");
    }
} while (number < 1 || number > 10);
```

Componentes del patrón:
- `do-while` porque necesitas pedir el dato **al menos una vez**.
- Verificar el retorno de `scanf` (devuelve el número de items leídos, 0 si no matchea).
- Limpiar el buffer con `getchar()` en loop cuando `scanf` falla — los caracteres
  inválidos quedan en stdin y causarían un loop infinito si no se descartan.

### Iterar sobre lista enlazada

```c
// Con while (más natural: no sabes cuántos nodos hay):
struct Node *p = head;
while (p != NULL) {
    process(p->data);
    p = p->next;
}

// Con for (más compacto):
for (struct Node *p = head; p != NULL; p = p->next) {
    process(p->data);
}
```

### Doble iteración (two pointers)

```c
// Invertir un array in-place:
for (int left = 0, right = n - 1; left < right; left++, right--) {
    int tmp = arr[left];
    arr[left] = arr[right];
    arr[right] = tmp;
}
```

---

## 7. Loops infinitos

Tres formas idiomáticas:

```c
for (;;) { ... }      // preferida en muchos estilos (Linux kernel, etc.)
while (1) { ... }     // también muy común
do { ... } while (1); // rara vez usada
```

`for (;;)` y `while (1)` son equivalentes. Algunos prefieren `for (;;)` porque
es explícitamente "sin condición" y no genera warnings de "condición siempre verdadera"
en algunos compiladores.

Uso típico: event loops, servidores, lectura continua de input:

```c
for (;;) {
    int event = get_next_event();
    if (event == EVENT_QUIT) break;
    handle_event(event);
}
```

---

## Ejercicios

### Ejercicio 1 — Tres loops, mismo resultado

```c
// Imprime los números del 1 al 20 que son divisibles por 3,
// usando cada tipo de loop: for, while, do-while.
// ¿Cuál es más natural para este problema?

#include <stdio.h>

int main(void) {
    // Versión for:
    printf("for:      ");
    for (int i = 1; i <= 20; i++) {
        if (i % 3 == 0) printf("%d ", i);
    }
    printf("\n");

    // Versión while:
    printf("while:    ");
    int j = 1;
    while (j <= 20) {
        if (j % 3 == 0) printf("%d ", j);
        j++;
    }
    printf("\n");

    // Versión do-while:
    printf("do-while: ");
    int k = 1;
    do {
        if (k % 3 == 0) printf("%d ", k);
        k++;
    } while (k <= 20);
    printf("\n");

    return 0;
}
```

<details><summary>Predicción</summary>

Salida:
```
for:      3 6 9 12 15 18
while:    3 6 9 12 15 18
do-while: 3 6 9 12 15 18
```

Las tres versiones producen el mismo resultado. El `for` es el más natural porque
tienes inicio (1), fin (20) y paso (+1) claros. El `while` y `do-while` requieren
declarar e incrementar la variable manualmente. Para este caso, `do-while` no aporta
nada sobre `while` porque sabemos que la condición es verdadera desde el inicio (1 <= 20).

</details>

---

### Ejercicio 2 — Tabla de multiplicar

```c
// Imprime la tabla de multiplicar del 7 (del 1 al 10).
// Usa for. Formato: "7 x 1 = 7", "7 x 2 = 14", etc.
// ¿Cuántas líneas imprime? ¿Cuál es el último resultado?

#include <stdio.h>

int main(void) {
    for (int i = 1; i <= 10; i++) {
        printf("7 x %2d = %2d\n", i, 7 * i);
    }
    return 0;
}
```

<details><summary>Predicción</summary>

```
7 x  1 =  7
7 x  2 = 14
7 x  3 = 21
7 x  4 = 28
7 x  5 = 35
7 x  6 = 42
7 x  7 = 49
7 x  8 = 56
7 x  9 = 63
7 x 10 = 70
```

10 líneas. El último resultado es 70 (7 × 10).

</details>

---

### Ejercicio 3 — Sumar hasta que el usuario diga basta

```c
// Pide números al usuario y los suma. Termina cuando el usuario
// ingresa 0. Imprime la suma total y cuántos números ingresó.
// ¿Qué tipo de loop es el más adecuado?

#include <stdio.h>

int main(void) {
    int sum = 0;
    int count = 0;
    int num;

    printf("Enter numbers (0 to stop):\n");
    do {
        printf("> ");
        if (scanf("%d", &num) != 1) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF) {}
            printf("Invalid input, try again.\n");
            continue;
        }
        if (num != 0) {
            sum += num;
            count++;
        }
    } while (num != 0);

    printf("Sum: %d (%d numbers)\n", sum, count);
    return 0;
}
```

<details><summary>Predicción</summary>

Si el usuario ingresa 5, 10, -3, 0:
```
Enter numbers (0 to stop):
> 5
> 10
> -3
> 0
Sum: 12 (3 numbers)
```

`do-while` es adecuado porque necesitas pedir al menos un número. También funcionaría
un `while(1)` con `break` cuando `num == 0`. Nota: `num` se inicializa por `scanf`
antes de evaluarse en el `while`, así que no hay UB — salvo si `scanf` falla en la
primera llamada y `num` no fue escrito, pero el `continue` vuelve a pedir sin llegar
al check de `num != 0`.

Espera — hay un problema sutil: si `scanf` falla en la primera iteración, `continue` salta
a `while (num != 0)` donde `num` no fue inicializado. Solución: declarar `int num = 0;`.

</details>

---

### Ejercicio 4 — Factorial con while

```c
// Calcula el factorial de n usando while.
// Prueba con n = 0, 1, 5, 12.
// ¿Cuál es el factorial de 0? ¿Y el de 12?

#include <stdio.h>

int main(void) {
    int tests[] = {0, 1, 5, 12};
    int n_tests = (int)(sizeof(tests) / sizeof(tests[0]));

    for (int t = 0; t < n_tests; t++) {
        int n = tests[t];
        long long factorial = 1;
        int i = n;
        while (i > 1) {
            factorial *= i;
            i--;
        }
        printf("%d! = %lld\n", n, factorial);
    }
    return 0;
}
```

<details><summary>Predicción</summary>

```
0! = 1
1! = 1
5! = 120
12! = 479001600
```

- `0!` = 1 por definición (el while no ejecuta ninguna iteración porque `0 > 1` es falso).
- `1!` = 1 (el while no ejecuta porque `1 > 1` es falso).
- `5!` = 5 × 4 × 3 × 2 = 120.
- `12!` = 479001600 (cabe en un `long long` sin problema; incluso cabe en un `int` de 32 bits, ya que 2^31 - 1 = 2147483647).
- `13!` = 6227020800 ya **no** cabría en un `int` de 32 bits.

</details>

---

### Ejercicio 5 — continue para filtrar caracteres

```c
// Recorre un string e imprime solo las letras mayúsculas,
// saltando todo lo demás con continue.
// String: "Hello World! ABC 123 xyz"

#include <stdio.h>
#include <ctype.h>

int main(void) {
    const char *msg = "Hello World! ABC 123 xyz";

    printf("Original: \"%s\"\n", msg);
    printf("Uppercase only: ");
    for (const char *p = msg; *p != '\0'; p++) {
        if (!isupper((unsigned char)*p)) {
            continue;
        }
        putchar(*p);
    }
    printf("\n");
    return 0;
}
```

<details><summary>Predicción</summary>

```
Original: "Hello World! ABC 123 xyz"
Uppercase only: HWABC
```

Las letras mayúsculas son: H (de Hello), W (de World), A, B, C (de ABC).
Las letras de "xyz" son minúsculas → se saltan. Los dígitos, espacios y signos
también se saltan.

</details>

---

### Ejercicio 6 — break en búsqueda con puntero

```c
// Busca la primera vocal en un string usando un puntero.
// Si la encuentra, imprime cuál es y en qué posición.
// Si no, imprime "No vowel found".

#include <stdio.h>
#include <ctype.h>

int main(void) {
    const char *tests[] = {"rhythm", "Hello", "bcdfg", "AEIOUxyz"};
    int n = (int)(sizeof(tests) / sizeof(tests[0]));

    for (int t = 0; t < n; t++) {
        const char *s = tests[t];
        const char *p = s;
        int found = 0;

        while (*p != '\0') {
            char lower = (char)tolower((unsigned char)*p);
            if (lower == 'a' || lower == 'e' || lower == 'i' ||
                lower == 'o' || lower == 'u') {
                printf("\"%s\": first vowel '%c' at position %d\n",
                       s, *p, (int)(p - s));
                found = 1;
                break;
            }
            p++;
        }
        if (!found) {
            printf("\"%s\": no vowel found\n", s);
        }
    }
    return 0;
}
```

<details><summary>Predicción</summary>

```
"rhythm": first vowel 'y' at position ...
```

Espera — 'y' no es una vocal estándar en este código (solo se verifican a, e, i, o, u).
Así que "rhythm" no tiene vocales según este programa.

```
"rhythm": no vowel found
"Hello": first vowel 'e' at position 1
"bcdfg": no vowel found
"AEIOUxyz": first vowel 'A' at position 0
```

- "rhythm" — contiene r, h, y, t, h, m. Ninguna es a/e/i/o/u → "no vowel found".
- "Hello" — H(no), e(sí) → primera vocal 'e' en posición 1.
- "bcdfg" — ninguna vocal → "no vowel found".
- "AEIOUxyz" — A es vocal inmediatamente → posición 0, carácter 'A' (mayúscula).

Nota: `p - s` calcula la distancia en bytes entre dos punteros al mismo array, lo cual
da el índice del carácter encontrado.

</details>

---

### Ejercicio 7 — Loop infinito con break condicional

```c
// Simula un "servidor" que procesa eventos.
// Genera eventos aleatorios del 1 al 10. Si el evento es 7, termina.
// Cuenta cuántos eventos procesó antes de terminar.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void) {
    srand((unsigned)time(NULL));
    int count = 0;

    printf("Event loop started...\n");
    for (;;) {
        int event = rand() % 10 + 1;   // 1 a 10
        count++;
        printf("Event #%d: %d", count, event);

        if (event == 7) {
            printf(" <- QUIT signal!\n");
            break;
        }
        printf(" (processed)\n");
    }
    printf("Processed %d events before quit.\n", count);
    return 0;
}
```

<details><summary>Predicción</summary>

La salida es no determinística (depende de `rand()`), pero la estructura es:
```
Event loop started...
Event #1: 3 (processed)
Event #2: 9 (processed)
Event #3: 7 <- QUIT signal!
Processed 3 events before quit.
```

El número de iteraciones varía cada ejecución. Estadísticamente, se espera ~10
iteraciones en promedio (probabilidad 1/10 de obtener 7 en cada intento).
El `for (;;)` es un loop infinito que solo termina con `break`.

</details>

---

### Ejercicio 8 — Loops anidados: triángulo de asteriscos

```c
// Imprime un triángulo rectángulo de 5 filas:
// *
// **
// ***
// ****
// *****
// Luego imprime un triángulo invertido.

#include <stdio.h>

int main(void) {
    int rows = 5;

    printf("--- Triangle ---\n");
    for (int i = 1; i <= rows; i++) {
        for (int j = 0; j < i; j++) {
            putchar('*');
        }
        putchar('\n');
    }

    printf("\n--- Inverted triangle ---\n");
    for (int i = rows; i >= 1; i--) {
        for (int j = 0; j < i; j++) {
            putchar('*');
        }
        putchar('\n');
    }

    return 0;
}
```

<details><summary>Predicción</summary>

```
--- Triangle ---
*
**
***
****
*****

--- Inverted triangle ---
*****
****
***
**
*
```

El loop externo controla las filas. El interno imprime `i` asteriscos por fila.
En el triángulo normal, `i` va de 1 a 5. En el invertido, de 5 a 1.

Conteo de iteraciones del loop interno:
- Triángulo normal: 1 + 2 + 3 + 4 + 5 = 15 asteriscos.
- Triángulo invertido: 5 + 4 + 3 + 2 + 1 = 15 asteriscos.

</details>

---

### Ejercicio 9 — break en loops anidados con flag

```c
// Busca un valor en una matriz 4x4. Usa una flag para salir de
// ambos loops cuando lo encuentre. Prueba con un valor que existe
// y uno que no.

#include <stdio.h>

int main(void) {
    int matrix[4][4] = {
        { 1,  2,  3,  4},
        { 5,  6,  7,  8},
        { 9, 10, 11, 12},
        {13, 14, 15, 16}
    };
    int rows = 4, cols = 4;
    int targets[] = {11, 99};

    for (int t = 0; t < 2; t++) {
        int target = targets[t];
        int found = 0;

        for (int i = 0; i < rows && !found; i++) {
            for (int j = 0; j < cols && !found; j++) {
                if (matrix[i][j] == target) {
                    printf("find(%d): found at [%d][%d]\n", target, i, j);
                    found = 1;
                }
            }
        }
        if (!found) {
            printf("find(%d): not found\n", target);
        }
    }
    return 0;
}
```

<details><summary>Predicción</summary>

```
find(11): found at [2][2]
find(99): not found
```

- 11 está en la fila 2 (tercera fila), columna 2 (tercera columna) → `[2][2]`.
  `matrix[2][2]` = 11. La flag `found = 1` termina ambos loops.
- 99 no está en la matriz. Ambos loops completan todas las iteraciones (4 × 4 = 16
  comparaciones) sin encontrarlo.

</details>

---

### Ejercicio 10 — continue trampa en while vs for

```c
// Demuestra la diferencia entre continue en for vs while.
// Intenta imprimir los números del 0 al 9, saltando el 5.
// Primero con for (funciona), luego con while (cuidado con
// dónde pones el incremento).

#include <stdio.h>

int main(void) {
    printf("for (correct):\n");
    for (int i = 0; i < 10; i++) {
        if (i == 5) continue;
        printf("%d ", i);
    }
    printf("\n");

    printf("\nwhile (correct — increment before continue):\n");
    int j = -1;
    while (j < 9) {
        j++;
        if (j == 5) continue;
        printf("%d ", j);
    }
    printf("\n");

    // Versión INCORRECTA (no la ejecutes, es loop infinito):
    // int k = 0;
    // while (k < 10) {
    //     if (k == 5) continue;   // k++ nunca se ejecuta → k queda en 5
    //     printf("%d ", k);
    //     k++;
    // }

    printf("\nNotice: in for, i++ always runs after continue.\n");
    printf("In while, if j++ is after continue, it gets skipped.\n");
    return 0;
}
```

<details><summary>Predicción</summary>

```
for (correct):
0 1 2 3 4 6 7 8 9

while (correct — increment before continue):
0 1 2 3 4 6 7 8 9

Notice: in for, i++ always runs after continue.
In while, if j++ is after continue, it gets skipped.
```

Ambas versiones imprimen 0-9 sin el 5. La diferencia:
- En `for`: `continue` salta al paso de actualización `i++`, que **siempre** se ejecuta.
  Es imposible causar un loop infinito con `continue` en un `for` convencional.
- En `while`: el incremento `j++` debe ir **antes** del `continue`. Si fuera después,
  cuando `j == 5`, el `continue` saltaría `j++` y `j` quedaría en 5 para siempre.

La versión while usa `j = -1` y empieza con `j++` para que el primer valor procesado
sea 0, equivalente al `for (int i = 0; ...)`.

</details>
