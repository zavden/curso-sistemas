# T02 — fread, fwrite, fgets, fputs

## I/O de texto: fgets y fputs

### fgets — Leer una línea

```c
char *fgets(char *str, int n, FILE *stream);
// Lee hasta n-1 caracteres o hasta '\n' (lo que ocurra primero).
// INCLUYE el '\n' en str (si cabe).
// SIEMPRE termina con '\0'.
// Retorna str si éxito, NULL si EOF o error.

char buf[100];
while (fgets(buf, sizeof(buf), f) != NULL) {
    // buf contiene una línea (con '\n' al final)
    printf("Line: %s", buf);    // sin \n extra — buf ya tiene uno
}
```

```c
// Comportamiento detallado:
// Archivo contiene: "hello world\n"

char buf[8];
fgets(buf, sizeof(buf), f);
// buf = "hello w\0" — leyó 7 chars (n-1), sin '\n'
// La línea NO está completa — quedan "orld\n" por leer

char buf2[100];
fgets(buf2, sizeof(buf2), f);
// buf2 = "hello world\n\0" — leyó toda la línea incluyendo '\n'

// Detectar si la línea está completa:
size_t len = strlen(buf);
if (len > 0 && buf[len - 1] == '\n') {
    // Línea completa
    buf[len - 1] = '\0';    // quitar '\n' si se desea
} else {
    // Línea truncada (buffer pequeño) o última línea sin '\n'
}
```

```c
// Quitar el '\n' — tres métodos:
char buf[100];
fgets(buf, sizeof(buf), f);

// Método 1 — strcspn (más limpio):
buf[strcspn(buf, "\n")] = '\0';

// Método 2 — strchr:
char *nl = strchr(buf, '\n');
if (nl) *nl = '\0';

// Método 3 — strlen:
size_t len = strlen(buf);
if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
```

### fputs — Escribir un string

```c
int fputs(const char *str, FILE *stream);
// Escribe str al stream. NO agrega '\n'.
// Retorna no-negativo si éxito, EOF si error.

fputs("hello world\n", f);    // con newline explícito
fputs("hello ", f);
fputs("world\n", f);          // se pueden concatenar llamadas
```

### puts vs fputs

```c
// puts escribe a stdout Y agrega '\n':
puts("hello");       // imprime "hello\n"

// fputs NO agrega '\n':
fputs("hello", stdout);    // imprime "hello" (sin newline)
fputs("hello\n", stdout);  // imprime "hello\n"

// puts(s) equivale a: fputs(s, stdout); fputc('\n', stdout);
```

### fgetc y fputc — Un carácter a la vez

```c
// fgetc lee un carácter:
int ch = fgetc(f);    // retorna int, NO char
// Retorna el carácter como unsigned char convertido a int,
// o EOF (-1) si fin de archivo o error.

// CUIDADO: usar int, no char:
int ch;    // CORRECTO — puede almacenar EOF
// char ch; // INCORRECTO — EOF puede confundirse con un carácter válido (0xFF)

while ((ch = fgetc(f)) != EOF) {
    putchar(ch);
}

// fputc escribe un carácter:
fputc('A', f);
fputc('\n', f);
```

## I/O binario: fread y fwrite

### fwrite — Escribir bloques de datos

```c
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
// Escribe nmemb elementos de tamaño size desde ptr al stream.
// Retorna el número de elementos escritos (no bytes).
// Si retorna < nmemb, hubo error.

// Escribir un array de ints:
int data[] = {1, 2, 3, 4, 5};
size_t written = fwrite(data, sizeof(int), 5, f);
if (written != 5) {
    perror("fwrite");
}

// Escribir un struct:
struct Point { int x, y; };
struct Point p = {10, 20};
fwrite(&p, sizeof(struct Point), 1, f);

// Escribir un array de structs:
struct Point points[100];
fwrite(points, sizeof(struct Point), 100, f);
```

### fread — Leer bloques de datos

```c
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
// Lee nmemb elementos de tamaño size del stream a ptr.
// Retorna el número de elementos leídos.
// Si retorna < nmemb, puede ser EOF o error (verificar con feof/ferror).

// Leer un array de ints:
int data[5];
size_t read_count = fread(data, sizeof(int), 5, f);
if (read_count != 5) {
    if (feof(f)) {
        printf("EOF: only read %zu elements\n", read_count);
    } else {
        perror("fread");
    }
}

// Leer un struct:
struct Point p;
fread(&p, sizeof(struct Point), 1, f);
printf("(%d, %d)\n", p.x, p.y);
```

