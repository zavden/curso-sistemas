# T01 — El modelo de strings en C

## Null-terminated strings

En C no existe un tipo "string". Un string es simplemente un
**array de char terminado en `\0`** (null terminator, byte 0):

```c
#include <stdio.h>

int main(void) {
    // Estas tres formas son equivalentes:
    char s1[] = "hello";
    char s2[] = {'h', 'e', 'l', 'l', 'o', '\0'};
    char s3[6] = {'h', 'e', 'l', 'l', 'o', '\0'};

    // sizeof incluye el terminador:
    printf("sizeof(s1) = %zu\n", sizeof(s1));    // 6 (5 chars + '\0')

    // strlen NO incluye el terminador:
    printf("strlen(s1) = %zu\n", strlen(s1));    // 5

    return 0;
}
```

```c
// El terminador '\0' es el byte 0 (no el carácter '0'):
// '\0' == 0      → true
// '0'  == 48     → true (código ASCII de '0')

// En memoria, "hello" se ve así:
// [104][101][108][108][111][0]
//   h    e    l    l    o   \0

// Toda función de string (strlen, printf %s, strcpy, etc.)
// lee caracteres hasta encontrar '\0'.
// Si no hay '\0': la función sigue leyendo → UB.
```

## Puntero vs array para strings

```c
// Hay dos formas de tener un string — son DIFERENTES:

// 1. Array de char — COPIA del string literal:
char arr[] = "hello";
// arr es un array de 6 chars en el stack
// Se puede modificar:
arr[0] = 'H';          // OK → "Hello"

// 2. Puntero a string literal — apunta a memoria de solo lectura:
const char *ptr = "hello";
// ptr apunta a un string literal en la sección .rodata
// NO se puede modificar:
// ptr[0] = 'H';       // UB — segfault en la mayoría de sistemas

// Técnicamente, el tipo de un string literal es char[] (no const),
// pero modificarlo es UB. Siempre usar const char * para string literals.
```

```c
// Diferencias:
//
// | Aspecto     | char arr[] = "hi"     | const char *p = "hi"   |
// |-------------|------------------------|------------------------|
// | Ubicación   | Stack (copia)          | .rodata (solo lectura) |
// | Modificable | Sí                     | No (UB)                |
// | sizeof      | 3 (array de 3 chars)   | 8 (tamaño del puntero) |
// | Reasignable | No (arr = "bye" falla) | Sí (p = "bye" OK)      |
// | Uso típico  | Buffer modificable     | String constante       |

const char *p = "hello";
p = "world";       // OK — p apunta a otro string literal
// arr = "world";  // ERROR — array no se puede reasignar
```

## Por qué null-terminated es peligroso

```c
// 1. El terminador se puede perder:
char buf[5];
buf[0] = 'h';
buf[1] = 'e';
buf[2] = 'l';
buf[3] = 'l';
buf[4] = 'o';
// NO hay '\0' — buf NO es un string válido
printf("%s\n", buf);      // UB — lee más allá del buffer
strlen(buf);               // UB — no encuentra '\0'

// Correcto:
char buf2[6];
buf2[0] = 'h';
buf2[1] = 'e';
buf2[2] = 'l';
buf2[3] = 'l';
buf2[4] = 'o';
buf2[5] = '\0';           // ahora sí es un string válido
```

```c
// 2. Calcular la longitud es O(n):
size_t len = strlen(s);
// strlen recorre TODOS los caracteres hasta '\0'.
// Para un string de 1 millón de chars, recorre 1 millón de bytes.
// Si necesitas la longitud repetidamente, guardarla en una variable.

// En contraste, lenguajes como Rust, Go, Pascal almacenan
// la longitud junto al string → O(1).
```

```c
// 3. No se puede contener '\0' dentro del string:
char s[] = "hello\0world";
printf("%s\n", s);          // imprime solo "hello"
printf("len = %zu\n", strlen(s));   // 5 — strlen para en el primer \0

// sizeof(s) = 12 — el compilador sí sabe que hay 12 bytes
// pero todas las funciones de string ven solo "hello"

// Para datos binarios con bytes 0, usar memcpy/memcmp
// con longitud explícita, no funciones de string.
```

```c
// 4. Buffer overflow — el error más común:
char buf[10];
strcpy(buf, "this string is way too long for the buffer");
// Escribe más allá de buf → UB → posible exploit

// La raíz del problema: las funciones de string no conocen
// el tamaño del buffer destino. Solo buscan '\0' en el source.
```

## Strings en memoria

