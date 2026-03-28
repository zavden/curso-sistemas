# T01 — El modelo de strings en C

> Sin erratas detectadas en el material base.

---

## 1. Null-terminated strings

En C no existe un tipo `string`. Un string es un **array de `char` terminado en `\0`** (null terminator, byte con valor 0):

```c
// Tres formas equivalentes:
char s1[] = "hello";                              // string literal
char s2[] = {'h', 'e', 'l', 'l', 'o', '\0'};     // char por char
char s3[6] = {'h', 'e', 'l', 'l', 'o', '\0'};    // tamaño explícito

// En memoria: [104][101][108][108][111][0]
//               h    e    l    l    o   \0
```

El terminador `'\0'` es el byte 0 (no el carácter `'0'` que es 48):

```c
'\0' == 0     // true — byte nulo
'0'  == 48    // true — carácter ASCII '0'
```

**Toda** función de string (`strlen`, `printf %s`, `strcpy`, `strcmp`, etc.) recorre el array byte por byte hasta encontrar `'\0'`. Si no hay `'\0'`, la función sigue leyendo más allá del buffer → **UB**.

---

## 2. `sizeof` vs `strlen`

```c
char s[] = "hello";

sizeof(s);    // 6 — tamaño del array (5 chars + '\0')
strlen(s);    // 5 — caracteres antes de '\0' (sin incluirlo)
```

`sizeof` incluye el terminador. `strlen` no. Confundirlos es fuente de **off-by-one errors**:

```c
char buf[10];
// Espacio para 9 caracteres + '\0':
//   sizeof(buf) = 10
//   Máximo strlen útil = 9
```

`sizeof` se evalúa en compilación (para arrays de tamaño fijo). `strlen` se evalúa en runtime — recorre todo el string, O(n).

---

## 3. Puntero vs array para strings

Hay dos formas de tener un string — son **fundamentalmente diferentes**:

### `char arr[]` — copia modificable en el stack

```c
char arr[] = "hello";
// arr es un array de 6 chars en el stack
// Se puede modificar:
arr[0] = 'H';         // OK → "Hello"
// No se puede reasignar:
// arr = "world";     // ERROR — array no es reasignable
sizeof(arr);           // 6 (tamaño del array)
```

### `const char *ptr` — puntero a string literal en `.rodata`

```c
const char *ptr = "hello";
// ptr apunta a un string literal en memoria de solo lectura
// NO se puede modificar el contenido:
// ptr[0] = 'H';     // UB — segfault en la mayoría de sistemas
// Sí se puede reasignar el puntero:
ptr = "world";        // OK — apunta a otro literal
sizeof(ptr);           // 8 (tamaño del puntero, no del string)
```

### Resumen

```
char arr[] = "hello";          const char *ptr = "hello";

Stack:                         Stack:            .rodata:
+---+---+---+---+---+---+     +--------+        +---+---+---+---+---+---+
| h | e | l | l | o |\0 |     | 0x...  |------->| h | e | l | l | o |\0 |
+---+---+---+---+---+---+     +--------+        +---+---+---+---+---+---+
  arr (6 bytes, modificable)     ptr (8 bytes)     (solo lectura)
```

| Aspecto | `char arr[]` | `const char *ptr` |
|---------|-------------|-------------------|
| Ubicación datos | Stack (copia) | `.rodata` (solo lectura) |
| Modificable | Sí | No (UB) |
| sizeof | Tamaño del array | Tamaño del puntero (8) |
| Reasignable | No | Sí |
| Uso típico | Buffer modificable | String constante |

Nota técnica: en C, el tipo de un string literal es `char[]` (no `const`), pero modificarlo es UB. Siempre usar `const char *` para apuntar a string literals.

---

## 4. Por qué null-terminated es peligroso

### El terminador se puede perder

```c
char buf[5] = {'h', 'e', 'l', 'l', 'o'};  // NO hay '\0'
printf("%s\n", buf);    // UB — lee más allá del buffer
strlen(buf);            // UB — no encuentra '\0'
```

### La longitud es O(n)

