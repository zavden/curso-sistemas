# Lab -- AddressSanitizer y UBSan

## Objetivo

Compilar programas con errores de memoria y comportamiento indefinido, usar
ASan y UBSan para detectarlos, aprender a leer sus reportes, y entender
cuando cada herramienta tiene ventaja sobre Valgrind.

## Prerequisitos

- GCC con soporte de sanitizers
- Valgrind instalado (para las comparaciones)
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `heap_errors.c` | Tres errores de heap: buffer overflow, use-after-free, double-free |
| `stack_errors.c` | Stack buffer overflow (lo que Valgrind no detecta) |
| `ub_errors.c` | Cuatro tipos de UB: signed overflow, div/0, shift invalido, null deref |
| `combined.c` | Errores de memoria + UB juntos |

---

## Parte 1 -- ASan: errores de heap

**Objetivo**: Detectar errores de memoria en el heap con ASan e interpretar
sus reportes.

### Paso 1.1 -- Verificar el entorno

```bash
gcc --version
```

Confirma que GCC esta instalado. Verifica que los sanitizers estan disponibles:

```bash
echo 'int main(void){return 0;}' | gcc -fsanitize=address -x c - -o /dev/null
echo $?
```

Si imprime `0`, los sanitizers funcionan.

### Paso 1.2 -- Examinar el codigo fuente

```bash
cat heap_errors.c
```

Observa la estructura: tres funciones independientes (`heap_buffer_overflow`,
`use_after_free`, `double_free`) seleccionables por argumento de linea de
comandos.

### Paso 1.3 -- Compilar con ASan

```bash
gcc -fsanitize=address -g heap_errors.c -o heap_errors
```

Los flags:

- `-fsanitize=address` activa AddressSanitizer
- `-g` incluye informacion de debug (numeros de linea en el reporte)

### Paso 1.4 -- Ejecutar: heap-buffer-overflow

```bash
./heap_errors 1
```

Salida esperada (los numeros de direccion varian):

```
=================================================================
==XXXXX==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x60300000XXX
WRITE of size 4 at 0x60300000XXX thread T0
    #0 0x... in heap_buffer_overflow heap_errors.c:25
    #1 0x... in main heap_errors.c:68
    ...

0x60300000XXX is located 0 bytes after 20-byte region [0x...,0x...)
allocated by thread T0 here:
    #0 0x... in malloc (...)
    #1 0x... in heap_buffer_overflow heap_errors.c:22
```

Como leer el reporte:

1. **Tipo de error**: `heap-buffer-overflow`
2. **Operacion**: `WRITE of size 4` (escribio un `int` de 4 bytes)
3. **Donde ocurrio**: `heap_errors.c:25` (la linea del acceso ilegal)
4. **Contexto de la memoria**: `0 bytes after 20-byte region` -- justo despues del bloque de 20 bytes (5 ints)
5. **Donde se alloco**: `heap_errors.c:22` (la linea del `malloc`)

### Paso 1.5 -- Ejecutar: use-after-free

```bash
./heap_errors 2
```

Salida esperada:

```
==XXXXX==ERROR: AddressSanitizer: heap-use-after-free on address 0x60200000XXX
READ of size 4 at 0x60200000XXX thread T0
    #0 0x... in use_after_free heap_errors.c:42
    ...

0x60200000XXX is located 0 bytes inside of 4-byte region
freed by thread T0 here:
    #0 0x... in free (...)
    #1 0x... in use_after_free heap_errors.c:40

previously allocated by thread T0 here:
    #0 0x... in malloc (...)
    #1 0x... in use_after_free heap_errors.c:35
```

ASan muestra tres puntos clave: donde se uso, donde se libero, y donde se
alloco originalmente.

### Paso 1.6 -- Ejecutar: double-free

```bash
./heap_errors 3
```

Salida esperada:

```
==XXXXX==ERROR: AddressSanitizer: attempting double-free on 0x61500000XXX
    #0 0x... in free (...)
    #1 0x... in double_free heap_errors.c:53

0x61500000XXX is located 0 bytes inside of 64-byte region
freed by thread T0 here:
    #0 0x... in free (...)
    #1 0x... in double_free heap_errors.c:52
```

### Paso 1.7 -- Comparar: sin sanitizer

Compila el mismo programa sin sanitizer y ejecuta:

```bash
gcc -g heap_errors.c -o heap_errors_nosanit
./heap_errors_nosanit 1
```

Antes de ejecutar, predice: el heap-buffer-overflow va a causar un crash,
va a imprimir basura, o va a funcionar "normalmente"?

### Paso 1.8 -- Verificar la prediccion

