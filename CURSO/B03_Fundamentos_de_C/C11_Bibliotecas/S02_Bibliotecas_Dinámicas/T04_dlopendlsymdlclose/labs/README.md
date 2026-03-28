# Lab -- dlopen, dlsym, dlclose

## Objetivo

Cargar bibliotecas compartidas en runtime con `dlopen`, obtener punteros a
funciones con `dlsym`, diagnosticar errores con `dlerror`, y descargar con
`dlclose`. Al finalizar, habras construido un sistema de plugins donde el
host descubre y carga `.so` en runtime y se pueden agregar plugins nuevos
sin recompilar el host.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `plugin.h` | Interfaz comun que todos los plugins implementan |
| `dlopen_basic.c` | Cargar libm en runtime, llamar cos() via dlsym |
| `dlerror_demo.c` | Manejo de errores: biblioteca no encontrada, simbolo inexistente |
| `plugin_upper.c` | Plugin que convierte texto a mayusculas |
| `plugin_reverse.c` | Plugin que invierte el texto |
| `plugin_count.c` | Plugin que cuenta caracteres, palabras y lineas |
| `host.c` | Programa host que descubre y carga plugins de plugins/ |

---

## Parte 1 -- dlopen y dlsym basico

**Objetivo**: Cargar `libm.so.6` en runtime con `dlopen`, obtener un puntero
a `cos()` con `dlsym`, usarlo como funcion normal, y cerrar con `dlclose`.

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat dlopen_basic.c
```

Observa:

- Se usa `dlopen("libm.so.6", RTLD_LAZY)` para cargar la biblioteca matematica
- Se llama `dlerror()` antes de `dlsym` para limpiar errores previos
- El puntero retornado por `dlsym` se castea al tipo correcto: `double (*)(double)`
- Despues de usar la funcion, se llama `dlclose` para liberar el handle

### Paso 1.2 -- Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic dlopen_basic.c -ldl -o dlopen_basic
```

Nota el flag `-ldl` al final. Enlaza con `libdl`, la biblioteca que provee
`dlopen`, `dlsym`, `dlclose` y `dlerror`.

### Paso 1.3 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- `dlopen` retorna un puntero opaco (`void *`). Que retorna si falla?
- `cos(0.0)` deberia dar un valor conocido. Cual?
- `cos(3.14159)` es aproximadamente cos(pi). Que valor esperas?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.4 -- Ejecutar y verificar

```bash
./dlopen_basic
```

Salida esperada:

```
dlopen("libm.so.6") succeeded
dlsym("cos") succeeded
cos(0.0) = 1.000000
cos(3.14159) = -1.000000
dlclose succeeded
```

Observa:

- `dlopen` retorno un puntero no nulo (exito)
- `dlsym` encontro el simbolo `cos` y retorno su direccion
- Se uso el puntero como funcion normal: `cosine(0.0)` llama a `cos(0.0)`
- La conversion de `void *` a function pointer se hace con `memcpy` porque
  ISO C prohibe el cast directo (es un workaround recomendado por POSIX)
- `dlclose` descargo la biblioteca y el handle ya no es valido

### Paso 1.5 -- Verificar que -ldl es necesario

Intenta compilar sin `-ldl`:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic dlopen_basic.c -o dlopen_basic_noldl 2>&1 || true
```

Salida esperada (puede variar segun la version de glibc):

```
/usr/bin/ld: /tmp/<...>.o: undefined reference to `dlclose'
/usr/bin/ld: /tmp/<...>.o: undefined reference to `dlsym'
/usr/bin/ld: /tmp/<...>.o: undefined reference to `dlopen'
/usr/bin/ld: /tmp/<...>.o: undefined reference to `dlerror'
```

En glibc 2.34+, estas funciones estan directamente en libc y el error no
aparece. Si tu sistema no produce error, igualmente incluye `-ldl` por
portabilidad.

### Paso 1.6 -- Inspeccionar los simbolos de libm

```bash
nm -D /lib64/libm.so.6 | grep " cos$"
```

Salida esperada:

```
<address> W cos
```

La `W` indica que `cos` es un weak symbol. `nm -D` muestra los simbolos
dinamicos exportados -- los mismos que `dlsym` puede encontrar.

### Limpieza de la parte 1

```bash
rm -f dlopen_basic dlopen_basic_noldl
```

---

## Parte 2 -- Manejo de errores con dlerror

**Objetivo**: Provocar errores de `dlopen` (archivo no encontrado) y `dlsym`
(simbolo inexistente), y diagnosticarlos con `dlerror`.

### Paso 2.1 -- Examinar el codigo fuente

```bash
cat dlerror_demo.c
```

Observa:

- Test 1: intenta abrir un `.so` inexistente con ruta relativa `./`
- Test 2: busca un simbolo inexistente en `libm.so.6`
- Test 3: busca `sin` (que si existe) como caso de exito para comparar
- Despues de cada `dlsym`, se verifica `dlerror()` en vez de solo comparar
  con NULL (patron seguro)

### Paso 2.2 -- Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic dlerror_demo.c -ldl -o dlerror_demo
```

