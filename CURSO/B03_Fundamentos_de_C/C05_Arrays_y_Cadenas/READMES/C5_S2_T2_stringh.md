# T02 — string.h

## Erratas detectadas en el material base

| Archivo | Línea | Error | Corrección |
|---------|-------|-------|------------|
| `README.md` | 273 | `memmove(buf + 2, buf, 5)` dice resultado `"hehello rld"` | El resultado correcto es `"hehelloorld"` — la posición 5 (espacio original) se sobreescribe con `'l'` y la posición 6 (`'w'`) con `'o'` |
| `labs/README.md` | 388 | `strcspn(buf, "\n") = '\0'` | Falta indexar el array: `buf[strcspn(buf, "\n")] = '\0'` |

---

## 1 — Funciones de longitud: strlen y strnlen

```c
#include <string.h>

size_t strlen(const char *s);
```

`strlen` recorre byte por byte hasta encontrar `'\0'` y retorna la cantidad de caracteres **sin contar el terminador**. Es O(n) — no hay forma de saber la longitud sin recorrer.

```c
strlen("hello");        // 5
strlen("");             // 0
strlen("abc\0def");     // 3 — para en el primer '\0'
```

Si `s` no tiene `'\0'`: **comportamiento indefinido** — `strlen` sigue leyendo memoria hasta encontrar un byte cero o provocar un fallo.

```c
// strnlen — strlen con límite (POSIX, no C estándar):
size_t strnlen(const char *s, size_t maxlen);

strnlen("hello", 100);   // 5
strnlen("hello", 3);     // 3 — no busca más allá de maxlen
```

`strnlen` es útil cuando no hay garantía de que el buffer tenga `'\0'` — nunca lee más de `maxlen` bytes.

---

## 2 — Funciones de copia: strcpy, strncpy, strlcpy, snprintf

### strcpy — copia sin verificar tamaño

```c
char *strcpy(char *dest, const char *src);

char dest[50];
strcpy(dest, "hello");    // dest = "hello\0"
// Retorna dest

// PELIGRO: si strlen(src) >= sizeof(dest) → buffer overflow:
char small[5];
strcpy(small, "this is way too long");  // UB
```

### strncpy — NO es la versión segura de strcpy

`strncpy` fue diseñada para campos de tamaño fijo en Unix v7 (nombres de archivos en directorios de 14 bytes), **no para copia segura de strings**.

```c
char *strncpy(char *dest, const char *src, size_t n);
```

Tiene dos trampas:

```c
// Trampa 1: si strlen(src) >= n, NO agrega '\0':
char buf[5];
strncpy(buf, "hello world", 5);
// buf = {'h','e','l','l','o'} — SIN terminador
// buf NO es un string válido

// Trampa 2: si src es corto, rellena TODO el resto con '\0':
char buf2[1000];
strncpy(buf2, "hi", 1000);
// Copia "hi" y escribe 998 bytes de '\0' — desperdicio
```

Si debes usar `strncpy`, fuerza siempre el terminador:

```c
strncpy(dest, src, sizeof(dest) - 1);
dest[sizeof(dest) - 1] = '\0';
```

### strlcpy — la versión segura (BSD/POSIX, no estándar C)

```c
size_t strlcpy(char *dest, const char *src, size_t size);

char dest[10];
strlcpy(dest, "hello world", sizeof(dest));
// dest = "hello wor\0" — truncado pero siempre terminado
// Retorna strlen(src) = 11
// Si retorno >= size → hubo truncación
```

Disponible en BSD y glibc 2.38+. No es estándar C.

### snprintf — la alternativa portátil recomendada

```c
char dest[10];
int n = snprintf(dest, sizeof(dest), "%s", "hello wonderful world");
// dest = "hello won\0" — siempre terminado
// n = 21 — lo que habría escrito sin límite
// Si n >= sizeof(dest) → hubo truncación
```