```c
// Un programa C tiene strings en varias ubicaciones:

// 1. String literals → .rodata (solo lectura):
const char *msg = "hello";
// "hello" existe en .rodata durante toda la ejecución

// 2. Arrays en stack → automáticos:
void foo(void) {
    char buf[100] = "hello";
    // buf está en el stack, se destruye al salir de foo
}

// 3. Arrays static → .data o .bss:
static char config[256] = "default";
// Existe durante toda la ejecución, modificable

// 4. Heap → malloc:
char *s = malloc(100);
strcpy(s, "hello");
// Existe hasta que se llame free(s)
```

## Operaciones básicas con strings

```c
#include <stdio.h>
#include <string.h>

int main(void) {
    // Longitud:
    const char *s = "hello";
    size_t len = strlen(s);              // 5

    // Comparación (NO usar == para strings):
    const char *a = "hello";
    const char *b = "hello";
    if (a == b) { /* compara punteros, NO contenido */ }
    if (strcmp(a, b) == 0) { /* compara contenido — correcto */ }

    // Copia:
    char dest[20];
    strcpy(dest, "hello");               // copia "hello\0" a dest

    // Concatenación:
    char buf[50] = "hello";
    strcat(buf, " world");               // buf = "hello world"

    // Búsqueda:
    char *p = strchr("hello", 'l');      // p apunta a "llo"
    char *q = strstr("hello world", "world");  // q apunta a "world"

    return 0;
}
```

## Imprimir y leer strings

```c
#include <stdio.h>

int main(void) {
    // Imprimir:
    const char *name = "Alice";
    printf("%s\n", name);           // Alice
    printf("%.3s\n", name);         // Ali (máximo 3 chars)
    printf("%-10s|\n", name);       // Alice     | (alineado izquierda)
    printf("%10s|\n", name);        //      Alice| (alineado derecha)

    puts(name);                      // Alice\n (agrega newline)
    fputs(name, stdout);             // Alice (sin newline)

    // Leer:
    char buf[100];

    // fgets — la forma SEGURA (limita la lectura):
    fgets(buf, sizeof(buf), stdin);
    // Lee hasta sizeof(buf)-1 chars o hasta '\n'
    // INCLUYE el '\n' en el buffer
    // SIEMPRE termina con '\0'

    // gets — PROHIBIDA (eliminada en C11):
    // gets(buf);    // NO — no hay límite de lectura → buffer overflow
    // Fue eliminada del estándar por ser imposible de usar de forma segura.

    // scanf %s — peligroso sin límite:
    // scanf("%s", buf);    // NO — como gets, sin límite
    scanf("%99s", buf);     // OK — límite de 99 chars + '\0'
    // Pero %s para en whitespace, no lee líneas completas.

    return 0;
}
```

## Quitar el newline de fgets

```c
// fgets incluye '\n' en el buffer. Para quitarlo:

char buf[100];
fgets(buf, sizeof(buf), stdin);

// Método 1 — strcspn (moderno, limpio):
buf[strcspn(buf, "\n")] = '\0';
// strcspn retorna el índice del primer '\n' (o la longitud si no hay)

// Método 2 — buscar y reemplazar:
char *newline = strchr(buf, '\n');
if (newline) {
    *newline = '\0';
}

// Método 3 — con strlen:
size_t len = strlen(buf);
if (len > 0 && buf[len - 1] == '\n') {
    buf[len - 1] = '\0';
}
```

## Tabla resumen: modelo de strings en C

| Aspecto | C | Rust |
|---|---|---|
| Representación | `char[]` + `\0` | `String` / `&str` (len + ptr) |
| Longitud | O(n) — strlen | O(1) — campo len |
| Modificable | Depende de cómo se declaró | `String` sí, `&str` no |
| Contener `\0` | No (termina el string) | Sí (len es explícito) |
| Encoding | Bytes crudos | UTF-8 garantizado |
| Seguridad | Responsabilidad del programador | Garantizada por el compilador |
| Buffer overflow | Posible y común | Imposible (bounds checking) |

---

## Ejercicios

### Ejercicio 1 — String manual

```c
// Construir el string "HELLO" char por char en un array.
// NO usar un string literal — asignar cada char individualmente.
// No olvidar el terminador '\0'.
// Imprimir con printf("%s") y verificar que funciona.
// ¿Qué pasa si se omite el '\0'?
```

### Ejercicio 2 — my_strlen

```c
// Implementar size_t my_strlen(const char *s) sin usar string.h.
// Recorrer el string hasta encontrar '\0' y contar.
// Probar con: "", "a", "hello", "hello\0world".
// Comparar resultados con strlen().
```

### Ejercicio 3 — Leer líneas con fgets

```c
// Leer líneas de stdin con fgets hasta EOF.
// Quitar el '\n' de cada línea con strcspn.
// Imprimir cada línea con su longitud.
// Probar redirigiendo un archivo: ./program < input.txt
```