### Paso 2.3 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- `dlopen("./libno_existe.so", ...)` fallara. Que tipo de mensaje dara `dlerror`?
- `dlsym(h2, "funcion_inventada")` retornara NULL. Que dira `dlerror`?
- `sin(pi/2)` deberia ser aproximadamente que valor?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.4 -- Ejecutar y verificar

```bash
./dlerror_demo
```

Salida esperada:

```
=== Test 1: library not found ===
dlopen failed: ./libno_existe.so: cannot open shared object file: No such file or directory

=== Test 2: symbol not found ===
dlsym failed: /lib64/libm.so.6: undefined symbol: funcion_inventada

=== Test 3: valid symbol ===
dlsym("sin") succeeded
sin(1.5708) = 1.000000

All tests completed
```

Observa:

- `dlerror` da mensajes descriptivos: incluye la ruta y la causa del error
- "cannot open shared object file" indica que el archivo no existe en el path
- "undefined symbol" indica que el simbolo no esta exportado en el `.so`
- `dlerror()` retorna NULL cuando no hay error (test 3 tuvo exito)

### Paso 2.5 -- RTLD_NOW vs RTLD_LAZY

En `dlerror_demo.c` se usa `RTLD_NOW`, que resuelve todos los simbolos al
cargar. Con `RTLD_LAZY`, los simbolos se resuelven la primera vez que se
usan. La diferencia importa cuando un `.so` depende de otros simbolos:

- `RTLD_NOW`: si falta un simbolo, `dlopen` falla inmediatamente (mas seguro)
- `RTLD_LAZY`: `dlopen` tiene exito, pero la funcion falla al llamarla (mas
  rapido para cargar, pero el error se detecta mas tarde)

Para plugins, `RTLD_NOW` es preferible porque detecta problemas al cargar.

### Limpieza de la parte 2

```bash
rm -f dlerror_demo
```

---

## Parte 3 -- Sistema de plugins

**Objetivo**: Construir un host que carga plugins desde un directorio,
usando una interfaz comun definida en `plugin.h`.

### Paso 3.1 -- Examinar la interfaz

```bash
cat plugin.h
```

Observa:

- La estructura `Plugin` define 5 campos: `name`, `version`, `init`,
  `process`, `cleanup`
- Cada plugin debe exportar `Plugin *plugin_create(void)`
- El host no conoce los plugins en compilacion -- solo busca `plugin_create`

### Paso 3.2 -- Examinar un plugin

```bash
cat plugin_upper.c
```

Observa:

- Las funciones `init`, `process`, `cleanup` son `static` (no se exportan
  directamente)
- Solo `plugin_create` es visible externamente -- es el punto de entrada
- `plugin_create` retorna un puntero a una estructura `Plugin` con las
  callbacks asignadas

### Paso 3.3 -- Examinar el host

```bash
cat host.c
```

Observa:

- `load_plugin` hace: `dlopen` -> `dlsym("plugin_create")` -> `create()` -> `init()`
- El `main` recorre `plugins/` buscando archivos `.so`
- Despues de procesar, llama `cleanup()` y `dlclose()` para cada plugin

### Paso 3.4 -- Compilar el primer plugin

```bash
mkdir -p plugins
gcc -std=c11 -Wall -Wextra -Wpedantic -shared -fPIC plugin_upper.c -o plugins/plugin_upper.so
```

`-shared` produce un `.so` en vez de un ejecutable. `-fPIC` genera codigo
posicion-independiente, necesario para bibliotecas compartidas.

### Paso 3.5 -- Verificar que el plugin exporta plugin_create

```bash
nm -D plugins/plugin_upper.so | grep plugin_create
```

Salida esperada:

```
<address> T plugin_create
```

La `T` indica que `plugin_create` esta en la seccion de texto (codigo) y es
visible. Las funciones `static` (`init`, `process`, `cleanup`) NO aparecen
porque no se exportan.

### Paso 3.6 -- Compilar el host

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic host.c -ldl -o host
```

### Paso 3.7 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- Cuantos plugins hay en `plugins/`? Solo uno: `plugin_upper.so`
- Que hara `plugin_upper` con el texto "Hello World"?
- En que orden se llaman las funciones: `init`, `process`, `cleanup`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.8 -- Ejecutar con un plugin

```bash
./host
```

Salida esperada:

```
[upper] initialized
Loaded plugin: uppercase v1.0

--- Processing "Hello World" with 1 plugin(s) ---
[upper] HELLO WORLD

--- Cleanup ---
[upper] cleaned up
```

Observa:

- El host encontro `plugin_upper.so` automaticamente
- Llamo `plugin_create` -> obtuvo la estructura -> llamo `init()`
- `process("Hello World")` convirtio el texto a mayusculas
- `cleanup()` se llamo antes de `dlclose()`

### Limpieza de la parte 3

No limpies todavia -- los plugins y el host se usan en la parte 4.

---

## Parte 4 -- Agregar plugins sin recompilar el host

**Objetivo**: Demostrar que se puede extender el sistema compilando un nuevo
plugin sin tocar el ejecutable host.

### Paso 4.1 -- Compilar el segundo plugin

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -shared -fPIC plugin_reverse.c -o plugins/plugin_reverse.so
```

