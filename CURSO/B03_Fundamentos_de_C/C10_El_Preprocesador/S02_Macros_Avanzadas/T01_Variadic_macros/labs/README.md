# Lab -- Variadic macros

## Objetivo

Usar macros variadicos (`__VA_ARGS__`) para crear wrappers de `fprintf` con
formato variable. Al finalizar, sabras definir macros con `...`, resolver el
problema de cero argumentos con `##__VA_ARGS__` y `__VA_OPT__`, construir
macros de logging con archivo y linea, y verificar la expansion con `gcc -E`.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `basic_variadic.c` | `__VA_ARGS__` basico con LOG y PRINT_ALL |
| `zero_args_problem.c` | Problema de coma extra, `##__VA_ARGS__` |
| `va_opt_demo.c` | `__VA_OPT__` de C23 para cero argumentos |
| `log_system.c` | Sistema de logging con niveles y `__FILE__`/`__LINE__` |
| `gcc_expand.c` | Archivo reducido para inspeccionar expansion con `gcc -E` |

---

## Parte 1 -- `__VA_ARGS__` basico

**Objetivo**: Definir macros con `...` y usar `__VA_ARGS__` para pasar
argumentos variables a `fprintf` y `printf`.

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat basic_variadic.c
```

Observa:

- `LOG(fmt, ...)` tiene un parametro fijo (`fmt`) y argumentos variables (`...`)
- `__VA_ARGS__` en la expansion se reemplaza por los argumentos despues de `fmt`
- `PRINT_ALL(...)` no tiene parametro fijo -- todo es variadic

### Paso 1.2 -- Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic basic_variadic.c -o basic_variadic
```

No debe producir warnings ni errores.

### Paso 1.3 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- `LOG("value of x = %d", x)` se expande a `fprintf(stderr, ...)`. Que
  argumentos recibe `fprintf` despues de la expansion?
- La macro `LOG` agrega `"\n"` al final del formato. Como funciona la
  concatenacion de string literals en C (`"[LOG] " fmt "\n"`)?
- `PRINT_ALL` usa `printf` en vez de `fprintf`. A donde va la salida?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.4 -- Ejecutar y verificar

```bash
./basic_variadic
```

Salida esperada (stderr y stdout mezclados en terminal):

```
[LOG] value of x = 42
[LOG] x = 42, y = 10
[LOG] sum = 52, product = 420
--- PRINT_ALL demo ---
x = 42
x = 42, y = 10
```

Observa:

- `LOG` antepone `"[LOG] "` y agrega `"\n"` automaticamente gracias a la
  concatenacion de string literals en C: `"[LOG] " fmt "\n"` se convierte en
  un solo string durante compilacion
- `PRINT_ALL` pasa todos sus argumentos directamente a `printf`
- Las lineas de `LOG` van a stderr, las de `PRINT_ALL` a stdout. En la
  terminal se ven juntas, pero se pueden separar con redireccion

### Paso 1.5 -- Separar stderr de stdout

```bash
./basic_variadic 2>/dev/null
```

Salida esperada (solo stdout):

```
--- PRINT_ALL demo ---
x = 42
x = 42, y = 10
```

```bash
./basic_variadic 2>&1 1>/dev/null
```

Salida esperada (solo stderr):

```
[LOG] value of x = 42
[LOG] x = 42, y = 10
[LOG] sum = 52, product = 420
```

Esto confirma que `LOG` escribe a stderr y `PRINT_ALL` a stdout.

### Limpieza de la parte 1

```bash
rm -f basic_variadic
```

---

## Parte 2 -- El problema de cero argumentos

**Objetivo**: Observar el error de compilacion cuando no se pasan argumentos
variadicos, y resolverlo con `##__VA_ARGS__` (extension GCC) y con la tecnica
de no tener parametro fijo.

### Paso 2.1 -- Examinar el codigo fuente

```bash
cat zero_args_problem.c
```

Observa:

