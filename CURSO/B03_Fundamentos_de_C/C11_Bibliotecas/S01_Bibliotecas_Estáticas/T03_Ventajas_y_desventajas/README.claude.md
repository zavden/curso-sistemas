# T03 — Ventajas y desventajas del enlace estático

> Sin erratas detectadas en el material base.

---

## 1. Independencia total y portabilidad

Un binario enlazado estáticamente contiene **todo** lo que necesita para
ejecutarse. No depende de ninguna biblioteca externa en runtime:

```bash
gcc -static main.c -lmathutil -lm -o program

ldd program
# not a dynamic executable

# Se puede copiar a cualquier máquina Linux (misma arquitectura):
scp program server:/usr/local/bin/
# Funciona sin instalar dependencias.
```

Esto es ideal para:
- **CLI tools** distribuidos como un solo binario (ej: BusyBox)
- **Contenedores mínimos** (`FROM scratch` en Docker — sin libc, sin shell)
- **Sistemas embedded** donde no hay librerías compartidas
- **Rescue tools** para cuando el sistema está roto (libc dañada, etc.)
- **Cross-compilation**: compilar en la máquina de desarrollo, ejecutar en
  cualquier máquina de la misma arquitectura

Con enlace dinámico, el binario falla si no encuentra sus `.so`:

```
./program: error while loading shared libraries: libfoo.so: cannot open
shared object file: No such file or directory
```

---

## 2. Sin dependency hell y determinismo

### Dependency hell

Con bibliotecas dinámicas, puede ocurrir:

```bash
./program
# error: libfoo.so.2: No such file or directory
# El sistema tiene libfoo.so.3, pero el programa necesita .2.
```

Con enlace estático, la versión correcta está **dentro** del binario. No
importa qué versión tenga el sistema — no hay "DLL hell" ni
"dependency hell".

### Determinismo

El binario se comporta exactamente igual siempre:
- No depende de la versión de libc del sistema
- No depende de `LD_LIBRARY_PATH`
- No depende de `ldconfig`
- No se ve afectado por actualizaciones del sistema

Esto es importante para:
- **Builds reproducibles**: el mismo código produce el mismo comportamiento
- **Debugging**: el binario de producción es idéntico al de desarrollo
- **Certificación / auditoría**: el comportamiento es verificable
- **CI/CD**: los tests corren con exactamente las mismas dependencias

---

## 3. Rendimiento

El enlace estático tiene ventajas de rendimiento menores pero medibles:

```
Enlace dinámico:
  call printf → PLT stub → GOT → libc.so → printf real
  (indirección en cada llamada externa)

Enlace estático:
  call printf → printf (directo, sin indirección)
```

Beneficios concretos:
- **Sin overhead de PLT/GOT**: las llamadas a funciones son directas
- **Sin carga dinámica al inicio**: el linker dinámico (`ld-linux`) no
  necesita resolver símbolos al arrancar el proceso
- **Mejor locality de caché**: el código de las funciones de biblioteca está
  en el mismo binario, potencialmente cerca del código que las llama
- **LTO (Link Time Optimization)**: el linker puede optimizar a través de
  todos los archivos objeto, incluyendo los de la biblioteca

La diferencia es generalmente pequeña (< 5%), pero puede importar en:
- Programas que se ejecutan miles de veces por segundo (hooks de git,
  scripts de build)
- Hot loops que llaman funciones de biblioteca intensivamente
- Sistemas real-time con requisitos de latencia

---

## 4. Desventaja: tamaño del binario

```bash
gcc app_calc.c -L. -lmathutil -o calc_dynamic
gcc -static app_calc.c -L. -lmathutil -o calc_static

ls -lh calc_dynamic calc_static
#   ~16K   calc_dynamic
#  ~1.8M   calc_static   (~100x más grande)
```

La mayor parte del tamaño es **libc estática**. Un programa trivial que
solo usa `printf` ya paga ~1.8 MB porque incluye toda la maquinaria de
glibc.

### Impacto con múltiples programas

