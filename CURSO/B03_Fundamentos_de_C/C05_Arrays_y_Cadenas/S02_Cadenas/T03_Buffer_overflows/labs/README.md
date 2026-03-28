# Lab -- Buffer overflows

## Objetivo

Provocar buffer overflows controlados, observar como corrompen la memoria, y
aprender a detectarlos y prevenirlos con las herramientas del compilador. Al
finalizar, sabras usar ASan, stack canaries y FORTIFY_SOURCE, y escribir codigo
que valida tamanos antes de copiar.

NOTA: Este lab usa AddressSanitizer para detectar overflows de forma segura. No
se trata de explotar vulnerabilidades, sino de detectarlas y prevenirlas.

## Prerequisitos

- GCC instalado
- libasan instalado (Fedora: `sudo dnf install libasan`)
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `stack_overflow.c` | Programa con overflow intencional usando strcpy |
| `dangerous_funcs.c` | Comparacion de funciones peligrosas vs seguras |
| `fortify_demo.c` | Programa para demostrar FORTIFY_SOURCE |
| `safe_copy.c` | Patrones de codigo seguro con validacion de tamanos |

---

## Parte 1 -- Stack buffer overflow

**Objetivo**: Provocar un overflow con strcpy y observar como corrompe variables
adyacentes en el stack.

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat stack_overflow.c
```

Observa la estructura:

- `buf` tiene 8 bytes
- `canary` y `guard` son variables adyacentes con valores conocidos
- `strcpy` copia 16 bytes (mas el terminador) en un buffer de 8

Antes de compilar y ejecutar, predice:

- Al escribir 17 bytes en un buffer de 8, que pasara con `canary` y `guard`?
- El programa terminara normalmente o con un crash?
- El compilador dara alguna advertencia?

### Paso 1.2 -- Compilar sin protecciones

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -fno-stack-protector stack_overflow.c -o stack_overflow
```

Observa: el compilador emite una advertencia `-Wstringop-overflow=` porque
detecta en compilacion que strcpy desborda el buffer. Esta es una primera linea
de defensa -- el compilador te avisa antes de ejecutar.

### Paso 1.3 -- Ejecutar el programa

```bash
./stack_overflow
```

Salida esperada (los valores exactos dependen de la disposicion del stack):

```
Before strcpy:
  buf address:    0x...
  canary value:   0x41414141
  guard value:    0xDEADBEEF

After strcpy:
  buf content:    AAAAAAAABBBBBBBB
  canary value:   0x42424242
  guard value:    0xDEADBEEF

  canary was corrupted!
```

El valor `0x42424242` corresponde a "BBBB" en ASCII (0x42 = 'B'). Los 8 bytes
"BBBBBBBB" de la cadena sobrescribieron `canary` porque esta justo despues de
`buf` en el stack.

NOTA: El comportamiento exacto depende del compilador y la arquitectura. En
algunos casos `guard` tambien se corrompe, en otros el programa genera un
segfault. Esto es comportamiento indefinido -- el punto es que la corrupcion
ocurre de forma silenciosa.

### Paso 1.4 -- Verificar con nm

```bash
nm stack_overflow | grep main
```

Confirma que el programa se enlazo correctamente. Los overflows no impiden la
compilacion ni el enlace -- solo causan problemas en runtime.

### Limpieza de la parte 1

```bash
rm -f stack_overflow
```

---

## Parte 2 -- Funciones peligrosas vs seguras

**Objetivo**: Comparar funciones de string que no verifican tamano (strcpy,
sprintf) con sus versiones seguras (strncpy, snprintf).

### Paso 2.1 -- Examinar el codigo fuente

```bash
cat dangerous_funcs.c
```

Observa las diferencias entre cada par:

- `dangerous_strcpy` usa `strcpy` sin verificar longitud
- `safe_strncpy` usa `strncpy` con limite explicito y agrega terminador
- `dangerous_sprintf` usa `sprintf` sin limite
- `safe_snprintf` usa `snprintf` con limite y detecta truncacion

Antes de compilar, predice:

- Si `long_input` tiene 45 caracteres y `dest` tiene 16 bytes, que hara
  `strncpy` con el exceso?
- `snprintf` retorna un valor -- que representa?

### Paso 2.2 -- Compilar con advertencias

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic dangerous_funcs.c -o dangerous_funcs
```

Observa las advertencias del compilador:

- `-Wformat-overflow=` para `sprintf`: el compilador calculo que la salida
  formateada (36+ bytes) no cabe en 32 bytes
- `-Wformat-truncation=` para `snprintf`: detecta que la salida sera truncada

El compilador distingue entre overflow (sprintf, peligroso) y truncacion
(snprintf, controlada).

### Paso 2.3 -- Ejecutar solo las funciones seguras

```bash
./dangerous_funcs
```

Salida esperada:

```
=== Short input (safe for both) ===
strcpy:
  strcpy result: Alice
strncpy:
  strncpy result: Alice

=== Long input (dangerous for strcpy) ===
strncpy (safe):
  strncpy result: A very long use

=== sprintf vs snprintf ===
snprintf (safe):
  snprintf result: Name: Bartholomew Lon
  WARNING: output was truncated (48 chars needed, 32 available)
```

Observa:

- `strncpy` corto la cadena a 15 caracteres (16 - 1 para el terminador)
- `snprintf` trunco la salida pero informo cuantos bytes necesitaba (48)
- El programa no solo evita el overflow -- tambien te dice que hubo truncacion

NOTA: El programa NO llama a `dangerous_strcpy` con la cadena larga porque eso
causaria un overflow real. Solo ejecuta las versiones seguras con inputs largos.

### Paso 2.4 -- Verificar que snprintf retorna el tamano necesario

La clave de `snprintf` es que retorna cuantos caracteres **habria escrito** si
hubiera espacio suficiente. Esto permite detectar truncacion:

```c
int written = snprintf(buf, sizeof(buf), "...", args);
if ((size_t)written >= sizeof(buf)) {
    /* truncation occurred */
}
```

Este patron aparece en todo codigo C profesional.

### Limpieza de la parte 2

```bash
rm -f dangerous_funcs
```

---

## Parte 3 -- Defensas del compilador

**Objetivo**: Usar tres mecanismos de deteccion de overflows: stack canaries,
FORTIFY_SOURCE, y AddressSanitizer.

### Paso 3.1 -- Stack canary (-fstack-protector)

Compila `stack_overflow.c` con el stack protector habilitado:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -fstack-protector-strong stack_overflow.c -o stack_overflow_sp
```

Antes de ejecutar, predice:

- El stack protector inserta un valor "canario" entre el buffer y la direccion
  de retorno. Al retornar de la funcion, verifica que el canario no cambio.
  Que pasara al ejecutar este programa que desborda `buf`?

### Paso 3.2 -- Ejecutar con stack protector

```bash
./stack_overflow_sp
```

Salida esperada:

```
Before strcpy:
  buf address:    0x...
  canary value:   0x41414141
  guard value:    0xDEADBEEF

After strcpy:
  buf content:    AAAAAAAABBBBBBBB
  canary value:   0x42424242
  guard value:    0xDEADBEEF

  canary was corrupted!
*** stack smashing detected ***: terminated
Aborted
```

El programa imprime los valores (el overflow ocurre) pero al intentar retornar
de `main`, el stack protector detecta que su canario fue corrompido y aborta.
El mensaje "stack smashing detected" viene del runtime de GCC, no de nuestro
codigo.

### Paso 3.3 -- FORTIFY_SOURCE

Ahora veamos otra defensa. Examina el codigo:

```bash
cat fortify_demo.c
```