`snprintf` es **siempre seguro**: termina con `'\0'`, nunca desborda, y permite detectar truncación.

---

## 3 — Funciones de concatenación: strcat y strncat

### strcat — concatena sin verificar tamaño

```c
char *strcat(char *dest, const char *src);

char buf[50] = "hello";
strcat(buf, " ");       // "hello "
strcat(buf, "world");   // "hello world"
```

**Peligro 1**: no verifica tamaño de `dest` — buffer overflow si no cabe.

**Peligro 2**: rendimiento O(n+m) por llamada — busca el `'\0'` de `dest` cada vez. Concatenar en un loop es O(n²) ("Shlemiel the painter"):

```c
char result[1000] = "";
for (int i = 0; i < 100; i++) {
    strcat(result, "x");  // cada vez recorre TODO result
}
// Solución: mantener un puntero al final o usar snprintf
```

### strncat — limita chars de src (NO el tamaño de dest)

```c
char *strncat(char *dest, const char *src, size_t n);

char buf[20] = "hello";
strncat(buf, " wonderful world", 6);
// Copia 6 chars de src: " wonde", luego agrega '\0'
// buf = "hello wonde\0"
```

A diferencia de `strncpy`, `strncat` **siempre** agrega `'\0'`. Pero cuidado: `n` limita los caracteres de `src`, **no** el tamaño total de `dest`. Para limitar por tamaño de dest:

```c
strncat(buf, src, sizeof(buf) - strlen(buf) - 1);
```

---

## 4 — Funciones de comparación: strcmp, strncmp, strcasecmp

### strcmp — comparación lexicográfica

```c
int strcmp(const char *s1, const char *s2);

// Retorna:
//   < 0  si s1 < s2
//   = 0  si s1 == s2
//   > 0  si s1 > s2

strcmp("abc", "abc");     // 0
strcmp("abc", "abd");     // < 0 — 'c' < 'd'
strcmp("abd", "abc");     // > 0 — 'd' > 'c'
strcmp("abc", "abcd");    // < 0 — "abc" es prefijo, más corto
```

Los valores exactos (`-1`, `1`, `-4`) dependen de la implementación — el estándar C solo garantiza el **signo** del resultado.

**Error clásico** — usar `==` para comparar strings:

```c
const char *a = "hello";
char buf[10];
strcpy(buf, "hello");
if (a == buf)           // FALSO — compara direcciones
if (strcmp(a, buf) == 0) // VERDADERO — compara contenido
```

### strncmp — compara máximo n caracteres

```c
strncmp("hello", "help", 3);   // 0 — primeros 3 iguales
strncmp("hello", "help", 4);   // < 0 — 'l' < 'p'

// Uso típico: verificar prefijos
if (strncmp(line, "ERROR:", 6) == 0) {
    // line empieza con "ERROR:"
}
```

### strcasecmp — comparación sin distinguir mayúsculas (POSIX)

```c
#include <strings.h>  // nota: strings.h con 's' al final

strcasecmp("Hello", "HELLO");     // 0
strncasecmp("Hello", "HELP", 3);  // 0
```

---

## 5 — Funciones de búsqueda: strchr, strstr, strcspn, strspn

### strchr y strrchr — buscar un carácter

```c
char *strchr(const char *s, int c);   // primera ocurrencia
char *strrchr(const char *s, int c);  // última ocurrencia

char *p = strchr("hello world", 'o');
// p apunta a "o world", índice 4

char *q = strrchr("hello world", 'o');
// q apunta a "orld", índice 7

strchr("hello", 'z');     // NULL — no encontrado

// strchr con '\0' retorna el terminador:
char *end = strchr("hello", '\0');  // apunta al '\0' final
```

### strstr — buscar un substring

```c
char *strstr(const char *haystack, const char *needle);

strstr("hello world", "world");  // apunta a "world"
strstr("hello", "xyz");          // NULL
strstr("hello", "");             // "hello" — vacío siempre se encuentra
```

