# T01 — if/else, switch

> **Sin erratas detectadas** en el material fuente.

---

## 1. if / else

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
} else {
    // ninguna se cumplió
}
```

Las condiciones se evalúan **de arriba hacia abajo**. La primera que se cumple gana y las demás se saltan.

### Llaves opcionales — pero peligrosas

```c
// Sin llaves, solo el statement INMEDIATO pertenece al if:
if (x > 0)
    printf("positive\n");
    printf("always\n");        // SIEMPRE se ejecuta (no está en el if)
                               // La indentación engaña

// RECOMENDACIÓN: usar llaves SIEMPRE, incluso para una línea.
if (x > 0) {
    printf("positive\n");
    printf("and this too\n");
}
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
// La indentación engaña — parece que es del if externo.
// GCC: warning: suggest explicit braces [-Wdangling-else]

// Con llaves queda claro:
if (a > 0) {
    if (b > 0) {
        printf("both positive\n");
    }
} else {
    printf("a is not positive\n");
}
```

### Early return (guard clauses)

```c
int process(int *data, int size) {
    if (data == NULL) return -1;
    if (size <= 0)    return -1;

    // código principal sin indentación extra
    for (int i = 0; i < size; i++) {
        // ...
    }
    return 0;
}
```

Evita la pirámide de `if` anidados. Cada guard clause valida una precondición y sale temprano si no se cumple.

---

## 2. switch

Selecciona un bloque de código basándose en el valor de una **expresión entera**:

```c
switch (day) {
    case 1: printf("Monday\n");    break;
    case 2: printf("Tuesday\n");   break;
    case 3: printf("Wednesday\n"); break;
    // ...
    default: printf("Invalid\n");  break;
}
```

### Restricciones

```c
// La expresión debe ser tipo entero:
switch (x) { }        // OK: int
switch (c) { }        // OK: char (se promueve a int)
// switch (3.14) { }  // ERROR: double
// switch ("hi") { }  // ERROR: string

// Los case deben ser CONSTANTES enteras:
case 5:        // OK
case 'A':      // OK (ASCII 65)
case FOO:      // OK si FOO es #define o enum
// case x:     // ERROR: variable, no constante

// No se permiten case duplicados
```

---

## 3. Fall-through

Cuando un `case` no tiene `break`, la ejecución **cae** al siguiente case. Esto es intencional en C.

### Fall-through para agrupar cases

```c
// Uso legítimo — múltiples valores con el mismo comportamiento:
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

// Días de cada mes:
switch (month) {
    case 1: case 3: case 5: case 7:
    case 8: case 10: case 12:
        days = 31; break;
    case 4: case 6: case 9: case 11:
        days = 30; break;
    case 2:
        days = is_leap ? 29 : 28; break;
}
```

### Fall-through acumulativo

```c
// Niveles de acceso — cada nivel incluye los anteriores:
switch (level) {
    case 3:
        printf("  - admin panel\n");
        __attribute__((fallthrough));   // marca intencional (GCC)
    case 2:
        printf("  - write access\n");
        __attribute__((fallthrough));
    case 1:
        printf("  - read access\n");
        break;
}
// level=3: imprime admin + write + read
// level=2: imprime write + read
// level=1: imprime read
```

### Fall-through accidental — el bug

```c
switch (command) {
    case CMD_START:
        start_engine();
        // OLVIDÓ break — cae a CMD_STOP
    case CMD_STOP:
        stop_engine();      // se ejecuta también para CMD_START
        break;
}
```

`-Wimplicit-fallthrough` (GCC 7+, incluido en `-Wextra`) detecta este error.

Para marcar fall-through intencional y silenciar el warning:
- **C23:** `[[fallthrough]];`
- **GCC:** `__attribute__((fallthrough));`
- **Comentario:** `/* fall through */` (GCC lo reconoce)

---

## 4. default y switch con enum

```c
// default se ejecuta si ningún case coincide:
switch (x) {
    case 1: /* ... */ break;
    case 2: /* ... */ break;
    default: /* ... */ break;  // convención: ponerlo al final
}
```

**Con enum — estrategia sin default:**

```c
enum Color { RED, GREEN, BLUE };