Si 100 programas en el sistema enlazaran libc estáticamente:
- Estático: 100 × 1.8 MB = **180 MB** en disco
- Dinámico: 100 × 16K + 1 × libc.so ≈ **3.6 MB** en disco

### Mitigación: musl libc

`musl` es una implementación alternativa de libc, más pequeña que glibc:
- Con glibc: binario estático ≈ 1.8 MB
- Con musl: binario estático ≈ 100 KB

Alpine Linux usa musl, por eso es popular para contenedores Docker con
binarios estáticos.

### `size`: desglose por secciones

```bash
size calc_static calc_dynamic
```

```
   text    data     bss     dec     hex filename
 ~780000  ~24000  ~33000  ~837000  ...  calc_static
  ~2000    ~600    ~16      ~2600  ...  calc_dynamic
```

- `text`: código ejecutable — el estático tiene ~400x más
- `data`: datos inicializados (strings, tablas)
- `bss`: datos sin inicializar (se inicializan a 0 en runtime)

---

## 5. Desventaja: actualizaciones de seguridad

Este es el argumento **más fuerte** contra enlace estático en producción.

```
Escenario: se descubre un bug de seguridad en libssl.

Con enlace dinámico:
  dnf update openssl    ← actualiza libssl.so
  → TODOS los programas usan la versión parcheada inmediatamente.
  → No hay que recompilar nada.

Con enlace estático:
  → Hay que RECOMPILAR cada programa que enlazó libssl estáticamente.
  → Si hay 20 programas → 20 recompilaciones y redespliegues.
  → Si se olvida alguno → vulnerabilidad activa en producción.
```

En la práctica, los equipos de seguridad prefieren enlace dinámico para
bibliotecas que manejan seguridad (OpenSSL, zlib, etc.) porque una sola
actualización del paquete corrige todos los programas afectados.

---

## 6. Desventaja: sin compartir memoria

Con enlace dinámico, el kernel puede compartir las **páginas de código**
(read-only) de una `.so` entre todos los procesos que la usan:

```
Enlace dinámico:
  libc.so se carga UNA vez en memoria física.
  100 procesos comparten las mismas páginas de código.
  Costo de memoria: ~2 MB total.

Enlace estático:
  Cada proceso tiene su propia copia de libc.
  100 procesos × 1.8 MB = 180 MB de memoria.
```

En la práctica, copy-on-write y el hecho de que cada binario usa un
subconjunto diferente de libc mitigan algo este efecto. Pero en servidores
con muchos procesos concurrentes (como workers de nginx o forks de
PostgreSQL), la diferencia de memoria puede ser significativa.

---

## 7. Desventaja: licencias

Las licencias de software libre pueden imponer obligaciones diferentes
según el tipo de enlace:

| Licencia | Enlace dinámico | Enlace estático |
|----------|----------------|----------------|
| **MIT/BSD/Apache** | OK, sin restricciones | OK, sin restricciones |
| **LGPL** (ej: glibc) | OK, no es "obra derivada" | El usuario debe poder re-enlazar (distribuir los `.o` o el fuente) |
| **GPL** | Obra derivada → distribuir todo el fuente | Obra derivada → distribuir todo el fuente |

En la práctica:
- **glibc** es LGPL → enlace estático requiere cuidado legal (debes
  permitir que los usuarios re-enlacen con otra versión de glibc)
- **musl libc** es MIT → enlace estático sin ninguna restricción
- Esto es otra razón por la que Alpine Linux (musl) es popular para
  contenedores con binarios estáticos

---

## 8. Limitaciones técnicas de glibc estática

Algunas funciones de glibc **no funcionan correctamente** con enlace
estático:

### NSS (Name Service Switch)

```bash
gcc -static prog.c -o prog
# warning: Using 'getaddrinfo' in statically linked applications
# requires at runtime the shared libraries from the glibc version
# used for linking
```

`getaddrinfo`, `gethostbyname` y otras funciones de resolución de nombres
usan **plugins dinámicos** (módulos NSS como `libnss_dns.so`). Con enlace
estático, estos módulos no pueden cargarse, lo que puede causar que la
resolución DNS falle o se limite a `/etc/hosts`.

