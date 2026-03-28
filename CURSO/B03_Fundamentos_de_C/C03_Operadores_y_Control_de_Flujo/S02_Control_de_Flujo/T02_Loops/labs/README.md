# Lab — Loops

## Objetivo

Practicar los tres tipos de loops en C, entender sus diferencias semanticas,
usar break y continue para controlar el flujo dentro de loops, y aplicar
patrones comunes como recorrer strings y validar input. Al finalizar, sabras
elegir el loop correcto para cada situacion y evitar errores clasicos como
loops infinitos accidentales.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `for_variations.c` | Variaciones del for: conteo arriba/abajo, paso variable, dos variables |
| `for_optional_parts.c` | Partes opcionales del for: sin init, sin update, loop infinito controlado |
| `while_vs_dowhile.c` | Comparacion directa entre while y do-while con condicion falsa |
| `break_continue.c` | break para buscar el primer negativo, continue para filtrar |
| `nested_break.c` | break en loops anidados: problema y solucion con flag |
| `string_traverse.c` | Recorrer strings con indice y con puntero, contar tipos de caracteres |
| `sentinel_search.c` | Busqueda lineal con retorno -1 como sentinel value |
| `input_validation.c` | Validacion de input del usuario con do-while |

---

## Parte 1 — for: variaciones y partes opcionales

**Objetivo**: Explorar las distintas formas de usar el for: conteo con paso
variable, multiples variables, y omision de secciones.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat for_variations.c
```

Observa las cuatro variaciones:

- Conteo ascendente (0 a 4)
- Conteo descendente (4 a 0)
- Paso de 3 en 3
- Dos variables moviendose en direcciones opuestas

Antes de compilar, predice:

- En el loop "Step by 3", cual es el ultimo valor que se imprime?
- En el loop "Two variables", cuantas lineas imprime?

### Paso 1.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic for_variations.c -o for_variations
./for_variations
```

Salida esperada:

```
--- Count up ---
0 1 2 3 4
--- Count down ---
4 3 2 1 0
--- Step by 3 ---
0 3 6 9 12
--- Two variables ---
lo=0 hi=9
lo=1 hi=8
lo=2 hi=7
lo=3 hi=6
lo=4 hi=5
```

Verifica tus predicciones:

- "Step by 3" imprime 12 como ultimo valor (15 ya no cumple `i < 15`)
- "Two variables" imprime 5 lineas: cuando `lo` llega a 5 y `hi` llega a 4,
  la condicion `lo < hi` es falsa

### Paso 1.3 — Partes opcionales del for

```bash
cat for_optional_parts.c
```

Observa los tres casos:

- Sin inicializacion: la variable se declara antes del for
- Sin actualizacion: el incremento esta dentro del cuerpo
- Sin condicion: es un loop infinito que sale con break

Antes de ejecutar, predice:

- En "No update", que valores se imprimen? (pista: `j` empieza en 0 y salta de 3 en 3)
- En "No condition", cuantas veces se imprime "iteration"?

### Paso 1.4 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic for_optional_parts.c -o for_optional_parts
./for_optional_parts
```

Salida esperada:

```
--- No init ---
10 11 12 13 14
--- No update ---
0 3 6 9
--- No condition (infinite, break at 3) ---
iteration 0
iteration 1
iteration 2
```

- "No update": imprime 0, 3, 6, 9. Cuando `j` llega a 12, la condicion
  `j < 10` es falsa (12 no es menor que 10)
- "No condition": `for (;;)` es un loop infinito. Sin el `break`, nunca
  terminaria. El break sale cuando `count >= 3`

### Limpieza de la parte 1

```bash
rm -f for_variations for_optional_parts
```

---

## Parte 2 — while y do-while

**Objetivo**: Ver la diferencia semantica entre while (0 o mas ejecuciones) y
do-while (1 o mas ejecuciones), y entender cuando usar cada uno.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat while_vs_dowhile.c
```

El programa compara ambos loops en dos escenarios:

1. Condicion falsa desde el inicio (`x = 100`, condicion `< 10`)
2. Conteo del 1 al 5 (ambos producen el mismo resultado)

Antes de ejecutar, predice:

- Con `x = 100` y condicion `x < 10`, cuantas veces ejecuta el cuerpo el while?
- Con `y = 100` y condicion `y < 10`, cuantas veces ejecuta el cuerpo el do-while?

### Paso 2.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic while_vs_dowhile.c -o while_vs_dowhile
./while_vs_dowhile
```

Salida esperada:

```
=== while with false condition ===
while finished. Executed 0 times.

=== do-while with false condition ===
do-while body: y=100
do-while finished. Executed 1 time.

=== while counting to 5 ===
1 2 3 4 5

