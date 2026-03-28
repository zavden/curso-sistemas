# Lab — fprintf, fscanf, snprintf, sscanf

## Objetivo

Usar las cuatro funciones principales de I/O formateado de `<stdio.h>` para
escribir datos estructurados a archivo, leerlos con parseo, formatear strings
en buffers de tamano limitado, y extraer campos de strings. Al finalizar,
sabras verificar valores de retorno, detectar truncacion con `snprintf`, y
aplicar el patron seguro `fgets` + `sscanf`.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `fprintf_log.c` | Escribe un log formateado a archivo con timestamp |
| `fscanf_parse.c` | Parsea datos de archivo y maneja lineas corruptas |
| `snprintf_buf.c` | Demuestra snprintf, truncacion, y el patron string builder |
| `sscanf_extract.c` | Extrae campos de strings y valida input |

---

## Parte 1 — fprintf: escritura formateada a archivo

**Objetivo**: Escribir datos formateados a un archivo usando `fprintf`, con
distintos especificadores de formato, y usar `fprintf(stderr, ...)` para
mensajes de error.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat fprintf_log.c
```

Observa:

- Se abre un archivo con `fopen` en modo `"w"`
- Se usa `fprintf(log, ...)` con distintos especificadores: `%s`, `%d`, `%ld`, `%.2f`, `%-12s`
- Se usa `fprintf(stderr, ...)` para un mensaje de error
- Se cierra el archivo, se reabre en modo `"r"` y se lee con `fgets`

Antes de compilar, predice:

- La linea `fprintf(stderr, ...)` aparecera en la salida del programa o en la
  salida de error?
- El formato `%-12s` produce alineacion a la izquierda o a la derecha?
- El formato `%.2f` con el valor 23.756, mostrara 23.75 o 23.76?

### Paso 1.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic fprintf_log.c -o fprintf_log
./fprintf_log
```

Salida esperada:

```
Log written to server.log
--- Contents of server.log ---
=== Server Log ===
Started at: <timestamp>
TIMESTAMP    LEVEL    MESSAGE
<timestamp>  INFO     Server started
<timestamp>  WARN     High memory usage: 87.3%
<timestamp>  ERROR    Connection refused on port 8080
<timestamp>  INFO     Server stopped

Summary: 1542 requests, avg response 23.76 ms
```

Nota: `<timestamp>` sera un numero como `1773845239` (segundos desde epoch).

### Paso 1.3 — Separar stdout de stderr

```bash
./fprintf_log 2>/dev/null
```

Ahora no ves "Log written to server.log" porque fue escrito a stderr y lo
redirigiste a `/dev/null`. Solo aparece la salida de `printf` (stdout).

```bash
./fprintf_log 1>/dev/null
```

Ahora solo ves "Log written to server.log" -- la salida de `fprintf(stderr, ...)`.

### Paso 1.4 — Examinar el archivo generado

```bash
cat server.log
```

El archivo contiene exactamente lo que `fprintf` escribio. Observa que `%-12ld`
alinea el timestamp a la izquierda con ancho 12, y `%-8s` alinea el nivel.

### Limpieza de la parte 1

```bash
rm -f fprintf_log server.log
```

---

## Parte 2 — fscanf: lectura formateada de archivo

**Objetivo**: Leer datos formateados de un archivo con `fscanf`, verificar el
valor de retorno, y manejar lineas que no coinciden con el formato esperado.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat fscanf_parse.c
```

Observa:

- El programa primero crea un archivo `sensors.dat` con datos formateados
- Una linea dice `"CORRUPTED LINE"` -- no tiene el formato `nombre valor`
- `fscanf` retorna el numero de campos leidos (no el numero de bytes)
- Si `fscanf` no puede parsear un campo, hay que consumir la linea residual
  con `fgetc` para evitar un bucle infinito

Antes de compilar, predice:

- Cuantas lineas parseara exitosamente? (hay 6 lineas, una corrupta)
- Que valor retornara `fscanf` para la linea `"CORRUPTED LINE"`?
- Que pasaria si no consumieramos la linea corrupta con el bucle `fgetc`?

### Paso 2.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic fscanf_parse.c -o fscanf_parse
./fscanf_parse
```

Salida esperada:

```
Parsing sensors.dat:

  Line 1: sensor=temp_cpu     value=72.50  [OK: 2 fields]
  Line 2: sensor=temp_gpu     value=68.30  [OK: 2 fields]
  Line 3: sensor=fan_rpm      value=1800.00  [OK: 2 fields]
  Line 4: sensor=voltage      value=12.04  [OK: 2 fields]
  Line 5: parse error (got 1 fields, expected 2)  [SKIP]
  Line 6: sensor=temp_ssd     value=45.10  [OK: 2 fields]

Result: 5 parsed, 1 failed, 6 total
```

