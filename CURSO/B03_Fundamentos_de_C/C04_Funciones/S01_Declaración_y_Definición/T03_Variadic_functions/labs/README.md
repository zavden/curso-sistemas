# Lab — Variadic functions

## Objetivo

Implementar y ejecutar funciones variadicas usando `<stdarg.h>`. Practicar los
tres patrones para determinar cuantos argumentos recibio la funcion: conteo
explicito, sentinel, y format string. Al finalizar, sabras usar va_list,
va_start, va_arg y va_end, y entenderas la promocion de argumentos variadicos.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `sum_variadic.c` | Funcion sum con conteo explicito de argumentos |
| `sentinel.c` | Funciones que usan sentinel (-1 y NULL) para terminar |
| `mini_printf.c` | Mini-printf que interpreta un format string |

---

## Parte 1 — stdarg.h basico: conteo explicito

**Objetivo**: Entender la maquinaria de `<stdarg.h>` con el patron mas simple:
pasar el numero de argumentos como primer parametro.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat sum_variadic.c
```

Observa la estructura:

- `#include <stdarg.h>` — proporciona va_list, va_start, va_arg, va_end
- `int sum(int count, ...)` — `count` es el parametro fijo, `...` son los
  argumentos variables
- La funcion usa `count` para saber cuantas veces llamar a `va_arg`

### Paso 1.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic sum_variadic.c -o sum_variadic
```

Antes de ejecutar, predice la salida para cada llamada:

- `sum(3, 10, 20, 30)` — que devuelve?
- `sum(5, 1, 2, 3, 4, 5)` — que devuelve?
- `sum(1, 42)` — que devuelve?
- `sum(0)` — que devuelve? El bucle se ejecuta alguna vez?

### Paso 1.3 — Verificar la prediccion

```bash
./sum_variadic
```

Salida esperada:

```
sum(3, 10, 20, 30) = 60
sum(5, 1, 2, 3, 4, 5) = 15
sum(1, 42) = 42
sum(0) = 0
```

Analisis:

- Con `count = 0`, el bucle `for` no ejecuta ninguna iteracion — `va_arg`
  nunca se llama y `total` se queda en 0
- Cada llamada a `va_arg(args, int)` avanza al siguiente argumento. Si
  llamaras a `va_arg` mas veces que argumentos pasados, seria **undefined
  behavior**

### Paso 1.4 — Traza manual del flujo

Para entender exactamente que ocurre, sigue la traza de `sum(3, 10, 20, 30)`:

```
1. count = 3, los ... son: 10, 20, 30
2. va_start(args, count) — inicializa args despues de count
3. i=0: va_arg(args, int) -> 10, total = 10
4. i=1: va_arg(args, int) -> 20, total = 30
5. i=2: va_arg(args, int) -> 30, total = 60
6. i=3: i < count es falso, sale del bucle
7. va_end(args) — limpieza
8. return 60
```

Punto clave: `va_start` recibe el **ultimo parametro fijo** (`count`). Los
argumentos variables empiezan despues de ese parametro.

### Limpieza de la parte 1

```bash
rm -f sum_variadic
```

---

## Parte 2 — Sentinel: recorrer hasta un marcador

**Objetivo**: Usar un valor especial (sentinel) para indicar el final de los
argumentos, en lugar de pasar un conteo.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat sentinel.c
```

El archivo tiene dos funciones:

- `sum_until(int first, ...)` — suma enteros hasta encontrar `-1`
- `print_strings(const char *first, ...)` — imprime strings hasta encontrar
  `NULL`

Observa las diferencias con la parte 1:

- No hay parametro `count` — el sentinel cumple esa funcion
- El primer argumento es el primer valor real (no un conteo)
- `sum_until` verifica si `first` ya es -1 antes de iniciar `va_start`

### Paso 2.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic sentinel.c -o sentinel
```

Antes de ejecutar, predice:

- `sum_until(10, 20, 30, -1)` — que devuelve? El -1 se incluye en la suma?
- `sum_until(-1)` — que devuelve? `va_start` se llama alguna vez?
- `print_strings("hello", "world", "foo", NULL)` — que imprime? Hay
  salto de linea entre las palabras?

### Paso 2.3 — Verificar la prediccion

```bash
./sentinel
```

Salida esperada:

```
--- sum_until (sentinel = -1) ---
sum_until(10, 20, 30, -1) = 60
sum_until(5, -1) = 5
sum_until(-1) = 0

