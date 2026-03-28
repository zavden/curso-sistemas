# Lab -- Enlace dinamico

## Objetivo

Compilar un programa contra una biblioteca compartida (.so), experimentar el
error "cannot open shared object file", y resolverlo con los distintos
mecanismos de busqueda del dynamic linker: LD_LIBRARY_PATH, RUNPATH (-rpath),
y ldconfig. Al finalizar, sabras diagnosticar problemas de enlace dinamico con
`ldd` y `LD_DEBUG`, y entenderas la diferencia entre lazy binding e immediate
binding.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `mathops.h` | Header de la biblioteca: declaraciones de add() y mul() |
| `mathops.c` | Implementacion de la biblioteca con mensajes de traza |
| `main_dynlink.c` | Programa que llama a add() y mul() de libmathops.so |

---

## Parte 1 -- Compilar contra una biblioteca compartida

**Objetivo**: Crear una biblioteca compartida (.so) y enlazar un programa
contra ella. Observar que el programa compila correctamente pero falla al
ejecutarse si el loader no encuentra la .so.

### Paso 1.1 -- Examinar los archivos fuente

```bash
cat mathops.h
```

Observa las declaraciones de `add()` y `mul()`.

```bash
cat mathops.c
```

Observa que cada funcion imprime un mensaje de traza con `[libmathops]`.
Esto permitira ver cuando se ejecuta codigo de la biblioteca.

```bash
cat main_dynlink.c
```

Observa que `main()` incluye `mathops.h` y llama a `add()` y `mul()`.

### Paso 1.2 -- Compilar la biblioteca compartida

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -fPIC -c mathops.c -o mathops.o
gcc -shared mathops.o -o libmathops.so
```

Verifica que se creo:

```bash
ls -l libmathops.so
```

Salida esperada:

```
-rwxr-xr-x 1 <user> <group> ~16K <date> libmathops.so
```

### Paso 1.3 -- Enlazar el programa contra la .so

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic main_dynlink.c -L. -lmathops -o main_dynlink
```

Los flags de enlace:

- `-L.` indica al **linker** (en compilacion) donde buscar libmathops.so
- `-lmathops` indica que se enlaza contra `libmathops.so` (el prefijo `lib` y
  el sufijo `.so` son implicitos)

La compilacion debe terminar sin errores ni warnings.

### Paso 1.4 -- Predecir el resultado de la ejecucion

Antes de ejecutar, responde mentalmente:

- El programa compilo sin errores. Significa que va a ejecutarse sin problemas?
- `-L.` le dice al linker donde buscar en compilacion. El dynamic linker en
  runtime sabe donde esta libmathops.so?
- En que directorios busca el dynamic linker por defecto?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.5 -- Ejecutar y observar el error

```bash
./main_dynlink
```

Salida esperada:

```
./main_dynlink: error while loading shared libraries: libmathops.so: cannot open shared object file: No such file or directory
```

El programa compilo, pero **no puede ejecutarse**. El dynamic linker
(`ld-linux.so`) no encuentra `libmathops.so` porque el directorio actual (`.`)
no esta en su lista de busqueda por defecto.

Puntos clave:

- `-L` es para el **linker en compilacion** (link time)
- El **dynamic linker en runtime** tiene su propia lista de busqueda
- Son dos fases separadas con configuraciones independientes

### Paso 1.6 -- Verificar con ldd

```bash
ldd main_dynlink
```

Salida esperada:

```
	linux-vdso.so.1 (0x00007ff...)
	libmathops.so => not found
	libc.so.6 => /lib64/libc.so.6 (0x00007f...)
	/lib64/ld-linux-x86-64.so.2 (0x00007f...)
```

`libmathops.so => not found` confirma que el loader no sabe donde buscarla.

---

## Parte 2 -- LD_LIBRARY_PATH

**Objetivo**: Resolver el error de carga usando la variable de entorno
LD_LIBRARY_PATH para indicar al dynamic linker donde buscar la .so.

### Paso 2.1 -- Ejecutar con LD_LIBRARY_PATH

```bash
LD_LIBRARY_PATH=. ./main_dynlink
```

Salida esperada:

```
Dynamic linking demo
  [libmathops] add(3, 5) called
add(3, 5) = 8
  [libmathops] mul(4, 7) called
mul(4, 7) = 28
  [libmathops] mul(2, 3) called
  [libmathops] add(10, 6) called
add(10, mul(2, 3)) = 16
```

Ahora funciona. `LD_LIBRARY_PATH=.` le dice al dynamic linker que busque en el
directorio actual.

### Paso 2.2 -- Verificar con ldd usando LD_LIBRARY_PATH

```bash
LD_LIBRARY_PATH=. ldd main_dynlink
```

Salida esperada:

```
	linux-vdso.so.1 (0x00007ff...)
	libmathops.so => ./libmathops.so (0x00007f...)
	libc.so.6 => /lib64/libc.so.6 (0x00007f...)
	/lib64/ld-linux-x86-64.so.2 (0x00007f...)
```

Ahora `libmathops.so => ./libmathops.so` muestra que se encontro en el
directorio actual.

### Paso 2.3 -- Mover la biblioteca a un subdirectorio

```bash
mkdir -p libdir
mv libmathops.so libdir/
```

Predice: si ejecutas `LD_LIBRARY_PATH=. ./main_dynlink`, funcionara?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.4 -- Verificar con la biblioteca movida

```bash
LD_LIBRARY_PATH=. ./main_dynlink
```

Salida esperada:

```
./main_dynlink: error while loading shared libraries: libmathops.so: cannot open shared object file: No such file or directory
```

No funciona porque la .so ya no esta en `.`. Hay que actualizar el path:

```bash
LD_LIBRARY_PATH=./libdir ./main_dynlink
```

Salida esperada:

```
Dynamic linking demo
  [libmathops] add(3, 5) called
add(3, 5) = 8
...
```

LD_LIBRARY_PATH es fragil: si mueves la biblioteca, tienes que actualizar la
variable. Ademas afecta a **todos** los programas lanzados desde esa shell,
no solo al tuyo.

### Paso 2.5 -- Devolver la biblioteca a su lugar

```bash
mv libdir/libmathops.so .
```

---

## Parte 3 -- RUNPATH con -rpath

**Objetivo**: Embeber la ruta de busqueda dentro del propio ejecutable usando
`-Wl,-rpath`, eliminando la dependencia de LD_LIBRARY_PATH.

### Paso 3.1 -- Organizar en estructura app/

```bash
mkdir -p app/bin app/lib
cp libmathops.so app/lib/
```

### Paso 3.2 -- Compilar con -rpath usando $ORIGIN

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic main_dynlink.c \
    -L./app/lib -lmathops \
    -Wl,-rpath,'$ORIGIN/../lib' \
    -o app/bin/main_runpath
```

Nota importante: `$ORIGIN` se debe poner entre comillas simples para que el
shell no lo expanda. `$ORIGIN` es una variable especial que el dynamic linker
reemplaza por el directorio del ejecutable en runtime.

### Paso 3.3 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- El ejecutable esta en `app/bin/`. `$ORIGIN` se resolvera a `app/bin/`.
- `$ORIGIN/../lib` se resolvera a `app/lib/`. Ahi esta libmathops.so?
- Necesitas establecer LD_LIBRARY_PATH?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.4 -- Ejecutar sin LD_LIBRARY_PATH

```bash
./app/bin/main_runpath
```

Salida esperada:

```
Dynamic linking demo
  [libmathops] add(3, 5) called
add(3, 5) = 8
  [libmathops] mul(4, 7) called
mul(4, 7) = 28
  [libmathops] mul(2, 3) called
  [libmathops] add(10, 6) called
add(10, mul(2, 3)) = 16
```

Funciona sin LD_LIBRARY_PATH. El RUNPATH embebido le dice al loader donde
buscar.

### Paso 3.5 -- Verificar el RUNPATH embebido

```bash
readelf -d app/bin/main_runpath | grep -i runpath
```

Salida esperada:

```
 0x000000000000001d (RUNPATH)            Library runpath: [$ORIGIN/../lib]
```

El binario contiene la instruccion `$ORIGIN/../lib` que el dynamic linker
resuelve en runtime relativo al ejecutable.

### Paso 3.6 -- Verificar con ldd

```bash
ldd app/bin/main_runpath
```

Salida esperada:

```
	linux-vdso.so.1 (0x00007ff...)
	libmathops.so => /...ruta.../app/lib/libmathops.so (0x00007f...)
	libc.so.6 => /lib64/libc.so.6 (0x00007f...)
	/lib64/ld-linux-x86-64.so.2 (0x00007f...)