### strpbrk — buscar cualquier carácter de un conjunto

```c
char *q = strpbrk("hello, world!", " ,!");
// q apunta a "," — primera ocurrencia de ' ', ',' o '!'
```

### strcspn y strspn — longitud de segmentos

```c
// strcspn: longitud del segmento ANTES del primer char del conjunto
strcspn("hello, world", " ,");   // 5 — "hello" antes de ','

// strspn: longitud del segmento que SÍ contiene chars del conjunto
strspn("   hello", " ");         // 3 — tres espacios al inicio
```

Patrón idiomático para eliminar newline de `fgets`:

```c
buf[strcspn(buf, "\n")] = '\0';
```

Patrón para saltar whitespace:

```c
const char *start = s + strspn(s, " \t\n");
```

---

## 6 — Funciones de memoria (mem*)

Las funciones `mem*` operan con **bloques de bytes**, no strings. No buscan `'\0'` — usan un parámetro de longitud explícito. Están declaradas en `<string.h>`.

### memcpy — copia n bytes (sin solapamiento)

```c
void *memcpy(void *dest, const void *src, size_t n);

int a[5] = {1, 2, 3, 4, 5};
int b[5];
memcpy(b, a, sizeof(a));   // copia 20 bytes — b = {1,2,3,4,5}

// Funciona con CUALQUIER tipo, no solo strings
// Si src y dest se solapan: UB → usar memmove
```

### memmove — copia n bytes (soporta solapamiento)

```c
void *memmove(void *dest, const void *src, size_t n);

char buf[] = "hello world";
memmove(buf + 2, buf, 5);
// Copia buf[0..4] ("hello") a buf[2..6]
// Resultado: "hehelloorld"
// buf[5] (espacio) → 'l', buf[6] ('w') → 'o'
```

`memmove` maneja el solapamiento copiando en la dirección correcta (de atrás hacia adelante si es necesario). Es ligeramente más lento que `memcpy` por esa verificación.

### memset — llena n bytes con un valor

```c
void *memset(void *s, int c, size_t n);

char buf[100];
memset(buf, 0, sizeof(buf));    // todo a 0
memset(buf, 'A', 10);           // primeros 10 bytes a 'A'
```

**Trampa con enteros**: `c` se convierte a `unsigned char` (1 byte):

```c
int arr[3];
memset(arr, 0, sizeof(arr));    // OK — 0x00000000 == 0
memset(arr, 1, sizeof(arr));    // ¡NO es {1, 1, 1}!
// Cada BYTE se pone a 0x01: cada int = 0x01010101 = 16843009
// Para poner ints a un valor específico ≠ 0, usar un loop
```

`memset` a 0 funciona porque todos los bytes en cero representan 0 para `int`. Cualquier otro valor produce resultados inesperados.

### memcmp — compara n bytes

```c
int memcmp(const void *s1, const void *s2, size_t n);

int a[3] = {1, 2, 3};
int b[3] = {1, 2, 3};
int c[3] = {1, 2, 4};

memcmp(a, b, sizeof(a));   // 0 — iguales
memcmp(a, c, sizeof(a));   // < 0 — difieren en el tercer int
```

**Cuidado con structs**: `memcmp` compara bytes de padding, que son indeterminados. Para comparar structs, comparar campo por campo.

---

## 7 — strtok: tokenizar strings

```c
char *strtok(char *str, const char *delim);

char line[] = "hello,world,foo,bar";
char *token = strtok(line, ",");
while (token != NULL) {
    printf("Token: '%s'\n", token);
    token = strtok(NULL, ",");     // NULL para continuar
}
// Token: 'hello'
// Token: 'world'
// Token: 'foo'
// Token: 'bar'
```

Cuatro advertencias sobre `strtok`:

1. **Modifica el string original** — reemplaza delimitadores con `'\0'`
2. **Usa estado global (`static`)** — NO es thread-safe
3. **No se pueden tokenizar dos strings a la vez**
4. **No se puede usar con string literals** — son de solo lectura

