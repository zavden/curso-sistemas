# T03 — Buffer overflows

## Qué es un buffer overflow

Un buffer overflow ocurre cuando un programa escribe más datos
de los que caben en un buffer, sobrescribiendo memoria adyacente:

```c
#include <stdio.h>
#include <string.h>

int main(void) {
    char buf[8];
    strcpy(buf, "this string is much longer than 8 bytes");
    // Escribe 40+ bytes en un buffer de 8
    // Sobrescribe todo lo que esté después de buf en el stack

    return 0;
}
```

```c
// ¿Qué hay después de buf en el stack?
//
// Dirección alta
// ┌──────────────────┐
// │ return address    │ ← dirección de retorno de main
// ├──────────────────┤
// │ saved RBP        │ ← frame pointer guardado
// ├──────────────────┤
// │ buf[7]           │
// │ buf[6]           │
// │ buf[5]           │
// │ ...              │
// │ buf[0]           │ ← strcpy empieza aquí
// └──────────────────┘
// Dirección baja
//
// strcpy escribe "this string is..."
// → sobrescribe saved RBP
// → sobrescribe return address
// → cuando main retorna, salta a una dirección corrupta
// → crash (o peor: ejecución de código arbitrario)
```

## Cómo ocurren los buffer overflows

### 1. Funciones de string sin límite

```c
// gets — la función más peligrosa de C:
char buf[100];
gets(buf);     // lee stdin sin límite → eliminada en C11
// Si el usuario escribe más de 99 chars → overflow

// strcpy sin verificar longitud:
char dest[10];
strcpy(dest, user_input);    // si user_input > 9 chars → overflow

// strcat sin verificar espacio:
char buf[20] = "hello";
strcat(buf, user_input);     // si "hello" + user_input > 19 → overflow

// sprintf sin límite:
char buf[50];
sprintf(buf, "User: %s, Data: %s", name, data);
// Si name + data son muy largos → overflow
```

### 2. Errores de cálculo de tamaño

```c
// Off-by-one — olvidar el terminador:
char buf[5];
strncpy(buf, "hello", 5);    // copia 5 chars, NO hay espacio para '\0'
// buf = {'h','e','l','l','o'} — sin terminador

// Calcular mal el espacio disponible:
char buf[100] = "prefix: ";
size_t remaining = sizeof(buf) - strlen(buf);    // 92
strncat(buf, data, remaining);    // ERROR: remaining incluye el '\0' que strncat agrega
strncat(buf, data, remaining - 1);  // CORRECTO
```

### 3. Integer overflow que causa buffer overflow

```c
// Si el tamaño se calcula con overflow aritmético:
size_t count = get_user_count();    // supongamos que retorna SIZE_MAX
size_t size = count * sizeof(int);  // overflow → size = valor muy pequeño

int *arr = malloc(size);            // aloca poco
for (size_t i = 0; i < count; i++) {
    arr[i] = 0;                     // escribe MÁS ALLÁ de lo alocado
}
```

### 4. Stack buffer overflow vs heap buffer overflow

```c
// Stack overflow — buffer en el stack:
void vulnerable(const char *input) {
    char buf[64];            // en el stack
    strcpy(buf, input);      // overflow sobrescribe return address
}

// Heap overflow — buffer en el heap:
void vulnerable2(const char *input) {
    char *buf = malloc(64);  // en el heap
    strcpy(buf, input);      // overflow corrompe metadata del heap
    free(buf);               // o sobrescribe datos de otros bloques
}
```

## Exploits clásicos

### Stack smashing

```c
// Un atacante puede sobrescribir la dirección de retorno
// para ejecutar código arbitrario:

void vulnerable(const char *input) {
    char buf[64];
    strcpy(buf, input);
    // Si input tiene más de 64+8 bytes (buf + saved RBP):
    // los siguientes 8 bytes sobrescriben la return address
}

// El atacante construye un input como:
// [64 bytes de relleno][8 bytes de RBP falso][dirección de código malicioso]
//
// Cuando la función retorna, salta a la dirección del atacante.
// Históricamente, el código malicioso (shellcode) se ponía
// en el mismo buffer → NX bit lo previene hoy.
```

