# T02 — Dependencias automaticas

## El problema

Make decide que recompilar comparando timestamps del target contra
sus prerequisites. Pero las reglas tipicas solo listan el `.c`
como prerequisite del `.o`, **no los headers** que ese `.c` incluye:

```c
// config.h
#ifndef CONFIG_H
#define CONFIG_H
#define MAX_ITEMS 100
#endif
```

```c
// utils.h
#ifndef UTILS_H
#define UTILS_H
void print_items(void);
#endif
```

```c
// main.c
#include <stdio.h>
#include "config.h"
#include "utils.h"

int main(void) {
    printf("Max items: %d\n", MAX_ITEMS);
    print_items();
    return 0;
}
```

```makefile
# Makefile con el bug: los headers NO estan en los prerequisites.
CC     = gcc
CFLAGS = -Wall -Wextra

all: program

program: main.o utils.o
	$(CC) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f program *.o
```

```bash
# Demostrar el bug:

make
# gcc -Wall -Wextra -c -o main.o main.c
# gcc -Wall -Wextra -c -o utils.o utils.c
# gcc -o program main.o utils.o

# Ahora cambias MAX_ITEMS de 100 a 500 en config.h:
# nano config.h

touch config.h      # simular la modificacion
make
# make: 'program' is up to date.
#
# BUG: Make no recompilo main.o aunque config.h cambio.
# La regla %.o: %.c solo mira el .c, no el .h.
# El programa sigue usando MAX_ITEMS = 100.
```

Este es un error silencioso: no hay mensaje de fallo, el programa
simplemente usa valores viejos. En proyectos grandes puede producir
comportamiento inexplicable que se resuelve con `make clean && make`
pero que vuelve a aparecer con cada cambio de header.

## Solucion manual

La solucion obvia es listar los headers como prerequisites:

```makefile
main.o: main.c config.h utils.h
	$(CC) $(CFLAGS) -c -o $@ $<

utils.o: utils.c utils.h
	$(CC) $(CFLAGS) -c -o $@ $<
```

```makefile
# Tambien se puede agregar prerequisites sin duplicar la recipe.
# Las dependencias se acumulan:

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

main.o: config.h utils.h
utils.o: utils.h
```

```bash
# Ahora funciona:
touch config.h
make
# gcc -Wall -Wextra -c -o main.o main.c
# gcc -o program main.o utils.o
# main.o se recompilo porque config.h es mas nuevo.
```

Problemas de la solucion manual:

1. **Tedioso** -- hay que inspeccionar cada `.c` para ver que headers
   incluye, y los headers que esos headers incluyen (transitivamente).
2. **Propenso a errores** -- olvidar un header es facil y el bug
   es silencioso.
3. **Se desactualiza** -- cada vez que agregas o quitas un `#include`
   en un `.c`, hay que actualizar el Makefile. Si no lo haces, vuelve
   el bug.

## GCC genera dependencias

GCC puede analizar los `#include` de un `.c` y generar la lista de
dependencias en formato Makefile:

```bash
# -M: generar dependencias (TODOS los headers, incluidos los del sistema):
gcc -M main.c
# main.o: main.c /usr/include/stdio.h /usr/include/x86_64-linux-gnu/... \
#  config.h utils.h

# -MM: igual pero SIN headers del sistema (los de < >):
gcc -MM main.c
# main.o: main.c config.h utils.h
# Esto es lo que necesitamos: un fragmento de Makefile.
```

```bash
# -MM solo imprime las dependencias, no compila.
# Para generar dependencias COMO EFECTO SECUNDARIO de la compilacion:

# -MMD: compila Y genera un archivo .d con las dependencias.
gcc -MMD -c -o main.o main.c
# Produce main.o (el objeto) Y main.d (las dependencias).

cat main.d
# main.o: main.c config.h utils.h
```

```bash
# -MF archivo: controlar el nombre del archivo de salida de dependencias.
gcc -MM -MF deps/main.d main.c
# Escribe las dependencias en deps/main.d en vez de stdout.

# -MF se combina con -MMD para controlar donde va el .d:
gcc -MMD -MF build/main.d -c -o build/main.o main.c
# Compila a build/main.o y genera dependencias en build/main.d.
```

