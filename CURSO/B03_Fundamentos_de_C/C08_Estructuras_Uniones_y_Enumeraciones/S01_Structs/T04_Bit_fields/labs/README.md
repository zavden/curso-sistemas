# Lab -- Bit fields

## Objetivo

Declarar y manipular bit fields en structs, observar el truncamiento por
overflow, comparar bit fields con operaciones bitwise, y verificar las
limitaciones que impone el estandar. Al finalizar, sabras cuando conviene usar
bit fields y cuando es mejor usar mascaras manuales.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `declaration.c` | Structs con campos de 1, 5, 4 y 7 bits, sizeof |
| `overflow.c` | Asignacion de valores fuera del rango del campo |
| `bitfield_vs_bitwise.c` | Permisos rwx con bit fields y con mascaras |
| `limitations.c` | Direccion de campo, campo de ancho 0, portabilidad |

---

## Parte 1 -- Declaracion de bit fields

**Objetivo**: Declarar campos de distintos anchos, verificar sizeof, y observar
el ahorro de memoria respecto a un struct con campos int normales.

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat declaration.c
```

Observa tres cosas:

- `struct Permissions` tiene 3 campos de 1 bit cada uno (3 bits totales)
- `struct PackedDate` tiene campos de 5, 4 y 7 bits (16 bits totales)
- Al final se compara sizeof contra un struct con tres `int`

Antes de compilar, predice:

- Cuantos bytes ocupara `struct Permissions`? Recuerda que el tipo base es
  `unsigned int`.
- Cuantos bytes ocupara `struct PackedDate`? Los 16 bits caben en un
  `unsigned int`?
- Cuantos bytes ocupara un struct con tres `int` normales?

### Paso 1.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic declaration.c -o declaration
./declaration
```

Salida esperada:

```
=== Bit field de 1 bit ===
read=1  write=1  exec=0
sizeof(struct Permissions) = 4

=== Bit fields de 5, 4 y 7 bits ===
day=18  month=3  year=126
Fecha: 18/3/2026
sizeof(struct PackedDate) = 4

=== Comparacion de tamano ===
sizeof(struct PackedDate)   = 4 bytes
sizeof(struct RegularDate)  = 12 bytes
```

Ambos structs con bit fields ocupan 4 bytes (un `unsigned int`), sin importar
que solo usen 3 o 16 bits. El compilador asigna una unidad de almacenamiento
completa. El struct regular con tres `int` ocupa 12 bytes -- tres veces mas.

### Paso 1.3 -- Analisis

La clave esta en el tipo base. Los campos se empaquetan dentro de la unidad de
almacenamiento del tipo (`unsigned int` = 4 bytes). Mientras los bits totales
quepan en esa unidad, sizeof es 4.

En arrays grandes, la diferencia importa: 1 millon de `PackedDate` ocupa ~4 MB
en lugar de ~12 MB.

### Limpieza de la parte 1

```bash
rm -f declaration
```

---

## Parte 2 -- Overflow de bit fields

**Objetivo**: Asignar valores que exceden el rango del campo y observar el
truncamiento. Comparar el comportamiento entre campos unsigned y signed.

### Paso 2.1 -- Examinar el codigo fuente

```bash
cat overflow.c
```

Observa:

- `struct SmallField` tiene un campo unsigned de 3 bits (rango 0-7)
- `struct SignedField` tiene un campo signed de 3 bits (rango -4 a 3)
- Se asignan valores dentro y fuera del rango

Antes de compilar, predice:

- Si asignas 8 a un campo de 3 bits unsigned, que valor se leera? Piensa en
  binario: 8 = 1000. Cuantos bits se conservan?
- Si asignas 15 (1111) a 3 bits, que queda?
- Si asignas 5 (101) a un campo signed de 3 bits, como se interpreta 101 en
  complemento a dos?

### Paso 2.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic overflow.c -o overflow
```

Observa los warnings del compilador:

```
overflow.c: warning: unsigned conversion from 'int' to 'unsigned char:3' changes value from '8' to '0' [-Woverflow]
overflow.c: warning: unsigned conversion from 'int' to 'unsigned char:3' changes value from '15' to '7' [-Woverflow]
overflow.c: warning: unsigned conversion from 'int' to 'unsigned char:3' changes value from '255' to '7' [-Woverflow]
overflow.c: warning: overflow in conversion from 'int' to 'signed char:3' changes value from '4' to '-4' [-Woverflow]
overflow.c: warning: overflow in conversion from 'int' to 'signed char:3' changes value from '5' to '-3' [-Woverflow]
overflow.c: warning: overflow in conversion from 'int' to 'signed char:3' changes value from '7' to '-1' [-Woverflow]
```

GCC detecta los overflows en compilacion y avisa exactamente que valor se
almacenara. Esto es gracias a `-Wall` -- sin ese flag no veras estos warnings.

```bash
./overflow
```

Salida esperada:

```
=== Rango de un campo de 3 bits unsigned ===
Rango valido: 0 a 7

