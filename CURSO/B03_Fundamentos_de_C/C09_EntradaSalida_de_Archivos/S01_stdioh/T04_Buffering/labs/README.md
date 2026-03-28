# Lab — Buffering

## Objetivo

Observar los tres modos de buffering de stdio en accion, forzar el flush con
fflush, configurar el buffer con setvbuf, y demostrar como el buffering puede
causar perdida de datos ante un crash. Al finalizar, sabras predecir cuando
el output se muestra en pantalla y cuando queda retenido en el buffer.

## Prerequisitos

- GCC instalado
- `strace` disponible (para la parte 3)
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `buf_modes.c` | Demuestra line buffering (stdout), unbuffered (stderr) y full buffering (archivo) |
| `fflush_demo.c` | Uso de fflush para forzar flush, terminal vs pipe |
| `setvbuf_demo.c` | Cambiar modo de buffering con setvbuf, contar syscalls |
| `crash_demo.c` | Programa que crashea: output perdido vs output seguro |

---

## Parte 1 — Tres modos de buffering

**Objetivo**: Observar la diferencia entre line buffering (stdout a terminal),
unbuffered (stderr) y full buffering (archivo).

### Paso 1.1 — Compilar el programa

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic buf_modes.c -o buf_modes
```

Verifica que compila sin warnings.

### Paso 1.2 — Ejecutar en terminal

```bash
./buf_modes
```

Observa con atencion lo que sucede segundo a segundo. Antes de ejecutar,
predice:

- Los mensajes de stderr aparecen inmediatamente o se acumulan?
- Los mensajes de stdout (los "tick") aparecen uno a uno o todos juntos?
- Cuando aparece "tick 1"?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.3 — Analizar el resultado

Lo que deberias haber observado:

```
[stderr] immediate 1       <- aparece inmediatamente (segundo 0)
[stderr] immediate 2       <- aparece inmediatamente (segundo 1)
[stdout] tick 1[stdout] tick 2[stdout] tick 3
                           <- los tres ticks aparecen juntos (segundo 2)
Wrote 3 lines to buf_output.txt
```

stderr es unbuffered: cada fprintf a stderr produce una syscall write
inmediatamente. stdout es line buffered en terminal: los "tick" sin '\n' quedan
en el buffer. Cuando el tercer tick incluye '\n', se flushea todo el buffer de
golpe.

### Paso 1.4 — Verificar el archivo creado

```bash
cat buf_output.txt
```

Salida esperada:

```
line one
line two
line three
```

El archivo se escribio correctamente porque fclose flushea el buffer. Si el
programa hubiera crasheado antes de fclose, estas lineas podrian haberse
perdido (lo veremos en la parte 4).

### Paso 1.5 — Redirigir stdout a archivo

```bash
./buf_modes > stdout_capture.txt 2>&1
```

```bash
cat stdout_capture.txt
```

Antes de mirar la salida, predice:

- El orden de las lineas sera el mismo que en terminal?
- stderr y stdout se intercalaran igual?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.6 — Analizar la redireccion

Salida esperada (el orden puede variar):

```
[stderr] immediate 1
[stderr] immediate 2
[stdout] tick 1[stdout] tick 2[stdout] tick 3
Wrote 3 lines to buf_output.txt
```

Cuando stdout se redirige a archivo, pasa de line buffered a **full buffered**.
Esto significa que ni siquiera '\n' fuerza el flush — solo se flushea cuando el
buffer se llena (8192 bytes tipicamente) o al cerrar el stream. Sin embargo,
stderr sigue siendo unbuffered siempre.

### Limpieza de la parte 1

```bash
rm -f buf_modes buf_output.txt stdout_capture.txt
```

---

## Parte 2 — fflush: forzar el flush

**Objetivo**: Usar fflush para controlar cuando los datos salen del buffer.

### Paso 2.1 — Compilar el programa

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic fflush_demo.c -o fflush_demo
```

### Paso 2.2 — Ejecutar en terminal

```bash
./fflush_demo
```

Observa:

- "Loading..." aparece inmediatamente (gracias a fflush)
- Despues de 2 segundos, " Done!" aparece

Sin fflush, "Loading..." quedaria en el buffer y no se veria hasta que
" Done!\n" fuerce el flush con el '\n'. La experiencia del usuario seria ver
nada durante 2 segundos y luego "Loading... Done!" de golpe.

### Paso 2.3 — Comparar con y sin fflush

Vamos a ver la diferencia quitando el fflush. Compila una version sin el:

```bash
cat > no_fflush.c << 'EOF'
#include <stdio.h>
#include <unistd.h>

int main(void) {
    printf("Loading...");
    /* No fflush here */
    sleep(2);
    printf(" Done!\n");
    return 0;
}
EOF

gcc -std=c11 -Wall -Wextra -Wpedantic no_fflush.c -o no_fflush
```

```bash
./no_fflush
```

Ahora todo aparece junto despues de 2 segundos. Compara con:

```bash
./fflush_demo
```