Verifica tu prediccion: la linea 5 (`"CORRUPTED LINE"`) parseo solo 1 campo
(`"CORRUPTED"` como string) pero no encontro un `double` despues.

### Paso 2.3 — Entender el valor de retorno de fscanf

El formato es `"%63s %lf"` -- espera 2 campos. Para la linea corrupta:

- `"CORRUPTED"` se lee como `%63s` (campo 1, exitoso)
- `"LINE"` no es un `double` valido para `%lf` (campo 2, falla)
- `fscanf` retorna `1` (un campo leido)

Si no consumieramos el resto de la linea corrupta, `fscanf` intentaria leer
`"LINE"` otra vez como `%63s` en la siguiente iteracion, creando un
desfase permanente en el parseo.

### Paso 2.4 — Examinar los datos

```bash
cat sensors.dat
```

Compara el archivo con la salida del programa para verificar que cada linea se
proceso correctamente.

### Limpieza de la parte 2

```bash
rm -f fscanf_parse sensors.dat
```

---

## Parte 3 — snprintf: formateo seguro en buffer

**Objetivo**: Usar `snprintf` para formatear strings con limite de tamano,
detectar truncacion comparando el valor de retorno con el tamano del buffer,
y aplicar el patron string builder.

### Paso 3.1 — Examinar el codigo fuente

```bash
cat snprintf_buf.c
```

Observa tres secciones:

1. **Truncacion**: el mismo formato se escribe en un buffer grande (100 bytes)
   y uno chico (20 bytes)
2. **String builder**: se van concatenando strings en un buffer usando
   `snprintf(buf + pos, sizeof(buf) - pos, ...)`
3. **Input largo**: se copia un string de 47 chars en un buffer de 16 bytes

Antes de compilar, predice:

- El string `"User: Alice, Score: 95"` tiene 22 caracteres. Que valor
  retornara `snprintf` cuando el buffer es de 20 bytes?
- Cuantos caracteres quedaran en `small_buf`? (recuerda: necesita espacio
  para `'\0'`)
- En la seccion string builder, habra truncacion con un buffer de 80 bytes?

### Paso 3.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic snprintf_buf.c -o snprintf_buf
./snprintf_buf
```

Salida esperada:

```
large_buf: "User: Alice, Score: 95"
  needed=22, capacity=100, truncated=NO

small_buf: "User: Alice, Score:"
  needed=22, capacity=20, truncated=YES
  WARNING: output truncated, needed 22+1 bytes

--- String builder ---

Report (43/80 bytes): "cpu_temp=72.5 gpu_temp=68.3 fan_rpm=1800.0 "

--- snprintf with long input ---

snprintf result: "a_very_long_sen" (needed 47, capacity 16)
strlen(safe_buf) = 15 (always < capacity)
```

### Paso 3.3 — Analizar la deteccion de truncacion

La clave de `snprintf` es su valor de retorno:

- Retorna el numero de caracteres que **habria escrito** si el buffer fuera
  suficientemente grande (sin contar `'\0'`)
- Si `retorno >= tamano_buffer`, hubo truncacion

En el ejemplo:

- `"User: Alice, Score: 95"` necesita 22 chars + 1 para `'\0'` = 23 bytes
- `small_buf` tiene 20 bytes, asi que `snprintf` retorna 22 (lo que habria
  escrito) pero solo escribe 19 chars + `'\0'`
- La condicion `22 >= 20` es verdadera: truncacion detectada

### Paso 3.4 — Analizar el patron string builder

Observa la tecnica en el codigo fuente:

```c
pos += snprintf(report + pos, sizeof(report) - pos, "%s=%.1f ", ...);
```

Cada llamada a `snprintf` escribe a partir de `report + pos` con un limite
de `sizeof(report) - pos` bytes restantes. El `pos` avanza con cada escritura
exitosa. Si `pos + ret >= sizeof(report)`, el buffer se lleno y se detiene.

Este patron permite construir strings complejos de forma incremental sin riesgo
de buffer overflow.

### Limpieza de la parte 3

```bash
rm -f snprintf_buf
```

---

## Parte 4 — sscanf: parseo de strings

**Objetivo**: Usar `sscanf` para extraer campos de strings con formatos
complejos, aplicar el patron seguro `fgets` + `sscanf`, y validar input
verificando el valor de retorno.

### Paso 4.1 — Examinar el codigo fuente

```bash
cat sscanf_extract.c
```

Observa tres secciones:

1. **Parseo estructurado**: extrae fecha, nivel y mensaje de un string de log
2. **fgets + sscanf**: lee CSV linea por linea con `fgets` y parsea con `sscanf`
3. **Validacion**: parsea IPs y verifica tanto el formato como el rango

Antes de compilar, predice:

- En `"%d-%d-%d %15s %127[^\n]"`, que hace el especificador `%[^\n]`?
- En el CSV, la linea `"INVALID LINE"` parseara con el formato `"%63[^,],%d,%lf"`?
- La IP `"256.1.2.3"` parseara 4 campos con `"%d.%d.%d.%d"` o fallara?

### Paso 4.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic sscanf_extract.c -o sscanf_extract
./sscanf_extract
```

