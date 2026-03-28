# Lab — Creacion de bibliotecas estaticas

## Objetivo

Construir una biblioteca estatica `libvec3.a` a partir de multiples archivos
objeto, inspeccionar su contenido con herramientas de bajo nivel, enlazarla
desde un programa cliente y repetir el proceso con `make`.

## Prerequisitos

- GCC instalado
- `ar`, `nm`, `file` y `make` disponibles
- Estar en el directorio `lab_codex/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `vec3.h` | API publica de la biblioteca |
| `vec3.c` | Operaciones `create`, `add`, `dot`, `length` |
| `vec3_io.c` | Impresion del vector y reporte de magnitud |
| `main.c` | Programa que usa la biblioteca |
| `Makefile` | Reproduce el flujo de compilacion y limpieza |

---

## Parte 1 — Examinar la estructura del proyecto

**Objetivo**: Ubicar la API publica, la implementacion y el programa cliente.

### Paso 1.1 — Examinar el header publico

```bash
cat vec3.h
```

Observa:

- El header define el tipo `Vec3`.
- Los prototipos declaran la API publica.
- No hay definiciones de funciones en el header.

### Paso 1.2 — Examinar la implementacion principal

```bash
cat vec3.c
```

Observa:

- `vec3.c` contiene operaciones matematicas.
- Produce un objeto relocatable independiente (`vec3.o`).
- `vec3_length` depende de `sqrt`, por eso luego se usara `-lm`.

### Paso 1.3 — Examinar la segunda unidad de compilacion

```bash
cat vec3_io.c
```

Observa:

- La biblioteca no tiene que venir de un solo `.c`.
- `vec3_io.c` aporta simbolos adicionales al archivo `.a`.

### Paso 1.4 — Examinar el programa cliente

```bash
cat main.c
```

Antes de continuar, responde mentalmente:

- Que archivos `.o` se generaran si compilas cada `.c` con `-c`?
- Que archivo deberia contener el codigo de `vec3_print`?
- El programa final necesitara tener `libvec3.a` disponible en runtime?

Intenta predecir antes de continuar al siguiente paso.

---

## Parte 2 — Compilar a archivos objeto

**Objetivo**: Generar `.o` y comprobar que todavia no son ejecutables.

### Paso 2.1 — Compilar cada unidad por separado

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -c vec3.c -o vec3.o
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -c vec3_io.c -o vec3_io.o
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -c main.c -o main.o
```

Cada comando usa `-c`, asi que GCC compila pero no enlaza.

### Paso 2.2 — Verificar el tipo de archivo

```bash
file vec3.o
file vec3_io.o
file main.o
```

Salida esperada aproximada:

```text
vec3.o: ELF 64-bit LSB relocatable, x86-64, ...
vec3_io.o: ELF 64-bit LSB relocatable, x86-64, ...
main.o: ELF 64-bit LSB relocatable, x86-64, ...
```

La palabra clave es `relocatable`: el linker aun no asigna direcciones finales.

### Paso 2.3 — Inspeccionar simbolos con `nm`

```bash
nm vec3.o
nm vec3_io.o
nm main.o
```

Salida esperada aproximada:

```text
vec3.o:
T vec3_add
T vec3_create
T vec3_dot
T vec3_length
U sqrt

vec3_io.o:
T vec3_print
T vec3_print_report
U printf

main.o:
T main
U vec3_add
U vec3_create
U vec3_length
U vec3_print
U vec3_print_report
```

Interpretacion:

- `T` = simbolo definido en la seccion de codigo.
- `U` = simbolo indefinido que otro objeto o biblioteca debe resolver.

### Paso 2.4 — Verificar que `main.o` no se puede ejecutar

```bash
./main.o
```

Salida esperada aproximada:

```text
bash: ./main.o: cannot execute binary file: Exec format error
```

Un `.o` no es un ejecutable completo. Solo es una pieza para el enlazado.

---

## Parte 3 — Crear la biblioteca estatica con `ar`

**Objetivo**: Empaquetar varios objetos en `libvec3.a` e inspeccionar su
contenido interno.

### Paso 3.1 — Crear el archivo `.a`

```bash
ar rcs libvec3.a vec3.o vec3_io.o
```

Recordatorio:

- `r` inserta o reemplaza miembros
- `c` crea el archivo si no existe
- `s` genera el indice de simbolos

