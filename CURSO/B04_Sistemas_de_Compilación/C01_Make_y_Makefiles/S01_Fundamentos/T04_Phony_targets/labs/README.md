# Lab -- Phony targets

## Objetivo

Demostrar por que los targets que no representan archivos necesitan `.PHONY`,
experimentar el problema de colision de nombres, y construir un Makefile
profesional con targets convencionales: all, clean, install, test y help.
Al finalizar, sabras usar PREFIX para instalacion local y el patron de
auto-documentacion con `## comments` y grep.

## Prerequisitos

- GCC y GNU Make instalados
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `main.c` | Programa principal que usa greeting.h |
| `greeting.h` | Header con declaraciones de greet() y greet_loud() |
| `greeting.c` | Implementacion de las funciones de saludo |
| `Makefile.broken` | Makefile SIN .PHONY (para demostrar el problema) |
| `Makefile.complete` | Makefile completo con targets convencionales y .PHONY |

---

## Parte 1 -- El problema sin .PHONY

**Objetivo**: Experimentar como un archivo con el mismo nombre que un target
rompe el funcionamiento de Make, y resolverlo con `.PHONY`.

### Paso 1.1 -- Examinar el Makefile sin .PHONY

```bash
cat Makefile.broken
```

Observa que hay targets `all` y `clean`, pero **ninguna declaracion `.PHONY`**.
Estos targets no producen archivos con esos nombres -- son acciones. Sin
embargo, Make no lo sabe.

### Paso 1.2 -- Compilar el proyecto

```bash
make -f Makefile.broken
```

Salida esperada:

```
gcc -Wall -Wextra -std=c11 -c -o main.o main.c
gcc -Wall -Wextra -std=c11 -c -o greeting.o greeting.c
gcc -Wall -Wextra -std=c11 -o greeter main.o greeting.o
```

### Paso 1.3 -- Probar clean normalmente

```bash
make -f Makefile.broken clean
```

Salida esperada:

```
rm -f main.o greeting.o greeter
```

Funciona correctamente. Ahora recompila para tener los archivos de nuevo:

```bash
make -f Makefile.broken
```

### Paso 1.4 -- Predecir: que pasa si existe un archivo llamado "clean"

Antes de ejecutar, responde mentalmente:

- Make trata todos los targets como archivos por defecto. Si creas un archivo
  llamado "clean", que hara Make al ejecutar `make clean`?
- El target "clean" no tiene prerequisites. Si el archivo "clean" existe y no
  depende de nada, Make lo considera actualizado?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.5 -- Crear el archivo "clean" y observar el problema

```bash
touch clean
make -f Makefile.broken clean
```

Salida esperada:

```
make: 'clean' is up to date.
```

Make encontro el archivo `clean`, verifico que no tiene prerequisites, y
concluyo que esta "actualizado". No ejecuto la recipe. Los archivos `.o` y el
ejecutable `greeter` siguen existiendo:

```bash
ls *.o greeter
```

Salida esperada:

```
greeting.o  main.o  greeter
```

Este es el problema central: basta con que alguien cree un archivo con el
nombre de un target (accidentalmente, o un programa que genere un archivo
"test", "all", etc.) para que Make deje de funcionar.

### Paso 1.6 -- Repetir con "all"

```bash
touch all
make -f Makefile.broken
```

Salida esperada:

```
make: 'all' is up to date.
```

Incluso el target por defecto se rompe. Make ve el archivo `all`, no tiene
razon para reconstruirlo, y no hace nada.

### Paso 1.7 -- La solucion: agregar .PHONY manualmente

Ahora vas a ver la diferencia. Sin modificar el Makefile, puedes declarar
targets phony desde la linea de comandos invocando el Makefile completo:

```bash
make -f Makefile.complete clean
```

Salida esperada:

```
rm -f main.o greeting.o greeter
```

Funciona a pesar de que el archivo `clean` sigue existiendo. Examina por que:

```bash
grep '\.PHONY' Makefile.complete
```

Salida esperada:

```
.PHONY: all
.PHONY: install
.PHONY: uninstall
.PHONY: test
.PHONY: clean
.PHONY: distclean
.PHONY: help
```

Cada target que no representa un archivo esta declarado como `.PHONY`. Make
sabe que no debe buscar archivos con esos nombres.

### Paso 1.8 -- Verificar que .PHONY ignora los archivos espurios

Los archivos `clean` y `all` siguen existiendo:

```bash
ls -la clean all
```

Pero con el Makefile completo, Make los ignora:

```bash
make -f Makefile.complete
```

Salida esperada:

```
gcc -Wall -Wextra -std=c11 -c -o main.o main.c
gcc -Wall -Wextra -std=c11 -c -o greeting.o greeting.c
gcc -Wall -Wextra -std=c11 -o greeter main.o greeting.o
```

Compila normalmente. El archivo `all` no interfiere.

### Limpieza de la parte 1

```bash
rm -f clean all
make -f Makefile.complete clean
```

---

## Parte 2 -- Targets convencionales

**Objetivo**: Explorar los targets estandar de un Makefile Unix (all, clean,
test, help) y entender para que sirve cada uno.

### Paso 2.1 -- Examinar el Makefile completo

```bash
cat Makefile.complete
```

Observa la estructura:

- Variables de configuracion al inicio (CC, CFLAGS, PREFIX, VERSION)
- `.PHONY` declarado junto a cada target
- Comentarios `##` al final de la linea de cada target

### Paso 2.2 -- Compilar con all (target por defecto)

```bash
make -f Makefile.complete
```

Salida esperada:

```
gcc -Wall -Wextra -std=c11 -c -o main.o main.c
gcc -Wall -Wextra -std=c11 -c -o greeting.o greeting.c
gcc -Wall -Wextra -std=c11 -o greeter main.o greeting.o
```

`all` es el primer target del Makefile, asi que es el target por defecto.
`make` sin argumentos ejecuta `all`.

### Paso 2.3 -- Ejecutar el programa compilado

```bash
./greeter
```

Salida esperada:

```
=== Greeting program ===
Hello, World!
HELLO, WORLD!!!
```

### Paso 2.4 -- Ejecutar los tests

```bash
make -f Makefile.complete test
```

Salida esperada:

```
Running tests...
  [PASS] greeter executes without error
  [PASS] output contains Hello
  [PASS] output contains HELLO (loud)
All tests passed.
```

El target `test` depende de `$(TARGET)`, asi que recompila si es necesario
antes de ejecutar los tests.

### Paso 2.5 -- Predecir: que hace make clean seguido de make test

Antes de ejecutar, responde mentalmente:

- `make clean` elimina los .o y el ejecutable. Si despues ejecutas `make test`,
  que pasa primero? Se ejecutan los tests directamente o Make necesita hacer
  algo antes?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.6 -- Verificar la prediccion

```bash
make -f Makefile.complete clean
make -f Makefile.complete test
```

Salida esperada:

```
rm -f main.o greeting.o greeter
gcc -Wall -Wextra -std=c11 -c -o main.o main.c
gcc -Wall -Wextra -std=c11 -c -o greeting.o greeting.c
gcc -Wall -Wextra -std=c11 -o greeter main.o greeting.o
Running tests...
  [PASS] greeter executes without error
  [PASS] output contains Hello
  [PASS] output contains HELLO (loud)
All tests passed.
```

Make recompilo automaticamente antes de ejecutar los tests porque `test`
tiene `$(TARGET)` como prerequisite. La cadena de dependencias se resuelve
automaticamente: test necesita greeter, greeter necesita los .o, los .o
necesitan los .c.

---

## Parte 3 -- install con PREFIX

**Objetivo**: Usar el target `install` para copiar el ejecutable a un directorio
local con PREFIX, y entender como funciona el comando `install(1)`.

