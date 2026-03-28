# T03 — Ventajas y desventajas del enlace estático

## Ventajas

### Independencia total

```bash
# Un binario estático no necesita nada instalado en el sistema:
gcc -static main.c -lvec3 -lm -o program

ldd program
# not a dynamic executable

# Se puede copiar a cualquier máquina Linux (misma arquitectura):
scp program server:/usr/local/bin/
# Funciona sin instalar dependencias.

# Ideal para:
# - Herramientas CLI que se distribuyen como un solo binario
# - Contenedores mínimos (FROM scratch en Docker)
# - Sistemas embedded donde no hay librerías compartidas
# - Rescue tools (cuando el sistema está roto)
```

### Sin problemas de versiones (dependency hell)

```bash
# Con bibliotecas dinámicas, puede pasar:
./program
# error: libfoo.so.2: No such file or directory
# El sistema tiene libfoo.so.3, pero el programa necesita .2.

# Con enlace estático, la versión correcta está DENTRO del binario.
# No importa qué versión tenga el sistema.
# No hay "DLL hell" / "dependency hell".
```

### Rendimiento ligeramente mejor

```c
// Con enlace estático:
// - No hay overhead de carga dinámica al iniciar
// - No hay indirección por PLT/GOT (ver T02 dinámicas)
// - El linker puede optimizar across .o files (LTO)
// - Mejor locality en la cache de instrucciones

// La diferencia es generalmente pequeña (< 5%),
// pero puede importar en:
// - Programas que se ejecutan miles de veces (scripts, git)
// - Hot loops que llaman funciones de biblioteca
// - Sistemas real-time con requisitos de latencia
```

### Determinismo

```c
// El binario se comporta exactamente igual siempre:
// - No depende de la versión de libc del sistema
// - No depende de LD_LIBRARY_PATH
// - No depende de ldconfig
// - No se ve afectado por actualizaciones del sistema

// Importante para:
// - Builds reproducibles
// - Debugging (el binario de producción == el de desarrollo)
// - Certificación / auditoría de software
```

## Desventajas

### Tamaño del binario

```bash
# Comparación de tamaño:
gcc main.c -lvec3 -lm -o program_dynamic
gcc main.c -static -lvec3 -lm -o program_static

ls -lh program_dynamic program_static
# program_dynamic:   20K
# program_static:  1.8M
# ¡90x más grande!

# La mayor parte del tamaño es libc estática.
# Si 100 programas en el sistema enlazaran libc estáticamente:
# 100 × 1.8 MB = 180 MB (vs ~2 MB compartidos con dinámica).

# Con musl libc en lugar de glibc, el binario estático es más pequeño:
# musl produce ~100 KB en lugar de 1.8 MB.
```

### No se actualiza automáticamente

```c
// Si se descubre un bug de seguridad en libssl:

// Con enlace dinámico:
// apt update && apt upgrade
// Actualiza libssl.so → TODOS los programas usan la versión nueva.
// No hay que recompilar nada.

// Con enlace estático:
// Hay que RECOMPILAR cada programa que use libssl.
// Si hay 20 programas → 20 recompilaciones y redespliegues.
// Si alguno se olvida → vulnerabilidad activa.

// Este es el argumento MÁS fuerte contra enlace estático
// en servidores de producción.
```

### Sin compartir memoria

```c
// Con enlace dinámico:
// libc.so se carga UNA vez en memoria.
// 100 procesos comparten las mismas páginas de código (read-only).
// Costo de memoria: ~2 MB total.

// Con enlace estático:
// Cada proceso tiene su propia copia de libc.
// 100 procesos × 1.8 MB = 180 MB de memoria.
// (En la práctica, copy-on-write mitiga algo esto.)
```

### Licencias

```c
// Algunas bibliotecas tienen licencias que OBLIGAN
// a distribuir el código fuente si se enlazan estáticamente:

// LGPL (ej: glibc):
// - Enlace dinámico: OK, no es "obra derivada"
// - Enlace estático: el usuario debe poder re-enlazar
//   (distribuir los .o o el código fuente)

// GPL:
// - Cualquier enlace (estático o dinámico): obra derivada
// - Hay que distribuir TODO el código fuente

// MIT/BSD/Apache:
// - Cualquier enlace: OK, sin restricciones

// En la práctica:
// glibc es LGPL → enlace estático requiere cuidado.
// musl libc es MIT → enlace estático sin problemas.
```

