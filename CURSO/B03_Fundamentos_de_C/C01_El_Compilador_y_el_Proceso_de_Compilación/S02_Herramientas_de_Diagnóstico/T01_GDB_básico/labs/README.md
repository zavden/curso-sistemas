# Lab -- GDB basico

## Objetivo

Usar GDB para depurar programas C: poner breakpoints, avanzar paso a
paso, inspeccionar variables, analizar un crash con backtrace, y
rastrear cambios con watchpoints.

## Prerequisitos

- gcc instalado
- gdb instalado

## Archivos del laboratorio

| Archivo | Proposito |
|---|---|
| debug_basic.c | Programa con factorial() y process_array() -- breakpoints y step |
| debug_crash.c | Programa que crashea por NULL pointer -- backtrace |
| debug_watch.c | Programa con bug en nested loop -- watchpoints |

---

## Parte 1 -- Breakpoints y ejecucion paso a paso

### Objetivo

Aprender a compilar con simbolos de debug, poner breakpoints, y usar
next, step, print y finish para controlar la ejecucion.

### Paso 1.1: Compilar con simbolos de debug

```bash
gcc -g -O0 debug_basic.c -o debug_basic
```

Dos flags importantes:
- `-g` incluye informacion de debug (nombres de variables, lineas de codigo)
- `-O0` desactiva optimizaciones (el binario corresponde 1:1 con el fuente)

### Paso 1.2: Iniciar GDB

```bash
gdb ./debug_basic
```

Salida esperada (fragmento):

```
GNU gdb (GDB) ...
Reading symbols from ./debug_basic...
(gdb)
```

GDB queda esperando comandos. El programa todavia no se ejecuto.

### Paso 1.3: Poner breakpoint en main y ejecutar

```
(gdb) break main
```

Salida esperada:

```
Breakpoint 1 at 0x...: file debug_basic.c, line 29.
```

```
(gdb) run
```

Salida esperada:

```
Breakpoint 1, main () at debug_basic.c:29
29	    int num = 6;
```

La ejecucion se detuvo en la primera linea ejecutable de main().

### Paso 1.4: Avanzar con next

`next` ejecuta la linea actual sin entrar en funciones.

```
(gdb) next
```

Salida esperada:

```
30	    int fact = factorial(num);
```

Ahora estas en la linea 30, que llama a `factorial()`.

### Paso 1.5: Entrar en la funcion con step

Predice: si escribes `step` en la linea 30, donde terminaras? Y si
escribes `next`?

`step` entra en la funcion. `next` la ejecuta como caja negra.

```
(gdb) step
```

Salida esperada:

```
factorial (n=6) at debug_basic.c:13
13	    int result = 1;
```

Ahora estas dentro de `factorial()`. GDB muestra que el argumento
`n` vale 6.

### Paso 1.6: Inspeccionar variables con print

```
(gdb) print n
```

Salida esperada:

```
$1 = 6
```

```
(gdb) info args
```

Salida esperada:

```
n = 6
```

```
(gdb) info locals
```

Salida esperada:

```
result = 0
```

`result` todavia no fue asignado (la linea 13 no se ejecuto aun), por
eso muestra un valor sin inicializar.

### Paso 1.7: Avanzar dentro del loop

```
(gdb) next
```

Salida esperada:

```
14	    for (int i = 1; i <= n; i++) {
```

```
(gdb) next
```

Salida esperada:

```
15	        result *= i;
```

```
(gdb) print result
```

Salida esperada:

```
$2 = 1
```

```
(gdb) print i
```

Salida esperada:

```
$3 = 1
```

Ejecuta `next` dos veces mas para avanzar el loop y verifica como
cambian `result` e `i`:

```
(gdb) next
(gdb) next
(gdb) print i
(gdb) print result
```

### Paso 1.8: Volver de la funcion con finish

`finish` ejecuta hasta que la funcion actual retorne.

```
(gdb) finish
```

Salida esperada (fragmento):

```
Run till exit from #0  factorial (n=6) at debug_basic.c:...
...
Value returned is $... = 720
```

GDB muestra el valor de retorno (720). Ahora estas de vuelta en
main().

### Paso 1.9: Continuar hasta el final

