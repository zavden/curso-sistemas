# T01 — const y punteros

## Las 4 combinaciones

Hay cuatro formas de combinar `const` con un puntero.
Se leen de **derecha a izquierda**:

```c
// 1. Puntero a dato mutable (puntero mutable):
int *p;
// p puede cambiar, *p puede cambiar
// "p is a pointer to int"

// 2. Puntero a dato const (puntero mutable):
const int *p;
// p puede cambiar, *p NO puede cambiar
// "p is a pointer to const int"

// 3. Puntero const a dato mutable:
int *const p = &x;
// p NO puede cambiar, *p puede cambiar
// "p is a const pointer to int"

// 4. Puntero const a dato const:
const int *const p = &x;
// p NO puede cambiar, *p NO puede cambiar
// "p is a const pointer to const int"
```

### Regla de lectura derecha a izquierda

```c
// Leer la declaración de derecha a izquierda:
//
// const int *p;
//           ←──
// p es un puntero (*) a int que es const
// → el dato apuntado es const
//
// int *const p;
//       ←────
// p es un const puntero (*) a int
// → el puntero es const
//
// const int *const p;
//             ←────
// p es un const puntero (*) a int que es const
// → ambos son const

// Nota: "int const *p" y "const int *p" son equivalentes.
// const puede ir antes o después del tipo:
const int *p;    // más común
int const *p;    // equivalente — "int that is const"
```

## 1. int *p — Todo mutable

```c
int x = 10, y = 20;
int *p = &x;

*p = 100;     // OK — modificar el dato
p = &y;       // OK — redirigir el puntero
*p = 200;     // OK — modificar y a través de p

printf("x=%d y=%d\n", x, y);    // x=100 y=200
```

## 2. const int *p — Dato protegido

El uso más importante: promete no modificar el dato apuntado.
Se usa en parámetros de funciones que solo **leen**:

```c
void print_value(const int *p) {
    printf("%d\n", *p);
    // *p = 42;    // ERROR: assignment of read-only location
}

int main(void) {
    int x = 10;
    const int *p = &x;

    printf("%d\n", *p);    // OK — leer
    // *p = 20;            // ERROR — no se puede modificar vía p
    p = &y;                // OK — el puntero sí se puede redirigir

    x = 20;                // OK — x no es const, solo la vista a través de p
    printf("%d\n", *p);    // 20

    return 0;
}
```

```c
// const int* es la forma de decir "no voy a modificar esto":
int sum(const int *arr, int n) {
    int total = 0;
    for (int i = 0; i < n; i++) {
        total += arr[i];
        // arr[i] = 0;    // ERROR — protegido por const
    }
    return total;
}

// La función strlen tiene const porque no modifica el string:
size_t strlen(const char *s);

// strcpy tiene const en src pero no en dest:
char *strcpy(char *dest, const char *src);
// dest se modifica, src no.
```

### Conversión int* → const int* es implícita

```c
int x = 42;
int *p = &x;

// int* se convierte a const int* automáticamente:
const int *cp = p;    // OK — agregar const es seguro

// const int* NO se convierte a int* automáticamente:
// int *q = cp;       // WARNING: discards 'const' qualifier

// Esto tiene sentido:
// - Agregar restricciones (const) es seguro
// - Quitar restricciones puede causar bugs
```

## 3. int *const p — Puntero fijo

El puntero no se puede redirigir, pero el dato sí se puede
modificar:

```c
int x = 10;
int *const p = &x;    // DEBE inicializarse en la declaración

*p = 100;     // OK — modificar el dato
// p = &y;    // ERROR: assignment of read-only variable 'p'

printf("x = %d\n", x);    // 100
```

```c
// Es como una referencia en C++: siempre apunta al mismo lugar.
// Uso raro en la práctica.

// Ejemplo: un puntero a un buffer que no debe cambiar:
void fill_buffer(int *const buf, int n) {
    for (int i = 0; i < n; i++) {
        buf[i] = i;        // OK — modifica el dato
    }
    // buf = NULL;          // ERROR — no se puede redirigir
}
```

