# Lab -- Dependencias automaticas

## Objetivo

Demostrar el bug silencioso que ocurre cuando un Makefile no declara los headers
como dependencias, usar GCC para generar dependencias automaticamente con
`-MMD -MP`, y verificar que Make recompila exactamente los .o afectados ante
cada cambio de header. Al finalizar, sabras implementar el patron estandar de
dependencias automaticas y entenderas por que `-MP` es necesario para sobrevivir
la eliminacion de headers.

## Prerequisitos

- GCC y GNU Make instalados
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `config.h` | Header con APP_NAME y VERSION |
| `logger.h` | Header con declaracion de log_message() |
| `logger.c` | Implementacion de log_message(); incluye logger.h y config.h |
| `engine.h` | Header con declaracion de run_engine() |
| `engine.c` | Implementacion de run_engine(); incluye engine.h y logger.h |
| `main.c` | Programa principal; incluye config.h y engine.h |
| `Makefile.broken` | Makefile SIN dependencias automaticas (tiene el bug) |
| `Makefile.fixed` | Makefile CON dependencias automaticas (-MMD -MP) |

Grafo de dependencias de headers:

```
main.c    --> config.h, engine.h
engine.c  --> engine.h, logger.h
logger.c  --> logger.h, config.h
```

---

## Parte 1 -- El bug de headers

**Objetivo**: Demostrar que un Makefile con la pattern rule `%.o: %.c` no
detecta cambios en los headers. Modificar un .h y ver que Make dice "up to
date" cuando no deberia.

### Paso 1.1 -- Examinar el Makefile con el bug

```bash
cat Makefile.broken
```

Observa la pattern rule:

```makefile
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
```

El unico prerequisite de cada `.o` es su `.c`. Los headers no aparecen en
ningun lado. Make no sabe que `main.o` depende de `config.h` y `engine.h`.

### Paso 1.2 -- Compilar con el Makefile roto

```bash
make -f Makefile.broken
```

Salida esperada:

```
gcc -Wall -Wextra -std=c11 -c -o main.o main.c
gcc -Wall -Wextra -std=c11 -c -o engine.o engine.c
gcc -Wall -Wextra -std=c11 -c -o logger.o logger.c
gcc -o app main.o engine.o logger.o
```

Verifica que el programa funciona:

```bash
./app
```

Salida esperada:

```
Starting AutoDeps Demo v1
[AutoDeps Demo v1] Engine started
[AutoDeps Demo v1] Processing...
[AutoDeps Demo v1] Engine stopped
```

### Paso 1.3 -- Predecir el efecto de cambiar un header

Ahora vas a simular un cambio en `config.h` (por ejemplo, cambiar VERSION de
1 a 2). Antes de hacerlo, responde mentalmente:

- `main.c` incluye `config.h`. Se recompilara `main.o`?
- `logger.c` incluye `config.h`. Se recompilara `logger.o`?
- Que dira Make al ejecutar `make`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.4 -- Simular un cambio en config.h

```bash
touch config.h
make -f Makefile.broken
```

Salida esperada:

```
make: 'app' is up to date.
```

Este es el bug. `config.h` cambio (tiene un timestamp mas nuevo que los .o),
pero Make no lo sabe porque la regla `%.o: %.c` solo mira el `.c`. El
programa sigue usando el valor viejo de VERSION.

### Paso 1.5 -- Confirmar que el bug es real

Para demostrar que no es un falso positivo, toca un `.c`:

```bash
touch main.c
make -f Makefile.broken
```

Salida esperada:

```
gcc -Wall -Wextra -std=c11 -c -o main.o main.c
gcc -o app main.o engine.o logger.o
```

Make si detecta cambios en `.c` porque el `.c` esta en los prerequisites.
El problema es exclusivo de los headers.

### Limpieza de la parte 1

```bash
make -f Makefile.broken clean
```

---

## Parte 2 -- gcc -MM: ver las dependencias