void print_color(enum Color c) {
    switch (c) {
        case RED:   printf("red\n");   break;
        case GREEN: printf("green\n"); break;
        case BLUE:  printf("blue\n");  break;
        // Sin default — -Wswitch advierte si falta un valor del enum
    }
}

// Si agregas YELLOW al enum y olvidas el case:
// warning: enumeration value 'YELLOW' not handled in switch
// Este warning SOLO aparece si no hay default.
// Con default, GCC asume que el default lo cubre y no advierte.
```

---

## 5. if/else vs switch

| Usar switch cuando | Usar if/else cuando |
|---|---|
| Comparas **una** variable contra **constantes** | Comparas rangos (`x > 5 && x < 10`) |
| Hay muchos cases (más legible) | Las condiciones son expresiones complejas |
| Los valores son enteros o enum | Comparas diferentes variables |
| | Los valores no son enteros (strings, floats) |

### Assembly: if vs switch con optimización

Sin optimización (`-O0`), ambos generan comparaciones secuenciales, pero switch tiende a ser más compacto.

Con optimización (`-O2`), el compilador puede transformar **ambas** formas en una **lookup table** (acceso O(1) en lugar de O(n) comparaciones). La diferencia de rendimiento es mínima — elegir la estructura más **legible**.

---

## 6. Duff's device — curiosidad histórica

```c
// Loop unrolling manual con fall-through (Tom Duff, 1983):
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
```

El switch se entrelaza con el do-while. Es C válido pero **no escribir código así** — los compiladores modernos hacen loop unrolling automáticamente con `-O2`.

---

## Ejercicios

### Ejercicio 1 — Branching básico

```c
#include <stdio.h>

int main(void) {
    int score = 85;

    if (score >= 90) {
        printf("A\n");
    } else if (score >= 80) {
        printf("B\n");
    } else if (score >= 70) {
        printf("C\n");
    } else if (score >= 60) {
        printf("D\n");
    } else {
        printf("F\n");
    }

    return 0;
}
```

**¿Qué letra imprime? ¿Qué pasa si cambiamos `score` a 90?**

<details>
<summary>Predicción</summary>

```
B
```

85 no es >= 90, pero sí es >= 80 → entra en la segunda rama → "B". Las condiciones posteriores (>= 70, >= 60) no se evalúan porque la primera coincidencia gana.

Si `score = 90`: 90 >= 90 es true → entra en la primera rama → "A".

</details>

---

### Ejercicio 2 — Dangling else

```c
#include <stdio.h>

int main(void) {
    int a = 1, b = -1;

    if (a > 0)
        if (b > 0)
            printf("both positive\n");
    else
        printf("else branch\n");

    return 0;
}
```

**¿Se imprime "else branch"? ¿A cuál `if` pertenece el `else`?**

<details>
<summary>Predicción</summary>

```
else branch
```

El `else` pertenece al `if` **más cercano** (`b > 0`), no al externo (`a > 0`). Con `a=1, b=-1`: el if externo es true, el if interno es false → el else se ejecuta.

Si fuera `a=-1, b=-1`: el if externo es false → no se entra a nada → no se imprime nada. El else NO maneja el caso `a <= 0` como la indentación sugiere.

GCC advierte con `-Wdangling-else`.

</details>

---

### Ejercicio 3 — switch básico con break

```c
#include <stdio.h>

int main(void) {
    int day = 5;

    switch (day) {
        case 1: printf("Mon "); break;
        case 2: printf("Tue "); break;
        case 3: printf("Wed "); break;
        case 4: printf("Thu "); break;
        case 5: printf("Fri "); break;
        case 6: printf("Sat "); break;
        case 7: printf("Sun "); break;
        default: printf("??? "); break;
    }
    printf("\n");

    day = 9;
    switch (day) {
        case 1: printf("Mon\n"); break;
        default: printf("Not Monday\n"); break;
    }

    return 0;
}
```

**¿Qué imprime para `day=5` y para `day=9`?**

<details>
<summary>Predicción</summary>

```
Fri
Not Monday
```

- `day=5`: entra en `case 5`, imprime "Fri ", el `break` sale del switch.
- `day=9`: no coincide con ningún case → entra en `default` → "Not Monday".

</details>

---

### Ejercicio 4 — Fall-through accidental

```c
#include <stdio.h>

