# T02 — string.h

## Funciones de longitud

```c
#include <string.h>

// strlen — longitud del string (sin contar '\0'):
size_t strlen(const char *s);

const char *s = "hello";
size_t len = strlen(s);        // 5
strlen("");                     // 0
strlen("abc\0def");            // 3 — para en el primer '\0'

// strlen recorre byte por byte hasta '\0' → O(n)
// Si s no tiene '\0': UB (lee memoria indefinidamente)
```

```c
// strnlen — strlen con límite (POSIX, no C estándar):
size_t strnlen(const char *s, size_t maxlen);

strnlen("hello", 100);    // 5
strnlen("hello", 3);      // 3 — no busca más allá de maxlen

// Útil cuando no se sabe si el buffer tiene '\0':
char buf[10];
// ...llenar buf sin garantía de '\0'...
size_t len = strnlen(buf, sizeof(buf));
// Si no hay '\0' en buf, retorna 10 (no lee más allá)
```

## Funciones de copia

### strcpy

```c
// strcpy — copia src a dest, incluyendo '\0':
char *strcpy(char *dest, const char *src);

char dest[20];
strcpy(dest, "hello");     // dest = "hello\0"
// Retorna dest

// PELIGRO: si src es más largo que dest → buffer overflow:
char small[5];
strcpy(small, "this is way too long");    // UB — desborda small
```

### strncpy — NO hace lo que crees

```c
// strncpy — copia EXACTAMENTE n bytes:
char *strncpy(char *dest, const char *src, size_t n);

char dest[10];
strncpy(dest, "hello", sizeof(dest));
// Copia "hello" y rellena el resto con '\0': "hello\0\0\0\0\0"

// TRAMPA 1: si strlen(src) >= n, NO agrega '\0':
char buf[5];
strncpy(buf, "hello world", 5);
// buf = {'h','e','l','l','o'} — SIN terminador '\0'
// buf NO es un string válido

// TRAMPA 2: si src es corto, rellena todo con '\0':
char buf2[1000];
strncpy(buf2, "hi", 1000);
// Copia "hi" y escribe 998 bytes de '\0' — desperdicio

// strncpy fue diseñada para campos de tamaño fijo en Unix v7
// (nombres de archivos en directorios), NO para strings seguros.
```

```c
// Cómo usar strncpy "correctamente" (si debes usarla):
char dest[10];
strncpy(dest, src, sizeof(dest) - 1);
dest[sizeof(dest) - 1] = '\0';    // SIEMPRE forzar terminador

// Alternativa mejor — snprintf:
snprintf(dest, sizeof(dest), "%s", src);
// Siempre termina con '\0', nunca desborda, retorna lo que
// habría escrito (para detectar truncación).
```

### strlcpy — La versión segura (BSD/POSIX, no estándar C)

```c
// strlcpy — copia con límite, SIEMPRE termina con '\0':
size_t strlcpy(char *dest, const char *src, size_t size);

char dest[10];
strlcpy(dest, "hello world", sizeof(dest));
// dest = "hello wor\0" — truncado pero siempre terminado
// Retorna strlen(src) = 11
// Si retorno >= size → hubo truncación

// No está en el estándar C. Disponible en BSD, Linux (glibc 2.38+).
// Alternativa portátil: snprintf.
```

## Funciones de concatenación

### strcat

```c
// strcat — concatena src al final de dest:
char *strcat(char *dest, const char *src);

char buf[50] = "hello";
strcat(buf, " ");            // "hello "
strcat(buf, "world");        // "hello world"

// PELIGRO: no verifica el tamaño de dest:
char small[10] = "hello";
strcat(small, " world!!!");  // UB — desborda small

// Rendimiento: strcat busca el '\0' de dest cada vez → O(n+m)
// Concatenar en un loop es O(n²) — "Shlemiel the painter":
char result[1000] = "";
for (int i = 0; i < 100; i++) {
    strcat(result, "x");    // cada vez recorre todo result
}
// Solución: mantener un puntero al final o usar snprintf.
```

### strncat

