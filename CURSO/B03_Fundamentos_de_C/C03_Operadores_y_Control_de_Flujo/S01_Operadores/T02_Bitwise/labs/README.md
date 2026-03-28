# Lab — Operadores bitwise

## Objetivo

Practicar los seis operadores bitwise de C (&, |, ^, ~, <<, >>), visualizar
como cada uno transforma los bits de un valor, y aplicar los patrones clasicos
de manipulacion de bits sobre flags y permisos. Al finalizar, sabras interpretar
operaciones bitwise, crear mascaras, y usar flags para representar estados
compactos.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `print_bits.c` | Operadores AND, OR, XOR, NOT con funcion de visualizacion binaria |
| `shifts.c` | Shift left/right, mascaras de un solo bit, extraccion de nibbles |
| `masks_flags.c` | Patrones set, clear, toggle, check con flags definidos como macros |
| `unix_perms.c` | Caso practico: permisos Unix (rwxrwxrwx) con operaciones bitwise |

---

## Parte 1 — AND, OR, XOR, NOT

**Objetivo**: Visualizar la representacion binaria de valores y verificar como
cada operador bitwise transforma los bits.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat print_bits.c
```

Observa:

- La funcion `print_bits_8()` recorre los 8 bits de un `uint8_t` de izquierda
  a derecha, usando shift y AND para extraer cada bit individual
- Se aplican los cuatro operadores (&, |, ^, ~) sobre pares de valores
- Al final se demuestran dos propiedades de XOR

### Paso 1.2 — Predecir el resultado de AND

Antes de compilar, calcula mentalmente:

```
  a = 1010 1100
  b = 1111 0000
```

- Que bits quedaran en `a & b`?
- Que nibble (grupo de 4 bits) sobrevive intacto y cual se borra?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.3 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic print_bits.c -o print_bits
./print_bits
```

Salida esperada:

```
=== AND (&) ===
  a       = 0xAC = 1010 1100
  b       = 0xF0 = 1111 0000
  a & b   = 0xA0 = 1010 0000

=== OR (|) ===
  c       = 0xA0 = 1010 0000
  d       = 0x0C = 0000 1100
  c | d   = 0xAC = 1010 1100

=== XOR (^) ===
  a       = 0xAC = 1010 1100
  b       = 0xF0 = 1111 0000
  a ^ b   = 0x5C = 0101 1100

=== NOT (~) ===
  a       = 0xAC = 1010 1100
  ~a      = 0x53 = 0101 0011

=== XOR properties ===
  a ^ a   = 0x00 = 0000 0000
  a ^ 0   = 0xAC = 1010 1100
```

### Paso 1.4 — Analizar cada operador

Verifica tus predicciones contra la salida:

- **AND**: `b = 0xF0` actua como mascara. El nibble alto de `a` sobrevive
  porque la mascara tiene 1s ahi. El nibble bajo se borra porque la mascara
  tiene 0s. AND **extrae** bits.

- **OR**: combina los bits de ambos operandos. Donde cualquiera tiene un 1,
  el resultado tiene un 1. OR **enciende** bits.

- **XOR**: produce 1 donde los bits difieren. En los 4 bits superiores,
  `1010 ^ 1111 = 0101` (los bits se invirtieron). En los 4 inferiores,
  `1100 ^ 0000 = 1100` (quedaron igual). XOR **invierte** bits selectivamente.

- **NOT**: invierte todos los bits sin excepcion. `1010 1100` se convierte en
  `0101 0011`.

- **Propiedades de XOR**: `a ^ a` siempre es 0 (cada bit difiere consigo
  mismo cero veces). `a ^ 0` siempre es `a` (XOR con 0 no cambia nada).

### Limpieza de la parte 1

```bash
rm -f print_bits
```

---

## Parte 2 — Shifts (<< y >>)

