# T01 — if/else, switch

## if / else

```c
if (condición) {
    // se ejecuta si condición es no-cero (true)
}

if (condición) {
    // rama true
} else {
    // rama false
}

if (condición1) {
    // ...
} else if (condición2) {
    // ...
} else if (condición3) {
    // ...
} else {
    // ninguna se cumplió
}
```

### Verdadero y falso en C

```c
// En C, no hay tipo bool nativo antes de C99.
// Cualquier expresión entera se evalúa como:
//   0              → false
//   cualquier otro → true

if (42)    { }    // true
if (-1)    { }    // true
if (0)     { }    // false
if (ptr)   { }    // true si ptr != NULL
if ('\0')  { }    // false (valor 0)
```

### Llaves opcionales (pero peligrosas)

```c
// Las llaves son opcionales para un solo statement:
if (x > 0)
    printf("positive\n");

// Pero esto es peligroso:
if (x > 0)
    printf("positive\n");
    printf("always\n");        // SIEMPRE se ejecuta (no está en el if)
                               // La indentación engaña

// Con llaves no hay ambigüedad:
if (x > 0) {
    printf("positive\n");
    printf("and this too\n");
}

// RECOMENDACIÓN: usar llaves SIEMPRE, incluso para una línea.
```

### Dangling else

```c
// ¿A cuál if pertenece este else?
if (a > 0)
    if (b > 0)
        printf("both positive\n");
else
    printf("which if?\n");

// El else pertenece al if MÁS CERCANO (el de b > 0).
// La indentación del else es engañosa.

// Con llaves queda claro:
if (a > 0) {
    if (b > 0) {
        printf("both positive\n");
    }
} else {
    printf("a is not positive\n");
}
```

### Patrones comunes

```c
// Early return (guard clause):
int process(int *data, int size) {
    if (data == NULL) return -1;
    if (size <= 0)    return -1;

    // código principal sin indentación extra
    for (int i = 0; i < size; i++) {
        // ...
    }
    return 0;
}

// Verificar error de función:
FILE *f = fopen("data.txt", "r");
if (f == NULL) {
    perror("fopen");
    return 1;
}
// usar f...
```

## switch

`switch` selecciona un bloque de código basándose en el valor
de una expresión entera:

```c
switch (expresión) {
    case valor1:
        // código
        break;
    case valor2:
        // código
        break;
    default:
        // si ningún case coincide
        break;
}
```

```c
// Ejemplo:
switch (day) {
    case 1: printf("Monday\n");    break;
    case 2: printf("Tuesday\n");   break;
    case 3: printf("Wednesday\n"); break;
    case 4: printf("Thursday\n");  break;
    case 5: printf("Friday\n");    break;
    case 6: printf("Saturday\n");  break;
    case 7: printf("Sunday\n");    break;
    default: printf("Invalid\n");  break;
}
```

### Restricciones de switch

```c
// La expresión debe ser un tipo entero:
switch (x) { }        // OK: int
switch (c) { }        // OK: char (se promueve a int)
// switch (3.14) { }  // ERROR: no se puede usar double
// switch ("hello") { } // ERROR: no se puede usar string

// Los case deben ser CONSTANTES enteras:
case 5:        // OK
case 'A':      // OK (valor ASCII 65)
case FOO:      // OK si FOO es un #define o enum
// case x:     // ERROR: x es variable, no constante
// case 1..5:  // ERROR en C estándar (extensión GNU: case 1 ... 5)

// No se permiten case duplicados:
case 5:
case 5:        // ERROR: duplicado
```

### Fall-through — Es una feature

Cuando un `case` no tiene `break`, la ejecución **cae** al
siguiente case. Esto es intencional en C:

```c
// Sin break — fall-through:
switch (grade) {
    case 'A':
        printf("Excellent ");
        // sin break — cae al siguiente case
    case 'B':
        printf("Good ");
        // sin break — cae al siguiente
    case 'C':
        printf("Pass\n");
        break;
    case 'F':
        printf("Fail\n");
        break;
}

// Si grade == 'A': imprime "Excellent Good Pass"
// Si grade == 'B': imprime "Good Pass"
// Si grade == 'C': imprime "Pass"
```

### Fall-through intencionado — Agrupar cases

```c
// Uso legítimo: múltiples valores con el mismo comportamiento:
switch (c) {
    case 'a':
    case 'e':
    case 'i':
    case 'o':
    case 'u':
        printf("vowel\n");
        break;
    default:
        printf("consonant\n");
        break;
}

// Otro ejemplo:
switch (month) {
    case 1: case 3: case 5: case 7:
    case 8: case 10: case 12:
        days = 31;
        break;
    case 4: case 6: case 9: case 11:
        days = 30;
        break;
    case 2:
        days = is_leap ? 29 : 28;
        break;
}
```