```c
// strncat — concatena MÁXIMO n chars de src, luego agrega '\0':
char *strncat(char *dest, const char *src, size_t n);

char buf[20] = "hello";
strncat(buf, " wonderful world", 6);
// buf = "hello wonde\0" — solo 6 chars de src + '\0'

// A diferencia de strncpy, strncat SÍ agrega '\0' siempre.
// Pero n es la cantidad MÁXIMA de chars de src, NO el tamaño de dest.
// Para limitar por tamaño de dest:
strncat(buf, src, sizeof(buf) - strlen(buf) - 1);
```

## Funciones de comparación

### strcmp

```c
// strcmp — compara dos strings lexicográficamente:
int strcmp(const char *s1, const char *s2);

// Retorna:
//   < 0  si s1 < s2
//   = 0  si s1 == s2
//   > 0  si s1 > s2

strcmp("abc", "abc");     // 0 — iguales
strcmp("abc", "abd");     // < 0 — 'c' < 'd'
strcmp("abd", "abc");     // > 0 — 'd' > 'c'
strcmp("abc", "abcd");    // < 0 — "abc" es prefijo, más corto
strcmp("", "");           // 0

// NUNCA usar == para comparar strings:
const char *a = "hello";
char buf[10];
strcpy(buf, "hello");
if (a == buf) { /* FALSO — compara direcciones, no contenido */ }
if (strcmp(a, buf) == 0) { /* VERDADERO — compara contenido */ }
```

### strncmp

```c
// strncmp — compara máximo n caracteres:
int strncmp(const char *s1, const char *s2, size_t n);

strncmp("hello", "help", 3);     // 0 — los primeros 3 son iguales
strncmp("hello", "help", 4);     // < 0 — 'l' < 'p'

// Útil para verificar prefijos:
if (strncmp(line, "ERROR:", 6) == 0) {
    // line empieza con "ERROR:"
}
```

### strcasecmp (POSIX, no estándar C)

```c
// Comparación sin importar mayúsculas/minúsculas:
#include <strings.h>    // nota: strings.h, con 's' al final

strcasecmp("Hello", "HELLO");    // 0 — iguales ignorando caso
strncasecmp("Hello", "HELP", 3); // 0 — "Hel" == "HEL"
```

## Funciones de búsqueda

```c
// strchr — busca la primera ocurrencia de un char:
char *strchr(const char *s, int c);

char *p = strchr("hello world", 'o');
// p apunta a "o world"
// p - "hello world" == 4 (índice)

strchr("hello", 'z');    // NULL — no encontrado

// strrchr — busca la ÚLTIMA ocurrencia:
char *q = strrchr("hello world", 'o');
// q apunta a "orld"

// strchr con '\0' retorna el terminador:
char *end = strchr("hello", '\0');
// end apunta al '\0' al final
```

```c
// strstr — busca un substring:
char *strstr(const char *haystack, const char *needle);

char *p = strstr("hello world", "world");
// p apunta a "world"

strstr("hello", "xyz");     // NULL — no encontrado
strstr("hello", "");        // "hello" — string vacío siempre se encuentra

// strpbrk — busca cualquiera de un conjunto de chars:
char *q = strpbrk("hello, world!", " ,!");
// q apunta a "," — la primera ocurrencia de ' ', ',' o '!'
```

```c
// strcspn / strspn — longitud de segmento:

// strcspn — chars hasta encontrar alguno del conjunto:
size_t n = strcspn("hello, world", " ,");
// n = 5 — "hello" tiene 5 chars antes de ',' o ' '
// Uso típico: quitar newline:
buf[strcspn(buf, "\n")] = '\0';

// strspn — chars que SÍ están en el conjunto:
size_t m = strspn("   hello", " ");
// m = 3 — hay 3 espacios al inicio
// Uso típico: saltar whitespace:
const char *start = s + strspn(s, " \t\n");
```

## Funciones de memoria (mem*)

Las funciones `mem*` operan con **bloques de bytes**, no strings.
No buscan `\0` — usan un parámetro de longitud explícito:

```c
// memcpy — copia n bytes (no puede solaparse):
void *memcpy(void *dest, const void *src, size_t n);

int a[5] = {1, 2, 3, 4, 5};
int b[5];
memcpy(b, a, sizeof(a));     // copia 20 bytes
// b = {1, 2, 3, 4, 5}

// memcpy funciona con CUALQUIER tipo, no solo strings.
// Si src y dest se solapan: UB. Usar memmove.
```