```

`ldd` resuelve `$ORIGIN` y muestra el path absoluto de libmathops.so.

### Paso 3.7 -- Portabilidad: mover todo el directorio app/

```bash
mv app /tmp/app_moved
/tmp/app_moved/bin/main_runpath
```

Salida esperada:

```
Dynamic linking demo
  [libmathops] add(3, 5) called
add(3, 5) = 8
...
```

Funciona incluso despues de mover el directorio completo. `$ORIGIN` se
resuelve al nuevo path del ejecutable, y `../lib` sigue apuntando a la
biblioteca. Esta es la ventaja de `$ORIGIN` sobre paths absolutos.

```bash
mv /tmp/app_moved app
```

### Paso 3.8 -- Comparar: el primer ejecutable (sin -rpath)

```bash
readelf -d main_dynlink | grep -i runpath
```

Salida esperada: sin salida (no tiene RUNPATH).

La diferencia:

- `main_dynlink` no tiene RUNPATH y depende de LD_LIBRARY_PATH o ldconfig
- `app/bin/main_runpath` tiene RUNPATH y es auto-contenido

---

## Parte 4 -- ldconfig y /etc/ld.so.conf

**Objetivo**: Instalar la biblioteca en el sistema usando ldconfig para que
cualquier programa la encuentre sin LD_LIBRARY_PATH ni -rpath.

Esta parte requiere acceso root (sudo). Si no tienes acceso root, puedes leer
los pasos y saltar a la parte 5.

### Paso 4.1 -- Instalar la biblioteca en /usr/local/lib

```bash
sudo cp libmathops.so /usr/local/lib/
```

### Paso 4.2 -- Predecir: funciona sin ldconfig?

Antes de ejecutar, responde mentalmente:

- La biblioteca esta en /usr/local/lib/. El dynamic linker revisa ese
  directorio?
- El dynamic linker usa un cache (/etc/ld.so.cache). Si no ejecutas ldconfig,
  el cache esta actualizado?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.3 -- Probar sin ejecutar ldconfig

```bash
./main_dynlink
```

El resultado depende de tu distribucion:

- En algunas (como Fedora), /usr/local/lib/ ya esta en /etc/ld.so.conf y el
  cache puede encontrar la biblioteca
- En otras, seguira fallando porque el cache no esta actualizado

Si falla, continua con el paso 4.4.

### Paso 4.4 -- Crear configuracion y actualizar cache

```bash
echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/lab-mathops.conf
sudo ldconfig
```

Verifica que la biblioteca esta en el cache:

```bash
ldconfig -p | grep mathops
```

Salida esperada:

```
	libmathops.so (libc6,x86-64) => /usr/local/lib/libmathops.so
```

### Paso 4.5 -- Ejecutar sin LD_LIBRARY_PATH ni -rpath

```bash
./main_dynlink
```

Salida esperada:

```
Dynamic linking demo
  [libmathops] add(3, 5) called
add(3, 5) = 8
  [libmathops] mul(4, 7) called
mul(4, 7) = 28
  [libmathops] mul(2, 3) called
  [libmathops] add(10, 6) called
add(10, mul(2, 3)) = 16
```

Ahora funciona porque ldconfig agrego libmathops.so al cache del sistema.

### Limpieza de la parte 4

```bash
sudo rm /usr/local/lib/libmathops.so
sudo rm /etc/ld.so.conf.d/lab-mathops.conf
sudo ldconfig
```

Verifica que se elimino del cache:

```bash
ldconfig -p | grep mathops
```

No debe producir salida.

---

## Parte 5 -- Diagnostico con ldd y LD_DEBUG

**Objetivo**: Usar ldd para listar dependencias y LD_DEBUG para trazar paso a
paso como el dynamic linker busca y carga las bibliotecas.

### Paso 5.1 -- ldd sobre un programa del sistema

```bash
ldd /usr/bin/ls
```

Salida esperada (puede variar):

```
	linux-vdso.so.1 (0x00007ff...)
	libselinux.so.1 => /lib64/libselinux.so.1 (0x00007f...)
	libcap.so.2 => /lib64/libcap.so.2 (0x00007f...)
	libc.so.6 => /lib64/libc.so.6 (0x00007f...)
	...
	/lib64/ld-linux-x86-64.so.2 (0x00007f...)