Compila con FORTIFY_SOURCE:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -D_FORTIFY_SOURCE=2 fortify_demo.c -o fortify_demo
```

NOTA: `-D_FORTIFY_SOURCE=2` requiere `-O1` o superior porque necesita inlining
para instrumentar las funciones.

Antes de ejecutar, predice:

- FORTIFY_SOURCE reemplaza `strcpy` por `__strcpy_chk`, que verifica el tamano
  del destino en runtime. Que pasara?

### Paso 3.4 -- Ejecutar con FORTIFY

```bash
./fortify_demo
```

Salida esperada:

```
Buffer size: 8 bytes
Attempting strcpy of a long string into buf...
*** buffer overflow detected ***: terminated
Aborted
```

FORTIFY detecto el overflow **antes** de que corrompiera el stack. A diferencia
del stack canary (que detecta despues del hecho), FORTIFY aborta **durante** la
copia, antes de escribir mas alla del buffer.

### Paso 3.5 -- AddressSanitizer (ASan)

ASan es la herramienta mas poderosa para detectar overflows. Compila
`stack_overflow.c` con ASan:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -fsanitize=address -g stack_overflow.c -o stack_overflow_asan
```

NOTA: Si la compilacion falla con "cannot find libasan", instala la biblioteca:

```bash
# Fedora/RHEL:
sudo dnf install libasan

# Debian/Ubuntu:
sudo apt install libasan8    # el numero de version puede variar
```

### Paso 3.6 -- Ejecutar con ASan

```bash
./stack_overflow_asan
```

Salida esperada (resumida):

```
=================================================================
==<PID>==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x...
WRITE of size 17 at 0x...
    #0 0x... in main stack_overflow.c:15
...
Address 0x... is located in stack of thread T0 at offset ... in frame
    #0 0x... in main stack_overflow.c:4
...
[32, 40) 'buf' <== Memory access at offset 40 overflows this variable
=================================================================
```

Observa la informacion que da ASan:

- **Tipo de error**: stack-buffer-overflow
- **Operacion**: WRITE of size 17 (los 16 caracteres mas el terminador)
- **Ubicacion exacta**: `stack_overflow.c:15` (la linea del strcpy)
- **Variable afectada**: 'buf' de tamano 8, acceso en offset 40 (fuera del rango [32, 40))
- **Stack trace completo**: muestra la cadena de llamadas

Esta informacion es mucho mas util que un simple "Aborted". ASan te dice
exactamente donde ocurrio el overflow, cuantos bytes se escribieron, y que
variable fue afectada.

### Paso 3.7 -- Comparar las tres defensas

Resumen de lo observado:

| Defensa | Cuando detecta | Que reporta | Flag |
|---------|---------------|-------------|------|
| Stack canary | Al retornar de la funcion | "stack smashing detected" | `-fstack-protector-strong` |
| FORTIFY_SOURCE | Durante la copia (runtime) | "buffer overflow detected" | `-D_FORTIFY_SOURCE=2 -O2` |
| ASan | Durante la copia (runtime) | Reporte detallado con linea, variable, tamano | `-fsanitize=address -g` |

Para desarrollo, ASan es la mejor opcion. Para produccion, FORTIFY_SOURCE y
stack canaries no tienen costo significativo de rendimiento.

### Limpieza de la parte 3

```bash
rm -f stack_overflow_sp fortify_demo stack_overflow_asan
```

---

## Parte 4 -- Escribir codigo seguro

**Objetivo**: Aprender los patrones correctos para copiar strings sin riesgo de
overflow.

### Paso 4.1 -- Examinar los patrones

```bash
cat safe_copy.c
```

El archivo demuestra 4 patrones:

1. **Validar longitud antes de copiar** -- strlen + comparacion + strcpy
2. **Usar snprintf para todo** -- formateo seguro con deteccion de truncacion
3. **Concatenacion segura** -- tracking del espacio restante con snprintf
4. **fgets para input del usuario** -- limite explicito al leer de stdin

Antes de compilar, predice:

- En el patron 1, si la fuente tiene 31 bytes y el destino tiene 10, que
  retornara `copy_with_check`?
- En el patron 2, si el buffer tiene 15 bytes y la cadena formateada necesita
  25, que contendra el buffer?

### Paso 4.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic safe_copy.c -o safe_copy
```

Observa: **cero advertencias**. Este codigo compila limpio porque no hay
operaciones que el compilador detecte como peligrosas.

```bash
echo "Hello World test" | ./safe_copy
```

Salida esperada (stderr y stdout se mezclan en la terminal):

```
=== Pattern 1: Validate before copy ===
Short string:
  Copied: "Hi"
