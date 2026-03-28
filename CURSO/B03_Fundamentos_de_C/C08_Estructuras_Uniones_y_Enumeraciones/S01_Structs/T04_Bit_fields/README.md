# T04 — Bit fields

## Qué son los bit fields

Los bit fields permiten declarar campos de un struct que
ocupan un número específico de **bits** (no bytes):

```c
#include <stdio.h>

struct Flags {
    unsigned int read    : 1;    // 1 bit
    unsigned int write   : 1;    // 1 bit
    unsigned int execute : 1;    // 1 bit
};

int main(void) {
    struct Flags perms = { .read = 1, .write = 1, .execute = 0 };

    printf("read=%u write=%u exec=%u\n",
           perms.read, perms.write, perms.execute);
    // read=1 write=1 exec=0

    perms.execute = 1;
    printf("sizeof = %zu\n", sizeof(struct Flags));    // 4 (un unsigned int)

    return 0;
}
```

```c
// Sintaxis:
// tipo nombre : ancho_en_bits;
//
// tipo: unsigned int, signed int, int, _Bool (C99+)
//       Otros tipos son implementation-defined.
// ancho: 1 a sizeof(tipo)*8 bits

struct PackedData {
    unsigned int day   : 5;    // 0-31 (5 bits)
    unsigned int month : 4;    // 0-15 (4 bits)
    unsigned int year  : 7;    // 0-127 (7 bits, desde offset como 1900+)
};
// Total: 16 bits → cabe en un unsigned int de 4 bytes
// sizeof = 4

struct PackedData date = { .day = 15, .month = 3, .year = 126 };
printf("%d/%d/%d\n", date.day, date.month, date.year + 1900);
// 15/3/2026
```

## Signed vs unsigned en bit fields

```c
// SIEMPRE usar unsigned para bit fields:
struct BadSigned {
    int value : 3;    // signed — rango: -4 a 3 (complemento a dos)
};

struct BadSigned s = { .value = 5 };
printf("%d\n", s.value);    // implementation-defined
// 5 en 3 bits signed = 101 → interpretado como -3

struct GoodUnsigned {
    unsigned int value : 3;    // unsigned — rango: 0 a 7
};

struct GoodUnsigned g = { .value = 5 };
printf("%u\n", g.value);    // 5 — correcto

// _Bool para campos de 1 bit:
struct BoolFields {
    _Bool active : 1;     // 0 o 1
    _Bool visible : 1;
    _Bool locked : 1;
};
```

## Campos sin nombre y de ancho 0

```c
// Campo sin nombre — padding explícito:
struct Register {
    unsigned int opcode  : 4;
    unsigned int         : 4;    // 4 bits de padding (sin nombre)
    unsigned int operand : 8;
};

// Campo de ancho 0 — fuerza alineación al siguiente unit:
struct Aligned {
    unsigned int a : 3;
    unsigned int   : 0;    // fuerza que b empiece en el siguiente int
    unsigned int b : 5;
};
// sizeof puede ser 8 (a en un int, b en otro int)
// Sin el :0, sizeof podría ser 4 (a y b en el mismo int)
```

## Layout: NO portátil

```c
// El estándar C NO especifica:
// 1. El orden de los bits dentro del byte (MSB o LSB primero)
// 2. Si los bit fields cruzan límites de unidad de almacenamiento
// 3. La alineación de los bit fields
// 4. El endianness de los bits

// Ejemplo: struct { unsigned a:4; unsigned b:4; }
//
// En big-endian:      [aaaa bbbb]
// En little-endian:   [bbbb aaaa]  (a veces)
//
// Cada combinación compilador + arquitectura puede ser diferente.
// GCC x86-64: los campos se asignan de LSB a MSB.
// Otro compilador/arch: puede ser al revés.

// CONCLUSIÓN:
// - NO usar bit fields para representar datos que se envían por red
//   o se escriben a archivos binarios.
// - Para esos casos, usar masks y shifts explícitos.
```

```c
// En lugar de bit fields para protocolos:

// MAL — no portátil:
struct __attribute__((packed)) IPHeader {
    unsigned int version : 4;
    unsigned int ihl     : 4;
    // El orden de version e ihl depende del compilador/arch
};

// BIEN — portátil con masks:
uint8_t header[20];
uint8_t version = (header[0] >> 4) & 0x0F;
uint8_t ihl     = header[0] & 0x0F;
```