Asignando valores dentro del rango:
  asignado=0  leido=0
  asignado=1  leido=1
  asignado=2  leido=2
  asignado=3  leido=3
  asignado=4  leido=4
  asignado=5  leido=5
  asignado=6  leido=6
  asignado=7  leido=7

Asignando valores fuera del rango:
  asignado=8   leido=0
  asignado=15  leido=7
  asignado=255 leido=7

=== Signed bit field de 3 bits ===
  asignado=3   leido=3
  asignado=4   leido=-4
  asignado=5   leido=-3
  asignado=7   leido=-1
```

### Paso 2.3 -- Analisis

Unsigned: el valor se trunca a los N bits inferiores. 8 (1000) en 3 bits =
000 = 0. 15 (1111) en 3 bits = 111 = 7. 255 (11111111) en 3 bits = 111 = 7.

Signed: los mismos 3 bits se interpretan en complemento a dos. El bit mas
significativo es el bit de signo. 5 = 101 en 3 bits signed = -3. 7 = 111 en
3 bits signed = -1. El rango de 3 bits signed es -4 a 3.

Leccion: siempre usar `unsigned int` para bit fields, a menos que necesites
valores negativos intencionalmente.

### Limpieza de la parte 2

```bash
rm -f overflow
```

---

## Parte 3 -- Bit fields vs operaciones bitwise

**Objetivo**: Implementar los mismos permisos (read, write, execute) con dos
enfoques -- bit fields y mascaras bitwise -- y comparar la legibilidad, el
control y el tamano.

### Paso 3.1 -- Examinar el codigo fuente

```bash
cat bitfield_vs_bitwise.c
```

Observa las dos implementaciones lado a lado:

- Bit fields: `bf.read = 1`, `bf.write = 0`, `if (bf.read)`
- Bitwise: `bw |= PERM_READ`, `bw &= ~PERM_WRITE`, `if (bw & PERM_READ)`

Antes de compilar, predice:

- Las dos implementaciones produciran la misma salida?
- El sizeof del struct con bit fields sera igual al sizeof de un
  `unsigned int`?
- Cual de los dos enfoques permite combinar multiples flags en una sola
  operacion (por ejemplo, "read y execute pero no write")?

### Paso 3.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic bitfield_vs_bitwise.c -o bitfield_vs_bitwise
./bitfield_vs_bitwise
```

Salida esperada:

```
=== Bit fields ===
Inicial (rw-):
  rw-
Agregar execute (rwx):
  rwx
Quitar write (r-x):
  r-x
Verificar read: si

=== Bitwise con mascaras ===
Inicial (rw-):
  rw-
Agregar execute (rwx):
  rwx
Quitar write (r-x):
  r-x
Verificar read: si

=== Tamano ===
sizeof(struct BFPerms) = 4
sizeof(unsigned int)   = 4
```

### Paso 3.3 -- Analisis

Ambos enfoques producen el mismo resultado, pero hay diferencias importantes:

| Aspecto | Bit fields | Bitwise |
|---------|-----------|---------|
| Legibilidad | `p.read = 1` | `p |= PERM_READ` |
| Combinar flags | No directo | `PERM_READ | PERM_EXEC` |
| Pasar como argumento | Necesita el struct | Un solo `unsigned int` |
| Portabilidad de layout | No garantizada | Total |
| Control del orden de bits | Ninguno | Total |

Bitwise es preferido cuando se necesita portabilidad, combinar flags, o pasar
permisos como argumento a funciones. Bit fields son preferidos para uso interno
donde la legibilidad importa mas.

### Limpieza de la parte 3

```bash
rm -f bitfield_vs_bitwise
```

---

## Parte 4 -- Limitaciones de bit fields

**Objetivo**: Verificar que no se puede tomar la direccion de un bit field,
observar el efecto del campo de ancho 0 en el sizeof, y entender las
restricciones de portabilidad.

### Paso 4.1 -- Examinar el codigo fuente

```bash
cat limitations.c
```

Observa:

- Las lineas comentadas con `&f.active` -- son codigo invalido
- `struct Aligned` usa un campo de ancho 0 (`:0`)
- `struct NoZero` es identico pero sin el campo de ancho 0