```c
size_t len = strlen(s);
// Recorre TODOS los caracteres hasta '\0'
// Para 1 millón de chars → 1 millón de bytes recorridos
// Si necesitas la longitud repetidamente, guardarla en una variable
```

### No puede contener `'\0'` dentro del string

```c
char s[] = "hello\0world";
printf("%s\n", s);        // "hello" — printf para en el primer \0
strlen(s);                // 5
sizeof(s);                // 12 — el compilador sí sabe el tamaño real
```

Para datos binarios con bytes 0, usar `memcpy`/`memcmp` con longitud explícita.

### Buffer overflow

```c
char buf[10];
strcpy(buf, "this string is way too long for the buffer");
// Escribe más allá de buf → UB → posible exploit
```

Las funciones de string no conocen el tamaño del buffer destino. Solo buscan `'\0'` en el source.

---

## 5. Strings en memoria

Un programa C tiene strings en cuatro ubicaciones posibles:

```c
// 1. String literals → .rodata (solo lectura, toda la ejecución):
const char *msg = "hello";

// 2. Arrays en stack → automáticos (se destruyen al salir del scope):
void foo(void) {
    char buf[100] = "hello";
}

// 3. Arrays static → .data o .bss (toda la ejecución, modificables):
static char config[256] = "default";

// 4. Heap → malloc (hasta free):
char *s = malloc(100);
strcpy(s, "hello");
// ...
free(s);
```

---

## 6. Operaciones básicas sin `string.h`

Implementar las funciones de string manualmente revela cómo funcionan internamente — todas recorren hasta `'\0'`:

### `my_strlen`

```c
size_t my_strlen(const char *s) {
    size_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}
```

### `my_strcmp`

```c
int my_strcmp(const char *a, const char *b) {
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
    // 0 si iguales, negativo si a < b, positivo si a > b
}
```

El cast a `unsigned char` es importante: `char` puede ser signed, y para la comparación correcta según el estándar se deben comparar como unsigned.

### `my_strcpy`

```c
char *my_strcpy(char *dest, const char *src) {
    char *start = dest;
    while (*src != '\0') {
        *dest++ = *src++;
    }
    *dest = '\0';
    return start;
}
```

---

## 7. Operaciones con `string.h`

```c
#include <string.h>

// Longitud:
size_t len = strlen("hello");          // 5

// Comparación (NUNCA usar == para strings):
if (strcmp(a, b) == 0) { /* iguales */ }
// == compara direcciones de punteros, no contenido

// Copia:
char dest[20];
strcpy(dest, "hello");                 // copia "hello\0"

// Concatenación:
char buf[50] = "hello";
strcat(buf, " world");                 // buf = "hello world"

// Búsqueda:
char *p = strchr("hello", 'l');        // → "llo"
char *q = strstr("hello world", "world");  // → "world"
```

---

## 8. Imprimir y leer strings

### Imprimir

```c
const char *name = "Alice";
printf("%s\n", name);        // Alice
printf("%.3s\n", name);      // Ali (máximo 3 chars)
printf("%-10s|\n", name);    // Alice     | (alineado izquierda)
printf("%10s|\n", name);     //      Alice| (alineado derecha)

puts(name);                   // Alice\n (agrega newline)
fputs(name, stdout);           // Alice (sin newline)
```

### Leer

```c
char buf[100];

// fgets — la forma SEGURA:
fgets(buf, sizeof(buf), stdin);
// Lee hasta sizeof(buf)-1 chars o hasta '\n'
// INCLUYE el '\n' en el buffer
// SIEMPRE termina con '\0'

// gets — PROHIBIDA (eliminada en C11):
// gets(buf);    // Sin límite de lectura → buffer overflow garantizado

// scanf %s — peligroso sin límite:
scanf("%99s", buf);    // OK con límite (99 chars + '\0')
// Pero %s para en whitespace — no lee líneas completas
```

### Quitar el newline de `fgets`

```c
char buf[100];
fgets(buf, sizeof(buf), stdin);

// Método 1 — strcspn (moderno, limpio):
buf[strcspn(buf, "\n")] = '\0';

// Método 2 — strchr:
char *nl = strchr(buf, '\n');
if (nl) *nl = '\0';

// Método 3 — strlen:
size_t len = strlen(buf);
if (len > 0 && buf[len - 1] == '\n')
    buf[len - 1] = '\0';
```