```
(gdb) continue
```

Salida esperada:

```
factorial(6) = 720
sum of array = 150
done
[Inferior 1 (process ...) exited normally]
```

El programa termino. `continue` ejecuta hasta el proximo breakpoint o
hasta el final.

### Paso 1.10: Salir de GDB

```
(gdb) quit
```

### Limpieza

```bash
rm -f debug_basic
```

---

## Parte 2 -- Inspeccionar datos

### Objetivo

Usar print con arrays, formatos de impresion, set var para modificar
valores en runtime, y list para ver el codigo fuente.

### Paso 2.1: Compilar y abrir en GDB

```bash
gcc -g -O0 debug_basic.c -o debug_basic
gdb ./debug_basic
```

### Paso 2.2: Poner breakpoint en process_array

```
(gdb) break process_array
(gdb) run
```

Salida esperada:

```
factorial(6) = 720

Breakpoint 1, process_array (arr=0x7fffffffd530, n=5) at debug_basic.c:21
21	    int sum = 0;
```

La primera llamada (`factorial`) se ejecuto normalmente. GDB paro
al entrar en `process_array`.

### Paso 2.3: Inspeccionar argumentos

```
(gdb) info args
```

Salida esperada:

```
arr = 0x7fffffffd530
n = 5
```

`arr` es un puntero. Para ver su contenido hay que desreferenciarlo.

### Paso 2.4: Ver el array completo

```
(gdb) print *arr@5
```

Salida esperada:

```
$1 = {10, 20, 30, 40, 50}
```

La sintaxis `*arr@5` significa: desreferenciar `arr` y mostrar 5
elementos consecutivos.

### Paso 2.5: Avanzar al loop e inspeccionar elementos

```
(gdb) next
(gdb) next
(gdb) next
(gdb) print i
```

Salida esperada:

```
$2 = 0
```

```
(gdb) print arr[i]
```

Salida esperada:

```
$3 = 10
```

```
(gdb) print sum
```

Salida esperada:

```
$4 = 0
```

### Paso 2.6: Formato hexadecimal

```
(gdb) print/x arr[0]
```

Salida esperada:

```
$5 = 0xa
```

10 en decimal = 0xa en hexadecimal.

```
(gdb) print/x arr[2]
```

Salida esperada:

```
$6 = 0x1e
```

30 en decimal = 0x1e en hexadecimal.

### Paso 2.7: Modificar una variable con set var

Predice: si cambias `n` de 5 a 3 con `set var`, cuantos elementos
sumara el loop?

```
(gdb) print n
```

Salida esperada:

```
$7 = 5
```

```
(gdb) set var n = 3
(gdb) print n
```

Salida esperada:

```
$8 = 3
```

```
(gdb) continue
```

Salida esperada:

```
sum of array = 60
done
[Inferior 1 (process ...) exited normally]
```

El programa sumo solo 3 elementos (10 + 20 + 30 = 60) en vez de
5 (150). `set var` modifica el valor en runtime sin recompilar.

### Paso 2.8: Ver el codigo fuente con list

Reinicia el programa para probar `list`:

```
(gdb) break process_array
(gdb) run
```

```
(gdb) list
```

Salida esperada (fragmento):

```
16	}
17
18	int process_array(int *arr, int n) {
19	    int sum = 0;
20	    for (int i = 0; i < n; i++) {
21	        sum += arr[i];
22	    }
23	    return sum;
24	}
```

`list` muestra 10 lineas del codigo fuente alrededor de la posicion
actual. Util para orientarse sin salir de GDB.

### Paso 2.9: Salir

```
(gdb) quit
```

Responde `y` si pregunta si quieres terminar el proceso.

### Limpieza

```bash
rm -f debug_basic
```

---

## Parte 3 -- Depurar un segfault con backtrace

### Objetivo

Ejecutar un programa que crashea, usar backtrace para ver la cadena de
llamadas, y navegar entre frames para encontrar la causa del crash.

### Paso 3.1: Compilar debug_crash.c

```bash
gcc -g -O0 debug_crash.c -o debug_crash
```

### Paso 3.2: Ejecutarlo fuera de GDB

```bash
./debug_crash
```

Salida esperada:

```
Segmentation fault (core dumped)
```

O simplemente `Segmentation fault`. El programa crashea pero no dice
donde ni por que.

### Paso 3.3: Ejecutarlo dentro de GDB

```bash
gdb ./debug_crash
```

```
(gdb) run
```

Salida esperada:

```
starting program...

Program received signal SIGSEGV, Segmentation fault.
0x... in access_data (p=0x0) at debug_crash.c:15
15	    int value = *p;    /* crashes here if p is NULL */
```

GDB muestra exactamente donde ocurrio el crash: linea 15 de
debug_crash.c, dentro de `access_data`. Nota que `p=0x0` -- el
puntero es NULL.

### Paso 3.4: Backtrace

Predice: `debug_crash.c` tiene la cadena main() -> caller() ->
process() -> access_data(). Si ejecutas `bt`, que frames esperas ver
y en que orden?

```
(gdb) backtrace
```

Salida esperada:

```
#0  0x... in access_data (p=0x0) at debug_crash.c:15
#1  0x... in process (data=0x0) at debug_crash.c:20
#2  0x... in caller () at debug_crash.c:26
#3  0x... in main () at debug_crash.c:32
```

El backtrace se lee de abajo hacia arriba:
- Frame #3: `main()` llamo a `caller()` (linea 32)
- Frame #2: `caller()` llamo a `process(ptr)` (linea 26)
- Frame #1: `process()` llamo a `access_data(data)` (linea 20)
- Frame #0: `access_data()` crasheo al desreferenciar `p` (linea 15)

### Paso 3.5: Navegar frames con frame

```
(gdb) frame 0
```

Salida esperada:

```
#0  0x... in access_data (p=0x0) at debug_crash.c:15
15	    int value = *p;    /* crashes here if p is NULL */
```

```
(gdb) print p
```

Salida esperada:

```
$1 = (int *) 0x0
```

`p` es NULL (0x0). La desreferencia `*p` causa el segfault.

### Paso 3.6: Rastrear el origen del NULL

Quien paso ese NULL? Sube un frame:

```
(gdb) frame 1
```

Salida esperada:

```
#1  0x... in process (data=0x0) at debug_crash.c:20
20	    int result = access_data(data);
```

```
(gdb) print data
```

Salida esperada:

```
$2 = (int *) 0x0
```

`process()` recibio `data` como NULL y lo paso a `access_data()`.
Sube otro frame:

```
(gdb) frame 2
```

Salida esperada:

```
#2  0x... in caller () at debug_crash.c:26
26	    int answer = process(ptr);
```

```
(gdb) print ptr
```

Salida esperada:

```
$3 = (int *) 0x0
```

Aqui esta la causa raiz: `caller()` declaro `int *ptr = NULL` (linea
25) y lo paso a `process()` sin validar.

### Paso 3.7: Resumen de la investigacion

Con tres comandos (`bt`, `frame`, `print`) se identifico:
- Donde crasheo: `access_data()`, linea 15
- Que causo el crash: desreferenciar un puntero NULL
- Quien origino el NULL: `caller()`, linea 25

### Paso 3.8: Salir

```
(gdb) quit
```

### Limpieza

```bash
rm -f debug_crash
```

---

## Parte 4 -- Watchpoints

### Objetivo

Usar watchpoints para rastrear cuando una variable cambia de valor
y encontrar un bug donde un contador se vuelve negativo
inesperadamente.

### Paso 4.1: Examinar el codigo antes de depurar

Abre `debug_watch.c` y lee el codigo. El programa tiene un loop
externo que incrementa `counter` en 3, y un loop interno que a veces
resta 7.

Predice: al finalizar, counter sera positivo o negativo? En que
iteracion del loop externo (valor de `i`) ocurrira el primer valor
negativo?

### Paso 4.2: Compilar y ejecutar sin GDB

```bash
gcc -g -O0 debug_watch.c -o debug_watch
./debug_watch
```

Salida esperada:

```
counter = -11
total_ops = 13
ERROR: counter went negative!
```

El contador termino negativo. Hay que encontrar donde ocurre la
primera transicion a negativo.

### Paso 4.3: Iniciar GDB y poner breakpoint en main

