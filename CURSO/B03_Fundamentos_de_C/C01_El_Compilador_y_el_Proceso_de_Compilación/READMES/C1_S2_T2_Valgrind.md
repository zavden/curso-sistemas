# T02 — Valgrind

> **Errata del material base**
>
> Sin erratas detectadas. El material teórico y los laboratorios son factualmente correctos.

---

## 1. Qué es Valgrind

Valgrind es un framework de instrumentación que ejecuta tu programa en una CPU virtual, interceptando cada acceso a memoria. Su herramienta principal, **Memcheck**, detecta errores de memoria que el compilador no puede ver:

```bash
# Compilar con -g (debug info) y sin optimización:
gcc -g -O0 main.c -o main

# Ejecutar bajo Valgrind:
valgrind ./main

# Valgrind ejecuta el programa ~20-30x más lento
# pero detecta errores que de otra forma son invisibles
```

| Valgrind detecta | Valgrind NO detecta |
|-------------------|---------------------|
| Lecturas/escrituras fuera de límites (heap) | Errores en stack (buffer overflow en arrays locales) — usar ASan |
| Uso de memoria sin inicializar | Errores en tiempo de compilación |
| Memory leaks (malloc sin free) | Errores lógicos |
| Double free | Signed integer overflow — usar UBSan |
| Use-after-free | |
| Mismatched alloc/dealloc (malloc/delete) | |
| Overlapping memcpy | |

---

## 2. Acceso inválido a memoria

### Lectura fuera de límites

```c
int *arr = malloc(5 * sizeof(int));
for (int i = 0; i < 5; i++) arr[i] = i * 10;
printf("%d\n", arr[5]);    // Invalid read: índice 5 en array de 5
free(arr);
```

```
==PID== Invalid read of size 4
==PID==    at 0x...: main (prog.c:10)
==PID==  Address 0x... is 0 bytes after a block of size 20 alloc'd
==PID==    at 0x...: malloc (...)
==PID==    by 0x...: main (prog.c:5)
```

Cómo leer el reporte:
- **"Invalid read of size 4"** → leyó 4 bytes (un `int`) de memoria inválida
- **"at main (prog.c:10)"** → línea exacta donde ocurrió
- **"0 bytes after a block of size 20"** → justo después del bloque de 20 bytes (5 ints × 4)
- **Stack trace inferior** → dónde se asignó la memoria

Ver `labs/invalid_access.c:4-16` — `heap_overflow_read()` lee `arr[5]` de un array de 5 elementos.

### Escritura fuera de límites

```
==PID== Invalid write of size 4
==PID==    at 0x...: main (prog.c:5)
==PID==  Address 0x... is 0 bytes after a block of size 12 alloc'd
```

Ver `labs/invalid_access.c:18-28` — `heap_overflow_write()` escribe `arr[3]` en array de 3 (`i <= 3` debería ser `i < 3`).

---

## 3. Uso de memoria sin inicializar

```c
int *data = malloc(4 * sizeof(int));
data[0] = 10;
data[1] = 20;
// data[2] y data[3] sin inicializar

if (data[2] > 0) {        // Conditional jump on uninitialised value
    printf("positive\n");
}
```

```
==PID== Conditional jump or move depends on uninitialised value(s)
==PID==    at 0x...: main (prog.c:9)
```

Valgrind distingue `malloc` (no inicializa) de `calloc` (inicializa a cero). Para rastrear **dónde** se creó la memoria no inicializada:

```bash
valgrind --track-origins=yes ./main
```

```
==PID== Conditional jump or move depends on uninitialised value(s)
==PID==    at 0x...: main (prog.c:9)
==PID==  Uninitialised value was created by a heap allocation
==PID==    at 0x...: malloc (...)
==PID==    by 0x...: main (prog.c:3)
```

`--track-origins=yes` es más lento pero muestra exactamente qué `malloc` produjo el valor problemático.

Ver `labs/invalid_access.c:30-46` — `uninitialized_read()` solo inicializa 2 de 4 elementos.

---