`strcspn` es el más robusto: si no hay `'\n'` (el input llenó el buffer), retorna la longitud del string y el `'\0'` se sobreescribe con `'\0'` — no-op segura.

---

## 9. Comparación con Rust

| Aspecto | C | Rust |
|---------|---|------|
| Representación | `char[]` + `\0` | `String` / `&str` (ptr + len) |
| Longitud | O(n) — `strlen` | O(1) — campo `len` |
| Modificable | Depende de la declaración | `String` sí, `&str` no |
| Contener `\0` | No (termina el string) | Sí (len es explícito) |
| Encoding | Bytes crudos | UTF-8 garantizado |
| Buffer overflow | Posible y común | Imposible (bounds checking) |
| Comparación | `strcmp()` | `==` directamente |

```rust
// String (heap, modificable, owned):
let mut s = String::from("hello");
s.push_str(" world");
println!("{}", s.len());   // 11 — O(1)

// &str (slice, immutable, borrowed):
let greeting: &str = "hello";
// greeting.len() es O(1) — la longitud está almacenada, no se recorre

// Rust puede contener \0:
let s = "hello\0world";
println!("{}", s.len());   // 11 — no se trunca
```

En Rust, `==` compara contenido directamente (no punteros). No hay riesgo de buffer overflow. El encoding UTF-8 está garantizado — operaciones como `.chars()` iteran correctamente sobre caracteres multibyte.

---

## Ejercicios

### Ejercicio 1 — Construir string char por char

Construye el string `"HELLO"` asignando cada carácter individualmente. No uses un string literal para la inicialización:

```c
#include <stdio.h>
#include <string.h>

int main(void) {
    char s[6];
    s[0] = 'H';
    s[1] = 'E';
    s[2] = 'L';
    s[3] = 'L';
    s[4] = 'O';
    s[5] = '\0';

    printf("String: \"%s\"\n", s);
    printf("strlen: %zu\n", strlen(s));
    printf("sizeof: %zu\n", sizeof(s));

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex1.c -o ex1
./ex1
```

<details><summary>Predicción</summary>

```
String: "HELLO"
strlen: 5
sizeof: 6
```

El string tiene 5 caracteres visibles + el terminador `'\0'`. `strlen` retorna 5 (no cuenta `'\0'`). `sizeof` retorna 6 (el tamaño total del array). Sin `s[5] = '\0'`, `printf("%s")` leería más allá del buffer buscando un byte 0 — UB.

</details>

---

### Ejercicio 2 — `my_strlen` con distintos inputs

Implementa `strlen` manualmente y pruébala con casos edge:

```c
#include <stdio.h>
#include <string.h>

size_t my_strlen(const char *s) {
    size_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

int main(void) {
    const char *tests[] = {
        "",
        "a",
        "hello",
        "hello\0world",
        "   spaces   ",
    };
    int n = (int)(sizeof(tests) / sizeof(tests[0]));

    for (int i = 0; i < n; i++) {
        size_t mine = my_strlen(tests[i]);
        size_t real = strlen(tests[i]);
        printf("my_strlen = %2zu, strlen = %2zu, match: %s  \"%s\"\n",
               mine, real, mine == real ? "yes" : "NO",
               tests[i]);
    }

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex2.c -o ex2
./ex2
```

<details><summary>Predicción</summary>

```
my_strlen =  0, strlen =  0, match: yes  ""
my_strlen =  1, strlen =  1, match: yes  "a"
my_strlen =  5, strlen =  5, match: yes  "hello"
my_strlen =  5, strlen =  5, match: yes  "hello"
my_strlen = 12, strlen = 12, match: yes  "   spaces   "
```

Todos coinciden. El caso `"hello\0world"` imprime solo `"hello"` porque `printf("%s")` para en el primer `'\0'`. Tanto `my_strlen` como `strlen` retornan 5 — el `'\0'` embebido trunca el string para todas las funciones de string.

