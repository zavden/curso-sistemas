# Lab — Fases de compilacion

## Objetivo

Ejecutar cada fase de GCC por separado, examinar los archivos intermedios que
produce cada una, y entender que ocurre en cada etapa del pipeline. Al
finalizar, sabras interpretar .i, .s y .o, y diagnosticar errores de enlace.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `phases.c` | Programa simple con macros y `#include` |
| `math_utils.h` | Header con prototipos de funciones |
| `math_utils.c` | Implementacion de funciones matematicas |
| `main_multi.c` | Programa principal que usa math_utils |
| `optimize.c` | Programa para comparar optimizacion |

---

## Parte 1 — Las 4 fases del pipeline

**Objetivo**: Detener GCC en cada fase y verificar que archivo produce cada una.

### Paso 1.1 — Verificar el entorno

```bash
gcc --version
```

Confirma que GCC esta instalado. Anota la version.

### Paso 1.2 — Compilacion completa

```bash
gcc phases.c -o phases
./phases
```

Salida esperada:

```
Hello, world! (0)
Hello, world! (1)
Hello, world! (2)
```

Esto ejecuto las 4 fases internamente. Ahora vamos a ejecutar cada una por
separado.

### Paso 1.3 — Fase 1: Preprocesamiento (-E)

```bash
gcc -E phases.c -o phases.i
```

Antes de examinar el archivo, predice:

- El archivo `phases.c` tiene ~10 lineas. Cuantas lineas tendra `phases.i`?
- Que paso con las macros `GREETING` y `REPEAT`?
- Que paso con el `#include <stdio.h>`?

### Paso 1.4 — Verificar la prediccion

```bash
wc -l phases.c phases.i
```

Salida esperada (los numeros pueden variar):

```
  11 phases.c
 ~800 phases.i
```

Cientos de lineas. Todo el contenido de `stdio.h` (y los headers que este
incluye) se inserto en `phases.i`. Ahora mira el final del archivo:

```bash
tail -15 phases.i
```

Observa:

- `GREETING` fue reemplazado por `"Hello"`
- `REPEAT` fue reemplazado por `3`
- No quedan directivas `#define` ni `#include`
- Los comentarios desaparecieron

### Paso 1.5 — Fase 2: Compilacion (-S)

```bash
gcc -S phases.c -o phases.s
```

```bash
wc -l phases.s
cat phases.s
```

Observa el assembly generado. Busca:

- La cadena `"Hello"` en la seccion `.rodata`
- La instruccion `call` a `printf`
- El valor `3` de `REPEAT` (busca una comparacion o un conteo)

### Paso 1.6 — Fase 3: Ensamblado (-c)

```bash
gcc -c phases.c -o phases.o
```

```bash
file phases.o
```

Salida esperada:

```
phases.o: ELF 64-bit LSB relocatable, x86-64, ...
```

La palabra clave es **relocatable** — las direcciones aun no son finales.

### Paso 1.7 — Fase 4: Enlace

```bash
gcc phases.o -o phases_from_obj
./phases_from_obj
```

Mismo resultado que antes. Ahora compara los tipos de archivo:

```bash
file phases.o phases_from_obj
```

Salida esperada:

```
phases.o:          ELF 64-bit LSB relocatable, ...
phases_from_obj:   ELF 64-bit LSB pie executable, ...
```

El `.o` es relocatable (incompleto). El ejecutable es un ELF completo con
direcciones resueltas.

### Limpieza de la parte 1

```bash
rm -f phases phases_from_obj phases.i phases.s phases.o
```

---

## Parte 2 — Inspeccion de archivos intermedios

**Objetivo**: Usar `nm`, `objdump`, `size` y `file` para entender que contiene
cada archivo intermedio.

### Paso 2.1 — Regenerar el archivo objeto

```bash
gcc -c phases.c -o phases.o
```

### Paso 2.2 — Tabla de simbolos con nm

```bash
nm phases.o
```

Salida esperada (el orden puede variar):

```
                 U printf
0000000000000000 T main
```

Interpretacion:

- `T main` — `main` esta **definido** aqui (T = text/code)
- `U printf` — `printf` esta **undefined** (necesita resolverse al enlazar)

Antes de continuar, predice: cuando enlacemos el ejecutable, el `U` de `printf`
desaparecera? O seguira siendo `U`?

### Paso 2.3 — Simbolos del ejecutable

```bash
gcc phases.o -o phases
nm phases | grep -E "main|printf"
```

