# T01 — Aritméticos y de asignación

## Operadores aritméticos

```c
int a = 17, b = 5;

a + b     // 22   suma
a - b     // 12   resta
a * b     // 85   multiplicación
a / b     // 3    división entera (trunca hacia cero)
a % b     // 2    módulo (resto de la división)

-a        // -17  negación unaria
+a        // 17   plus unario (casi nunca se usa)
```

### División entera

```c
// La división entre enteros TRUNCA hacia cero (no redondea):
 17 / 5   //  3   (no 3.4, no 4)
-17 / 5   // -3   (no -4, trunca hacia cero)
 17 / -5  // -3
-17 / -5  //  3

// Si quieres división real, al menos un operando debe ser double:
17.0 / 5    // 3.4
(double)17 / 5  // 3.4
17 / 5.0    // 3.4

// División por cero:
17 / 0      // UB para enteros (señal SIGFPE en la mayoría de sistemas)
17.0 / 0.0  // inf (IEEE 754, no es UB)
```

### Módulo (%)

```c
// El signo del resultado de % sigue al dividendo (desde C99):
 17 %  5   //  2
-17 %  5   // -2
 17 % -5   //  2
-17 % -5   // -2

// Siempre se cumple: (a/b)*b + a%b == a

// Usos comunes:
n % 2 == 0          // ¿n es par?
n % 10              // último dígito
index % ARRAY_SIZE  // índice circular (wrap around)
```

## Operadores de asignación

```c
int x = 10;

x = 20;       // asignación simple

// Asignación compuesta — operación + asignación en uno:
x += 5;       // x = x + 5   → 25
x -= 3;       // x = x - 3   → 22
x *= 2;       // x = x * 2   → 44
x /= 4;       // x = x / 4   → 11
x %= 3;       // x = x % 3   → 2

// También existen para bitwise:
x &= 0xFF;   // x = x & 0xFF
x |= 0x01;   // x = x | 0x01
x ^= mask;   // x = x ^ mask
x <<= 2;     // x = x << 2
x >>= 1;     // x = x >> 1
```

```c
// La asignación es una EXPRESIÓN que retorna el valor asignado:
int a, b, c;
a = b = c = 0;     // c=0 retorna 0, b=0 retorna 0, a=0

// Esto permite patrones como:
int n;
while ((n = read_input()) > 0) {
    process(n);
}
// Asigna a n y compara en una sola expresión
```

## Incremento y decremento

```c
int x = 5;

// Prefijo — incrementa ANTES de usar el valor:
int a = ++x;    // x se incrementa a 6, a recibe 6

// Postfijo — incrementa DESPUÉS de usar el valor:
x = 5;
int b = x++;    // b recibe 5, luego x se incrementa a 6

// Lo mismo para decremento:
x = 5;
int c = --x;    // x = 4, c = 4
x = 5;
int d = x--;    // d = 5, x = 4
```

```c
// En statements independientes, no hay diferencia:
x++;    // incrementa x
++x;    // incrementa x (mismo efecto)
// Ambos son equivalentes cuando no se usa el valor resultante.

// NUNCA modificar la misma variable dos veces sin sequence point:
int i = 0;
int bad = i++ + i++;    // UB: modifica i dos veces
int bad2 = ++i + ++i;   // UB
```

## Precedencia y asociatividad

La precedencia determina qué operador se evalúa primero.
La asociatividad determina el orden cuando hay operadores
de la misma precedencia:

```c
// Precedencia (de mayor a menor para los aritméticos):
// 1. ++ -- (postfijo)     — izquierda a derecha
// 2. ++ -- + - (prefijo)  — derecha a izquierda
// 3. * / %                — izquierda a derecha
// 4. + -                  — izquierda a derecha
// 5. = += -= *= /= %=     — derecha a izquierda

2 + 3 * 4       // 14 (no 20) — * antes que +
2 * 3 + 4 * 5   // 26 — (2*3) + (4*5)
10 - 3 - 2      // 5 — (10-3) - 2 (izquierda a derecha)
a = b = 5       // a = (b = 5) — derecha a izquierda
```

### Tabla de precedencia completa