El string vacío `""` tiene longitud 0: el primer byte ya es `'\0'`.

</details>

---

### Ejercicio 3 — `sizeof` vs `strlen` en distintos contextos

Compara `sizeof` y `strlen` para arrays, punteros y parámetros de función:

```c
#include <stdio.h>
#include <string.h>

void show_in_function(char param[]) {
    printf("  Inside function:\n");
    printf("    sizeof(param) = %zu (pointer!)\n", sizeof(param));
    printf("    strlen(param) = %zu\n", strlen(param));
}

int main(void) {
    char arr[] = "hello";
    const char *ptr = "hello";

    printf("Array:\n");
    printf("  sizeof(arr)  = %zu\n", sizeof(arr));
    printf("  strlen(arr)  = %zu\n", strlen(arr));

    printf("\nPointer:\n");
    printf("  sizeof(ptr)  = %zu\n", sizeof(ptr));
    printf("  strlen(ptr)  = %zu\n", strlen(ptr));

    printf("\nFunction parameter:\n");
    show_in_function(arr);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex3.c -o ex3
./ex3
```

<details><summary>Predicción</summary>

El compilador emite un warning en `show_in_function`:
```
warning: 'sizeof' on array function parameter 'param' will return size of 'char *'
```

Salida:
```
Array:
  sizeof(arr)  = 6
  strlen(arr)  = 5

Pointer:
  sizeof(ptr)  = 8
  strlen(ptr)  = 5

Function parameter:
  Inside function:
    sizeof(param) = 8 (pointer!)
    strlen(param) = 5
```

`sizeof(arr)` = 6 (el array real). `sizeof(ptr)` = 8 (el puntero). `sizeof(param)` = 8 (decay a puntero en la función). `strlen` siempre retorna 5 en los tres casos — mide el contenido, no el contenedor.

Moraleja: `sizeof` solo da el tamaño real del string cuando se aplica a un array local. Para todo lo demás, usar `strlen`.

</details>

---

### Ejercicio 4 — `char[]` vs `const char *`: mutabilidad

Demuestra que `char[]` se puede modificar pero `const char *` no se puede reasignar como array:

```c
#include <stdio.h>
#include <string.h>

int main(void) {
    char arr[] = "cat";
    const char *ptr = "cat";

    printf("Before:\n");
    printf("  arr = \"%s\" (sizeof=%zu)\n", arr, sizeof(arr));
    printf("  ptr = \"%s\" (sizeof=%zu)\n", ptr, sizeof(ptr));

    /* Modify array content: */
    arr[0] = 'b';
    arr[1] = 'a';
    arr[2] = 't';
    printf("\nAfter modifying arr:\n");
    printf("  arr = \"%s\"\n", arr);

    /* Reassign pointer: */
    ptr = "dog";
    printf("\nAfter reassigning ptr:\n");
    printf("  ptr = \"%s\"\n", ptr);

    /* Can also overwrite entire array content with strcpy: */
    strcpy(arr, "ox");
    printf("\nAfter strcpy(arr, \"ox\"):\n");
    printf("  arr = \"%s\" (strlen=%zu, sizeof still %zu)\n",
           arr, strlen(arr), sizeof(arr));

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex4.c -o ex4
./ex4
```

<details><summary>Predicción</summary>

```
Before:
  arr = "cat" (sizeof=4)
  ptr = "cat" (sizeof=8)

After modifying arr:
  arr = "bat"

After reassigning ptr:
  ptr = "dog"

After strcpy(arr, "ox"):
  arr = "ox" (strlen=2, sizeof still 4)
```

`arr` tiene `sizeof` = 4 (3 chars + `'\0'`). Modificar `arr[0..2]` cambia "cat" a "bat". `ptr` se reasigna a otro string literal — ahora apunta a `"dog"` en `.rodata`.

Después de `strcpy(arr, "ox")`: `arr` contiene `{'o','x','\0','t'}`. `strlen` = 2, pero `sizeof` sigue siendo 4 (el tamaño del array no cambia). El byte `'t'` queda en `arr[3]` pero es invisible porque `'\0'` está en `arr[2]`.

</details>

---