- `LOG_BAD(fmt, ...)` usa `__VA_ARGS__` estandar
- `LOG_GCC(fmt, ...)` usa `##__VA_ARGS__` (extension GCC)
- `LOG_NOFIXED(...)` no tiene parametro fijo
- Hay una llamada a `LOG_BAD("starting server")` comentada

### Paso 2.2 -- Ver el error con la macro problematica

Descomenta la linea `LOG_BAD("starting server");` en `zero_args_problem.c` y
compila:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic zero_args_problem.c -o zero_args_problem
```

Salida esperada (error de compilacion):

```
zero_args_problem.c: In function 'main':
zero_args_problem.c:~20: error: expected expression before ')' token
```

El error ocurre porque `LOG_BAD("starting server")` se expande a:

```c
fprintf(stderr, "[BAD] " "starting server" "\n", )
```

La coma antes de `)` no tiene argumento -- es un error de sintaxis.

### Paso 2.3 -- Volver a comentar y compilar

Vuelve a comentar la linea `LOG_BAD("starting server");` (dejala como estaba
originalmente) y compila:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic zero_args_problem.c -o zero_args_problem
```

Debe compilar sin errores.

### Paso 2.4 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- `LOG_GCC("starting server")` usa `##__VA_ARGS__`. Que hace `##` cuando no
  hay argumentos variadicos?
- `LOG_NOFIXED("starting server\n")` no tiene parametro fijo. Por que no
  tiene el problema de la coma?
- `LOG_NOFIXED` puede agregar un prefijo como `"[LOG] "` al formato? Por que?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.5 -- Ejecutar y verificar

```bash
./zero_args_problem
```

Salida esperada:

```
[BAD] error code: 404
[GCC] starting server
[GCC] error code: 404
starting server
error code: 404
```

Observa:

- `LOG_BAD` funciona solo cuando HAY argumentos variadicos (`code`)
- `LOG_GCC` funciona en ambos casos: `##` elimina la coma cuando `__VA_ARGS__`
  esta vacio
- `LOG_NOFIXED` funciona siempre, pero no puede agregar prefijo automatico
  porque `fmt` esta dentro de `__VA_ARGS__`

### Paso 2.6 -- Verificar el warning de -pedantic

`##__VA_ARGS__` es una extension GCC, no es C estandar. Compila con `-pedantic`
para ver si GCC lo advierte:

```bash
gcc -std=c11 -Wall -Wextra -pedantic zero_args_problem.c -o zero_args_problem 2>&1
```

Dependiendo de la version de GCC, puede o no producir un warning sobre la
extension `##`. En versiones recientes (GCC 13+) es comun que se acepte sin
warning. En versiones anteriores puede aparecer:

```
warning: ISO C99 requires at least one argument for the "..." in a variadic macro
```

Esto confirma que `##__VA_ARGS__` no es parte del estandar C11, aunque es
ampliamente soportado.

### Limpieza de la parte 2

```bash
rm -f zero_args_problem
```

---

## Parte 3 -- `__VA_OPT__` (C23)

**Objetivo**: Usar `__VA_OPT__`, la solucion estandar de C23 para el problema
de cero argumentos. Comparar con `##__VA_ARGS__`.

### Paso 3.1 -- Examinar el codigo fuente

```bash
cat va_opt_demo.c
```

Observa:

- `LOG` usa `__VA_OPT__(,)` -- se expande a una coma solo si hay args
- `DEBUG` siempre pasa `__FILE__` y `__LINE__` con una coma normal, y usa
  `__VA_OPT__(,)` solo para la coma antes de los argumentos extra

### Paso 3.2 -- Compilar con C23

`__VA_OPT__` requiere C23 (o GCC con `-std=gnu2x`/`-std=c2x`):

```bash
gcc -std=c2x -Wall -Wextra -Wpedantic va_opt_demo.c -o va_opt_demo
```

Debe compilar sin warnings ni errores. Si tu version de GCC no soporta
`-std=c2x`, prueba con `-std=gnu2x`.

