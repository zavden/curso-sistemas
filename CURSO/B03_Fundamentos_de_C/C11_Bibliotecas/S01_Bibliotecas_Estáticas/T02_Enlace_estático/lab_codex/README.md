# Lab — Enlace estatico

## Objetivo

Entender como resuelve simbolos el linker cuando trabaja con bibliotecas
estaticas, por que el orden de `-l` importa, cuando conviene usar `-L` y
cuando una ruta directa al archivo `.a`, y como diagnosticar errores de enlace.

## Prerequisitos

- GCC instalado
- `ar`, `nm` y `ldd` disponibles
- Estar en el directorio `lab_codex/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `format.h` | API para formatear nombres |
| `format.c` | Define `format_name` |
| `greet.h` | API publica de saludos |
| `greet.c` | Define `greet_person` y llama a `format_name` |
| `greet_duplicate.c` | Segunda definicion de `greet_person` para provocar conflicto |
| `main.c` | Programa cliente que usa `greet_person` |

---

## Parte 1 — Construir y observar las bibliotecas

**Objetivo**: Generar dos bibliotecas con dependencia entre si.

### Paso 1.1 — Examinar la cadena de dependencias

```bash
cat format.h
cat greet.h
cat greet.c
cat main.c
```

Observa la relacion:

- `main.c` llama a `greet_person`
- `greet.c` define `greet_person`
- `greet.c` llama a `format_name`
- `format.c` define `format_name`

Esto crea la cadena `main.o -> libgreet.a -> libformat.a`.

### Paso 1.2 — Compilar los objetos

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -c format.c -o format.o
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -c greet.c -o greet.o
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -c main.c -o main.o
```

### Paso 1.3 — Crear las bibliotecas

```bash
ar rcs libformat.a format.o
ar rcs libgreet.a greet.o
```

### Paso 1.4 — Inspeccionar simbolos

```bash
nm libformat.a
nm libgreet.a
nm main.o
```

Busca este patron:

- `main.o` tiene `U greet_person`
- `libgreet.a` tiene `T greet_person` y `U format_name`
- `libformat.a` tiene `T format_name`

Antes de continuar, responde mentalmente:

- Si el linker procesa `libformat.a` antes de saber que necesita
  `format_name`, incluira `format.o` o lo ignorara?
- Que deberia ir mas a la izquierda: `main.o` o las bibliotecas?

Intenta predecir antes de continuar al siguiente paso.

---

## Parte 2 — Enlace correcto

**Objetivo**: Resolver todos los simbolos con el orden correcto.

### Paso 2.1 — Enlazar en el orden correcto

```bash
gcc main.o -L. -lgreet -lformat -o greet_demo
```

Explicacion:

- `main.o` introduce la necesidad de `greet_person`
- `-lgreet` satisface esa necesidad, pero introduce `format_name`
- `-lformat` llega despues y resuelve `format_name`

### Paso 2.2 — Ejecutar el programa

```bash
./greet_demo
```

Salida esperada:

```text
Hello, Dr. Ada Lovelace!
```

### Paso 2.3 — Comprobar dependencias dinamicas

```bash
ldd greet_demo
```

Observa que `libgreet.a` y `libformat.a` no aparecen en la salida. El codigo ya
quedo dentro del binario final.

### Paso 2.4 — Verificar los simbolos incorporados

```bash
nm greet_demo | grep -E 'greet_person|format_name'
```

Debes ver ambos simbolos definidos dentro del ejecutable.

---

## Parte 3 — El orden de los argumentos importa

**Objetivo**: Ver fallos reales cuando el linker procesa las bibliotecas en el
orden equivocado.

### Paso 3.1 — Probar el orden incorrecto

```bash
gcc main.o -L. -lformat -lgreet -o greet_demo_wrong
```

Salida esperada aproximada:

```text
/usr/bin/ld: ./libgreet.a(greet.o): in function `greet_person':
greet.c:(.text+0x...): undefined reference to `format_name'
collect2: error: ld returned 1 exit status
```

Por que falla:

- Cuando el linker ve `-lformat`, aun no existe ninguna referencia pendiente a
  `format_name`, asi que no extrae `format.o`.
- Luego ve `-lgreet`, extrae `greet.o` para resolver `greet_person`.
- `greet.o` introduce ahora `format_name`, pero `libformat.a` ya paso.

### Paso 3.2 — Probar bibliotecas antes del objeto principal

```bash
gcc -L. -lgreet -lformat main.o -o greet_demo_wrong_2
```

Salida esperada aproximada:

```text
/usr/bin/ld: main.o: in function `main':
main.c:(.text+0x...): undefined reference to `greet_person'
collect2: error: ld returned 1 exit status
```

Aqui el problema es todavia mas temprano: las bibliotecas se procesaron antes
de que `main.o` declarara que necesitaba `greet_person`.