### Ejercicio 5 — `my_strcmp`: comparar strings

Implementa `strcmp` manualmente y prueba con varios pares:

```c
#include <stdio.h>
#include <string.h>

int my_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int main(void) {
    struct { const char *a; const char *b; } tests[] = {
        {"abc",   "abc"},
        {"abc",   "abd"},
        {"abd",   "abc"},
        {"ab",    "abc"},
        {"abc",   "ab"},
        {"",      ""},
        {"",      "a"},
        {"hello", "hello"},
    };
    int n = (int)(sizeof(tests) / sizeof(tests[0]));

    for (int i = 0; i < n; i++) {
        int mine = my_strcmp(tests[i].a, tests[i].b);
        int real = strcmp(tests[i].a, tests[i].b);

        printf("cmp(\"%s\", \"%s\") = %4d (stdlib: %4d) %s\n",
               tests[i].a, tests[i].b, mine, real,
               ((mine == 0) == (real == 0) &&
                (mine > 0) == (real > 0) &&
                (mine < 0) == (real < 0)) ? "OK" : "MISMATCH");
    }

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex5.c -o ex5
./ex5
```

<details><summary>Predicción</summary>

```
cmp("abc", "abc") =    0 (stdlib:    0) OK
cmp("abc", "abd") =   -1 (stdlib:   -1) OK
cmp("abd", "abc") =    1 (stdlib:    1) OK
cmp("ab",  "abc") =  -99 (stdlib:  -99) OK
cmp("abc", "ab")  =   99 (stdlib:   99) OK
cmp("",    "")    =    0 (stdlib:    0) OK
cmp("",    "a")   =  -97 (stdlib:  -97) OK
cmp("hello", "hello") = 0 (stdlib:  0) OK
```

`my_strcmp("ab", "abc")`: tras comparar `'a'=='a'` y `'b'=='b'`, compara `'\0'` (0) con `'c'` (99) → `0 - 99 = -99`. El signo importa más que el valor exacto: negativo = `a < b`, cero = iguales, positivo = `a > b`.

El cast a `unsigned char` es esencial: si `char` es signed y hay caracteres con valores > 127, la resta podría dar resultados incorrectos sin el cast.

Nota: el valor exacto de `strcmp` de la stdlib puede variar (la especificación solo garantiza el signo), pero GCC típicamente retorna la diferencia de los chars.

</details>

---

### Ejercicio 6 — `my_strcpy` y buffer

Implementa `strcpy` manualmente y demuestra que copia incluyendo `'\0'`:

```c
#include <stdio.h>
#include <string.h>

char *my_strcpy(char *dest, const char *src) {
    char *start = dest;
    while ((*dest++ = *src++) != '\0')
        ;
    return start;
}

int main(void) {
    char buf[20] = "initial_garbage!!";
    printf("Before: \"%s\" (strlen=%zu)\n", buf, strlen(buf));

    my_strcpy(buf, "hello");
    printf("After:  \"%s\" (strlen=%zu)\n", buf, strlen(buf));

    /* Show remaining bytes to prove \0 terminates: */
    printf("Bytes:  ");
    for (int i = 0; i < 12; i++) {
        printf("[%d]=%d ", i, buf[i]);
    }
    printf("\n");

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex6.c -o ex6
./ex6
```

<details><summary>Predicción</summary>

```
Before: "initial_garbage!!" (strlen=17)
After:  "hello" (strlen=5)
Bytes:  [0]=104 [1]=101 [2]=108 [3]=108 [4]=111 [5]=0 [6]=95 [7]=103 [8]=97 [9]=114 [10]=98 [11]=97
```

Después de `my_strcpy(buf, "hello")`, los primeros 6 bytes son `h,e,l,l,o,\0`. Los bytes restantes del buffer aún contienen basura del string original (`_garba...`), pero son invisibles porque `'\0'` está en posición 5. `strlen` retorna 5.

La versión compacta `while ((*dest++ = *src++) != '\0')` copia un byte y avanza ambos punteros en una sola expresión. Cuando copia `'\0'`, la condición es falsa y el loop termina — el terminador queda copiado.

</details>

---