**Objetivo**: Usar `gcc -MM` para que GCC analice los `#include` de cada `.c`
y muestre la lista de dependencias en formato Makefile.

### Paso 2.1 -- Generar dependencias de main.c

```bash
gcc -MM main.c
```

Salida esperada:

```
main.o: main.c config.h engine.h
```

GCC analizo los `#include` de `main.c` y listo todos los headers locales.
La salida es un fragmento de Makefile valido: dice que `main.o` depende de
`main.c`, `config.h` y `engine.h`.

### Paso 2.2 -- Comparar con gcc -M (incluye headers del sistema)

```bash
gcc -M main.c
```

Salida esperada (simplificada):

```
main.o: main.c /usr/include/stdio.h /usr/include/x86_64-linux-gnu/... \
 config.h engine.h
```

`-M` incluye los headers del sistema (los de `< >`). `-MM` los excluye.
Para dependencias de Makefile se usa `-MM` porque los headers del sistema
no cambian durante el desarrollo.

### Paso 2.3 -- Generar dependencias de los otros archivos

```bash
gcc -MM engine.c
```

Salida esperada:

```
engine.o: engine.c engine.h logger.h
```

```bash
gcc -MM logger.c
```

Salida esperada:

```
logger.o: logger.c logger.h config.h
```

Observa como cada archivo tiene dependencias distintas. Estas son exactamente
las lineas que habria que agregar al Makefile manualmente. GCC las genera
por ti.

---

## Parte 3 -- -MMD -MP y -include: el patron automatico

**Objetivo**: Usar `-MMD -MP` para generar archivos `.d` como efecto secundario
de la compilacion, e incluirlos en el Makefile con `-include`.

### Paso 3.1 -- Examinar el Makefile corregido

```bash
cat Makefile.fixed
```

Observa las diferencias clave respecto a `Makefile.broken`:

1. `DEPFLAGS = -MMD -MP` -- flags para generar dependencias
2. `DEPS = $(OBJS:.o=.d)` -- lista de archivos .d
3. `$(DEPFLAGS)` en la recipe de compilacion
4. `-include $(DEPS)` al final -- incluye los .d si existen
5. `$(DEPS)` en la regla `clean` -- limpia los .d

### Paso 3.2 -- Predecir la primera compilacion

Antes de compilar, responde mentalmente:

- En la primera compilacion, los archivos `.d` existen?
- Si no existen, `-include $(DEPS)` dara error o los ignorara?
- Despues de compilar, que archivos nuevos apareceran ademas de los `.o`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.3 -- Compilar con el Makefile corregido

```bash
make -f Makefile.fixed
```

Salida esperada:

```
gcc -Wall -Wextra -std=c11 -MMD -MP -c -o main.o main.c
gcc -Wall -Wextra -std=c11 -MMD -MP -c -o engine.o engine.c
gcc -Wall -Wextra -std=c11 -MMD -MP -c -o logger.o logger.c
gcc -o app main.o engine.o logger.o
```

Observa que cada invocacion de gcc incluye `-MMD -MP`. Estos flags generan
los archivos `.d` como efecto secundario -- la compilacion produce tanto el
`.o` como el `.d`.

### Paso 3.4 -- Verificar que se generaron los .d

```bash
ls *.d
```

Salida esperada:

```
engine.d  logger.d  main.d
```

Cada `.c` produjo un `.d` ademas de su `.o`.

### Paso 3.5 -- Verificar que el programa funciona

```bash
./app
```

Salida esperada:

```
Starting AutoDeps Demo v1
[AutoDeps Demo v1] Engine started
[AutoDeps Demo v1] Processing...
[AutoDeps Demo v1] Engine stopped
```

---

## Parte 4 -- Verificar recompilacion selectiva

**Objetivo**: Comprobar que con dependencias automaticas, `touch` sobre un
header recompila exactamente los `.o` que dependen de el y no los demas.