### Paso 3.2 — Listar los miembros del archivo

```bash
ar t libvec3.a
```

Salida esperada:

```text
vec3.o
vec3_io.o
```

La biblioteca estatica no es magia: es un contenedor de objetos.

### Paso 3.3 — Inspeccionar simbolos del `.a`

```bash
nm libvec3.a
```

Busca estas entradas:

- `vec3_create`
- `vec3_add`
- `vec3_length`
- `vec3_print`
- `vec3_print_report`

Observa que `sqrt` y `printf` siguen apareciendo como `U` porque pertenecen a
otras bibliotecas del sistema.

### Paso 3.4 — Ejecutar `ranlib` manualmente

```bash
ranlib libvec3.a
```

En GNU `ar`, `rcs` ya hace este trabajo. Ejecutarlo otra vez no debe romper
nada; solo actualiza el indice.

### Paso 3.5 — Ver el tamano de la biblioteca

```bash
ls -lh libvec3.a
```

El archivo suele ser pequeno porque solo contiene dos objetos y no una copia
de `libm` ni de `libc`.

---

## Parte 4 — Enlazar y ejecutar el programa cliente

**Objetivo**: Usar `libvec3.a` desde un programa real.

### Paso 4.1 — Enlazar contra la biblioteca

```bash
gcc main.o -L. -lvec3 -lm -o demo_vec3
```

Explicacion:

- `-L.` agrega el directorio actual al path de bibliotecas
- `-lvec3` busca `libvec3.a` o `libvec3.so`
- `-lm` resuelve `sqrt`

### Paso 4.2 — Ejecutar el binario

```bash
./demo_vec3
```

Salida esperada aproximada:

```text
sum = (5.00, 7.00, 9.00)
sum length = 12.45
report: x=5.00 y=7.00 z=9.00 | len=12.45
```

### Paso 4.3 — Comprobar dependencias dinamicas

```bash
ldd demo_vec3
```

Salida esperada aproximada:

```text
linux-vdso.so.1 => ...
libm.so.6 => /lib/.../libm.so.6
libc.so.6 => /lib/.../libc.so.6
...
```

Observa algo importante: `libvec3.a` no aparece en `ldd`.

Eso significa que el codigo de `vec3` fue copiado dentro del ejecutable en
tiempo de enlace. La biblioteca estatica no se necesita en runtime.

### Paso 4.4 — Verificar que los simbolos quedaron dentro del binario

```bash
nm demo_vec3 | grep vec3
```

Debes ver simbolos `T vec3_*` dentro del ejecutable final.

### Limpieza de la parte 4

```bash
rm -f demo_vec3
```

---

## Parte 5 — Repetir el flujo con `make`

**Objetivo**: Automatizar compilacion, archivo y limpieza.

### Paso 5.1 — Examinar el Makefile

```bash
cat Makefile
```

Observa:

- `libvec3.a` depende de `vec3.o` y `vec3_io.o`
- `demo_vec3` depende de `main.o` y `libvec3.a`
- `clean` elimina artefactos

### Paso 5.2 — Compilar todo con `make`

```bash
make
```

Debe generar:

- `vec3.o`
- `vec3_io.o`
- `main.o`
- `libvec3.a`
- `demo_vec3`

### Paso 5.3 — Ejecutar el programa

```bash
./demo_vec3
```

La salida debe coincidir con la parte 4.

### Paso 5.4 — Forzar recompilacion limpia

```bash
make clean
make
```

Observa que `clean` deja el directorio en estado inicial y `make` vuelve a
reconstruir todo.

---

## Limpieza final

```bash
make clean
rm -f libvec3.a demo_vec3 main.o vec3.o vec3_io.o
```

---

## Conceptos reforzados

1. Una biblioteca estatica `.a` es un contenedor de archivos objeto, no un
   formato magico independiente.
2. `gcc -c` genera objetos relocatables que todavia no son ejecutables.
3. `ar rcs` empaqueta varios `.o` y crea el indice de simbolos.
4. `nm` permite distinguir simbolos definidos (`T`) y pendientes (`U`).
5. Al enlazar con una biblioteca estatica, el codigo necesario queda copiado
   dentro del ejecutable final.
6. `ldd` muestra dependencias dinamicas, por eso no lista `libvec3.a`.

