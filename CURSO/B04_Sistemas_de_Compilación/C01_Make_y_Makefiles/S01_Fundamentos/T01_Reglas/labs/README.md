# Lab -- Reglas de Make

## Objetivo

Escribir reglas de Make desde cero, experimentar el error "missing separator",
entender como Make decide que recompilar comparando timestamps, practicar
compilacion separada con cadenas de dependencias, y usar prefijos de recipe
(@ y -) junto con los flags -n (dry run) y -B (forzar reconstruccion). Al
finalizar, sabras escribir un Makefile con multiples reglas para compilar un
programa C modular.

## Prerequisitos

- GCC instalado
- Make instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `hello.c` | Programa simple que imprime "Hello from Make" |
| `calc.h` | Header con declaraciones de add() y subtract() |
| `calc.c` | Implementacion de la biblioteca de calculo |
| `main_calc.c` | Programa principal que usa calc.h |

---

## Parte 1 -- Regla basica y TAB obligatorio

**Objetivo**: Escribir la regla mas simple posible, compilar un programa, y
experimentar el error "missing separator" al usar espacios en vez de TAB.

### Paso 1.1 -- Examinar el archivo fuente

```bash
cat hello.c
```

Observa que es un programa minimo que imprime un mensaje. Este sera el target
de nuestra primera regla.

### Paso 1.2 -- Crear el Makefile con una regla basica

Crea un archivo llamado `Makefile` con el siguiente contenido. Asegurate de
usar un **tabulador literal** (no espacios) antes del comando gcc:

```makefile
hello: hello.c
	gcc -Wall -Wextra -o hello hello.c
```

Para verificar que escribiste un TAB y no espacios:

```bash
cat -A Makefile
```

Salida esperada:

```
hello: hello.c$
^Igcc -Wall -Wextra -o hello hello.c$
```

`^I` indica un tabulador literal. Si ves espacios en su lugar, corrige el
archivo antes de continuar.

### Paso 1.3 -- Ejecutar make

```bash
make
```

Salida esperada:

```
gcc -Wall -Wextra -o hello hello.c
```

Make imprime el comando que ejecuta (el echo). Verifica que el programa
funciona:

```bash
./hello
```

Salida esperada:

```
Hello from Make
```

### Paso 1.4 -- Provocar el error "missing separator"

Ahora crea un Makefile intencionalmente roto. Crea un archivo `Makefile_broken`
usando **4 espacios** en vez de TAB antes del comando:

```makefile
hello: hello.c
    gcc -Wall -Wextra -o hello hello.c
```

Importante: la segunda linea debe comenzar con 4 espacios, NO un tabulador.

Predice antes de ejecutar:

- Que error mostrara Make al intentar usar este Makefile?
- En que linea indicara el problema?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.5 -- Ejecutar con el Makefile roto

```bash
make -f Makefile_broken
```

Salida esperada:

```
Makefile_broken:2: *** missing separator.  Stop.
```

Make reporta "missing separator" en la linea 2 porque encontro espacios donde
esperaba un TAB. Este es uno de los errores mas frecuentes al comenzar con
Make.

Verifica la diferencia con `cat -A`:

```bash
cat -A Makefile_broken
```

Salida esperada:

```
hello: hello.c$
    gcc -Wall -Wextra -o hello hello.c$
```

No hay `^I` al inicio de la linea 2 -- solo espacios. Ese es el problema.

### Paso 1.6 -- Corregir con sed

Si necesitas convertir espacios a TAB en un Makefile existente:

```bash
sed -i 's/^    /\t/' Makefile_broken
cat -A Makefile_broken
```

Salida esperada:

```
hello: hello.c$
^Igcc -Wall -Wextra -o hello hello.c$
```

Ahora la linea 2 comienza con `^I` (TAB). Verifica que funciona:

```bash
make -f Makefile_broken
```

Salida esperada:

```
make: 'hello' is up to date.
```

Funciona correctamente. Dice "up to date" porque `hello` ya existe del paso
1.3 (lo entenderemos mejor en la parte 2).

---

## Parte 2 -- Timestamps y recompilacion selectiva

**Objetivo**: Entender como Make compara los timestamps (mtime) del target
contra sus prerequisites para decidir si recompila.

