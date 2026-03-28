# T02 — Bitwise

## Los operadores bitwise

Los operadores bitwise trabajan sobre los bits individuales
de un valor. Se usan para manipular flags, máscaras, protocolos
y programación de bajo nivel:

```c
&    // AND bitwise
|    // OR bitwise
^    // XOR bitwise (OR exclusivo)
~    // NOT bitwise (complemento)
<<   // shift left (desplazamiento a la izquierda)
>>   // shift right (desplazamiento a la derecha)
```

## AND bitwise (&)

Cada bit del resultado es 1 solo si ambos bits de los
operandos son 1:

```
  1010 1100
& 1111 0000
──────────
  1010 0000
```

```c
unsigned char a = 0xAC;    // 1010 1100
unsigned char b = 0xF0;    // 1111 0000
unsigned char c = a & b;   // 1010 0000 = 0xA0

// Uso principal: EXTRAER bits con una máscara
uint8_t byte = 0b11010110;
uint8_t low_nibble  = byte & 0x0F;   // 0b00000110 = 6
uint8_t high_nibble = byte & 0xF0;   // 0b11010000 = 0xD0

// Verificar si un bit específico está encendido:
if (byte & 0x04) {         // 0x04 = 0b00000100
    // el bit 2 está encendido
}

// Verificar si es par (bit 0 = 0):
if ((n & 1) == 0) {
    // n es par
}
```

## OR bitwise (|)

Cada bit del resultado es 1 si al menos uno de los bits es 1:

```
  1010 0000
| 0000 1100
──────────
  1010 1100
```

```c
unsigned char a = 0xA0;    // 1010 0000
unsigned char b = 0x0C;    // 0000 1100
unsigned char c = a | b;   // 1010 1100 = 0xAC

// Uso principal: ENCENDER bits
uint8_t flags = 0;
flags |= 0x01;     // encender bit 0
flags |= 0x04;     // encender bit 2
// flags = 0b00000101 = 0x05
```

## XOR bitwise (^)

Cada bit del resultado es 1 si los bits son diferentes:

```
  1010 1100
^ 1111 0000
──────────
  0101 1100
```

```c
unsigned char a = 0xAC;    // 1010 1100
unsigned char b = 0xF0;    // 1111 0000
unsigned char c = a ^ b;   // 0101 1100 = 0x5C

// Propiedades de XOR:
// a ^ a == 0        (cualquier valor XOR consigo mismo = 0)
// a ^ 0 == a        (XOR con 0 no cambia nada)
// a ^ b ^ b == a    (XOR doble cancela)

// Uso: INVERTIR bits (toggle)
flags ^= 0x04;     // invertir bit 2 (si era 1→0, si era 0→1)

// Uso: swap sin variable temporal
int a = 5, b = 3;
a ^= b;     // a = 5^3 = 6
b ^= a;     // b = 3^6 = 5
a ^= b;     // a = 6^5 = 3
// Ahora a=3, b=5 — intercambiados
// (curiosidad, no usar en código real — es menos legible y no más rápido)
```

## NOT bitwise (~)

Invierte todos los bits:

```
~ 1010 1100
──────────
  0101 0011
```

```c
unsigned char a = 0xAC;    // 1010 1100
unsigned char b = ~a;      // 0101 0011 = 0x53

// Uso principal: crear máscara inversa para APAGAR bits
flags &= ~0x04;    // apagar bit 2
// ~0x04 = 0b...11111011
// flags & 0b11111011 → apaga el bit 2, deja los demás

// CUIDADO con el tipo:
unsigned char x = 0x0F;
// ~x como unsigned char sería 0xF0
// Pero ~ promueve a int primero:
// ~x = 0xFFFFFF0 (en 32-bit int)
// Esto importa si asignas a un tipo más grande
```

## Shift left (<<)

Desplaza los bits a la izquierda, rellenando con ceros:

```
   0000 1010  << 2
   ──────────
   0010 1000
```