## 4. const int *const p — Todo protegido

```c
int x = 42;
const int *const p = &x;

// *p = 100;    // ERROR — dato protegido
// p = &y;      // ERROR — puntero protegido

// Solo se puede leer:
printf("%d\n", *p);    // OK
```

```c
// Uso: tablas de lookup constantes:
static const char *const error_messages[] = {
    "Success",
    "File not found",
    "Permission denied",
    "Out of memory",
};
// const char *const: cada puntero es const Y apunta a const char
// static: solo visible en este archivo
// No se pueden modificar los punteros ni los strings.
```

## Tabla resumen

```c
// |  Declaración          |  ¿Se puede *p = x?  |  ¿Se puede p = &y?  |
// |-----------------------|----------------------|----------------------|
// |  int *p               |  Sí                  |  Sí                  |
// |  const int *p         |  No                  |  Sí                  |
// |  int *const p         |  Sí                  |  No                  |
// |  const int *const p   |  No                  |  No                  |
```

## const y punteros a punteros

```c
// Con doble puntero, hay más combinaciones:
const int **pp;          // puntero a (puntero a const int)
int *const *pp;          // puntero a (const puntero a int)
int **const pp;          // const puntero a (puntero a int)

// La conversión int** → const int** NO es segura:
const int x = 42;
int *p;
const int **pp = &p;     // si esto fuera legal...
*pp = &x;                // pp modifica p para apuntar a x (const)
*p = 100;                // p modifica x — ¡pero x es const! UB

// Por eso el compilador da warning en: const int **pp = &p;
// int** y const int** son tipos incompatibles.
```

## const en la práctica

```c
// Regla: poner const en todo lo que no necesites modificar.
// Esto documenta la intención y el compilador verifica.

// Parámetros — const donde no se modifica:
int find(const char *haystack, const char *needle);
void copy(char *dest, const char *src, size_t n);
int sum(const int *arr, size_t n);

// Variables locales — const si no cambian:
const int max_retries = 5;
const char *filename = argv[1];

// Retorno — const para strings literales:
const char *get_error_message(int code);
// Retorna un puntero a string literal (no debe modificarse)
```

```c
// Casting away const — técnicamente posible pero peligroso:
const int x = 42;
int *p = (int *)&x;     // cast para quitar const
*p = 100;               // UB — x fue declarado como const
// El compilador puede haber puesto x en memoria de solo lectura,
// o puede haber sustituido x por 42 en todo el código.

// Solo es "válido" si el objeto original NO era const:
int y = 42;
const int *cp = &y;
int *mp = (int *)cp;    // y no es const, así que esto es OK
*mp = 100;              // OK — y no era const
```

---

## Ejercicios

### Ejercicio 1 — Identificar errores

```c
// ¿Cuáles de estas líneas compilan? ¿Cuáles dan warning?
int x = 10, y = 20;
const int cx = 30;

int *p1 = &x;
const int *p2 = &x;
int *const p3 = &x;
const int *const p4 = &x;

*p1 = 100;
*p2 = 100;
*p3 = 100;
*p4 = 100;
p1 = &y;
p2 = &y;
p3 = &y;
p4 = &y;

int *p5 = &cx;
const int *p6 = &cx;

// Compilar con -Wall y verificar.
```

### Ejercicio 2 — Firmas de función

```c
// Escribir las firmas correctas (con const donde corresponda):
// 1. Función que busca un char en un string (no modifica el string)
// 2. Función que copia n bytes de src a dest
// 3. Función que compara dos structs Point (no modifica ninguno)
// 4. Función que retorna un puntero a un string constante
```

### Ejercicio 3 — const correctness

```c
// Este código compila pero tiene const incorrectos.
// Agregar const donde sea apropiado y quitar donde no:
void process(int *data, int n, int *result) {
    int sum = 0;
    for (int i = 0; i < n; i++) {
        sum += data[i];
    }
    *result = sum;
}

char *get_greeting(void) {
    return "Hello, World!";
}
```