## Formato del archivo .d

El archivo `.d` generado es un fragmento de Makefile valido.
Contiene una regla sin recipe (solo target y prerequisites):

```makefile
# Contenido de main.d:
main.o: main.c config.h utils.h

# Esto es exactamente lo que escribiriamos a mano.
# Make puede incluir este archivo y obtener las dependencias
# de forma automatica.
```

```makefile
# Si el archivo incluye headers que a su vez incluyen otros:
# main.c -> config.h -> types.h
# main.c -> utils.h

# El .d lista TODAS las dependencias transitivas:
main.o: main.c config.h types.h utils.h
```

## El flag -MP

Sin `-MP`, hay un problema cuando se elimina un header:

```bash
# Situacion: main.d contiene:
#   main.o: main.c config.h utils.h

# Ahora eliminamos utils.h del proyecto (ya no se usa):
rm utils.h
# Editamos main.c para quitar el #include "utils.h"

make
# make: *** No rule to make target 'utils.h', needed by 'main.o'.  Stop.
#
# Make incluyo el .d viejo que dice que main.o depende de utils.h.
# utils.h no existe y no hay regla para crearlo → error.
```

```bash
# La solucion es -MP: genera phony targets para cada header.
gcc -MM -MP main.c
# main.o: main.c config.h utils.h
# config.h:
# utils.h:

# Los targets vacios (config.h: y utils.h:) actuan como phony.
# Si el header desaparece, Make encuentra el target vacio
# y simplemente lo ignora — no hay error.
```

```bash
# Con -MMD -MP combinados:
gcc -MMD -MP -c -o main.o main.c

cat main.d
# main.o: main.c config.h utils.h
# config.h:
# utils.h:

# Ahora si borras un header y su #include correspondiente,
# Make no falla. En la siguiente compilacion se genera un .d nuevo
# sin el header eliminado.
```

## El flag -MT

`-MT` permite cambiar el target que aparece en el archivo `.d`.
Es util cuando los `.o` van a un directorio separado:

```bash
# Sin -MT:
gcc -MM main.c
# main.o: main.c config.h utils.h
#   ↑ target es "main.o" (directorio actual implicito)

# Con -MT:
gcc -MM -MT build/main.o main.c
# build/main.o: main.c config.h utils.h
#   ↑ ahora el target coincide con la ruta real del .o
```

```bash
# -MT se puede usar multiples veces para generar varios targets:
gcc -MM -MT build/main.o -MT build/main.d main.c
# build/main.o build/main.d: main.c config.h utils.h
# Asi, el .d tambien se regenera cuando un header cambia.
```

## El patron completo

La forma estandar de implementar dependencias automaticas usa una
pattern rule con `-MMD -MP`:

```makefile
CC      = gcc
CFLAGS  = -Wall -Wextra -g

SRCS    = main.c utils.c parser.c
OBJS    = $(SRCS:.c=.o)
DEPS    = $(OBJS:.o=.d)

DEPFLAGS = -MMD -MP

all: program

program: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

clean:
	rm -f program $(OBJS) $(DEPS)

-include $(DEPS)
```

```makefile
# Desglose de la linea clave:
#
# %.o: %.c
# 	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<
#
# $(DEPFLAGS) = -MMD -MP
#   -MMD → genera un .d como efecto secundario (parser.d para parser.o)
#   -MP  → agrega phony targets para los headers
#
# El comando ejecutado para parser.c es:
#   gcc -Wall -Wextra -g -MMD -MP -c -o parser.o parser.c
#
# Esto produce:
#   parser.o   (el archivo objeto)
#   parser.d   (las dependencias: parser.o: parser.c parser.h utils.h ...)
```

## -include vs include

```makefile
# include — incluir otro Makefile. Si no existe, Make da error:
include extra_rules.mk
# Si extra_rules.mk no existe:
# Makefile:1: extra_rules.mk: No such file or directory
# make: *** No rule to make target 'extra_rules.mk'.  Stop.

# -include — incluir si existe, ignorar silenciosamente si no:
-include extra_rules.mk
# Si extra_rules.mk no existe, Make no se queja y continua.
```