Antes de compilar, predice:

- Por que no se puede tomar la direccion de un bit field? Un puntero apunta a
  un byte, pero un bit field puede empezar en medio de un byte.
- Cual sera sizeof de `struct NoZero` (campos a:3 y b:5, total 8 bits)?
- Cual sera sizeof de `struct Aligned` (con `:0` entre a y b)?

### Paso 4.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic limitations.c -o limitations
./limitations
```

Salida esperada:

```
=== No se puede tomar la direccion ===
Intentar &f.active no compila.
Workaround: usar variable temporal.

f.visible = 1 (via variable temporal)

=== Efecto de campo de ancho 0 ===
sizeof(struct NoZero)  = 4  (a y b en el mismo int)
sizeof(struct Aligned) = 8  (a en un int, b en otro)

=== Portabilidad: el layout NO esta garantizado ===
El orden de bits (MSB/LSB) depende del compilador y la arquitectura.
No usar bit fields para protocolos de red o archivos binarios.
```

### Paso 4.3 -- Verificar el error de compilacion

Descomenta la linea `int *p = &f.active;` en `limitations.c`:

```bash
sed -n '28p' limitations.c
```

Esa linea dice `* int *p = &f.active;`. Elimina el `* ` del inicio para
descomentarla temporalmente:

```bash
cp limitations.c limitations_test.c
sed -i 's|/\* Esto NO compila|// Esto NO compila|; s|int \*p = &f.active;|int *p = \&f.active;|; s|\*/||' limitations_test.c
```

En su lugar, simplemente crea un archivo de prueba rapido:

```bash
cat > address_test.c << 'EOF'
#include <stdio.h>

struct Flags {
    unsigned int active : 1;
};

int main(void) {
    struct Flags f = { .active = 1 };
    int *p = &f.active;
    printf("%d\n", *p);
    return 0;
}
EOF

gcc -std=c11 -Wall -Wextra -Wpedantic address_test.c -o address_test
```

Salida esperada (error):

```
address_test.c: error: cannot take address of bit-field 'active'
```

El compilador rechaza el codigo. Un puntero necesita apuntar a una direccion de
byte, pero un bit field puede ocupar una posicion arbitraria dentro de un byte.

### Paso 4.4 -- Analisis del campo de ancho 0

Sin `:0`, los campos `a` (3 bits) y `b` (5 bits) suman 8 bits y caben en un
solo `unsigned int` -- sizeof es 4.

Con `:0`, el compilador fuerza que `b` empiece en una nueva unidad de
almacenamiento. Cada campo vive en su propio `unsigned int` -- sizeof sube a 8.

Esto es util para controlar la alineacion de campos en registros de hardware
(con la documentacion especifica del compilador).

### Limpieza de la parte 4

```bash
rm -f limitations address_test.c
```

---

## Limpieza final

```bash
rm -f declaration overflow bitfield_vs_bitwise limitations address_test.c
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  bitfield_vs_bitwise.c  declaration.c  limitations.c  overflow.c
```

---

## Conceptos reforzados

1. Un bit field ocupa el numero exacto de bits declarado, pero el struct
   siempre ocupa al menos una unidad de almacenamiento completa del tipo base
   (`unsigned int` = 4 bytes).

2. Asignar un valor fuera del rango de un bit field trunca los bits superiores.
   En un campo unsigned de 3 bits, 8 (1000) se convierte en 0 (000). GCC
   detecta esto en compilacion con `-Wall`.

3. Los bit fields signed interpretan el bit mas significativo como signo
   (complemento a dos). Un campo signed de 3 bits tiene rango -4 a 3, no 0 a 7.
   Usar siempre `unsigned int` salvo que se necesite signo intencionalmente.

4. Las operaciones bitwise con mascaras ofrecen mas control y portabilidad que
   los bit fields: permiten combinar flags con `|`, testear con `&`, y pasar
   como argumento a funciones.

5. No se puede tomar la direccion (`&`) de un bit field porque un puntero
   apunta a bytes, no a bits. Esto impide usar bit fields con `scanf`, pasar
   por puntero, o cualquier funcion que reciba `int *`.

6. Un campo de ancho 0 (`:0`) fuerza que el siguiente campo empiece en una
   nueva unidad de almacenamiento, duplicando el sizeof del struct.

7. El layout de bits dentro de un byte (MSB vs LSB) es implementation-defined.
   Nunca usar bit fields para representar datos que cruzan limites del programa
   (red, archivos binarios, hardware sin documentacion del compilador).