### Paso 2.1 -- Ejecutar make cuando todo esta al dia

```bash
make
```

Salida esperada:

```
make: 'hello' is up to date.
```

Make ve que el archivo `hello` existe y es mas nuevo que `hello.c`. No hay nada
que hacer.

### Paso 2.2 -- Examinar los timestamps

```bash
ls -l hello.c hello
```

Salida esperada (las fechas variaran):

```
-rw-r--r-- 1 <user> <group>   ~80 <fecha> hello.c
-rwxr-xr-x 1 <user> <group> ~16K <fecha> hello
```

Observa que `hello` (el ejecutable) tiene un mtime igual o posterior al de
`hello.c`. Por eso Make dice "up to date".

### Paso 2.3 -- Simular una modificacion con touch

```bash
touch hello.c
```

Predice: si ejecutas `make` ahora, que ocurrira?

- `hello.c` ahora tiene un mtime mas reciente que `hello`
- Make compara el mtime del target contra los prerequisites...

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.4 -- Recompilar despues de touch

```bash
make
```

Salida esperada:

```
gcc -Wall -Wextra -o hello hello.c
```

Make recompila porque `hello.c` tiene un mtime mas reciente que `hello`.
Punto clave: Make no compara **contenido**, solo **timestamps**. Aunque
`touch` no cambio el contenido de hello.c, el mtime es mas nuevo y eso basta
para que Make recompile.

### Paso 2.5 -- Borrar el target y recompilar

```bash
rm hello
make
```

Salida esperada:

```
gcc -Wall -Wextra -o hello hello.c
```

Si el target no existe, Make siempre ejecuta la recipe. Este es el caso mas
simple del algoritmo de decision.

### Paso 2.6 -- Verificar con stat

```bash
make
stat --format="%n: modify %y" hello.c hello
```

Salida esperada (la primera linea puede ser "up to date"):

```
<ruta>/hello.c: modify 2026-03-18 ...
<ruta>/hello: modify 2026-03-18 ...
```

Observa que `hello` tiene un mtime igual o posterior a `hello.c`. Esa
comparacion es la base de toda la logica de Make.

---

## Parte 3 -- Multiples reglas y compilacion separada

**Objetivo**: Escribir un Makefile con varias reglas para compilar un programa
modular (.c -> .o -> ejecutable) y observar como Make recorre la cadena de
dependencias recompilando solo lo necesario.

### Paso 3.1 -- Examinar los archivos fuente

```bash
cat calc.h
```

Observa las declaraciones de `add()` y `subtract()`.

```bash
cat calc.c
```

Observa que incluye `calc.h` e implementa ambas funciones.

```bash
cat main_calc.c
```

Observa que incluye `calc.h` y llama a las funciones en `main()`.

### Paso 3.2 -- Crear el Makefile con compilacion separada

Elimina el Makefile anterior y crea uno nuevo con el siguiente contenido.
Recuerda: cada linea de recipe debe empezar con TAB:

```makefile
calculator: main_calc.o calc.o
	gcc -o calculator main_calc.o calc.o

main_calc.o: main_calc.c calc.h
	gcc -Wall -Wextra -c main_calc.c

calc.o: calc.c calc.h
	gcc -Wall -Wextra -c calc.c
```

### Paso 3.3 -- Predecir la cadena de ejecucion

Antes de ejecutar, responde mentalmente:

- Cual es la primera regla? Que target quiere construir Make por defecto?
- Para construir `calculator` necesita `main_calc.o` y `calc.o`. Existen?
- Si no existen, que reglas buscara Make para crearlos?
- En que orden se ejecutaran los tres comandos gcc?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.4 -- Compilar todo

```bash
make
```

Salida esperada:

```
gcc -Wall -Wextra -c main_calc.c
gcc -Wall -Wextra -c calc.c
gcc -o calculator main_calc.o calc.o
```

Make recorrio la cadena de dependencias:

1. Para `calculator` necesita `main_calc.o` -- no existe, busca su regla
2. Para `main_calc.o` necesita `main_calc.c` y `calc.h` -- existen, compila
3. Para `calculator` necesita `calc.o` -- no existe, busca su regla
4. Para `calc.o` necesita `calc.c` y `calc.h` -- existen, compila
5. Ahora tiene ambos .o -- enlaza el ejecutable

