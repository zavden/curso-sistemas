# T04 — Operador coma y ternario

## Operador ternario (?:)

El operador ternario es la única expresión condicional de C.
Evalúa una condición y retorna uno de dos valores:

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

```c
// El ternario es una EXPRESIÓN, no un statement.
// Puede usarse donde se espera un valor:

printf("x es %s\n", (x > 0) ? "positivo" : "no positivo");

int abs_val = (x >= 0) ? x : -x;

// En inicialización:
const int mode = (debug) ? MODE_DEBUG : MODE_RELEASE;
// No se puede hacer esto con if/else y const
```

### Usos legítimos

```c
// 1. Asignación condicional simple:
int min = (a < b) ? a : b;

// 2. Argumentos condicionales:
printf("Found %d item%s\n", n, (n == 1) ? "" : "s");

// 3. Inicialización de const:
const char *label = (error) ? "FAIL" : "OK";

// 4. Retorno condicional:
return (x >= 0) ? x : -x;

// 5. Macros simples:
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ABS(x)    ((x) >= 0 ? (x) : -(x))
// CUIDADO: estas macros evalúan los argumentos dos veces
// MIN(i++, j++) tiene side effects duplicados
```

### Cuándo NO usar el ternario

```c
// NO anidar ternarios — se vuelve ilegible:

// Difícil de leer:
int result = (a > b) ? (a > c ? a : c) : (b > c ? b : c);

// Mucho mejor:
int result;
if (a > b) {
    result = (a > c) ? a : c;
} else {
    result = (b > c) ? b : c;
}

// O mejor aún:
int result = a;
if (b > result) result = b;
if (c > result) result = c;
```

```c
// NO usar ternario para ejecutar acciones (usar if/else):

// Mal estilo:
(condition) ? do_something() : do_other();

// Bien:
if (condition) {
    do_something();
} else {
    do_other();
}
```

### Tipo del resultado

```c
// El tipo del resultado sigue las conversiones aritméticas usuales:
int i = 5;
double d = 3.14;
// (condition) ? i : d → el resultado es double (i se promueve)

// Con punteros:
int *p = (condition) ? &x : NULL;
// Ambas ramas deben ser tipos compatibles
```

### Asociatividad

```c
// El ternario asocia de DERECHA a IZQUIERDA:
a ? b : c ? d : e
// se parsea como:
a ? b : (c ? d : e)
// NO como:
(a ? b : c) ? d : e

// Esto permite encadenar (aunque no es recomendable):
const char *s = (n == 1) ? "one"
              : (n == 2) ? "two"
              : (n == 3) ? "three"
              :            "other";
// Funciona pero un switch es más claro.
```

## Operador coma (,)

El operador coma evalúa dos expresiones de izquierda a
derecha y retorna el valor de la **derecha**:

```c
int x = (1, 2, 3);    // x = 3
// Evalúa 1 (descarta), evalúa 2 (descarta), evalúa 3 (retorna)
```

### Coma como operador vs coma como separador

```c
// La coma tiene DOS roles en C:

// 1. SEPARADOR (no es un operador):
int a, b, c;                    // separa declaraciones
foo(1, 2, 3);                   // separa argumentos
int arr[] = {1, 2, 3};          // separa inicializadores

// 2. OPERADOR (evalúa de izquierda a derecha, retorna el derecho):
int x = (a = 1, b = 2, a + b); // x = 3

// Para usar la coma como operador en contextos donde
// es separador, se necesitan paréntesis:
foo((a++, b));    // incrementa a, pasa b como argumento
// Sin paréntesis: foo(a++, b) — dos argumentos
```

### Usos legítimos del operador coma

```c
// 1. En for — múltiples variables o incrementos:
for (int i = 0, j = 10; i < j; i++, j--) {
    // i++ y j-- se ejecutan ambos en cada iteración
    // La coma aquí es OPERADOR (no separador de declaración)
    // (la coma en "int i = 0, j = 10" SÍ es separador)
}

// 2. En macros, para evaluar múltiples expresiones:
#define SWAP(a, b, type) \
    ((type) _tmp = (a), (a) = (b), (b) = _tmp)
// (Mejor con do { } while(0) o statement expression)

// 3. En condiciones del while (raro pero válido):
while (c = getchar(), c != EOF && c != '\n') {
    // lee un char y verifica que no es EOF ni newline
}
```

### Cuándo NO usar el operador coma

```c
// En la mayoría de los casos, el operador coma hace el código
// menos legible. Preferir statements separados:

// Mal estilo:
x = 1, y = 2, z = 3;

// Bien:
x = 1;
y = 2;
z = 3;

// El único uso realmente útil es en for loops con múltiples
// variables y en macros donde necesitas una sola expresión.
```

### Precedencia

```c
// La coma tiene la MENOR precedencia de todos los operadores:
int x = 1, 2;     // ERROR: se parsea como (int x = 1), (2);
                   // La coma aquí es SEPARADOR de declaraciones
                   // y 2 es un statement inválido

int x = (1, 2);   // OK: x = 2 (coma como operador)

// La asignación tiene mayor precedencia que la coma:
x = 1, y = 2;     // (x = 1), (y = 2) — dos asignaciones
x = (1, y = 2);   // x = 2 (la coma retorna y=2, que es 2)
```

## Tabla de precedencia (actualizada)

```
Precedencia  Operador                     Nota
──────────────────────────────────────────────────
1 (máxima)   () [] -> . postfix ++ --
2            prefix ++ -- + - ! ~ (type) * & sizeof
3            * / %
4            + -
5            << >>
6            < <= > >=
7            == !=
8            &                            bitwise AND
9            ^                            bitwise XOR
10           |                            bitwise OR
11           &&                           lógico AND
12           ||                           lógico OR
13           ?:                           ternario
14           = += -= *= /= %= etc.        asignación
15 (mínima)  ,                            coma
```

---

## Ejercicios

### Ejercicio 1 — Ternario

```c
// Reescribir estos if/else como expresiones ternarias:

// a)
int sign;
if (x > 0)
    sign = 1;
else if (x < 0)
    sign = -1;
else
    sign = 0;

// b)
const char *msg;
if (score >= 90)
    msg = "A";
else if (score >= 80)
    msg = "B";
else
    msg = "C";
```

### Ejercicio 2 — Operador coma en for

```c
// Escribir un for loop que use el operador coma para:
// - Inicializar i=0 y j=9 (i cuenta de 0 a 9, j de 9 a 0)
// - Incrementar i y decrementar j en cada iteración
// - Imprimir ambos valores
```

### Ejercicio 3 — MIN/MAX seguros

```c
// La macro #define MIN(a,b) ((a)<(b)?(a):(b)) evalúa
// los argumentos dos veces.
// Mostrar un ejemplo donde MIN(i++, j++) da resultado incorrecto.
// ¿Cómo se soluciona en C11? (pista: _Generic no ayuda aquí,
// pero statement expressions + typeof sí — extensión GNU)
```