### Limitaciones técnicas

```c
// Algunas funciones de glibc NO funcionan con enlace estático:

// 1. NSS (Name Service Switch):
// getaddrinfo, gethostbyname usan plugins dinámicos (.so).
// Con -static, solo resuelven /etc/hosts, no DNS.
// gcc -static prog.c
// → warning: Using 'getaddrinfo' in statically linked applications
//   requires at runtime the shared libraries from the glibc version
//   used for linking

// 2. iconv (conversión de charset):
// Similar — usa módulos dinámicos.

// 3. dlopen (carga dinámica):
// Requiere el linker dinámico — no funciona con -static.

// Solución: usar musl libc que implementa todo internamente.
```

## Cuándo usar enlace estático

```
┌────────────────────────────────────┬───────────────────┐
│ Usar estático                      │ Usar dinámico     │
├────────────────────────────────────┼───────────────────┤
│ CLI tools distribuidos como binario│ Servidores de     │
│ Contenedores Docker mínimos        │   producción      │
│ Sistemas embedded                  │ Aplicaciones de   │
│ Rescue/recovery tools              │   escritorio      │
│ Builds reproducibles               │ Plugins/módulos   │
│ Cross-compilation                  │ Bibliotecas del   │
│ Entornos sin package manager       │   sistema         │
│ Herramientas de seguridad/forense  │ Software con      │
│ Cuando la portabilidad es crítica  │   actualizaciones │
│                                    │   frecuentes      │
└────────────────────────────────────┴───────────────────┘
```

```bash
# Proyectos que usan enlace estático:
# - Go: estático por defecto
# - Rust: estático por defecto (con musl)
# - BusyBox: herramientas Unix en un solo binario estático
# - Alpine Linux: usa musl libc, facilita enlace estático
# - Distroless containers: binarios estáticos en contenedores vacíos

# Proyectos que usan enlace dinámico:
# - La mayoría del software de escritorio (GNOME, KDE)
# - Servidores web (nginx, Apache)
# - Bases de datos (PostgreSQL, MySQL)
# - Cualquier cosa que use plugins
```

## Enlace estático parcial

```bash
# En la práctica, lo más común es mezclar:
# - Bibliotecas propias: estáticas (portabilidad)
# - Bibliotecas del sistema: dinámicas (actualizaciones)

gcc main.c \
    -Wl,-Bstatic -lmylib -lmyparser \
    -Wl,-Bdynamic -lssl -lcrypto -lm -lc \
    -o program

# mylib y myparser: copiadas dentro del binario.
# ssl, crypto, m, c: dinámicas, se resuelven en runtime.

# Verificar:
ldd program
# libssl.so.3 => /lib/x86_64-linux-gnu/libssl.so.3
# libcrypto.so.3 => ...
# libm.so.6 => ...
# libc.so.6 => ...
# (mylib y myparser NO aparecen — están dentro del binario)
```

---

## Ejercicios

### Ejercicio 1 — Comparar tamaños

```bash
# 1. Crear un programa que use strlen, printf, sqrt, qsort.
# 2. Compilar en 3 versiones:
#    a) Completamente dinámico (default)
#    b) Completamente estático (-static)
#    c) Mixto: una libpropia.a estática + sistema dinámico
# 3. Comparar tamaños con ls -lh
# 4. Comparar dependencias con ldd
# 5. Verificar que los tres producen el mismo resultado.
```

### Ejercicio 2 — Portabilidad

```bash
# 1. Compilar un programa -static en tu sistema.
# 2. Copiar el binario a un contenedor Docker mínimo:
#    FROM scratch
#    COPY program /program
#    CMD ["/program"]
# 3. Verificar que ejecuta correctamente.
# 4. Intentar lo mismo con un binario dinámico y explicar el error.
```

### Ejercicio 3 — Análisis de dependencias

```bash
# 1. Elegir 5 comandos del sistema (ls, grep, git, curl, python3).
# 2. Para cada uno, ejecutar:
#    ldd /usr/bin/comando
#    ls -lh /usr/bin/comando
# 3. Contar cuántas bibliotecas dinámicas usa cada uno.
# 4. ¿Cuál usa más? ¿Cuál menos?
# 5. Calcular cuánto disco se ahorraría si todas las bibliotecas
#    comunes se enlazaran estáticamente en cada binario.
```