### Ejemplo real: Morris Worm (1988)

```c
// El primer worm de Internet explotó un buffer overflow
// en fingerd (finger daemon):

// El daemon leía input de red con gets():
char buf[512];
gets(buf);    // input de la red, sin límite

// El worm enviaba más de 512 bytes con shellcode
// y una dirección de retorno apuntando al buffer
// → ejecutaba código del atacante en el servidor
```

## Mitigaciones del compilador y SO

### 1. Stack canary (stack protector)

```c
// GCC/Clang insertan un valor "canario" entre el buffer y
// la dirección de retorno:
//
// ┌──────────────────┐
// │ return address    │
// ├──────────────────┤
// │ canary value      │ ← valor aleatorio puesto al entrar
// ├──────────────────┤
// │ buf[63]           │
// │ ...               │
// │ buf[0]            │
// └──────────────────┘
//
// Al retornar, verifica que el canario no cambió.
// Si cambió → abort ("*** stack smashing detected ***")

// Habilitado por defecto con -fstack-protector:
// gcc -fstack-protector-strong prog.c
// -fstack-protector       → protege funciones con arrays
// -fstack-protector-strong → protege más funciones (recomendado)
// -fstack-protector-all    → protege TODAS las funciones
```

### 2. ASLR (Address Space Layout Randomization)

```c
// El SO aleatoriza las direcciones de:
// - Stack
// - Heap
// - Bibliotecas compartidas
// - Ejecutable (con PIE: Position Independent Executable)
//
// El atacante no puede predecir dónde está el código/datos.
// Compilar con PIE:
// gcc -pie -fPIE prog.c
//
// Verificar:
// cat /proc/sys/kernel/randomize_va_space
// 2 = full ASLR (predeterminado en Linux moderno)
```

### 3. NX bit (No-Execute / DEP)

```c
// Las páginas de memoria se marcan como ejecutables o no:
// - Stack: NO ejecutable (NX)
// - Heap: NO ejecutable
// - Código (.text): ejecutable
//
// Si un atacante pone shellcode en el stack y salta ahí:
// → segfault porque la página no es ejecutable.
//
// Habilitado por defecto en Linux moderno.
// gcc -z noexecstack (predeterminado)
```

### 4. FORTIFY_SOURCE

```c
// GCC reemplaza funciones peligrosas por versiones que
// verifican el tamaño en compilación o runtime:
// gcc -D_FORTIFY_SOURCE=2 -O2 prog.c

char buf[10];
strcpy(buf, "this is too long");
// Con FORTIFY: abort en runtime
// *** buffer overflow detected ***: terminated

// FORTIFY reemplaza strcpy por __strcpy_chk que verifica:
// if (__builtin_object_size(dest) < strlen(src) + 1) abort();

// Funciona con: memcpy, strcpy, strcat, sprintf, gets, etc.
// Requiere -O1 o superior (necesita inlining).
```

## Detección con herramientas

### AddressSanitizer (ASan)

```c
// gcc -fsanitize=address -g prog.c
// La herramienta más efectiva para detectar overflows:

void example(void) {
    char buf[8];
    buf[8] = 'x';    // off-by-one
}

// ASan output:
// ERROR: AddressSanitizer: stack-buffer-overflow on address 0x...
// WRITE of size 1 at 0x...
//     #0 0x... in example prog.c:3
//
// Address 0x... is located in stack of thread T0
// at offset 40 in frame 'example'
//     [32, 40) 'buf' (size 8)
```

### Valgrind (solo para heap)