```c
unsigned int x = 0x0A;     // 0000 1010 = 10
unsigned int y = x << 2;   // 0010 1000 = 40

// Shift left por N es equivalente a multiplicar por 2^N:
x << 0    // x * 1   = x
x << 1    // x * 2
x << 2    // x * 4
x << 3    // x * 8
x << N    // x * 2^N

// Crear una máscara con un solo bit encendido:
1 << 0    // 0b00000001 = bit 0
1 << 3    // 0b00001000 = bit 3
1 << 7    // 0b10000000 = bit 7
1U << 31  // bit 31 (usar 1U para evitar UB con signed)
```

```c
// PELIGROS:
// Shift por un valor >= ancho del tipo → UB
int bad = 1 << 32;     // UB si int es 32 bits

// Shift de valor negativo → UB
int bad2 = -1 << 2;    // UB en C11

// Shift left de signed que produce overflow → UB
int bad3 = 1 << 31;    // UB: overflow signed (el bit entra en el signo)
unsigned int ok = 1U << 31;  // OK: unsigned
```

## Shift right (>>)

Desplaza los bits a la derecha:

```
   0010 1000  >> 2
   ──────────
   0000 1010
```

```c
unsigned int x = 40;       // 0010 1000
unsigned int y = x >> 2;   // 0000 1010 = 10

// Shift right por N es equivalente a dividir por 2^N (para unsigned):
x >> 1    // x / 2
x >> 2    // x / 4
x >> N    // x / 2^N

// Para UNSIGNED: se rellena con ceros (logical shift)
// Para SIGNED: implementation-defined (arithmetic o logical)
// En la práctica (GCC/x86): arithmetic shift (rellena con el bit de signo)
int neg = -8;             // 1111...1000
int shifted = neg >> 1;   // 1111...1100 = -4 (arithmetic shift)
// Pero esto NO está garantizado por el estándar.
```

## Aplicaciones prácticas

### Flags con bits

```c
// Patrón clásico para flags/opciones:

// Definir las flags como potencias de 2:
#define FLAG_READ    (1U << 0)    // 0b0001 = 1
#define FLAG_WRITE   (1U << 1)    // 0b0010 = 2
#define FLAG_EXEC    (1U << 2)    // 0b0100 = 4
#define FLAG_HIDDEN  (1U << 3)    // 0b1000 = 8

unsigned int perms = 0;

// Encender un flag:
perms |= FLAG_READ;               // set bit
perms |= FLAG_READ | FLAG_WRITE;  // set múltiples bits

// Apagar un flag:
perms &= ~FLAG_WRITE;             // clear bit

// Invertir un flag:
perms ^= FLAG_HIDDEN;             // toggle bit

// Verificar un flag:
if (perms & FLAG_READ) {
    // lectura permitida
}

// Verificar múltiples flags (TODOS deben estar):
if ((perms & (FLAG_READ | FLAG_WRITE)) == (FLAG_READ | FLAG_WRITE)) {
    // lectura Y escritura
}

// Verificar si AL MENOS uno está:
if (perms & (FLAG_READ | FLAG_WRITE)) {
    // lectura O escritura (o ambas)
}
```

```c
// Ejemplo real — permisos de archivo Unix:
// rwxrwxrwx = 9 bits
#define S_IRUSR  (1 << 8)    // 0400 — owner read
#define S_IWUSR  (1 << 7)    // 0200 — owner write
#define S_IXUSR  (1 << 6)    // 0100 — owner execute
#define S_IRGRP  (1 << 5)    // 0040 — group read
#define S_IWGRP  (1 << 4)    // 0020 — group write
#define S_IXGRP  (1 << 3)    // 0010 — group execute
#define S_IROTH  (1 << 2)    // 0004 — other read
#define S_IWOTH  (1 << 1)    // 0002 — other write
#define S_IXOTH  (1 << 0)    // 0001 — other execute
```

### Máscaras para extraer campos

