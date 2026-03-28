# Lab — Ventajas y desventajas del enlace estatico

## Objetivo

Comparar un ejecutable enlazado de forma habitual contra una version
completamente estatica para observar, con evidencia real, los beneficios y
costos del enlace estatico.

## Prerequisitos

- GCC instalado
- `ar`, `ldd`, `file` y `make` disponibles
- Estar en el directorio `lab_codex/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `portable_math.h` | API publica de la biblioteca |
| `portable_math.c` | Implementacion de funciones numericas |
| `main.c` | Programa cliente que imprime un reporte |
| `Makefile` | Build normal, build totalmente estatico y limpieza |

---

## Parte 1 — Construir la biblioteca base

**Objetivo**: Generar una biblioteca estatica que ambos binarios usaran.

### Paso 1.1 — Examinar la API y el programa cliente

```bash
cat portable_math.h
cat portable_math.c
cat main.c
```

Observa:

- `portable_math.c` compila a `portable_math.o`
- `main.c` llama funciones de la biblioteca
- el contenido del programa es pequeno; las diferencias de tamano luego vendran
  sobre todo del modo de enlace con las bibliotecas del sistema

### Paso 1.2 — Crear la biblioteca estatica

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -c portable_math.c -o portable_math.o
ar rcs libportablemath.a portable_math.o
```

### Paso 1.3 — Inspeccionar el archivo `.a`

```bash
ar t libportablemath.a
nm libportablemath.a
```

Debes ver el miembro `portable_math.o` y simbolos `T` como:

- `clamp_i32`
- `mean_i32`
- `sum_i32`

---

## Parte 2 — Binario con enlace habitual

**Objetivo**: Compilar un ejecutable que usa la biblioteca propia estatica pero
mantiene dependencias dinamicas del sistema.

### Paso 2.1 — Compilar el programa de forma normal

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -c main.c -o main.o
gcc main.o ./libportablemath.a -o report_default
```

### Paso 2.2 — Ejecutar el programa

```bash
./report_default
```

Salida esperada:

```text
sum: 81
mean: 13
clamped mean: 13
```

### Paso 2.3 — Medir tamano y dependencias

```bash
ls -lh report_default
file report_default
ldd report_default
```

Salida esperada aproximada:

```text
-rwxr-xr-x ... report_default  16K
report_default: ELF 64-bit LSB pie executable, x86-64, ...
linux-vdso.so.1 => ...
libc.so.6 => /lib/.../libc.so.6
...
```

Interpretacion:

- `libportablemath.a` no aparece en `ldd` porque ya fue copiada al binario
- el ejecutable sigue dependiendo de `libc.so.6`
- este es el caso comun cuando enlazas una biblioteca propia estatica pero no
  pides `-static` para todo el programa

---

## Parte 3 — Binario totalmente estatico

**Objetivo**: Construir una version sin dependencias dinamicas.

### Paso 3.1 — Compilar con `-static`

```bash
gcc -static main.o ./libportablemath.a -o report_static
```

Si tu entorno tiene instaladas las versiones estaticas de `libc`, el comando
debe completar correctamente. Si falla con `cannot find -lc` o mensajes
similares, eso significa que la toolchain del sistema no incluye enlazado
totalmente estatico; tambien es una observacion valida.

### Paso 3.2 — Medir tamano y tipo de archivo

```bash
ls -lh report_default report_static
file report_static
```

Salida esperada aproximada:

```text
report_default  ~16K
report_static   ~700K a ~2M

report_static: ELF 64-bit LSB executable, x86-64, statically linked, ...
```

Lo importante no es el numero exacto sino la diferencia de orden de magnitud.

### Paso 3.3 — Verificar dependencias

```bash
ldd report_static
```

Salida esperada:

```text
not a dynamic executable
```

Este mensaje resume la ventaja principal del enlace totalmente estatico: el
binario ya no necesita bibliotecas compartidas del host para arrancar.

### Paso 3.4 — Ejecutar la version estatica

```bash
./report_static
```

La salida funcional debe ser la misma que `report_default`.

---

## Parte 4 — Analizar los tradeoffs

**Objetivo**: Traducir observaciones tecnicas a decisiones practicas.

### Paso 4.1 — Comparar lado a lado

```bash
printf '\n== default ==\n'
ls -lh report_default
ldd report_default

printf '\n== static ==\n'
ls -lh report_static
ldd report_static
```

Usa esta comparacion para responder:

- Cual binario ocupa mas espacio en disco?
- Cual depende del host para encontrar `libc`?
- Cual seria mas facil copiar a un entorno minimo o de rescate?
- Cual recibiria automaticamente una actualizacion de seguridad de `libc` del
  sistema sin recompilar?

### Paso 4.2 — Relacionar evidencia con teoria

Relaciona lo que viste con estas afirmaciones:

1. El enlace estatico mejora la portabilidad del binario final.
2. El enlace estatico aumenta el tamano del ejecutable.
3. El enlace dinamico facilita que el sistema actualice bibliotecas compartidas.
4. Un binario totalmente estatico reduce dependencias externas, pero congela la
   version de las bibliotecas dentro del ejecutable.

### Paso 4.3 — Reproducir el build normal con `make`

```bash
make
```

El Makefile genera:

- `libportablemath.a`
- `report_default`

Luego puedes volver a ejecutar:

```bash
./report_default
```

Si quieres intentar la version totalmente estatica desde `make`, usa el target
dedicado:

```bash
make report_static
./report_static
```

### Paso 4.4 — Limpiar artefactos

```bash
make clean
```

---

## Limpieza final

```bash
make clean
rm -f portable_math.o main.o libportablemath.a report_default report_static
```

---

## Conceptos reforzados

1. Una biblioteca propia `.a` puede estar embebida en el ejecutable aunque el
   programa siga usando bibliotecas del sistema de forma dinamica.
2. `-static` cambia el modelo completo del binario, no solo el de una
   biblioteca concreta.
3. El binario totalmente estatico suele crecer mucho porque incorpora `libc` y
   otras dependencias del sistema.
4. `ldd` es la forma mas rapida de comprobar si un ejecutable depende de
   bibliotecas compartidas.
5. El enlace estatico favorece portabilidad y despliegue sencillo.
6. El enlace dinamico favorece ahorro de espacio y actualizaciones compartidas.