```

Cada linea muestra: nombre de la .so, la ruta donde se encontro, y la
direccion de carga en memoria.

### Paso 5.2 -- ldd sobre nuestro programa

```bash
ldd main_dynlink
```

`libmathops.so => not found` porque no esta ni en el cache ni en RUNPATH.

```bash
ldd app/bin/main_runpath
```

`libmathops.so => .../app/lib/libmathops.so` porque tiene RUNPATH.

### Paso 5.3 -- LD_DEBUG=libs: trazar la busqueda

```bash
LD_DEBUG=libs LD_LIBRARY_PATH=. ./main_dynlink 2>&1 | head -30
```

Salida esperada (simplificada):

```
      <pid>:	find library=libmathops.so [0]; searching
      <pid>:	 search path=./tls/haswell/x86_64:./tls/haswell:./tls/x86_64:./tls:./haswell/x86_64:./haswell:./x86_64:.		(LD_LIBRARY_PATH)
      <pid>:	  trying file=./tls/haswell/x86_64/libmathops.so
      <pid>:	  trying file=./tls/haswell/libmathops.so
      ...
      <pid>:	  trying file=./libmathops.so
      <pid>:
      <pid>:	calling init: .../libmathops.so
      ...
```

Observa:

- El loader prueba multiples subdirectorios (tls, haswell, x86_64) antes del
  directorio base
- Estas variantes permiten bibliotecas optimizadas para hardware especifico
- Una vez encontrada, llama a las funciones de inicializacion de la .so

### Paso 5.4 -- LD_DEBUG=libs sin LD_LIBRARY_PATH

```bash
LD_DEBUG=libs ./main_dynlink 2>&1 | head -25
```

Salida esperada (simplificada):

```
      <pid>:	find library=libmathops.so [0]; searching
      <pid>:	 search cache=/etc/ld.so.cache
      <pid>:	 search path=/lib64:/usr/lib64		(system search path)
      <pid>:	  trying file=/lib64/libmathops.so
      <pid>:	  trying file=/usr/lib64/libmathops.so
      <pid>:
      <pid>:	libmathops.so: cannot open shared object file: No such file or directory
```

Sin LD_LIBRARY_PATH, el loader busca en el cache y los directorios del sistema.
Como libmathops.so no esta en ninguno, falla.

### Paso 5.5 -- LD_DEBUG=bindings: ver resolucion de simbolos

```bash
LD_DEBUG=bindings LD_LIBRARY_PATH=. ./main_dynlink 2>&1 | grep mathops
```

Salida esperada (simplificada):

```
      <pid>:	binding file ./main_dynlink [0] to ./libmathops.so [0]: normal symbol `add'
      <pid>:	binding file ./main_dynlink [0] to ./libmathops.so [0]: normal symbol `mul'
```

Muestra como el loader conecta los simbolos del programa con los de la
biblioteca. Cada llamada a `add()` o `mul()` en el programa se resuelve a la
funcion correspondiente en libmathops.so.

---

## Parte 6 -- Lazy binding vs immediate binding

**Objetivo**: Entender la diferencia entre lazy binding (resolucion bajo
demanda, comportamiento por defecto) e immediate binding (resolucion al inicio
con LD_BIND_NOW).

### Paso 6.1 -- Observar lazy binding (por defecto)

```bash
LD_DEBUG=bindings LD_LIBRARY_PATH=. ./main_dynlink 2>&1 | grep -E "(binding|calling init|transferring)"
```

Observa que los bindings de `add` y `mul` aparecen intercalados con la salida
del programa. Esto es **lazy binding**: cada simbolo se resuelve la primera
vez que se llama, no al inicio.

### Paso 6.2 -- Predecir el efecto de LD_BIND_NOW

Antes de ejecutar, responde mentalmente:

- Con lazy binding, si un simbolo no existe, cuando se detecta el error?
- Con LD_BIND_NOW, cuando se resolverian **todos** los simbolos?
- Que ventaja tiene LD_BIND_NOW para detectar errores tempranamente?

Intenta predecir antes de continuar al siguiente paso.

### Paso 6.3 -- Activar immediate binding con LD_BIND_NOW

```bash
LD_BIND_NOW=1 LD_DEBUG=bindings LD_LIBRARY_PATH=. ./main_dynlink 2>&1 | grep -E "(binding|Dynamic)"
```

Salida esperada (simplificada):

```
      <pid>:	binding file ./main_dynlink [0] to ./libmathops.so [0]: normal symbol `add'
      <pid>:	binding file ./main_dynlink [0] to ./libmathops.so [0]: normal symbol `mul'
      ...