=== do-while counting to 5 ===
1 2 3 4 5
```

La diferencia clave:

- **while** evalua la condicion **antes** de la primera iteracion. Si es falsa
  desde el inicio, el cuerpo nunca se ejecuta (0 veces)
- **do-while** ejecuta el cuerpo **al menos una vez** y luego evalua la
  condicion. Incluso con condicion falsa, imprime `y=100`

Cuando la condicion es verdadera desde el inicio (conteo 1 a 5), ambos
producen el mismo resultado.

### Paso 2.3 — Cuando usar cada uno

Regla general:

- **for**: cuando sabes cuantas iteraciones (contador con inicio, fin y paso claros)
- **while**: cuando no sabes cuantas iteraciones (leer hasta EOF, buscar un valor)
- **do-while**: cuando necesitas ejecutar al menos una vez (validar input, menus)

### Limpieza de la parte 2

```bash
rm -f while_vs_dowhile
```

---

## Parte 3 — break y continue

**Objetivo**: Usar break para salir de un loop al encontrar un valor, continue
para saltar iteraciones, y entender el comportamiento de break en loops anidados.

### Paso 3.1 — break y continue basicos

```bash
cat break_continue.c
```

El programa tiene un array `{4, 7, 2, -3, 8, -1, 6}` y hace dos operaciones:

1. Buscar el primer numero negativo con break
2. Imprimir solo los positivos con continue

Antes de ejecutar, predice:

- Cual es el primer numero negativo y en que indice esta?
- Que numeros imprime la seccion con continue?

### Paso 3.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic break_continue.c -o break_continue
./break_continue
```

Salida esperada:

```
Array: 4 7 2 -3 8 -1 6

--- break: find first negative ---
First negative: -3 at index 3

--- continue: print only positives ---
4 7 2 8 6
```

- **break** detiene el loop completamente al encontrar -3 en el indice 3.
  No sigue buscando -1
- **continue** salta el `printf` para los negativos (-3 y -1) pero no detiene
  el loop. Continua con el siguiente elemento

### Paso 3.3 — break en loops anidados

```bash
cat nested_break.c
```

Este programa busca un valor en una matriz 3x4. Muestra dos intentos:

1. Solo break en el loop interno (el externo sigue ejecutandose)
2. Flag `found` para salir de ambos loops

Antes de ejecutar, predice:

- En el intento 1, cuantas veces se imprime "Outer loop: i=... continues"?
- En el intento 2, se imprime "Outer loop" alguna vez?

### Paso 3.4 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic nested_break.c -o nested_break
./nested_break
```

Salida esperada:

```
--- Attempt 1: break in inner loop only ---
Outer loop: i=0 continues
Found -1 at [1][2], breaking inner loop
Outer loop: i=1 continues
Outer loop: i=2 continues

--- Attempt 2: flag to break both loops ---
Found -1 at [1][2]
```

Analisis:

- **Intento 1**: break solo sale del for interno. El for externo sigue
  ejecutandose para i=1 y i=2. "Outer loop" se imprime 3 veces
- **Intento 2**: la flag `found` se verifica en la condicion de ambos loops
  (`i < rows && !found` y `j < cols && !found`). Al poner `found = 1`, ambos
  loops terminan

### Paso 3.5 — continue y while: un error clasico

Un error comun es poner el incremento despues de continue en un while. Observa
este patron (no lo ejecutes, es un loop infinito):

```c
// PELIGRO: loop infinito
int j = 0;
while (j < 10) {
    if (j == 5) {
        continue;    // salta j++ => j queda en 5 para siempre
    }
    j++;
}
```

En un for, el incremento `i++` se ejecuta siempre despues de cada iteracion
(incluyendo cuando se usa continue). En un while, si el incremento esta
despues del continue, nunca se ejecuta.

Solucion: poner el incremento antes del continue, o usar for.

### Limpieza de la parte 3

```bash
rm -f break_continue nested_break
```

---

## Parte 4 — Patrones comunes

**Objetivo**: Aplicar loops a tres patrones frecuentes en C: recorrer strings,
busqueda con sentinel value, y validacion de input.

### Paso 4.1 — Recorrer strings

```bash
cat string_traverse.c
```

El programa recorre el string `"Hello, World! 123"` de tres formas:

1. Con indice: `msg[i] != '\0'`
2. Con puntero: `*p != '\0'`
3. Contando tipos de caracteres (letras, digitos, espacios, otros)

Antes de ejecutar, predice:

- Cuantas letras tiene "Hello, World! 123"?
- Cuantos digitos?
- Que caracteres cuentan como "other"?

### Paso 4.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic string_traverse.c -o string_traverse
./string_traverse
```

Salida esperada:

