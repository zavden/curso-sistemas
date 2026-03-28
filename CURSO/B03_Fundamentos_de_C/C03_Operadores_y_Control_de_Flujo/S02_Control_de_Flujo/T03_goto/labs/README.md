# Lab — goto

## Objetivo

Experimentar con `goto` en C: entender como funciona el salto incondicional, el
scope de los labels, y por que el patron de cleanup con goto es la forma
idiomatica de manejar errores en C. Al finalizar, sabras cuando goto es
legitimo y cuando es un anti-patron.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `goto_basic.c` | Salto incondicional basico con goto |
| `goto_scope.c` | Demuestra que los labels tienen function scope |
| `goto_cleanup.c` | Patron de cleanup centralizado con goto |
| `no_goto_cleanup.c` | Misma logica sin goto (para comparar) |

---

## Parte 1 — goto basico

**Objetivo**: Entender el salto incondicional y el scope de los labels.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat goto_basic.c
```

Observa la estructura:

- `printf("Step A")` se ejecuta normalmente
- `goto skip` salta a la etiqueta `skip:`
- `printf("Step B")` queda entre el goto y el label

Antes de compilar y ejecutar, predice:

- Que lineas se imprimiran?
- "Step B" aparecera en la salida?

### Paso 1.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic goto_basic.c -o goto_basic
./goto_basic
```

Salida esperada:

```
Step A
Step C
Done
```

"Step B" nunca se ejecuta. `goto skip` transfiere el control directamente a la
linea despues de `skip:`, saltando todo lo que hay en medio.

### Paso 1.3 — Scope de labels

```bash
cat goto_scope.c
```

Este programa tiene un label `inner:` definido **dentro de un bloque** `{ }`,
pero el `goto inner` esta **fuera** de ese bloque.

Antes de compilar, predice:

- Compilara sin errores? Los labels respetan el scope de bloque?
- Si compila, que imprimira?

### Paso 1.4 — Verificar la prediccion

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic goto_scope.c -o goto_scope
./goto_scope
```

Salida esperada:

```
Inside block (count=0)
Inside block (count=1)
Inside block (count=2)
Done (count=3)
```

Compila sin problemas. Los labels en C tienen **function scope**: son visibles
en toda la funcion, sin importar en que bloque `{ }` se definieron. Esto es
diferente de las variables, que tienen block scope.

Observa que el `goto inner` salta **hacia atras**, creando un loop. Esto
funciona pero es un anti-patron: para loops se deben usar `for` o `while`.
Aqui se muestra solo para demostrar que goto puede saltar en ambas direcciones.

### Limpieza de la parte 1

```bash
rm -f goto_basic goto_scope
```

---

## Parte 2 — Patron de cleanup con goto

**Objetivo**: Entender por que goto es la forma idiomatica de centralizar la
limpieza de recursos en C.

### Paso 2.1 — Examinar el codigo

```bash
cat goto_cleanup.c
```

Observa la estructura de `process_file()`:

1. Todas las variables de recursos se inicializan a `NULL` al inicio
2. Cada recurso se adquiere y se verifica; si falla, `goto cleanup`
3. El label `cleanup:` al final libera **todos** los recursos
4. `free(NULL)` es seguro (no hace nada), pero `fclose(NULL)` es UB

Antes de ejecutar, predice:

- En el test 1 (archivo valido), cuantos recursos se liberan en cleanup?
- En el test 2 (archivo inexistente), que recursos estaran en NULL cuando
  llegue a cleanup?

### Paso 2.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic goto_cleanup.c -o goto_cleanup
./goto_cleanup
```

Salida esperada:

```
=== Test 1: valid file ===
[OK] File opened: /tmp/goto_lab_test.txt
[OK] Buffer allocated (1024 bytes)
[OK] Data allocated (400 bytes)
[OK] Read first line: Hello from goto lab
[OK] Data filled: data[0]=0, data[99]=198
--- cleanup ---
  free(data)   [was allocated]
  free(buffer) [was allocated]
  fclose(f)    [was open]
Return: 0 (success)

=== Test 2: non-existent file ===
[FAIL] Cannot open '/tmp/nonexistent_xyz.txt'
--- cleanup ---
  free(data)   [was NULL]
  free(buffer) [was NULL]
  skip fclose  [was NULL]
Return: -1 (failure)
```

Puntos clave:

- **Test 1**: todos los recursos se adquirieron, el cleanup los libera todos
- **Test 2**: `fopen` fallo, asi que `goto cleanup` salta inmediatamente.
  `data` y `buffer` siguen en `NULL`. `free(NULL)` no hace nada. `fclose` se
  salta porque `f` es `NULL`

El **mismo bloque de cleanup** funciona para ambos casos. No importa en que
punto falle la funcion: el cleanup siempre es correcto porque las variables
empezaron en `NULL`.