### Paso 3.3 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- `LOG("server starting")` no tiene argumentos variadicos. A que se expande
  `__VA_OPT__(,)`? Y `__VA_ARGS__`?
- `LOG("port = %d", 8080)` tiene un argumento variadico. A que se expande
  `__VA_OPT__(,)` ahora?
- `DEBUG("entering main")` tiene `__FILE__` y `__LINE__` como argumentos
  fijos del `fprintf`, pero no tiene argumentos variadicos extra. Que pasa
  con el `__VA_OPT__(,)` despues de `__LINE__`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.4 -- Ejecutar y verificar

```bash
./va_opt_demo
```

Salida esperada:

```
[LOG] server starting
[LOG] ready
[LOG] port = 8080
[LOG] x = 42, name = test
[DBG va_opt_demo.c:26] entering main
[DBG va_opt_demo.c:27] x = 42
```

Observa:

- `LOG("server starting")` funciona sin coma extra -- `__VA_OPT__(,)` se
  expandio a nada
- `LOG("port = %d", 8080)` incluye la coma -- `__VA_OPT__(,)` se expandio
  a `","`
- `DEBUG` muestra el archivo y numero de linea automaticamente
- A diferencia de `##__VA_ARGS__`, `__VA_OPT__` es parte del estandar C23

### Paso 3.5 -- Intentar compilar con C11

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic va_opt_demo.c -o va_opt_demo 2>&1
```

Salida esperada (error):

```
va_opt_demo.c:~7: warning: __VA_OPT__ is not available until C23
```

Esto confirma que `__VA_OPT__` no esta disponible en C11 estandar. Para
proyectos que necesitan C11, `##__VA_ARGS__` sigue siendo la alternativa
practica.

### Limpieza de la parte 3

```bash
rm -f va_opt_demo
```

---

## Parte 4 -- Sistema de logging con niveles

**Objetivo**: Construir un sistema de logging completo con macros variadicos,
niveles condicionales (`LOG_ERROR`, `LOG_WARN`, `LOG_INFO`, `LOG_DEBUG`), y
control en tiempo de compilacion con `-DLOG_LEVEL`.

### Paso 4.1 -- Examinar el codigo fuente

```bash
cat log_system.c
```

Observa:

- Se definen 4 niveles: ERROR(1), WARN(2), INFO(3), DEBUG(4)
- `LOG_LEVEL` se define por defecto a 3 (INFO) si no se pasa con `-D`
- Los macros deshabilitados se reemplazan por `((void)0)` -- no generan codigo
- `LOG_ERROR` siempre esta activo (no tiene `#if`)
- `LOG_DEBUG` incluye `__func__` ademas de `__FILE__` y `__LINE__`

### Paso 4.2 -- Compilar con nivel por defecto (INFO)

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic log_system.c -o log_system
```

### Paso 4.3 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- Con `LOG_LEVEL=3` (INFO), que macros estaran activos? Cuales se expanden
  a `((void)0)`?
- Las llamadas a `LOG_DEBUG(...)` generaran alguna salida?
- `LOG_ERROR` se muestra siempre, sin importar el nivel. Por que?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.4 -- Ejecutar con nivel INFO (default)

```bash
./log_system
```

Salida esperada:

```
[INFO]  server starting
[INFO]  request 5 received
[INFO]  done
[INFO]  request 0 received
[WARN]  log_system.c:47: request id is zero, using default
[INFO]  done
[INFO]  request -1 received
[ERROR] log_system.c:43: invalid request id: -1
[INFO]  server shutting down
```

Observa:

- No aparecen mensajes `[DEBUG]` -- `LOG_DEBUG` se expandio a `((void)0)`
- `LOG_WARN` y `LOG_ERROR` incluyen archivo y linea
- `LOG_INFO` no incluye archivo/linea (decision de diseno para reducir ruido)

### Paso 4.5 -- Recompilar con nivel DEBUG

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -DLOG_LEVEL=4 log_system.c -o log_system
```