### Paso 4.2 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- Ahora hay 2 archivos `.so` en `plugins/`. Cuantos plugins cargara el host?
- Recompilaste el host? No. Cambio algo en `host.c`? No.
- "Hello World" al reves es...?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.3 -- Ejecutar sin recompilar el host

```bash
./host
```

Salida esperada (el orden de los plugins puede variar):

```
[upper] initialized
Loaded plugin: uppercase v1.0
[reverse] initialized
Loaded plugin: reverse v1.0

--- Processing "Hello World" with 2 plugin(s) ---
[upper] HELLO WORLD
[reverse] dlroW olleH

--- Cleanup ---
[upper] cleaned up
[reverse] cleaned up
```

Observa:

- El host cargo ambos plugins sin haber sido recompilado
- Cada plugin se inicializo, proceso el texto a su manera, y se limpio
- Este es el patron de plugins: el host define la interfaz, los plugins la
  implementan, y se descubren en runtime

### Paso 4.4 -- Agregar un tercer plugin

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -shared -fPIC plugin_count.c -o plugins/plugin_count.so
```

```bash
./host
```

Salida esperada (el orden de los plugins puede variar):

```
[upper] initialized
Loaded plugin: uppercase v1.0
[reverse] initialized
Loaded plugin: reverse v1.0
[count] initialized
Loaded plugin: count v1.0

--- Processing "Hello World" with 3 plugin(s) ---
[upper] HELLO WORLD
[reverse] dlroW olleH
[count] chars=11 words=2 lines=0

--- Cleanup ---
[upper] cleaned up
[reverse] cleaned up
[count] cleaned up
```

Tres plugins, cero recompilaciones del host. Cada uno implementa la misma
interfaz `Plugin` de forma diferente.

### Paso 4.5 -- Verificar el file con file y ldd

```bash
file host
```

Salida esperada:

```
host: ELF 64-bit LSB executable, x86-64, ...
```

```bash
ldd host
```

Salida esperada (entre otras lineas):

```
	libdl.so.2 => /lib64/libdl.so.2 (<address>)
	libc.so.6 => /lib64/libc.so.6 (<address>)
```

Nota que los plugins NO aparecen en `ldd` -- no son dependencias de
compilacion. Se cargan en runtime con `dlopen`, por eso son invisibles para
el linker.

```bash
file plugins/plugin_upper.so
```

Salida esperada:

```
plugins/plugin_upper.so: ELF 64-bit LSB shared object, x86-64, ...
```

La diferencia: el host es `executable`, los plugins son `shared object`.

### Limpieza de la parte 4

```bash
rm -f host
rm -rf plugins/
```

---

## Limpieza final

```bash
rm -f dlopen_basic dlopen_basic_noldl dlerror_demo host
rm -rf plugins/
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  dlerror_demo.c  dlopen_basic.c  host.c  plugin.h  plugin_count.c  plugin_reverse.c  plugin_upper.c
```

---

## Conceptos reforzados

1. `dlopen` carga una biblioteca compartida en runtime y retorna un handle
   opaco (`void *`). Si falla, retorna NULL y `dlerror()` describe el error.
   El flag `-ldl` es necesario para enlazar con la API de carga dinamica.

2. `dlsym` busca un simbolo (funcion o variable) por nombre dentro de un
   handle abierto. Retorna un `void *` que se castea al tipo correcto. El
   patron seguro es: llamar `dlerror()` antes, luego `dlsym`, luego
   verificar `dlerror()` de nuevo (porque NULL puede ser un valor legitimo).

3. `dlerror` retorna un string con el ultimo error de `dlopen`/`dlsym`/`dlclose`,
   o NULL si no hubo error. Se limpia despues de llamarla, asi que solo se
   debe llamar una vez por operacion.

4. `dlclose` descarga la biblioteca y decrementa su contador de referencias.
   Despues de cerrar, los punteros obtenidos con `dlsym` son invalidos --
   usarlos causa undefined behavior.

5. `RTLD_NOW` resuelve todos los simbolos al cargar (falla inmediatamente si
   falta alguno). `RTLD_LAZY` resuelve simbolos bajo demanda (falla cuando
   se usa un simbolo faltante). Para plugins, `RTLD_NOW` es mas seguro.

6. `-shared -fPIC` produce un `.so` (shared object). `-shared` indica al
   linker que genere una biblioteca, y `-fPIC` genera codigo
   posicion-independiente necesario para que el `.so` se cargue en cualquier
   direccion de memoria.

7. El patron de plugins separa la interfaz (header con estructura y
   prototipo de `plugin_create`) de la implementacion (cada `.so`). El host
   solo conoce la interfaz en compilacion y descubre los plugins en runtime
   recorriendo un directorio. Agregar un plugin nuevo no requiere recompilar
   el host.

8. `nm -D` muestra los simbolos dinamicos exportados de un `.so`. Las
   funciones `static` no aparecen porque no se exportan. Solo los simbolos
   visibles externamente son accesibles via `dlsym`.
