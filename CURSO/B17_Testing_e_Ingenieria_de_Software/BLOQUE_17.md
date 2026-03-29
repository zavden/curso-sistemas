# Bloque 17: Testing e Ingeniería de Software

**Objetivo**: Dominar las técnicas de testing para código C y Rust a nivel de
sistemas, desde unit tests hasta fuzzing. Complementar con las prácticas de
ingeniería de software esenciales para mantener un proyecto profesional.

---

## Ubicación en la ruta de estudio

```
B01 → B02 → B03 → B04 → B05 → B15 → B16 → ★ B17 ★
       Docker  Linux    C     Build  Rust   ED    Patrones  Testing+IS
```

### Prerequisitos obligatorios: B01–B05

| Bloque | Aporta a B17 |
|--------|-------------|
| B01 (Docker) | Entorno reproducible para tests, contenedores como entorno CI |
| B02 (Linux) | Shell scripting para automation, exit codes, señales |
| B03 (C) | Código C que testear, Valgrind/sanitizers como base |
| B04 (Build) | Make/CMake para integrar test targets, CTest |
| B05 (Rust) | cargo test como punto de partida, cargo bench |

### Prerequisito recomendado: B15 + B16

- **B15** proporciona código real (estructuras de datos) sobre el que escribir
  tests significativos, no ejemplos triviales.
- **B16** aporta patrones que hacen el código testeable (Strategy para
  inyección de dependencias, Facade para reducir superficie de test).

### Se puede empezar sin B15/B16

Sí. Los capítulos C01–C03 son autocontenidos. C04 (Benchmarking) se
beneficia de tener código de B15 para medir, y C05 (Ingeniería de Software)
es independiente de los demás bloques.

---

## Capítulo 1: Testing en C [A]

### Sección 1: Fundamentos
- **T01 - Test sin framework**: assert.h, funciones main() de prueba, convención de exit codes, por qué C no tiene testing built-in
- **T02 - Estructura de un test**: Arrange-Act-Assert, un test = una verificación, nombres descriptivos
- **T03 - Compilación condicional para tests**: #ifdef TEST, test targets en Makefile, separar código de producción y test
- **T04 - Fixtures y setup/teardown**: inicialización compartida, cleanup con goto o función, simular constructores/destructores

### Sección 2: Frameworks de Testing en C
- **T01 - Unity**: instalación, TEST_ASSERT macros, test runners, setUp/tearDown, integración con Make
- **T02 - CMocka**: assert_int_equal, expect_*, will_return, test groups, por qué elegir CMocka sobre Unity
- **T03 - CTest con CMake**: add_test(), ctest --output-on-failure, labels, fixtures, integración con el build system
- **T04 - Comparación de frameworks**: Unity vs CMocka vs Check vs MinUnit, criterios de elección

### Sección 3: Mocking y Test Doubles en C
- **T01 - Qué es un test double**: fake, stub, mock, spy — diferencias reales, no solo nomenclatura
- **T02 - Function pointer injection**: reemplazar dependencias vía struct de function pointers (Strategy = DI natural en C)
- **T03 - Link-time substitution**: compilar con implementación mock (mock_file.o en lugar de real_file.o), weak symbols
- **T04 - CMocka mocking**: wrapping functions con --wrap, will_return, mock(), expect_function_call

### Sección 4: Cobertura en C
- **T01 - gcov y lcov**: -fprofile-arcs -ftest-coverage, gcov *.c, lcov → HTML report
- **T02 - Interpretar cobertura**: líneas vs ramas vs condiciones, por qué 100% no significa correcto
- **T03 - Cobertura en CI**: integrar gcov con Make/CMake, umbrales mínimos, trend tracking

## Capítulo 2: Testing en Rust [A]