Alternativa thread-safe:

```c
// strtok_r (POSIX) — estado explícito:
char *saveptr;
char *token = strtok_r(line, ",", &saveptr);
while (token != NULL) {
    token = strtok_r(NULL, ",", &saveptr);
}
```

---

## 8 — Patrones seguros con snprintf

### Construcción de strings

```c
char buf[100];
int n = snprintf(buf, sizeof(buf), "Name: %s, Age: %d", name, age);
if (n >= (int)sizeof(buf)) {
    // Truncado — n es lo que habría escrito
}
```

### Concatenación acumulativa

```c
char result[100];
int pos = 0;
pos += snprintf(result + pos, sizeof(result) - pos, "Hello ");
pos += snprintf(result + pos, sizeof(result) - pos, "%s", name);
pos += snprintf(result + pos, sizeof(result) - pos, "!");
```

Este patrón mantiene un cursor `pos` que avanza con cada escritura. `snprintf` siempre termina con `'\0'` y nunca desborda. Si hay truncación, `pos` acumula el total que se habría necesitado.

**Precaución**: si `pos >= sizeof(result)`, la expresión `sizeof(result) - pos` produce underflow de `size_t` (unsigned). En código robusto, agregar una guarda:

```c
if (pos < (int)sizeof(result)) {
    pos += snprintf(result + pos, sizeof(result) - pos, "...");
}
```

### Tabla comparativa de funciones de copia

| Función | Para | Agrega `'\0'` | Segura | Recomendada |
|---------|------|:---:|:---:|:---:|
| `strcpy` | Strings | Sí | No (overflow) | No |
| `strncpy` | Campos fijos | Solo si src < n | Parcial | No |
| `strlcpy` | Strings | Sí | Sí | Sí (si disponible) |
| `snprintf` | Strings | Sí | Sí | Sí (portátil) |
| `memcpy` | Bytes | No busca `'\0'` | N/A (usa n) | Sí (para datos) |

---

## 9 — Comparación con Rust

| Aspecto | C (`string.h`) | Rust |
|---------|----------------|------|
| Longitud | `strlen()` — O(n), recorre cada vez | `s.len()` — O(1), almacenado en el tipo |
| Copia | `strcpy`/`strncpy` (inseguras) | `s.clone()` o `.to_string()` (sin overflow posible) |
| Concatenación | `strcat` (O(n²) en loop) | `format!()`, `push_str()`, `String + &str` |
| Comparación | `strcmp()` (olvidar `== 0` es bug) | `==` opera sobre contenido directamente |
| Búsqueda | `strchr`/`strstr` retornan `NULL` | `.find()` retorna `Option<usize>` |
| Tokenización | `strtok` (modifica, no thread-safe) | `.split()` retorna iterador (no modifica) |
| Seguridad | Responsabilidad del programador | El compilador garantiza bounds checking |

```rust
// Equivalentes en Rust
let s = String::from("hello world");

s.len();                        // O(1), no O(n)
s.contains("world");            // como strstr != NULL
s.starts_with("hello");         // como strncmp == 0
s.find('o');                    // Some(4) — como strchr
s.rfind('o');                   // Some(7) — como strrchr
let tokens: Vec<&str> = s.split(',').collect();  // como strtok

// Concatenación sin riesgo de overflow
let result = format!("{} {}", "hello", "world");
```

En Rust, `&str` y `String` almacenan la longitud como parte del tipo (fat pointer / struct con len), eliminando las búsquedas O(n) de `'\0'` y la posibilidad de buffer overflow.

---

## Ejercicios

### Ejercicio 1 — strlen vs sizeof