### `iconv` (conversión de charset)

Similar a NSS — usa módulos dinámicos para soportar diferentes codificaciones.

### `dlopen` (carga dinámica de bibliotecas)

Por definición, requiere el linker dinámico. No funciona con `-static`.

### Solución: musl libc

`musl` implementa NSS, iconv y otras funciones internamente (sin plugins
dinámicos). Un binario estático con musl no tiene estas limitaciones.

---

## 9. Enlace estático parcial

En la práctica, lo más común es **mezclar**: bibliotecas propias estáticas
(portabilidad) y bibliotecas del sistema dinámicas (actualizaciones):

```bash
gcc main.c \
    -Wl,-Bstatic -lmylib -lmyparser \
    -Wl,-Bdynamic -lssl -lcrypto -lm -lc \
    -o program
```

```bash
ldd program
# libssl.so.3 => /lib64/libssl.so.3
# libcrypto.so.3 => /lib64/libcrypto.so.3
# libm.so.6 => /lib64/libm.so.6
# libc.so.6 => /lib64/libc.so.6
# (mylib y myparser NO aparecen — están dentro del binario)
```

Esto da lo mejor de ambos mundos:
- Las bibliotecas propias viajan con el binario (portabilidad)
- Las bibliotecas de seguridad se actualizan centralmente
- El tamaño es razonable (no incluye toda libc)

---

## 10. Cuándo usar cada tipo de enlace

```
┌────────────────────────────────────┬───────────────────────────────┐
│ Usar estático                      │ Usar dinámico                 │
├────────────────────────────────────┼───────────────────────────────┤
│ CLI tools (un solo binario)        │ Servidores de producción      │
│ Contenedores Docker mínimos        │ Aplicaciones de escritorio    │
│ Sistemas embedded / freestanding   │ Plugins / módulos cargables   │
│ Rescue / recovery tools            │ Software con parches de       │
│ Builds reproducibles               │   seguridad frecuentes        │
│ Cross-compilation                  │ Sistemas con muchos procesos  │
│ Entornos sin package manager       │   que comparten libc          │
│ Cuando la portabilidad es crítica  │ Bibliotecas del sistema       │
└────────────────────────────────────┴───────────────────────────────┘
```

### Proyectos reales

- **Go**: enlace estático por defecto (sin CGO). Un solo binario, sin
  dependencias. Es una decisión de diseño del lenguaje.
- **Rust**: enlace estático de crates por defecto. Con target
  `x86_64-unknown-linux-musl`, todo es estático incluyendo libc.
- **BusyBox**: todas las herramientas Unix en un solo binario estático.
- **Alpine Linux**: usa musl libc, facilita enlace estático.
- **Distroless containers**: binarios estáticos en contenedores sin OS.

### Resumen de tradeoffs

| Aspecto | Estático | Dinámico |
|---------|----------|----------|
| Portabilidad | Excelente | Requiere `.so` presentes |
| Tamaño binario | Grande | Pequeño |
| Uso de memoria | Duplicado por proceso | Compartido entre procesos |
| Actualizaciones | Recompilar todo | Actualizar `.so` una vez |
| Determinismo | Total | Depende del entorno |
| Licencias (LGPL) | Requiere cuidado | Sin restricciones |
| Rendimiento | Ligeramente mejor | Overhead de PLT/GOT |
| Funciones NSS/DNS | Limitadas (glibc) | Sin problemas |

---

## Ejercicios

### Ejercicio 1 — Compilar estático y dinámico, comparar tamaños

Compila `app_calc.c` con la biblioteca `libmathutil` en modo estático
completo y en modo dinámico. Compara tamaños.

```bash
cd labs/

# Preparar la biblioteca
gcc -std=c11 -Wall -Wextra -Wpedantic -c mathutil.c -o mathutil.o
ar rcs libmathutil.a mathutil.o
gcc -std=c11 -Wall -Wextra -Wpedantic -shared -fPIC mathutil.c -o libmathutil.so

# Compilar estático
gcc -std=c11 -Wall -Wextra -Wpedantic -static app_calc.c -L. -lmathutil -o calc_static

# Compilar dinámico
gcc -std=c11 -Wall -Wextra -Wpedantic app_calc.c -L. -lmathutil -o calc_dynamic \
    -Wl,-rpath,'$ORIGIN'

ls -lh calc_static calc_dynamic
```