### Paso 3.1 -- Examinar el target install

```bash
grep -A 3 'install:' Makefile.complete | head -4
```

Salida esperada:

```
install: $(TARGET) ## Install to PREFIX (default: ./install)
	install -d $(PREFIX)/bin
	install -m 755 $(TARGET) $(PREFIX)/bin/$(TARGET)
```

Observa:

- `install -d` crea el directorio si no existe (como `mkdir -p`)
- `install -m 755` copia el archivo y establece permisos de ejecucion
- `$(PREFIX)` por defecto es `./install` (definido en las variables)

### Paso 3.2 -- Instalar con PREFIX por defecto

```bash
make -f Makefile.complete install
```

Salida esperada:

```
install -d ./install/bin
install -m 755 greeter ./install/bin/greeter
```

Verifica la estructura creada:

```bash
ls -la install/bin/
```

Salida esperada:

```
-rwxr-xr-x 1 <user> <group> ~16K <date> greeter
```

### Paso 3.3 -- Ejecutar el programa instalado

```bash
./install/bin/greeter
```

Salida esperada:

```
=== Greeting program ===
Hello, World!
HELLO, WORLD!!!
```

### Paso 3.4 -- Instalar en otra ubicacion con PREFIX personalizado

```bash
make -f Makefile.complete install PREFIX=/tmp/lab-phony
```

Salida esperada:

```
install -d /tmp/lab-phony/bin
install -m 755 greeter /tmp/lab-phony/bin/greeter
```

```bash
/tmp/lab-phony/bin/greeter
```

Salida esperada:

```
=== Greeting program ===
Hello, World!
HELLO, WORLD!!!
```

PREFIX permite controlar donde se instala **sin modificar el Makefile**. En
proyectos reales, `/usr/local` es el PREFIX por defecto, y los usuarios lo
sobreescriben asi:

```
make install PREFIX=$HOME/.local
```

### Paso 3.5 -- Desinstalar

```bash
make -f Makefile.complete uninstall
```

Salida esperada:

```
rm -f ./install/bin/greeter
```

Verifica:

```bash
ls install/bin/
```

El directorio existe pero el ejecutable se elimino. `uninstall` solo elimina
los archivos que `install` coloco, no los directorios.

### Limpieza de la parte 3

```bash
rm -rf install/ /tmp/lab-phony
```

---

## Parte 4 -- Auto-documentacion con help

**Objetivo**: Entender el patron de auto-documentacion donde cada target
tiene un comentario `##` que se extrae automaticamente con grep para generar
la ayuda.

### Paso 4.1 -- Ejecutar make help

```bash
make -f Makefile.complete help
```

Salida esperada:

```
greeter v1.0.0 -- Build system

Usage: make -f Makefile.complete [TARGET]

  all              Build the project
  install          Install to PREFIX (default: ./install)
  uninstall        Remove installed files
  test             Run basic tests
  clean            Remove build artifacts
  distclean        Remove all generated files including install
  help             Show available targets

Variables:
  CC=gcc  CFLAGS=-Wall -Wextra -std=c11
  PREFIX=./install
```

La ayuda se genera automaticamente a partir de los comentarios `##` en el
Makefile.

### Paso 4.2 -- Entender el mecanismo de auto-documentacion

Examina la recipe del target help:

```bash
grep -A 3 '^help:' Makefile.complete
```

Salida esperada:

```
help: ## Show available targets
	@echo "$(TARGET) v$(VERSION) -- Build system"
	@echo ""
	@echo "Usage: make -f Makefile.complete [TARGET]"
```

La linea clave es la de grep+awk. Vamos a desglosarla ejecutandola
directamente:

```bash
grep -E '^[a-zA-Z_-]+:.*##' Makefile.complete
```

Salida esperada:

```
all: $(TARGET) ## Build the project
install: $(TARGET) ## Install to PREFIX (default: ./install)
uninstall: ## Remove installed files
test: $(TARGET) ## Run basic tests
clean: ## Remove build artifacts
distclean: clean ## Remove all generated files including install
help: ## Show available targets
```

grep extrae solo las lineas que tienen un nombre de target seguido de `##`.
Los targets sin `##` (como las reglas de compilacion) no aparecen.

### Paso 4.3 -- Agregar el formateo con awk

```bash
grep -E '^[a-zA-Z_-]+:.*##' Makefile.complete | \
    awk -F ':.*## ' '{printf "  %-15s %s\n", $$1, $$2}'
```

Salida esperada:

```
  all              Build the project
  install          Install to PREFIX (default: ./install)
  uninstall        Remove installed files
  test             Run basic tests
  clean            Remove build artifacts
  distclean        Remove all generated files including install
  help             Show available targets
```

awk usa `:.*## ` como separador de campo:

- `$$1` es el nombre del target (antes de `:`)
- `$$2` es la descripcion (despues de `## `)
- `%-15s` alinea a la izquierda en 15 caracteres

Nota: en un Makefile se usa `$$` para un `$` literal, porque Make interpreta
`$` como referencia a variable.

### Paso 4.4 -- Predecir: que pasa si un target no tiene comentario ##

Antes de continuar, responde mentalmente:

- La regla `%.o: %.c` no tiene comentario `##`. Aparecera en la ayuda?
- Si agregas `## Compile source files` a esa regla, apareceria?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.5 -- Verificar la prediccion

La regla `%.o: %.c` no aparece porque:

1. Contiene `%` que no coincide con `[a-zA-Z_-]+` en el regex de grep
2. No tiene comentario `##`

Si tuviera un comentario `##`, seguiria sin aparecer porque el patron `%` no
coincide con la expresion regular. El grep esta disenado para filtrar solo
targets nombrados.

Este es un patron comun en proyectos open source. Permite que el Makefile se
auto-documente: cada vez que agregas un target nuevo, solo necesitas poner
`## descripcion` al final de su linea para que aparezca en `make help`.

---

## Limpieza final

```bash
make -f Makefile.complete distclean
rm -rf install/ /tmp/lab-phony
```

Verifica que solo quedan los archivos fuente originales y los Makefiles:

```bash
ls
```

Salida esperada:

```
Makefile.broken  Makefile.complete  README.md  greeting.c  greeting.h  main.c
```

---

## Conceptos reforzados

1. Sin `.PHONY`, un archivo con el mismo nombre que un target (como `clean` o
   `all`) hace que Make considere el target "actualizado" y no ejecute su
   recipe. Basta un `touch clean` para romper `make clean`.

2. `.PHONY: clean` le dice a Make que `clean` no es un archivo. Make siempre
   ejecuta su recipe, sin importar si existe un archivo con ese nombre.

3. Los targets convencionales de un proyecto Unix son: `all` (compilar, target
   por defecto), `clean` (eliminar artefactos), `install` (copiar al sistema),
   `uninstall` (remover), `test` (ejecutar pruebas), y `help` (mostrar ayuda).

4. `PREFIX` controla el directorio base de instalacion. Se sobreescribe desde
   la linea de comandos (`make install PREFIX=$HOME/.local`) sin modificar el
   Makefile. El comando `install(1)` es preferible a `cp` porque crea
   directorios y establece permisos de forma atomica.

5. La variable `$(RM)` predefinida por Make equivale a `rm -f`. Se usa en
   targets de limpieza para evitar errores si el archivo no existe.

6. El patron de auto-documentacion con `## comments` y grep permite que el
   Makefile se documente a si mismo. Agregar `## descripcion` a un target
   lo hace visible en `make help` sin mantener una lista manual aparte.

7. Declarar `.PHONY` junto a cada target (forma 3 del README) es la
   convencion mas legible en Makefiles grandes: cada target muestra
   inmediatamente si es phony o no, sin buscar en otra parte del archivo.