Lo mas probable es que el programa funcione sin errores aparentes. El
acceso `arr[5]` escribe en memoria que pertenece a las estructuras internas
de malloc -- puede funcionar silenciosamente miles de veces y corromper
datos mucho despues. Esta es la razon por la que los sanitizers son
indispensables: el programa "funciona" pero tiene un bug real.

### Paso 1.9 -- Comparar: Valgrind

```bash
valgrind --tool=memcheck ./heap_errors_nosanit 1
```

Salida esperada:

```
==XXXXX== Invalid write of size 4
==XXXXX==    at 0x...: heap_buffer_overflow (heap_errors.c:25)
==XXXXX==    by 0x...: main (heap_errors.c:68)
==XXXXX==  Address 0x... is 0 bytes after a block of size 20 alloc'd
```

Valgrind tambien detecta heap-buffer-overflow. Pero nota la diferencia de
velocidad: ASan es ~2x mas lento que la ejecucion normal; Valgrind es ~20-30x
mas lento.

**Nota**: no ejecutes un binario compilado con ASan bajo Valgrind. Los dos
instrumentan la memoria de formas incompatibles. Usa el binario compilado
sin sanitizer para las pruebas con Valgrind.

### Limpieza de la parte 1

```bash
rm -f heap_errors heap_errors_nosanit
```

---

## Parte 2 -- ASan: errores de stack (lo que Valgrind no detecta)

**Objetivo**: Demostrar la ventaja principal de ASan sobre Valgrind: la
deteccion de accesos fuera de limites en arrays locales (stack).

### Paso 2.1 -- Compilar con ASan

```bash
gcc -fsanitize=address -g stack_errors.c -o stack_errors
```

### Paso 2.2 -- Ejecutar con ASan

```bash
./stack_errors
```

Salida esperada:

```
Legitimate access:
  arr[0] = 10
  arr[1] = 20
  arr[2] = 30
  arr[3] = 40
  arr[4] = 50

Out-of-bounds access:
==XXXXX==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x7ffd...
READ of size 4 at 0x7ffd... thread T0
    #0 0x... in main stack_errors.c:31

Address 0x7ffd... is located in stack of thread T0 at offset ... in frame
    #0 0x... in main stack_errors.c:17

  This frame has 1 object(s):
    [32, 52) 'arr' (line 18)     <== Memory access at offset ... overflows this variable
```

ASan identifica:

- La variable exacta que se desbordo (`arr`, linea 18)
- El tamano real del array ([32, 52) = 20 bytes = 5 ints)
- Que el acceso cae fuera de ese rango

### Paso 2.3 -- Compilar sin sanitizer

```bash
gcc -g stack_errors.c -o stack_errors_nosanit
```

Antes de ejecutar, predice: que va a pasar? El programa crashea, imprime
basura, o imprime un numero "normal"?

### Paso 2.4 -- Verificar sin sanitizer

```bash
./stack_errors_nosanit
```

Lo mas probable: el programa imprime un numero cualquiera sin errores.
`arr[8]` lee un valor de la pila que pertenece a otra variable o al frame
del llamador. No hay crash porque sigue siendo memoria del proceso.

### Paso 2.5 -- Probar con Valgrind

Antes de ejecutar, predice: Valgrind detectara el acceso fuera de limites
en un array local del stack?

```bash
valgrind --tool=memcheck ./stack_errors_nosanit
```

### Paso 2.6 -- Verificar la prediccion

Salida esperada:

```
==XXXXX== ERROR SUMMARY: 0 errors from 0 contexts
```

Valgrind NO detecta el error. Reporta 0 errores. Esto es porque Valgrind
instrumenta las funciones de heap (`malloc`, `free`) -- no tiene visibilidad
sobre los accesos a variables locales del stack.

**Este es el caso de uso principal de ASan**: errores de stack que pasan
completamente desapercibidos tanto para el programador como para Valgrind.
En una base de codigo real, un stack overflow puede corromper el return
address o variables adyacentes y causar bugs intermitentes e imposibles
de reproducir.

### Limpieza de la parte 2

```bash
rm -f stack_errors stack_errors_nosanit
```

---

## Parte 3 -- UBSan: comportamiento indefinido

**Objetivo**: Detectar comportamiento indefinido con UBSan, entender la
diferencia entre el modo warn-and-continue y el modo abort.

### Paso 3.1 -- Compilar con UBSan

```bash
gcc -fsanitize=undefined -g ub_errors.c -o ub_errors
```

### Paso 3.2 -- Signed overflow

```bash
./ub_errors 1
```

Salida esperada:

```
INT_MAX     = 2147483647
ub_errors.c:28:17: runtime error: signed integer overflow:
2147483647 + 1 cannot be represented in type 'int'
INT_MAX + 1 = -2147483648
```