```c
// ¿Qué imprime este programa?
#include <stdio.h>
#include <string.h>

int main(void) {
    char a[] = "hello";
    const char *b = "hello";
    char c[20] = "hello";
    char d[] = "abc\0def";

    printf("a: sizeof=%zu strlen=%zu\n", sizeof(a), strlen(a));
    printf("b: sizeof=%zu strlen=%zu\n", sizeof(b), strlen(b));
    printf("c: sizeof=%zu strlen=%zu\n", sizeof(c), strlen(c));
    printf("d: sizeof=%zu strlen=%zu\n", sizeof(d), strlen(d));
    return 0;
}
```

<details><summary>Predicción</summary>

```
a: sizeof=6 strlen=5
b: sizeof=8 strlen=5
c: sizeof=20 strlen=5
d: sizeof=8 strlen=3
```

- `a`: array de 6 bytes (`"hello\0"`), strlen cuenta 5
- `b`: puntero → sizeof es 8 bytes en x86-64 (tamaño del puntero, no del string)
- `c`: array de 20 bytes, pero solo "hello" ocupa 5 — sizeof es el array completo
- `d`: literal `"abc\0def"` ocupa 8 bytes en memoria (`abc\0def\0`), pero `strlen` para en el primer `'\0'` → 3

</details>

### Ejercicio 2 — Trampas de strncpy

```c
// ¿Qué imprime este programa? ¿Hay algún problema?
#include <stdio.h>
#include <string.h>

int main(void) {
    char buf[5];
    strncpy(buf, "Hello, world!", 5);
    printf("buf = \"%s\"\n", buf);

    char buf2[10];
    strncpy(buf2, "Hi", sizeof(buf2));
    printf("buf2 bytes: ");
    for (size_t i = 0; i < sizeof(buf2); i++) {
        printf("%02x ", (unsigned char)buf2[i]);
    }
    printf("\n");
    return 0;
}
```

<details><summary>Predicción</summary>

El primer `printf` produce **comportamiento indefinido**: `strncpy` copió 5 bytes (`H`,`e`,`l`,`l`,`o`) sin `'\0'`. `printf("%s")` sigue leyendo más allá del buffer buscando un terminador. La salida es impredecible — podría imprimir basura o crashear.

El segundo `printf` muestra: `48 69 00 00 00 00 00 00 00 00` — `'H'`=0x48, `'i'`=0x69, y 8 bytes de zero-padding. `strncpy` rellena todo el resto del buffer con `'\0'`.

</details>

### Ejercicio 3 — strcmp y el signo

```c
// ¿Qué imprime? ¿Los valores exactos están garantizados?
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("%d\n", strcmp("apple", "banana"));
    printf("%d\n", strcmp("banana", "apple"));
    printf("%d\n", strcmp("cat", "category"));
    printf("%d\n", strcmp("", "anything"));
    printf("%d\n", strcmp("ABC", "abc"));
    return 0;
}
```

<details><summary>Predicción</summary>

```
-1       (o cualquier valor < 0)
1        (o cualquier valor > 0)
-1       (o cualquier valor < 0)
-1       (o cualquier valor < 0)
-1       (o cualquier valor < 0)
```

- `"apple"` < `"banana"` → negativo (primer char `'a'` < `'b'`)
- `"banana"` > `"apple"` → positivo
- `"cat"` < `"category"` → negativo (`"cat"` es prefijo, más corto)
- `""` < `"anything"` → negativo (string vacío es menor que cualquier otro)
- `"ABC"` < `"abc"` → negativo (`'A'`=65 < `'a'`=97 en ASCII)

Los valores exactos (`-1`, `1`, `-32`) **no están garantizados** por el estándar — solo el signo. El código correcto usa `< 0`, `== 0`, `> 0`, nunca `== -1`.

</details>

### Ejercicio 4 — strchr y aritmética de punteros

```c
// ¿Qué imprime?
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *path = "/home/user/documents/file.txt";

    char *last_slash = strrchr(path, '/');
    if (last_slash) {
        printf("Directory: %.*s\n", (int)(last_slash - path), path);
        printf("Filename: %s\n", last_slash + 1);
    }

    char *dot = strrchr(path, '.');
    if (dot) {
        printf("Extension: %s\n", dot);
    }
    return 0;
}
```