## 4. Memory leaks

### Detección con --leak-check=full

```bash
valgrind --leak-check=full ./leak
```

```
==PID== HEAP SUMMARY:
==PID==     in use at exit: 46 bytes in 3 blocks
==PID==   total heap usage: 3 allocs, 0 frees, 46 bytes allocated
==PID==
==PID== 14 bytes in 1 blocks are definitely lost in loss record 2 of 3
==PID==    at 0x...: malloc (...)
==PID==    by 0x...: create_greeting (leak.c:7)
==PID==    by 0x...: main (leak.c:24)
==PID==
==PID== LEAK SUMMARY:
==PID==    definitely lost: 46 bytes in 3 blocks
==PID==    indirectly lost: 0 bytes in 0 blocks
==PID==      possibly lost: 0 bytes in 0 blocks
==PID==    still reachable: 0 bytes in 0 blocks
```

Ver `labs/leak.c` — `create_greeting()` y `create_scores()` hacen malloc pero main nunca hace free.

### Categorías de leaks

| Categoría | Significado | Gravedad |
|-----------|-------------|----------|
| **definitely lost** | No hay ningún puntero apuntando al bloque. Leak real. | Alta — corregir siempre |
| **indirectly lost** | Apuntado por un bloque que está definitely lost (ej: nodos de lista enlazada cuyo head se perdió) | Alta — corregir el padre |
| **possibly lost** | Un puntero apunta al interior del bloque (no al inicio). Puede ser leak o intencional. | Media — investigar |
| **still reachable** | Hay punteros válidos pero nunca se hizo free. Común en globales. | Baja — generalmente no es problema |

### Opciones de leak-check

```bash
valgrind --leak-check=full ./main       # reporte detallado (recomendado)
valgrind --leak-check=summary ./main    # solo resumen
valgrind --leak-check=no ./main         # no verificar leaks
valgrind --leak-check=full --show-reachable=yes ./main  # incluir still reachable
```

---

## 5. Double free y use-after-free

### Double free

```c
int *p = malloc(sizeof(int));
*p = 42;
free(p);
free(p);       // Invalid free
```

```
==PID== Invalid free() / delete / delete[] / realloc()
==PID==    at 0x...: free (...)
==PID==    by 0x...: main (prog.c:7)         ← dónde ocurrió el free inválido
==PID==  Address 0x... is 0 bytes inside a block of size 4 free'd
==PID==    at 0x...: free (...)
==PID==    by 0x...: main (prog.c:6)         ← dónde se hizo el primer free
==PID==  Block was alloc'd at
==PID==    at 0x...: malloc (...)
==PID==    by 0x...: main (prog.c:4)         ← dónde se asignó la memoria
```

### Use-after-free

```c
int *p = malloc(10 * sizeof(int));
p[0] = 42;
free(p);
printf("%d\n", p[0]);     // Invalid read of freed memory
```

```
==PID== Invalid read of size 4
==PID==    at 0x...: main (prog.c:9)         ← lectura inválida
==PID==  Address 0x... is 0 bytes inside a block of size 40 free'd
==PID==    at 0x...: free (...)
==PID==    by 0x...: main (prog.c:7)         ← dónde se liberó
==PID==  Block was alloc'd at
==PID==    at 0x...: malloc (...)
==PID==    by 0x...: main (prog.c:5)         ← dónde se asignó
```

### Las tres stack traces

Para double free y use-after-free, Valgrind muestra **tres** stack traces que responden a tres preguntas:

| Stack trace | Pregunta |
|-------------|----------|
| Primera | ¿Dónde ocurrió el error? (la lectura inválida o el free inválido) |
| Segunda ("free'd at") | ¿Dónde se liberó la memoria la primera vez? |
| Tercera ("alloc'd at") | ¿Dónde se asignó la memoria originalmente? |

Con estas tres piezas se reconstruye la secuencia: **asignación → liberación → uso indebido**.

Ver `labs/use_after_free.c` — `use_after_free_example()` y `double_free_example()`.

---

## 6. Opciones útiles de Valgrind