```
--- Index traversal ---
String: "Hello, World! 123"
Characters: 'H' 'e' 'l' 'l' 'o' ',' ' ' 'W' 'o' 'r' 'l' 'd' '!' ' ' '1' '2' '3'

--- Pointer traversal ---
Characters: 'H' 'e' 'l' 'l' 'o' ',' ' ' 'W' 'o' 'r' 'l' 'd' '!' ' ' '1' '2' '3'

--- Count character types ---
Letters: 10, Digits: 3, Spaces: 2, Other: 2
```

- 10 letras: H, e, l, l, o, W, o, r, l, d
- 3 digitos: 1, 2, 3
- 2 espacios
- 2 otros: la coma y el signo de exclamacion

Ambos metodos de recorrido (indice y puntero) producen el mismo resultado.
El estilo con puntero es mas idiomatico en C, pero el de indice es mas legible
para principiantes.

### Paso 4.3 — Busqueda con sentinel value

```bash
cat sentinel_search.c
```

La funcion `find()` busca un valor en un array y retorna su indice. Si no lo
encuentra, retorna -1 (el sentinel value que indica "no encontrado").

Antes de ejecutar, predice el resultado para cada busqueda:

- `find(42)` — esta en el array?
- `find(99)` — esta en el array?
- `find(10)` — en que indice?
- `find(55)` — en que indice?

### Paso 4.4 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic sentinel_search.c -o sentinel_search
./sentinel_search
```

Salida esperada:

```
Array: 10 25 3 42 7 18 55

find(42) = 3 (found at index 3)
find(99) = -1 (not found)
find(10) = 0 (found at index 0)
find(55) = 6 (found at index 6)
```

El patron:

- El loop recorre el array con for (numero conocido de iteraciones)
- Si encuentra el target, retorna inmediatamente con `return i`
- Si el loop termina sin encontrarlo, retorna -1
- El valor -1 es un sentinel: no es un indice valido, asi que el llamador sabe
  distinguir "encontrado" de "no encontrado"

### Paso 4.5 — Validacion de input con do-while

```bash
cat input_validation.c
```

Este programa usa do-while para pedir un numero entre 1 y 10. Repite hasta
que el usuario ingrese un valor valido. Tambien maneja input no numerico
(letras, simbolos).

### Paso 4.6 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic input_validation.c -o input_validation
./input_validation
```

Prueba varias entradas:

1. Escribe `0` (fuera de rango)
2. Escribe `15` (fuera de rango)
3. Escribe `abc` (input no numerico)
4. Escribe `5` (valido)

Salida esperada (ejemplo con las entradas anteriores):

```
Enter a number between 1 and 10: 0
0 is out of range. Try again.
Enter a number between 1 and 10: 15
15 is out of range. Try again.
Enter a number between 1 and 10: abc
Invalid input. Try again.
Enter a number between 1 and 10: 5

You entered: 5 (after 4 attempts)
```

Este es el caso de uso clasico de do-while: necesitas pedir el input al menos
una vez antes de poder verificar si es valido. Un while necesitaria duplicar
el prompt o usar una variable de control artificial.

### Limpieza de la parte 4

```bash
rm -f string_traverse sentinel_search input_validation
```

---

## Limpieza final

```bash
rm -f for_variations for_optional_parts while_vs_dowhile
rm -f break_continue nested_break
rm -f string_traverse sentinel_search input_validation
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md            for_optional_parts.c  nested_break.c      string_traverse.c
break_continue.c     for_variations.c      sentinel_search.c   while_vs_dowhile.c
input_validation.c
```

---

## Conceptos reforzados

1. Las tres secciones del **for** (inicializacion, condicion, actualizacion)
   son todas opcionales. Omitir la condicion produce un loop infinito
   equivalente a `while (1)`.

2. **while** evalua la condicion antes de la primera iteracion y puede ejecutar
   0 veces. **do-while** ejecuta el cuerpo al menos 1 vez y evalua la condicion
   despues.

3. **break** sale del loop mas interno solamente. Para salir de loops anidados
   se necesita un mecanismo adicional como una flag o goto.

4. **continue** salta al siguiente ciclo del loop. En un for, la actualizacion
   (`i++`) se ejecuta siempre. En un while, si el incremento esta despues del
   continue, se salta y puede causar un loop infinito.

5. Recorrer un string en C se basa en la condicion de terminacion `!= '\0'`.
   Se puede hacer con indice (`str[i]`) o con puntero (`*p`), ambos son
   equivalentes.

6. El patron de busqueda con **sentinel value** (-1) permite que una funcion
   comunique "no encontrado" sin usar parametros de salida ni variables globales.

7. **do-while** es la estructura natural para validacion de input: el programa
   necesita pedir el dato al menos una vez antes de poder verificarlo.