<details><summary>Predicción</summary>

```
-rwxr-xr-x ...  ~16K ... calc_dynamic
-rwxr-xr-x ... ~1.8M ... calc_static
```

- `calc_static` es ~100x más grande porque incluye toda glibc.
- `calc_dynamic` es pequeño porque solo contiene el código de `app_calc.c`
  y las funciones de `libmathutil`; `printf`, `atoi`, etc. se cargan de
  `libc.so` en runtime.
- Si `-static` falla, instalar: `sudo dnf install glibc-static`.

</details>

---

### Ejercicio 2 — Inspeccionar con `ldd` y `file`

Usa `ldd` y `file` para verificar las dependencias de cada binario.

```bash
echo "=== calc_static ==="
ldd calc_static
file calc_static

echo ""
echo "=== calc_dynamic ==="
ldd calc_dynamic
file calc_dynamic
```

<details><summary>Predicción</summary>

`calc_static`:
```
not a dynamic executable
calc_static: ELF 64-bit LSB executable, ..., statically linked, ...
```

`calc_dynamic`:
```
linux-vdso.so.1 (0x...)
libmathutil.so => .../libmathutil.so (0x...)
libc.so.6 => /lib64/libc.so.6 (0x...)
/lib64/ld-linux-x86-64.so.2 (0x...)

calc_dynamic: ELF 64-bit LSB executable, ..., dynamically linked,
interpreter /lib64/ld-linux-x86-64.so.2, ...
```

- `calc_static` no tiene dependencias externas — "not a dynamic executable".
- `calc_dynamic` depende de `libmathutil.so` (nuestra), `libc.so.6` (sistema)
  y `ld-linux-x86-64.so.2` (linker dinámico).
- `file` confirma: "statically linked" vs "dynamically linked".

</details>

---

### Ejercicio 3 — Portabilidad: copiar a directorio aislado

Copia ambos binarios a un directorio vacío y prueba si funcionan.

```bash
mkdir -p portability_test
cp calc_static calc_dynamic portability_test/

portability_test/calc_static 7
portability_test/calc_dynamic 7
```

<details><summary>Predicción</summary>

`calc_static`:
```
mathutil version: 1.0.0
factorial(7)  = 5040
fibonacci(7)  = 13
is_prime(7)   = yes
```

`calc_dynamic`:
```
portability_test/calc_dynamic: error while loading shared libraries:
libmathutil.so: cannot open shared object file: No such file or directory
```

- El estático funciona perfectamente — no necesita nada externo.
- El dinámico falla porque `libmathutil.so` no está en `portability_test/`.
  El `-rpath '$ORIGIN'` busca la `.so` en el directorio del ejecutable,
  que ahora es `portability_test/`, donde no existe.
- `libc.so` sí se encuentra porque está en las rutas del sistema
  (`/lib64/`), pero `libmathutil.so` no.

</details>

---

### Ejercicio 4 — Corregir la portabilidad del dinámico

Copia la `.so` al directorio de prueba y verifica que ahora funciona.

```bash
cp libmathutil.so portability_test/
portability_test/calc_dynamic 7

ldd portability_test/calc_dynamic
```

<details><summary>Predicción</summary>

```
mathutil version: 1.0.0
factorial(7)  = 5040
fibonacci(7)  = 13
is_prime(7)   = yes
```

`ldd`:
```
linux-vdso.so.1 (0x...)
libmathutil.so => .../portability_test/libmathutil.so (0x...)
libc.so.6 => /lib64/libc.so.6 (0x...)
/lib64/ld-linux-x86-64.so.2 (0x...)
```

- Ahora funciona: `libmathutil.so` se encuentra en el mismo directorio
  que el ejecutable (gracias a `-rpath '$ORIGIN'`).