La diferencia es evidente: fflush hace que el progreso sea visible en tiempo
real.

### Paso 2.4 — fflush y redireccion a archivo

```bash
./fflush_demo > fflush_redir.txt
cat fflush_redir.txt
```

Salida esperada:

```
Loading... Done!
Check fflush_output.txt
```

Cuando stdout va a un archivo, es full buffered. fflush fuerza la escritura al
archivo en ese momento, pero el usuario no ve diferencia visual porque el
archivo se escribe de golpe al final. El beneficio de fflush con archivos es
para **durabilidad**: si el programa crashea, lo que se flusheo ya esta en
disco.

### Paso 2.5 — Verificar el archivo de fflush

```bash
cat fflush_output.txt
```

Salida esperada:

```
event 1
event 2
event 3
```

Los tres eventos estan porque el programa termino normalmente. En la parte 4
veremos que pasa si crashea antes de fclose.

### Limpieza de la parte 2

```bash
rm -f fflush_demo no_fflush no_fflush.c fflush_redir.txt fflush_output.txt
```

---

## Parte 3 — setvbuf: cambiar modo y tamano del buffer

**Objetivo**: Usar setvbuf para configurar el modo de buffering y observar el
efecto en las syscalls con strace.

### Paso 3.1 — Compilar el programa

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic setvbuf_demo.c -o setvbuf_demo
```

### Paso 3.2 — Ejecutar en terminal

```bash
./setvbuf_demo
```

Observa:

- "tick 1...", "tick 2...", "tick 3" aparecen uno a uno, cada segundo
- No hay '\n' hasta el final, pero los ticks aparecen inmediatamente

Esto sucede porque setvbuf configuro stdout como _IONBF (unbuffered). Cada
printf produce una syscall write al instante, sin importar si hay '\n' o no.

### Paso 3.3 — Contar syscalls con strace

Antes de ejecutar, predice:

- El programa escribe 10 lineas al archivo con _IONBF (unbuffered). Cuantas
  syscalls write se haran para el archivo?
- Despues escribe 3 mensajes a stdout (tambien _IONBF). Cuantas write para
  stdout?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.4 — Verificar con strace

```bash
strace -e trace=write ./setvbuf_demo 2>&1 | grep 'write('
```

Salida esperada (simplificada):

```
write(3, "line 0\n", 7)             = 7
write(3, "line 1\n", 7)             = 7
write(3, "line 2\n", 7)             = 7
...
write(3, "line 9\n", 7)             = 7
write(1, "Wrote 10 lines ...", ...)  = ...
write(1, "tick 1...", 9)            = 9
write(1, "tick 2...", 9)            = 9
write(1, "tick 3\n", 6)            = 6
```

Cada fprintf produce su propia write. Con _IONBF, 10 lineas = 10 syscalls.
Sin setvbuf (full buffered por defecto), las 10 lineas se acumularian y
se escribirian en 1 o 2 syscalls.

### Paso 3.5 — Comparar con full buffering

Crea una version con full buffering para comparar:

```bash
cat > setvbuf_full.c << 'EOF'
#include <stdio.h>

int main(void) {
    FILE *f = fopen("setvbuf_output.txt", "w");
    if (f == NULL) {
        perror("fopen");
        return 1;
    }

    /* Default: full buffered — no setvbuf needed */

    for (int i = 0; i < 10; i++) {
        fprintf(f, "line %d\n", i);
    }

    fclose(f);
    printf("Wrote 10 lines full-buffered to setvbuf_output.txt\n");
    return 0;
}
EOF

gcc -std=c11 -Wall -Wextra -Wpedantic setvbuf_full.c -o setvbuf_full
```

```bash
strace -e trace=write ./setvbuf_full 2>&1 | grep 'write('
```

Salida esperada:

```
write(3, "line 0\nline 1\nline 2\n...", ~70) = ~70
write(1, "Wrote 10 lines ...", ...)          = ...
```

Con full buffering, las 10 lineas se acumulan en el buffer y se escriben en una
sola syscall write. Esto es mucho mas eficiente: 1 syscall en lugar de 10.

### Paso 3.6 — El valor de BUFSIZ

```bash
cat > bufsiz.c << 'EOF'
#include <stdio.h>

int main(void) {
    printf("BUFSIZ = %d bytes\n", BUFSIZ);
    return 0;
}
EOF

gcc -std=c11 -Wall -Wextra -Wpedantic bufsiz.c -o bufsiz
./bufsiz
```

Salida esperada:

```
BUFSIZ = 8192 bytes
```

Este es el tamano por defecto del buffer de stdio. Cuando un stream tiene full
buffering, acumula hasta BUFSIZ bytes antes de hacer un write. Por eso 10
lineas cortas caben en un solo flush.

### Limpieza de la parte 3

```bash
rm -f setvbuf_demo setvbuf_full setvbuf_full.c bufsiz bufsiz.c setvbuf_output.txt
```

---

## Parte 4 — Problema del buffering: crash y output perdido

**Objetivo**: Demostrar que los datos en el buffer se pierden si el programa
crashea, y como protegerse con line buffering.

### Paso 4.1 — Compilar el programa

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic crash_demo.c -o crash_demo
```

