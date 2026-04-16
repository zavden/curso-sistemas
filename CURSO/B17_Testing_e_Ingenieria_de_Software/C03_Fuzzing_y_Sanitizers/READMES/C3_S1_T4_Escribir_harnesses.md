# T04 - Escribir harnesses: el arte de conectar el fuzzer con tu código

## Índice

1. [Qué es un harness y por qué es el paso más importante](#1-qué-es-un-harness-y-por-qué-es-el-paso-más-importante)
2. [La función target: qué fuzzear](#2-la-función-target-qué-fuzzear)
3. [Elegir el punto de entrada correcto](#3-elegir-el-punto-de-entrada-correcto)
4. [Patrones de harness según tipo de input](#4-patrones-de-harness-según-tipo-de-input)
5. [Patrón 1: bytes crudos → función](#5-patrón-1-bytes-crudos--función)
6. [Patrón 2: bytes → string null-terminated](#6-patrón-2-bytes--string-null-terminated)
7. [Patrón 3: bytes → estructura binaria](#7-patrón-3-bytes--estructura-binaria)
8. [Patrón 4: bytes → archivo temporal](#8-patrón-4-bytes--archivo-temporal)
9. [Patrón 5: bytes → múltiples parámetros](#9-patrón-5-bytes--múltiples-parámetros)
10. [Patrón 6: bytes → operaciones secuenciales (stateful)](#10-patrón-6-bytes--operaciones-secuenciales-stateful)
11. [Patrón 7: roundtrip (encode/decode)](#11-patrón-7-roundtrip-encodedecode)
12. [Patrón 8: differential (comparar implementaciones)](#12-patrón-8-differential-comparar-implementaciones)
13. [Limitar el input: tamaño y complejidad](#13-limitar-el-input-tamaño-y-complejidad)
14. [Limitar recursos: timeout](#14-limitar-recursos-timeout)
15. [Limitar recursos: memoria](#15-limitar-recursos-memoria)
16. [Limitar recursos: profundidad de recursión](#16-limitar-recursos-profundidad-de-recursión)
17. [Inicialización del harness](#17-inicialización-del-harness)
18. [Manejo de estado global](#18-manejo-de-estado-global)
19. [Harness para APIs con múltiples funciones](#19-harness-para-apis-con-múltiples-funciones)
20. [Harness para parsers de formatos complejos](#20-harness-para-parsers-de-formatos-complejos)
21. [Harness con custom mutators para formatos con checksum](#21-harness-con-custom-mutators-para-formatos-con-checksum)
22. [Harness para librerías de red (sin red real)](#22-harness-para-librerías-de-red-sin-red-real)
23. [Anti-patrones: harnesses que matan la eficiencia](#23-anti-patrones-harnesses-que-matan-la-eficiencia)
24. [Checklist de calidad de un harness](#24-checklist-de-calidad-de-un-harness)
25. [Harness portátil: AFL++ y libFuzzer con el mismo código](#25-harness-portátil-afl-y-libfuzzer-con-el-mismo-código)
26. [Comparación con Rust y Go](#26-comparación-con-rust-y-go)
27. [Errores comunes](#27-errores-comunes)
28. [Ejemplo completo: fuzzear una librería de compresión LZ77](#28-ejemplo-completo-fuzzear-una-librería-de-compresión-lz77)
29. [Programa de práctica: http_header_parser](#29-programa-de-práctica-http_header_parser)
30. [Ejercicios](#30-ejercicios)

---

## 1. Qué es un harness y por qué es el paso más importante

Un harness (arnés) es el código que conecta el fuzzer con la función que quieres
testear. Es el adaptador que traduce los bytes aleatorios del fuzzer en llamadas
a tu API.

```
┌──────────────────────────────────────────────────────────────────────┐
│                 El harness es el cuello de botella                    │
│                                                                      │
│  Fuzzer    ─── bytes aleatorios ───▶  HARNESS  ───▶  Función target │
│  (AFL++,                              (tu código)     (tu librería)  │
│   libFuzzer)                                                         │
│                                                                      │
│  Un harness BUENO:                                                   │
│  ✓ Traduce bytes a inputs significativos para el target              │
│  ✓ Ejerce la mayor superficie de ataque posible                     │
│  ✓ Es rápido (no introduce overhead innecesario)                    │
│  ✓ No tiene state leakage entre ejecuciones                         │
│  ✓ Limita recursos sensatamente (timeout, memoria)                  │
│  ✓ Libera toda la memoria que aloca                                  │
│                                                                      │
│  Un harness MALO:                                                    │
│  ✗ Solo llama a una función trivial (getters/setters)               │
│  ✗ Ignora la mayoría de los bytes del fuzzer                        │
│  ✗ Es lento (I/O, sleep, red)                                      │
│  ✗ Tiene state leakage (variables globales sin reset)               │
│  ✗ Memory leaks (en in-process/libFuzzer)                            │
│  ✗ No limita la profundidad de recursión                            │
│                                                                      │
│  La calidad del harness determina la calidad del fuzzing.            │
│  Puedes tener el mejor fuzzer del mundo, pero si el harness          │
│  no ejerce el código interesante, no encontrarás bugs.              │
└──────────────────────────────────────────────────────────────────────┘
```

### Analogía

El harness es como un adaptador de enchufe eléctrico:
- El fuzzer produce "electricidad" (bytes aleatorios)
- Tu código necesita "un tipo específico de enchufe" (struct, string, archivo)
- El harness convierte de uno a otro

Si el adaptador es malo, no llega corriente. Si el adaptador es bueno, toda la
corriente llega al aparato.

---

## 2. La función target: qué fuzzear

No todas las funciones de un programa merecen ser fuzzeadas. El fuzzing es más
efectivo en funciones que:

```
┌──────────────────────────────────────────────────────────────────────┐
│            Funciones ideales para fuzzear                            │
│                                                                      │
│  1. PARSERS                                                          │
│     Funciones que toman bytes/strings y extraen estructura.          │
│     • parse_json(data, len) → JsonValue*                            │
│     • parse_xml(data, len) → XmlNode*                               │
│     • parse_http_request(data, len) → HttpRequest*                  │
│     • parse_certificate(data, len) → X509*                          │
│                                                                      │
│  2. DECODIFICADORES                                                  │
│     Funciones que transforman un formato a otro.                     │
│     • base64_decode(in, out)                                        │
│     • decompress_lz4(in, in_len, out, out_len)                      │
│     • image_decode_png(data, len) → Pixels*                         │
│                                                                      │
│  3. VALIDADORES                                                      │
│     Funciones que verifican si un input es válido.                   │
│     • validate_email(str) → bool                                    │
│     • verify_signature(data, sig) → bool                            │
│     • check_utf8(data, len) → bool                                  │
│                                                                      │
│  4. TRANSFORMADORES                                                  │
│     Funciones que procesan datos y producen output.                  │
│     • regex_match(pattern, input) → Match*                          │
│     • sort(array, len, cmp)                                          │
│     • encrypt(key, data, len) → Ciphertext*                         │
│                                                                      │
│  Propiedad común: aceptan bytes/strings y los PROCESAN.              │
│  Si una función no procesa su input (solo lo almacena), fuzzearlo    │
│  no tiene mucho sentido.                                             │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 3. Elegir el punto de entrada correcto

### El principio de profundidad

```
┌──────────────────────────────────────────────────────────────────────┐
│         Elegir el punto de entrada: profundidad vs amplitud          │
│                                                                      │
│  main() → parse_args() → read_file() → parse_data() → validate()   │
│    ▲                                       ▲             ▲          │
│    │                                       │             │          │
│    Fuzzear aquí:                    Fuzzear aquí:   Fuzzear aquí:   │
│    MUY AMPLIO                      IDEAL            MUY ESTRECHO    │
│    • Incluye I/O de archivos       • Directamente   • Solo verifica │
│    • Incluye CLI parsing             la función       sintaxis      │
│    • Lento (~5ms/exec)               de parsing     • Pierde bugs   │
│    • Poco del input llega          • Rápido           en parsing    │
│      al parser real                  (~50μs/exec)                   │
│                                    • Todo el input                   │
│                                      se procesa                     │
│                                                                      │
│  REGLA: fuzzear la función más profunda que aún acepta              │
│         bytes/strings como input directo.                            │
│                                                                      │
│  Si la función toma un FILE*, un fd, o un socket, necesitas         │
│  un wrapper que traduzca bytes → ese tipo de I/O.                   │
└──────────────────────────────────────────────────────────────────────┘
```

### Tabla de decisión

| API de la función | Punto de entrada ideal | Patrón de harness |
|---|---|---|
| `parse(const char *data, size_t len)` | Directo | Patrón 1 (bytes crudos) |
| `parse(const char *str)` | Directo con null-term | Patrón 2 (string) |
| `parse(FILE *fp)` | Wrapper con fmemopen | Patrón 4 (archivo temporal) |
| `parse(int fd)` | Wrapper con memfd/pipe | Patrón 4 |
| `parse(struct Config *cfg)` | Construir struct desde bytes | Patrón 3 (estructura) |
| `init() → process() → cleanup()` | Wrapper stateful | Patrón 6 (secuencial) |
| `encrypt(key, data) / decrypt(key, data)` | Roundtrip | Patrón 7 |

---

## 4. Patrones de harness según tipo de input

Existen patrones recurrentes para escribir harnesses. Elegir el correcto
desde el inicio ahorra horas de debugging.

```
┌──────────────────────────────────────────────────────────────────────┐
│              Los 8 patrones de harness                                │
│                                                                      │
│  #1 Bytes crudos         → parse(data, len)                         │
│  #2 String null-term     → parse(str)                               │
│  #3 Estructura binaria   → parse(&struct)                           │
│  #4 Archivo temporal     → parse(FILE*)                             │
│  #5 Múltiples params     → func(a, b, c, flags)                    │
│  #6 Secuencial stateful  → init→op1→op2→cleanup                    │
│  #7 Roundtrip            → decode(encode(x)) == x                  │
│  #8 Differential         → implA(x) == implB(x)                    │
│                                                                      │
│  Cada patrón resuelve un problema diferente de "adaptación"          │
│  entre los bytes del fuzzer y la API del target.                    │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 5. Patrón 1: bytes crudos → función

El patrón más simple. La función target acepta bytes directamente.

```c
/* Función target */
int parse_packet(const uint8_t *data, size_t len);

/* Harness — trivial */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    parse_packet(data, size);
    return 0;
}
```

### Cuándo usarlo

- La función ya acepta `(const uint8_t *data, size_t len)` o similar
- Parsers binarios: protocolos de red, formatos de archivo, codecs
- La función no necesita null-termination

### Consideraciones

```c
/* Versión mejorada con límite de tamaño */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* Rechazar inputs absurdamente grandes */
    if (size > 1024 * 64) return 0;  /* 64KB max */

    /* Rechazar inputs vacíos si el parser no los maneja */
    if (size == 0) return 0;

    ParseResult *result = parse_packet(data, size);
    if (result) {
        /* Ejercitar más código: acceder a los campos parseados */
        (void)result->type;
        (void)result->payload_len;
        parse_result_free(result);
    }
    return 0;
}
```

---

## 6. Patrón 2: bytes → string null-terminated

Muchas funciones C esperan strings terminados en null (`\0`). Los bytes del fuzzer
NO están null-terminated, así que el harness debe agregar el `\0`.

```c
/* Función target */
int parse_config(const char *config_str);

/* Harness — agregar null terminator */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* Crear copia null-terminated */
    char *str = (char *)malloc(size + 1);
    if (!str) return 0;
    memcpy(str, data, size);
    str[size] = '\0';

    parse_config(str);

    free(str);
    return 0;
}
```

### Variante: usar stacks para evitar malloc

```c
/* Para inputs pequeños, un buffer en stack es más rápido */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 4095) return 0;  /* limitar para caber en stack */

    char str[4096];
    memcpy(str, data, size);
    str[size] = '\0';

    parse_config(str);
    return 0;
}
```

### Tratar null bytes dentro del input

```c
/* Problema: si data contiene \0 internos, strlen(str) < size.
   El parser podría no procesar todos los bytes. */

/* Solución 1: reemplazar nulls por otro carácter */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    char *str = (char *)malloc(size + 1);
    if (!str) return 0;
    memcpy(str, data, size);
    /* Reemplazar nulls internos por espacios */
    for (size_t i = 0; i < size; i++) {
        if (str[i] == '\0') str[i] = ' ';
    }
    str[size] = '\0';

    parse_config(str);
    free(str);
    return 0;
}

/* Solución 2: pasar el tamaño explícitamente si la API lo soporta */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    parse_config_n((const char *)data, size);
    return 0;
}
```

---

## 7. Patrón 3: bytes → estructura binaria

Cuando la función target acepta una estructura, puedes interpretar los bytes
del fuzzer como los campos de la estructura.

```c
/* Estructura target */
typedef struct {
    uint32_t version;
    uint16_t flags;
    uint16_t count;
    uint8_t  data[256];
} PacketHeader;

int process_header(const PacketHeader *header);

/* Harness — interpretar bytes como struct */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* Necesitamos al menos sizeof(PacketHeader) bytes */
    if (size < sizeof(PacketHeader)) return 0;

    /* Interpretar los bytes como la estructura.
       NOTA: esto asume que la alineación es correcta.
       Para evitar problemas de alineación, usar memcpy: */
    PacketHeader header;
    memcpy(&header, data, sizeof(PacketHeader));

    process_header(&header);
    return 0;
}
```

### Versión con campos variables

```c
typedef struct {
    uint16_t type;
    uint16_t payload_len;
    /* payload sigue después del header */
} MessageHeader;

int process_message(const MessageHeader *header, const uint8_t *payload,
                    size_t payload_len);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < sizeof(MessageHeader)) return 0;

    MessageHeader header;
    memcpy(&header, data, sizeof(MessageHeader));

    /* El payload son los bytes restantes */
    const uint8_t *payload = data + sizeof(MessageHeader);
    size_t payload_len = size - sizeof(MessageHeader);

    /* Limitar payload_len al valor del header o al tamaño real,
       lo que sea menor (evitar leer fuera del buffer) */
    if (header.payload_len < payload_len) {
        payload_len = header.payload_len;
    }

    process_message(&header, payload, payload_len);
    return 0;
}
```

### Dividir bytes en múltiples campos con FuzzedDataProvider (concepto)

```c
/* Concepto de FuzzedDataProvider para C
   (existe nativamente en C++ con #include <fuzzer/FuzzedDataProvider.h>)
   En C lo implementamos manualmente: */

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t pos;
} FuzzInput;

void fuzz_input_init(FuzzInput *fi, const uint8_t *data, size_t size) {
    fi->data = data;
    fi->size = size;
    fi->pos = 0;
}

/* Consumir N bytes */
const uint8_t *fuzz_consume_bytes(FuzzInput *fi, size_t n) {
    if (fi->pos + n > fi->size) return NULL;
    const uint8_t *ptr = fi->data + fi->pos;
    fi->pos += n;
    return ptr;
}

/* Consumir un entero */
uint32_t fuzz_consume_uint32(FuzzInput *fi) {
    uint32_t val = 0;
    const uint8_t *bytes = fuzz_consume_bytes(fi, 4);
    if (bytes) memcpy(&val, bytes, 4);
    return val;
}

uint16_t fuzz_consume_uint16(FuzzInput *fi) {
    uint16_t val = 0;
    const uint8_t *bytes = fuzz_consume_bytes(fi, 2);
    if (bytes) memcpy(&val, bytes, 2);
    return val;
}

uint8_t fuzz_consume_uint8(FuzzInput *fi) {
    if (fi->pos >= fi->size) return 0;
    return fi->data[fi->pos++];
}

/* Consumir un bool */
int fuzz_consume_bool(FuzzInput *fi) {
    return fuzz_consume_uint8(fi) & 1;
}

/* Consumir un entero en rango [min, max] */
uint32_t fuzz_consume_uint32_in_range(FuzzInput *fi, uint32_t min, uint32_t max) {
    uint32_t val = fuzz_consume_uint32(fi);
    if (max <= min) return min;
    return min + (val % (max - min + 1));
}

/* Consumir los bytes restantes */
const uint8_t *fuzz_consume_remaining(FuzzInput *fi, size_t *out_len) {
    *out_len = fi->size - fi->pos;
    const uint8_t *ptr = fi->data + fi->pos;
    fi->pos = fi->size;
    return ptr;
}
```

### Uso del FuzzedDataProvider casero

```c
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    FuzzInput fi;
    fuzz_input_init(&fi, data, size);

    uint32_t version = fuzz_consume_uint32(&fi);
    uint16_t flags = fuzz_consume_uint16(&fi);
    int strict_mode = fuzz_consume_bool(&fi);

    size_t payload_len;
    const uint8_t *payload = fuzz_consume_remaining(&fi, &payload_len);

    if (!payload) return 0;

    Config cfg = {
        .version = version,
        .flags = flags,
        .strict = strict_mode,
    };

    process_with_config(&cfg, payload, payload_len);
    return 0;
}
```

---

## 8. Patrón 4: bytes → archivo temporal

Cuando la función target lee de un `FILE*` o `fd`, necesitas convertir los bytes
del fuzzer en un archivo que la función pueda leer.

### Opción A: fmemopen (sin disco)

```c
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

/* Función target que lee de FILE* */
int parse_document(FILE *fp);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* fmemopen: crear un FILE* que lee de un buffer en memoria */
    FILE *fp = fmemopen((void *)data, size, "rb");
    if (!fp) return 0;

    parse_document(fp);

    fclose(fp);
    return 0;
}
```

`fmemopen` es la mejor opción: no toca disco, es rápido, y no deja archivos
temporales.

### Opción B: memfd_create (Linux, sin disco)

```c
#include <sys/mman.h>
#include <unistd.h>

/* Función target que lee de un fd */
int parse_from_fd(int fd);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* memfd_create: crear un fd anónimo en memoria */
    int fd = memfd_create("fuzz_input", 0);
    if (fd < 0) return 0;

    write(fd, data, size);
    lseek(fd, 0, SEEK_SET);  /* rebobinar al inicio */

    parse_from_fd(fd);

    close(fd);
    return 0;
}
```

### Opción C: archivo temporal (último recurso)

```c
#include <stdio.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* tmpfile: crear archivo temporal (se borra al cerrar) */
    FILE *fp = tmpfile();
    if (!fp) return 0;

    fwrite(data, 1, size, fp);
    fseek(fp, 0, SEEK_SET);

    parse_document(fp);

    fclose(fp);  /* elimina el archivo automáticamente */
    return 0;
}
```

### Comparación

| Método | Disco | Velocidad | Portabilidad | Recomendado |
|---|---|---|---|---|
| `fmemopen` | No | Muy rápida | POSIX | **Sí** (para FILE*) |
| `memfd_create` | No | Muy rápida | Linux | **Sí** (para fd) |
| `tmpfile` | Sí (ramdisk) | Rápida | Universal | Aceptable |
| `mkstemp` + write | Sí (disco) | Lenta | Universal | **No** |

---

## 9. Patrón 5: bytes → múltiples parámetros

Cuando la función tiene múltiples parámetros independientes, dividir los bytes
del fuzzer entre ellos.

```c
/* Función con múltiples parámetros */
int search(const char *pattern, const char *text, int flags);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 3) return 0;

    /* Usar el primer byte como separador de los dos strings */
    /* y el segundo byte como flags */
    uint8_t flags_byte = data[0];
    int flags = 0;
    if (flags_byte & 0x01) flags |= SEARCH_CASE_INSENSITIVE;
    if (flags_byte & 0x02) flags |= SEARCH_MULTILINE;
    if (flags_byte & 0x04) flags |= SEARCH_DOTALL;

    /* Buscar un separador en los bytes restantes */
    const uint8_t *rest = data + 1;
    size_t rest_len = size - 1;

    /* Dividir: los primeros N bytes son el pattern, el resto es text */
    size_t split_point = 0;
    for (size_t i = 0; i < rest_len; i++) {
        if (rest[i] == 0xFF) {  /* usar 0xFF como separador */
            split_point = i;
            break;
        }
    }
    if (split_point == 0) split_point = rest_len / 2;

    /* Crear strings null-terminated */
    char *pattern_str = malloc(split_point + 1);
    char *text_str = malloc(rest_len - split_point);
    if (!pattern_str || !text_str) {
        free(pattern_str);
        free(text_str);
        return 0;
    }

    memcpy(pattern_str, rest, split_point);
    pattern_str[split_point] = '\0';

    size_t text_start = (split_point < rest_len) ? split_point + 1 : split_point;
    size_t text_len = rest_len - text_start;
    memcpy(text_str, rest + text_start, text_len);
    text_str[text_len] = '\0';

    search(pattern_str, text_str, flags);

    free(pattern_str);
    free(text_str);
    return 0;
}
```

### Versión más limpia con FuzzInput

```c
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    FuzzInput fi;
    fuzz_input_init(&fi, data, size);

    /* Consumir flags como un byte */
    int flags = 0;
    uint8_t flags_byte = fuzz_consume_uint8(&fi);
    if (flags_byte & 0x01) flags |= SEARCH_CASE_INSENSITIVE;
    if (flags_byte & 0x02) flags |= SEARCH_MULTILINE;

    /* Consumir la longitud del pattern (1 byte = max 255) */
    uint8_t pattern_len = fuzz_consume_uint8(&fi);

    /* Consumir el pattern */
    const uint8_t *pattern_bytes = fuzz_consume_bytes(&fi, pattern_len);
    if (!pattern_bytes) return 0;

    /* El texto son los bytes restantes */
    size_t text_len;
    const uint8_t *text_bytes = fuzz_consume_remaining(&fi, &text_len);

    /* Crear strings */
    char *pattern = strndup((const char *)pattern_bytes, pattern_len);
    char *text = strndup((const char *)text_bytes, text_len);
    if (!pattern || !text) { free(pattern); free(text); return 0; }

    search(pattern, text, flags);

    free(pattern);
    free(text);
    return 0;
}
```

---

## 10. Patrón 6: bytes → operaciones secuenciales (stateful)

Para APIs con estado (create → operate → destroy), el harness interpreta
los bytes del fuzzer como una secuencia de operaciones.

```c
/* API stateful */
typedef struct HashTable HashTable;
HashTable *ht_create(size_t initial_capacity);
int ht_insert(HashTable *ht, const char *key, const char *value);
const char *ht_get(HashTable *ht, const char *key);
int ht_delete(HashTable *ht, const char *key);
void ht_destroy(HashTable *ht);

/* Operaciones posibles */
enum {
    OP_INSERT = 0,
    OP_GET    = 1,
    OP_DELETE = 2,
};

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2) return 0;

    FuzzInput fi;
    fuzz_input_init(&fi, data, size);

    /* Crear el hash table con capacidad variable */
    uint8_t cap = fuzz_consume_uint8(&fi);
    size_t capacity = (cap % 64) + 1;  /* 1-64 */
    HashTable *ht = ht_create(capacity);
    if (!ht) return 0;

    /* Interpretar los bytes restantes como operaciones */
    while (fi.pos < fi.size) {
        uint8_t op = fuzz_consume_uint8(&fi) % 3;  /* 0, 1, o 2 */

        /* Consumir un "key" (hasta 16 bytes) */
        uint8_t key_len = fuzz_consume_uint8(&fi) % 16 + 1;
        const uint8_t *key_bytes = fuzz_consume_bytes(&fi, key_len);
        if (!key_bytes) break;

        /* Crear key null-terminated */
        char key[17];
        memcpy(key, key_bytes, key_len);
        key[key_len] = '\0';

        switch (op) {
        case OP_INSERT: {
            uint8_t val_len = fuzz_consume_uint8(&fi) % 32 + 1;
            const uint8_t *val_bytes = fuzz_consume_bytes(&fi, val_len);
            if (!val_bytes) goto cleanup;
            char value[33];
            memcpy(value, val_bytes, val_len);
            value[val_len] = '\0';
            ht_insert(ht, key, value);
            break;
        }
        case OP_GET:
            ht_get(ht, key);
            break;
        case OP_DELETE:
            ht_delete(ht, key);
            break;
        }
    }

cleanup:
    ht_destroy(ht);
    return 0;
}
```

### Ventajas del patrón stateful

- Ejerce secuencias de operaciones que los unit tests rara vez cubren
- Encuentra bugs de estado (use-after-delete, double-insert, corrupción)
- El fuzzer descubre automáticamente secuencias interesantes
  (ej: insert→delete→get del mismo key)

---

## 11. Patrón 7: roundtrip (encode/decode)

El harness verifica una propiedad: `decode(encode(x)) == x`. Si no se cumple,
es un bug — el harness llama a `abort()` para que el fuzzer lo detecte.

```c
/* API de codec */
int compress(const uint8_t *in, size_t in_len,
             uint8_t *out, size_t *out_len);
int decompress(const uint8_t *in, size_t in_len,
               uint8_t *out, size_t *out_len);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0 || size > 4096) return 0;

    /* 1. Comprimir */
    size_t compressed_cap = size * 2 + 64;  /* overhead de compresión */
    uint8_t *compressed = malloc(compressed_cap);
    if (!compressed) return 0;

    size_t compressed_len = compressed_cap;
    int ret = compress(data, size, compressed, &compressed_len);
    if (ret != 0) {
        free(compressed);
        return 0;  /* la compresión falló — OK para algunos inputs */
    }

    /* 2. Descomprimir */
    size_t decompressed_cap = size + 64;
    uint8_t *decompressed = malloc(decompressed_cap);
    if (!decompressed) {
        free(compressed);
        return 0;
    }

    size_t decompressed_len = decompressed_cap;
    ret = decompress(compressed, compressed_len, decompressed, &decompressed_len);
    if (ret != 0) {
        /* La descompresión falló en datos que NOSOTROS comprimimos.
           Esto es un bug! */
        abort();
    }

    /* 3. Verificar roundtrip */
    if (decompressed_len != size || memcmp(data, decompressed, size) != 0) {
        /* Los datos no son iguales — bug en compress o decompress */
        abort();
    }

    free(compressed);
    free(decompressed);
    return 0;
}
```

### Cuándo usar roundtrip

```
┌──────────────────────────────────────────────────────────────────────┐
│         Funciones ideales para roundtrip fuzzing                     │
│                                                                      │
│  encode/decode:     base64, hex, URL encoding, UTF-8                │
│  compress/decompress: zlib, lz4, zstd, brotli                      │
│  serialize/deserialize: JSON, MessagePack, Protobuf                 │
│  encrypt/decrypt:   AES, ChaCha20 (con misma key)                   │
│  parse/format:      parse_date → format_date                        │
│  marshal/unmarshal: struct → bytes → struct                         │
│                                                                      │
│  Propiedad: f(g(x)) == x  (o g(f(x)) == x)                        │
│                                                                      │
│  VENTAJA: encuentra bugs LÓGICOS, no solo crashes.                  │
│  Un test de roundtrip con abort() convierte bugs lógicos             │
│  en "crashes" que el fuzzer detecta.                                │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 12. Patrón 8: differential (comparar implementaciones)

Compara dos implementaciones de la misma función. Si producen resultados diferentes,
es un bug en al menos una de ellas.

```c
/* Dos implementaciones del mismo algoritmo */
int sort_v1(int *array, size_t len);   /* implementación original */
int sort_v2(int *array, size_t len);   /* implementación optimizada */

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < sizeof(int) || size > 1024) return 0;

    size_t count = size / sizeof(int);

    /* Crear dos copias del array */
    int *arr1 = malloc(count * sizeof(int));
    int *arr2 = malloc(count * sizeof(int));
    if (!arr1 || !arr2) { free(arr1); free(arr2); return 0; }

    memcpy(arr1, data, count * sizeof(int));
    memcpy(arr2, data, count * sizeof(int));

    /* Ejecutar ambas implementaciones */
    sort_v1(arr1, count);
    sort_v2(arr2, count);

    /* Comparar resultados */
    if (memcmp(arr1, arr2, count * sizeof(int)) != 0) {
        /* Las implementaciones difieren — bug en una de ellas */
        abort();
    }

    free(arr1);
    free(arr2);
    return 0;
}
```

### Casos de uso

```
┌──────────────────────────────────────────────────────────────────────┐
│         Cuándo usar differential fuzzing                             │
│                                                                      │
│  • Comparar implementación nueva vs vieja después de un refactor    │
│  • Comparar tu implementación vs una de referencia                  │
│    (ej: tu parser JSON vs jq)                                       │
│  • Comparar versión C vs versión assembly/SIMD                      │
│  • Verificar que optimizaciones no cambian el resultado             │
│  • Fuzzing de compiladores: gcc vs clang producen mismo output      │
│                                                                      │
│  VENTAJA: no necesitas saber la respuesta correcta.                 │
│  Solo necesitas que dos implementaciones coincidan.                  │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 13. Limitar el input: tamaño y complejidad

### Por qué limitar

```
┌──────────────────────────────────────────────────────────────────────┐
│         Por qué limitar el tamaño del input                          │
│                                                                      │
│  Razón 1: VELOCIDAD                                                 │
│  Un parser de XML con 1MB de input tarda 50ms.                      │
│  Con 1KB: tarda 0.05ms.                                              │
│  50ms × 10K exec = 500 segundos para 10K inputs.                   │
│  0.05ms × 10K exec = 0.5 segundos.                                  │
│  1000x más rápido = 1000x más inputs explorados.                    │
│                                                                      │
│  Razón 2: COBERTURA                                                  │
│  La mayoría de los bugs están en los primeros KB del input.         │
│  Bytes adicionales rara vez descubren caminos nuevos.               │
│  El fuzzer es más eficiente con inputs pequeños.                    │
│                                                                      │
│  Razón 3: MEMORIA                                                   │
│  Inputs grandes causan allocaciones grandes.                         │
│  Con ASan, cada allocación tiene overhead.                          │
│  Un corpus de 10K archivos de 1MB = 10GB en disco.                  │
│                                                                      │
│  Razón 4: REPRODUCIBILIDAD                                          │
│  Un crash con 50 bytes es fácil de analizar.                        │
│  Un crash con 50KB requiere minimización extensiva.                  │
└──────────────────────────────────────────────────────────────────────┘
```

### Cómo limitar

```c
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* Límite absoluto — rechazar inputs demasiado grandes */
    if (size > 4096) return 0;

    /* Límite mínimo — algunos parsers necesitan al menos N bytes */
    if (size < 4) return 0;

    process(data, size);
    return 0;
}
```

### Tabla de límites recomendados

| Tipo de target | Max recomendado | Razón |
|---|---|---|
| Parser de texto (JSON, XML, INI) | 4KB-16KB | La mayoría de bugs en estructuras pequeñas |
| Parser binario (protocolos) | 1KB-4KB | Los headers son cortos |
| Codec (compress/encode) | 4KB-64KB | Necesita más datos para ejercitar |
| Image decoder | 16KB-256KB | Las imágenes válidas tienen cierto tamaño mínimo |
| Regex/pattern matching | 256B-1KB | Patterns complejos son cortos |
| Cryptographic functions | 256B-4KB | Keys y bloques son pequeños |

### libFuzzer: -max_len

```bash
# Limitar el tamaño máximo de inputs generados
./fuzz_target corpus/ -max_len=4096

# libFuzzer empieza con inputs pequeños y crece gradualmente.
# -max_len limita el crecimiento.
```

---

## 14. Limitar recursos: timeout

### En el harness (software)

```c
#include <signal.h>
#include <setjmp.h>

static jmp_buf timeout_jmp;

static void timeout_handler(int sig) {
    longjmp(timeout_jmp, 1);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* NOTA: este patrón tiene limitaciones (no es signal-safe en general).
       Es mejor usar el timeout del fuzzer (-timeout) que hacer esto
       manualmente. Solo se muestra como concepto. */

    /* El método preferido: confiar en el timeout del fuzzer */
    process(data, size);
    return 0;
}
```

### En el fuzzer (recomendado)

```bash
# libFuzzer: timeout en segundos
./fuzz_target corpus/ -timeout=5

# AFL++: timeout en milisegundos
afl-fuzz -t 5000 -i corpus -o output -- ./target @@
```

### Qué timeout usar

| Target | Timeout recomendado | Razón |
|---|---|---|
| Parser simple | 1 segundo | Debe ser instantáneo |
| Parser complejo | 5 segundos | Puede tener procesamiento costoso |
| Codec | 5-10 segundos | La compresión/descompresión puede ser lenta |
| Regex | 2 segundos | ReDoS puede causar exponential backtracking |
| Crypto | 10 segundos | Algunas operaciones son lentas por diseño |

### Detectar algorithmic complexity bugs

```
  Un timeout NO siempre es un bug de loop infinito.
  Puede ser un bug de COMPLEJIDAD ALGORÍTMICA:

  Ejemplo: regex "(a+)+$" con input "aaaaaaaaaaaaaaaa!"
  Tiempo: O(2^n) donde n = número de 'a's.
  Con 30 'a's: ~1 segundo.
  Con 40 'a's: ~17 minutos.
  Con 50 'a's: ~12 días.

  Esto es un bug REAL (ReDoS) que causa DoS en producción.
  El fuzzer lo detecta como timeout.
```

---

## 15. Limitar recursos: memoria

### En el harness

```c
#include <sys/resource.h>

/* Limitar la memoria del proceso */
static void limit_memory(size_t max_mb) {
    struct rlimit rl;
    rl.rlim_cur = max_mb * 1024 * 1024;
    rl.rlim_max = max_mb * 1024 * 1024;
    setrlimit(RLIMIT_AS, &rl);
}

int LLVMFuzzerInitialize(int *argc, char ***argv) {
    limit_memory(256);  /* 256 MB máximo */
    return 0;
}
```

### En el fuzzer (recomendado)

```bash
# libFuzzer: límite RSS en MB
./fuzz_target corpus/ -rss_limit_mb=512

# AFL++: límite de memoria en MB
afl-fuzz -m 256 -i corpus -o output -- ./target @@

# AFL++ con ASan (necesita más memoria)
afl-fuzz -m none -i corpus -o output -- ./target_asan @@
```

### Detectar memory bomb inputs

```c
/* Ejemplo: descompresión de "zip bomb"
   Un archivo comprimido de 1KB que descomprime a 1GB.

   Si el decompressor no limita el output, consume toda la RAM.
   El fuzzer lo detecta como OOM. */

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 4096) return 0;

    /* Limitar el tamaño de output del decompressor */
    size_t max_output = 1024 * 1024;  /* 1MB máximo de output */
    uint8_t *output = malloc(max_output);
    if (!output) return 0;

    size_t output_len = max_output;
    decompress(data, size, output, &output_len);

    free(output);
    return 0;
}
```

---

## 16. Limitar recursos: profundidad de recursión

Los parsers recursivos (JSON, XML, expresiones) pueden causar stack overflow
con inputs profundamente anidados.

### El problema

```c
/* Parser recursivo sin límite */
JsonValue *parse_value(Parser *p) {
    if (peek(p) == '[') {
        advance(p);
        JsonValue *arr = make_array();
        while (peek(p) != ']') {
            JsonValue *elem = parse_value(p);  /* ← recursión sin límite */
            array_push(arr, elem);
            if (peek(p) == ',') advance(p);
        }
        advance(p);
        return arr;
    }
    /* ... */
}

/* Input malicioso: "[[[[[[[[[[[[" (miles de [ sin ] )
   → stack overflow → SIGSEGV */
```

### Solución: contador de profundidad

```c
/* Agregar profundidad máxima al parser */
#define MAX_DEPTH 64

typedef struct {
    const char *input;
    int pos;
    int len;
    int depth;  /* ← NUEVO: contador de profundidad */
} Parser;

JsonValue *parse_value(Parser *p) {
    /* Verificar profundidad ANTES de recursar */
    if (p->depth >= MAX_DEPTH) {
        return NULL;  /* rechazar input demasiado profundo */
    }
    p->depth++;

    JsonValue *result = NULL;

    if (peek(p) == '[') {
        advance(p);
        result = make_array();
        while (peek(p) != ']') {
            JsonValue *elem = parse_value(p);  /* recursión controlada */
            if (!elem) { json_free(result); p->depth--; return NULL; }
            array_push(result, elem);
            if (peek(p) == ',') advance(p);
        }
        advance(p);
    }
    /* ... */

    p->depth--;
    return result;
}
```

### En el harness (alternativa)

```c
#include <sys/resource.h>

/* Limitar el tamaño del stack a 1MB (detecta overflow antes del SIGSEGV) */
int LLVMFuzzerInitialize(int *argc, char ***argv) {
    struct rlimit rl;
    rl.rlim_cur = 1024 * 1024;  /* 1MB stack */
    rl.rlim_max = 1024 * 1024;
    setrlimit(RLIMIT_STACK, &rl);
    return 0;
}
```

**Nota**: limitar el stack es un workaround. La solución correcta es agregar
límite de profundidad en el parser. Si no puedes modificar el parser, el
límite de stack convierte el overflow en un crash más predecible.

---

## 17. Inicialización del harness

### Para libFuzzer

```c
/* LLVMFuzzerInitialize — se ejecuta UNA vez */
static DatabaseSchema *schema;
static RegexEngine *engine;

int LLVMFuzzerInitialize(int *argc, char ***argv) {
    /* Setup costoso: una vez */
    schema = load_schema("schema.json");
    engine = regex_engine_create();

    /* Configurar límites de recursos */
    struct rlimit rl = { .rlim_cur = 256 * 1024 * 1024,
                          .rlim_max = 256 * 1024 * 1024 };
    setrlimit(RLIMIT_AS, &rl);

    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* Usar schema y engine sin reinicializarlos */
    validate_against_schema(data, size, schema);
    regex_match_all(engine, (const char *)data, size);
    return 0;
}
```

### Para AFL++ (deferred fork server)

```c
static DatabaseSchema *schema;
static RegexEngine *engine;

int main(int argc, char *argv[]) {
    /* Setup costoso: antes del fork */
    schema = load_schema("schema.json");
    engine = regex_engine_create();

    __AFL_INIT();  /* Fork aquí — lo de arriba se ejecuta una vez */

    /* Leer input */
    if (argc < 2) return 1;
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 1;

    char buf[65536];
    size_t len = fread(buf, 1, sizeof(buf), f);
    fclose(f);

    validate_against_schema((uint8_t *)buf, len, schema);
    regex_match_all(engine, buf, len);

    return 0;
}
```

### Qué poner en la inicialización

| Acción | ¿En Initialize? | Razón |
|---|---|---|
| Cargar archivo de config | Sí | Solo se lee una vez |
| Compilar regex | Sí | Compilar es costoso |
| Crear pools de memoria | Sí | Amortizar creación |
| Abrir base de datos (read-only) | Sí | Una conexión, muchas queries |
| Setear limits (rlimit) | Sí | Configuración global |
| Allocar buffer de trabajo fijo | Sí | Si el tamaño no cambia |
| Inicializar RNG | **No** | Introduce no-determinismo |
| Crear threads | **No** | No determinista |
| Abrir sockets | **No** | Side effects de red |

---

## 18. Manejo de estado global

### El problema del state leakage

```c
/* PELIGROSO: variable global que persiste entre llamadas */
static int total_parsed = 0;
static char last_error[256] = "";

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    total_parsed++;  /* ← state leakage */

    Result *r = parse(data, size);
    if (!r) {
        /* last_error aún tiene el error de la llamada ANTERIOR */
        snprintf(last_error, sizeof(last_error), "error #%d", total_parsed);
    } else {
        result_free(r);
    }
    return 0;
}
```

### Soluciones

```c
/* Solución 1: No usar estado global en el harness */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* Sin variables globales mutables */
    Result *r = parse(data, size);
    if (r) result_free(r);
    return 0;
}

/* Solución 2: Resetear estado al inicio de cada llamada */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* Si el target NECESITA estado global, resetearlo */
    parser_reset_global_state();  /* función del target */

    Result *r = parse(data, size);
    if (r) result_free(r);
    return 0;
}

/* Solución 3: Crear/destruir el contexto en cada llamada */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    ParserContext *ctx = parser_context_create();
    if (!ctx) return 0;

    parser_parse(ctx, data, size);

    parser_context_destroy(ctx);  /* limpia todo el estado */
    return 0;
}
```

### Detectar state leakage

```bash
# Ejecutar el fuzzer con -print_coverage=1 y buscar
# inconsistencias en la cobertura para el mismo input

# O usar el stability metric de AFL++ (< 90% = state leakage)
```

---

## 19. Harness para APIs con múltiples funciones

### Fuzzear una API completa (no solo una función)

```c
/* API de un hash table */
Map *map_create(size_t cap);
void map_set(Map *m, const char *key, const char *val);
const char *map_get(Map *m, const char *key);
int map_has(Map *m, const char *key);
void map_delete(Map *m, const char *key);
size_t map_size(Map *m);
void map_clear(Map *m);
void map_destroy(Map *m);

/* Harness que ejerce TODAS las funciones */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    FuzzInput fi;
    fuzz_input_init(&fi, data, size);

    uint8_t cap = fuzz_consume_uint8(&fi);
    Map *m = map_create((cap % 128) + 1);
    if (!m) return 0;

    while (fi.pos < fi.size) {
        uint8_t op = fuzz_consume_uint8(&fi) % 6;

        /* Consumir key (max 31 chars) */
        uint8_t klen = (fuzz_consume_uint8(&fi) % 31) + 1;
        const uint8_t *kbytes = fuzz_consume_bytes(&fi, klen);
        if (!kbytes) break;
        char key[32];
        memcpy(key, kbytes, klen);
        key[klen] = '\0';

        switch (op) {
        case 0: { /* SET */
            uint8_t vlen = (fuzz_consume_uint8(&fi) % 63) + 1;
            const uint8_t *vbytes = fuzz_consume_bytes(&fi, vlen);
            if (!vbytes) goto done;
            char val[64];
            memcpy(val, vbytes, vlen);
            val[vlen] = '\0';
            map_set(m, key, val);
            break;
        }
        case 1: /* GET */
            map_get(m, key);
            break;
        case 2: /* HAS */
            map_has(m, key);
            break;
        case 3: /* DELETE */
            map_delete(m, key);
            break;
        case 4: /* SIZE */
            map_size(m);
            break;
        case 5: /* CLEAR */
            map_clear(m);
            break;
        }
    }

done:
    map_destroy(m);
    return 0;
}
```

### Agregar invariantes como assertions

```c
/* Versión con invariantes — encuentra bugs lógicos */
switch (op) {
case 0: { /* SET */
    /* ... */
    map_set(m, key, val);

    /* INVARIANTE: después de set, get debe retornar el valor */
    const char *got = map_get(m, key);
    if (!got || strcmp(got, val) != 0) {
        abort();  /* bug: set no persistió el valor */
    }

    /* INVARIANTE: después de set, has debe retornar true */
    if (!map_has(m, key)) {
        abort();  /* bug: has no encuentra el key que acabamos de setear */
    }
    break;
}
case 3: /* DELETE */
    map_delete(m, key);

    /* INVARIANTE: después de delete, has debe retornar false */
    if (map_has(m, key)) {
        abort();  /* bug: key todavía existe después de delete */
    }
    break;
}
```

---

## 20. Harness para parsers de formatos complejos

### Estrategia: múltiples harnesses por parser

```
┌──────────────────────────────────────────────────────────────────────┐
│         Múltiples harnesses para un parser complejo                  │
│                                                                      │
│  Parser XML:                                                        │
│                                                                      │
│  Harness 1: fuzz_xml_parse.c                                       │
│  → Solo parsear, verificar que no crashea                           │
│  → Encuentra buffer overflows, null derefs                          │
│                                                                      │
│  Harness 2: fuzz_xml_validate.c                                     │
│  → Parsear + validar contra DTD/Schema                              │
│  → Encuentra bugs en el validador                                   │
│                                                                      │
│  Harness 3: fuzz_xml_roundtrip.c                                    │
│  → parse → serialize → parse → comparar                            │
│  → Encuentra bugs de serialización                                  │
│                                                                      │
│  Harness 4: fuzz_xml_xpath.c                                        │
│  → parse → evaluar XPath expression                                 │
│  → Encuentra bugs en el motor XPath                                 │
│                                                                      │
│  Harness 5: fuzz_xml_transform.c                                    │
│  → parse → aplicar XSLT → verificar output                         │
│  → Encuentra bugs en el transformador                               │
│                                                                      │
│  Cada harness tiene su propio corpus y diccionario.                  │
│  Múltiples harnesses = mayor superficie de ataque cubierta.         │
└──────────────────────────────────────────────────────────────────────┘
```

### Ejemplo: parser de imágenes

```c
/* fuzz_image_decode.c — solo decodificar */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    Image *img = image_decode(data, size);
    if (img) image_free(img);
    return 0;
}

/* fuzz_image_resize.c — decodificar + resize */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4) return 0;

    FuzzInput fi;
    fuzz_input_init(&fi, data, size);
    uint16_t target_w = fuzz_consume_uint16(&fi) % 2048 + 1;
    uint16_t target_h = fuzz_consume_uint16(&fi) % 2048 + 1;

    size_t img_len;
    const uint8_t *img_data = fuzz_consume_remaining(&fi, &img_len);

    Image *img = image_decode(img_data, img_len);
    if (img) {
        Image *resized = image_resize(img, target_w, target_h);
        if (resized) image_free(resized);
        image_free(img);
    }
    return 0;
}

/* fuzz_image_roundtrip.c — decode → encode → decode → comparar */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    Image *img1 = image_decode(data, size);
    if (!img1) return 0;

    size_t encoded_len;
    uint8_t *encoded = image_encode_png(img1, &encoded_len);
    if (!encoded) { image_free(img1); return 0; }

    Image *img2 = image_decode(encoded, encoded_len);
    if (!img2) {
        /* Nuestro propio output no se puede decodificar — BUG */
        abort();
    }

    /* Verificar dimensiones */
    if (img1->width != img2->width || img1->height != img2->height) {
        abort();  /* Las dimensiones cambiaron — BUG */
    }

    image_free(img1);
    image_free(img2);
    free(encoded);
    return 0;
}
```

---

## 21. Harness con custom mutators para formatos con checksum

Uno de los problemas más difíciles: formatos donde cada input tiene un checksum
o hash que el parser valida. Sin un custom mutator, el fuzzer nunca genera inputs
que pasen la validación.

```c
/*
 * Formato con checksum:
 * [4 bytes: magic "PACK"]
 * [2 bytes: payload length (little-endian)]
 * [N bytes: payload]
 * [4 bytes: CRC32 del payload]
 */

/* CRC32 simple */
static uint32_t crc32_simple(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

/* Custom mutator que mantiene el formato válido */
size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size,
                                size_t max_size, unsigned int seed) {
    /* El formato mínimo es: 4 (magic) + 2 (len) + 0 (payload) + 4 (crc) = 10 */
    if (max_size < 10) return 0;

    /* Si el input actual es muy pequeño, crear uno nuevo */
    if (size < 10) {
        /* Crear un paquete mínimo */
        memcpy(data, "PACK", 4);
        data[4] = 0;  /* payload len = 0 */
        data[5] = 0;
        uint32_t crc = crc32_simple(NULL, 0);
        memcpy(data + 6, &crc, 4);
        return 10;
    }

    /* Extraer el payload actual */
    uint16_t payload_len;
    memcpy(&payload_len, data + 4, 2);

    /* Limitar payload_len al tamaño real */
    if (payload_len + 10 > size) {
        payload_len = (size > 10) ? size - 10 : 0;
    }

    /* Mutar SOLO el payload (no el magic ni el CRC) */
    if (payload_len > 0) {
        size_t new_payload_len = LLVMFuzzerMutate(
            data + 6, payload_len, max_size - 10);

        /* Actualizar el campo de longitud */
        uint16_t new_len = (uint16_t)new_payload_len;
        memcpy(data + 4, &new_len, 2);

        /* Recalcular el CRC */
        uint32_t crc = crc32_simple(data + 6, new_payload_len);
        memcpy(data + 6 + new_payload_len, &crc, 4);

        return 6 + new_payload_len + 4;
    }

    /* Si no hay payload, insertar uno aleatorio */
    size_t new_payload = LLVMFuzzerMutate(data + 6, 0, max_size - 10);
    uint16_t new_len = (uint16_t)new_payload;
    memcpy(data + 4, &new_len, 2);
    uint32_t crc = crc32_simple(data + 6, new_payload);
    memcpy(data + 6 + new_payload, &crc, 4);
    return 6 + new_payload + 4;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 65536) return 0;
    process_packet(data, size);
    return 0;
}
```

---

## 22. Harness para librerías de red (sin red real)

Fuzzear código de red sin abrir sockets reales. La clave es reemplazar la
capa de I/O con un buffer en memoria.

### Técnica: fmemopen para socket→FILE*

```c
/* El server HTTP lee de un FILE* */
HttpRequest *http_parse_request(FILE *stream);

/* Harness: fingir que los bytes del fuzzer son un socket */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 65536) return 0;

    FILE *fake_socket = fmemopen((void *)data, size, "rb");
    if (!fake_socket) return 0;

    HttpRequest *req = http_parse_request(fake_socket);
    if (req) {
        http_request_free(req);
    }

    fclose(fake_socket);
    return 0;
}
```

### Técnica: mockear send/recv

```c
/* Si la librería usa recv() directamente, podemos interceptarla */

/* Buffer global con los datos del fuzzer */
static const uint8_t *g_fuzz_data;
static size_t g_fuzz_size;
static size_t g_fuzz_pos;

/* "Mock" de recv que lee del buffer del fuzzer */
ssize_t mock_recv(int sockfd, void *buf, size_t len, int flags) {
    (void)sockfd;
    (void)flags;

    if (g_fuzz_pos >= g_fuzz_size) return 0;  /* EOF */

    size_t available = g_fuzz_size - g_fuzz_pos;
    size_t to_read = (len < available) ? len : available;

    memcpy(buf, g_fuzz_data + g_fuzz_pos, to_read);
    g_fuzz_pos += to_read;
    return to_read;
}

/* Compilar con: -Drecv=mock_recv
   O usar LD_PRELOAD para interceptar en runtime */

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    g_fuzz_data = data;
    g_fuzz_size = size;
    g_fuzz_pos = 0;

    /* La librería llama a recv() internamente,
       que ahora lee de nuestro buffer */
    Connection conn = { .fd = -1 };  /* fd fake */
    process_connection(&conn);

    return 0;
}
```

### Técnica: wrapper de la API de la librería

```c
/* Si la librería tiene una API limpia que separa I/O de lógica: */

/* Mejor: la librería acepta un callback de lectura */
typedef ssize_t (*ReadFunc)(void *ctx, void *buf, size_t len);

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t pos;
} BufferReader;

ssize_t buffer_read(void *ctx, void *buf, size_t len) {
    BufferReader *br = (BufferReader *)ctx;
    if (br->pos >= br->size) return 0;
    size_t available = br->size - br->pos;
    size_t to_read = (len < available) ? len : available;
    memcpy(buf, br->data + br->pos, to_read);
    br->pos += to_read;
    return to_read;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    BufferReader reader = { .data = data, .size = size, .pos = 0 };
    Protocol *proto = protocol_create(buffer_read, &reader);
    protocol_process(proto);
    protocol_destroy(proto);
    return 0;
}
```

---

## 23. Anti-patrones: harnesses que matan la eficiencia

```
┌──────────────────────────────────────────────────────────────────────┐
│         Anti-patrones de harness                                     │
│                                                                      │
│  ❌ #1: HARNESS DEMASIADO SUPERFICIAL                                │
│  int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {     │
│      if (size > 0) printf("got %zu bytes\n", size);                  │
│      return 0;                                                       │
│  }                                                                   │
│  Problema: no llama a ninguna función del target.                    │
│                                                                      │
│  ❌ #2: HARNESS CON I/O DE DISCO                                    │
│  int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {     │
│      FILE *f = fopen("/tmp/fuzz_input", "wb");                      │
│      fwrite(data, 1, size, f);                                      │
│      fclose(f);                                                      │
│      process_file("/tmp/fuzz_input");                                │
│      unlink("/tmp/fuzz_input");                                      │
│      return 0;                                                       │
│  }                                                                   │
│  Problema: I/O de disco es 1000x más lento que memoria.             │
│  Solución: usar fmemopen() o memfd_create().                        │
│                                                                      │
│  ❌ #3: HARNESS CON PRINTF                                          │
│  int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {     │
│      Result *r = parse(data, size);                                  │
│      if (r) {                                                        │
│          printf("Parsed: type=%d, len=%zu\n", r->type, r->len);     │
│          result_free(r);                                             │
│      } else {                                                        │
│          printf("Parse failed for %zu bytes\n", size);               │
│      }                                                               │
│      return 0;                                                       │
│  }                                                                   │
│  Problema: printf es lento y abruma la terminal.                    │
│  Solución: eliminar TODOS los printf del harness.                   │
│                                                                      │
│  ❌ #4: HARNESS QUE IGNORA LA MAYORÍA DEL INPUT                    │
│  int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {     │
│      if (size != 42) return 0;  /* solo acepta inputs de 42 bytes */│
│      parse(data, size);                                              │
│      return 0;                                                       │
│  }                                                                   │
│  Problema: el 99.99% de los inputs son rechazados.                  │
│  Solución: aceptar rangos amplios de tamaño.                        │
│                                                                      │
│  ❌ #5: HARNESS CON MEMORY LEAK                                     │
│  int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {     │
│      Result *r = parse(data, size);                                  │
│      if (r) {                                                        │
│          /* ¡Nunca se libera! */                                    │
│          return 0;                                                   │
│      }                                                               │
│      return 0;                                                       │
│  }                                                                   │
│  Problema: con libFuzzer (in-process), la memoria se acumula.       │
│  Solución: SIEMPRE liberar lo que se alloca.                        │
│                                                                      │
│  ❌ #6: HARNESS QUE LLAMA A exit()                                  │
│  int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {     │
│      if (size == 0) exit(1);  /* mata al fuzzer */                  │
│      parse(data, size);                                              │
│      return 0;                                                       │
│  }                                                                   │
│  Problema: exit() mata al fuzzer completo.                          │
│  Solución: retornar 0 para inputs no interesantes.                  │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 24. Checklist de calidad de un harness

```
┌──────────────────────────────────────────────────────────────────────┐
│            Checklist antes de fuzzear                                │
│                                                                      │
│  CORRECTITUD                                                         │
│  □ ¿Compila sin warnings con -Wall -Wextra?                        │
│  □ ¿Retorna 0 siempre?                                              │
│  □ ¿No llama a exit(), _exit(), abort() (excepto en roundtrip)?     │
│  □ ¿Libera toda la memoria allocada?                                │
│  □ ¿Cierra todos los FILE*/fd abiertos?                             │
│  □ ¿Maneja size == 0 sin crashear?                                  │
│  □ ¿No modifica el buffer data[] del fuzzer?                        │
│                                                                      │
│  EFICIENCIA                                                          │
│  □ ¿Sin I/O de disco? (usar fmemopen/memfd)                        │
│  □ ¿Sin printf/fprintf/puts?                                        │
│  □ ¿Sin sleep/usleep?                                                │
│  □ ¿Sin red (socket/connect/send/recv)?                             │
│  □ ¿Inicialización costosa en LLVMFuzzerInitialize?                 │
│  □ ¿exec speed > 500/sec?                                          │
│                                                                      │
│  COBERTURA                                                           │
│  □ ¿Llama a la función target directamente (no via main)?          │
│  □ ¿Usa la mayor parte de los bytes del fuzzer?                     │
│  □ ¿Ejerce múltiples funciones de la API?                           │
│  □ ¿Verifica propiedades cuando es posible (roundtrip)?             │
│                                                                      │
│  ROBUSTEZ                                                            │
│  □ ¿Limita el tamaño del input? (< 64KB típico)                    │
│  □ ¿El parser target tiene límite de profundidad de recursión?      │
│  □ ¿No tiene state leakage entre llamadas?                          │
│  □ ¿Funciona tanto con libFuzzer como con AFL++?                    │
│                                                                      │
│  SANITIZERS                                                          │
│  □ ¿Se compila con -fsanitize=address,undefined?                    │
│  □ ¿Se compila con -g para debug symbols?                           │
│  □ ¿Los suppress files de sanitizers son mínimos?                   │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 25. Harness portátil: AFL++ y libFuzzer con el mismo código

Puedes escribir un solo archivo que funcione con ambos fuzzers usando
compilación condicional.

```c
/* fuzz_target.c — compatible con AFL++ y libFuzzer */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "my_parser.h"

/* La función de procesamiento es la misma para ambos */
static void do_fuzz(const uint8_t *data, size_t size) {
    if (size > 65536) return;
    if (size == 0) return;

    ParseResult *result = parse(data, size);
    if (result) {
        parse_result_free(result);
    }
}

#ifdef __AFL_FUZZ_TESTCASE_LEN
/* ======================== AFL++ PERSISTENT MODE ======================== */

__AFL_FUZZ_INIT();

int main(int argc, char *argv[]) {
    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;

    while (__AFL_LOOP(10000)) {
        int len = __AFL_FUZZ_TESTCASE_LEN;
        do_fuzz(buf, len);
    }
    return 0;
}

#elif defined(LIBFUZZER)
/* ======================== LIBFUZZER ======================== */

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    do_fuzz(data, size);
    return 0;
}

#else
/* ======================== STANDALONE (para reproducir crashes) ======================== */

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 1; }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *data = malloc(fsize);
    if (!data) { fclose(f); return 1; }

    fread(data, 1, fsize, f);
    fclose(f);

    do_fuzz(data, fsize);

    free(data);
    return 0;
}

#endif
```

### Compilar para cada fuzzer

```bash
# Para libFuzzer
clang -DLIBFUZZER -fsanitize=fuzzer,address -g -o fuzz_libfuzzer fuzz_target.c parser.c

# Para AFL++ persistent mode
afl-cc -fsanitize=address -g -o fuzz_afl fuzz_target.c parser.c

# Para reproducir crashes manualmente
clang -fsanitize=address -g -o fuzz_repro fuzz_target.c parser.c
./fuzz_repro crash_file
```

---

## 26. Comparación con Rust y Go

### Harness en Rust (cargo-fuzz)

```rust
// fuzz/fuzz_targets/fuzz_parser.rs
#![no_main]
use libfuzzer_sys::fuzz_target;
use my_crate::parse;

fuzz_target!(|data: &[u8]| {
    let _ = parse(data);
});
```

**Diferencias con C**:
- No necesitas manejar memoria (no hay malloc/free)
- No necesitas null-terminator (Rust strings conocen su longitud)
- `&[u8]` es safe — no puedes leer fuera del buffer
- Los panics son capturados automáticamente por cargo-fuzz
- Puedes usar `Arbitrary` trait para inputs estructurados

### Harness en Go (go test -fuzz)

```go
func FuzzParser(f *testing.F) {
    // Semillas
    f.Add([]byte("hello"))
    f.Add([]byte(`{"key": "value"}`))

    f.Fuzz(func(t *testing.T, data []byte) {
        result, err := Parse(data)
        if err != nil {
            return // errores esperados
        }
        // Verificar propiedades
        if result.Len() < 0 {
            t.Fatal("negative length")
        }
    })
}
```

**Diferencias con C**:
- Integrado en `go test` — no necesitas herramientas externas
- Múltiples tipos de input nativos ([]byte, string, int, etc.)
- Las semillas se declaran en el mismo código
- t.Fatal() en lugar de abort()

### Tabla comparativa de harnesses

| Aspecto | C (libFuzzer) | C (AFL++) | Rust (cargo-fuzz) | Go (go test -fuzz) |
|---|---|---|---|---|
| Función entry | `LLVMFuzzerTestOneInput` | `main` o `__AFL_LOOP` | `fuzz_target!` macro | `func FuzzXxx` |
| Input type | `uint8_t*, size_t` | archivo o stdin | `&[u8]` o `Arbitrary` | `[]byte`, `string`, `int` |
| Gestión memoria | Manual (malloc/free) | Manual | Automática (ownership) | Automática (GC) |
| Null terminator | Manual | Manual | No necesario | No necesario |
| Sanitizers | Manual (-fsanitize) | Manual (-fsanitize) | RUSTFLAGS (nightly) | `-race` flag |
| Setup | Compilar + corpus | Compilar + corpus | `cargo fuzz init` | Nada (built-in) |
| Roundtrip assert | `abort()` | `abort()` | `panic!()` | `t.Fatal()` |
| Custom mutator | `LLVMFuzzerCustomMutator` | Custom mutator API | No nativo | No nativo |
| State leakage | Posible (globals) | Aislado por fork | Posible (pero Rust ayuda) | Posible |
| Velocidad | Muy alta | Alta | Alta | Media |

---

## 27. Errores comunes

### Error 1: fuzzear la función equivocada

```
❌ Fuzzeas main() en vez de la función de parsing.
   exec speed: 50/sec (lento por I/O de archivos, CLI parsing, etc.)
   El fuzzer pasa 99% del tiempo en código que no es interesante.

✓ Identificar la función que DIRECTAMENTE procesa el input:
   parse_data(data, len)  ← fuzzear esta
   No: main(argc, argv)  ← NO fuzzear esta
```

### Error 2: no usar FuzzedDataProvider para múltiples parámetros

```
❌ Todos los bytes van a un solo parámetro.
   search(data, size);  /* pero search necesita pattern + text + flags */
   El fuzzer nunca ejercita las combinaciones de parámetros.

✓ Dividir los bytes entre los parámetros:
   FuzzInput fi;
   fuzz_input_init(&fi, data, size);
   int flags = fuzz_consume_uint8(&fi);
   // ...separar pattern y text...
```

### Error 3: harness sin limits que causa OOM/stack overflow

```
❌ El parser recursa sin límite con inputs como "(((((((((".
   Stack overflow → SIGSEGV → pero no es un bug explotable.
   El fuzzer reporta cientos de "crashes" que son todos
   el mismo stack overflow.

✓ Agregar límite de profundidad en el parser.
   O limitar -max_len a un tamaño donde la recursión no sea problemática.
   O limitar stack size con setrlimit(RLIMIT_STACK, ...).
```

### Error 4: harness con escritura a disco

```
❌ El harness escribe un archivo temporal en cada ejecución.
   exec speed: 200/sec (limitado por I/O de disco).
   Con fmemopen: 15,000/sec.

✓ Reemplazar fopen/fwrite/fclose con fmemopen() o memfd_create().
```

### Error 5: harness que no ejerce suficiente código

```
❌ El harness solo parsea, pero la librería tiene también
   validate(), transform(), serialize().
   Los bugs en validate/transform/serialize nunca se encuentran.

✓ Múltiples harnesses:
   fuzz_parse.c       → solo parse
   fuzz_validate.c    → parse + validate
   fuzz_roundtrip.c   → parse + serialize + parse + compare
   fuzz_transform.c   → parse + transform
```

### Error 6: no hacer harness portátil

```
❌ El harness solo funciona con libFuzzer.
   No puedes usar AFL++ (que tiene CmpLog) ni reproducir crashes
   sin recompilar.

✓ Usar compilación condicional (sección 25):
   #ifdef __AFL_FUZZ_TESTCASE_LEN → AFL++ persistent
   #elif defined(LIBFUZZER) → libFuzzer
   #else → standalone
```

---

## 28. Ejemplo completo: fuzzear una librería de compresión LZ77

### La librería

```c
/* lz77.h */
#ifndef LZ77_H
#define LZ77_H

#include <stddef.h>
#include <stdint.h>

/* Comprime data. out debe tener al menos in_len * 2 bytes.
   Retorna el tamaño comprimido, o -1 si error. */
int lz77_compress(const uint8_t *in, size_t in_len,
                  uint8_t *out, size_t out_cap);

/* Descomprime data. out debe tener al menos max_out_len bytes.
   Retorna el tamaño descomprimido, o -1 si error. */
int lz77_decompress(const uint8_t *in, size_t in_len,
                    uint8_t *out, size_t out_cap);

#endif
```

```c
/* lz77.c — implementación simplificada con bugs intencionales */
#include "lz77.h"
#include <string.h>

#define WINDOW_SIZE 4096
#define MIN_MATCH   3
#define MAX_MATCH   258

int lz77_compress(const uint8_t *in, size_t in_len,
                  uint8_t *out, size_t out_cap) {
    size_t in_pos = 0;
    size_t out_pos = 0;

    while (in_pos < in_len) {
        int best_offset = 0;
        int best_length = 0;

        /* Buscar la mejor coincidencia en la ventana */
        size_t search_start = (in_pos > WINDOW_SIZE) ? in_pos - WINDOW_SIZE : 0;

        for (size_t s = search_start; s < in_pos; s++) {
            int match_len = 0;
            while (match_len < MAX_MATCH &&
                   in_pos + match_len < in_len &&
                   in[s + match_len] == in[in_pos + match_len]) {
                match_len++;
            }
            if (match_len >= MIN_MATCH && match_len > best_length) {
                best_offset = in_pos - s;
                best_length = match_len;
            }
        }

        if (best_length >= MIN_MATCH) {
            /* Escribir token de referencia: [1][offset:12][length:4] = 17 bits */
            if (out_pos + 3 > out_cap) return -1;
            out[out_pos++] = 0x80 | ((best_offset >> 8) & 0x0F);
            out[out_pos++] = best_offset & 0xFF;
            out[out_pos++] = best_length - MIN_MATCH;
            in_pos += best_length;
        } else {
            /* Escribir literal: [0][byte] */
            if (out_pos + 1 > out_cap) return -1;
            out[out_pos++] = in[in_pos++];
            /* BUG: no verifica que el bit 7 del literal sea 0.
               Si el byte del input tiene bit 7 set, se confunde
               con un token de referencia durante la descompresión. */
        }
    }

    return (int)out_pos;
}

int lz77_decompress(const uint8_t *in, size_t in_len,
                    uint8_t *out, size_t out_cap) {
    size_t in_pos = 0;
    size_t out_pos = 0;

    while (in_pos < in_len) {
        if (in[in_pos] & 0x80) {
            /* Token de referencia */
            if (in_pos + 3 > in_len) return -1;

            int offset = ((in[in_pos] & 0x0F) << 8) | in[in_pos + 1];
            int length = in[in_pos + 2] + MIN_MATCH;
            in_pos += 3;

            if (offset == 0 || offset > (int)out_pos) {
                return -1;  /* referencia inválida */
            }

            /* BUG: no verifica out_pos + length <= out_cap */
            for (int i = 0; i < length; i++) {
                out[out_pos] = out[out_pos - offset];
                out_pos++;
            }
        } else {
            /* Literal */
            if (out_pos >= out_cap) return -1;
            out[out_pos++] = in[in_pos++];
        }
    }

    return (int)out_pos;
}
```

### Harness 1: fuzzear decompress

```c
/* fuzz_decompress.c — fuzzear la descompresión con datos arbitrarios */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "lz77.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 4096) return 0;

    /* Buffer de output generoso */
    size_t out_cap = 1024 * 64;  /* 64KB max output */
    uint8_t *out = malloc(out_cap);
    if (!out) return 0;

    lz77_decompress(data, size, out, out_cap);

    free(out);
    return 0;
}
```

### Harness 2: roundtrip

```c
/* fuzz_roundtrip.c — compress(x) → decompress → x' == x? */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "lz77.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0 || size > 4096) return 0;

    /* Comprimir */
    size_t comp_cap = size * 2 + 64;
    uint8_t *compressed = malloc(comp_cap);
    if (!compressed) return 0;

    int comp_len = lz77_compress(data, size, compressed, comp_cap);
    if (comp_len < 0) {
        free(compressed);
        return 0;
    }

    /* Descomprimir */
    size_t decomp_cap = size + 64;
    uint8_t *decompressed = malloc(decomp_cap);
    if (!decompressed) {
        free(compressed);
        return 0;
    }

    int decomp_len = lz77_decompress(compressed, comp_len,
                                      decompressed, decomp_cap);

    /* Verificar roundtrip */
    if (decomp_len < 0) {
        /* No debería fallar en datos que nosotros comprimimos */
        abort();
    }

    if ((size_t)decomp_len != size) {
        /* Tamaño diferente */
        abort();
    }

    if (memcmp(data, decompressed, size) != 0) {
        /* Datos diferentes */
        abort();
    }

    free(compressed);
    free(decompressed);
    return 0;
}
```

### Compilar y ejecutar

```bash
# Harness de decompress
clang -fsanitize=fuzzer,address,undefined -g -O1 \
    -o fuzz_decompress fuzz_decompress.c lz77.c

# Harness de roundtrip
clang -fsanitize=fuzzer,address,undefined -g -O1 \
    -o fuzz_roundtrip fuzz_roundtrip.c lz77.c

# Corpus
mkdir corpus_decompress corpus_roundtrip
echo -n 'hello' > corpus_decompress/seed1
echo -n 'aaaaaaaaaa' > corpus_roundtrip/seed1
echo -n 'abcabcabc' > corpus_roundtrip/seed2

# Fuzzear
./fuzz_decompress corpus_decompress/ -max_total_time=300
./fuzz_roundtrip corpus_roundtrip/ -max_total_time=300
```

### Bugs esperados

```
Bug 1 (decompress): heap-buffer-overflow
  El bucle de copia no verifica out_pos + length <= out_cap.
  Input: token de referencia con length grande.
  ASan: heap-buffer-overflow WRITE en lz77.c

Bug 2 (roundtrip): abort() por datos diferentes
  Cuando un byte literal tiene bit 7 set (>= 128),
  compress lo escribe sin marcar como literal.
  decompress lo interpreta como token de referencia.
  El roundtrip falla: decompress(compress(x)) != x.

Bug 3 (decompress): posible integer overflow
  Si offset es muy grande y out_pos es pequeño,
  out_pos - offset puede underflow (unsigned).
  Ya hay un check, pero ¿cubre todos los casos?
```

---

## 29. Programa de práctica: http_header_parser

### Especificación

Construye un parser de headers HTTP y escribe múltiples harnesses.

```
http_header_parser/
├── Makefile
├── http.h               ← API
├── http.c               ← implementación
├── fuzz_parse.c          ← harness 1: parsear request line + headers
├── fuzz_roundtrip.c      ← harness 2: parse → serialize → parse → compare
├── fuzz_malformed.c      ← harness 3: parsear con validación estricta
├── fuzz_multi.c          ← harness 4: secuencia de requests (keep-alive)
├── corpus/               ← semillas
├── http.dict             ← diccionario
└── tests/
    ├── test_http.c
    └── regression/
```

### http.h

```c
#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

#define HTTP_MAX_HEADERS    64
#define HTTP_MAX_HEADER_LEN 4096
#define HTTP_MAX_METHOD_LEN 16
#define HTTP_MAX_URI_LEN    2048

typedef struct {
    char name[128];
    char value[HTTP_MAX_HEADER_LEN];
} HttpHeader;

typedef struct {
    char method[HTTP_MAX_METHOD_LEN];
    char uri[HTTP_MAX_URI_LEN];
    char version[16];         /* "HTTP/1.0" o "HTTP/1.1" */
    HttpHeader headers[HTTP_MAX_HEADERS];
    int header_count;
    const char *body;         /* puntero al inicio del body */
    size_t body_len;
    int is_valid;
    char error[256];
} HttpRequest;

/* Parsea un HTTP request desde un buffer.
   Retorna 0 si éxito, -1 si error. */
int http_parse_request(HttpRequest *req, const char *data, size_t len);

/* Serializa un HttpRequest a string.
   Retorna el tamaño escrito, -1 si error. */
int http_serialize_request(const HttpRequest *req, char *out, size_t out_cap);

/* Obtener el valor de un header (case-insensitive).
   Retorna NULL si no existe. */
const char *http_get_header(const HttpRequest *req, const char *name);

/* Obtener Content-Length, -1 si no existe o inválido. */
int http_content_length(const HttpRequest *req);

#endif
```

### Tareas

1. **Implementa `http.c`** con soporte para:
   - Request line: METHOD URI HTTP/1.x\r\n
   - Headers: Name: Value\r\n (con folding de líneas)
   - Body separado por \r\n\r\n
   - Content-Length parsing
   - Case-insensitive header lookup
   - Introduce 3-4 bugs intencionales

2. **Escribe 4 harnesses** (uno por cada tipo):
   - `fuzz_parse.c`: solo parsear, verificar que no crashea
   - `fuzz_roundtrip.c`: parse → serialize → parse → compare
   - `fuzz_malformed.c`: parsear y verificar que los campos parseados
     son consistentes (ej: Content-Length == body_len)
   - `fuzz_multi.c`: interpretar el input como múltiples requests
     concatenadas (HTTP pipelining)

3. **Haz cada harness portátil** (AFL++ + libFuzzer + standalone)

4. **Escribe `http.dict`** con todos los tokens HTTP relevantes

5. **Prepara corpus** con 7+ semillas (GET, POST, con headers, sin body, etc.)

6. **Compila y fuzzea** cada harness 10 minutos

7. **Minimiza crashes**, arregla bugs, crea regression tests

8. **Compara** qué harness encontró más bugs y por qué

---

## 30. Ejercicios

### Ejercicio 1: FuzzedDataProvider en C

**Objetivo**: Implementar y usar un FuzzedDataProvider completo para C.

**Tareas**:

**a)** Completa la implementación del `FuzzInput` (sección 7) agregando:
   - `fuzz_consume_string(fi, max_len)` → retorna string null-terminated
   - `fuzz_consume_float(fi)` → retorna un float
   - `fuzz_consume_enum(fi, count)` → retorna un valor 0..count-1

**b)** Usa tu FuzzInput para escribir un harness para una API de base de datos
   in-memory:
   ```c
   // API: create_db, insert, query, delete, close_db
   // El harness debe interpretar los bytes como secuencias de operaciones
   ```

**c)** Compara la cobertura de tu harness stateful vs un harness que solo
   llama a `insert` con los bytes crudos. ¿Cuántas más funciones cubre?

---

### Ejercicio 2: harness de roundtrip

**Objetivo**: Encontrar bugs lógicos con roundtrip fuzzing.

**Tareas**:

**a)** Implementa un encoder/decoder de Run-Length Encoding (RLE):
   ```
   Encode: "AAABBC" → "3A2B1C"
   Decode: "3A2B1C" → "AAABBC"
   ```

**b)** Introduce un bug sutil: el encoder maneja mal runs de más de 9
   (ej: "AAAAAAAAAAA" (11 A's) → "11A" pero el decoder interpreta
   "11A" como "1" seguido de "1A").

**c)** Escribe un harness de roundtrip que:
   - Encode los bytes del fuzzer
   - Decode el resultado
   - Verifique que el decode es igual al input original
   - Llame a `abort()` si no son iguales

**d)** ¿Cuánto tarda el fuzzer en encontrar el bug? ¿Cuál es el input
   minimizado que lo activa?

---

### Ejercicio 3: harness para API stateful con invariantes

**Objetivo**: Encontrar bugs de estado con fuzzing y assertions.

**Tareas**:

**a)** Implementa un stack (pila) con esta API:
   ```c
   Stack *stack_create(size_t capacity);
   int stack_push(Stack *s, int value);
   int stack_pop(Stack *s, int *value);
   int stack_peek(Stack *s, int *value);
   size_t stack_size(Stack *s);
   int stack_is_empty(Stack *s);
   void stack_destroy(Stack *s);
   ```

**b)** Introduce un bug: `stack_pop` no decrementa el contador de tamaño
   en ciertos edge cases (ej: cuando capacity == size).

**c)** Escribe un harness stateful con invariantes:
   ```c
   // Después de push(x), peek debe retornar x
   // Después de push, size debe incrementar en 1
   // Después de pop, size debe decrementar en 1
   // is_empty == true ↔ size == 0
   ```

**d)** ¿El fuzzer encuentra el bug con solo las invariantes (sin ASan)?

---

### Ejercicio 4: múltiples harnesses para un parser

**Objetivo**: Demostrar el valor de múltiples harnesses.

**Tareas**:

**a)** Toma el parser INI del tópico T02 (AFL++) y escribe 3 harnesses:
   1. `fuzz_parse.c`: solo parsear
   2. `fuzz_get.c`: parsear + ini_get() con keys del mismo input
   3. `fuzz_stress.c`: parsear el mismo input 100 veces (buscar memory leaks)

**b)** Fuzzea cada uno 5 minutos. Compara:
   - ¿Qué harness tiene mejor exec speed?
   - ¿Qué harness encuentra más crashes?
   - ¿Qué harness tiene más cobertura?

**c)** ¿Hay bugs que solo un harness encuentra? ¿Cuáles y por qué?

**d)** Combina los corpus de los tres harnesses con `-merge=1`.
   ¿El corpus combinado tiene más cobertura que cualquiera individual?

---

## Navegación

- **Anterior**: [T03 - libFuzzer](../T03_libFuzzer/README.md)
- **Siguiente**: [C03/S02/T01 - cargo-fuzz](../../S02_Fuzzing_en_Rust/T01_Cargo_fuzz/README.md)

---

> **Sección 1: Fuzzing en C** — Tópico 4 de 4 completado
>
> - T01: Concepto de fuzzing ✓
> - T02: AFL++ ✓
> - T03: libFuzzer ✓
> - T04: Escribir harnesses ✓
>
> **Fin de la Sección 1: Fuzzing en C**
>
> Resumen de S01:
> 1. **Concepto de fuzzing**: generación de inputs, coverage-guided vs dumb, corpus, crash discovery
> 2. **AFL++**: instalación, afl-cc/afl-fuzz, dashboard, crashes, modo persistente, paralelismo, CmpLog
> 3. **libFuzzer**: LLVMFuzzerTestOneInput, -fsanitize=fuzzer, merge, value profile, fork mode, CI/CD
> 4. **Escribir harnesses**: 8 patrones, limitar recursos, APIs stateful, roundtrip, differential, portabilidad