int main(void) {
    int cmd = 1;

    printf("Output: ");
    switch (cmd) {
        case 1: printf("A ");
        case 2: printf("B ");
        case 3: printf("C "); break;
        case 4: printf("D "); break;
    }
    printf("\n");

    return 0;
}
```

**¿Qué imprime con `cmd=1`? ¿Y con `cmd=2`? ¿Y con `cmd=3`?**

<details>
<summary>Predicción</summary>

```
cmd=1: Output: A B C
cmd=2: Output: B C
cmd=3: Output: C
```

- `cmd=1`: entra en case 1, imprime "A ". Sin break → cae a case 2, imprime "B ". Sin break → cae a case 3, imprime "C ". Break sale.
- `cmd=2`: entra en case 2, imprime "B ". Sin break → cae a case 3, imprime "C ". Break sale.
- `cmd=3`: entra en case 3, imprime "C ". Break sale.

El fall-through es acumulativo: `cmd=1` ejecuta A+B+C, `cmd=2` ejecuta B+C.

</details>

---

### Ejercicio 5 — Fall-through intencional: días del mes

```c
#include <stdio.h>

int main(void) {
    int month = 6;
    int days;

    switch (month) {
        case 1: case 3: case 5: case 7:
        case 8: case 10: case 12:
            days = 31; break;
        case 4: case 6: case 9: case 11:
            days = 30; break;
        case 2:
            days = 28; break;
        default:
            days = -1; break;
    }

    printf("Month %d has %d days\n", month, days);

    return 0;
}
```

**¿Cuántos días tiene el mes 6? ¿Y el mes 13?**

<details>
<summary>Predicción</summary>

```
Month 6 has 30 days
```

- `month=6`: `case 6` cae en el grupo 4/6/9/11 → `days = 30`.
- `month=13`: no coincide con ningún case → `default` → `days = -1`.

Los cases vacíos (sin código entre ellos) son fall-through para agrupar valores con el mismo comportamiento. Este es el uso más limpio de fall-through.

</details>

---

### Ejercicio 6 — `=` vs `==`

```c
#include <stdio.h>

int main(void) {
    int x = 0;

    if (x = 0) {
        printf("A: true\n");
    } else {
        printf("A: false\n");
    }

    if (x = 5) {
        printf("B: true, x=%d\n", x);
    } else {
        printf("B: false\n");
    }

    return 0;
}
```

**¿Qué imprime cada `if`? ¿Qué valor tiene `x` después de cada uno?**

<details>
<summary>Predicción</summary>

```
A: false
B: true, x=5
```

- `if (x = 0)`: **asigna** 0 a x, luego evalúa 0 → false. Entra al else.
- `if (x = 5)`: **asigna** 5 a x, luego evalúa 5 → true (no-cero). Entra al if.

Ninguno es una comparación. `x = 0` siempre es false, `x = 5` siempre es true. GCC advierte con `-Wparentheses`.

</details>

---

### Ejercicio 7 — Guard clauses

```c
#include <stdio.h>