Long string:
  ERROR: source (31 bytes) too long for dest (10 bytes)

=== Pattern 2: snprintf ===
  Result: "Hello Alice, age 30"
  WARNING: truncated (25 needed, 15 available)
  Result: "Hello Bartholo"

=== Pattern 3: Safe concatenation ===
  Concatenated: "Hello World!" (used 12 of 32 bytes)

=== Pattern 4: fgets for input ===
  Enter text (max 15 chars):   Read 15 chars: "Hello World tes"
```

Observa:

- Patron 1: la funcion rechazo la copia y retorno -1, sin corromper memoria
- Patron 2: `snprintf` trunco a 14 caracteres y reporto que necesitaba 25
- Patron 3: la concatenacion se hizo con tracking del espacio disponible
- Patron 4: `fgets` leyo exactamente 15 caracteres (sizeof - 1) y paro

### Paso 4.3 -- El patron mas importante: snprintf

De todos los patrones, `snprintf` es el mas versatil:

```c
// Formatear:
snprintf(buf, sizeof(buf), "User: %s", name);

// Concatenar (con offset):
snprintf(buf + offset, remaining, "%s", data);

// Detectar truncacion:
int n = snprintf(buf, sizeof(buf), "%s", data);
if ((size_t)n >= sizeof(buf)) { /* truncated */ }
```

Siempre:

- Termina con `'\0'` (a diferencia de `strncpy`)
- Nunca escribe mas de `size` bytes
- Retorna cuantos caracteres habria escrito (permite detectar truncacion)

### Paso 4.4 -- Compilar con ASan para verificar

Si tienes ASan instalado, compila con sanitizer para confirmar que el codigo
seguro no genera errores:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -fsanitize=address -g safe_copy.c -o safe_copy_asan
echo "test" | ./safe_copy_asan
```

ASan no debe reportar ningun error. Compara con el reporte que dio para
`stack_overflow.c` -- la diferencia es el codigo seguro.

### Limpieza de la parte 4

```bash
rm -f safe_copy safe_copy_asan
```

---

## Limpieza final

```bash
rm -f stack_overflow stack_overflow_sp stack_overflow_asan
rm -f dangerous_funcs
rm -f fortify_demo
rm -f safe_copy safe_copy_asan
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  dangerous_funcs.c  fortify_demo.c  safe_copy.c  stack_overflow.c
```

---

## Conceptos reforzados

1. Un buffer overflow ocurre cuando se escriben mas datos de los que caben en un
   buffer. La corrupcion de memoria puede ser silenciosa -- el programa puede
   seguir ejecutandose con datos corrompidos.

2. `strcpy`, `sprintf` y `gets` no verifican el tamano del destino. El
   compilador advierte sobre overflows detectables en compilacion
   (`-Wstringop-overflow`, `-Wformat-overflow`), pero no puede detectar todos.

3. `strncpy` limita los bytes copiados pero **no garantiza el terminador nulo**.
   Siempre hay que agregar `dest[size - 1] = '\0'` manualmente.

4. `snprintf` es la funcion mas segura para strings: limita la escritura, siempre
   agrega terminador, y retorna cuantos caracteres habria escrito, permitiendo
   detectar truncacion.

5. El stack protector (`-fstack-protector-strong`) detecta la corrupcion al
   retornar de la funcion, despues de que el overflow ocurrio. Es barato en
   rendimiento y deberia estar siempre activo.

6. FORTIFY_SOURCE (`-D_FORTIFY_SOURCE=2`) reemplaza funciones como `strcpy` por
   versiones que verifican el tamano en runtime. Detecta el overflow **durante**
   la copia, antes de corromper datos criticos.

7. AddressSanitizer (`-fsanitize=address`) es la herramienta mas detallada:
   reporta la linea exacta, el tamano de la escritura, la variable afectada, y
   el stack trace completo. Usar siempre durante desarrollo.

8. El patron seguro para copiar strings es: verificar la longitud antes de
   copiar, o usar `snprintf` que hace la verificacion internamente.