### Sección 1: Testing Built-in
- **T01 - Unit tests con #[test]**: #[cfg(test)], mod tests, assert!, assert_eq!, assert_ne!, mensajes custom
- **T02 - Tests que deben fallar**: #[should_panic], expected message, cuándo usar vs Result
- **T03 - Tests con Result<(), E>**: retornar Result en vez de panic, el operador ? en tests, errores descriptivos
- **T04 - Organización**: tests junto al código vs archivo tests.rs, convención del módulo tests, #[cfg(test)] para imports

### Sección 2: Integration Tests y Doc Tests
- **T01 - Integration tests**: directorio tests/, cada archivo es un crate separado, use mi_crate::, probar API pública
- **T02 - Doc tests**: ```rust en doc comments, se compilan y ejecutan, cuándo marcar no_run o ignore
- **T03 - Test helpers compartidos**: tests/common/mod.rs, fixtures reutilizables, test utilities crate
- **T04 - cargo test flags**: --test, --lib, --doc, --release, -- --nocapture, filtrado por nombre

### Sección 3: Property-Based Testing
- **T01 - Concepto**: generar inputs aleatorios, verificar propiedades invariantes, shrinking automático
- **T02 - proptest**: proptest!, any::<T>(), estrategias custom, ProptestConfig, regex estrategias
- **T03 - Diseñar propiedades**: round-trip (encode→decode = identity), invariantes (sorted después de sort), modelo oracle
- **T04 - proptest vs quickcheck**: diferencias de API, shrinking determinístico, cuándo usar cada uno

### Sección 4: Mocking en Rust
- **T01 - Trait-based dependency injection**: definir trait, implementación real + mock, inyectar en tests
- **T02 - mockall crate**: #[automock], expect_*, returning, times, sequences
- **T03 - Cuándo no mockear**: preferir integración real cuando es rápido, fake implementations sobre mocks complejos
- **T04 - Test doubles sin crates externos**: struct con campos configurables, closures como stubs

### Sección 5: Cobertura en Rust
- **T01 - cargo-tarpaulin**: instalación, cargo tarpaulin, output HTML/JSON, exclusiones
- **T02 - llvm-cov**: RUSTFLAGS="-C instrument-coverage", llvm-profdata, llvm-cov show, branch coverage
- **T03 - Cobertura realista**: excluir código no testeable (main, error paths triviales), focus on critical paths

## Capítulo 3: Fuzzing y Sanitizers [M]

### Sección 1: Fuzzing en C
- **T01 - Concepto de fuzzing**: generación de inputs, corpus, crash discovery, coverage-guided vs dumb
- **T02 - AFL++**: instalación, afl-cc, afl-fuzz, corpus inicial, diccionarios, interpretar crashes
- **T03 - libFuzzer**: LLVMFuzzerTestOneInput, compilar con -fsanitize=fuzzer, merge corpus
- **T04 - Escribir harnesses**: función target, parsear input, limitar recursos (timeout, memory)

### Sección 2: Fuzzing en Rust
- **T01 - cargo-fuzz**: instalación, cargo fuzz init, fuzz_target!, corpus, artifacts
- **T02 - Arbitrary trait**: derivar Arbitrary para structs custom, generar inputs estructurados
- **T03 - Fuzz + sanitizers**: RUSTFLAGS para ASan/MSan, compilar en nightly, interpretar reportes
- **T04 - Integrar fuzzing en workflow**: cuánto tiempo fuzzear, CI fuzzing (OSS-Fuzz), regression tests desde crashes

### Sección 3: Sanitizers como Red de Seguridad
- **T01 - AddressSanitizer (ASan)**: buffer overflow, use-after-free, double-free, stack overflow — en C y Rust unsafe
- **T02 - MemorySanitizer (MSan)**: lectura de memoria no inicializada, solo Clang/nightly Rust
- **T03 - UndefinedBehaviorSanitizer (UBSan)**: signed overflow, null deref, alignment — en C
- **T04 - ThreadSanitizer (TSan)**: data races, deadlocks, en C con pthreads y en Rust unsafe

## Capítulo 4: Benchmarking y Profiling [M]

### Sección 1: Microbenchmarking
- **T01 - Benchmarking correcto**: warmup, iteraciones, varianza, por qué `time ./programa` no basta
- **T02 - Criterion en Rust**: cargo bench, criterion::Criterion, black_box, groups, comparaciones, HTML reports
- **T03 - Benchmark en C**: clock_gettime(CLOCK_MONOTONIC), repeticiones manuales, media y desviación, evitar optimización del compilador
- **T04 - Trampas comunes**: dead code elimination, branch prediction warming, tamaño de input vs cache, benchmark vs realidad

### Sección 2: Profiling de Aplicaciones
- **T01 - perf en Linux**: perf stat (contadores), perf record + perf report (sampling), perf annotate (instrucciones)
- **T02 - Flamegraphs**: perf script → flamegraph.pl, cargo-flamegraph, identificar hotspots, interpretar la gráfica
- **T03 - Valgrind/Callgrind**: callgrind para profiling de instrucciones, kcachegrind para visualización, comparar runs
- **T04 - Profiling de memoria**: heaptrack (Linux), DHAT (Valgrind), peak memory, allocation hotspots

### Sección 3: Benchmarking Guiado por Hipótesis
- **T01 - Formular hipótesis**: "SoA será 3x más rápido para iteración columnar" → medir → confirmar o refutar
- **T02 - Benchmark comparativo**: mismo algoritmo en C vs Rust, AoS vs SoA, Vec vs LinkedList — presentar resultados
- **T03 - Regression benchmarks en CI**: comparar contra baseline, alertar si rendimiento degrada, criterion + cargo-criterion

## Capítulo 5: Ingeniería de Software [A]

### Sección 1: Git para Proyectos Reales
- **T01 - Modelo mental de Git**: working tree, staging area, commits como snapshots, refs como punteros
- **T02 - Branching strategies**: trunk-based, feature branches, Git Flow — cuándo usar cada uno, por qué trunk-based suele ganar
- **T03 - Commits atómicos**: un commit = un cambio lógico, mensajes descriptivos (conventional commits), por qué importa para bisect
- **T04 - Rebase vs merge**: cuándo rebase (local, limpieza), cuándo merge (preservar historia), interactive rebase para squash

### Sección 2: Code Review
- **T01 - Por qué code review**: detectar bugs, compartir conocimiento, mantener estilo, no es control sino colaboración
- **T02 - Qué buscar en un review**: corrección, claridad, tests, edge cases, seguridad — checklist práctico
- **T03 - Pull Requests efectivos**: descripción clara, diff pequeño, facilitar al reviewer, self-review antes de pedir review
- **T04 - Herramientas**: GitHub PR reviews, comentarios inline, suggestions, draft PRs

### Sección 3: CI/CD Básico
- **T01 - Concepto de CI**: compilar + testear en cada push, feedback rápido, "si no está en CI no existe"
- **T02 - GitHub Actions para C**: workflow YAML, matrix (gcc/clang, Ubuntu/macOS), steps: build, test, sanitizers, cobertura
- **T03 - GitHub Actions para Rust**: actions-rs, cargo test/clippy/fmt, matrix de toolchains, caching de target/
- **T04 - Pipeline completo**: lint → build → test → coverage → benchmark, fail fast, badges en README

### Sección 4: Versionado y Release
- **T01 - Semantic Versioning**: MAJOR.MINOR.PATCH, cuándo incrementar cada uno, pre-release, 0.x.y como inestable
- **T02 - Changelog**: CHANGELOG.md, formato Keep a Changelog, automatización con conventional commits
- **T03 - Release en Rust**: cargo publish, Cargo.toml metadata, docs.rs, yanking
- **T04 - Release en C**: tags de Git, tarball de fuentes, packaging básico (pkg-config .pc, install target en Make/CMake)