<details><summary>Predicción</summary>

```
Directory: /home/user/documents
Filename: file.txt
Extension: .txt
```

- `strrchr(path, '/')` apunta al último `'/'`, que está antes de `"file.txt"`. `last_slash - path` = longitud del directorio. `%.*s` imprime exactamente esa cantidad de chars.
- `last_slash + 1` salta el `'/'` y apunta a `"file.txt"`.
- `strrchr(path, '.')` apunta al `'.'` antes de `"txt"`. `dot` = `".txt"`.

Este es un patrón real para extraer componentes de rutas sin modificar el string original.

</details>

### Ejercicio 5 — memset trap

```c
// ¿Qué imprime? ¿Por qué no funciona como se espera?
#include <stdio.h>
#include <string.h>

int main(void) {
    int arr[4];
    memset(arr, 0, sizeof(arr));
    printf("After memset 0:  {%d, %d, %d, %d}\n",
           arr[0], arr[1], arr[2], arr[3]);

    memset(arr, 1, sizeof(arr));
    printf("After memset 1:  {%d, %d, %d, %d}\n",
           arr[0], arr[1], arr[2], arr[3]);

    memset(arr, -1, sizeof(arr));
    printf("After memset -1: {%d, %d, %d, %d}\n",
           arr[0], arr[1], arr[2], arr[3]);

    memset(arr, 0xFF, sizeof(arr));
    printf("After memset FF: {%d, %d, %d, %d}\n",
           arr[0], arr[1], arr[2], arr[3]);
    return 0;
}
```

<details><summary>Predicción</summary>

```
After memset 0:  {0, 0, 0, 0}
After memset 1:  {16843009, 16843009, 16843009, 16843009}
After memset -1: {-1, -1, -1, -1}
After memset FF: {-1, -1, -1, -1}
```

- **memset 0**: cada byte = 0x00, cada int = 0x00000000 = 0. Funciona.
- **memset 1**: cada byte = 0x01, cada int = 0x01010101 = 16843009. **No** es 1.
- **memset -1**: `-1` truncado a `unsigned char` = 0xFF. Cada byte = 0xFF, cada int = 0xFFFFFFFF = -1 (en complemento a dos). Funciona "por casualidad".
- **memset 0xFF**: idéntico al caso -1. 0xFF y -1 producen el mismo byte.

Solo `0` y `-1` (0xFF) producen resultados útiles con `memset` sobre enteros.

</details>

### Ejercicio 6 — memmove vs memcpy con solapamiento

```c
// ¿Qué imprime cada caso? ¿Cuál tiene comportamiento indefinido?
#include <stdio.h>
#include <string.h>

int main(void) {
    // Caso 1: memmove (seguro)
    char a[] = "abcdefghij";
    memmove(a + 3, a, 5);
    printf("memmove: \"%s\"\n", a);

    // Caso 2: memcpy con solapamiento (UB)
    char b[] = "abcdefghij";
    memcpy(b + 3, b, 5);
    printf("memcpy:  \"%s\"\n", b);

    // Caso 3: memmove hacia atrás
    char c[] = "abcdefghij";
    memmove(c, c + 3, 5);
    printf("memmove back: \"%s\"\n", c);
    return 0;
}
```

<details><summary>Predicción</summary>

```
memmove: "abcabcdeij"
memcpy:  (indeterminado — UB)
memmove back: "defghfghij"
```