### Ejemplo: serializar y deserializar

```c
#include <stdio.h>
#include <stdlib.h>

struct Record {
    int id;
    char name[50];
    double score;
};

// Guardar array de records a archivo binario:
int save_records(const char *path, const struct Record *recs, int n) {
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return -1; }

    // Escribir la cantidad de records primero:
    fwrite(&n, sizeof(int), 1, f);
    // Escribir los records:
    fwrite(recs, sizeof(struct Record), n, f);

    fclose(f);
    return 0;
}

// Leer array de records desde archivo binario:
int load_records(const char *path, struct Record **recs, int *n) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return -1; }

    fread(n, sizeof(int), 1, f);

    *recs = malloc(*n * sizeof(struct Record));
    if (!*recs) { fclose(f); return -1; }

    size_t got = fread(*recs, sizeof(struct Record), *n, f);
    fclose(f);

    if ((int)got != *n) { free(*recs); return -1; }
    return 0;
}
```

## Texto vs binario

```c
// Diferencia principal: tratamiento de newlines.

// TEXTO ("r", "w"):
// - En Windows: '\n' se convierte a '\r\n' al escribir,
//   '\r\n' se convierte a '\n' al leer.
// - En Linux: no hay conversión.
// - fgets, fputs, fprintf, fscanf trabajan con texto.

// BINARIO ("rb", "wb"):
// - Sin conversión de newlines en ningún SO.
// - Los bytes se leen/escriben exactamente como están.
// - fread, fwrite trabajan con datos crudos.

// En Linux no hay diferencia práctica, pero para portabilidad:
// - Usar "r"/"w" para archivos de texto (CSV, config, logs)
// - Usar "rb"/"wb" para archivos binarios (imágenes, datos, DB)
```

```c
// CUIDADO con fread/fwrite para datos binarios:

// 1. Endianness: un archivo binario escrito en little-endian
//    no se lee correctamente en big-endian.

// 2. Padding: un struct escrito con un compilador puede tener
//    padding diferente en otro compilador/arquitectura.

// 3. Tamaño de tipos: sizeof(int) puede ser diferente.

// Para archivos binarios portátiles:
// - Usar tipos de tamaño fijo (uint32_t, etc.)
// - Serializar campo por campo (no structs enteros)
// - Definir un endianness (generalmente little-endian)
// - O usar formatos de texto (JSON, CSV)
```

## Posicionamiento: ftell, fseek, rewind

```c
// ftell — obtener posición actual:
long pos = ftell(f);
// Retorna la posición actual en bytes desde el inicio.

// fseek — mover a una posición:
int fseek(FILE *stream, long offset, int whence);
// whence:
//   SEEK_SET — desde el inicio del archivo
//   SEEK_CUR — desde la posición actual
//   SEEK_END — desde el final del archivo

fseek(f, 0, SEEK_SET);      // ir al inicio
fseek(f, 0, SEEK_END);      // ir al final
fseek(f, -10, SEEK_CUR);    // retroceder 10 bytes

// rewind — equivale a fseek(f, 0, SEEK_SET) + clearerr:
rewind(f);

// Obtener el tamaño de un archivo:
fseek(f, 0, SEEK_END);
long size = ftell(f);
rewind(f);
printf("File size: %ld bytes\n", size);
```

---

## Ejercicios

### Ejercicio 1 — Leer líneas

```c
// Implementar una función que lea un archivo de texto línea
// por línea con fgets, quite el '\n', e imprima cada línea
// con su número: "  1: primera línea"
// Manejar líneas más largas que el buffer (256 bytes).
```

### Ejercicio 2 — Archivo binario de structs

```c
// Definir struct Student { int id; char name[50]; double gpa; }.
// Implementar:
// - save_students(path, students, n) — guarda en binario
// - load_students(path, students_out, n_out) — lee
// Guardar 5 estudiantes, leerlos en otro array, e imprimir.
// Verificar que los datos son idénticos.
```

### Ejercicio 3 — Copiar archivo binario

```c
// Implementar copy_file(src, dst) usando fread/fwrite
// con un buffer de 4096 bytes.
// Copiar un archivo de cualquier tamaño.
// Comparar el original y la copia con diff o cmp.
```
