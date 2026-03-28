# Lab — RAII en C

## Objetivo

Practicar los patrones que simulan RAII en C: goto cleanup y
`__attribute__((cleanup))`. Al finalizar, sabras implementar liberacion segura
de multiples recursos, entenderas por que goto cleanup es el estandar en el
kernel de Linux, y podras decidir cuando usar cada patron.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `goto_cleanup.c` | Copia de archivo con patron goto cleanup y mensajes de traza |
| `cleanup_attr.c` | Misma logica con `__attribute__((cleanup))` y macros |
| `compare_styles.c` | Tres estilos (if-else anidado, goto, attribute) en un programa |

---

## Parte 1 — Patron goto cleanup

**Objetivo**: Compilar y ejecutar un programa que adquiere tres recursos (dos
archivos y un buffer malloc), observar como goto cleanup libera todo en orden
inverso, y verificar que los error paths tambien liberan correctamente.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat goto_cleanup.c
```

Observa la estructura del programa:

- Todas las variables de recurso se declaran al inicio, inicializadas a `NULL`.
- Cada adquisicion (`fopen`, `malloc`) verifica el resultado y salta a `cleanup`
  si falla.
- El label `cleanup:` libera todo en orden **inverso** al de adquisicion.
- `free(NULL)` es seguro (no hace nada), pero `fclose(NULL)` no lo es, por eso
  los `fclose` verifican con `if`.

### Paso 1.2 — Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic goto_cleanup.c -o goto_cleanup
```

No debe producir ningun warning ni error.

### Paso 1.3 — Crear archivo de prueba y ejecutar (caso exitoso)

```bash
echo "hello from the test file" > /tmp/raii_input.txt
./goto_cleanup /tmp/raii_input.txt /tmp/raii_output.txt
```

Salida esperada:

```
--- goto cleanup demo ---

[info] resources acquired: fin, fout, buffer (4096 bytes)
[info] copy complete
[cleanup] freeing buffer... done
[cleanup] closing fout... done
[cleanup] closing fin... done

result: success (code 0)
```

Observa el orden de liberacion: buffer, fout, fin. Es el **inverso** del orden
de adquisicion (fin, fout, buffer). Este orden inverso es una regla fundamental:
el ultimo recurso adquirido es el primero en liberarse.

Verifica que la copia funciono:

```bash
cat /tmp/raii_output.txt
```

### Paso 1.4 — Ejecutar con error (archivo inexistente)

Antes de ejecutar, predice:

- Si el archivo de entrada no existe, el primer `fopen` fallara. Que recursos
  se habran adquirido en ese momento?
- Que mensajes de `[cleanup]` aparecerarn?
- El programa imprimira `free(NULL)` o se crasheara?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.5 — Verificar la prediccion

```bash
./goto_cleanup /tmp/nonexistent.txt /tmp/raii_output.txt
```

Salida esperada:

```
--- goto cleanup demo ---

fopen input: No such file or directory
[cleanup] freeing buffer... done
[cleanup] closing fout... done
[cleanup] closing fin... done

result: failure (code -1)
```

Todos los `[cleanup]` se ejecutaron aunque los recursos eran NULL. Esto
funciona porque:

- `free(NULL)` no hace nada (definido por el estandar C)
- `fclose` esta protegido con `if (fout)` / `if (fin)`

El punto unico de salida (`cleanup:`) garantiza que **siempre** se ejecuta la
limpieza, sin importar donde ocurra el error.

### Limpieza de la parte 1

```bash
rm -f goto_cleanup /tmp/raii_input.txt /tmp/raii_output.txt
```

---

## Parte 2 — `__attribute__((cleanup))` de GCC

**Objetivo**: Usar la extension GCC `__attribute__((cleanup))` para que la
liberacion ocurra automaticamente al salir del scope, sin necesidad de goto.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat cleanup_attr.c
```

Observa las diferencias clave con `goto_cleanup.c`:

- Las funciones `cleanup_free` y `cleanup_fclose` reciben un **puntero a la
  variable**, no el valor directamente. Asi funciona el atributo cleanup.
- Las macros `AUTO_FREE` y `AUTO_FCLOSE` hacen la sintaxis mas legible.
- No hay label `cleanup:` ni `goto`. Los `return` tempranos son seguros porque
  el cleanup ocurre automaticamente.

### Paso 2.2 — Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic cleanup_attr.c -o cleanup_attr
```