```c
// Extraer bits específicos de un valor:

// Un color RGB565 empaquetado en 16 bits:
// RRRR RGGG GGGB BBBB
uint16_t pixel = 0xF81F;   // rojo puro + azul puro

uint8_t red   = (pixel >> 11) & 0x1F;   // 5 bits superiores
uint8_t green = (pixel >>  5) & 0x3F;   // 6 bits medios
uint8_t blue  =  pixel        & 0x1F;   // 5 bits inferiores

// Construir un pixel:
uint16_t new_pixel = ((uint16_t)red << 11)
                   | ((uint16_t)green << 5)
                   | blue;
```

### Potencia de 2

```c
// Verificar si un número es potencia de 2:
// Una potencia de 2 tiene exactamente un bit encendido:
// 8 = 0b1000, 8-1 = 0b0111
// 8 & 7 = 0 → es potencia de 2

int is_power_of_2(unsigned int n) {
    return n != 0 && (n & (n - 1)) == 0;
}

// Redondear hacia arriba a la siguiente potencia de 2:
uint32_t next_power_of_2(uint32_t n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
}
```

### Swap de bytes

```c
// Invertir el orden de bytes (endianness):
uint32_t swap_bytes(uint32_t x) {
    return ((x >> 24) & 0x000000FF)
         | ((x >>  8) & 0x0000FF00)
         | ((x <<  8) & 0x00FF0000)
         | ((x << 24) & 0xFF000000);
}
```

### Contar bits

```c
// Contar bits encendidos (population count):
int popcount(uint32_t x) {
    int count = 0;
    while (x) {
        count += x & 1;
        x >>= 1;
    }
    return count;
}

// Versión optimizada (Brian Kernighan):
int popcount_fast(uint32_t x) {
    int count = 0;
    while (x) {
        x &= x - 1;    // apaga el bit menos significativo
        count++;
    }
    return count;
}

// GCC built-in:
int n = __builtin_popcount(x);     // usa instrucción POPCNT si disponible
```

## Asignación compuesta

```c
// Todos los operadores bitwise tienen versión compuesta:
flags |=  FLAG_READ;     // flags = flags | FLAG_READ
flags &= ~FLAG_WRITE;   // flags = flags & ~FLAG_WRITE
flags ^=  FLAG_HIDDEN;  // flags = flags ^ FLAG_HIDDEN
value <<= 2;             // value = value << 2
value >>= 1;             // value = value >> 1
```

## Tabla de operaciones

| Operación | Operador | Ejemplo | Resultado |
|---|---|---|---|
| AND | & | 0b1100 & 0b1010 | 0b1000 |
| OR | \| | 0b1100 \| 0b1010 | 0b1110 |
| XOR | ^ | 0b1100 ^ 0b1010 | 0b0110 |
| NOT | ~ | ~0b1100 | 0b...0011 |
| Shift left | << | 0b0001 << 3 | 0b1000 |
| Shift right | >> | 0b1000 >> 2 | 0b0010 |

| Operación sobre flags | Código |
|---|---|
| Set bit | `flags \|= BIT` |
| Clear bit | `flags &= ~BIT` |
| Toggle bit | `flags ^= BIT` |
| Test bit | `if (flags & BIT)` |

---

## Ejercicios

### Ejercicio 1 — Flags

```c
// Definir flags para un sistema de permisos:
// CAN_READ, CAN_WRITE, CAN_DELETE, IS_ADMIN
// Crear una variable permissions, encender READ y WRITE,
// verificar si tiene DELETE, encender ADMIN,
// apagar WRITE, imprimir el resultado en binario.
```

### Ejercicio 2 — Extraer campos

```c
// Un color RGB está empaquetado en un uint32_t como 0x00RRGGBB.
// Escribir funciones:
// uint8_t get_red(uint32_t color);
// uint8_t get_green(uint32_t color);
// uint8_t get_blue(uint32_t color);
// uint32_t make_color(uint8_t r, uint8_t g, uint8_t b);
```

### Ejercicio 3 — Potencias de 2

```c
// Implementar is_power_of_2(unsigned int n).
// Probar con: 0, 1, 2, 3, 4, 7, 8, 16, 255, 256.
// ¿Por qué funciona n & (n-1)?
```