### Paso 4.2 — Ejecutar sin modo seguro

```bash
./crash_demo
```

El programa termina con "Aborted" (SIGABRT). Ahora revisa el log:

```bash
cat crash_log.txt
```

Antes de mirar la salida, predice:

- El programa escribio 3 lineas al log antes del abort(). Cuantas aparecen
  en crash_log.txt?
- Por que el fclose nunca se ejecuto?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.3 — Analizar el resultado

```bash
cat crash_log.txt
```

Salida esperada:

```
(archivo vacio o con 0 bytes)
```

```bash
wc -c crash_log.txt
```

Salida esperada:

```
0 crash_log.txt
```

Cero bytes. Las tres lineas que el programa "escribio" con fprintf nunca
llegaron al disco. Estaban en el buffer de stdio (en memoria del proceso).
Cuando abort() mato al proceso, el SO libero la memoria y los datos se
perdieron. fclose nunca se ejecuto, asi que no hubo flush.

### Paso 4.4 — Ejecutar con modo seguro

```bash
./crash_demo --safe
```

```bash
cat crash_log_safe.txt
```

Salida esperada:

```
step 1: initialized
step 2: processing
step 3: about to do risky operation
```

Las tres lineas estan. El modo `--safe` usa `setvbuf(log, NULL, _IOLBF, 0)`
para hacer el archivo line buffered. Cada '\n' en fprintf fuerza un flush
automatico al disco. Aunque abort() mato al proceso, los datos ya estaban en
disco.

### Paso 4.5 — Comparar el tamano de los archivos

```bash
wc -c crash_log.txt crash_log_safe.txt
```

Salida esperada:

```
  0 crash_log.txt
 ~75 crash_log_safe.txt
```

La diferencia es clara: mismas tres fprintf, mismo abort(), pero el modo de
buffering determino si los datos sobrevivieron o no.

### Paso 4.6 — Verificar con strace

```bash
strace -e trace=write ./crash_demo 2>&1 | grep 'write(3'
```

Salida esperada:

```
(ninguna linea con write(3, ...))
```

Ningun write al file descriptor 3 (el archivo). Los fprintf no generaron
ninguna syscall porque todo quedo en el buffer de stdio.

```bash
strace -e trace=write ./crash_demo --safe 2>&1 | grep 'write(3'
```

Salida esperada:

```
write(3, "step 1: initialized\n", 20)         = 20
write(3, "step 2: processing\n", 19)          = 19
write(3, "step 3: about to do risky oper"..., 35) = 35
```

Con line buffering, cada fprintf con '\n' produce un write inmediato. Los datos
estan a salvo en disco antes del crash.

### Limpieza de la parte 4

```bash
rm -f crash_demo crash_log.txt crash_log_safe.txt
```

---

## Limpieza final

```bash
rm -f buf_modes fflush_demo no_fflush setvbuf_demo setvbuf_full bufsiz crash_demo
rm -f no_fflush.c setvbuf_full.c bufsiz.c
rm -f buf_output.txt stdout_capture.txt fflush_output.txt fflush_redir.txt
rm -f setvbuf_output.txt crash_log.txt crash_log_safe.txt
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  buf_modes.c  crash_demo.c  fflush_demo.c  setvbuf_demo.c
```

---

## Conceptos reforzados

1. stdout es **line buffered** cuando esta conectado a una terminal: los datos
   se flushean al encontrar '\n'. Sin '\n', el output queda retenido en el
   buffer hasta que algo fuerce el flush.

2. stderr es **unbuffered** siempre: cada fprintf a stderr produce una syscall
   write inmediata. Por eso se usa para mensajes de error y depuracion.

3. Los archivos abiertos con fopen son **full buffered** por defecto: los datos
   se acumulan hasta que el buffer (BUFSIZ bytes) se llena o se llama a fclose.

4. Cuando stdout se redirige a un archivo o pipe, cambia de line buffered a
   **full buffered**. Esto causa que ni '\n' fuerce el flush.

5. fflush fuerza la escritura del buffer al SO. Es imprescindible para output
   interactivo sin '\n' (barras de progreso, prompts) y para asegurar que datos
   criticos llegan a disco.

6. setvbuf permite cambiar el modo de buffering (_IOFBF, _IOLBF, _IONBF) y el
   tamano del buffer. Debe llamarse **antes** de cualquier operacion de I/O en
   el stream.

7. Con unbuffered (_IONBF), cada escritura genera una syscall. Esto es seguro
   pero ineficiente: 10 escrituras = 10 syscalls. Con full buffering, esas 10
   escrituras se agrupan en 1 syscall.

8. Si un programa crashea, los datos en el buffer de stdio se **pierden**. La
   solucion es usar fflush despues de escrituras criticas, o configurar el
   stream como line buffered con setvbuf.