### Paso 4.1 -- Predecir el efecto de cambiar config.h

Antes de tocar `config.h`, responde mentalmente mirando el grafo de
dependencias:

```
main.c    --> config.h, engine.h
engine.c  --> engine.h, logger.h
logger.c  --> logger.h, config.h
```

- Que archivos `.o` deberan recompilarse al tocar `config.h`?
- Que archivo `.o` NO debera recompilarse?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.2 -- Tocar config.h y recompilar

```bash
touch config.h
make -f Makefile.fixed
```

Salida esperada:

```
gcc -Wall -Wextra -std=c11 -MMD -MP -c -o main.o main.c
gcc -Wall -Wextra -std=c11 -MMD -MP -c -o logger.o logger.c
gcc -o app main.o engine.o logger.o
```

Resultado:

- `main.o` -- recompilado (main.c incluye config.h)
- `logger.o` -- recompilado (logger.c incluye config.h)
- `engine.o` -- NO recompilado (engine.c no incluye config.h)
- `app` -- re-enlazado (main.o y logger.o cambiaron)

Compara esto con la parte 1 donde Make decia "up to date". Ahora detecta
correctamente los cambios en headers.

### Paso 4.3 -- Tocar logger.h y recompilar

```bash
touch logger.h
make -f Makefile.fixed
```

Salida esperada:

```
gcc -Wall -Wextra -std=c11 -MMD -MP -c -o engine.o engine.c
gcc -Wall -Wextra -std=c11 -MMD -MP -c -o logger.o logger.c
gcc -o app main.o engine.o logger.o
```

Resultado:

- `engine.o` -- recompilado (engine.c incluye logger.h)
- `logger.o` -- recompilado (logger.c incluye logger.h)
- `main.o` -- NO recompilado (main.c no incluye logger.h)

### Paso 4.4 -- Tocar engine.h y recompilar

```bash
touch engine.h
make -f Makefile.fixed
```

Salida esperada:

```
gcc -Wall -Wextra -std=c11 -MMD -MP -c -o main.o main.c
gcc -Wall -Wextra -std=c11 -MMD -MP -c -o engine.o engine.c
gcc -o app main.o engine.o logger.o
```

Resultado:

- `main.o` -- recompilado (main.c incluye engine.h)
- `engine.o` -- recompilado (engine.c incluye engine.h)
- `logger.o` -- NO recompilado (logger.c no incluye engine.h)

### Paso 4.5 -- Verificar que sin cambios no recompila

```bash
make -f Makefile.fixed
```

Salida esperada:

```
make: 'app' is up to date.
```

Sin cambios en ningun archivo fuente ni header, Make no recompila nada.

---

## Parte 5 -- -MP ante headers eliminados

**Objetivo**: Demostrar que `-MP` genera phony targets para cada header en el
archivo `.d`, evitando un error de Make cuando se elimina un header del
proyecto.

### Paso 5.1 -- Crear un header temporal

Agrega un header `debug.h` que se usara temporalmente:

```bash
cat > debug.h << 'EOF'
#ifndef DEBUG_H
#define DEBUG_H

#define DEBUG_PRINT(x) printf("DBG: %d\n", (x))

#endif
EOF
```

### Paso 5.2 -- Agregar el include a main.c

Edita `main.c` para incluir `debug.h`. Agrega la linea `#include "debug.h"`
despues de los otros includes y una llamada a `DEBUG_PRINT` en main:

```bash
cat > main.c << 'EOF'
#include <stdio.h>
#include "config.h"
#include "engine.h"
#include "debug.h"

int main(void) {
    printf("Starting %s v%d\n", APP_NAME, VERSION);
    DEBUG_PRINT(42);
    run_engine();
    return 0;
}
EOF
```

### Paso 5.3 -- Compilar con debug.h incluido

```bash
make -f Makefile.fixed
```

Salida esperada:

```
gcc -Wall -Wextra -std=c11 -MMD -MP -c -o main.o main.c
gcc -o app main.o engine.o logger.o
```

Verifica que `main.d` ahora incluye `debug.h`:

```bash
cat main.d
```

Salida esperada:

```
main.o: main.c config.h engine.h debug.h
config.h:
engine.h:
debug.h:
```

Observa las lineas `config.h:`, `engine.h:` y `debug.h:` al final. Son los
**phony targets** que genera `-MP`. Cada header tiene una regla vacia que
dice "si este archivo no existe, no hagas nada" en lugar de causar un error.

### Paso 5.4 -- Predecir que pasa al eliminar debug.h

Ahora vas a eliminar `debug.h` del proyecto y quitar su `#include` de main.c.
Antes de hacerlo, responde mentalmente:

- El archivo `main.d` todavia dice que `main.o` depende de `debug.h`.
- Si `debug.h` no existe y no hay regla para crearlo, que haria Make?
- `-MP` genero la linea `debug.h:` en main.d. Que efecto tiene eso?

Intenta predecir antes de continuar al siguiente paso.

### Paso 5.5 -- Eliminar debug.h y quitar el include

```bash
rm debug.h
```

Restaura `main.c` sin el include de debug.h:

```bash
cat > main.c << 'EOF'
#include <stdio.h>
#include "config.h"
#include "engine.h"

int main(void) {
    printf("Starting %s v%d\n", APP_NAME, VERSION);
    run_engine();
    return 0;
}
EOF
```

### Paso 5.6 -- Compilar despues de eliminar el header

```bash
make -f Makefile.fixed
```

Salida esperada:

```
gcc -Wall -Wextra -std=c11 -MMD -MP -c -o main.o main.c
gcc -o app main.o engine.o logger.o
```

Funciona sin errores. El archivo `main.d` viejo contenia `debug.h` como
dependencia, pero la linea `debug.h:` (generada por `-MP`) actuo como phony
target: Make vio que `debug.h` no existe, encontro la regla vacia, y la
considero satisfecha. Al recompilar `main.o`, se genera un `main.d` nuevo
que ya no incluye `debug.h`.

### Paso 5.7 -- Verificar el .d actualizado

```bash
cat main.d
```

Salida esperada:

```
main.o: main.c config.h engine.h
config.h:
engine.h:
```

`debug.h` desaparecio del .d. Las dependencias se actualizaron
automaticamente.

### Paso 5.8 -- Demostrar el error sin -MP

Para ver lo que pasaria sin `-MP`, simula la situacion manualmente. Crea un
`.d` que referencie un header inexistente, pero sin el phony target:

```bash
cat > test_nomp.d << 'EOF'
main.o: main.c config.h engine.h ghost.h
EOF
```

Intenta incluirlo con make:

```bash
make -f Makefile.fixed -f test_nomp.d main.o
```

Salida esperada:

```
make: *** No rule to make target 'ghost.h', needed by 'main.o'.  Stop.
```

Sin la linea `ghost.h:` que `-MP` habria generado, Make no sabe como obtener
`ghost.h` y falla. Esto es exactamente lo que pasaria si usaras `-MMD` sin
`-MP` y luego borraras un header.

```bash
rm test_nomp.d
```

---

## Parte 6 -- Inspeccion de archivos .d

**Objetivo**: Examinar en detalle el contenido de cada archivo `.d` generado
para entender su formato y como Make los usa.

### Paso 6.1 -- Asegurar una compilacion limpia

```bash
make -f Makefile.fixed clean
make -f Makefile.fixed
```

### Paso 6.2 -- Examinar todos los .d

```bash
cat main.d
```

Salida esperada:

```
main.o: main.c config.h engine.h
config.h:
engine.h:
```

```bash
cat engine.d
```

Salida esperada:

```
engine.o: engine.c engine.h logger.h
engine.h:
logger.h:
```

```bash
cat logger.d
```