- `ldd` muestra que la ruta de `libmathutil.so` apunta a `portability_test/`.
- Esto demuestra que el binario dinámico **necesita distribuir la `.so`
  junto con el ejecutable**, mientras que el estático es autocontenido.

</details>

---

### Ejercicio 5 — Uso de disco: dos programas con la misma biblioteca

Compila `app_stats.c` también en ambos modos. Mide el espacio total.

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -static app_stats.c -L. -lmathutil -o stats_static
gcc -std=c11 -Wall -Wextra -Wpedantic app_stats.c -L. -lmathutil -o stats_dynamic \
    -Wl,-rpath,'$ORIGIN'

ls -lh calc_static stats_static calc_dynamic stats_dynamic libmathutil.so
```

<details><summary>Predicción</summary>

```
-rwxr-xr-x ...  ~16K ... calc_dynamic
-rwxr-xr-x ... ~1.8M ... calc_static
-rwxr-xr-x ...  ~16K ... stats_dynamic
-rwxr-xr-x ... ~1.8M ... stats_static
-rwxr-xr-x ...  ~16K ... libmathutil.so
```

Espacio total:
- **Estático**: 1.8M + 1.8M = **~3.6 MB**
- **Dinámico**: 16K + 16K + 16K (.so) = **~48 KB**

Con solo 2 programas, el estático usa ~75x más disco. En un sistema con
100 programas que comparten libc, la diferencia sería enorme.

- Cada binario estático tiene su propia copia completa de libc.
- Con dinámico, `libc.so` se comparte y `libmathutil.so` existe una sola vez.

</details>

---

### Ejercicio 6 — Actualizar la biblioteca dinámica sin recompilar

Reemplaza `libmathutil.so` con la versión 2.0.0 y verifica que el binario
dinámico la usa sin recompilar. Verifica que el estático sigue en v1.

```bash
# Estado actual
./calc_static 5
LD_LIBRARY_PATH=. ./calc_dynamic 5

# Actualizar solo la .so
gcc -std=c11 -Wall -Wextra -Wpedantic -shared -fPIC mathutil_v2.c -o libmathutil.so

# Verificar sin recompilar
LD_LIBRARY_PATH=. ./calc_dynamic 5
./calc_static 5
```

<details><summary>Predicción</summary>

Antes de actualizar (ambos v1.0.0):
```
mathutil version: 1.0.0
factorial(5)  = 120
fibonacci(5)  = 5
is_prime(5)   = yes
```

Después de actualizar la `.so`:

`calc_dynamic`:
```
mathutil version: 2.0.0
factorial(5)  = 120
fibonacci(5)  = 5
is_prime(5)   = yes
```

`calc_static`:
```
mathutil version: 1.0.0
factorial(5)  = 120
fibonacci(5)  = 5
is_prime(5)   = yes
```

- El dinámico usa v2.0.0 **sin haber sido recompilado** — el linker
  dinámico carga la nueva `.so` en cada ejecución.
- El estático sigue en v1.0.0 — la biblioteca está embebida en el binario.
- Para actualizar el estático hay que: recompilar `.o` → recrear `.a` →
  re-enlazar el programa.

</details>

---

### Ejercicio 7 — `size`: desglose de secciones

Compara el desglose de secciones de ambos binarios.

```bash
size calc_static calc_dynamic
```

<details><summary>Predicción</summary>

```
   text    data     bss     dec     hex filename
 ~780000  ~24000  ~33000  ~837000  ...  calc_static
  ~2000    ~600    ~16      ~2600  ...  calc_dynamic
```

- `text` (código ejecutable): el estático tiene ~400x más porque incluye
  toda la implementación de `printf`, `atoi`, `malloc`, etc. de libc.
- `data` (datos inicializados): el estático tiene más porque libc tiene
  tablas internas, locale data, etc.
- `bss` (datos sin inicializar): el estático tiene más porque libc reserva
  buffers internos (ej: buffer de stdio).
- `dec` (total en decimal): la suma de las tres secciones.

</details>

---

### Ejercicio 8 — Analizar dependencias de comandos del sistema

Investiga las dependencias dinámicas de comandos reales del sistema.

```bash
for cmd in ls grep git curl; do
    path=$(which $cmd 2>/dev/null)
    if [ -n "$path" ]; then
        echo "=== $cmd ==="
        ls -lh "$path" | awk '{print "  Size:", $5}'
        echo "  Dependencies: $(ldd "$path" 2>/dev/null | grep -c '=>')"
        echo ""
    fi
