# T03 — fprintf, fscanf, sprintf, sscanf

## La familia printf

```c
// Todas escriben con formato — difieren en el destino:

int printf(const char *fmt, ...);                  // → stdout
int fprintf(FILE *f, const char *fmt, ...);        // → archivo
int sprintf(char *str, const char *fmt, ...);      // → string (PELIGROSO)
int snprintf(char *str, size_t n, const char *fmt, ...);  // → string (SEGURO)
int dprintf(int fd, const char *fmt, ...);         // → file descriptor (POSIX)

// Todas retornan el número de caracteres escritos (sin '\0').
// snprintf retorna lo que HABRÍA escrito (para detectar truncación).
```

### fprintf — Escribir a archivo con formato

```c
FILE *f = fopen("log.txt", "w");

fprintf(f, "User: %s, Age: %d\n", "Alice", 30);
fprintf(f, "Score: %.2f%%\n", 95.5);
fprintf(f, "Timestamp: %ld\n", time(NULL));

// fprintf a stderr para mensajes de error:
fprintf(stderr, "Error: %s\n", strerror(errno));

fclose(f);
```

### sprintf vs snprintf

```c
// sprintf — SIN límite de tamaño → buffer overflow:
char buf[20];
sprintf(buf, "Hello, %s! Your score is %d", name, score);
// Si el resultado es > 19 chars → buffer overflow → UB

// snprintf — CON límite → seguro:
char buf[20];
int n = snprintf(buf, sizeof(buf), "Hello, %s! Your score is %d", name, score);
// Escribe máximo sizeof(buf)-1 chars + '\0'.
// Retorna cuántos chars HABRÍA escrito sin límite.

if (n >= (int)sizeof(buf)) {
    printf("Truncated! Needed %d bytes\n", n + 1);
}

// REGLA: NUNCA usar sprintf. SIEMPRE usar snprintf.
```

### Formatos de printf

```c
// Enteros:
printf("%d",   42);        // 42 (decimal, signed)
printf("%u",   42u);       // 42 (decimal, unsigned)
printf("%x",   255);       // ff (hexadecimal)
printf("%X",   255);       // FF (hexadecimal mayúscula)
printf("%o",   8);         // 10 (octal)
printf("%ld",  42L);       // long
printf("%lld", 42LL);      // long long
printf("%zu",  sizeof(int));  // size_t

// Flotantes:
printf("%f",   3.14);      // 3.140000 (6 decimales)
printf("%.2f", 3.14);      // 3.14 (2 decimales)
printf("%e",   3.14);      // 3.140000e+00 (notación científica)
printf("%g",   3.14);      // 3.14 (la más corta de %f y %e)

// Strings y chars:
printf("%s",   "hello");   // hello
printf("%.3s", "hello");   // hel (máximo 3 chars)
printf("%c",   'A');       // A

// Punteros:
printf("%p", (void *)ptr); // 0x7ffd1234abcd

// Ancho y alineación:
printf("%10d",  42);       //         42 (derecha, ancho 10)
printf("%-10d", 42);       // 42         (izquierda, ancho 10)
printf("%010d", 42);       // 0000000042 (relleno con ceros)
printf("%+d",   42);       // +42 (siempre mostrar signo)
```

## La familia scanf

```c
// Todas leen con formato — difieren en la fuente:

int scanf(const char *fmt, ...);                   // ← stdin
int fscanf(FILE *f, const char *fmt, ...);         // ← archivo
int sscanf(const char *str, const char *fmt, ...); // ← string

// Retornan el número de campos leídos exitosamente.
// Retornan EOF si error o fin de entrada antes de leer algo.
```

### fscanf — Leer de archivo con formato

```c
FILE *f = fopen("data.txt", "r");
// data.txt contiene: "Alice 30 95.5"

char name[50];
int age;
double score;

int read = fscanf(f, "%49s %d %lf", name, &age, &score);
// %49s: lee string (máx 49 chars + '\0')
// %d: lee int
// %lf: lee double (lf para double en scanf, f para float)

if (read == 3) {
    printf("Name: %s, Age: %d, Score: %.1f\n", name, age, score);
} else {
    printf("Failed to parse (read %d fields)\n", read);
}

fclose(f);
```

### sscanf — Parsear un string