Observa: `main` tiene una direccion real (ya no es 0). `printf` puede aparecer
como `U` todavia si se enlaza dinamicamente (se resolvera en runtime, no en
link time). Esto es normal con bibliotecas dinamicas.

```bash
nm -D phases | grep printf
```

Con `-D` (dynamic symbols) veras `printf` como simbolo dinamico — se resolvera
cuando el programa se ejecute.

### Paso 2.4 — Secciones con objdump

```bash
objdump -h phases.o
```

Busca estas secciones:

| Seccion | Contenido |
|---------|-----------|
| `.text` | Codigo ejecutable (las instrucciones de main) |
| `.rodata` | Datos de solo lectura (el string "Hello, world!") |
| `.data` | Variables globales inicializadas (aqui no hay) |
| `.bss` | Variables globales sin inicializar (aqui no hay) |

### Paso 2.5 — Codigo desensamblado

```bash
objdump -d phases.o
```

Observa las instrucciones de `main`. Busca la llamada a `printf`:

```
callq  0 <main+0x...>
```

La direccion es 0 (o un placeholder). El enlazador la completara. Ahora compara
con el ejecutable:

```bash
objdump -d phases | grep -A3 "call.*printf"
```

Ahora la llamada tiene una direccion real (apunta a la PLT de printf).

### Paso 2.6 — Tamano de secciones con size

```bash
size phases.o
size phases
```

Salida esperada (los numeros varian):

```
   text    data     bss     dec     hex filename
   ~100       0       0    ~100     ... phases.o
  ~1500     ~500      ~8  ~2000     ... phases
```

El ejecutable es mas grande porque incluye codigo de inicio (crt0, crti) y
la tabla PLT para enlace dinamico.

### Paso 2.7 — Dependencias del ejecutable

```bash
ldd phases
```

Salida esperada:

```
linux-vdso.so.1 (0x...)
libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x...)
/lib64/ld-linux-x86-64.so.2 (0x...)
```

El programa depende de libc (donde vive `printf`) y del linker dinamico.

### Limpieza de la parte 2

```bash
rm -f phases.o phases
```

---

## Parte 3 — Compilacion multi-archivo y errores de enlace

**Objetivo**: Compilar archivos separados, enlazarlos, y provocar los errores
clasicos de enlace.

### Paso 3.1 — Examinar los archivos fuente

```bash
cat math_utils.h
cat math_utils.c
cat main_multi.c
```

Observa la estructura: `main_multi.c` incluye `math_utils.h` (solo
declaraciones). La implementacion real esta en `math_utils.c`.

### Paso 3.2 — Compilar cada archivo por separado

```bash
gcc -c math_utils.c -o math_utils.o
gcc -c main_multi.c -o main_multi.o
```

Verifica los simbolos de cada uno:

```bash
nm math_utils.o
nm main_multi.o
```

Antes de mirar la salida, predice:

- En `math_utils.o`, `add` y `multiply` estaran como T (definido) o U (undefined)?
- En `main_multi.o`, `add` y `multiply` estaran como T o U?
- En `main_multi.o`, `printf` estara como T o U?

### Paso 3.3 — Verificar la prediccion

Resultados esperados:

`math_utils.o`:
```
0000000000000000 T add
0000000000000014 T multiply
```
Ambas definidas (T).

`main_multi.o`:
```
                 U add
0000000000000000 T main
                 U multiply
                 U printf
```
`main` definida, el resto undefined (U).

### Paso 3.4 — Enlazar

```bash
gcc math_utils.o main_multi.o -o calculator
./calculator
```

Salida esperada:

```
7 + 5 = 12
7 * 5 = 35
```

El enlazador conecto los `U` de `main_multi.o` con los `T` de `math_utils.o`.

### Paso 3.5 — Error: undefined reference

Que pasa si olvidamos enlazar `math_utils.o`?

```bash
gcc main_multi.o -o calculator_broken
```

Salida esperada (error):

```
undefined reference to `add'
undefined reference to `multiply'
```

El enlazador encontro `add` y `multiply` como undefined en `main_multi.o` pero
no encontro ninguna definicion. Sin `math_utils.o`, no hay quien defina esos
simbolos.

### Paso 3.6 — Error: multiple definition

Crea un archivo duplicado:

```bash
cat > extra_math.c << 'EOF'
int add(int a, int b) {
    return a + b + 1;
}
EOF

gcc -c extra_math.c -o extra_math.o
```

Ahora enlaza con los dos:

```bash
gcc math_utils.o extra_math.o main_multi.o -o calculator_dup
```