**Objetivo**: Entender los desplazamientos de bits como multiplicacion y
division por potencias de 2, y ver como se crean mascaras de un solo bit.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat shifts.c
```

Observa:

- Se aplica shift left con 1, 2 y 3 posiciones sobre el valor 10
- Se aplica shift right sobre 80
- Se genera la tabla completa de mascaras `1 << N` para N de 0 a 7
- Se extraen los nibbles de un byte usando shift y AND

### Paso 2.2 — Predecir los shifts

Antes de compilar:

- `10 << 1` es multiplicar 10 por 2. Resultado?
- `10 << 3` es multiplicar 10 por 8. Resultado?
- `80 >> 2` es dividir 80 por 4. Resultado?
- `1 << 5` deberia tener un solo bit encendido. Cual posicion?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.3 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic shifts.c -o shifts
./shifts
```

Salida esperada:

```
=== Shift left (<<) — multiply by powers of 2 ===
  x        = 0x0A = 0000 1010 (10)
  x << 1   = 0x14 = 0001 0100 (20)
  x << 2   = 0x28 = 0010 1000 (40)
  x << 3   = 0x50 = 0101 0000 (80)

=== Shift right (>>) — divide by powers of 2 ===
  y        = 0x50 = 0101 0000 (80)
  y >> 1   = 0x28 = 0010 1000 (40)
  y >> 2   = 0x14 = 0001 0100 (20)
  y >> 3   = 0x0A = 0000 1010 (10)

=== Creating single-bit masks with 1 << N ===
  1 << 0   = 0x01 = 0000 0001 (1)
  1 << 1   = 0x02 = 0000 0010 (2)
  1 << 2   = 0x04 = 0000 0100 (4)
  1 << 3   = 0x08 = 0000 1000 (8)
  1 << 4   = 0x10 = 0001 0000 (16)
  1 << 5   = 0x20 = 0010 0000 (32)
  1 << 6   = 0x40 = 0100 0000 (64)
  1 << 7   = 0x80 = 1000 0000 (128)

=== Extracting nibbles ===
  byte          = 0xD6 = 1101 0110 (214)
  low  nibble   = 0x06 = 0000 0110 (6)
  high nibble   = 0x0D = 0000 1101 (13)
```

### Paso 2.4 — Analizar los resultados

- **Shift left**: cada posicion desplazada multiplica por 2. Los bits se mueven
  hacia la izquierda y se rellenan ceros por la derecha. Si un bit "sale" por
  la izquierda, se pierde.

- **Shift right**: cada posicion desplazada divide por 2 (division entera, sin
  resto). Los bits se mueven hacia la derecha y se rellenan ceros por la
  izquierda (para unsigned).

- **Mascaras `1 << N`**: cada una tiene exactamente un bit encendido en la
  posicion N. Este patron es la base de los flags y las mascaras. En la
  seccion de flags de la teoria, `#define FLAG_READ (1U << 0)` usa exactamente
  este patron.

- **Extraccion de nibbles**: para obtener el nibble bajo se aplica
  `byte & 0x0F` (mascara que conserva solo los 4 bits inferiores). Para el
  nibble alto se hace `(byte >> 4) & 0x0F` (primero se desplaza y luego se
  aplica la mascara). Este es el mismo patron que la teoria muestra para
  extraer campos de valores empaquetados (como RGB565).

### Limpieza de la parte 2

```bash
rm -f shifts
```

---

## Parte 3 — Mascaras y flags

**Objetivo**: Aplicar los cuatro patrones clasicos de manipulacion de bits
(set, clear, toggle, check) sobre flags definidos como macros.

### Paso 3.1 — Examinar el codigo fuente

```bash
cat masks_flags.c
```

Observa:

- Los flags se definen como `(1U << N)`, cada uno en una posicion diferente
- La funcion `print_flags()` muestra el valor en hex, binario y como letras
  `[HXWR]`
- Se demuestran las cuatro operaciones en orden: set, check, clear, toggle
- Al final se verifica la presencia de multiples flags simultaneamente

### Paso 3.2 — Predecir las operaciones

Antes de compilar, sigue mentalmente la secuencia:

- `flags = 0`, luego `flags |= FLAG_READ` (bit 0). Resultado en binario?
- Despues `flags |= FLAG_WRITE` (bit 1). Resultado?
- Despues `flags |= FLAG_EXEC` (bit 2). Resultado?
- Despues `flags &= ~FLAG_WRITE`. Que bit se apaga? Resultado?
- Despues `flags ^= FLAG_HIDDEN` (bit 3, estaba en 0). Resultado?
- Despues `flags ^= FLAG_HIDDEN` otra vez (ahora estaba en 1). Resultado?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.3 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic masks_flags.c -o masks_flags
./masks_flags
```

Salida esperada:

```
=== SET bits (|=) ===
Initial:
  flags = 0x00 = 0000 0000  [----]

Set READ:
  flags = 0x01 = 0000 0001  [---R]

Set WRITE:
  flags = 0x03 = 0000 0011  [--WR]

Set EXEC:
  flags = 0x07 = 0000 0111  [-XWR]

=== CHECK bits (&) ===
Has READ?   yes
Has WRITE?  yes
Has HIDDEN? no

=== CLEAR bits (&= ~) ===
Clear WRITE:
  flags = 0x05 = 0000 0101  [-X-R]
Has WRITE?  no

=== TOGGLE bits (^=) ===
Toggle HIDDEN (was off -> on):
  flags = 0x0D = 0000 1101  [HX-R]

Toggle HIDDEN (was on -> off):
  flags = 0x05 = 0000 0101  [-X-R]

=== Combining multiple flags ===
Set READ | WRITE | EXEC:
  flags = 0x07 = 0000 0111  [-XWR]

Check if has BOTH read AND write:
  -> yes, has both READ and WRITE

Check if has AT LEAST ONE of read or hidden:
  -> yes, has at least one
```

### Paso 3.4 — Analizar los patrones

Verifica tus predicciones y relaciona cada operacion con su patron:

| Operacion | Patron | Ejemplo |
|-----------|--------|---------|
| Set (encender) | `flags \|= BIT` | OR con la mascara enciende ese bit sin tocar los demas |
| Clear (apagar) | `flags &= ~BIT` | NOT invierte la mascara, AND apaga solo ese bit |
| Toggle (invertir) | `flags ^= BIT` | XOR invierte el bit: si era 0 pasa a 1, si era 1 pasa a 0 |
| Check (verificar) | `flags & BIT` | AND con la mascara da distinto de cero si el bit esta encendido |

Para verificar multiples flags:

- **Todos presentes**: `(flags & (A | B)) == (A | B)` — se compara que ambos
  bits esten encendidos.
- **Al menos uno**: `flags & (A | B)` — basta con que cualquiera este
  encendido para que el resultado sea distinto de cero.

### Limpieza de la parte 3

```bash
rm -f masks_flags
```

---

## Parte 4 — Caso practico: permisos Unix

**Objetivo**: Aplicar operaciones bitwise a un caso real: construir, verificar
y modificar permisos de archivo estilo Unix (rwxrwxrwx), el mismo sistema que
usa `chmod`.

### Paso 4.1 — Examinar el codigo fuente

```bash
cat unix_perms.c
```

Observa:

- Los 9 bits de permisos se definen como `(1U << N)` donde las posiciones van
  de 8 (owner read) a 0 (other execute), exactamente como en el estandar POSIX
- La funcion `print_perm_bits()` muestra el valor en octal, binario agrupado
  de a 3, y la cadena `rwxrwxrwx`
- Se construyen los permisos 0755 bit a bit
- Se verifican permisos individuales y se modifican con clear/set

### Paso 4.2 — Predecir la construccion

Antes de compilar, piensa en la construccion de 0755:

- Owner rwx = bits 8,7,6 encendidos = 111 000 000 en binario
- Group r-x = bits 5,3 encendidos = 000 101 000
- Other r-x = bits 2,0 encendidos = 000 000 101
- OR de los tres: 111 101 101 = 0755 en octal

Que pasa al hacer `perms &= ~PERM_W_USR`? Que bit se apaga?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.3 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic unix_perms.c -o unix_perms
./unix_perms
```