```c
// valgrind ./prog
// Detecta overflows en el HEAP, no en el stack:

void example(void) {
    char *buf = malloc(8);
    buf[8] = 'x';         // heap overflow
    free(buf);
}

// Valgrind output:
// Invalid write of size 1
//   at 0x...: example (prog.c:3)
// Address 0x... is 0 bytes after a block of size 8 alloc'd
```

## Cómo prevenir buffer overflows

### Regla 1: Siempre limitar la cantidad de datos

```c
// MAL:
gets(buf);                        // sin límite
strcpy(dest, src);                // sin límite
sprintf(buf, "%s", data);        // sin límite

// BIEN:
fgets(buf, sizeof(buf), stdin);                   // limitado
snprintf(dest, sizeof(dest), "%s", src);          // limitado
strncpy(dest, src, sizeof(dest) - 1);             // limitado
dest[sizeof(dest) - 1] = '\0';                    // asegurar terminador
```

### Regla 2: Verificar longitudes antes de copiar

```c
size_t src_len = strlen(src);
if (src_len >= sizeof(dest)) {
    // Manejar: truncar, error, o alocar más
    fprintf(stderr, "string too long\n");
    return -1;
}
strcpy(dest, src);    // ahora es seguro
```

### Regla 3: Usar snprintf para todo

```c
// snprintf es la navaja suiza para strings seguros:
char buf[100];

// Formatear:
snprintf(buf, sizeof(buf), "User: %s", name);

// Concatenar:
snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), ", age: %d", age);

// Siempre termina con '\0', nunca desborda, retorna
// cuánto habría escrito → detectar truncación.
```

### Regla 4: Compilar con protecciones

```bash
# Compilación con todas las protecciones:
gcc -Wall -Wextra \
    -fstack-protector-strong \
    -D_FORTIFY_SOURCE=2 \
    -O2 \
    -pie -fPIE \
    -z noexecstack \
    -z relro -z now \
    -fsanitize=address \
    prog.c -o prog

# Para desarrollo: -fsanitize=address,undefined
# Para producción: quitar -fsanitize, mantener el resto
```

## Funciones a evitar vs recomendadas

| Evitar | Usar en su lugar | Razón |
|---|---|---|
| gets() | fgets() | gets no tiene límite — eliminada en C11 |
| strcpy() | snprintf() o strlcpy() | No verifica tamaño del destino |
| strcat() | snprintf() | No verifica espacio disponible |
| sprintf() | snprintf() | No verifica tamaño del buffer |
| scanf("%s") | scanf("%99s") o fgets() | Sin límite por defecto |
| strncpy() | snprintf() | No garantiza terminador |

---

## Ejercicios

### Ejercicio 1 — Provocar y detectar

```c
// Crear un programa con un buffer overflow intencional
// (stack buffer overflow con strcpy).
// 1. Compilar SIN protecciones: gcc -fno-stack-protector
//    ¿Qué pasa al ejecutar? ¿Crash? ¿Silencioso?
// 2. Compilar CON stack protector: gcc -fstack-protector
//    ¿Qué mensaje aparece?
// 3. Compilar con ASan: gcc -fsanitize=address
//    ¿Qué información da ASan?
```

### Ejercicio 2 — Reescribir código vulnerable

```c
// Este código tiene 3 vulnerabilidades. Identificarlas y corregir:
void process_input(void) {
    char name[20];
    char greeting[40];

    printf("Enter name: ");
    gets(name);
    sprintf(greeting, "Hello, %s! Welcome to the system.", name);
    printf("%s\n", greeting);
}
```

### Ejercicio 3 — Safe string builder

```c
// Implementar un "string builder" simple:
// struct StringBuilder { char *buf; size_t len; size_t cap; };
// sb_init(sb, initial_cap) — aloca buffer
// sb_append(sb, str) — agrega string, crece si es necesario
// sb_free(sb) — libera
// Usar solo funciones seguras (snprintf, memcpy con tamaño).
// Verificar con valgrind que no hay leaks.
```