### Ejercicio 7 — Comparar con `==` vs `strcmp`

Demuestra por qué `==` no funciona para comparar strings:

```c
#include <stdio.h>
#include <string.h>

int main(void) {
    char a[] = "hello";
    char b[] = "hello";
    const char *p = "hello";
    const char *q = "hello";

    printf("Arrays with same content:\n");
    printf("  a == b:          %s\n", (a == b) ? "true" : "false");
    printf("  strcmp(a,b) == 0: %s\n", strcmp(a, b) == 0 ? "true" : "false");

    printf("\nPointers to same literal:\n");
    printf("  p == q:          %s\n", (p == q) ? "true" : "false");
    printf("  strcmp(p,q) == 0: %s\n", strcmp(p, q) == 0 ? "true" : "false");

    printf("\nArray vs pointer:\n");
    printf("  a == p:          %s\n", (a == p) ? "true" : "false");
    printf("  strcmp(a,p) == 0: %s\n", strcmp(a, p) == 0 ? "true" : "false");

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex7.c -o ex7
./ex7
```

<details><summary>Predicción</summary>

```
Arrays with same content:
  a == b:          false
  strcmp(a,b) == 0: true

Pointers to same literal:
  p == q:          true
  strcmp(p,q) == 0: true

Array vs pointer:
  a == p:          false
  strcmp(a,p) == 0: true
```

`a == b` es `false`: son dos arrays distintos en el stack con direcciones diferentes. `strcmp` compara contenido → true.

`p == q` es `true` (probablemente): el compilador puede optimizar colocando ambos string literals en la misma dirección (string pooling). Pero esto **no está garantizado** — depende del compilador y flags. Nunca confiar en `==` para comparar strings.

`a == p` es `false`: `a` está en el stack, `p` apunta a `.rodata`. Direcciones diferentes.

Moraleja: siempre usar `strcmp` para comparar contenido de strings.

</details>

---

### Ejercicio 8 — Null embebido trunca el string

Demuestra que `'\0'` embebido corta el string para todas las funciones de string:

```c
#include <stdio.h>
#include <string.h>

int main(void) {
    char s[] = "hello\0world";

    printf("printf: \"%s\"\n", s);
    printf("strlen: %zu\n", strlen(s));
    printf("sizeof: %zu\n", sizeof(s));

    printf("\nAll bytes:\n");
    for (size_t i = 0; i < sizeof(s); i++) {
        printf("  [%2zu] = %3d ('%c')\n",
               i, (unsigned char)s[i],
               s[i] >= 32 ? s[i] : '.');
    }

    /* memcmp sees all bytes: */
    char t[] = "hello\0WORLD";
    printf("\nstrcmp(s, t): %d (only sees \"hello\")\n", strcmp(s, t));
    printf("memcmp(s, t, sizeof(s)): %d (sees all bytes)\n",
           memcmp(s, t, sizeof(s) < sizeof(t) ? sizeof(s) : sizeof(t)));

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex8.c -o ex8
./ex8
```

<details><summary>Predicción</summary>

```
printf: "hello"
strlen: 5
sizeof: 12

All bytes:
  [ 0] = 104 ('h')
  [ 1] = 101 ('e')
  [ 2] = 108 ('l')
  [ 3] = 108 ('l')
  [ 4] = 111 ('o')
  [ 5] =   0 ('.')
  [ 6] = 119 ('w')
  [ 7] = 111 ('o')
  [ 8] = 114 ('r')
  [ 9] = 108 ('l')
  [10] = 100 ('d')
  [11] =   0 ('.')

strcmp(s, t): 0 (only sees "hello")
memcmp(s, t, sizeof(s)): <nonzero> (sees all bytes)
```

`printf` y `strlen` solo ven `"hello"` (5 chars). `sizeof` reporta 12 (11 chars + `'\0'` final del compilador). Los bytes después del `'\0'` embebido (`world`) existen en memoria pero son invisibles para funciones de string.

`strcmp(s, t)` retorna 0: ambos tienen `"hello\0..."` y la comparación para en el primer `'\0'`. `memcmp` compara **todos** los bytes incluyendo los que están después del `'\0'` — detecta que `'w'` ≠ `'W'`.