### Paso 2.3 — El detalle de fclose(NULL)

La teoria menciona que `free(NULL)` es seguro (el estandar lo garantiza), pero
`fclose(NULL)` es comportamiento indefinido. Verifica esto en el codigo:

```bash
grep -n "fclose\|free" goto_cleanup.c
```

Observa que `free(data)` y `free(buffer)` se llaman sin verificar, pero
`fclose(f)` tiene una verificacion `if (f != NULL)`. Esta diferencia es
intencional y critica.

### Limpieza de la parte 2

```bash
rm -f goto_cleanup
```

---

## Parte 3 — goto vs alternativas

**Objetivo**: Comparar la version con goto contra la version sin goto para
evaluar la claridad y mantenibilidad de cada enfoque.

### Paso 3.1 — Compilar la version sin goto

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic no_goto_cleanup.c -o no_goto_cleanup
./no_goto_cleanup
```

Salida esperada:

```
=== Test 1: valid file ===
[OK] File opened: /tmp/goto_lab_test.txt
[OK] Buffer allocated (1024 bytes)
[OK] Data allocated (400 bytes)
[OK] Read first line: Hello from goto lab
[OK] Data filled: data[0]=0, data[99]=198
--- cleanup ---
  free(data)
  free(buffer)
  fclose(f)
Return: 0 (success)

=== Test 2: non-existent file ===
[FAIL] Cannot open '/tmp/nonexistent_xyz.txt'
Return: -1 (failure)
```

La salida funcional es la misma. La diferencia esta en la **estructura del
codigo**.

### Paso 3.2 — Comparar la estructura

```bash
wc -l goto_cleanup.c no_goto_cleanup.c
```

Salida esperada:

```
  97 goto_cleanup.c
  87 no_goto_cleanup.c
 184 total
```

La version sin goto tiene menos lineas, pero eso no cuenta toda la historia.
Observa los puntos de cleanup:

```bash
grep -n "cleanup\|free\|fclose" goto_cleanup.c
```

En la version con goto, `free` y `fclose` aparecen **una sola vez** en el
bloque `cleanup:`.

```bash
grep -n "cleanup\|free\|fclose" no_goto_cleanup.c
```

En la version sin goto, `free` y `fclose` se repiten en cada punto de error.
Si agregaras un cuarto recurso (por ejemplo, otro `malloc`), tendrias que:

- **Con goto**: agregar un `free()` al bloque cleanup (1 linea)
- **Sin goto**: agregar un `free()` en cada punto de error anterior (N lineas)

### Paso 3.3 — Visualizar las diferencias

```bash
diff goto_cleanup.c no_goto_cleanup.c
```

Observa las diferencias clave:

- La version con goto declara todas las variables al inicio como `NULL`
- La version sin goto declara cada variable justo antes de usarla
- La version con goto tiene un unico bloque `cleanup:` al final
- La version sin goto repite el cleanup en cada `return -1`

### Paso 3.4 — Reflexion

Piensa en estas preguntas:

- Si agregas un quinto recurso, cual version es mas facil de modificar?
- Si olvidas un `free()` en algun punto de error, en cual version es mas
  probable que eso ocurra?
- El kernel de Linux (millones de lineas de C) usa el patron goto-cleanup
  extensivamente. Por que crees que eligieron ese patron?

### Limpieza de la parte 3

```bash
rm -f no_goto_cleanup
```

---

## Limpieza final

```bash
rm -f goto_basic goto_scope goto_cleanup no_goto_cleanup
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  goto_basic.c  goto_cleanup.c  goto_scope.c  no_goto_cleanup.c
```

---

## Conceptos reforzados

1. `goto` transfiere el control incondicionalmente a un label dentro de la
   misma funcion. El codigo entre el `goto` y el label se salta por completo.

2. Los labels tienen **function scope**: son visibles en toda la funcion, sin
   importar en que bloque `{ }` se definieron. Esto es diferente del block
   scope de las variables.

3. El patron goto-cleanup centraliza la liberacion de recursos en un unico
   punto. Al inicializar todos los punteros a `NULL`, el mismo bloque de
   cleanup funciona sin importar en que paso fallo la funcion.

4. `free(NULL)` es seguro (el estandar C lo define como no-op), pero
   `fclose(NULL)` es comportamiento indefinido. Por eso el cleanup debe
   verificar `if (f != NULL)` antes de llamar a `fclose`.

5. La version sin goto duplica el codigo de cleanup en cada punto de error.
   Con 3 recursos, la duplicacion es manejable. Con 5 o 10 recursos (comun en
   codigo del kernel), el patron goto-cleanup es claramente superior en
   claridad y mantenibilidad.

6. goto es legitimo para cleanup y error handling. Usarlo para crear loops o
   como jump table es un anti-patron: para eso existen `for`, `while` y
   `switch`.