Salida esperada:

```
=== Building permissions 0755 (rwxr-xr-x) ===

Start empty:
  0000 = 000 000 000  ---------

Set owner: rwx
  0700 = 111 000 000  rwx------

Set group: r-x
  0750 = 111 101 000  rwxr-x---

Set other: r-x
  0755 = 111 101 101  rwxr-xr-x

=== Testing individual permissions ===
Owner can read?    yes
Owner can write?   yes
Group can write?   no
Other can execute? yes

=== Removing owner write (0755 -> 0555) ===
  0555 = 101 101 101  r-xr-xr-x

=== Adding group write (0555 -> 0575) ===
  0575 = 101 111 101  r-xrwxr-x

=== Common permission patterns ===
0644:   0644 = 110 100 100  rw-r--r--
0755:   0755 = 111 101 101  rwxr-xr-x
0700:   0700 = 111 000 000  rwx------
```

### Paso 4.4 — Verificar contra el sistema real

Compara la salida del programa con los permisos reales de archivos:

```bash
ls -l unix_perms.c
```

Salida esperada:

```
-rw-r--r-- 1 ... unix_perms.c
```

Esos permisos `rw-r--r--` corresponden a 0644 — exactamente lo que el programa
muestra como `110 100 100`. El sistema operativo usa este mismo mecanismo
de bits para almacenar permisos en el inodo del archivo.

```bash
ls -l unix_perms
```

Salida esperada:

```
-rwxr-xr-x 1 ... unix_perms
```

El ejecutable compilado tiene permisos 0755 = `rwxr-xr-x` = `111 101 101`.

### Paso 4.5 — Conexion con chmod

El comando `chmod 644 archivo` literalmente escribe esos 9 bits en el inodo.
Cuando escribes `chmod 755`, estas diciendo: owner = rwx (7 = 111), group =
r-x (5 = 101), other = r-x (5 = 101). Cada digito octal codifica exactamente
3 bits.

### Limpieza de la parte 4

```bash
rm -f unix_perms
```

---

## Limpieza final

```bash
rm -f print_bits shifts masks_flags unix_perms
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  masks_flags.c  print_bits.c  shifts.c  unix_perms.c
```

---

## Conceptos reforzados

1. La funcion `print_bits_8()` usa `(value >> i) & 1` para extraer cada bit
   individual — esto combina shift right y AND con mascara 1, el patron mas
   basico de extraccion de bits.

2. AND (&) **extrae** bits: aplica una mascara donde los 1s conservan y los 0s
   borran. Es la operacion para "leer" bits individuales o grupos de bits.

3. OR (|) **enciende** bits: pone un 1 donde la mascara tiene un 1, sin afectar
   los demas bits. Es la operacion de "set".

4. XOR (^) **invierte** bits selectivamente: cambia 0 a 1 y 1 a 0 solo donde
   la mascara tiene un 1. Es la operacion de "toggle". Ademas, `a ^ a == 0` y
   `a ^ 0 == a`.

5. NOT (~) invierte **todos** los bits. Se usa principalmente combinado con AND
   para crear la operacion "clear": `flags &= ~BIT` apaga un bit especifico.

6. Shift left (`<<`) equivale a multiplicar por 2^N. Shift right (`>>`)
   equivale a dividir por 2^N (para unsigned). `1 << N` crea una mascara con
   un unico bit encendido en la posicion N.

7. Los cuatro patrones fundamentales de manipulacion de bits son: set
   (`flags |= BIT`), clear (`flags &= ~BIT`), toggle (`flags ^= BIT`), y
   check (`flags & BIT`).

8. Los permisos Unix (rwxrwxrwx) son un caso real de flags bitwise: 9 bits
   donde cada uno representa un permiso especifico. El valor octal de `chmod`
   (como 755) codifica estos bits agrupados de a 3.
