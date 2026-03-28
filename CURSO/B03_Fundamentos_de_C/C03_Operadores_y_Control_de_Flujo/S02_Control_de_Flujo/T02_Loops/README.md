# T02 — Loops

## for

El loop más usado en C. La estructura es:

```c
for (inicialización; condición; actualización) {
    cuerpo;
}
```

```c
// Ejemplo básico:
for (int i = 0; i < 10; i++) {
    printf("%d\n", i);
}
// Imprime 0 a 9

// Paso a paso:
// 1. int i = 0          (se ejecuta UNA vez)
// 2. i < 10 → true      (se verifica ANTES de cada iteración)
// 3. printf(...)         (cuerpo)
// 4. i++                (se ejecuta DESPUÉS del cuerpo)
// 5. Volver a paso 2
```

### Variaciones del for

```c
// Contar hacia abajo:
for (int i = 9; i >= 0; i--) {
    printf("%d\n", i);
}

// Paso diferente:
for (int i = 0; i < 100; i += 10) {
    printf("%d\n", i);   // 0, 10, 20, ..., 90
}

// Múltiples variables (operador coma):
for (int i = 0, j = 9; i < j; i++, j--) {
    printf("i=%d j=%d\n", i, j);
}

// Iterar sobre un array:
int arr[] = {10, 20, 30, 40, 50};
size_t len = sizeof(arr) / sizeof(arr[0]);
for (size_t i = 0; i < len; i++) {
    printf("%d\n", arr[i]);
}

// Iterar sobre un string:
for (const char *p = str; *p != '\0'; p++) {
    printf("%c", *p);
}
// O:
for (int i = 0; str[i] != '\0'; i++) {
    printf("%c", str[i]);
}
```

### Partes opcionales del for

```c
// Todas las partes del for son opcionales:

// Sin inicialización:
int i = 0;
for (; i < 10; i++) { }

// Sin actualización:
for (int i = 0; i < 10;) {
    i += 2;
}

// Sin condición (loop infinito):
for (;;) {
    // loop infinito
    if (should_stop) break;
}

// Sin nada (equivalente a while(1)):
for (;;) { }
```

## while

Ejecuta el cuerpo **mientras** la condición sea verdadera.
La condición se evalúa **antes** de cada iteración:

```c
while (condición) {
    cuerpo;
}
```

```c
// Ejemplo:
int i = 0;
while (i < 10) {
    printf("%d\n", i);
    i++;
}

// Leer hasta EOF:
int c;
while ((c = getchar()) != EOF) {
    putchar(c);
}

// Buscar en una lista enlazada:
struct Node *current = head;
while (current != NULL) {
    if (current->value == target) {
        break;
    }
    current = current->next;
}
```

### for vs while

```c
// Usar for cuando:
// - Sabes cuántas iteraciones (contador)
// - Tienes inicialización, condición y actualización claras

for (int i = 0; i < n; i++) { }     // claro: n iteraciones

// Usar while cuando:
// - No sabes cuántas iteraciones
// - La condición depende de algo externo (input, búsqueda)

while (fgets(line, sizeof(line), f)) { }  // hasta EOF
while (!found) { }                         // hasta encontrar
```

## do-while

Ejecuta el cuerpo **al menos una vez**. La condición se evalúa
**después** de cada iteración:

```c
do {
    cuerpo;
} while (condición);    // nota el ; al final
```

```c
// La diferencia clave: el cuerpo se ejecuta antes de la primera
// verificación de condición:

// while puede ejecutar 0 veces:
int x = 100;
while (x < 10) {
    printf("%d\n", x);   // nunca se ejecuta
}

// do-while siempre ejecuta al menos 1 vez:
int y = 100;
do {
    printf("%d\n", y);   // se ejecuta una vez (imprime 100)
} while (y < 10);
```

### Usos del do-while

```c
// 1. Validar input del usuario:
int n;
do {
    printf("Enter a number (1-10): ");
    scanf("%d", &n);
} while (n < 1 || n > 10);
// Pide al menos una vez, repite si es inválido

// 2. Menú interactivo:
int choice;
do {
    printf("1. Option A\n");
    printf("2. Option B\n");
    printf("0. Exit\n");
    scanf("%d", &choice);
    switch (choice) {
        case 1: do_a(); break;
        case 2: do_b(); break;
    }
} while (choice != 0);
```

### do-while en macros

