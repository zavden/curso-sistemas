# Investigar un crash: flujo sistemático

## Índice

1. [El proceso de investigación](#el-proceso-de-investigación)
2. [Fase 1: Reproducir](#fase-1-reproducir)
3. [Fase 2: Aislar](#fase-2-aislar)
4. [Fase 3: Diagnosticar](#fase-3-diagnosticar)
5. [Fase 4: Corregir y verificar](#fase-4-corregir-y-verificar)
6. [Investigar crashes en C](#investigar-crashes-en-c)
7. [Investigar crashes en Rust](#investigar-crashes-en-rust)
8. [Crashes en producción](#crashes-en-producción)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## El proceso de investigación

Cuando un programa crashea, la tentación es lanzarse al código y buscar el bug.
Esto funciona con bugs triviales, pero con crashes complejos lleva a horas
perdidas persiguiendo pistas falsas. Un proceso sistemático es más efectivo.

```
┌──────────────────────────────────────────────────────────────┐
│            Flujo sistemático de investigación                 │
│                                                              │
│  ┌──────────────┐                                            │
│  │ 1. REPRODUCIR │  ¿Puedo hacer que crashee de forma        │
│  │              │  consistente?                              │
│  └──────┬───────┘                                            │
│         │ Sí                                                 │
│         ▼                                                    │
│  ┌──────────────┐                                            │
│  │  2. AISLAR   │  ¿Cuál es el caso mínimo que produce      │
│  │              │  el crash?                                 │
│  └──────┬───────┘                                            │
│         │                                                    │
│         ▼                                                    │
│  ┌──────────────┐                                            │
│  │3. DIAGNOSTICAR│  ¿Cuál es la causa raíz?                  │
│  │              │  (no el síntoma)                           │
│  └──────┬───────┘                                            │
│         │                                                    │
│         ▼                                                    │
│  ┌──────────────┐                                            │
│  │ 4. CORREGIR  │  Arreglar la causa raíz y verificar       │
│  │  Y VERIFICAR │  que el fix resuelve el problema          │
│  └──────────────┘                                            │
│                                                              │
│  Cada fase tiene un entregable claro.                        │
│  No avanzar a la siguiente sin completar la actual.          │
└──────────────────────────────────────────────────────────────┘
```

### Mentalidad de detective

```
┌──────────────────────────────────────────────────────────────┐
│  Principios de investigación                                 │
│                                                              │
│  1. No asumas — verifica cada hipótesis con evidencia        │
│  2. Un cambio a la vez — si cambias dos cosas, no sabes      │
│     cuál tuvo efecto                                         │
│  3. Documenta lo que intentaste — evita repetir callejones   │
│     sin salida                                               │
│  4. La causa raíz no es el síntoma — "segfault en línea 42"  │
│     es un síntoma; "buffer overflow en línea 30" es causa    │
│  5. El bug más simple que explica los datos es probablemente │
│     el correcto                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## Fase 1: Reproducir

La reproducción es el paso **más importante**. Un bug que no puedes reproducir
no puedes verificar que lo arreglaste.

### Recopilar información

Antes de tocar el código, recopila todo lo disponible:

```bash
# ¿Qué dice el usuario/log?
# - "Se cae cuando proceso archivos grandes"
# - "Solo pasa los lunes por la mañana"
# - "Funcionaba hasta la versión 2.3"

# ¿Qué evidencia existe?
# - Core dump
# - Logs de la aplicación
# - Logs del sistema (journalctl, dmesg)
# - Métricas (CPU, memoria, disco)

# Recopilar datos del sistema
uname -a                        # Versión del kernel
cat /etc/os-release             # Distribución
free -h                         # Memoria disponible
df -h                           # Espacio en disco
ulimit -a                       # Límites del proceso
```

### Niveles de reproducibilidad

```
┌──────────────────────────────────────────────────────────────┐
│  Nivel         │ Acción                                      │
├────────────────┼─────────────────────────────────────────────┤
│ Determinístico │ Ejecutar el mismo comando → crash siempre   │
│ (mejor caso)   │ → Ir directo a Fase 2                      │
├────────────────┼─────────────────────────────────────────────┤
│ Probabilístico │ Crash 1 de cada N ejecuciones               │
│                │ → Ejecutar en loop, identificar condiciones │
├────────────────┼─────────────────────────────────────────────┤
│ Condicional    │ Solo bajo ciertas condiciones (carga,       │
│                │ datos específicos, timing)                  │
│                │ → Replicar las condiciones                  │
├────────────────┼─────────────────────────────────────────────┤
│ Heisenbug      │ Desaparece al observar (logging, debugger)  │
│                │ → Probable race condition o UB              │
│                │ → Usar TSan, Miri, Valgrind                 │
├────────────────┼─────────────────────────────────────────────┤
│ No reproducible│ Solo pasó una vez                           │
│                │ → Analizar core dump, logs, git bisect      │
│                │ → Buscar bugs similares reportados          │
└────────────────┴─────────────────────────────────────────────┘
```

### Crear un script de reproducción

```bash
#!/bin/bash
# reproduce_crash.sh — Documenta exactamente cómo reproducir

set -e

# Entorno
export LANG=en_US.UTF-8
export TZ=UTC

# Datos de entrada que causan el crash
cat > /tmp/test_input.txt << 'EOF'
line1
line2 with special chars: áéíóú
a very long line that exceeds the buffer...
EOF

# Reproducir
echo "=== Intentando reproducir crash ==="
./target/debug/my_program --input /tmp/test_input.txt --threads 4

echo "=== Si llegamos aquí, no crasheó ==="
```

### Reproducir crashes intermitentes

```bash
# Ejecutar en loop hasta que crashee
for i in $(seq 1 1000); do
    echo "Intento $i..."
    if ! ./target/debug/my_program < test_input.txt 2>/dev/null; then
        echo "CRASH en intento $i"
        echo "Exit code: $?"
        break
    fi
done

# Variante: capturar el core dump
ulimit -c unlimited
for i in $(seq 1 1000); do
    ./target/debug/my_program < test_input.txt 2>/dev/null || {
        echo "Crash en intento $i, core dump generado"
        break
    }
done
```

### Reproducir bajo condiciones específicas

```bash
# Simular poca memoria
# (limitar a 100MB de memoria virtual)
ulimit -v 102400
./target/debug/my_program

# Simular disco lleno
# Crear un tmpfs pequeño
sudo mount -t tmpfs -o size=1M tmpfs /tmp/small_disk
./target/debug/my_program --output /tmp/small_disk/result.txt

# Simular latencia de red
# (requiere tc — traffic control)
sudo tc qdisc add dev lo root netem delay 100ms

# Simular errores de syscalls (strace fault injection)
strace -e inject=read:error=EIO:when=10 \
    ./target/debug/my_program < test_input.txt
```

---

## Fase 2: Aislar

Una vez que puedes reproducir el crash, el objetivo es encontrar el **caso mínimo
de reproducción** (Minimal Reproducible Example / MRE).

### ¿Por qué aislar?

```
┌──────────────────────────────────────────────────────────────┐
│  Input original: archivo de 10,000 líneas                    │
│  Programa: 50,000 líneas de código                           │
│  → Demasiadas variables, difícil diagnosticar                │
│                                                              │
│  Después de aislar:                                          │
│  Input mínimo: 3 líneas específicas                          │
│  Código relevante: 200 líneas en 2 funciones                 │
│  → Causa raíz visible                                        │
└──────────────────────────────────────────────────────────────┘
```

### Reducir el input

**Bisección manual del input:**

```bash
# Archivo original: 10,000 líneas
wc -l crash_input.txt
# 10000

# ¿La primera mitad causa crash?
head -5000 crash_input.txt > half1.txt
./my_program < half1.txt
# → No crash

# ¿La segunda mitad?
tail -5000 crash_input.txt > half2.txt
./my_program < half2.txt
# → CRASH

# Repetir con la segunda mitad
tail -2500 half2.txt > quarter.txt
./my_program < quarter.txt
# → No crash → el bug necesita algo de ambas mitades

# Combinar inicio de half2 con final
head -2500 half2.txt > combined.txt
./my_program < combined.txt
# → CRASH → reducido a 2500 líneas

# Continuar hasta encontrar las líneas mínimas
```

**Reducción automática con `creduce` (para C) o scripts:**

```bash
# Script de reducción automática para inputs de texto
#!/bin/bash
# reduce_input.sh — Elimina líneas una por una
INPUT="$1"
PROGRAM="./my_program"

cp "$INPUT" minimal.txt
LINES=$(wc -l < minimal.txt)

for i in $(seq 1 "$LINES"); do
    # Intentar sin la línea i
    sed "${i}d" minimal.txt > attempt.txt

    if ! $PROGRAM < attempt.txt 2>/dev/null; then
        # Sigue crasheando sin la línea → la línea no es necesaria
        cp attempt.txt minimal.txt
        echo "Línea $i eliminada, quedan $(wc -l < minimal.txt) líneas"
    fi
done

echo "Input mínimo: minimal.txt ($(wc -l < minimal.txt) líneas)"
```

### Reducir el código

```rust
// Paso 1: Identificar la función que crashea (backtrace)
// Supongamos que el backtrace apunta a process_record()

// Paso 2: Comentar todo excepto esa función
fn main() {
    // Comentar todo el pipeline normal
    // let config = parse_config();
    // let records = load_records(&config);
    // let result = process_pipeline(records);

    // Llamar directamente con datos hardcoded
    let test_record = Record {
        name: "test".to_string(),
        value: -1,  // El valor que causa el crash
    };
    process_record(&test_record);  // ← ¿Crashea?
}

// Paso 3: Si crashea, reducir process_record()
fn process_record(r: &Record) {
    // Comentar partes hasta encontrar la línea exacta
    let normalized = normalize(r.value);  // ← ¿Aquí?
    // let category = categorize(normalized);
    // let output = format_output(category);
    // output
}
```

### Aislar componentes con tests unitarios

```rust
// En vez de ejecutar todo el programa, crear un test dirigido
#[test]
fn test_crash_reproduction() {
    // Datos mínimos que causan el crash
    let input = vec![0xFF, 0x00, 0x80, 0x01];

    // La función específica que sospechamos
    let result = parse_header(&input);

    // Si crashea aquí, hemos aislado el bug
    assert!(result.is_ok());
}
```

---

## Fase 3: Diagnosticar

Con el caso mínimo en mano, ahora buscamos la **causa raíz**.

### Elegir la herramienta correcta

```
┌──────────────────────────────────────────────────────────────┐
│  Síntoma                    │ Herramienta principal          │
├─────────────────────────────┼────────────────────────────────┤
│ Segfault (SIGSEGV)          │ GDB + core dump                │
│ Abort (SIGABRT)             │ RUST_BACKTRACE + GDB           │
│ Bus error (SIGBUS)          │ GDB (alineación de memoria)    │
│ Panic en Rust               │ RUST_BACKTRACE=full            │
│ Stack overflow              │ GDB (verificar recursión)      │
│ Datos corruptos sin crash   │ Miri / Valgrind / watchpoints  │
│ "Funciona en debug,         │ Miri (UB optimizado por LLVM)  │
│  falla en release"          │                                │
│ Crash intermitente          │ TSan / Miri con seeds          │
│ Crash solo con datos        │ strace / ltrace + GDB          │
│ específicos                 │                                │
│ Crash en FFI                │ Valgrind + GDB                 │
│ OOM kill                    │ Valgrind --tool=massif / heaptrack│
└─────────────────────────────┴────────────────────────────────┘
```

### Proceso de diagnóstico con GDB

```bash
# 1. Ejecutar bajo GDB
gdb ./target/debug/my_program

# 2. Configurar para capturar el crash
(gdb) set args < minimal_input.txt
(gdb) run

# 3. Cuando crashea, examinar el estado
Program received signal SIGSEGV, Segmentation fault.
0x00005555555551a3 in process_record (r=0x7fffffffe100) at src/main.rs:42

# 4. Ver el backtrace
(gdb) bt
#0  process_record (r=0x7fffffffe100) at src/main.rs:42
#1  main () at src/main.rs:15

# 5. Examinar variables
(gdb) print r
(gdb) print r->value
(gdb) print r->name

# 6. Examinar memoria alrededor del crash
(gdb) x/16xb r       # 16 bytes en hex desde r
(gdb) info registers  # Estado de los registros
```

### Proceso de diagnóstico con RUST_BACKTRACE

```bash
# Panic en Rust: el backtrace es tu mejor amigo
RUST_BACKTRACE=full ./target/debug/my_program < minimal_input.txt

# Output típico:
thread 'main' panicked at 'index out of bounds: the len is 3
    but the index is 5', src/parser.rs:87:15
stack backtrace:
   0: std::panicking::begin_panic_handler
   ...
   5: my_program::parser::parse_field
             at ./src/parser.rs:87:15        ← Línea exacta
   6: my_program::parser::parse_record
             at ./src/parser.rs:42:20
   7: my_program::main
             at ./src/main.rs:15:5
```

Lectura del backtrace:
- Leer de **abajo hacia arriba** (la causa está arriba, el origen abajo)
- El frame más alto con **tu código** (no `std::`) es donde ocurrió el error
- La línea indica la operación que falló, no necesariamente la causa raíz

### La pregunta "¿por qué?" en cadena

```
┌──────────────────────────────────────────────────────────────┐
│  Técnica de los 5 "¿Por qué?"                               │
│                                                              │
│  Síntoma: Index out of bounds en parser.rs:87                │
│                                                              │
│  ¿Por qué? → El array tiene 3 elementos pero accedemos      │
│              al índice 5                                     │
│  ¿Por qué? → El campo "length" del header dice 5            │
│  ¿Por qué? → El header se parsea de un buffer raw           │
│  ¿Por qué? → El buffer viene de una lectura de red          │
│              sin validación                                  │
│  ¿Por qué? → No validamos que length <= data.len()           │
│              ← CAUSA RAÍZ                                    │
│                                                              │
│  Fix: validar length contra el tamaño real de los datos      │
│  antes de usarlo como índice                                 │
└──────────────────────────────────────────────────────────────┘
```

### Usar strace para crashes relacionados con I/O

```bash
# ¿El crash está relacionado con archivos, red, o syscalls?
strace -f -e trace=file,network -o trace.log \
    ./target/debug/my_program < minimal_input.txt

# Buscar syscalls que fallaron justo antes del crash
grep "= -1" trace.log | tail -20

# Ejemplo de lo que podrías encontrar:
# open("/tmp/cache.db", O_RDWR) = -1 EACCES (Permission denied)
# → El programa asume que puede abrir el archivo pero no tiene permisos
```

### Usar Valgrind para problemas de memoria (C/C++ y Rust FFI)

```bash
# Detectar accesos inválidos, use-after-free, leaks
valgrind --tool=memcheck --leak-check=full \
    ./target/debug/my_program < minimal_input.txt

# Output típico:
# ==12345== Invalid read of size 4
# ==12345==    at 0x4005E3: process_record (main.c:42)
# ==12345==  Address 0x51f1068 is 8 bytes after a block of size 12 alloc'd
# ==12345==    at 0x4C2AB80: malloc (vg_replace_malloc.c:299)
# ==12345==    at 0x400587: create_record (main.c:25)
#
# Diagnóstico: leemos 4 bytes más allá del buffer alocado
# El buffer es de 12 bytes, accedemos al offset 20 (12 + 8)
```

---

## Fase 4: Corregir y verificar

### Escribir el fix

Una vez identificada la causa raíz:

```rust
// ANTES (bug): no validamos length del header
fn parse_records(data: &[u8], header: &Header) -> Vec<Record> {
    let mut records = Vec::new();
    for i in 0..header.num_records {  // num_records viene del input
        let offset = i * RECORD_SIZE;
        records.push(parse_one_record(&data[offset..]));  // ← panic si offset > data.len()
    }
    records
}

// DESPUÉS (fix): validar antes de usar
fn parse_records(data: &[u8], header: &Header) -> Result<Vec<Record>, ParseError> {
    let expected_size = header.num_records
        .checked_mul(RECORD_SIZE)
        .ok_or(ParseError::Overflow)?;

    if expected_size > data.len() {
        return Err(ParseError::InvalidLength {
            expected: expected_size,
            actual: data.len(),
        });
    }

    let mut records = Vec::new();
    for i in 0..header.num_records {
        let offset = i * RECORD_SIZE;
        records.push(parse_one_record(&data[offset..]));
    }
    Ok(records)
}
```

### Verificar el fix

```bash
# 1. El caso mínimo ya no crashea
./target/debug/my_program < minimal_input.txt
echo "Exit code: $?"  # Debe ser 0 (o error manejado, no crash)

# 2. El script de reproducción original ya no crashea
./reproduce_crash.sh

# 3. Los tests existentes siguen pasando
cargo test

# 4. Agregar test de regresión para este bug específico
```

### Test de regresión

```rust
// Agregar un test que HUBIERA detectado el bug
#[test]
fn test_parse_records_invalid_length() {
    // El header dice 100 records pero solo hay datos para 3
    let header = Header { num_records: 100 };
    let data = vec![0u8; 3 * RECORD_SIZE];

    let result = parse_records(&data, &header);
    assert!(result.is_err());

    match result {
        Err(ParseError::InvalidLength { expected, actual }) => {
            assert_eq!(expected, 100 * RECORD_SIZE);
            assert_eq!(actual, 3 * RECORD_SIZE);
        }
        _ => panic!("Expected InvalidLength error"),
    }
}

#[test]
fn test_parse_records_overflow() {
    // num_records tan grande que num_records * RECORD_SIZE overflow
    let header = Header { num_records: usize::MAX };
    let data = vec![0u8; 100];

    let result = parse_records(&data, &header);
    assert!(matches!(result, Err(ParseError::Overflow)));
}
```

### Verificar con herramientas de análisis

```bash
# Si el bug era de memoria, verificar con Valgrind
valgrind ./target/debug/my_program < minimal_input.txt
# "All heap blocks were freed -- no leaks are possible"

# Si había unsafe involucrado, verificar con Miri
cargo +nightly miri test test_parse_records

# Si era concurrente, verificar con TSan
RUSTFLAGS="-Zsanitizer=thread" \
    cargo +nightly test -Zbuild-std --target x86_64-unknown-linux-gnu
```

---

## Investigar crashes en C

Los crashes en C tienen patrones específicos por la ausencia de runtime checks.

### Segfault: el crash más común

```c
// Causa típica 1: NULL pointer dereference
struct Node *find(struct Node *head, int key) {
    while (head->key != key) {  // Si head es NULL → SIGSEGV
        head = head->next;
    }
    return head;
}

// Fix:
struct Node *find(struct Node *head, int key) {
    while (head != NULL && head->key != key) {
        head = head->next;
    }
    return head;  // Puede retornar NULL
}
```

```bash
# Diagnosticar con GDB
gdb ./program core
(gdb) bt
#0  0x0000000000401156 in find (head=0x0, key=42) at search.c:5
#                              ^^^^^ NULL!
(gdb) print head
$1 = (struct Node *) 0x0
# Confirmado: head es NULL
```

### Buffer overflow

```c
// Causa típica: escribir más allá del buffer
void read_name(int fd) {
    char buffer[64];
    read(fd, buffer, 256);  // Lee 256 bytes en buffer de 64!
}

// Herramientas para detectar:
// - Valgrind: "Invalid write of size 1"
// - ASan: "stack-buffer-overflow"
// - GDB con watchpoints en los límites del buffer
```

### Use-after-free

```c
struct Data *process(void) {
    struct Data *d = malloc(sizeof(struct Data));
    populate(d);
    free(d);
    return d;  // ← Retorna puntero a memoria liberada
}

// El caller usa d → comportamiento indefinido
// Puede funcionar por "suerte" hasta que otro malloc reutiliza esa memoria
```

```bash
# Valgrind lo detecta inmediatamente:
valgrind ./program
# ==123== Invalid read of size 8
# ==123==    at 0x401234: main (main.c:15)
# ==123==  Address 0x51f1040 is 0 bytes inside a block of size 32 free'd
# ==123==    at 0x4C2AB80: free (vg_replace_malloc.c:540)
# ==123==    at 0x401200: process (process.c:5)
```

### Investigación paso a paso en C

```bash
# 1. Compilar con información de debug y sin optimizaciones
gcc -g -O0 -fsanitize=address -o program program.c

# 2. Ejecutar con ASan activado
./program < input.txt
# ASan produce un reporte detallado si hay problema de memoria

# 3. Si no hay ASan, usar Valgrind
valgrind --tool=memcheck --track-origins=yes ./program < input.txt

# 4. Para debugging interactivo
gdb ./program
(gdb) break main
(gdb) run < input.txt
(gdb) step  # Avanzar línea por línea
```

---

## Investigar crashes en Rust

Rust tiene diferentes patrones de crash que C.

### Panics: el crash "controlado"

```
┌──────────────────────────────────────────────────────────────┐
│  Causas comunes de panic en Rust                             │
│                                                              │
│  • unwrap() / expect() en None o Err                         │
│  • Índice fuera de rango (array[i] con i >= len)             │
│  • Overflow aritmético (solo en debug)                       │
│  • slice::from_raw_parts con parámetros inválidos (unsafe)   │
│  • assert! / assert_eq! que falla                            │
│  • panic!() explícito o todo!() / unimplemented!()           │
└──────────────────────────────────────────────────────────────┘
```

```bash
# Paso 1: obtener backtrace completo
RUST_BACKTRACE=full cargo run 2>&1 | head -50

# Paso 2: si el panic es por unwrap(), buscar el contexto
# El mensaje de panic dice la línea exacta
# Mirar: ¿qué retornó None/Err y por qué?

# Paso 3: en tests
RUST_BACKTRACE=1 cargo test test_name -- --nocapture
```

### "Funciona en debug, falla en release"

Este es el síntoma clásico de **Undefined Behavior**:

```bash
# Debug funciona (sin optimizaciones, con overflow checks)
cargo run
# → OK

# Release falla (con optimizaciones, el compilador aprovecha UB)
cargo run --release
# → Crash o resultado incorrecto

# Diagnóstico: casi siempre es UB en código unsafe
# Paso 1: ejecutar con Miri
cargo +nightly miri test

# Paso 2: si Miri no puede (FFI), usar ASan
RUSTFLAGS="-Zsanitizer=address" \
    cargo +nightly test -Zbuild-std --target x86_64-unknown-linux-gnu
```

**¿Por qué pasa esto?** El compilador asume que no hay UB. En modo debug, las
optimizaciones no eliminan código "muerto" (que en realidad se alcanza por UB).
En release, LLVM optimiza basándose en esas asunciones y el programa se rompe.

### Panic vs abort vs segfault

```
┌──────────────────────────────────────────────────────────────┐
│  Tipo de crash   │ Señal    │ Backtrace │ Core dump          │
├──────────────────┼──────────┼───────────┼────────────────────┤
│ Panic (unwind)   │ -        │ RUST_     │ No                 │
│                  │          │ BACKTRACE │ (exit code 101)    │
├──────────────────┼──────────┼───────────┼────────────────────┤
│ Panic (abort)    │ SIGABRT  │ RUST_     │ Sí (si ulimit -c)  │
│ panic="abort"    │          │ BACKTRACE │                    │
├──────────────────┼──────────┼───────────┼────────────────────┤
│ Segfault         │ SIGSEGV  │ GDB       │ Sí (si ulimit -c)  │
│ (unsafe bug)     │          │           │                    │
├──────────────────┼──────────┼───────────┼────────────────────┤
│ Stack overflow   │ SIGSEGV  │ GDB       │ Sí                 │
│                  │          │ (limitado)│                    │
├──────────────────┼──────────┼───────────┼────────────────────┤
│ OOM kill         │ SIGKILL  │ Ninguno   │ No                 │
│                  │          │ (forzado) │ (dmesg para info)  │
└──────────────────┴──────────┴───────────┴────────────────────┘
```

### Investigar un OOM kill

```bash
# Verificar si fue OOM kill
dmesg | grep -i "oom\|killed"
# [12345.678] Out of memory: Killed process 9876 (my_program)

# Ver el consumo de memoria del programa
/usr/bin/time -v ./target/debug/my_program < input.txt
# Maximum resident set size: 4523456 kbytes

# Profiling de memoria con Valgrind
valgrind --tool=massif ./target/debug/my_program < input.txt
ms_print massif.out.12345

# O con heaptrack (más moderno)
heaptrack ./target/debug/my_program < input.txt
heaptrack_print heaptrack.my_program.12345.gz
```

---

## Crashes en producción

### Preparar para debugging post-mortem

```bash
# En el Cargo.toml de producción
[profile.release]
debug = 1          # Símbolos de debug (sin afectar rendimiento)
panic = "abort"    # Genera core dump en panic
strip = false      # No eliminar símbolos

# Configurar core dumps en el servidor
echo '/var/crashes/core.%e.%p.%t' | sudo tee /proc/sys/kernel/core_pattern
ulimit -c unlimited
```

### Recopilar información de un crash en producción

```bash
# 1. Guardar el core dump inmediatamente
# (los core dumps pueden ser eliminados por rotación)
cp /var/crashes/core.my_program.12345.* /tmp/crash_analysis/

# 2. Copiar el binario EXACTO que estaba corriendo
cp /usr/local/bin/my_program /tmp/crash_analysis/

# 3. Logs alrededor del momento del crash
journalctl -u my_program --since "2024-01-15 14:30" \
    --until "2024-01-15 14:35" > /tmp/crash_analysis/logs.txt

# 4. Estado del sistema al momento del crash
sar -r -f /var/log/sa/sa15 > /tmp/crash_analysis/memory.txt
sar -u -f /var/log/sa/sa15 > /tmp/crash_analysis/cpu.txt

# 5. Analizar el core dump
gdb /tmp/crash_analysis/my_program /tmp/crash_analysis/core.*
(gdb) bt
(gdb) info threads
(gdb) thread apply all bt
```

### Capturar información en el momento del crash

```rust
use std::panic;

fn setup_crash_handler() {
    // Hook para capturar panics antes de abortar
    panic::set_hook(Box::new(|info| {
        // Obtener backtrace
        let backtrace = std::backtrace::Backtrace::force_capture();

        // Log estructurado del crash
        eprintln!("=== CRASH REPORT ===");
        eprintln!("Time: {:?}", std::time::SystemTime::now());
        eprintln!("Thread: {:?}", std::thread::current().name());
        eprintln!("Panic: {}", info);
        eprintln!("Backtrace:\n{}", backtrace);
        eprintln!("=== END CRASH REPORT ===");

        // Opcionalmente: escribir a archivo, enviar a servicio de crash
        // reporting, etc.
    }));
}

fn main() {
    setup_crash_handler();
    // ... resto del programa
}
```

### Crash reporting con coredumpctl (systemd)

```bash
# Listar crashes recientes
coredumpctl list

# Ver detalles del último crash
coredumpctl info

# Abrir GDB con el último crash
coredumpctl gdb

# Exportar el core dump para análisis offline
coredumpctl dump -o /tmp/core_to_analyze
```

---

## Errores comunes

### 1. Saltar directamente al código sin reproducir primero

```
❌ "Vi el backtrace, sé cuál es el bug"
   → Aplicas un fix, pero no puedes verificar si realmente funciona
   → Puede ser un síntoma de un bug más profundo

✅ Primero reproducir, luego diagnosticar
   → Si no puedes reproducir, analiza core dump + logs
   → Siempre verifica el fix contra el caso de reproducción
```

### 2. Confundir síntoma con causa raíz

```
❌ "El crash es un index out of bounds en línea 87 → agregar un
   bounds check"
   → Solo ocultas el síntoma

✅ Preguntar "¿por qué el índice es inválido?"
   → Quizás el parser está leyendo un length corrupto
   → Quizás el buffer fue truncado por un error de I/O previo
   → La causa raíz puede estar 50 líneas antes del crash
```

### 3. No escribir test de regresión

```rust
// ❌ Arreglar el bug y seguir adelante
// → El mismo bug puede reaparecer en un refactor futuro

// ✅ Siempre agregar un test que reproduzca el bug original
#[test]
fn regression_issue_42_crash_on_empty_input() {
    // Este test reproduce el crash reportado en issue #42
    let result = process_data(&[]);
    assert!(result.is_ok());
}
```

### 4. Cambiar múltiples cosas al investigar

```
❌ "Cambié la alocación, el parseo y la validación → ya no crashea"
   → No sabes cuál cambio arregló el bug
   → Puede que los otros cambios introdujeron nuevos bugs

✅ Un cambio a la vez:
   1. Hipótesis: "el bug está en el parseo del length"
   2. Cambio: agregar validación de length
   3. Verificar: ¿sigue crasheando?
   4. Si sí → revertir, siguiente hipótesis
   5. Si no → este era el fix
```

### 5. No documentar la investigación

```
❌ Investigar por 3 horas, arreglar, y olvidar todo

✅ Documentar en el commit message o issue:
   "El crash ocurría porque parse_header() no validaba que
   num_records * RECORD_SIZE <= data.len(). Con input malicioso,
   num_records podía ser arbitrariamente grande, causando un
   index out of bounds en parse_records().

   Root cause: falta de validación en boundary de entrada.
   Fix: validar antes de iterar, retornar error si inválido.
   Regression test: test_parse_records_invalid_length"
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│           INVESTIGAR UN CRASH — CHEATSHEET                   │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  FASE 1: REPRODUCIR                                          │
│  Recopilar: logs, core dump, condiciones del entorno         │
│  Scripting: crear reproduce_crash.sh con pasos exactos       │
│  Intermitente: loop + ulimit -c unlimited                    │
│  Simular: ulimit -v (memoria), strace inject (I/O)           │
│                                                              │
│  FASE 2: AISLAR                                              │
│  Input: bisección (mitad → mitad → ...)                      │
│  Código: comentar secciones, test unitario directo           │
│  Meta: caso mínimo de reproducción (MRE)                     │
│                                                              │
│  FASE 3: DIAGNOSTICAR                                        │
│  Segfault    → gdb ./prog core → bt → print vars            │
│  Panic Rust  → RUST_BACKTRACE=full                           │
│  Memoria     → valgrind --tool=memcheck                      │
│  UB          → cargo +nightly miri test                      │
│  Races       → TSan: -Zsanitizer=thread                      │
│  I/O         → strace -e trace=file,network                  │
│  OOM         → dmesg | grep oom                              │
│  Técnica     → 5 "¿Por qué?" hasta la causa raíz            │
│                                                              │
│  FASE 4: CORREGIR Y VERIFICAR                                │
│  Fix         → Arreglar la causa raíz, no el síntoma         │
│  Verificar   → reproduce_crash.sh ya no crashea              │
│  Regresión   → Test que reproduce el bug original            │
│  Análisis    → Miri / Valgrind / TSan confirman fix          │
│  Documentar  → Commit message con causa raíz y fix           │
│                                                              │
│  HERRAMIENTA POR LENGUAJE                                    │
│  ┌─────────┬─────────────────┬───────────────────────┐      │
│  │         │ C               │ Rust                  │      │
│  │ Debug   │ gcc -g -O0      │ cargo build (default) │      │
│  │ Memoria │ Valgrind, ASan  │ Miri, Valgrind (FFI)  │      │
│  │ Trace   │ GDB, strace     │ RUST_BACKTRACE, GDB   │      │
│  │ Races   │ TSan, Helgrind  │ TSan, Miri            │      │
│  │ Reduce  │ creduce         │ Miri + test unitario  │      │
│  └─────────┴─────────────────┴───────────────────────┘      │
│                                                              │
│  PRODUCCIÓN                                                  │
│  [profile.release] debug=1, panic="abort"                    │
│  Core pattern: /var/crashes/core.%e.%p.%t                    │
│  panic::set_hook para crash reporting                        │
│  coredumpctl list / gdb para post-mortem                     │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Investigar un panic

Dado este programa que crashea con ciertos inputs:

```rust
use std::collections::HashMap;

fn parse_config(input: &str) -> HashMap<String, String> {
    let mut config = HashMap::new();
    for line in input.lines() {
        if line.starts_with('#') || line.is_empty() {
            continue;
        }
        let parts: Vec<&str> = line.splitn(2, '=').collect();
        let key = parts[0].trim().to_string();
        let value = parts[1].trim().to_string();  // ← Posible panic
        config.insert(key, value);
    }
    config
}

fn main() {
    let input = std::fs::read_to_string("config.txt").unwrap();
    let config = parse_config(&input);
    println!("Loaded {} settings", config.len());
}
```

**Tareas:**
1. Sin ejecutar, predice qué input causará el crash y cuál será el mensaje de error
2. Crea un archivo `config.txt` mínimo que reproduzca el crash
3. Ejecuta con `RUST_BACKTRACE=1` e interpreta el backtrace
4. Aplica la cadena de "5 ¿Por qué?" para llegar a la causa raíz
5. Corrige el código y escribe un test de regresión

**Pregunta de reflexión:** ¿Cuál es la diferencia entre "validar el input al
principio y rechazar lo inválido" vs "manejar cada caso inválido donde ocurre"?
¿Cuándo preferirías cada estrategia?

---

### Ejercicio 2: Diagnosticar un crash release-only

```rust
fn find_first_nonzero(data: &[u8]) -> usize {
    let mut i = 0;
    unsafe {
        let ptr = data.as_ptr();
        // Buscar el primer byte no-cero
        while *ptr.add(i) == 0 {
            i += 1;
            // BUG: no verificamos que i < data.len()
        }
    }
    i
}

fn main() {
    let data = vec![0u8; 100];  // Todo ceros
    let pos = find_first_nonzero(&data);
    println!("First nonzero at position: {}", pos);
}
```

**Tareas:**
1. Ejecuta en modo debug (`cargo run`) — ¿Qué observas?
2. Ejecuta en modo release (`cargo run --release`) — ¿Qué cambia?
3. Ejecuta con Miri (`cargo +nightly miri run`) — ¿Qué reporta?
4. Explica por qué el comportamiento difiere entre debug y release
5. Corrige el código (dos opciones: con unsafe seguro o sin unsafe)

**Pregunta de reflexión:** En este caso, ¿es el `unsafe` necesario para el
rendimiento, o es premature optimization? ¿Cómo se compara la versión
`data.iter().position(|&b| b != 0)` en benchmarks reales?

---

### Ejercicio 3: Workflow completo con un servidor TCP

Tienes un servidor TCP simple que crashea bajo carga:

```rust
use std::io::{Read, Write};
use std::net::{TcpListener, TcpStream};
use std::thread;

fn handle_client(mut stream: TcpStream) {
    let mut buffer = [0u8; 1024];
    let n = stream.read(&mut buffer).unwrap();
    let request = String::from_utf8(buffer[..n].to_vec()).unwrap();

    let response = match request.trim() {
        "PING" => "PONG\n",
        "TIME" => &format!("{:?}\n", std::time::SystemTime::now()),
        cmd => &format!("UNKNOWN: {}\n", cmd),
    };

    stream.write_all(response.as_bytes()).unwrap();
}

fn main() {
    let listener = TcpListener::bind("127.0.0.1:8080").unwrap();
    for stream in listener.incoming() {
        let stream = stream.unwrap();
        thread::spawn(|| handle_client(stream));
    }
}
```

El reporte dice: "El servidor crashea cuando se le envían datos binarios" y
"A veces crashea cuando muchos clientes se conectan y desconectan rápido".

**Tareas:**
1. Identifica al menos 3 puntos donde este código puede crashear (sin ejecutarlo)
2. Crea un script de reproducción que envíe datos binarios con `nc` o `curl`
3. Para cada `unwrap()`, decide: ¿debería ser un `?` con manejo de error, un
   `match`, o está bien como `unwrap()`?
4. ¿Hay un problema con el lifetime de `response` en los branches `TIME` y
   `UNKNOWN`? Explica qué pasa con la referencia a `format!()`
5. Corrige todos los problemas y verifica que el servidor maneja datos inválidos
   sin crashear

**Pregunta de reflexión:** En un servidor de producción, ¿un thread que paniquea
debería terminar todo el proceso o solo ese thread? ¿Cómo afecta la configuración
`panic = "abort"` vs `panic = "unwind"` a esta decisión?