Verifica:

```bash
./calculator
```

Salida esperada:

```
3 + 4 = 7
10 - 3 = 7
5 + subtract(9, 2) = 12
```

### Paso 3.5 -- Recompilar sin cambios

```bash
make
```

Salida esperada:

```
make: 'calculator' is up to date.
```

Nada cambio, nada se recompila.

### Paso 3.6 -- Modificar solo un archivo fuente

```bash
touch calc.c
make
```

Predice antes de ver la salida:

- calc.c cambio. Que archivo .o depende de calc.c?
- main_calc.c no cambio. Se recompilara main_calc.o?
- calculator depende de ambos .o. Se re-enlazara?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.7 -- Observar la recompilacion selectiva

Salida esperada del `make` anterior:

```
gcc -Wall -Wextra -c calc.c
gcc -o calculator main_calc.o calc.o
```

Solo se ejecutaron **2** comandos, no 3:

- `calc.o` se recompilo porque `calc.c` es mas nuevo
- `main_calc.o` NO se recompilo -- `main_calc.c` no cambio
- `calculator` se re-enlazo porque `calc.o` cambio

Esta es la ventaja de la compilacion separada: en un proyecto con cientos de
archivos, solo se recompilan los que cambiaron.

### Paso 3.8 -- Modificar el header

```bash
touch calc.h
make
```

Salida esperada:

```
gcc -Wall -Wextra -c main_calc.c
gcc -Wall -Wextra -c calc.c
gcc -o calculator main_calc.o calc.o
```

Ahora se ejecutaron **3** comandos. Tanto `main_calc.o` como `calc.o` dependen
de `calc.h`, asi que ambos se recompilan. Luego se re-enlaza el ejecutable.

Esto demuestra por que es importante listar los headers en los prerequisites:
si no pones `calc.h` como prerequisite de `main_calc.o`, Make no recompilaria
`main_calc.o` al cambiar el header, lo cual podria causar bugs silenciosos.

---

## Parte 4 -- Primera regla como default target

**Objetivo**: Verificar que Make ejecuta la primera regla por defecto y
entender la convencion de usar `all` como primera regla.

### Paso 4.1 -- Predecir el efecto de cambiar el orden

Actualmente la primera regla de nuestro Makefile es `calculator`. Que pasaria
si la regla `calc.o` fuera la primera?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.2 -- Crear un Makefile con el orden incorrecto

Primero limpia los artefactos anteriores:

```bash
rm -f calculator main_calc.o calc.o
```

Ahora sobreescribe el Makefile con las reglas en orden cambiado:

```makefile
calc.o: calc.c calc.h
	gcc -Wall -Wextra -c calc.c

main_calc.o: main_calc.c calc.h
	gcc -Wall -Wextra -c main_calc.c

calculator: main_calc.o calc.o
	gcc -o calculator main_calc.o calc.o
```

```bash
make
```

Salida esperada:

```
gcc -Wall -Wextra -c calc.c
```

Make solo compilo `calc.o` porque esa es ahora la primera regla (el default
target). No construyo `calculator` ni `main_calc.o`.

### Paso 4.3 -- Corregir con la convencion `all`

Limpia y sobreescribe el Makefile. La convencion es usar `all` como primera
regla:

```bash
rm -f calculator main_calc.o calc.o
```

```makefile
all: calculator

calculator: main_calc.o calc.o
	gcc -o calculator main_calc.o calc.o

main_calc.o: main_calc.c calc.h
	gcc -Wall -Wextra -c main_calc.c

calc.o: calc.c calc.h
	gcc -Wall -Wextra -c calc.c
```

Observa que `all` no tiene recipe -- solo dice "para que `all` este completo,
construye `calculator`". El orden del resto de las reglas no importa; lo que
importa es que `all` sea la primera.

```bash
make
```

Salida esperada:

```
gcc -Wall -Wextra -c main_calc.c
gcc -Wall -Wextra -c calc.c
gcc -o calculator main_calc.o calc.o
```

Ahora `make` sin argumentos construye todo correctamente.

### Paso 4.4 -- Verificar que se puede invocar un target especifico

```bash
rm -f calc.o
make calc.o
```

Salida esperada:

```
gcc -Wall -Wextra -c calc.c
```

Aunque `all` es el default, puedes invocar cualquier target por nombre pasandolo
como argumento a `make`.

---

## Parte 5 -- Prefijos de recipe: @ y -

**Objetivo**: Entender como `@` silencia el echo de un comando y como `-`
ignora errores, permitiendo que Make continue la ejecucion.

### Paso 5.1 -- Observar el echo por defecto

Por defecto, Make imprime cada comando antes de ejecutarlo. Ya lo hemos visto:

```bash
touch calc.c
make
```

Salida esperada:

```
gcc -Wall -Wextra -c calc.c
gcc -o calculator main_calc.o calc.o
```

Cada linea que empieza con `gcc` es Make imprimiendo el comando (echo), seguido
de su ejecucion.

### Paso 5.2 -- Agregar una regla clean con @

Agrega la siguiente regla al final de tu Makefile:

```makefile
clean:
	@echo "Cleaning build artifacts..."
	rm -f calculator main_calc.o calc.o
```

```bash
make clean
```

Salida esperada:

```
Cleaning build artifacts...
rm -f calculator main_calc.o calc.o
```

Observa la diferencia:

- La linea con `@echo` muestra solo la salida del echo, no el comando en si
  (porque `@` silencia el echo de Make)
- La linea con `rm` muestra el comando completo (Make lo imprime antes de
  ejecutarlo)

### Paso 5.3 -- Experimentar con el prefijo - (ignorar errores)

Reconstruye primero:

```bash
make
```

Ahora intenta borrar un archivo que no existe:

```bash
make clean
make clean
```

Predice: el segundo `make clean` fallara? Los archivos ya fueron eliminados
por el primer `make clean`.

Intenta predecir antes de continuar al siguiente paso.

### Paso 5.4 -- Verificar el comportamiento de rm -f

Salida esperada del segundo `make clean`:

```
Cleaning build artifacts...
rm -f calculator main_calc.o calc.o
```

No falla porque `rm -f` no reporta error cuando los archivos no existen. El
flag `-f` de `rm` hace innecesario el prefijo `-` de Make en este caso.

Ahora modifica la regla `clean` en el Makefile para usar `rm` sin `-f`:

```makefile
clean:
	@echo "Cleaning build artifacts..."
	rm calculator main_calc.o calc.o
```

```bash
make clean
```

Salida esperada:

```
Cleaning build artifacts...
rm calculator main_calc.o calc.o
rm: cannot remove 'calculator': No such file or directory
rm: cannot remove 'main_calc.o': No such file or directory
rm: cannot remove 'calc.o': No such file or directory
make: *** [Makefile:14: clean] Error 1
```

Sin `-f`, `rm` falla y Make se detiene con error.

### Paso 5.5 -- Usar el prefijo - para ignorar el error

Modifica la regla `clean` en el Makefile:

```makefile
clean:
	@echo "Cleaning build artifacts..."
	-rm calculator main_calc.o calc.o
	@echo "Clean complete"
```

```bash
make clean
```

Salida esperada:

```
Cleaning build artifacts...
rm calculator main_calc.o calc.o
rm: cannot remove 'calculator': No such file or directory
rm: cannot remove 'main_calc.o': No such file or directory
rm: cannot remove 'calc.o': No such file or directory
make: [Makefile:14: clean] Error 1 (ignored)
Clean complete
```

Observa:

- `rm` sigue fallando y mostrando errores
- Pero Make muestra "Error 1 (ignored)" y **continua** ejecutando la siguiente
  linea
- "Clean complete" se imprime porque Make no se detuvo

En la practica, la forma idiomatica es `rm -f` en vez de `-rm`. Pero el
prefijo `-` es util cuando no hay equivalente "silencioso" del comando.

### Paso 5.6 -- Restaurar clean con rm -f

Modifica la regla `clean` a su forma idiomatica:

```makefile
clean:
	rm -f calculator main_calc.o calc.o
```

---

## Parte 6 -- make -n (dry run) y make -B (forzar)

**Objetivo**: Usar `make -n` para ver que haria Make sin ejecutar nada, y
`make -B` para forzar la reconstruccion completa ignorando timestamps.

### Paso 6.1 -- Reconstruir todo