Dynamic linking demo
```

Observa que **todos** los bindings ocurren **antes** de que aparezca
"Dynamic linking demo". Con LD_BIND_NOW, el loader resuelve todos los simbolos
al cargar el programa, antes de ejecutar `main()`.

### Paso 6.4 -- Comparar el orden

La diferencia clave:

- **Lazy binding** (por defecto): los simbolos se resuelven cuando se usan por
  primera vez. Si una funcion nunca se llama, nunca se resuelve. Un simbolo
  faltante solo causa error cuando se intenta llamar.

- **Immediate binding** (LD_BIND_NOW=1): todos los simbolos se resuelven al
  inicio. El programa falla inmediatamente si falta algun simbolo, incluso uno
  que nunca se llamaria. Esto es mas seguro pero ligeramente mas lento al
  inicio.

### Paso 6.5 -- Immediate binding en compilacion con -z now

En lugar de usar la variable de entorno, se puede embeber la politica en el
binario:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic main_dynlink.c \
    -L. -lmathops -Wl,-z,now -o main_bindnow
```

```bash
readelf -d main_bindnow | grep BIND_NOW
```

Salida esperada:

```
 0x0000000000000018 (BIND_NOW)
```

```bash
readelf -d main_bindnow | grep FLAGS
```

Salida esperada (incluye BIND_NOW en los flags):

```
 0x000000000000001e (FLAGS)              BIND_NOW
 0x000000006ffffffb (FLAGS_1)            Flags: NOW
```

El binario siempre usara immediate binding, sin necesidad de la variable
LD_BIND_NOW.

### Limpieza de la parte 6

```bash
rm -f main_bindnow
```

---

## Limpieza final

```bash
rm -f main_dynlink main_bindnow
rm -f mathops.o libmathops.so
rm -rf app/ libdir/
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  main_dynlink.c  mathops.c  mathops.h
```

---

## Conceptos reforzados

1. `-L` y `-l` son flags del **linker en compilacion**. Le dicen donde buscar
   y contra que .so enlazar, pero NO afectan al dynamic linker en runtime.

2. El error "cannot open shared object file" ocurre cuando el dynamic linker
   (`ld-linux.so`) no encuentra la .so en su lista de busqueda. Que el programa
   compile no garantiza que ejecute.

3. `LD_LIBRARY_PATH` agrega directorios de busqueda para el dynamic linker.
   Es util para desarrollo pero fragil para produccion: afecta a todos los
   programas y depende de que la variable este establecida.

4. `-Wl,-rpath,'$ORIGIN/../lib'` embebe la ruta de busqueda en el binario.
   `$ORIGIN` se resuelve al directorio del ejecutable en runtime, lo que
   permite mover el directorio completo sin romper el enlace.

5. `ldconfig` actualiza el cache del sistema (`/etc/ld.so.cache`). Es la
   forma estandar de instalar bibliotecas para que todos los programas las
   encuentren. Requiere root y modificar `/etc/ld.so.conf.d/`.

6. `ldd` muestra las dependencias de un ejecutable y donde las resuelve el
   loader. `not found` indica que el loader no puede encontrar la biblioteca.

7. `LD_DEBUG=libs` traza la busqueda del loader directorio por directorio.
   `LD_DEBUG=bindings` muestra la resolucion de cada simbolo. Son las
   herramientas principales para diagnosticar problemas de enlace dinamico.

8. Con **lazy binding** (por defecto), los simbolos se resuelven la primera
   vez que se llaman. Con **immediate binding** (LD_BIND_NOW=1 o -Wl,-z,now),
   todos se resuelven al inicio. Immediate binding detecta simbolos faltantes
   antes de ejecutar `main()`.