- **Caso 1**: `memmove(a+3, a, 5)` copia `"abcde"` (posiciones 0-4) a posiciones 3-7. Resultado: `a`,`b`,`c`,`a`,`b`,`c`,`d`,`e`,`i`,`j` = `"abcabcdeij"`.
- **Caso 2**: `memcpy` con regiones solapadas es **UB**. En la práctica, muchas implementaciones de glibc usan `memmove` internamente para `memcpy`, así que puede dar el mismo resultado. Pero no está garantizado — el compilador puede optimizar asumiendo no-solapamiento.
- **Caso 3**: `memmove(c, c+3, 5)` copia `"defgh"` (posiciones 3-7) a posiciones 0-4. Resultado: `d`,`e`,`f`,`g`,`h`,`f`,`g`,`h`,`i`,`j` = `"defghfghij"`.

</details>

### Ejercicio 7 — snprintf para concatenación segura

```c
// ¿Qué imprime? ¿Qué pasa con la truncación?
#include <stdio.h>
#include <string.h>

int main(void) {
    char buf[20];
    int pos = 0;

    pos += snprintf(buf + pos, sizeof(buf) - pos, "User: ");
    pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", "administrator");
    pos += snprintf(buf + pos, sizeof(buf) - pos, " [%d]", 42);

    printf("buf = \"%s\"\n", buf);
    printf("pos = %d (needed %d bytes)\n", pos, pos + 1);
    printf("truncated = %s\n", pos >= (int)sizeof(buf) ? "yes" : "no");
    return 0;
}
```

<details><summary>Predicción</summary>

```
buf = "User: administrato"
pos = 24 (needed 25 bytes)
truncated = yes
```

- Primera llamada: `"User: "` = 6 chars, pos = 6
- Segunda llamada: `"administrator"` = 13 chars, pero solo cabe 13 chars (20-6=14, espacio para 13 + `'\0'`). pos = 6 + 13 = 19
- Tercera llamada: `" [42]"` = 5 chars, pero solo queda 1 byte (20-19=1). Solo cabe `'\0'`. snprintf retorna 5 (lo que habría escrito). pos = 19 + 5 = 24

Contenido del buffer: `"User: administrator"` son 20 chars que no caben en 20 bytes (necesitan 21 con `'\0'`). Así que se trunca a 19 chars + `'\0'` = `"User: administrato"`.

Wait, let me recalculate: "User: " = 6, "administrator" = 13, total = 19. sizeof(buf) = 20. 19 chars + '\0' = 20 bytes — cabe exactamente. pos = 19.

Tercera llamada: `sizeof(buf) - pos = 20 - 19 = 1`. Solo cabe '\0'. snprintf retorna 5. pos = 24.

buf = `"User: administrator"` (19 chars), la tercera parte `" [42]"` no cabe.

```
buf = "User: administrator"
pos = 24 (needed 25 bytes)
truncated = yes
```

</details>

### Ejercicio 8 — strtok modifica el string

```c
// ¿Qué imprime? ¿Qué le pasa al string original?
#include <stdio.h>
#include <string.h>

int main(void) {
    char csv[] = "Alice,30,engineer,,extra";
    printf("Before: \"%s\"\n", csv);

    int field = 0;
    char *token = strtok(csv, ",");
    while (token != NULL) {
        printf("Field %d: \"%s\"\n", field++, token);
        token = strtok(NULL, ",");
    }

    printf("\nAfter strtok, csv bytes:\n");
    for (size_t i = 0; i < sizeof(csv); i++) {
        if (csv[i] == '\0')
            printf("[\\0]");
        else
            printf("%c", csv[i]);
    }
    printf("\n");
    return 0;
}
```

<details><summary>Predicción</summary>

```
Before: "Alice,30,engineer,,extra"
Field 0: "Alice"
Field 1: "30"
Field 2: "engineer"
Field 3: "extra"
```

Nótese: **no hay Field para el campo vacío** entre `,,`. `strtok` trata delimitadores consecutivos como uno solo — no genera tokens vacíos. Si necesitas campos vacíos, usa `strsep` (BSD) o parsing manual.

Después de `strtok`, el array `csv` tiene `'\0'` donde estaban las comas:
```
Alice[\0]30[\0]engineer[\0][\0]extra[\0]
```