Observa: UBSan imprime el warning **y sigue ejecutando**. El resultado
`-2147483648` es el wrap-around de complemento a dos, pero el estandar C
no lo garantiza -- es UB. Sin UBSan, el compilador puede optimizar asumiendo
que el overflow nunca ocurre.

### Paso 3.3 -- Division por cero

```bash
./ub_errors 2
```

Salida esperada:

```
ub_errors.c:34:41: runtime error: division by zero
```

En este caso el programa probablemente recibe SIGFPE y termina, ya que la
division por cero suele generar una excepcion de hardware ademas del
reporte de UBSan.

### Paso 3.4 -- Shift invalido

```bash
./ub_errors 3
```

Salida esperada:

```
ub_errors.c:40:17: runtime error: shift exponent 32 is too large
    for 32-bit type 'int'
1 << 32 = ...
ub_errors.c:43:17: runtime error: shift exponent -1 is negative
1 << -1 = ...
```

UBSan detecta ambos: shift mayor o igual al ancho del tipo, y shift
negativo. Ambos son UB segun el estandar.

### Paso 3.5 -- Null dereference

```bash
./ub_errors 4
```

Salida esperada:

```
ub_errors.c:49:28: runtime error: load of null pointer of type 'int'
```

Seguido probablemente de SIGSEGV. UBSan identifica la linea exacta y el
tipo de puntero.

### Paso 3.6 -- Ejecutar todos secuencialmente

Antes de ejecutar, predice: si usamos la opcion `0` (todos), UBSan
mostrara los 4 errores o se detendra en el primero?

```bash
./ub_errors 0
```

### Paso 3.7 -- Verificar la prediccion

Sin `-fno-sanitize-recover=all`, UBSan imprime un warning y **continua**.
Deberia mostrar el signed overflow y seguir. Sin embargo, la division por
cero y el null deref suelen causar senales del sistema (SIGFPE, SIGSEGV)
que terminan el programa independientemente de UBSan.

### Paso 3.8 -- Modo abort: -fno-sanitize-recover=all

Recompila con la opcion que hace que UBSan aborte al primer error:

```bash
gcc -fsanitize=undefined -fno-sanitize-recover=all -g ub_errors.c -o ub_errors_strict
./ub_errors_strict 1
```

Salida esperada:

```
INT_MAX     = 2147483647
ub_errors.c:28:17: runtime error: signed integer overflow:
2147483647 + 1 cannot be represented in type 'int'
```

El programa aborta inmediatamente despues del error. No llega a imprimir
`INT_MAX + 1 = ...`. Sin esta opcion, UBSan funciona como un "linter
de runtime": reporta pero no detiene.

### Paso 3.9 -- Comparar: sin sanitizer

```bash
gcc -g ub_errors.c -o ub_errors_nosanit
./ub_errors_nosanit 1
```

Salida esperada:

```
INT_MAX     = 2147483647
INT_MAX + 1 = -2147483648
```

Sin UBSan no hay ningun warning. El programa "funciona" pero el
comportamiento no esta definido por el estandar y puede cambiar con
otro compilador, otra arquitectura, u otro nivel de optimizacion.

### Limpieza de la parte 3

```bash
rm -f ub_errors ub_errors_strict ub_errors_nosanit
```

---

## Parte 4 -- ASan + UBSan combinados

**Objetivo**: Usar ambos sanitizers juntos, configurar opciones de runtime,
y definir la linea de compilacion recomendada para desarrollo.

### Paso 4.1 -- Examinar el codigo

```bash
cat combined.c
```

El programa tiene dos errores:

1. Signed overflow (UB -- detectado por UBSan)
2. Heap buffer overflow (memoria -- detectado por ASan)

### Paso 4.2 -- Compilar con ambos sanitizers

```bash
gcc -fsanitize=address,undefined -g -O0 combined.c -o combined
```

Nota los flags:

- `-fsanitize=address,undefined` activa ambos sanitizers
- `-g` informacion de debug
- `-O0` sin optimizacion -- las optimizaciones pueden eliminar o reordenar
  el UB, haciendo que UBSan no lo vea

### Paso 4.3 -- Ejecutar

Antes de ejecutar, predice: cual error se reporta primero? El signed
overflow o el heap buffer overflow?

```bash
./combined
```

### Paso 4.4 -- Verificar la prediccion

Salida esperada:

```
combined.c:20:17: runtime error: signed integer overflow:
2147483647 + 1 cannot be represented in type 'int'
INT_MAX + 1 = -2147483648 (UBSan should warn above)
=================================================================
==XXXXX==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x...
WRITE of size 4 at 0x... thread T0
    #0 0x... in main combined.c:29
```