```makefile
# Por eso las dependencias usan -include:
-include $(DEPS)

# En la primera compilacion los .d no existen.
# -include los ignora y Make continua normalmente.
# Si usaras include sin guion, Make fallaria en la primera build.
```

## El truco de la primera compilacion

El flujo completo de como funcionan las dependencias automaticas
requiere entender la secuencia temporal:

```makefile
# Situacion: proyecto limpio, ningun .o ni .d existe.

# Paso 1: Make lee el Makefile.
# Encuentra: -include main.d utils.d parser.d
# Ninguno existe → -include los ignora silenciosamente.

# Paso 2: Make quiere construir program.
# Necesita main.o, utils.o, parser.o.
# Ninguno existe → debe compilar todos.

# Paso 3: Ejecuta la recipe de %.o: %.c para cada fuente.
#   gcc -Wall -Wextra -g -MMD -MP -c -o main.o main.c
#   gcc -Wall -Wextra -g -MMD -MP -c -o utils.o utils.c
#   gcc -Wall -Wextra -g -MMD -MP -c -o parser.o parser.c
# Se generan main.o, utils.o, parser.o (objetos)
# Y tambien main.d, utils.d, parser.d (dependencias)

# Paso 4: Linka.
#   gcc -o program main.o utils.o parser.o

# Paso 5: Segunda build (despues de modificar un header).
# Make lee el Makefile.
# Encuentra: -include main.d utils.d parser.d
# AHORA SI existen → los incluye.
# main.d dice: main.o: main.c config.h utils.h
# Si config.h cambio, Make recompila main.o. Correcto.
```

```makefile
# Resumen:
# 1ra build: no hay .d → -include ignora → compila todo → genera .d
# 2da build: hay .d   → -include los lee → sabe que headers importan
#
# No hay problema de "huevo o gallina":
# - Sin .d, Make compila todo (los .o no existen).
# - Al compilar, -MMD genera los .d.
# - En la siguiente build, los .d ya estan disponibles.
```

## Build directory separado

Cuando los archivos objeto van a un directorio `build/`, los
archivos `.d` deben ir al mismo lugar:

```makefile
CC       = gcc
CFLAGS   = -Wall -Wextra -g
CPPFLAGS = -Iinclude

SRCDIR   = src
BUILDDIR = build

SRCS     = $(wildcard $(SRCDIR)/*.c)
OBJS     = $(SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
DEPS     = $(OBJS:.o=.d)

DEPFLAGS = -MMD -MP

all: $(BUILDDIR)/program

$(BUILDDIR)/program: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILDDIR)

.PHONY: all clean

-include $(DEPS)
```

```makefile
# La sustitucion $(OBJS:.o=.d) produce:
#   OBJS = build/main.o build/utils.o build/parser.o
#   DEPS = build/main.d build/utils.d build/parser.d
#
# Los .d se generan junto a los .o en build/ gracias a -MMD.
# -MMD nombra el .d basandose en el -o del .o:
#   gcc ... -MMD -MP -c -o build/main.o src/main.c
#   → genera build/main.d (mismo directorio, misma base, extension .d)
#
# El .d generado contiene el path completo:
#   build/main.o: src/main.c include/config.h include/utils.h
#   include/config.h:
#   include/utils.h:
```

```bash
# Estructura resultante despues de make:
# build/
#   main.o
#   main.d
#   utils.o
#   utils.d
#   parser.o
#   parser.d
#   program

# make clean borra todo el directorio build/:
make clean
# rm -rf build
# Los .d se eliminan junto con los .o. Limpio.
```

## Limpiar los .d

Los archivos `.d` son generados y deben limpiarse junto con los
objetos:

```makefile
# Si no usas build directory separado, limpiar explicitamente:
clean:
	$(RM) program $(OBJS) $(DEPS)

# $(RM) es una variable predefinida de Make: "rm -f".
# Equivale a: rm -f program main.o utils.o ... main.d utils.d ...
```

```makefile
# Si usas build directory separado, basta con borrar el directorio:
clean:
	rm -rf $(BUILDDIR)

# Todos los .o y .d estan dentro de build/ → un solo rm -rf.
```