// Versión anidada (pirámide):
int find_nested(int *arr, int size, int target) {
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

// Versión con early returns:
int find_flat(int *arr, int size, int target) {
    if (arr == NULL) return -1;
    if (size <= 0)   return -1;
    if (target < 0)  return -1;

    for (int i = 0; i < size; i++) {
        if (arr[i] == target) return i;
    }
    return -1;
}

int main(void) {
    int data[] = {10, 20, 30, 40, 50};
    printf("nested: %d\n", find_nested(data, 5, 30));
    printf("flat:   %d\n", find_flat(data, 5, 30));
    printf("null:   %d\n", find_flat(NULL, 5, 30));

    return 0;
}
```

**¿Ambas funciones dan el mismo resultado? ¿Cuál es más legible?**

<details>
<summary>Predicción</summary>

```
nested: 2
flat:   2
null:   -1
```

Ambas producen el mismo resultado. `find_flat` es más legible porque:
1. Los guard clauses salen temprano, eliminando la indentación anidada
2. El camino principal (el `for`) está al nivel base
3. Las precondiciones son explícitas y fáciles de leer

El patrón de early return es preferido en la mayoría del código C moderno.

</details>

---

### Ejercicio 8 — switch con enum

```c
#include <stdio.h>

enum Direction { NORTH, SOUTH, EAST, WEST };

const char *dir_name(enum Direction d) {
    switch (d) {
        case NORTH: return "North";
        case SOUTH: return "South";
        case EAST:  return "East";
        case WEST:  return "West";
    }
    return "Unknown";
}

int main(void) {
    for (int i = 0; i < 4; i++) {
        printf("%d -> %s\n", i, dir_name((enum Direction)i));
    }
    printf("99 -> %s\n", dir_name((enum Direction)99));

    return 0;
}
```

**¿Qué imprime para 99? ¿Qué pasa si añadimos `UP` al enum pero no al switch?**

<details>
<summary>Predicción</summary>

```
0 -> North
1 -> South
2 -> East
3 -> West
99 -> Unknown
```

Para 99: ningún case coincide, no hay default, la ejecución cae fuera del switch y retorna "Unknown".

Si añadimos `UP` al enum sin case: como no hay `default`, GCC con `-Wswitch` advierte: `enumeration value 'UP' not handled in switch`. Si tuviera `default`, el warning no aparecería — por eso algunos prefieren no poner default con enums, para que el compilador detecte valores faltantes.

</details>

---

### Ejercicio 9 — Niveles de acceso acumulativos

```c
#include <stdio.h>

void show_access(int level) {
    printf("Level %d:\n", level);
    switch (level) {
        case 3:
            printf("  - delete\n");
            __attribute__((fallthrough));
        case 2:
            printf("  - write\n");
            __attribute__((fallthrough));
        case 1:
            printf("  - read\n");
            break;
        case 0:
            printf("  - no access\n");
            break;
        default:
            printf("  - invalid\n");
            break;
    }
}

int main(void) {
    for (int i = 0; i <= 4; i++) {
        show_access(i);
    }
    return 0;
}
```

**¿Qué accesos tiene cada nivel?**

<details>
<summary>Predicción</summary>

```
Level 0:
  - no access
Level 1:
  - read
Level 2:
  - write
  - read
Level 3:
  - delete
  - write
  - read
Level 4:
  - invalid
```

Cada nivel acumula los permisos de los niveles inferiores gracias al fall-through intencional. Level 3 ejecuta delete, cae a level 2 (write), cae a level 1 (read), y el break detiene. `__attribute__((fallthrough))` silencia el warning de `-Wimplicit-fallthrough`.

</details>

---

### Ejercicio 10 — Calculadora con switch

```c
#include <stdio.h>

int main(void) {
    double a = 10.0, b = 3.0;
    char op = '/';

    printf("%.1f %c %.1f = ", a, op, b);

    switch (op) {
        case '+': printf("%.2f\n", a + b); break;
        case '-': printf("%.2f\n", a - b); break;
        case '*': printf("%.2f\n", a * b); break;
        case '/':
            if (b == 0) {
                printf("ERROR: division by zero\n");
            } else {
                printf("%.2f\n", a / b);
            }
            break;
        default:
            printf("ERROR: unknown operator '%c'\n", op);
            break;
    }

    // ¿Qué pasa con op = '%'?
    op = '%';
    printf("%.1f %c %.1f = ", a, op, b);
    switch (op) {
        case '+': printf("%.2f\n", a + b); break;
        case '-': printf("%.2f\n", a - b); break;
        case '*': printf("%.2f\n", a * b); break;
        case '/': printf("%.2f\n", a / b); break;
        default:
            printf("ERROR: unknown operator '%c'\n", op);
            break;
    }

    return 0;
}
```

**¿Qué imprime para `op='/'`? ¿Y para `op='%'`?**

<details>
<summary>Predicción</summary>

```
10.0 / 3.0 = 3.33
10.0 % 3.0 = ERROR: unknown operator '%'
```

- `op='/'`: entra en case `/`, `b != 0` → divide: 10.0 / 3.0 = 3.333... → imprime 3.33.
- `op='%'`: no coincide con ningún case → default → "unknown operator '%'".

El `%` no se puede usar con doubles en un switch así. `%` (módulo) solo funciona con enteros en C. Para doubles se usaría `fmod(a, b)` de `<math.h>`.

</details>
