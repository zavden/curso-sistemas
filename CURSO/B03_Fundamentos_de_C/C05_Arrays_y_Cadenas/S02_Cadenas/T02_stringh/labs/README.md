# Lab — string.h

## Objetivo

Explorar las funciones principales de `string.h` compilando y ejecutando
programas que demuestran su comportamiento. Al finalizar, sabras usar
correctamente `strlen`, `strcpy`, `strcat`, `strcmp`, `strchr`, `strstr`,
`memcpy`, `memset` y `memmove`, y conoceras las trampas clasicas de
`strncpy` y `strcat`. Tambien practicaras el patron seguro con `snprintf`.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `str_length_copy.c` | Demuestra strlen, strcpy, strncpy y snprintf |
| `str_concat.c` | Demuestra strcat, strncat y concatenacion segura |
| `str_compare_search.c` | Demuestra strcmp, strncmp, strchr, strstr, strcspn, strspn |
| `mem_operations.c` | Demuestra memcpy, memset, memmove y memcmp |

---

## Parte 1 — strlen, strcpy, strncpy

**Objetivo**: Entender como funciona `strlen`, por que `strcpy` es insegura,
las trampas de `strncpy`, y el patron seguro con `snprintf`.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat str_length_copy.c
```

Antes de compilar, lee el codigo y predice:

- `strlen("abc\0def")` retornara 3 o 7?
- Cuando `strncpy` copia 5 bytes de `"hello world"` en un buffer de 5, el
  resultado tendra terminador `'\0'`?
- Cuando `strncpy` copia `"hi"` en un buffer de 10, que pasa con los 8 bytes
  restantes?
- `snprintf` con un buffer de 10 bytes y un string de 21 chars, que valor
  retorna?

### Paso 1.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic str_length_copy.c -o str_length_copy
```

GCC mostrara advertencias como `-Wstringop-truncation` y
`-Wformat-truncation`. Estas advertencias son **intencionales** — el
compilador detecta exactamente los patrones peligrosos que este programa
demuestra. En codigo real, estas advertencias te alertan de bugs.

```bash
./str_length_copy
```

Salida esperada:

```
strlen("Hello, world!") = 13
strlen("") = 0
strlen("abc\0def") = 3

strcpy -> dest_big = "Hello, world!"

strncpy(buf, "hello world", 5) bytes: 'h' 'e' 'l' 'l' 'o'  (no '\0')
strncpy(buf, "hi", 10) bytes: 'h' 'i' \0 \0 \0 \0 \0 \0 \0 \0

Safe strncpy -> "a long st" (len 9)

snprintf -> "hello won" (returned 21, buf size 10)
Truncation detected: needed 21 bytes, had 10
```

### Paso 1.3 — Analisis

Verifica tus predicciones:

- `strlen("abc\0def")` retorna 3, no 7. `strlen` para en el primer `'\0'`.
  Los bytes `def` existen en memoria pero `strlen` nunca los ve.

- `strncpy` con `n=5` y un `src` de 11 chars: copia solo 5 bytes y **no**
  agrega `'\0'`. El buffer no es un string valido. Esta es la trampa mas
  peligrosa de `strncpy`.

- `strncpy` con `src="hi"` y `n=10`: copia `'h'`, `'i'`, y luego rellena
  los 8 bytes restantes con `'\0'`. Este zero-padding desperdicia ciclos
  cuando el buffer es grande.

- `snprintf` retorna 21 (lo que habria escrito sin limite), pero solo escribe
  9 chars + `'\0'` en el buffer de 10. Comparar `retorno >= sizeof(buf)` es
  la forma correcta de detectar truncacion.

### Limpieza de la parte 1

```bash
rm -f str_length_copy
```

---

## Parte 2 — strcat, strncat

**Objetivo**: Entender como funciona la concatenacion de strings, el riesgo
de desbordamiento con `strcat`, y como usar `snprintf` para concatenar de
forma segura.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat str_concat.c
```

Antes de compilar, predice:

- Despues de `strncat(buf, " wonderful world", 6)` sobre `buf = "hello"`,
  cual sera el contenido de `buf`?
- Si concatenas `"first"`, `" + second"`, y `" + third"` en un buffer de 15
  bytes con `snprintf`, que se truncara?

### Paso 2.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic str_concat.c -o str_concat
./str_concat
```

Salida esperada:

```
Initial: "hello"
After strcat(" "): "hello "
After strcat("world"): "hello world"

strncat(buf, " wonderful world", 6) -> "hello wonde"
strlen after strncat: 11

snprintf concat -> "Hello World!" (pos=12)
snprintf concat (small buf) -> "first + second"
Would have needed 22 bytes total
```

### Paso 2.3 — Analisis

- `strcat` busca el `'\0'` de `dest` cada vez antes de concatenar. En un
  loop esto es O(n^2) — cada llamada recorre todo lo ya escrito.

- `strncat(buf, " wonderful world", 6)` copia **6 chars** de `src`:
  `" wonde"`, y luego agrega `'\0'`. A diferencia de `strncpy`, `strncat`
  **siempre** agrega terminador. Pero el parametro `n` limita chars de `src`,
  no el tamano de `dest`.

- Con `snprintf`, el patron `pos += snprintf(buf + pos, size - pos, ...)`
  acumula la posicion. Si el buffer se llena, `snprintf` trunca de forma
  segura y el valor de retorno indica cuantos bytes se habrian necesitado.

### Limpieza de la parte 2

```bash
rm -f str_concat
```

---

## Parte 3 — strcmp, strncmp, strchr, strstr

**Objetivo**: Comparar strings por contenido (no por puntero), buscar
caracteres y substrings, y usar `strcspn`/`strspn` para segmentar.

### Paso 3.1 — Examinar el codigo fuente

```bash
cat str_compare_search.c
```

Antes de compilar, predice:

- `strcmp("abc", "abd")` retornara un valor positivo o negativo?
- Si `a = "hello"` y `buf` es una copia con `strcpy(buf, "hello")`, la
  comparacion `a == buf` sera verdadera o falsa?
- `strchr("hello world", 'o')` retornara un puntero al primer `'o'`
  (indice 4) o al ultimo (indice 7)?
- `strcspn("hello, world", " ,")` retornara 5 o 6?

### Paso 3.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic str_compare_search.c -o str_compare_search
./str_compare_search
```

Salida esperada:

```
=== strcmp ===
strcmp("abc", "abc") = 0
strcmp("abc", "abd") = -1
strcmp("abd", "abc") = 1
strcmp("abc", "abcd") = -1
strcmp("", "") = 0

Pointer comparison (==): NOT EQUAL
Content comparison (strcmp): EQUAL

=== strncmp ===
strncmp("hello", "help", 3) = 0
strncmp("hello", "help", 4) = -4
Line starts with ERROR: -> "ERROR: file not found"

=== strchr ===
strchr("hello world", 'o') -> "o world" (index 4)
strchr("hello world", 'z') -> NULL
strrchr("hello world", 'o') -> "orld" (index 7)

=== strstr ===
strstr("hello world", "world") -> "world" (index 6)
strstr("hello world", "xyz") -> NULL

=== strcspn / strspn ===
strcspn("hello, world", " ,") = 5
strspn("   hello", " ") = 3
Before strcspn: "some text
"After strcspn:  "some text"
```

### Paso 3.3 — Analisis

- `strcmp("abc", "abd")` retorna un valor negativo porque `'c' < 'd'` en
  ASCII. Los valores exactos (`-1`, `-4`) dependen de la implementacion —
  el estandar solo garantiza signo negativo, cero o positivo.

- `a == buf` es **falso** aunque ambos contienen `"hello"`. El operador `==`
  compara **direcciones de memoria**, no contenido. Solo `strcmp` compara
  contenido caracter por caracter. Esta es una fuente comun de bugs.

- `strchr` retorna la **primera** ocurrencia (indice 4). Para la ultima,
  se usa `strrchr` (indice 7).

- `strcspn("hello, world", " ,")` retorna 5: la longitud del segmento
  `"hello"` antes del primer caracter que esta en `" ,"` (la coma en
  posicion 5). El patron `buf[strcspn(buf, "\n")] = '\0'` es la forma
  idiomatica de eliminar el newline de `fgets`.

### Limpieza de la parte 3

```bash
rm -f str_compare_search
```

---

## Parte 4 — memcpy, memset, memmove, memcmp

**Objetivo**: Usar las funciones `mem*` que operan sobre bloques de bytes
sin depender de `'\0'`. Entender cuando usar `memmove` vs `memcpy` y la
trampa de `memset` con enteros.

### Paso 4.1 — Examinar el codigo fuente

```bash
cat mem_operations.c
```

Antes de compilar, predice:

- `memset(arr, 1, sizeof(arr))` sobre un array de 3 `int` pondra cada
  entero en 1 o en otro valor?
- `memmove(buf+2, buf, 5)` sobre `"hello world"` producira `"hehelloorld"`
  o `"hehehehehe"`?
- `memcmp` entre `{1,2,3}` y `{1,2,4}` retornara un valor negativo o
  positivo?

### Paso 4.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic mem_operations.c -o mem_operations
./mem_operations
```