```bash
./log_system
```

Salida esperada:

```
[INFO]  server starting
[DEBUG] log_system.c:57 main(): compiled with LOG_LEVEL=4
[DEBUG] log_system.c:39 process_request(): processing request
[INFO]  request 5 received
[DEBUG] log_system.c:51 process_request(): request 5 completed
[INFO]  done
[DEBUG] log_system.c:39 process_request(): processing request
[INFO]  request 0 received
[WARN]  log_system.c:47: request id is zero, using default
[DEBUG] log_system.c:51 process_request(): request 1 completed
[INFO]  done
[DEBUG] log_system.c:39 process_request(): processing request
[INFO]  request -1 received
[ERROR] log_system.c:43: invalid request id: -1
[INFO]  server shutting down
```

Observa:

- Ahora aparecen los mensajes `[DEBUG]` con archivo, linea y nombre de funcion
- `-DLOG_LEVEL=4` definio `LOG_LEVEL` antes de que el preprocesador evaluara
  los `#if`, habilitando `LOG_DEBUG`
- Los numeros de linea en la salida apuntan a la linea exacta de la llamada
  en el codigo fuente

### Paso 4.6 -- Recompilar con nivel ERROR solamente

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -DLOG_LEVEL=1 log_system.c -o log_system
```

```bash
./log_system
```

Salida esperada:

```
[ERROR] log_system.c:43: invalid request id: -1
```

Solo aparece el unico error. Todas las llamadas a `LOG_WARN`, `LOG_INFO` y
`LOG_DEBUG` se expandieron a `((void)0)` -- no generaron codigo ejecutable,
ni siquiera la evaluacion de los argumentos.

### Limpieza de la parte 4

```bash
rm -f log_system
```

---

## Parte 5 -- `gcc -E` y expansion de macros

**Objetivo**: Usar `gcc -E` para ver como el preprocesador expande los macros
variadicos antes de la compilacion.

### Paso 5.1 -- Examinar el codigo fuente

```bash
cat gcc_expand.c
```

Este archivo es intencionalmente corto para que la salida de `gcc -E` sea
legible. Define dos macros (`LOG` y `DEBUG`) y los usa con y sin argumentos
variadicos.

### Paso 5.2 -- Ejecutar el preprocesador

```bash
gcc -std=c11 -E -P gcc_expand.c > expanded.c
```

Esto genera `expanded.c` con todo el codigo despues de la fase de
preprocesamiento: sin macros, sin `#include`, sin `#ifdef`. El flag `-P`
suprime los marcadores de linea (`# 1 "file.c"`) que GCC inserta por defecto,
produciendo una salida mas limpia para inspeccion manual.

### Paso 5.3 -- Predecir la expansion

Antes de ver el resultado, responde mentalmente:

- `LOG("starting")` usa `##__VA_ARGS__`. Como se ve la expansion cuando
  `__VA_ARGS__` esta vacio? Aparece la coma o no?
- `LOG("x = %d", x)` tiene un argumento variadico. A que se expande?
- `DEBUG("checkpoint")` usa `__FILE__` y `__LINE__`. Que valores concretos
  tendran despues del preprocesamiento?

Intenta predecir antes de continuar al siguiente paso.

### Paso 5.4 -- Inspeccionar la expansion

```bash
tail -20 expanded.c
```

Salida esperada:

```c
int main(void) {
    int x = 10;
    fprintf(stderr, "[LOG] " "starting" "\n");
    fprintf(stderr, "[LOG] " "x = %d" "\n", x);
    fprintf(stderr, "[DBG %s:%d] " "checkpoint" "\n", "gcc_expand.c", 16);
    fprintf(stderr, "[DBG %s:%d] " "value: %d" "\n", "gcc_expand.c", 17, x);
    return 0;
}
```

(Antes de `main` aparecen cientos de lineas con las declaraciones de `<stdio.h>`
expandidas. Lo relevante es la funcion `main` al final.)