</details>

---

### Ejercicio 9 — Leer con `fgets` y quitar newline

Lee líneas de stdin con `fgets` y quita el `'\n'`:

```c
#include <stdio.h>
#include <string.h>

int main(void) {
    char buf[50];

    printf("Enter a line: ");
    if (fgets(buf, sizeof(buf), stdin)) {
        printf("Raw:     \"%s\"\n", buf);
        printf("strlen:  %zu\n", strlen(buf));

        /* Check if newline is present: */
        char *nl = strchr(buf, '\n');
        printf("Has \\n:  %s\n", nl ? "yes" : "no");

        /* Remove it with strcspn: */
        buf[strcspn(buf, "\n")] = '\0';
        printf("Clean:   \"%s\"\n", buf);
        printf("strlen:  %zu\n", strlen(buf));
    }

    return 0;
}
```

```bash
echo "hello" | gcc -std=c11 -Wall -Wextra -Wpedantic ex9.c -o ex9 && echo "hello" | ./ex9
```

<details><summary>Predicción</summary>

```
Enter a line: Raw:     "hello
"
strlen:  6
Has \n:  yes
Clean:   "hello"
strlen:  5
```

`fgets` lee `"hello\n"` — incluye el `'\n'`. `strlen` = 6 (5 chars + `'\n'`). Después de `buf[strcspn(buf, "\n")] = '\0'`, el `'\n'` se reemplaza por `'\0'`. `strlen` = 5.

`strcspn(buf, "\n")` retorna 5 (el índice del primer `'\n'`). Si no hubiera `'\n'` (input más largo que el buffer), retornaría la longitud del string — se sobreescribiría `'\0'` con `'\0'` (no-op segura).

</details>

---

### Ejercicio 10 — Strings en las cuatro ubicaciones de memoria

Demuestra strings en stack, `.rodata`, `static` y heap:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char static_buf[20] = "static";

int main(void) {
    /* 1. String literal in .rodata */
    const char *literal = "rodata";

    /* 2. Array on stack */
    char stack_buf[20] = "stack";

    /* 3. Static storage */
    /* static_buf already declared above */

    /* 4. Heap */
    char *heap_buf = malloc(20);
    if (!heap_buf) return 1;
    strcpy(heap_buf, "heap");

    printf("%-10s addr: %p  value: \"%s\"\n", "literal", (void *)literal, literal);
    printf("%-10s addr: %p  value: \"%s\"\n", "stack",   (void *)stack_buf, stack_buf);
    printf("%-10s addr: %p  value: \"%s\"\n", "static",  (void *)static_buf, static_buf);
    printf("%-10s addr: %p  value: \"%s\"\n", "heap",    (void *)heap_buf, heap_buf);

    /* Modify stack, static, and heap (but not literal): */
    stack_buf[0] = 'S';
    static_buf[0] = 'S';
    heap_buf[0] = 'H';

    printf("\nAfter modification:\n");
    printf("  stack:  \"%s\"\n", stack_buf);
    printf("  static: \"%s\"\n", static_buf);
    printf("  heap:   \"%s\"\n", heap_buf);

    free(heap_buf);
    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex10.c -o ex10
./ex10
```

<details><summary>Predicción</summary>

```
literal    addr: 0x40...   value: "rodata"
stack      addr: 0x7fff... value: "stack"
static     addr: 0x60...   value: "static"
heap       addr: 0x...     value: "heap"

After modification:
  stack:  "Stack"
  static: "Static"
  heap:   "Heap"
```

Las cuatro direcciones están en rangos diferentes:
- `literal` → dirección baja (~0x40xxxx) en la sección `.rodata` del binario.
- `stack_buf` → dirección alta (~0x7fffxxxx) en el stack.
- `static_buf` → dirección intermedia (~0x60xxxx) en `.data`.
- `heap_buf` → dirección de heap (varía).

Los tres buffers modificables (`stack`, `static`, `heap`) se modifican correctamente. `literal` no se modifica — hacerlo sería UB. `free(heap_buf)` es obligatorio para evitar memory leak.

</details>