Salida esperada (error):

```
multiple definition of `add'
```

Dos archivos objeto definen `add` — el enlazador no sabe cual usar.

### Paso 3.7 — Todo de una vez

Tambien se puede compilar y enlazar en un solo comando:

```bash
gcc math_utils.c main_multi.c -o calculator_onestep
./calculator_onestep
```

GCC ejecuta las 4 fases para cada .c y luego enlaza todos los .o resultantes.

### Limpieza de la parte 3

```bash
rm -f math_utils.o main_multi.o calculator calculator_broken
rm -f extra_math.c extra_math.o calculator_dup calculator_onestep
```

---

## Parte 4 — Efecto de la optimizacion en el assembly

**Objetivo**: Ver como los flags de optimizacion cambian el codigo generado.

### Paso 4.1 — Examinar el codigo fuente

```bash
cat optimize.c
```

Observa:

- `square()` es una funcion simple
- `dead_code_example()` tiene un `if (0)` que nunca se ejecuta
- `main()` calcula `2 + 3` en una variable

### Paso 4.2 — Assembly sin optimizacion

```bash
gcc -S -O0 optimize.c -o optimize_O0.s
wc -l optimize_O0.s
```

### Paso 4.3 — Assembly con optimizacion

```bash
gcc -S -O2 optimize.c -o optimize_O2.s
wc -l optimize_O2.s
```

Antes de comparar, predice:

- El `if (0)` aparecera en la version -O2?
- La variable `val = 2 + 3` se calculara en runtime o el compilador la reemplazara por `5`?
- La funcion `square()` se llamara con `call` o el compilador la inlineara?

### Paso 4.4 — Comparar

```bash
diff optimize_O0.s optimize_O2.s | head -60
```

O examina directamente las diferencias clave:

```bash
echo "=== -O0: buscando dead code ==="
grep -c "This never runs" optimize_O0.s

echo "=== -O2: buscando dead code ==="
grep -c "This never runs" optimize_O2.s
```

Con -O0, la cadena "This never runs" sigue en el assembly (el compilador no
elimina nada). Con -O2, desaparece — el optimizador detecto que `if (0)` nunca
se ejecuta y elimino todo el bloque, incluyendo la cadena.

```bash
echo "=== -O0: llamadas a funciones ==="
grep "call" optimize_O0.s

echo "=== -O2: llamadas a funciones ==="
grep "call" optimize_O2.s
```

Con -O2, `square()` probablemente fue inlineada (sin `call square`). El
compilador calculo que es mas eficiente copiar el cuerpo de la funcion que
hacer una llamada.

### Paso 4.5 — Constant folding

Busca el valor `5` (resultado de `2 + 3`) en ambos assemblies:

```bash
echo "=== -O0 ==="
grep -n "\\$5\\|\\$2\\|\\$3" optimize_O0.s | head -5

echo "=== -O2 ==="
grep -n "\\$5\\|\\$2\\|\\$3" optimize_O2.s | head -5
```

Con -O0, veras los valores `2` y `3` por separado (se suman en runtime).
Con -O2, el compilador calculo `2 + 3 = 5` en compilacion y solo usa `5`
directamente. Esto se llama **constant folding**.

### Limpieza de la parte 4

```bash
rm -f optimize_O0.s optimize_O2.s
```

---

## Limpieza final

```bash
rm -f *.i *.s *.o phases phases_from_obj calculator calculator_* extra_math.c
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  main_multi.c  math_utils.c  math_utils.h  optimize.c  phases.c
```

---

## Conceptos reforzados

1. `gcc -E` produce un archivo `.i` con todo el texto expandido — los `#include`
   se reemplazan por cientos de lineas y las macros desaparecen.

2. `gcc -S` traduce C a assembly (`.s`). El assembly depende de la
   arquitectura (x86-64, ARM, etc.) y cambia drasticamente con optimizacion.

3. `gcc -c` produce un archivo objeto (`.o`) con codigo maquina
   **relocatable** — las direcciones de funciones externas son placeholders
   que el enlazador completara.

4. `nm` muestra la tabla de simbolos: `T` = definido, `U` = undefined. Los
   simbolos `U` deben resolverse al enlazar.

5. El enlazador conecta los `U` de un `.o` con los `T` de otro. Si falta una
   definicion, da "undefined reference". Si hay dos, da "multiple definition".

6. Con `-O2`, el compilador elimina dead code, inlinea funciones pequenas, y
   precalcula constantes (constant folding). Esto puede reducir el assembly a
   la mitad o menos.