Nota: el flag `-std=c11` no bloquea `__attribute__((cleanup))` porque GCC
siempre acepta sus propias extensiones. El flag `-Wpedantic` podria producir un
warning sobre extensiones no estandar en versiones futuras, pero actualmente no
lo hace para este atributo.

### Paso 2.3 — Crear archivo de prueba y ejecutar (caso exitoso)

```bash
echo "hello from the test file" > /tmp/raii_input.txt
./cleanup_attr /tmp/raii_input.txt /tmp/raii_output.txt
```

Salida esperada:

```
--- cleanup attribute demo ---

[info] resources acquired: fin, fout, buffer (4096 bytes)
[info] copy complete
[auto-cleanup] freeing pointer... done
[auto-cleanup] closing file... done
[auto-cleanup] closing file... done

result: success (code 0)
```

Observa: los mensajes `[auto-cleanup]` aparecen **despues** de `[info] copy
complete` y **antes** de `result:`. La liberacion ocurrio automaticamente al
salir del scope de `copy_file()`, en orden inverso al de declaracion.

### Paso 2.4 — Ejecutar con error (archivo inexistente)

Antes de ejecutar, predice:

- Si `fopen` del archivo de entrada falla, la funcion hace `return -1`. Se
  ejecutara el cleanup automaticamente?
- Cuantos mensajes `[auto-cleanup]` aparecerarn?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.5 — Verificar la prediccion

```bash
./cleanup_attr /tmp/nonexistent.txt /tmp/raii_output.txt
```

Salida esperada:

```
--- cleanup attribute demo ---

fopen input: No such file or directory
[auto-cleanup] closing file... done

result: failure (code -1)
```

Solo aparece un `[auto-cleanup]` porque solo `fin` habia sido declarada con
`AUTO_FCLOSE` antes del `return -1`. Las variables `fout` y `buffer` aun no
existian en ese scope. Esto es diferente del patron goto cleanup, donde el label
`cleanup:` siempre ejecuta todas las liberaciones (pero con NULL).

### Paso 2.6 — Comparar assembly (opcional)

Para ver que `__attribute__((cleanup))` realmente genera codigo de limpieza:

```bash
gcc -S -O0 cleanup_attr.c -o cleanup_attr.s
grep -n "cleanup" cleanup_attr.s | head -10
```

El compilador inserta llamadas a las funciones cleanup en cada punto de salida
de la funcion. No es magia en runtime: es codigo generado en compilacion.

```bash
rm -f cleanup_attr.s
```

### Limpieza de la parte 2

```bash
rm -f cleanup_attr /tmp/raii_input.txt /tmp/raii_output.txt
```

---

## Parte 3 — Comparacion de estilos

**Objetivo**: Ejecutar las tres formas de manejar multiples recursos (if-else
anidado, goto cleanup, cleanup attribute) y comparar legibilidad, seguridad y
comportamiento ante errores.

### Paso 3.1 — Examinar el codigo fuente

```bash
cat compare_styles.c
```

Observa los tres estilos:

1. **`nested_style`** (lineas ~12-49): if-else anidados. Cada recurso se
   adquiere dentro de un `if` que verifica el anterior. La limpieza ocurre en
   los cierres de llaves. El nivel de indentacion crece con cada recurso.

2. **`goto_style`** (lineas ~55-89): goto cleanup. Todas las variables al
   inicio, adquisicion lineal, un solo label de cleanup. El codigo es plano
   (sin anidamiento profundo).

3. **`cleanup_attr_style`** (lineas ~97-122): atributo cleanup. Similar al goto
   pero sin el label. Los `return` tempranos son seguros.

Antes de ejecutar, predice:

- Los tres estilos producirarn la misma salida en el caso exitoso?
- Cuando falle el segundo `fopen`, cual estilo mostrara mas informacion
  sobre que salio mal?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic compare_styles.c -o compare_styles
./compare_styles
```

Salida esperada:

```
=== comparing three styles ===

--- style 1: nested if-else ---
[nested] acquiring resources...
[nested] read from file1: line from file 1
[nested] read from file2: line from file 2
[nested] done (result: 0)

--- style 2: goto cleanup ---
[goto] acquiring resources...
[goto] read from file1: line from file 1
[goto] read from file2: line from file 2
[goto] done (result: 0)

--- style 3: cleanup attribute ---
[attr] acquiring resources...
[attr] read from file1: line from file 1
[attr] read from file2: line from file 2
[attr] done (result: 0)