```
Precedencia  Operador                    Asociatividad
─────────────────────────────────────────────────────────
1 (máxima)   () [] -> .  postfix ++ --  izquierda → derecha
2            prefix ++ --  + - ! ~      derecha → izquierda
             (type) * & sizeof
3            * / %                      izquierda → derecha
4            + -                        izquierda → derecha
5            << >>                      izquierda → derecha
6            < <= > >=                  izquierda → derecha
7            == !=                      izquierda → derecha
8            &                          izquierda → derecha
9            ^                          izquierda → derecha
10           |                          izquierda → derecha
11           &&                         izquierda → derecha
12           ||                         izquierda → derecha
13           ?:                         derecha → izquierda
14           = += -= *= /= %= etc.      derecha → izquierda
15 (mínima)  ,                          izquierda → derecha
```

### Trampas de precedencia

```c
// TRAMPA 1: bitwise vs comparación
if (x & MASK == 0) { }
// Se parsea como: x & (MASK == 0)
// == tiene mayor precedencia que &
// Correcto: if ((x & MASK) == 0)

// TRAMPA 2: shift vs suma
x << 2 + 1
// Se parsea como: x << (2 + 1) = x << 3
// + tiene mayor precedencia que <<
// Si querías (x << 2) + 1, usar paréntesis

// TRAMPA 3: asignación en condición
if (x = 5) { }
// Asigna 5 a x, luego evalúa 5 como true
// Probablemente querías: if (x == 5)
// -Wall genera warning para esto

// REGLA: ante la duda, usar paréntesis.
// La legibilidad importa más que memorizar la tabla.
```

## Promoción de enteros

Cuando se usan tipos menores que `int` en expresiones, se
promueven automáticamente a `int`:

```c
// Regla: antes de cualquier operación aritmética,
// char y short se promueven a int (o unsigned int).

char a = 100;
char b = 100;
// a + b → ambos se promueven a int → 200 (int)
// El resultado es int, no char

char c = a + b;    // 200 no cabe en char → truncamiento
                   // implementation-defined (probablemente -56 en signed char)

// La promoción ocurre ANTES de la operación:
unsigned char x = 255;
unsigned char y = 1;
int result = x + y;     // 256 (int, no unsigned char)
// No hay wrapping porque la suma se hizo en int
```

### Conversiones aritméticas usuales

```c
// Cuando los operandos tienen tipos diferentes después de
// la promoción, se aplican las "usual arithmetic conversions":

// float + int → float
// double + float → double
// long + int → long
// unsigned int + int → unsigned int (si mismo tamaño)

int i = -1;
unsigned int u = 1;
// i + u → i se convierte a unsigned → UINT_MAX + 1 = 0
// CUIDADO: el resultado es unsigned

long l = -1;
unsigned int u2 = 1;
// En LP64: l + u2 → u2 se convierte a long → -1 + 1 = 0
// long es más grande que unsigned int → resultado es long
```

## Operadores que no son lo que parecen

```c
// sizeof NO es una función, es un operador:
sizeof(int)      // con tipo: paréntesis obligatorios
sizeof x         // con expresión: paréntesis opcionales
// sizeof se evalúa en compilación (excepto VLAs)

// El cast (type) es un operador unario:
double d = (double)42;    // cast de int a double
int i = (int)3.14;        // cast de double a int (trunca a 3)

// & tiene dos significados:
// Unario: dirección de (&x)
// Binario: AND bitwise (a & b)

// * tiene dos significados:
// Unario: desreferenciar (*p)
// Binario: multiplicación (a * b)

// - tiene dos significados:
// Unario: negación (-x)
// Binario: resta (a - b)
```

---

## Ejercicios

### Ejercicio 1 — Precedencia

```c
// ¿Qué valor tiene cada expresión? Predecir antes de ejecutar.

int a = 2 + 3 * 4;
int b = (2 + 3) * 4;
int c = 10 - 3 - 2;
int d = 2 * 3 % 4;
int e = 15 / 4 * 4;
```

### Ejercicio 2 — División y módulo

```c
// Implementar una función que dado un número de segundos,
// imprima el equivalente en horas, minutos y segundos.
// Ejemplo: 3661 → "1h 1m 1s"
// Usar solo / y %.
```

### Ejercicio 3 — Promoción

```c
// ¿Qué imprime? ¿Por qué?

unsigned char a = 200;
unsigned char b = 100;
printf("Tipo del resultado: %zu bytes\n", sizeof(a + b));
printf("Valor: %d\n", a + b);

char c = a + b;
printf("En char: %d\n", c);
```