```bash
make
```

Salida esperada:

```
gcc -Wall -Wextra -c main_calc.c
gcc -Wall -Wextra -c calc.c
gcc -o calculator main_calc.o calc.o
```

### Paso 6.2 -- Dry run cuando todo esta al dia

```bash
make -n
```

Salida esperada:

```
make: 'all' is up to date.
```

`make -n` muestra que haria Make. Como todo esta al dia, no hay nada que
mostrar. El flag `-n` (o `--dry-run`) **nunca ejecuta** comandos.

### Paso 6.3 -- Dry run despues de un cambio

```bash
touch calc.c
make -n
```

Salida esperada:

```
gcc -Wall -Wextra -c calc.c
gcc -o calculator main_calc.o calc.o
```

Muestra los comandos que Make ejecutaria, pero no los ejecuto realmente.
Verifica:

```bash
ls -l calc.o
```

El timestamp de `calc.o` no cambio -- `make -n` no ejecuto nada.

### Paso 6.4 -- Ejecutar de verdad y verificar

```bash
make
ls -l calc.o
```

Ahora si se recompilo `calc.o`. El dry run es util para previsualizar que hara
Make antes de una compilacion larga.

### Paso 6.5 -- Forzar reconstruccion con make -B

```bash
make
```

Salida esperada:

```
make: 'all' is up to date.
```

Todo esta al dia. Ahora fuerza la reconstruccion completa:

```bash
make -B
```

Salida esperada:

```
gcc -Wall -Wextra -c main_calc.c
gcc -Wall -Wextra -c calc.c
gcc -o calculator main_calc.o calc.o
```

`make -B` (o `--always-make`) ignora los timestamps y reconstruye todo. Es
util cuando:

- Los timestamps se corrompieron (por ejemplo, al copiar archivos de otro lugar)
- Sospechas que algo salio mal y quieres reconstruir desde cero
- Cambias flags del compilador y quieres que todo se recompile

### Paso 6.6 -- Combinar -n con -B

```bash
make -n -B
```

Salida esperada:

```
gcc -Wall -Wextra -c main_calc.c
gcc -Wall -Wextra -c calc.c
gcc -o calculator main_calc.o calc.o
```

Muestra que haria Make si reconstruyera todo, sin ejecutar nada. Compara con
el resultado real para verificar que el Makefile esta correcto.

---

## Limpieza final

```bash
make clean
rm -f Makefile Makefile_broken hello
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  calc.c  calc.h  hello.c  main_calc.c
```

---

## Conceptos reforzados

1. Una regla de Make tiene tres partes: **target** (que construir),
   **prerequisites** (de que depende) y **recipe** (como construirlo). La
   recipe DEBE empezar con un tabulador literal, no espacios.

2. El error "missing separator" aparece cuando Make encuentra espacios donde
   espera un TAB. Se diagnostica con `cat -A` buscando `^I` al inicio de
   las lineas de recipe.

3. Make compara **timestamps** (mtime), no contenido. Si el target es mas
   nuevo que todos sus prerequisites, Make no recompila. Si algun prerequisite
   es mas nuevo, o si el target no existe, Make ejecuta la recipe.

4. La **compilacion separada** (.c -> .o -> ejecutable) permite que Make
   recompile solo los archivos que cambiaron. Al modificar un .c, solo su .o
   se recompila y luego se re-enlaza el ejecutable.

5. Los **headers deben listarse como prerequisites** de los .o que los
   incluyen. Si se omite un header, Make no detecta cambios en el y no
   recompila, lo cual puede causar bugs silenciosos.

6. Make ejecuta la **primera regla** del Makefile por defecto. La convencion
   es que la primera regla sea `all`, que agrupa los targets principales.

7. El prefijo `@` silencia el echo de un comando (Make no lo imprime antes
   de ejecutarlo). El prefijo `-` ignora errores (Make continua aunque el
   comando falle). En la practica, `rm -f` es preferible a `-rm` para
   limpieza.

8. `make -n` (dry run) muestra los comandos que Make ejecutaria sin ejecutar
   ninguno. `make -B` fuerza la reconstruccion completa ignorando timestamps.
   Se pueden combinar (`make -n -B`) para previsualizar una reconstruccion
   total.