Salida esperada:

```
--- Parsing structured strings ---

Input:   "2026-03-18 ERROR Connection refused on port 8080"
Fields:  5 (expected 5)
Date:    2026-03-18
Level:   ERROR
Message: Connection refused on port 8080

--- fgets + sscanf pattern ---

NAME       AGE   SCORE    STATUS
----       ---   -----    ------
Alice      30    95.5     OK
Bob        25    87.2     OK
???        ?     ?        PARSE ERROR (line 3: "INVALID LINE")
Charlie    35    91.8     OK

--- Validation with sscanf return value ---

  "192.168.1.100" -> 4 fields -> VALID (192.168.1.100)
  "10.0.0.1" -> 4 fields -> VALID (10.0.0.1)
  "not.an.ip.address" -> 0 fields -> INVALID (expected 4 fields)
  "256.1.2.3" -> 4 fields -> INVALID (octet out of range)
  "172.16" -> 2 fields -> INVALID (expected 4 fields)
```

### Paso 4.3 — Analizar el especificador %[^\n]

El formato `%[^\n]` es un scanset invertido: lee todos los caracteres **excepto**
`'\n'`. A diferencia de `%s` que se detiene en cualquier whitespace, `%[^\n]`
lee espacios, tabs y todo lo demas hasta el fin de linea. Esto permite
capturar mensajes completos como `"Connection refused on port 8080"`.

El numero `127` en `%127[^\n]` limita la lectura a 127 caracteres para evitar
buffer overflow, igual que `%49s` limita a 49 caracteres.

### Paso 4.4 — Analizar la validacion de IPs

`sscanf` con `"%d.%d.%d.%d"` parsea exitosamente `"256.1.2.3"` -- extrae 4
enteros. Pero `sscanf` no sabe que un octeto de IP debe estar entre 0 y 255.
La validacion semantica (el rango) es responsabilidad del programador.

Esto demuestra un principio importante: `sscanf` parsea la **sintaxis** (el
formato), pero la **semantica** (el significado) requiere validacion adicional.

### Paso 4.5 — El patron fgets + sscanf

El codigo usa `fgets` para leer cada linea completa y luego `sscanf` para
parsearla. Este es el patron recomendado en C porque:

- `fgets` lee con limite de tamano (seguro contra buffer overflow)
- `fgets` consume la linea completa incluyendo `'\n'` (sin residuos)
- `sscanf` parsea un string en memoria (sin riesgo de quedarse atascado)
- Si `sscanf` falla, simplemente se pasa a la siguiente linea

Comparalo con `fscanf` directo (parte 2), donde una linea corrupta requeria
un bucle manual para consumir el residuo.

### Limpieza de la parte 4

```bash
rm -f sscanf_extract users.csv
```

---

## Limpieza final

```bash
rm -f fprintf_log fscanf_parse snprintf_buf sscanf_extract
rm -f server.log sensors.dat users.csv
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  fprintf_log.c  fscanf_parse.c  snprintf_buf.c  sscanf_extract.c
```

---

## Conceptos reforzados

1. `fprintf` escribe a cualquier `FILE *` con formato -- `fprintf(stderr, ...)`
   es la forma correcta de emitir mensajes de error, separandolos de stdout.

2. `fscanf` retorna el numero de campos leidos exitosamente, no el numero de
   bytes. Verificar este valor es obligatorio para detectar lineas corruptas
   o fin de archivo.

3. Cuando `fscanf` falla a mitad de linea, deja el input residual en el
   stream. Hay que consumirlo manualmente (con `fgetc` o `fgets`) para evitar
   un bucle infinito.

4. `snprintf` siempre termina el buffer con `'\0'` y retorna el numero de
   caracteres que **habria escrito** sin limite. La condicion
   `retorno >= tamano` detecta truncacion de forma fiable.

5. Nunca usar `sprintf` -- no tiene limite de tamano y produce buffer overflow
   con datos largos. `snprintf` es su reemplazo seguro directo.

6. `sscanf` parsea un string ya en memoria. Combinado con `fgets`, forma el
   patron mas seguro para leer input formateado en C: `fgets` garantiza la
   lectura segura, `sscanf` el parseo sin residuos.

7. `sscanf` valida la **sintaxis** (formato) pero no la **semantica**
   (significado). Parsear `"256.1.2.3"` como 4 enteros funciona, pero
   el programador debe verificar que cada octeto este en rango [0, 255].

8. El especificador `%[^\n]` en `sscanf` lee hasta el fin de linea incluyendo
   espacios, a diferencia de `%s` que se detiene en cualquier whitespace.
   Siempre usar con limite de tamano (ej. `%127[^\n]`).