```bash
# Reporte completo con orígenes de valores sin inicializar:
valgrind --leak-check=full --track-origins=yes ./main

# Logging a archivo:
valgrind --log-file=valgrind.log ./main

# Profundidad de stack trace:
valgrind --num-callers=20 ./main   # default: 12

# Suppressions (para falsos positivos de bibliotecas):
valgrind --gen-suppressions=all ./main        # generar
valgrind --suppressions=my.supp ./main        # usar
```

---

## 7. Valgrind con GDB

Valgrind puede integrarse con GDB para depurar errores de memoria interactivamente:

```bash
# Terminal 1 — Valgrind en modo servidor de GDB:
valgrind --vgdb=yes --vgdb-error=0 ./main

# Terminal 2 — Conectar GDB:
gdb ./main
(gdb) target remote | vgdb

# Ahora GDB funciona normalmente pero con detección de Valgrind.
# Cuando Valgrind detecta un error, GDB para en ese punto.
```

```bash
# --vgdb-error=0  → parar en el primer error
# --vgdb-error=1  → parar después del primer error (dejar pasar el primero)
# --vgdb-error=N  → parar después de N errores
```

---

## 8. Cuándo usar Valgrind vs Sanitizers

| Criterio | Valgrind | ASan/UBSan |
|----------|----------|------------|
| Requiere recompilar | No | Sí (`-fsanitize=...`) |
| Overhead de velocidad | ~20-30x | ~2x |
| Detección de leaks | Excelente (categorías detalladas) | ASan básico (`ASAN_OPTIONS=detect_leaks=1`) |
| Errores de heap | Sí | Sí |
| Errores de stack | No | Sí (ASan) |
| Memoria sin inicializar | Sí | No (UBSan no, MSan sí pero solo clang) |
| Double free / use-after-free | Sí (3 stack traces) | Sí |
| Threads | Funciona pero lento | Mejor rendimiento |
| Producción | No (demasiado lento) | Posible (overhead menor) |

**Recomendación práctica**: usar ambos. ASan/UBSan en desarrollo diario (rápidos, detectan stack). Valgrind para auditorías de leaks y cuando no se puede recompilar.

---

## 9. Tabla de mensajes de Valgrind

| Mensaje | Significado | Causa típica |
|---------|-------------|--------------|
| Invalid read of size N | Lectura fuera de límites | Índice de array incorrecto |
| Invalid write of size N | Escritura fuera de límites | Buffer overflow |
| Conditional jump on uninitialised | Decisión con valor sin inicializar | malloc sin inicializar |
| Invalid free | Free de memoria ya liberada o no allocada | Double free |
| X bytes definitely lost | Memory leak confirmado | malloc sin free |
| X bytes indirectly lost | Leak transitivo | Lista enlazada perdida |
| Source and destination overlap | memcpy con regiones superpuestas | Usar memmove |
| Mismatched free | Tipo de free incorrecto | malloc + delete (C++) |

---

## 10. Archivos del laboratorio

| Archivo | Descripción |
|---------|-------------|
| `labs/leak.c` | `create_greeting()` y `create_scores()` retornan memoria que nunca se libera — 3 leaks |
| `labs/invalid_access.c` | Lectura OOB, escritura OOB, lectura de memoria sin inicializar |
| `labs/use_after_free.c` | Use-after-free y double free en funciones separadas |
| `labs/buggy.c` | 4 bugs combinados: leak, off-by-one sprintf, uninit, double free |

---

## Ejercicios

### Ejercicio 1 — Memory leak: el silencio engañoso

Compila y ejecuta `leak.c` sin Valgrind, luego con Valgrind:

```bash
cd labs/
gcc -g -O0 leak.c -o leak
./leak
echo "---"
valgrind --leak-check=full ./leak
```

**Predicción**: Antes de ejecutar, lee `leak.c` y responde:

<details><summary>¿El programa mostrará algún error sin Valgrind? ¿Cuántos bytes se pierden y en cuántos bloques?</summary>