Salida esperada:

```
logger.o: logger.c logger.h config.h
logger.h:
config.h:
```

### Paso 6.3 -- Interpretar el formato

Cada archivo `.d` tiene dos tipos de lineas:

1. **Linea de dependencias**: `target: prerequisites` -- le dice a Make que
   el `.o` depende del `.c` y de cada header. Es una regla sin recipe.

2. **Phony targets**: `header.h:` -- reglas vacias generadas por `-MP`. Si
   el header desaparece, Make encuentra esta regla y no falla.

Cuando Make encuentra `-include $(DEPS)`, lee estos archivos y los trata como
si su contenido estuviera escrito directamente en el Makefile. Las
dependencias se **acumulan**: la pattern rule `%.o: %.c` aporta el `.c` y
los `.d` aportan los headers.

### Paso 6.4 -- Verificar que -include ignora archivos inexistentes

```bash
make -f Makefile.fixed clean
ls *.d 2>&1
```

Salida esperada:

```
ls: cannot access '*.d': No such file or directory
```

Los `.d` no existen. Ahora compila:

```bash
make -f Makefile.fixed
```

Salida esperada:

```
gcc -Wall -Wextra -std=c11 -MMD -MP -c -o main.o main.c
gcc -Wall -Wextra -std=c11 -MMD -MP -c -o engine.o engine.c
gcc -Wall -Wextra -std=c11 -MMD -MP -c -o logger.o logger.c
gcc -o app main.o engine.o logger.o
```

No hubo error a pesar de que los `.d` no existian. `-include` (con guion
al inicio) ignora silenciosamente los archivos que no existen. En la primera
compilacion, Make no tiene informacion de dependencias de headers, pero eso
no importa: como los `.o` tampoco existen, compila todo y genera los `.d`
como efecto secundario. A partir de la segunda compilacion, los `.d` ya
existen y Make los usa.

---

## Limpieza final

```bash
make -f Makefile.fixed clean
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
Makefile.broken  Makefile.fixed  README.md  config.h  engine.c  engine.h  logger.c  logger.h  main.c
```

---

## Conceptos reforzados

1. Un Makefile con `%.o: %.c` como unica regla de compilacion tiene un **bug
   silencioso**: cambiar un header no provoca recompilacion. Make dice "up to
   date" y el programa usa codigo viejo.

2. `gcc -MM archivo.c` genera la lista de dependencias (sin headers del
   sistema) en formato Makefile. Es util para inspeccionar, pero no para
   automatizar porque no compila.

3. `gcc -MMD` genera el archivo `.d` **como efecto secundario** de la
   compilacion. Un solo comando produce tanto el `.o` como el `.d`, sin
   necesidad de una pasada extra.

4. `-MP` genera phony targets (reglas vacias) para cada header en el `.d`.
   Sin `-MP`, eliminar un header del proyecto causa el error "No rule to make
   target", porque el `.d` viejo aun lo referencia.

5. `-include $(DEPS)` incluye los archivos `.d` si existen y los ignora si
   no. Esto resuelve el problema de la primera compilacion: los `.d` no
   existen, `-include` los ignora, Make compila todo y genera los `.d`.

6. Las dependencias se **acumulan**: la pattern rule `%.o: %.c` aporta el
   `.c` como prerequisite, y los `.d` aportan los headers. Make combina ambas
   fuentes para decidir que recompilar.

7. La combinacion `DEPFLAGS = -MMD -MP` en la pattern rule mas
   `-include $(DEPS)` al final del Makefile es el **patron estandar** para
   dependencias automaticas. No requiere mantenimiento manual: los `.d` se
   regeneran cada vez que un `.o` se recompila.

8. Los archivos `.d` deben incluirse en la regla `clean`. Si quedan `.d`
   obsoletos con referencias a headers eliminados, pueden causar
   recompilaciones innecesarias o errores (si no se uso `-MP`).