done
```

<details><summary>Predicción</summary>

Ejemplo típico en Fedora:
```
=== ls ===
  Size: ~140K
  Dependencies: ~4

=== grep ===
  Size: ~200K
  Dependencies: ~3

=== git ===
  Size: ~3M
  Dependencies: ~10

=== curl ===
  Size: ~250K
  Dependencies: ~15
```

- `ls` y `grep` son relativamente simples — pocas dependencias.
- `git` es grande (>3MB) y tiene muchas dependencias (libcurl, libssl,
  libz, libpcre, etc.).
- `curl` es pequeño pero tiene muchas dependencias (libssl, libcrypto,
  libz, libidn, libnghttp2, etc.) porque soporta múltiples protocolos.
- Si todos estos se enlazaran estáticamente, cada binario sería mucho
  más grande, y las bibliotecas compartidas (libc, libssl) estarían
  duplicadas en disco y memoria.

</details>

---

### Ejercicio 9 — Recompilar el estático tras la actualización

Actualiza el binario estático para usar la v2.0.0 de la biblioteca.

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -c mathutil_v2.c -o mathutil.o
ar rcs libmathutil.a mathutil.o
gcc -std=c11 -Wall -Wextra -Wpedantic -static app_calc.c -L. -lmathutil -o calc_static

./calc_static 5
```

<details><summary>Predicción</summary>

```
mathutil version: 2.0.0
factorial(5)  = 120
fibonacci(5)  = 5
is_prime(5)   = yes
```

- Ahora el estático también muestra v2.0.0.
- Fueron necesarios 3 pasos: recompilar `.o`, recrear `.a`, re-enlazar
  el programa.
- Con enlace dinámico, bastó un solo paso (recompilar la `.so`).
- Si hubiera 20 programas, con enlace estático habría que re-enlazar
  **cada uno**. Con dinámico, ninguno.

</details>

---

### Ejercicio 10 — Reflexión: elegir para escenarios concretos

Para cada escenario, decide si usarías enlace estático, dinámico o mixto.
No hay código — solo razonamiento.

1. Una herramienta CLI que se distribuye como un binario descargable
   desde GitHub.
2. Un servidor web en producción que usa OpenSSL para HTTPS.
3. Un programa para un contenedor Docker `FROM scratch`.
4. Una aplicación de escritorio con plugins cargables.
5. Un programa para un microcontrolador ARM sin sistema operativo.

<details><summary>Predicción</summary>

1. **Estático**. El usuario descarga un binario y lo ejecuta. No debería
   necesitar instalar dependencias. Ejemplo: muchas herramientas de Go
   y Rust se distribuyen así.

2. **Dinámico** (al menos para OpenSSL). Las vulnerabilidades de OpenSSL
   son frecuentes y críticas. Actualizar `libssl.so` corrige todos los
   servicios sin recompilar. Enlace mixto: bibliotecas propias estáticas,
   sistema dinámico.

3. **Estático**. `FROM scratch` no tiene libc ni ninguna biblioteca.
   El binario debe ser completamente autocontenido. Alternativa: usar
   `FROM alpine` (musl) con binario estático.

4. **Dinámico**. Los plugins son `.so` que se cargan con `dlopen()` en
   runtime. El enlace estático es incompatible con `dlopen()`.

5. **Estático** (freestanding). No hay sistema operativo ni linker
   dinámico. Todo el código debe estar en el binario. Se compila con
   `-nostdlib` y se provee solo lo mínimo necesario.

</details>

---

### Limpieza

```bash
rm -f mathutil.o app_calc.o app_stats.o
rm -f libmathutil.a libmathutil.so
rm -f calc_static calc_dynamic stats_static stats_dynamic
rm -rf portability_test
```