Salida esperada:

```
=== memcpy ===
src: {10, 20, 30, 40, 50}
dst: {10, 20, 30, 40, 50}
memcpy string: "hello"

=== memset ===
memset(buf, 'A', 10) -> "AAAAAAAAAA"
memset to 0: {0, 0, 0, 0, 0}
memset(arr, 1, ...) NOT {1,1,1}: {16843009, 16843009, 16843009}
Each int = 0x01010101 = 16843009 (each BYTE is 0x01)

=== memmove ===
Before memmove: "hello world"
After memmove(buf+2, buf, 5): "hehelloorld"

Before memmove: "abcdefghij"
memmove(buf+3, buf, 5): "abcabcdeij"

=== memcmp ===
memcmp(a, b) = 0 (equal)
memcmp(a, c) = -1 (a < c)
```

### Paso 4.3 — Analisis

- `memset(arr, 1, sizeof(arr))` pone cada **byte** en `0x01`, no cada int.
  Un `int` de 4 bytes queda como `0x01010101 = 16843009`. Para poner ints a
  un valor especifico distinto de 0, se necesita un loop. `memset` a 0 si
  funciona porque `0x00000000 == 0`.

- `memmove` maneja correctamente el solapamiento. Cuando `dest` y `src` se
  solapan, `memmove` copia en la direccion correcta (de atras hacia adelante
  si es necesario) para no corromper los datos. `memcpy` con regiones
  solapadas es comportamiento indefinido.

- `memcmp` compara byte por byte. Retorna negativo porque `{1,2,3}` difiere
  de `{1,2,4}` en el tercer entero, donde los bytes de 3 son menores que los
  de 4. Para structs con padding, `memcmp` puede dar resultados inesperados
  porque los bytes de padding son indeterminados.

### Limpieza de la parte 4

```bash
rm -f mem_operations
```

---

## Limpieza final

```bash
rm -f str_length_copy str_concat str_compare_search mem_operations
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  mem_operations.c  str_compare_search.c  str_concat.c  str_length_copy.c
```

---

## Conceptos reforzados

1. `strlen` recorre byte por byte hasta encontrar `'\0'`. Si el string
   contiene `'\0'` embebido (como `"abc\0def"`), `strlen` retorna la
   longitud hasta el primer `'\0'`, no la longitud total del literal.

2. `strcpy` copia incluyendo `'\0'` pero no verifica el tamano de `dest`.
   Si `src` es mas largo que `dest`, desborda el buffer (comportamiento
   indefinido).

3. `strncpy` **no** es la version segura de `strcpy`. Cuando `strlen(src)
   >= n`, no agrega `'\0'` y el resultado no es un string valido. Fue
   disenada para campos de tamano fijo, no para copia segura de strings.

4. `snprintf` es la alternativa recomendada: siempre termina con `'\0'`,
   nunca desborda, y retorna cuantos bytes habria escrito para detectar
   truncacion.

5. `strncat` siempre agrega `'\0'` (a diferencia de `strncpy`), pero su
   parametro `n` limita los chars de `src`, no el tamano total de `dest`.

6. `strcmp` compara **contenido** caracter por caracter. El operador `==`
   compara **direcciones de memoria**. Usar `==` para comparar strings
   en C es un bug clasico.

7. `strchr` retorna un puntero a la primera ocurrencia de un caracter.
   `strrchr` retorna la ultima. `strstr` busca un substring completo.
   Todas retornan `NULL` si no encuentran coincidencia.

8. `strcspn(buf, "\n") = '\0'` es el patron idiomatico para eliminar el
   newline que `fgets` deja al final del buffer.

9. Las funciones `mem*` operan sobre **bytes**, no strings. No buscan
   `'\0'` y requieren un tamano explicito. `memcpy` no soporta
   solapamiento; `memmove` si.

10. `memset` con un valor distinto de 0 sobre un array de `int` no pone
    cada int en ese valor — pone cada **byte** en ese valor. Un `int` de
    4 bytes con cada byte en `0x01` vale `16843009`, no `1`.