```makefile
# Error comun: olvidar los .d en clean.
# Si solo limpias los .o pero no los .d:
clean:
	$(RM) program $(OBJS)
	# Faltan los .d

# Problema: los .d viejos siguen ahi.
# Si borraste un header, el .d viejo lo referencia → error en la
# siguiente build (a menos que uses -MP, que mitiga esto).
# Aun asi, .d obsoletos pueden causar recompilaciones innecesarias.
# Siempre limpiar los .d.
```

## Ejemplo completo

Un proyecto con varios `.c` y `.h` que demuestra el comportamiento
de las dependencias automaticas ante cambios en headers:

```c
// config.h
#ifndef CONFIG_H
#define CONFIG_H
#define APP_NAME "AutoDeps Demo"
#define VERSION  1
#endif
```

```c
// logger.h
#ifndef LOGGER_H
#define LOGGER_H
void log_message(const char *msg);
#endif
```

```c
// logger.c
#include <stdio.h>
#include "logger.h"
#include "config.h"

void log_message(const char *msg) {
    printf("[%s v%d] %s\n", APP_NAME, VERSION, msg);
}
```

```c
// engine.h
#ifndef ENGINE_H
#define ENGINE_H
void run_engine(void);
#endif
```

```c
// engine.c
#include "engine.h"
#include "logger.h"

void run_engine(void) {
    log_message("Engine started");
    log_message("Processing...");
    log_message("Engine stopped");
}
```

```c
// main.c
#include <stdio.h>
#include "config.h"
#include "engine.h"

int main(void) {
    printf("Starting %s v%d\n", APP_NAME, VERSION);
    run_engine();
    return 0;
}
```

```makefile
# Makefile
CC       = gcc
CFLAGS   = -Wall -Wextra -std=c11 -g
DEPFLAGS = -MMD -MP

SRCS = main.c engine.c logger.c
OBJS = $(SRCS:.c=.o)
DEPS = $(OBJS:.o=.d)

all: app

app: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

clean:
	$(RM) app $(OBJS) $(DEPS)

.PHONY: all clean

-include $(DEPS)
```

```bash
# Primera compilacion:
make
# gcc -Wall -Wextra -std=c11 -g -MMD -MP -c -o main.o main.c
# gcc -Wall -Wextra -std=c11 -g -MMD -MP -c -o engine.o engine.c
# gcc -Wall -Wextra -std=c11 -g -MMD -MP -c -o logger.o logger.c
# gcc -o app main.o engine.o logger.o

./app
# Starting AutoDeps Demo v1
# [AutoDeps Demo v1] Engine started
# [AutoDeps Demo v1] Processing...
# [AutoDeps Demo v1] Engine stopped

# Verificar los .d generados:
cat main.d
# main.o: main.c config.h engine.h
# config.h:
# engine.h:

cat logger.d
# logger.o: logger.c logger.h config.h
# logger.h:
# config.h:

cat engine.d
# engine.o: engine.c engine.h logger.h
# engine.h:
# logger.h:
```

```bash
# Cambiar config.h: VERSION de 1 a 2.
# Solo main.c y logger.c incluyen config.h.
# engine.c NO incluye config.h.

# Simular el cambio:
touch config.h
make
# gcc -Wall -Wextra -std=c11 -g -MMD -MP -c -o main.o main.c
# gcc -Wall -Wextra -std=c11 -g -MMD -MP -c -o logger.o logger.c
# gcc -o app main.o engine.o logger.o
#
# Resultado:
#   main.o   → recompilado (depende de config.h)
#   logger.o → recompilado (depende de config.h)
#   engine.o → NO recompilado (no depende de config.h)
#   app      → re-enlazado (main.o y logger.o cambiaron)
```