**Sin Valgrind**: el programa termina silenciosamente sin ningún error visible. No produce salida ni crashea. El sistema operativo recupera toda la memoria al finalizar el proceso, así que el leak es invisible.

**Con Valgrind**: reporta **46 bytes en 3 bloques** definitely lost:

| Bloque | Función | Tamaño | Cálculo |
|--------|---------|--------|---------|
| 1 | `create_greeting("Alice")` | 14 bytes | `strlen("Hello, ") + strlen("Alice") + 2 = 14` |
| 2 | `create_greeting("Bob")` | 12 bytes | `strlen("Hello, ") + strlen("Bob") + 2 = 12` |
| 3 | `create_scores(5)` | 20 bytes | `5 * sizeof(int) = 20` |

HEAP SUMMARY: `3 allocs, 0 frees` — tres malloc, cero free.

</details>

---

### Ejercicio 2 — Lectura fuera de límites

Ejecuta `invalid_access.c` con Valgrind:

```bash
gcc -g -O0 invalid_access.c -o invalid_access
./invalid_access
echo "---"
valgrind ./invalid_access
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿El programa crasheará sin Valgrind? ¿Cuántos errores reportará Valgrind?</summary>

**Sin Valgrind**: el programa probablemente **no crashea**. `arr[5]` lee basura de la memoria adyacente al heap block, `arr[3]` de un array de 3 escribe fuera de límites pero sin crashear, y `data[2]` muestra un valor indeterminado. El programa "funciona" — este es exactamente el peligro.

**Con Valgrind**: reporta **3 errores**:

1. **"Invalid read of size 4"** en `heap_overflow_read` (línea 13): lee `arr[5]` — "0 bytes after a block of size 20 alloc'd" (5 ints × 4 bytes = 20)
2. **"Invalid write of size 4"** en `heap_overflow_write` (línea 24): `i <= 3` debería ser `i < 3` — escribe `arr[3]` fuera de un block de 12 bytes
3. **"Conditional jump depends on uninitialised value"** en `uninitialized_read` (línea 39): `data[2]` nunca fue inicializado

</details>

---

### Ejercicio 3 — track-origins para valores sin inicializar

Ejecuta de nuevo con `--track-origins=yes`:

```bash
valgrind --track-origins=yes ./invalid_access
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿Qué información adicional mostrará --track-origins=yes respecto al error de memoria sin inicializar?</summary>

Sin `--track-origins=yes`, Valgrind solo dice *"Conditional jump depends on uninitialised value"* con la línea donde se **usó** el valor (línea 39: `if (data[2] > 0)`).

Con `--track-origins=yes`, Valgrind añade:

```
Uninitialised value was created by a heap allocation
   at 0x...: malloc (...)
   by 0x...: uninitialized_read (invalid_access.c:31)
```

Ahora sabes no solo **dónde** se usó el valor indeterminado (línea 39), sino **dónde** se creó (el `malloc` de la línea 31). Es más lento pero esencial cuando el origen del valor no inicializado no es obvio (por ejemplo, cuando la asignación y el uso están en funciones diferentes).

</details>

---

### Ejercicio 4 — Use-after-free: "funciona" sin herramientas

Ejecuta `use_after_free.c` sin y con Valgrind:

```bash
gcc -g -O0 use_after_free.c -o use_after_free
./use_after_free
echo "---"
valgrind ./use_after_free
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿Qué imprimirá "After free: %s" sin Valgrind? ¿Y el double free causará crash?</summary>

**Sin Valgrind**: `printf("After free: %s\n", name)` probablemente **imprime "Valgrind test" correctamente** — la memoria ya fue liberada pero el allocator no borra el contenido; los bytes siguen ahí hasta que otro `malloc` reutilice el espacio. El double free puede o no crashear dependiendo de la implementación de malloc.

**Con Valgrind**: reporta dos errores graves:

1. **Use-after-free**: "Invalid read of size 1" en `strlen` llamado por `printf` — la memoria ya fue liberada en línea 12 pero se lee en línea 15

2. **Double free**: "Invalid free()" — `free(data)` se llama dos veces (líneas 26 y 29)

Ambos con **tres stack traces** cada uno: dónde ocurrió, dónde se liberó, dónde se asignó.

Este es el peligro del use-after-free: puede "funcionar" durante años hasta que un cambio en el layout del heap haga que otro `malloc` reutilice el espacio y sobrescriba los datos.

</details>

---

### Ejercicio 5 — Las tres stack traces

Del reporte del ejercicio anterior, identifica las tres stack traces del error de use-after-free:

**Predicción**: Antes de revisar el reporte, responde:

<details><summary>¿Qué función y línea aparecen en cada una de las tres stack traces?</summary>

| Stack trace | Pregunta | Función/línea |
|-------------|----------|---------------|
| 1ª (el error) | ¿Dónde se lee la memoria liberada? | `printf` → `strlen` → `use_after_free_example` línea 15 |
| 2ª ("free'd at") | ¿Dónde se liberó? | `free` llamado en `use_after_free_example` línea 12 |
| 3ª ("alloc'd at") | ¿Dónde se asignó? | `malloc` llamado en `use_after_free_example` línea 6 |

La secuencia reconstruida: `malloc(32)` en línea 6 → `strcpy` y uso normal → `free(name)` en línea 12 → `printf("%s", name)` en línea 15 (usa memoria liberada).

La corrección: mover el `free(name)` **después** del último uso, o copiar los datos antes del free.

</details>

---

### Ejercicio 6 — Auditoría manual antes de Valgrind

Lee `buggy.c` sin ejecutarlo. Busca los 4 bugs manualmente:

```bash
cat buggy.c
```

Para cada función (`register_user`, `format_id`, `sum_array`, `process_data`), pregúntate: ¿se libera la memoria? ¿los accesos respetan límites? ¿se inicializan los valores? ¿hay double free?

**Predicción**: Antes de usar Valgrind, responde:

<details><summary>¿Cuáles son los 4 bugs y en qué líneas?</summary>

| # | Tipo | Función | Línea | Descripción |
|---|------|---------|-------|-------------|
| 1 | Memory leak | `register_user()` | 7 | `malloc(64)` sin free. Se llama 2 veces → 128 bytes perdidos |
| 2 | Invalid write (off-by-one) | `format_id()` | 18 | `sprintf(buf, "ID-%d", 1000)` escribe "ID-1000\0" = 8 bytes en buffer de 7 |
| 3 | Uninitialized read | `sum_array()` | 35 | Loop inicializa solo índices pares (`i += 2`), pero lee todos (`i++`) |
| 4 | Double free | `process_data()` | 53 | `free(buffer)` se llama dos veces (líneas 51 y 53) |

El bug 2 es sutil: "ID-1000" tiene 7 caracteres, pero `sprintf` añade `'\0'`, totalizando 8 bytes. `malloc(7)` asigna solo 7.

</details>

---

### Ejercicio 7 — Verificar con Valgrind

Compila y ejecuta `buggy.c` con Valgrind completo:

```bash
gcc -g -O0 buggy.c -o buggy
valgrind --leak-check=full --track-origins=yes ./buggy
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿En qué orden reportará Valgrind los 4 errores? ¿Cuántos bytes aparecerán en LEAK SUMMARY?</summary>

Valgrind reporta errores **en orden de ejecución**:

1. **Invalid write** — `format_id()` (línea 18): sprintf escribe 1 byte más allá del buffer de 7
2. **Conditional jump on uninitialised** — `sum_array()` (línea 35): `arr[1]`, `arr[3]`, `arr[5]` sin inicializar
3. **Invalid free** — `process_data()` (línea 53): double free con 3 stack traces

4. Al final, **LEAK SUMMARY**: **128 bytes in 2 blocks definitely lost** — las dos llamadas a `register_user()` que hacen `malloc(64)` sin free (2 × 64 = 128).

Nota: los leaks se reportan al final del programa (en el LEAK SUMMARY), mientras que los errores de acceso y double free se reportan cuando ocurren.