### Paso 3.3 — Volver al orden correcto

```bash
gcc main.o -L. -lgreet -lformat -o greet_demo
./greet_demo
```

Usa este paso para fijar mentalmente la regla:

`objeto que usa -> biblioteca que resuelve -> dependencia siguiente`

---

## Parte 4 — `-L` y `-l` vs ruta directa al `.a`

**Objetivo**: Comparar el enlace por busqueda automatica contra el enlace por
ruta exacta.

### Paso 4.1 — Enlazar con rutas directas

```bash
gcc main.o ./libgreet.a ./libformat.a -o greet_demo_direct
```

Esta variante no necesita `-L` ni `-l` porque le entregas al linker los
archivos exactos.

### Paso 4.2 — Ejecutar el binario directo

```bash
./greet_demo_direct
```

La salida debe ser igual a `greet_demo`.

### Paso 4.3 — Observar la diferencia conceptual

- `-lfoo` busca `libfoo.a` o `libfoo.so` en directorios conocidos
- `./libfoo.a` fuerza usar ese archivo exacto
- En ambos casos, el orden sigue importando

### Paso 4.4 — Confirmar que el orden tambien importa con rutas

```bash
gcc main.o ./libformat.a ./libgreet.a -o greet_demo_direct_wrong
```

Debe fallar con un `undefined reference to format_name` similar al de la parte
3. El problema no es `-l`; el problema es el orden de procesamiento.

---

## Parte 5 — Diagnosticar errores tipicos de linker

**Objetivo**: Provocar errores reales y leerlos de forma sistematica.

### Paso 5.1 — Error: falta `-L.`

```bash
gcc main.o -lgreet -lformat -o greet_demo_nopath
```

Salida esperada aproximada:

```text
/usr/bin/ld: cannot find -lgreet: No such file or directory
/usr/bin/ld: cannot find -lformat: No such file or directory
collect2: error: ld returned 1 exit status
```

Interpretacion:

- `-l` no busca en el directorio actual por defecto
- `-L.` agrega el directorio actual al path de busqueda

### Paso 5.2 — Error: falta una dependencia

```bash
gcc main.o -L. -lgreet -o greet_demo_missing_dep
```

Salida esperada aproximada:

```text
/usr/bin/ld: ./libgreet.a(greet.o): in function `greet_person':
greet.c:(.text+0x...): undefined reference to `format_name'
collect2: error: ld returned 1 exit status
```

La biblioteca `libgreet.a` existe, pero depende de otra que no se paso al
linker.

### Paso 5.3 — Error: multiple definition

Primero compila el objeto duplicado:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -c greet_duplicate.c -o greet_duplicate.o
```

Ahora enlaza el objeto duplicado despues de `libgreet.a`:

```bash
gcc main.o -L. -lgreet -lformat greet_duplicate.o -o greet_demo_duplicate
```

Salida esperada aproximada:

```text
/usr/bin/ld: greet_duplicate.o: in function `greet_person':
greet_duplicate.c:(.text+0x...): multiple definition of `greet_person';
./libgreet.a(greet.o):greet.c:(.text+0x...): first defined here
collect2: error: ld returned 1 exit status
```

Interpretacion:

- `main.o` obliga al linker a extraer `greet.o` desde `libgreet.a`
- despues aparece `greet_duplicate.o` con otra definicion global del mismo
  simbolo
- el linker rechaza el binario

### Paso 5.4 — Usar `nm -A` para investigar

```bash
nm -A libgreet.a libformat.a greet_duplicate.o | grep -E 'greet_person|format_name'
```

Este comando te ayuda a responder:

- Donde esta definido cada simbolo?
- Quien lo necesita?
- Hay dos definiciones globales del mismo nombre?

### Limpieza de la parte 5

```bash
rm -f greet_duplicate.o
```

---

## Limpieza final

```bash
rm -f *.o *.a greet_demo greet_demo_direct
rm -f greet_demo_wrong greet_demo_wrong_2 greet_demo_direct_wrong
rm -f greet_demo_nopath greet_demo_missing_dep greet_demo_duplicate
```

---

## Conceptos reforzados

1. El linker procesa archivos y bibliotecas de izquierda a derecha.
2. Una biblioteca estatica solo aporta objetos cuando ya existe una referencia
   pendiente a sus simbolos.
3. `-L` agrega directorios de busqueda; `-lfoo` busca por nombre;
   `./libfoo.a` fuerza un archivo exacto.
4. El orden incorrecto causa `undefined reference` aunque los simbolos existan.
5. `nm` y `nm -A` sirven para ubicar definiciones y dependencias reales.
6. `cannot find`, `undefined reference` y `multiple definition` describen
   problemas distintos y deben diagnosticarse de forma diferente.