Observa:

- `LOG("starting")` -- la coma antes de `##__VA_ARGS__` desaparecio (no hay
  `, )` al final)
- `LOG("x = %d", x)` -- `##__VA_ARGS__` se expandio a `, x`
- `DEBUG("checkpoint")` -- `__FILE__` se reemplazo por `"gcc_expand.c"` y
  `__LINE__` por `16` (el numero de linea real en el codigo fuente)
- Los string literals (`"[LOG] "`, `"starting"`, `"\n"`) siguen separados --
  el compilador los concatenara en la fase de compilacion, no el preprocesador

### Paso 5.5 -- Comparar con el codigo original

Compara lado a lado la macro y su expansion:

```
Macro:    LOG("starting")
Expandido: fprintf(stderr, "[LOG] " "starting" "\n");

Macro:    DEBUG("value: %d", x)
Expandido: fprintf(stderr, "[DBG %s:%d] " "value: %d" "\n", "gcc_expand.c", 17, x);
```

El preprocesador reemplazo:

1. El nombre del macro por el cuerpo de la definicion
2. `fmt` por el primer argumento (`"starting"` o `"value: %d"`)
3. `##__VA_ARGS__` por los argumentos restantes (nada o `, x`)
4. `__FILE__` por el nombre del archivo como string literal
5. `__LINE__` por el numero de linea como entero

### Paso 5.6 -- Compilar y ejecutar para confirmar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic gcc_expand.c -o gcc_expand
```

```bash
./gcc_expand
```

Salida esperada:

```
[LOG] starting
[LOG] x = 10
[DBG gcc_expand.c:16] checkpoint
[DBG gcc_expand.c:17] value: 10
```

La salida confirma que la expansion del preprocesador produjo codigo correcto.

### Limpieza de la parte 5

```bash
rm -f gcc_expand expanded.c
```

---

## Limpieza final

```bash
rm -f basic_variadic zero_args_problem va_opt_demo log_system gcc_expand
rm -f expanded.c
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  basic_variadic.c  gcc_expand.c  log_system.c  va_opt_demo.c  zero_args_problem.c
```

---

## Conceptos reforzados

1. Los macros variadicos usan `...` en la lista de parametros y `__VA_ARGS__`
   en la expansion para pasar un numero variable de argumentos. Introducidos
   en C99, son el mecanismo estandar para wrappers de `printf`/`fprintf`.

2. Cuando un macro tiene un parametro fijo seguido de `...`, llamarlo sin
   argumentos variadicos (`LOG("hello")`) produce una coma extra que es un
   error de sintaxis: `fprintf(stderr, "hello" "\n", )`.

3. `##__VA_ARGS__` (extension GCC/Clang) elimina la coma precedente cuando
   no hay argumentos variadicos. No es C estandar pero es de facto universal
   en compiladores modernos.

4. `__VA_OPT__(contenido)` (C23) se expande a `contenido` solo si hay
   argumentos variadicos, y a nada si no los hay. Es la solucion estandar
   al problema de cero argumentos. Requiere `-std=c2x` o superior.

5. La alternativa de no tener parametro fijo (`#define LOG(...) printf(__VA_ARGS__)`)
   evita el problema de la coma pero impide agregar prefijos o sufijos
   automaticos alrededor del formato.

6. Los macros de logging combinan `__FILE__`, `__LINE__` y `__func__` con
   `##__VA_ARGS__` para generar mensajes de diagnostico con contexto exacto
   del punto de llamada, sin overhead en runtime.

7. `((void)0)` como cuerpo de un macro deshabilitado garantiza que la
   llamada siga siendo una expresion valida (se puede usar en `if`/`else`,
   expresiones con coma, etc.) sin generar codigo ejecutable.

8. `gcc -E` muestra el codigo despues del preprocesamiento. Es la herramienta
   definitiva para depurar macros: revela la expansion real, los valores de
   `__FILE__`/`__LINE__`, y confirma que no quedan comas huerfanas.