UBSan reporta primero (el overflow ocurre antes en el codigo). UBSan
imprime el warning y continua. Luego ASan detecta el heap overflow y
**aborta** -- ASan siempre aborta al primer error de memoria.

### Paso 4.5 -- Linea de compilacion recomendada para desarrollo

```bash
gcc -Wall -Wextra -Wpedantic -std=c11 \
    -g -O0 \
    -fsanitize=address,undefined \
    -fno-sanitize-recover=all \
    combined.c -o combined_dev
```

Este es el comando completo para desarrollo:

| Flag | Proposito |
|------|-----------|
| `-Wall -Wextra -Wpedantic` | Warnings exhaustivos en compilacion |
| `-std=c11` | Estandar C11 estricto |
| `-g` | Informacion de debug (lineas en reportes) |
| `-O0` | Sin optimizacion (las optimizaciones pueden ocultar UB) |
| `-fsanitize=address,undefined` | Deteccion de errores de memoria + UB |
| `-fno-sanitize-recover=all` | Abortar al primer error (no solo warning) |

```bash
./combined_dev
```

Ahora ambos sanitizers abortan al primer error.

### Paso 4.6 -- Variables de entorno para runtime

Las opciones de runtime se configuran sin recompilar:

```bash
ASAN_OPTIONS="halt_on_error=0" ./combined
```

Con `halt_on_error=0`, ASan imprime el error pero intenta continuar
(esto no es confiable -- la memoria ya esta corrupta, pero puede revelar
multiples errores en una sola ejecucion).

```bash
UBSAN_OPTIONS="print_stacktrace=1" ./combined
```

Con `print_stacktrace=1`, UBSan incluye un stack trace completo en cada
warning (por defecto solo muestra archivo y linea).

Combinar ambas:

```bash
ASAN_OPTIONS="halt_on_error=0:detect_leaks=1" \
UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1" \
./combined
```

### Paso 4.7 -- Comparar: compilacion de produccion

```bash
gcc -Wall -Wextra -std=c11 -O2 -DNDEBUG combined.c -o combined_prod
```

Este binario no tiene sanitizers. Es mas rapido y mas pequeno:

```bash
ls -l combined combined_prod
```

Salida esperada (los tamanos varian):

```
-rwx... ~30K combined
-rwx... ~16K combined_prod
```

El binario con sanitizers es notablemente mas grande porque incluye toda
la instrumentacion de ASan y UBSan.

```bash
./combined_prod
```

El programa "funciona" -- el signed overflow produce un numero sin warning
y el heap overflow escribe silenciosamente fuera de limites. Los sanitizers
son herramientas de desarrollo, no de produccion.

### Limpieza de la parte 4

```bash
rm -f combined combined_dev combined_prod
```

---

## Limpieza final

```bash
rm -f heap_errors heap_errors_nosanit
rm -f stack_errors stack_errors_nosanit
rm -f ub_errors ub_errors_strict ub_errors_nosanit
rm -f combined combined_dev combined_prod
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  combined.c  heap_errors.c  stack_errors.c  ub_errors.c
```

---

## Conceptos reforzados

1. ASan detecta errores de memoria (heap-buffer-overflow, use-after-free,
   double-free, stack-buffer-overflow) agregando "red zones" alrededor de
   cada bloque de memoria durante la compilacion. El overhead es ~2x.

2. Valgrind detecta errores de heap pero NO detecta stack-buffer-overflow
   porque solo instrumenta `malloc`/`free`. ASan detecta ambos porque
   instrumenta el codigo en compilacion, incluyendo variables locales.

3. UBSan detecta comportamiento indefinido (signed overflow, division por
   cero, shift invalido, null deref) en runtime. Por defecto solo imprime
   un warning y continua; con `-fno-sanitize-recover=all` aborta al primer
   error.

4. ASan y UBSan se pueden usar juntos con `-fsanitize=address,undefined`.
   UBSan reporta sus errores como warnings (o aborta si se configura);
   ASan siempre aborta al primer error de memoria que detecta.

5. La linea de compilacion recomendada para desarrollo es:
   `gcc -Wall -Wextra -Wpedantic -std=c11 -g -O0 -fsanitize=address,undefined -fno-sanitize-recover=all`.
   Para produccion se elimina `-fsanitize` y se usa `-O2`.

6. Las opciones de runtime (`ASAN_OPTIONS`, `UBSAN_OPTIONS`) permiten
   ajustar el comportamiento de los sanitizers sin recompilar: desactivar
   leak detection, activar stack traces, o permitir que ASan continue
   despues de un error.