```bash
gdb ./debug_watch
```

```
(gdb) break main
(gdb) run
```

Salida esperada:

```
Breakpoint 1, main () at debug_watch.c:17
17	    int counter = 0;
```

### Paso 4.4: Poner watchpoint en counter

```
(gdb) watch counter
```

Salida esperada:

```
Hardware watchpoint 2: counter
```

GDB usara un watchpoint de hardware (es mas eficiente que software).
Cada vez que `counter` cambie de valor, GDB pausara la ejecucion.

### Paso 4.5: Observar los cambios

```
(gdb) continue
```

Salida esperada:

```
Hardware watchpoint 2: counter

Old value = 0
New value = 3
main () at debug_watch.c:22
22	        total_ops++;
```

`counter` paso de 0 a 3 (primera iteracion del loop externo).

```
(gdb) continue
```

Salida esperada:

```
Hardware watchpoint 2: counter

Old value = 3
New value = 6
main () at debug_watch.c:22
22	        total_ops++;
```

Sigue incrementando. Ejecuta `continue` varias veces mas y observa
el patron. En algun momento veras una resta:

```
Hardware watchpoint 2: counter

Old value = 12
New value = 5
main () at debug_watch.c:27
27	                total_ops++;
```

Aqui `counter` paso de 12 a 5. La linea 27 esta dentro del loop
interno, donde se resta 7 (linea 26: `counter -= 7`).

### Paso 4.6: Usar watchpoint condicional

Reinicia el programa para ir directamente al primer valor negativo:

```
(gdb) delete
```

Responde `y` para eliminar todos los breakpoints/watchpoints.

```
(gdb) break main
(gdb) run
```

Responde `y` para reiniciar el programa.

```
(gdb) watch counter if counter < 0
```

Salida esperada:

```
Hardware watchpoint 3: counter
```

Este watchpoint solo pausara cuando `counter` cambie Y el nuevo
valor sea menor que 0.

```
(gdb) continue
```

Salida esperada:

```
Hardware watchpoint 3: counter

Old value = 4
New value = -3
main () at debug_watch.c:27
27	                total_ops++;
```

La primera transicion a negativo: `counter` paso de 4 a -3.

### Paso 4.7: Identificar la iteracion del bug

```
(gdb) print i
```

Salida esperada:

```
$1 = 5
```

```
(gdb) print j
```

Salida esperada:

```
$2 = 0
```

```
(gdb) print counter
```

Salida esperada:

```
$3 = -3
```

El bug ocurre en la iteracion `i=5`, `j=0` del nested loop. En esa
iteracion `(i + j) % 5 == 0` es verdadero, y `counter -= 7` resta
demasiado porque `counter` solo valia 4 en ese punto (3 del
incremento del loop externo + 1 acumulado).

### Paso 4.8: Salir

```
(gdb) quit
```

### Limpieza

```bash
rm -f debug_watch
```

---

## Limpieza final

Verifica que no queden binarios:

```bash
ls -la debug_basic debug_crash debug_watch 2>&1
```

Si alguno existe, eliminalo:

```bash
rm -f debug_basic debug_crash debug_watch
```

---

## Conceptos reforzados

1. Compilar con `-g -O0` es requisito para depurar con GDB. `-g`
   incluye simbolos de debug y `-O0` evita que el compilador reordene
   o elimine codigo.

2. `next` ejecuta la linea actual sin entrar en funciones. `step`
   entra en la funcion. `finish` ejecuta hasta que la funcion actual
   retorne. Son tres niveles de granularidad para avanzar.

3. `print *arr@N` muestra N elementos consecutivos desde un puntero.
   `print/x` muestra valores en hexadecimal. `set var` permite
   cambiar valores en runtime sin recompilar.

4. Cuando un programa crashea por segfault, `backtrace` muestra la
   cadena completa de llamadas. `frame N` permite navegar a cualquier
   punto de esa cadena para inspeccionar variables con `print`.

5. Un watchpoint pausa la ejecucion cada vez que una variable cambia
   de valor. `watch var if condicion` es un watchpoint condicional que
   solo pausa cuando la condicion es verdadera. Ambos son mas
   eficientes que poner breakpoints manuales para rastrear cambios.