```c
// memmove — copia n bytes (PUEDE solaparse):
void *memmove(void *dest, const void *src, size_t n);

char buf[] = "hello world";
memmove(buf + 2, buf, 5);
// buf = "hehello rld"
// memmove maneja el solapamiento correctamente.
// Es ligeramente más lento que memcpy (debe verificar dirección).
```

```c
// memset — llena n bytes con un valor:
void *memset(void *s, int c, size_t n);

char buf[100];
memset(buf, 0, sizeof(buf));     // todo a 0
memset(buf, 'A', 10);           // primeros 10 bytes a 'A'

// CUIDADO: c se convierte a unsigned char (1 byte):
int arr[10];
memset(arr, 0, sizeof(arr));     // OK — todo a 0
memset(arr, 1, sizeof(arr));     // NO es {1,1,...}
// Cada BYTE se pone a 1: 0x01010101 = 16843009 por int

// Para poner ints a un valor específico, usar un loop.
```

```c
// memcmp — compara n bytes:
int memcmp(const void *s1, const void *s2, size_t n);

int a[3] = {1, 2, 3};
int b[3] = {1, 2, 3};
int c[3] = {1, 2, 4};

memcmp(a, b, sizeof(a));    // 0 — iguales
memcmp(a, c, sizeof(a));    // < 0 — difieren en el tercer int

// memcmp compara bytes, no elementos.
// Para structs con padding: memcmp puede dar resultados
// inesperados porque compara bytes de padding (indeterminados).
```

## memcpy vs strcpy vs strncpy

| Función | Para | Encuentra '\0' | Segura | Recomendada |
|---|---|---|---|---|
| strcpy | Strings | Sí | No (overflow) | No |
| strncpy | Campos fijos | Parcial | Parcial | No |
| strlcpy | Strings | Sí | Sí | Sí (si disponible) |
| snprintf | Strings | Sí | Sí | Sí |
| memcpy | Bytes | No | N/A (usa n) | Sí (para datos) |

## strtok — Tokenizar strings

```c
// strtok divide un string por delimitadores:
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

// CUIDADO:
// 1. strtok MODIFICA el string original (reemplaza delimitadores con '\0')
// 2. strtok usa estado global (static) → NO es thread-safe
// 3. No se puede tokenizar dos strings a la vez
// 4. No se puede usar con string literals (son const)

// Alternativa thread-safe: strtok_r (POSIX)
char *saveptr;
char *token = strtok_r(line, ",", &saveptr);
while (token != NULL) {
    token = strtok_r(NULL, ",", &saveptr);
}
```

## Patrones seguros

```c
// 1. snprintf para construir strings:
char buf[100];
int n = snprintf(buf, sizeof(buf), "Name: %s, Age: %d", name, age);
if (n >= (int)sizeof(buf)) {
    // Truncado — n es lo que habría escrito sin límite
}
// SIEMPRE termina con '\0'
// NUNCA desborda
// Retorna la longitud que habría tenido sin límite

// 2. snprintf para concatenar:
char result[100];
int pos = 0;
pos += snprintf(result + pos, sizeof(result) - pos, "Hello ");
pos += snprintf(result + pos, sizeof(result) - pos, "%s", name);
pos += snprintf(result + pos, sizeof(result) - pos, "!");
// result = "Hello Alice!"
```

---

## Ejercicios

### Ejercicio 1 — Reimplementar funciones

```c
// Implementar sin usar string.h:
// - size_t my_strlen(const char *s)
// - char *my_strcpy(char *dest, const char *src)
// - int my_strcmp(const char *s1, const char *s2)
// Probar con los mismos casos que las funciones originales.
```

### Ejercicio 2 — Safe concat

```c
// Implementar safe_concat(char *dest, size_t size, const char *s1, const char *s2)
// que concatene s1 y s2 en dest sin desbordar.
// Retornar 0 si cabe, -1 si se truncó.
// Usar snprintf internamente.
```

### Ejercicio 3 — Tokenizador

```c
// Leer una línea CSV de stdin (ej: "Alice,30,engineer").
// Tokenizar con strtok por ','.
// Imprimir cada campo numerado.
// Manejar el caso de campos vacíos.
```