```c
// sscanf lee de un string en vez de un archivo:
const char *line = "2026-03-17 Alice 95.5";

int year, month, day;
char name[50];
double score;

int n = sscanf(line, "%d-%d-%d %49s %lf",
               &year, &month, &day, name, &score);

if (n == 5) {
    printf("%s scored %.1f on %d-%02d-%02d\n",
           name, score, year, month, day);
}
```

```c
// sscanf es útil para parsear datos ya leídos con fgets:
char buf[256];
while (fgets(buf, sizeof(buf), f)) {
    int id;
    char name[50];
    if (sscanf(buf, "%d %49s", &id, name) == 2) {
        printf("ID: %d, Name: %s\n", id, name);
    }
}
// fgets lee la línea completa (seguro),
// sscanf la parsea (sin riesgo de buffer overflow con límites).
```

### Peligros de scanf

```c
// 1. %s sin límite → buffer overflow:
char name[10];
scanf("%s", name);    // si el input es > 9 chars → overflow
scanf("%9s", name);   // CORRECTO — límite de 9 chars

// 2. scanf deja el '\n' en stdin:
int n;
scanf("%d", &n);      // lee "42\n" — consume "42", deja '\n'
char c;
scanf("%c", &c);      // c = '\n' — lee el '\n' residual
// Solución: scanf(" %c", &c) — el espacio consume whitespace

// 3. Si el input no coincide, scanf se queda atascado:
// Input: "abc"
int x;
if (scanf("%d", &x) != 1) {
    // "abc" sigue en stdin — el siguiente scanf también falla
    // Hay que consumir el input inválido:
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF);
}

// 4. scanf con %s solo lee hasta whitespace:
scanf("%s", name);    // input "Alice Bob" → name = "Alice"
// "Bob" queda en stdin

// RECOMENDACIÓN: usar fgets + sscanf en lugar de scanf directo.
```

## Vulnerabilidades de format string

```c
// Si el format string viene del usuario → vulnerabilidad grave:

char user_input[100];
fgets(user_input, sizeof(user_input), stdin);

// VULNERABLE:
printf(user_input);    // el usuario controla el format string
// Si input = "%x %x %x %x": lee valores del stack
// Si input = "%n": ESCRIBE en memoria (número de chars impresos)
// Puede llevar a ejecución de código arbitrario.

// SEGURO:
printf("%s", user_input);    // el usuario no controla el formato

// fputs(user_input, stdout); // alternativa sin formateo
```

```c
// gcc -Wformat-security avisa:
printf(user_input);
// warning: format not a string literal and no format arguments

// REGLA: el primer argumento de printf SIEMPRE debe ser un
// string literal, NUNCA una variable del usuario.

// El ataque de format string es uno de los más antiguos
// y peligrosos en C. Explotaba programas como:
// syslog(user_message);    // syslog usa printf internamente
// fprintf(log, user_message);
```

## Funciones v* (con va_list)

```c
#include <stdarg.h>

// Para crear wrappers de printf/scanf:
void log_msg(const char *level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "[%s] ", level);
    vfprintf(stderr, fmt, args);    // vfprintf acepta va_list
    fprintf(stderr, "\n");

    va_end(args);
}

log_msg("ERROR", "Failed to open %s: %s", path, strerror(errno));
// [ERROR] Failed to open /tmp/data: No such file or directory

// Familia v*:
// vprintf, vfprintf, vsprintf, vsnprintf
// vscanf, vfscanf, vsscanf
```

---

## Ejercicios

### Ejercicio 1 — Parsear CSV con fgets + sscanf

```c
// Leer un archivo CSV con formato: name,age,score
// Usar fgets para leer cada línea.
// Usar sscanf para parsear los campos.
// Almacenar en un array de structs e imprimir una tabla.
```

### Ejercicio 2 — Safe log function

```c
// Implementar void log_to_file(FILE *f, const char *level, const char *fmt, ...)
// que escriba: [LEVEL] YYYY-MM-DD HH:MM:SS mensaje
// Usar vfprintf internamente.
// Agregar __attribute__((format(printf, 3, 4))) para verificación.
// Probar con varios niveles y formatos.
```

### Ejercicio 3 — snprintf builder

```c
// Implementar un string builder basado en snprintf:
// char buf[1024];
// int pos = 0;
// pos += snprintf(buf+pos, sizeof(buf)-pos, "Name: %s\n", name);
// pos += snprintf(buf+pos, sizeof(buf)-pos, "Age: %d\n", age);
// Crear una función que genere un "informe" de un array de structs
// en un buffer, detectando si hubo truncación.
```