</details>

---

### Ejercicio 8 — Corregir buggy.c

Aplica los fixes a cada bug de `buggy.c`. Para cada corrección, piensa qué patrón seguro aplica:

**Predicción**: Antes de corregir, responde:

<details><summary>¿Cuál es el fix correcto para cada bug?</summary>

| Bug | Fix |
|-----|-----|
| 1. Leak en `register_user()` | Agregar `free(record)` antes del return, o retornar el puntero para que el llamador lo libere |
| 2. Off-by-one en `format_id()` | `malloc(8)` en vez de `malloc(7)`. Mejor aún: usar `snprintf(buf, 7, "ID-%d", id)` para limitar la escritura |
| 3. Uninit en `sum_array()` | Inicializar todo el array: `for (int i = 0; i < count; i++) arr[i] = i;` o usar `calloc(count, sizeof(int))` |
| 4. Double free en `process_data()` | Eliminar el segundo `free(buffer)`, o poner `buffer = NULL` tras el primer free (`free(NULL)` es no-op según el estándar) |

Después de corregir, verifica con: `valgrind --leak-check=full --track-origins=yes ./buggy`. El objetivo: **0 errors, 0 bytes lost**.

</details>

---

### Ejercicio 9 — HEAP SUMMARY: allocs vs frees

Examina el HEAP SUMMARY de cada programa del lab:

```bash
valgrind ./leak 2>&1 | grep "total heap usage"
valgrind ./invalid_access 2>&1 | grep "total heap usage"
valgrind ./use_after_free 2>&1 | grep "total heap usage"
```

**Predicción**: Antes de ejecutar, cuenta los malloc y free de cada programa:

<details><summary>¿Cuántos allocs y frees reportará cada programa?</summary>

| Programa | allocs | frees | Diferencia |
|----------|--------|-------|------------|
| `leak.c` | 3 | 0 | 3 bloques perdidos (todo se pierde) |
| `invalid_access.c` | 3 | 3 | 0 (cada función hace free correctamente; los errores son de acceso, no de leaks) |
| `use_after_free.c` | 2 | 3 | -1 (más frees que allocs → el extra es el double free, reportado como Invalid free) |

La regla de oro: en un programa correcto, `allocs == frees`. Cualquier desviación indica un leak (allocs > frees) o un double free (frees > allocs, aunque Valgrind cuenta el free inválido).

Nota: Valgrind puede reportar allocs adicionales si el programa usa `printf` o funciones de la libc que internamente hacen `malloc` (como buffers de stdio). Esto es normal.

</details>

---

### Ejercicio 10 — Programa limpio: cero errores

Escribe un programa `clean.c` que:
1. Asigne un array de 100 ints con `malloc`
2. Lo llene con valores (ej: `arr[i] = i * 2`)
3. Encuentre el máximo
4. Imprima el máximo
5. Libere la memoria

```bash
gcc -g -O0 clean.c -o clean
valgrind --leak-check=full ./clean
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿Qué debe mostrar el reporte de Valgrind para un programa correcto?</summary>

```
==PID== HEAP SUMMARY:
==PID==     in use at exit: 0 bytes in 0 blocks
==PID==   total heap usage: 1 allocs, 1 frees, 400 bytes allocated
==PID==
==PID== All heap blocks were freed -- no leaks are possible
==PID==
==PID== ERROR SUMMARY: 0 errors from 0 contexts
```

Puntos clave:
- **"in use at exit: 0 bytes in 0 blocks"** — toda la memoria fue liberada
- **"1 allocs, 1 frees"** — cada malloc tiene su free
- **"400 bytes allocated"** — 100 ints × 4 bytes
- **"All heap blocks were freed"** — no hay leaks
- **"0 errors"** — sin accesos inválidos ni uso de memoria sin inicializar

Este es el objetivo: compilar siempre con `-g -O0` en desarrollo y verificar periódicamente con Valgrind que el programa está limpio.

</details>

Limpieza final:

```bash
rm -f leak invalid_access use_after_free buggy clean
```