```bash
# Cambiar logger.h:
touch logger.h
make
# gcc -Wall -Wextra -std=c11 -g -MMD -MP -c -o engine.o engine.c
# gcc -Wall -Wextra -std=c11 -g -MMD -MP -c -o logger.o logger.c
# gcc -o app main.o engine.o logger.o
#
# Resultado:
#   engine.o → recompilado (depende de logger.h)
#   logger.o → recompilado (depende de logger.h)
#   main.o   → NO recompilado (no depende de logger.h)

# Cambiar engine.h:
touch engine.h
make
# gcc -Wall -Wextra -std=c11 -g -MMD -MP -c -o main.o main.c
# gcc -Wall -Wextra -std=c11 -g -MMD -MP -c -o engine.o engine.c
# gcc -o app main.o engine.o logger.o
#
# Resultado:
#   main.o   → recompilado (depende de engine.h)
#   engine.o → recompilado (depende de engine.h)
#   logger.o → NO recompilado (no depende de engine.h)
```

```bash
# Sin dependencias automaticas, NINGUNO de esos cambios de header
# habria provocado recompilacion. Make diria "up to date" y el
# programa usaria codigo viejo.
#
# Con -MMD -MP, Make sabe exactamente que recompilar ante cada
# cambio de header — sin mantenimiento manual del Makefile.
```

---

## Ejercicios

### Ejercicio 1 --- Diagnosticar el bug de dependencias

```makefile
# Crear un proyecto con:
#   types.h  — define: typedef struct { int x, y; } Point;
#   geo.h    — declara: double distance(Point a, Point b);
#   geo.c    — implementa distance() usando sqrt (incluye geo.h y types.h)
#   main.c   — crea dos Points y muestra su distancia (incluye geo.h y types.h)
#
# 1. Escribir un Makefile SIN dependencias automaticas:
#      %.o: %.c
#          $(CC) $(CFLAGS) -c -o $@ $<
#
# 2. Compilar y ejecutar. Verificar que funciona.
#
# 3. Cambiar la struct Point en types.h: agregar un campo "int z".
#    Ejecutar make. Observar que dice "up to date" (el bug).
#
# 4. Agregar DEPFLAGS = -MMD -MP a la recipe e -include $(DEPS).
#    Ejecutar make clean && make. Modificar types.h otra vez.
#    Ejecutar make y verificar que AHORA si recompila los .o afectados.
#
# 5. Examinar los .d generados y verificar que listan types.h
#    como dependencia de geo.o y main.o.
```

### Ejercicio 2 --- -MP ante headers eliminados

```makefile
# Usar el proyecto del Ejercicio 1. Agregar un header extra:
#   debug.h — define: #define DEBUG_PRINT(x) printf("DBG: %d\n", x)
#
# Incluir debug.h en main.c. Compilar (make clean && make).
#
# 1. Verificar que main.d lista debug.h.
#
# 2. Ahora eliminar debug.h y quitar el #include de main.c.
#    Ejecutar make. Con -MP, debe compilar sin error.
#    Sin -MP, Make fallaria con "No rule to make target 'debug.h'".
#
# 3. Para comprobar, crear un segundo Makefile (Makefile.nomp) que use
#    -MMD SIN -MP. Repetir la eliminacion del header.
#    Verificar que falla con el error esperado.
#    Nota: copiar los .d de la build anterior para que contengan debug.h.
```

### Ejercicio 3 --- Build directory separado con dependencias

```makefile
# Crear un proyecto con esta estructura:
#   include/app.h
#   include/math_utils.h
#   include/string_utils.h
#   src/main.c         (incluye app.h, math_utils.h)
#   src/math_utils.c   (incluye math_utils.h)
#   src/string_utils.c (incluye string_utils.h)
#
# Escribir un Makefile que:
# 1. Compile los .o en build/ (build/main.o, build/math_utils.o, ...)
# 2. Genere los .d en build/ (build/main.d, build/math_utils.d, ...)
# 3. Use -CPPFLAGS = -Iinclude para encontrar los headers
# 4. Use -MMD -MP en la pattern rule
# 5. Use -include $(DEPS)
# 6. Tenga un target clean que borre build/ completo
#
# Verificar:
#   a. make           → compila todo, genera .o y .d en build/
#   b. make           → "up to date"
#   c. touch include/math_utils.h && make
#      → recompila build/main.o y build/math_utils.o (no string_utils.o)
#   d. ls build/*.d   → confirmar que existen los .d
#   e. make clean     → build/ desaparece
```