--- testing error paths ---

[nested] with bad path:
[nested] acquiring resources...
[nested] fopen path2: No such file or directory
[nested] done (result: -1)

[goto] with bad path:
[goto] acquiring resources...
[goto] fopen path2: No such file or directory
[goto] done (result: -1)

[attr] with bad path:
[attr] acquiring resources...
[attr] fopen path2: No such file or directory
```

Nota: los mensajes de error (`perror`) van a stderr y pueden aparecer en orden
distinto al de stdout. Esto es normal.

Los tres estilos producen el mismo resultado funcional. La diferencia esta en
la **estructura del codigo**:

### Paso 3.3 — Contar niveles de indentacion

Observa cuantos niveles de indentacion necesita cada estilo para el "trabajo
real" (el `fgets`/`fputs`):

```bash
grep -n "actual work" compare_styles.c
```

Salida esperada:

```
24:        /* --- actual work --- */
79:    /* --- actual work --- */
113:    /* --- actual work --- */
```

- **nested**: linea 24, indentado 6 niveles (dentro de 5 if anidados)
- **goto**: linea 79, indentado 1 nivel (dentro de la funcion, sin anidamiento)
- **attr**: linea 113, indentado 1 nivel (igual que goto)

Con 5 recursos, el estilo anidado requiere 6 niveles de indentacion. Con 10
recursos, serian 11 niveles. El codigo se vuelve ilegible. Los estilos goto y
cleanup attribute escalan sin cambiar el nivel de indentacion.

### Paso 3.4 — Contar lineas por estilo

```bash
awk '/Style 1:/,/Style 2:/' compare_styles.c | wc -l
awk '/Style 2:/,/Style 3:/' compare_styles.c | wc -l
awk '/Style 3:/,/^int main/' compare_styles.c | wc -l
```

Observa que el estilo anidado necesita mas lineas para la misma funcionalidad,
y que el estilo cleanup attribute es el mas conciso.

### Paso 3.5 — Portabilidad

Una diferencia importante no visible en la ejecucion:

- `nested_style` y `goto_style` son **C estandar** — funcionan en cualquier
  compilador (GCC, Clang, MSVC, etc.)
- `cleanup_attr_style` usa `__attribute__((cleanup))` — **solo funciona con
  GCC y Clang**, no con MSVC

Para codigo que debe compilar en todas partes (como el kernel de Linux o
bibliotecas portables), goto cleanup es la opcion segura.

### Limpieza de la parte 3

```bash
rm -f compare_styles
```

---

## Limpieza final

```bash
rm -f goto_cleanup cleanup_attr compare_styles
rm -f *.s *.o
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  cleanup_attr.c  compare_styles.c  goto_cleanup.c
```

---

## Conceptos reforzados

1. El patron goto cleanup centraliza la liberacion de recursos en un unico
   label. Todas las variables se inicializan a NULL al inicio, y el label
   `cleanup:` las libera en orden inverso. `free(NULL)` es seguro, pero
   `fclose(NULL)` no — siempre verificar con `if`.

2. `__attribute__((cleanup(func)))` llama a `func(&variable)` automaticamente
   cuando la variable sale del scope. Es lo mas cercano a RAII que ofrece C,
   pero es una extension GCC/Clang, no C estandar.

3. Las funciones de cleanup para el atributo reciben un **puntero a la
   variable**, no el valor. Por eso `cleanup_free` recibe `void *p` y hace
   `free(*(void **)p)`.

4. Las macros `AUTO_FREE` y `AUTO_FCLOSE` mejoran la legibilidad, escondiendo
   la sintaxis verbose del atributo cleanup.

5. El estilo if-else anidado no escala: cada recurso adicional agrega un nivel
   de indentacion. Con 5 recursos, el codigo real esta a 6 niveles de
   profundidad. Los estilos goto y cleanup attribute mantienen el codigo plano.

6. goto cleanup es el patron preferido del kernel de Linux (mas de 300,000
   gotos en el codigo fuente) porque es portable, explicito y predecible.
   Usar `__attribute__((cleanup))` solo cuando la portabilidad a MSVC no es
   necesaria.

7. En orden inverso de liberacion: el ultimo recurso adquirido es el primero en
   liberarse. Esto evita problemas cuando un recurso depende de otro (por
   ejemplo, un buffer que se usa para escribir en un archivo).