## Cuándo usar bit fields

```c
// USAR para:
// 1. Campos booleanos internos del programa:
struct Options {
    unsigned int verbose   : 1;
    unsigned int debug     : 1;
    unsigned int color     : 1;
    unsigned int recursive : 1;
};
// 4 booleans en 4 bits en lugar de 4 bytes.

// 2. Enums con rango pequeño:
struct Status {
    unsigned int state  : 3;    // 0-7 estados posibles
    unsigned int priority : 4;  // 0-15 niveles de prioridad
    unsigned int error : 1;     // flag de error
};

// 3. Ahorrar memoria en arrays grandes de structs:
struct Particle {
    float x, y, z;           // 12 bytes
    unsigned int type  : 4;  // 16 tipos
    unsigned int alive : 1;  // vivo/muerto
    unsigned int group : 5;  // 32 grupos
};
// Sin bit fields: 16 bytes (12 + 4 bytes para 3 campos int)
// Con bit fields: 16 bytes (12 + 4 bytes para todos los bit fields)
// Ahorro individual pequeño, pero en millones de partículas importa.
```

```c
// NO USAR para:
// 1. Protocolos de red o formatos de archivo
// 2. Registros de hardware (excepto con documentación específica del compiler)
// 3. Datos que se comparten entre programas/plataformas
// 4. Cuando necesitas la dirección de un campo (&field)
//    No se puede tomar la dirección de un bit field.
```

## No se puede tomar la dirección

```c
struct Flags {
    unsigned int a : 1;
    unsigned int b : 1;
};

struct Flags f = {0};

// &f.a;    // ERROR: cannot take address of bit-field 'a'
// int *p = &f.a;    // ERROR

// Esto significa que:
// - No se pueden pasar bit fields por puntero
// - No se pueden usar con scanf(&f.a)
// - No se pueden usar con funciones que reciben punteros

// Workaround:
int temp;
scanf("%d", &temp);
f.a = temp;
```

## Bit fields vs operaciones bitwise

```c
// Bit fields:
struct Perms {
    unsigned int read  : 1;
    unsigned int write : 1;
    unsigned int exec  : 1;
};
struct Perms p = { .read = 1, .write = 1 };
if (p.read) { /* ... */ }

// Operaciones bitwise (más control, portátil):
#define PERM_READ  (1u << 0)
#define PERM_WRITE (1u << 1)
#define PERM_EXEC  (1u << 2)

unsigned int perms = PERM_READ | PERM_WRITE;
if (perms & PERM_READ) { /* ... */ }
perms |= PERM_EXEC;     // agregar
perms &= ~PERM_WRITE;   // quitar

// Bitwise es preferido cuando:
// - Se necesita portabilidad
// - Se combinan flags con |
// - Se testean flags con &
// - Se necesita pasar como argumento a funciones

// Bit fields son preferidos cuando:
// - Se quiere sintaxis legible (p.read en vez de p & READ)
// - Se usan solo internamente
// - No se necesita combinar flags
```

---

## Ejercicios

### Ejercicio 1 — Fecha compacta

```c
// Crear struct CompactDate con bit fields:
// - day: 5 bits (1-31)
// - month: 4 bits (1-12)
// - year: 12 bits (0-4095, representando el año directamente)
// Verificar sizeof.
// Almacenar la fecha actual y imprimirla.
// ¿Cuántas CompactDates caben en 1 MB vs struct { int d,m,y; }?
```

### Ejercicio 2 — Comparar portabilidad

```c
// Definir una estructura de permisos de dos formas:
// 1. Con bit fields (read, write, execute, setuid, setgid, sticky)
// 2. Con #define y operaciones bitwise
// Implementar las mismas operaciones con ambas:
// - Establecer un permiso
// - Quitar un permiso
// - Verificar un permiso
// - Imprimir todos los permisos como "rwx"
```

### Ejercicio 3 — Bit fields en array

```c
// Crear struct Pixel { unsigned r:5; unsigned g:6; unsigned b:5; }
// (formato RGB565 — 16 bits por pixel).
// Crear un array de 100x100 pixels.
// Llenar con un gradiente (r de 0 a 31, g de 0 a 63).
// Calcular sizeof del array total.
// Comparar con struct { uint8_t r, g, b; } (RGB888).
```