Los bytes siguen ahí, pero las comas fueron reemplazadas por `'\0'`. El string original está **destruido** — `printf("%s", csv)` solo imprimiría `"Alice"`.

</details>

### Ejercicio 9 — strcspn y strspn en acción

```c
// ¿Qué imprime?
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *input = "  \t  hello, world! 123  ";

    // Skip leading whitespace
    size_t skip = strspn(input, " \t");
    printf("Leading whitespace: %zu chars\n", skip);
    printf("After skip: \"%s\"\n", input + skip);

    // Find length of first "word" (until space or punctuation)
    const char *word_start = input + skip;
    size_t word_len = strcspn(word_start, " ,!.\t");
    printf("First word: \"%.*s\" (%zu chars)\n",
           (int)word_len, word_start, word_len);

    // Count digits in a string
    const char *nums = "abc123def456";
    size_t to_digit = strcspn(nums, "0123456789");
    printf("Chars before first digit: %zu\n", to_digit);
    printf("From first digit: \"%s\"\n", nums + to_digit);
    return 0;
}
```

<details><summary>Predicción</summary>

```
Leading whitespace: 5 chars
After skip: "hello, world! 123  "
First word: "hello" (5 chars)
Chars before first digit: 3
From first digit: "123def456"
```

- `strspn(input, " \t")` cuenta caracteres que **sí** están en `" \t"`: 2 espacios + 1 tab + 2 espacios = 5.
- `strcspn(word_start, " ,!.\t")` cuenta caracteres que **no** están en el conjunto: `"hello"` = 5 chars, para en la coma.
- `strcspn("abc123def456", "0123456789")` = 3 — `"abc"` antes del primer dígito.

`strcspn` y `strspn` son complementarios: uno cuenta "hasta encontrar" y el otro "mientras pertenece".

</details>

### Ejercicio 10 — Construir una función segura con snprintf

```c
// Implementar safe_join que une un array de strings con un separador.
// ¿Qué imprime? ¿Detecta la truncación correctamente?
#include <stdio.h>
#include <string.h>

int safe_join(char *dest, size_t size,
              const char *parts[], int count, const char *sep) {
    int pos = 0;
    for (int i = 0; i < count; i++) {
        if (i > 0) {
            pos += snprintf(dest + pos, size - (pos < (int)size ? pos : size),
                            "%s", sep);
        }
        pos += snprintf(dest + pos, size - (pos < (int)size ? pos : size),
                        "%s", parts[i]);
    }
    return pos;  // total length needed (without '\0')
}

int main(void) {
    const char *words[] = {"hello", "beautiful", "world"};

    char big[50];
    int needed = safe_join(big, sizeof(big), words, 3, " ");
    printf("big: \"%s\" (needed %d)\n", big, needed);

    char small[12];
    needed = safe_join(small, sizeof(small), words, 3, " ");
    printf("small: \"%s\" (needed %d, truncated=%s)\n",
           small, needed, needed >= (int)sizeof(small) ? "yes" : "no");
    return 0;
}
```

<details><summary>Predicción</summary>

```
big: "hello beautiful world" (needed 21)
small: "hello beaut" (needed 21, truncated=yes)
```

- Con `big[50]`: sobra espacio. `"hello" + " " + "beautiful" + " " + "world"` = 5+1+9+1+5 = 21 chars. `pos` = 21, cabe en 50.
- Con `small[12]`: solo caben 11 chars + `'\0'`. Se trunca a `"hello beaut"`. `pos` sigue acumulando el total que se habría necesitado (21), permitiendo detectar truncación.

El patrón `size - (pos < (int)size ? pos : size)` evita el underflow de `size_t` cuando `pos` supera `size`: si `pos >= size`, pasa 0 como tamaño a `snprintf`, que solo escribe `'\0'` (o nada si ya no queda espacio).

</details>