### Fall-through accidental — El bug

```c
// El bug más común con switch: olvidar break
switch (command) {
    case CMD_START:
        start_engine();
        // OLVIDÓ break — cae a CMD_STOP
    case CMD_STOP:
        stop_engine();      // se ejecuta también para CMD_START
        break;
}

// -Wimplicit-fallthrough (GCC 7+) advierte de esto:
// warning: this statement may fall through

// Para indicar que el fall-through es INTENCIONAL:
// C23: [[fallthrough]];
// GCC: __attribute__((fallthrough));
// Comentario: /* fall through */ (GCC lo reconoce)

switch (state) {
    case INIT:
        initialize();
        __attribute__((fallthrough));  // silencia el warning
    case RUNNING:
        run();
        break;
}
```

### default

```c
// default se ejecuta si ningún case coincide:
switch (x) {
    case 1: /* ... */ break;
    case 2: /* ... */ break;
    default: /* ... */ break;
}

// default puede ir en cualquier posición (no solo al final):
switch (x) {
    default: printf("default\n"); break;
    case 1:  printf("one\n");     break;
    case 2:  printf("two\n");     break;
}
// Funciona igual, pero ponerlo al final es la convención.

// BUENA PRÁCTICA: siempre incluir default,
// aunque creas que todos los casos están cubiertos.
// Un valor inesperado puede llegar por un bug.
```

### switch con enum

```c
enum Color { RED, GREEN, BLUE };

void print_color(enum Color c) {
    switch (c) {
        case RED:   printf("red\n");   break;
        case GREEN: printf("green\n"); break;
        case BLUE:  printf("blue\n");  break;
        // Sin default: -Wswitch advierte si falta un valor del enum
    }
}

// Si agregas YELLOW al enum y olvidas el case:
// warning: enumeration value 'YELLOW' not handled in switch
// Por eso algunos prefieren NO poner default con enums:
// el warning solo aparece si no hay default.
```

### Duff's device — La curiosidad

```c
// Duff's device es un uso creativo (y confuso) de fall-through
// para hacer loop unrolling manual:

void duff_copy(char *to, const char *from, int count) {
    int n = (count + 7) / 8;
    switch (count % 8) {
        case 0: do { *to++ = *from++;
        case 7:      *to++ = *from++;
        case 6:      *to++ = *from++;
        case 5:      *to++ = *from++;
        case 4:      *to++ = *from++;
        case 3:      *to++ = *from++;
        case 2:      *to++ = *from++;
        case 1:      *to++ = *from++;
                } while (--n > 0);
    }
}

// Esto es código C válido. El switch se entrelaza con el do-while.
// Tom Duff lo escribió en 1983 para optimizar copia de datos.
//
// NO escribir código así. Es una curiosidad histórica.
// Los compiladores modernos hacen loop unrolling automáticamente
// con -O2 o -O3, y lo hacen mejor que esto.
```

## if/else vs switch

```c
// Usar switch cuando:
// - Comparas UNA variable contra VALORES CONSTANTES
// - Hay muchos cases (más legible que cadena de if/else if)
// - Los valores son enteros o enum

// Usar if/else cuando:
// - Comparas rangos (x > 5 && x < 10)
// - Las condiciones son expresiones complejas
// - Comparas diferentes variables
// - Los valores no son enteros (strings, floats)

// switch:
switch (opcode) {
    case OP_ADD: /* ... */ break;
    case OP_SUB: /* ... */ break;
    case OP_MUL: /* ... */ break;
}

// if/else:
if (x > 0 && x < 100) {
    // rango
} else if (y == NULL || z == NULL) {
    // condición compleja
}
```

---

## Ejercicios

### Ejercicio 1 — switch con fall-through

```c
// Escribir un switch que dado un mes (1-12),
// imprima cuántos días tiene (sin considerar bisiestos).
// Usar fall-through para agrupar los meses de 31 y 30 días.
```

### Ejercicio 2 — Calculadora

```c
// Escribir un programa que lea dos números y un operador (+,-,*,/),
// y muestre el resultado.
// Usar switch para el operador.
// Manejar: división por cero, operador inválido.
```

### Ejercicio 3 — Guard clauses

```c
// Refactorizar esta función usando early returns:

int process(int *arr, int size, int target) {
    int result = -1;
    if (arr != NULL) {
        if (size > 0) {
            if (target >= 0) {
                for (int i = 0; i < size; i++) {
                    if (arr[i] == target) {
                        result = i;
                        break;
                    }
                }
            }
        }
    }
    return result;
}
```