```c
// El uso más frecuente de do-while: macros multi-statement:

#define LOG(msg) do { \
    fprintf(stderr, "[%s:%d] ", __FILE__, __LINE__); \
    fprintf(stderr, "%s\n", msg); \
} while (0)

// ¿Por qué do { } while (0)?
// Sin él, la macro falla con if/else:

#define LOG_BAD(msg) { fprintf(stderr, msg); fprintf(stderr, "\n"); }

if (error)
    LOG_BAD("fail");    // se expande a { ... };
else                    // ERROR de compilación: else sin if
    LOG_BAD("ok");      // el ; después de } rompe el if/else

// Con do-while(0):
if (error)
    LOG("fail");        // se expande a do { ... } while(0);
else                    // OK: el ; termina el do-while correctamente
    LOG("ok");
```

## break y continue

### break — Salir del loop

```c
// break sale del loop más interno:
for (int i = 0; i < 100; i++) {
    if (arr[i] == target) {
        printf("Found at %d\n", i);
        break;                       // sale del for
    }
}

// En loops anidados, break sale SOLO del loop más interno:
for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
        if (matrix[i][j] == target) {
            break;     // sale del for de j, NO del for de i
        }
    }
    // continúa aquí después del break
}

// Para salir de loops anidados: usar goto o una flag
int found = 0;
for (int i = 0; i < rows && !found; i++) {
    for (int j = 0; j < cols && !found; j++) {
        if (matrix[i][j] == target) {
            found = 1;    // sale de ambos loops
        }
    }
}
```

### continue — Saltar al siguiente ciclo

```c
// continue salta el resto del cuerpo y va a la siguiente iteración:
for (int i = 0; i < 10; i++) {
    if (i % 2 == 0) {
        continue;      // salta los pares
    }
    printf("%d\n", i); // solo imprime impares: 1, 3, 5, 7, 9
}

// Con while, continue va a la condición:
int i = 0;
while (i < 10) {
    i++;
    if (i % 2 == 0) {
        continue;      // va directo a while (i < 10)
    }
    printf("%d\n", i);
}

// CUIDADO en while: si el incremento está después del continue,
// puede causar un loop infinito:
int j = 0;
while (j < 10) {
    if (j == 5) {
        continue;      // LOOP INFINITO: j nunca se incrementa
    }
    j++;               // este código no se ejecuta cuando j == 5
}
```

## Loops infinitos

```c
// Tres formas idiomáticas:

for (;;) {
    // forma preferida en muchos estilos
}

while (1) {
    // también común
}

do {
    // menos común
} while (1);

// Uso: servidores, event loops, lectura de input
for (;;) {
    int event = get_next_event();
    if (event == EVENT_QUIT) break;
    handle_event(event);
}
```

## Patrones comunes

### Buscar en array

```c
int find(const int *arr, size_t n, int target) {
    for (size_t i = 0; i < n; i++) {
        if (arr[i] == target) {
            return (int)i;    // encontrado
        }
    }
    return -1;                 // no encontrado
}
```

### Iterar sobre lista enlazada

```c
struct Node *p = head;
while (p != NULL) {
    process(p->data);
    p = p->next;
}

// O con for:
for (struct Node *p = head; p != NULL; p = p->next) {
    process(p->data);
}
```

### Procesar líneas de un archivo

```c
char line[256];
while (fgets(line, sizeof(line), file) != NULL) {
    // procesar line
}
```

### Doble iteración (dos punteros)

```c
// Invertir un array in-place:
for (int left = 0, right = n - 1; left < right; left++, right--) {
    int tmp = arr[left];
    arr[left] = arr[right];
    arr[right] = tmp;
}
```

---

## Ejercicios

### Ejercicio 1 — for, while, do-while

```c
// Implementar las tres versiones de un programa que imprima
// los números del 1 al 20 que son divisibles por 3.
// ¿Cuál es más natural para este problema?
```

### Ejercicio 2 — break y continue

```c
// Dado un array de enteros, encontrar el primer número negativo
// e imprimir su posición. Usar break.
// Luego, imprimir todos los números positivos del array
// saltando los negativos. Usar continue.
```

### Ejercicio 3 — Validación con do-while

```c
// Escribir un programa que pida al usuario un número entre
// 1 y 100, repitiendo hasta que ingrese un valor válido.
// Usar do-while.
// Bonus: contar cuántos intentos necesitó.
```