--- print_strings (sentinel = NULL) ---
hello world foo
one
a b c d
```

Analisis:

- El sentinel **no se incluye** en el resultado — solo marca el final
- Cuando `first == -1`, la funcion retorna 0 sin siquiera iniciar `va_start`
- `print_strings` usa un espacio como separador y un `\n` al final
- `NULL` funciona como sentinel natural para punteros. Para enteros, se
  usa un valor que no sea un dato valido (como -1)

### Paso 2.4 — Peligro del patron sentinel

Piensa en este problema:

- Si olvidas poner `-1` al final de `sum_until`, que ocurre?
- La funcion seguira leyendo argumentos con `va_arg` mas alla de lo que
  pasaste — eso es **undefined behavior**

El compilador **no puede detectar** que olvidaste el sentinel porque `...`
no tiene informacion de tipos ni de cantidad. Este es un riesgo real del
patron sentinel.

### Limpieza de la parte 2

```bash
rm -f sentinel
```

---

## Parte 3 — Format string: mini-printf

**Objetivo**: Implementar un mini-printf que interpreta un format string
para saber que tipos de argumentos leer con `va_arg`.

### Paso 3.1 — Examinar el codigo fuente

```bash
cat mini_printf.c
```

Observa la estructura de `my_printf`:

- Recorre el format string caracter por caracter
- Si encuentra `%`, lee el siguiente caracter para saber que tipo extraer
- `%d` -> `va_arg(args, int)`
- `%s` -> `va_arg(args, const char *)`
- `%c` -> `va_arg(args, int)` (no `char` — por la promocion de argumentos)
- `%%` -> imprime un `%` literal

Antes de compilar, responde mentalmente:

- Por que `%c` usa `va_arg(args, int)` en lugar de `va_arg(args, char)`?
- Que pasaria si usaras `va_arg(args, char)`?

### Paso 3.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic mini_printf.c -o mini_printf
```

Predice la salida de cada llamada:

- `my_printf("Hello %s, you are %d years old\n", "Alice", 25)`
- `my_printf("Letter: %c\n", 'Z')`
- `my_printf("100%% complete\n")`
- `my_printf("%s scored %d out of %d\n", "Bob", 87, 100)`

### Paso 3.3 — Verificar la prediccion

```bash
./mini_printf
```

Salida esperada:

```
Hello Alice, you are 25 years old
Letter: Z
100% complete
Bob scored 87 out of 100
```

Analisis:

- El format string le dice a la funcion **que tipo** tiene cada argumento
  variable. Sin el format string, la funcion no tendria forma de saberlo
- Este es exactamente el mismo patron que usa `printf` de la biblioteca
  estandar

### Paso 3.4 — Promocion de argumentos variadicos

La respuesta a la pregunta del paso 3.1:

Los argumentos variadicos sufren **default argument promotions**:

| Tipo pasado | Se promueve a |
|-------------|---------------|
| `char` | `int` |
| `short` | `int` |
| `float` | `double` |

Por eso `%c` usa `va_arg(args, int)`: cuando pasas `'Z'` a una funcion
variadica, el `char` se promueve a `int` antes de llegar a la funcion.
Usar `va_arg(args, char)` seria **undefined behavior** porque el argumento
en la pila es un `int`, no un `char`.

Lo mismo aplica a `float`: si quisieras soportar `%f`, usarias
`va_arg(args, double)`, nunca `va_arg(args, float)`.

### Paso 3.5 — Que pasa con un formato incorrecto

Piensa en esta llamada:

```c
my_printf("Value: %d\n", "hello");
```

- `my_printf` leera `va_arg(args, int)`, pero el argumento es un `const
  char *`
- Interpretara los bytes del puntero como un entero — basura o crash
- El compilador **no puede verificar** esto en `my_printf` porque no tiene
  `__attribute__((format))`
- `printf` real de glibc SI puede verificarlo porque tiene ese atributo

### Limpieza de la parte 3

```bash
rm -f mini_printf
```

---

## Limpieza final

```bash
rm -f sum_variadic sentinel mini_printf
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  mini_printf.c  sentinel.c  sum_variadic.c
```

---

## Conceptos reforzados

1. `<stdarg.h>` proporciona cuatro macros: `va_list` (tipo), `va_start`
   (inicializar), `va_arg` (leer siguiente argumento), `va_end` (limpiar).
   `va_start` siempre recibe el ultimo parametro fijo de la funcion.

2. Una funcion variadica **no sabe** cuantos argumentos recibio. Hay tres
   patrones para resolverlo: conteo explicito (como `sum`), sentinel
   (como `sum_until`), y format string (como `printf`).

3. El patron sentinel (-1 para enteros, NULL para punteros) elimina la
   necesidad de contar, pero introduce un riesgo: olvidar el sentinel causa
   **undefined behavior** que el compilador no puede detectar.

4. Los argumentos variadicos sufren **default argument promotions**: `char`
   y `short` se promueven a `int`, `float` se promueve a `double`. Usar el
   tipo original en `va_arg` (ej. `va_arg(args, char)`) es undefined behavior.

5. `printf` puede verificar argumentos contra el format string gracias a
   `__attribute__((format(printf, 1, 2)))`. Las funciones variadicas propias
   no tienen esta verificacion a menos que se anada el atributo explicitamente.

6. Pasar un tipo incorrecto a `va_arg` (ej. leer un puntero como `int`) es
   **undefined behavior** — puede producir basura, crash, o parecer funcionar
   correctamente en un sistema y fallar en otro.
