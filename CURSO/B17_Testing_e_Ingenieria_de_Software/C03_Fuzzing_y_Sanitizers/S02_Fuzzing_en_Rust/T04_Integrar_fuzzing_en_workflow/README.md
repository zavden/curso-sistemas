# T04 - Integrar fuzzing en workflow: cuánto tiempo fuzzear, CI fuzzing (OSS-Fuzz), regression tests desde crashes

## Índice

1. [Fuzzing no es un evento, es un proceso](#1-fuzzing-no-es-un-evento-es-un-proceso)
2. [Cuánto tiempo fuzzear: la pregunta fundamental](#2-cuánto-tiempo-fuzzear-la-pregunta-fundamental)
3. [Rendimientos decrecientes: la curva de cobertura](#3-rendimientos-decrecientes-la-curva-de-cobertura)
4. [Métricas para decidir cuándo parar](#4-métricas-para-decidir-cuándo-parar)
5. [Cuánto tiempo por escenario](#5-cuánto-tiempo-por-escenario)
6. [Fuzzing local: flujo del desarrollador](#6-fuzzing-local-flujo-del-desarrollador)
7. [Regression tests desde crashes](#7-regression-tests-desde-crashes)
8. [Convertir crash a test unitario: paso a paso](#8-convertir-crash-a-test-unitario-paso-a-paso)
9. [Regression tests con cargo fuzz run -runs=0](#9-regression-tests-con-cargo-fuzz-run--runs0)
10. [Directorio de regression corpus](#10-directorio-de-regression-corpus)
11. [CI fuzzing: por qué y cómo](#11-ci-fuzzing-por-qué-y-cómo)
12. [Nivel 1: regression tests en CI](#12-nivel-1-regression-tests-en-ci)
13. [Nivel 2: fuzzing breve en cada PR](#13-nivel-2-fuzzing-breve-en-cada-pr)
14. [Nivel 3: fuzzing continuo programado](#14-nivel-3-fuzzing-continuo-programado)
15. [GitHub Actions: configuración completa](#15-github-actions-configuración-completa)
16. [Cachear el corpus entre ejecuciones de CI](#16-cachear-el-corpus-entre-ejecuciones-de-ci)
17. [Manejar crashes en CI](#17-manejar-crashes-en-ci)
18. [OSS-Fuzz: fuzzing continuo a escala](#18-oss-fuzz-fuzzing-continuo-a-escala)
19. [OSS-Fuzz: arquitectura](#19-oss-fuzz-arquitectura)
20. [OSS-Fuzz: integrar un proyecto Rust](#20-oss-fuzz-integrar-un-proyecto-rust)
21. [OSS-Fuzz: el archivo Dockerfile](#21-oss-fuzz-el-archivo-dockerfile)
22. [OSS-Fuzz: el script build.sh](#22-oss-fuzz-el-script-buildsh)
23. [OSS-Fuzz: el archivo project.yaml](#23-oss-fuzz-el-archivo-projectyaml)
24. [OSS-Fuzz: flujo después de la integración](#24-oss-fuzz-flujo-después-de-la-integración)
25. [ClusterFuzzLite: OSS-Fuzz para repositorios privados](#25-clusterfuzzlite-oss-fuzz-para-repositorios-privados)
26. [Gestión del corpus a largo plazo](#26-gestión-del-corpus-a-largo-plazo)
27. [Gestión de crashes a largo plazo](#27-gestión-de-crashes-a-largo-plazo)
28. [Fuzzing y el ciclo de vida del bug](#28-fuzzing-y-el-ciclo-de-vida-del-bug)
29. [Fuzzing y code review](#29-fuzzing-y-code-review)
30. [Fuzzing y releases](#30-fuzzing-y-releases)
31. [Cuándo agregar nuevos targets](#31-cuándo-agregar-nuevos-targets)
32. [Métricas y dashboards](#32-métricas-y-dashboards)
33. [Comparación de estrategias de CI: C vs Rust vs Go](#33-comparación-de-estrategias-de-ci-c-vs-rust-vs-go)
34. [Errores comunes](#34-errores-comunes)
35. [Programa de práctica: integración completa](#35-programa-de-práctica-integración-completa)
36. [Ejercicios](#36-ejercicios)

---

## 1. Fuzzing no es un evento, es un proceso

En los tópicos anteriores (T01-T03) aprendimos la mecánica del fuzzing en Rust: cargo-fuzz, Arbitrary, sanitizers. Pero ejecutar el fuzzer una vez y olvidarse no es suficiente.

```
┌──────────────────────────────────────────────────────────────────┐
│              Fuzzing como evento vs como proceso                  │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Fuzzing como EVENTO (malo):                                     │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │ "Fuzzee 30 minutos, no encontré nada, listo"              │  │
│  │                                                            │  │
│  │ Problemas:                                                 │  │
│  │ - El código cambia → bugs nuevos aparecen                  │  │
│  │ - El corpus no se preserva → trabajo perdido               │  │
│  │ - Los crashes no se convierten en tests → regresan          │  │
│  │ - No hay forma de saber si el fuzzing fue suficiente        │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                  │
│  Fuzzing como PROCESO (bueno):                                   │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │ 1. Fuzzear como parte del desarrollo                       │  │
│  │ 2. Preservar el corpus (git o CI cache)                    │  │
│  │ 3. Convertir crashes en regression tests                   │  │
│  │ 4. Ejecutar regression tests en cada PR                    │  │
│  │ 5. Fuzzing continuo (CI o OSS-Fuzz)                        │  │
│  │ 6. Agregar targets cuando el código crece                  │  │
│  │ 7. Medir cobertura periódicamente                          │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

### El objetivo final

```
Commit → CI compila → Tests pasan → Fuzzing regression pasa
  → Fuzzing breve (5 min) sin crashes
  → Merge

Nightly/semanal → Fuzzing largo (horas) → Si crash:
  → Minimizar → Crear issue → Fix → Regression test → Merge
```

---

## 2. Cuánto tiempo fuzzear: la pregunta fundamental

"¿Cuánto tiempo debo dejar corriendo el fuzzer?" es la pregunta más frecuente. No hay una respuesta universal, pero hay principios claros.

### La respuesta corta

```
Primera vez con un target nuevo: 1-4 horas mínimo
Después de cambios significativos: 30-60 minutos
CI por PR:                        1-5 minutos
CI continuo (nightly):            2-8 horas
OSS-Fuzz:                         24/7 (continuo)
```

### La respuesta larga

El tiempo necesario depende de:

| Factor | Menos tiempo | Más tiempo |
|---|---|---|
| Complejidad del parser | Simple (CSV) | Complejo (HTML, PDF) |
| Tamaño del input | < 100 bytes | > 10 KB |
| Velocidad del fuzzer | > 5000 exec/s | < 500 exec/s |
| Calidad del corpus | Semillas diversas + diccionario | Sin semillas |
| Profundidad del código | Pocas ramas | Muchas ramas anidadas |
| Sanitizers | Sin sanitizer (rápido) | Con ASan+MSan (lento) |
| Número de targets | 1 target | 10+ targets |
| Historial de fuzzing | Corpus grande acumulado | Primera vez |

---

## 3. Rendimientos decrecientes: la curva de cobertura

El fuzzing tiene rendimientos fuertemente decrecientes: la mayoría de la cobertura se alcanza rápidamente, y cada hora adicional aporta menos.

```
Cobertura
(edges)
    │
300 ├──────────────────────────────────────────── máximo teórico
    │
280 ├─────────────────────────────────────╱────── 
    │                                  ╱
260 ├────────────────────────────────╱──
    │                             ╱
240 ├──────────────────────────╱─
    │                       ╱
200 ├────────────────────╱
    │                 ╱
150 ├─────────────╱
    │          ╱
100 ├───────╱
    │    ╱
 50 ├─╱
    │╱
  0 ├────┬────┬────┬────┬────┬────┬────┬────┬──→ tiempo
    0   10m  30m   1h   2h   4h   8h  16h  24h

    ├──────────────┤ Fase 1: progreso rápido (80% cobertura)
                   ├──────────────┤ Fase 2: progreso lento (15%)
                                  ├──────────────┤ Fase 3: meseta (5%)
```

### Las tres fases del fuzzing

```
Fase 1 (0 - 30 minutos): Exploración rápida
  - El fuzzer descubre la mayoría de los caminos "fáciles"
  - Cobertura crece rápidamente
  - La mayoría de panics obvios se encuentran aquí
  - Progreso: muchos "NEW" en la salida

Fase 2 (30 min - 4 horas): Exploración profunda
  - El fuzzer empieza a necesitar mutaciones más creativas
  - Cobertura crece lentamente
  - Bugs más sutiles se encuentran aquí
  - Progreso: "NEW" esporádicos, muchos "pulse"

Fase 3 (4+ horas): Meseta
  - Cobertura estancada o crece muy lentamente
  - Solo bugs muy profundos se encuentran aquí
  - Rendimiento por hora de fuzzing: muy bajo
  - Vale la pena solo para código de alta criticidad
```

### Implicación práctica

```
Para la mayoría de proyectos:
  30 minutos → encuentra el 80% de los bugs encontrables
  4 horas    → encuentra el 95%
  24 horas   → encuentra el 99%

Para código crítico de seguridad (parsers de red, crypto):
  Las últimas horas IMPORTAN porque un solo bug = RCE

Para lógica de negocio:
  30-60 minutos suelen ser suficientes
```

---

## 4. Métricas para decidir cuándo parar

### Métrica 1: cobertura estancada

Si la cobertura (campo `cov:` en la salida) no aumenta en los últimos 30 minutos, probablemente has alcanzado la meseta.

```bash
# Ejecutar con print_final_stats para ver estadísticas
cargo fuzz run fuzz_parse -- -max_total_time=3600 -print_final_stats=1

# Al final imprime:
# stat::number_of_executed_units: 2500000
# stat::average_exec_per_sec:     4166
# stat::new_units_added:          350
# stat::slowest_unit_time_sec:    0.002
# stat::peak_rss_mb:              45
```

### Métrica 2: ratio de inputs nuevos

```
Calculé cuántos "NEW" por minuto aparecen:

Minuto 1-5:   50 NEW/min  → Progreso excelente, seguir
Minuto 5-30:  10 NEW/min  → Progreso bueno, seguir
Minuto 30-60:  2 NEW/min  → Progreso lento, considerar parar
Minuto 60+:    0 NEW/min  → Meseta, parar o cambiar estrategia
```

### Métrica 3: exec/s sostenido

Si exec/s cae significativamente durante la sesión, puede indicar:
- Memory leak (LSan lo detectaría)
- Corpus creciendo demasiado (hacer merge)
- Inputs causando OOM parcial

### Cuándo cambiar estrategia en vez de parar

```
Si la cobertura se estancó, antes de parar:

1. ¿Tienes diccionario? → Agregar tokens relevantes
2. ¿Tienes semillas buenas? → Agregar inputs de tests existentes
3. ¿El max_len es apropiado? → Aumentar si el formato lo requiere
4. ¿Usas value_profile? → Activar: -use_value_profile=1
5. ¿Hay ramas inalcanzables? → Verificar con cargo fuzz coverage
6. ¿Necesitas otro harness? → Más targets para más profundidad
```

---

## 5. Cuánto tiempo por escenario

### Tabla de referencia

| Escenario | Duración | Justificación |
|---|---|---|
| Verificación rápida | 1-5 min | "¿Compila y no crashea inmediatamente?" |
| Desarrollo diario | 10-30 min | Encontrar panics obvios en código nuevo |
| Antes de merge/PR | 30-60 min | Verificar que el cambio no introduce crashes |
| Release semanal | 2-8 horas | Fuzzing profundo con múltiples sanitizers |
| Código de seguridad | 8-24 horas | Parsers de red, crypto, datos no confiables |
| CI por PR (regression) | < 1 min | `-runs=0` solo verifica corpus existente |
| CI por PR (fuzzing) | 2-5 min | Fuzzing breve para cobertura incremental |
| CI nightly | 1-4 horas | Fuzzing sostenido, varios targets |
| OSS-Fuzz | 24/7 | Fuzzing continuo con infraestructura de Google |

### Para proyectos pequeños (1-5 archivos)

```bash
# Sesión típica: 15 minutos por target
for target in $(cargo fuzz list); do
    cargo fuzz run "$target" -- -max_total_time=900
done
```

### Para proyectos medianos (5-20 archivos)

```bash
# Sesión típica: 30 min ASan + 30 min sin sanitizer por target
for target in $(cargo fuzz list); do
    cargo fuzz run "$target" -- -max_total_time=1800
    cargo fuzz run "$target" --sanitizer=none -- -max_total_time=1800
done
```

### Para proyectos grandes o críticos

```bash
# Sesión nocturna: 2 horas por target, múltiples sanitizers
# (ejecutar como tarea programada o CI nightly)
```

---

## 6. Fuzzing local: flujo del desarrollador

### El flujo recomendado

```
┌─────────────────────────────────────────────────────────────┐
│                 Flujo de fuzzing local                        │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. ESCRIBIR CÓDIGO                                         │
│     pub fn parse_message(data: &[u8]) -> Result<Msg, Err>   │
│                                                             │
│  2. ESCRIBIR TESTS UNITARIOS                                │
│     #[test]                                                 │
│     fn test_parse_basic() { ... }                           │
│                                                             │
│  3. ESCRIBIR HARNESS (si no existe)                         │
│     cargo fuzz add fuzz_parse_message                       │
│     fuzz_target!(|data: &[u8]| { ... });                    │
│                                                             │
│  4. FUZZEAR LOCALMENTE (15-30 min)                          │
│     cargo fuzz run fuzz_parse_message                       │
│       -- -max_total_time=900                                │
│                                                             │
│  5. SI CRASH:                                               │
│     a. Minimizar:                                           │
│        cargo fuzz tmin fuzz_parse_message CRASH              │
│     b. Analizar:                                            │
│        cargo fuzz fmt fuzz_parse_message MINIMIZED           │
│     c. Corregir el bug en src/                              │
│     d. Crear regression test (test unitario + corpus)       │
│     e. Verificar: cargo fuzz run ... MINIMIZED              │
│     f. Volver a paso 4 (fuzzear de nuevo)                   │
│                                                             │
│  6. SI NO CRASH:                                            │
│     a. Revisar cobertura: cargo fuzz coverage               │
│     b. Si cobertura < 80%: mejorar semillas/diccionario     │
│     c. Si cobertura > 80%: commit del corpus                │
│                                                             │
│  7. COMMIT                                                  │
│     git add src/ tests/ fuzz/                               │
│     git commit -m "Add message parser with fuzz targets"    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 7. Regression tests desde crashes

### El concepto

Un crash encontrado por fuzzing debe convertirse en un **regression test** para que el bug nunca vuelva a aparecer. Hay dos formas complementarias:

```
Forma 1: Test unitario en Rust
  - Reproduce el crash con assert
  - Se ejecuta con cargo test
  - Corre en stable (no necesita nightly)
  - Documenta el bug con contexto humano

Forma 2: Archivo en el regression corpus
  - El archivo crash minimizado se guarda en un directorio
  - Se ejecuta con cargo fuzz run -- -runs=0
  - Necesita nightly
  - Verifica que el harness completo no crashea
```

### Por qué ambos son necesarios

```
Test unitario:
  ✓ Ejecutable en stable
  ✓ Se integra con cargo test normal
  ✓ Documenta el bug con comentarios
  ✗ No verifica sanitizers
  ✗ Puede no cubrir exactamente el mismo code path que el fuzzer

Regression corpus:
  ✓ Verifica exactamente el mismo code path
  ✓ Incluye verificación de sanitizers
  ✓ Se mantiene automáticamente actualizado
  ✗ Necesita nightly
  ✗ No es autodocumentado (es un archivo binario)
```

---

## 8. Convertir crash a test unitario: paso a paso

### Paso 1: minimizar el crash

```bash
cargo fuzz tmin fuzz_parse fuzz/artifacts/fuzz_parse/crash-abc123
# → fuzz/artifacts/fuzz_parse/minimized-from-abc123
```

### Paso 2: entender el input

```bash
# Ver como bytes
xxd fuzz/artifacts/fuzz_parse/minimized-from-abc123

# Ver como tipo Arbitrary (si usas Arbitrary)
cargo fuzz fmt fuzz_parse fuzz/artifacts/fuzz_parse/minimized-from-abc123
```

### Paso 3: reproducir el crash

```bash
# Verificar que sigue crasheando
RUST_BACKTRACE=1 cargo fuzz run fuzz_parse \
    fuzz/artifacts/fuzz_parse/minimized-from-abc123
```

### Paso 4: escribir el test unitario

```rust
// tests/regression.rs

#[test]
fn regression_crash_abc123_empty_key_panic() {
    // Bug encontrado por fuzzing 2026-04-05
    // Causa: parse_object paniquea con unwrap() cuando una key
    // JSON está vacía y es seguida por un caracter no válido.
    // Input minimizado: {"":X}
    // Fix: reemplazar unwrap() por match en parse_value()
    let input = b"{\"\":X}";
    let result = my_lib::parse(input);
    // Debe retornar Err, no paniquear
    assert!(result.is_err());
}

#[test]
fn regression_crash_def456_deep_nesting() {
    // Bug encontrado por fuzzing 2026-04-05
    // Causa: recursión sin límite de profundidad causa stack overflow
    // Input minimizado: 100 niveles de anidación
    // Fix: agregar MAX_DEPTH = 64 en parse_value()
    let input = "[".repeat(100) + &"]".repeat(100);
    let result = my_lib::parse(input.as_bytes());
    assert!(result.is_err());
}
```

### Paso 5: verificar que el fix corrige el crash

```bash
# El test debe pasar
cargo test regression_crash_abc123

# El harness no debe crashear con el input
cargo fuzz run fuzz_parse fuzz/artifacts/fuzz_parse/minimized-from-abc123
# → No crash, sale limpiamente
```

### Convenciones para tests de regression

```rust
// Convención de nombres:
#[test]
fn regression_HASH_DESCRIPCION() { ... }
//          ^^^^  ^^^^^^^^^^^^
//          |     qué causaba
//          primeros chars del hash del crash

// Convención de documentación:
// Línea 1: Bug encontrado por fuzzing FECHA
// Línea 2: Causa: descripción concisa
// Línea 3: Input minimizado: representación legible
// Línea 4: Fix: qué se cambió
```

---

## 9. Regression tests con cargo fuzz run -runs=0

### Qué hace -runs=0

`-runs=0` le dice a libFuzzer: "ejecuta cada input del corpus exactamente una vez, sin mutar ni generar inputs nuevos". Es equivalente a un test suite que ejecuta cada input del corpus como un test case.

```bash
cargo fuzz run fuzz_parse -- -runs=0
```

### Salida

```
INFO: Running with entropic power schedule (0xFF, 100).
INFO: Seed: 1234567890
INFO: Loaded 1 modules (1234 inline 8-bit counters)
INFO:      150 files found in fuzz/corpus/fuzz_parse
INFO: seed corpus: files: 150 min: 1b max: 1024b total: 15360b
#150    INITED cov: 280 ft: 420 corp: 150/15360b
INFO: Done 150 runs in 0 second(s)
```

Si algún input del corpus causa crash, el fuzzer lo reporta como crash normal. Si todos pasan, sale con código 0.

### Usar -runs=0 en CI

```bash
# En CI: verificar que el corpus no causa crashes
cargo fuzz run fuzz_parse -- -runs=0

# Exit code:
# 0 = todos los inputs pasaron
# 1 = algún input causó crash (el CI falla)
```

### Ventaja sobre tests unitarios

```
Tests unitarios de regression: verifican inputs específicos
-runs=0:                       verifica TODOS los inputs del corpus

El corpus puede tener 500+ inputs, cada uno representando
un camino de código diferente que el fuzzer descubrió.
-runs=0 los ejecuta todos en < 1 segundo.
```

---

## 10. Directorio de regression corpus

### Estructura recomendada

```
fuzz/
├── corpus/           ← Corpus vivo (crece con el tiempo)
│   └── fuzz_parse/
│       ├── ...       ← Cientos/miles de inputs
│       └── ...
│
├── regression/       ← Inputs de regression (crashes minimizados)
│   └── fuzz_parse/
│       ├── crash_empty_key           ← Crash minimizado #1
│       ├── crash_deep_nesting        ← Crash minimizado #2
│       ├── crash_unicode_boundary    ← Crash minimizado #3
│       └── crash_overflow_value      ← Crash minimizado #4
│
└── artifacts/        ← Crashes sin procesar (no commitear)
    └── fuzz_parse/
        ├── crash-abc123...
        └── timeout-def456...
```

### Ejecutar regression tests

```bash
# Solo regression corpus (rápido, para CI):
cargo fuzz run fuzz_parse fuzz/regression/fuzz_parse -- -runs=0

# Corpus completo + regression (más lento):
cargo fuzz run fuzz_parse fuzz/corpus/fuzz_parse fuzz/regression/fuzz_parse -- -runs=0
```

### Qué commitear a git

```
✓ Commitear:
  fuzz/fuzz_targets/           ← harnesses (código)
  fuzz/Cargo.toml              ← configuración
  fuzz/regression/             ← crashes minimizados (pequeños)
  fuzz/dicts/                  ← diccionarios
  fuzz/corpus/fuzz_parse/seed_*  ← semillas manuales

? Depende:
  fuzz/corpus/                 ← corpus descubierto
  Ventaja: preservar cobertura acumulada
  Desventaja: puede ser grande (MB), archivos binarios

✗ No commitear:
  fuzz/target/                 ← binarios compilados
  fuzz/artifacts/              ← crashes sin procesar
```

### .gitignore recomendado

```
# fuzz/.gitignore
target/
artifacts/

# Si decides NO commitear el corpus automático:
# corpus/*/[0-9a-f]*
# (mantener solo los seed_* manuales)
```

---

## 11. CI fuzzing: por qué y cómo

### Los 3 niveles de CI fuzzing

```
Nivel 1: Regression (mínimo recomendado)
  ┌─────────────────────────────────────┐
  │ En cada PR:                         │
  │ cargo fuzz run target -- -runs=0    │
  │                                     │
  │ Costo: < 1 minuto                   │
  │ Beneficio: ningún crash conocido    │
  │            reaparece                │
  └─────────────────────────────────────┘

Nivel 2: Fuzzing breve (recomendado)
  ┌─────────────────────────────────────┐
  │ En cada PR:                         │
  │ cargo fuzz run target               │
  │   -- -max_total_time=120            │
  │                                     │
  │ Costo: 2-5 minutos                  │
  │ Beneficio: encuentra crashes en     │
  │            código nuevo del PR      │
  └─────────────────────────────────────┘

Nivel 3: Fuzzing continuo (ideal)
  ┌─────────────────────────────────────┐
  │ Nightly/semanal:                    │
  │ cargo fuzz run target               │
  │   -- -max_total_time=7200           │
  │                                     │
  │ Costo: horas de CPU                 │
  │ Beneficio: fuzzing profundo,        │
  │            corpus creciente,        │
  │            bugs sutiles             │
  └─────────────────────────────────────┘
```

---

## 12. Nivel 1: regression tests en CI

### GitHub Actions minimal

```yaml
# .github/workflows/fuzz-regression.yml
name: Fuzz Regression

on:
  pull_request:
  push:
    branches: [main]

jobs:
  fuzz-regression:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install Rust nightly
        uses: dtolnay/rust-toolchain@nightly

      - name: Install cargo-fuzz
        run: cargo install cargo-fuzz

      - name: Run regression tests
        run: |
          for target in $(cargo fuzz list); do
            echo "=== Regression: $target ==="
            cargo fuzz run "$target" -- -runs=0
          done
```

### Características

- **Duración**: < 1 minuto
- **Requisito**: corpus o regression directory commiteado en git
- **Falla si**: algún input del corpus causa crash
- **No falla si**: simplemente no hay corpus (nada que probar)

---

## 13. Nivel 2: fuzzing breve en cada PR

### GitHub Actions con fuzzing

```yaml
# .github/workflows/fuzz-pr.yml
name: Fuzz PR

on:
  pull_request:

jobs:
  fuzz:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        target: [fuzz_parse, fuzz_roundtrip, fuzz_validate]
    steps:
      - uses: actions/checkout@v4

      - name: Install Rust nightly
        uses: dtolnay/rust-toolchain@nightly

      - name: Install cargo-fuzz
        run: cargo install cargo-fuzz

      # Restaurar corpus cacheado
      - name: Restore corpus cache
        uses: actions/cache/restore@v4
        with:
          path: fuzz/corpus/${{ matrix.target }}
          key: fuzz-corpus-${{ matrix.target }}-${{ github.base_ref }}

      # Fuzzing breve: 2 minutos por target
      - name: Fuzz ${{ matrix.target }}
        run: |
          cargo fuzz run ${{ matrix.target }} -- \
            -max_total_time=120 \
            -print_final_stats=1

      # Guardar corpus actualizado
      - name: Save corpus cache
        uses: actions/cache/save@v4
        if: always()
        with:
          path: fuzz/corpus/${{ matrix.target }}
          key: fuzz-corpus-${{ matrix.target }}-${{ github.sha }}

      # Si hay crashes, subir como artifacts
      - name: Upload crash artifacts
        if: failure()
        uses: actions/upload-artifact@v4
        with:
          name: fuzz-crashes-${{ matrix.target }}
          path: fuzz/artifacts/${{ matrix.target }}/
```

### Características

- **Duración**: 2-5 minutos por target (paralelo con matrix)
- **Corpus**: cacheado entre PRs para acumular cobertura
- **Crashes**: subidos como artifacts del job para inspección
- **Falla el PR si**: se encuentra un crash

---

## 14. Nivel 3: fuzzing continuo programado

### GitHub Actions nightly

```yaml
# .github/workflows/fuzz-nightly.yml
name: Nightly Fuzzing

on:
  schedule:
    - cron: '0 2 * * *'  # 2 AM UTC cada día
  workflow_dispatch:       # También manual

jobs:
  fuzz:
    runs-on: ubuntu-latest
    strategy:
      fail-fast: false
      matrix:
        target: [fuzz_parse, fuzz_roundtrip, fuzz_validate]
        sanitizer: [address, none]
    steps:
      - uses: actions/checkout@v4

      - name: Install Rust nightly
        uses: dtolnay/rust-toolchain@nightly

      - name: Install cargo-fuzz
        run: cargo install cargo-fuzz

      - name: Restore corpus
        uses: actions/cache/restore@v4
        with:
          path: fuzz/corpus/${{ matrix.target }}
          key: fuzz-corpus-${{ matrix.target }}-nightly

      # Fuzzing largo: 1 hora por target+sanitizer
      - name: Fuzz ${{ matrix.target }} (${{ matrix.sanitizer }})
        run: |
          cargo fuzz run ${{ matrix.target }} \
            --sanitizer=${{ matrix.sanitizer }} \
            -- -max_total_time=3600 \
            -print_final_stats=1

      # Minimizar corpus después de fuzzing largo
      - name: Minimize corpus
        if: success()
        run: cargo fuzz cmin ${{ matrix.target }}

      - name: Save corpus
        uses: actions/cache/save@v4
        if: always()
        with:
          path: fuzz/corpus/${{ matrix.target }}
          key: fuzz-corpus-${{ matrix.target }}-nightly-${{ github.sha }}

      - name: Upload crashes
        if: failure()
        uses: actions/upload-artifact@v4
        with:
          name: crashes-${{ matrix.target }}-${{ matrix.sanitizer }}
          path: fuzz/artifacts/${{ matrix.target }}/

      # Crear issue automático si hay crashes
      - name: Create issue on crash
        if: failure()
        uses: actions/github-script@v7
        with:
          script: |
            const fs = require('fs');
            const dir = `fuzz/artifacts/${{ matrix.target }}`;
            let crashes = [];
            try {
              crashes = fs.readdirSync(dir).filter(f => f.startsWith('crash-'));
            } catch (e) {}

            if (crashes.length > 0) {
              await github.rest.issues.create({
                owner: context.repo.owner,
                repo: context.repo.repo,
                title: `Fuzz crash: ${{ matrix.target }} (${{ matrix.sanitizer }})`,
                body: `Nightly fuzzing found ${crashes.length} crash(es).\n\n` +
                      `Target: \`${{ matrix.target }}\`\n` +
                      `Sanitizer: \`${{ matrix.sanitizer }}\`\n` +
                      `Artifacts: check the workflow run artifacts.\n\n` +
                      `Run ID: ${context.runId}`,
                labels: ['bug', 'fuzzing']
              });
            }
```

### Características

- **Duración**: 1 hora × N targets × M sanitizers
- **Frecuencia**: cada noche (o semanal)
- **Corpus**: cacheado y minimizado después de cada sesión
- **Crashes**: suben como artifacts + crean issue automático
- **fail-fast: false**: cada target se fuzzea independientemente

---

## 15. GitHub Actions: configuración completa

### Workflow completo con los 3 niveles

```yaml
# .github/workflows/fuzz.yml
name: Fuzzing

on:
  pull_request:
    paths:
      - 'src/**'
      - 'fuzz/**'
      - 'Cargo.toml'
  push:
    branches: [main]
  schedule:
    - cron: '0 2 * * 1'  # Lunes a las 2 AM

env:
  CARGO_TERM_COLOR: always

jobs:
  # Nivel 1: siempre (PR + push + schedule)
  regression:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: dtolnay/rust-toolchain@nightly
      - run: cargo install cargo-fuzz
      - name: Regression tests
        run: |
          for target in $(cargo fuzz list); do
            echo "::group::Regression: $target"
            # Verificar regression corpus
            if [ -d "fuzz/regression/$target" ]; then
              cargo fuzz run "$target" fuzz/regression/"$target" -- -runs=0
            fi
            # Verificar corpus regular
            if [ -d "fuzz/corpus/$target" ]; then
              cargo fuzz run "$target" -- -runs=0
            fi
            echo "::endgroup::"
          done

  # Nivel 2: solo PRs
  fuzz-pr:
    if: github.event_name == 'pull_request'
    needs: regression
    runs-on: ubuntu-latest
    strategy:
      matrix:
        target: [fuzz_parse, fuzz_roundtrip]
    steps:
      - uses: actions/checkout@v4
      - uses: dtolnay/rust-toolchain@nightly
      - run: cargo install cargo-fuzz

      - uses: actions/cache/restore@v4
        with:
          path: fuzz/corpus/${{ matrix.target }}
          key: corpus-${{ matrix.target }}
          restore-keys: corpus-${{ matrix.target }}

      - name: Fuzz (2 min)
        run: cargo fuzz run ${{ matrix.target }} -- -max_total_time=120

      - uses: actions/cache/save@v4
        if: always()
        with:
          path: fuzz/corpus/${{ matrix.target }}
          key: corpus-${{ matrix.target }}-${{ github.sha }}

      - uses: actions/upload-artifact@v4
        if: failure()
        with:
          name: crashes-${{ matrix.target }}
          path: fuzz/artifacts/${{ matrix.target }}/

  # Nivel 3: solo schedule
  fuzz-nightly:
    if: github.event_name == 'schedule'
    needs: regression
    runs-on: ubuntu-latest
    strategy:
      fail-fast: false
      matrix:
        target: [fuzz_parse, fuzz_roundtrip]
    steps:
      - uses: actions/checkout@v4
      - uses: dtolnay/rust-toolchain@nightly
      - run: cargo install cargo-fuzz

      - uses: actions/cache/restore@v4
        with:
          path: fuzz/corpus/${{ matrix.target }}
          key: corpus-${{ matrix.target }}
          restore-keys: corpus-${{ matrix.target }}

      - name: Fuzz (1 hour)
        run: cargo fuzz run ${{ matrix.target }} -- -max_total_time=3600

      - name: Minimize corpus
        if: success()
        run: cargo fuzz cmin ${{ matrix.target }}

      - uses: actions/cache/save@v4
        if: always()
        with:
          path: fuzz/corpus/${{ matrix.target }}
          key: corpus-${{ matrix.target }}-nightly-${{ github.sha }}

      - uses: actions/upload-artifact@v4
        if: failure()
        with:
          name: crashes-nightly-${{ matrix.target }}
          path: fuzz/artifacts/${{ matrix.target }}/
```

---

## 16. Cachear el corpus entre ejecuciones de CI

### Por qué cachear el corpus

El corpus es la "memoria" del fuzzer. Sin cache:

```
PR #1: 0 inputs → fuzzea 2 min → descubre 200 inputs → descarta
PR #2: 0 inputs → fuzzea 2 min → descubre 200 inputs → descarta
PR #3: 0 inputs → fuzzea 2 min → descubre 200 inputs → descarta
→ Cada PR empieza de cero, pierde todo el progreso

Con cache:
PR #1: 0 inputs → fuzzea 2 min → descubre 200 inputs → GUARDA
PR #2: 200 inputs → fuzzea 2 min → descubre 50 más → GUARDA
PR #3: 250 inputs → fuzzea 2 min → descubre 30 más → GUARDA
→ El corpus crece con cada PR, cobertura acumulativa
```

### Estrategia de cache keys

```yaml
# Restaurar: buscar la key más reciente
- uses: actions/cache/restore@v4
  with:
    path: fuzz/corpus/${{ matrix.target }}
    key: corpus-${{ matrix.target }}-${{ github.sha }}
    restore-keys: |
      corpus-${{ matrix.target }}-
      corpus-

# Guardar: key única por commit
- uses: actions/cache/save@v4
  with:
    path: fuzz/corpus/${{ matrix.target }}
    key: corpus-${{ matrix.target }}-${{ github.sha }}
```

### Límite de tamaño

GitHub Actions cache tiene límite de 10 GB por repositorio. Los corpus pueden crecer rápido:

```bash
# Minimizar corpus antes de cachear (reduce 50-90%):
cargo fuzz cmin fuzz_parse
```

### Alternativa: corpus como git submodule o artifact

```bash
# Opción 1: corpus en una rama separada de git
git checkout --orphan corpus
git rm -rf .
cp -r fuzz/corpus/ .
git add .
git commit -m "Update corpus"
git push origin corpus

# En CI:
git clone --branch corpus --depth 1 ... fuzz/corpus

# Opción 2: corpus como release artifact
# Subir como release artifact mensualmente
```

---

## 17. Manejar crashes en CI

### Flujo cuando CI encuentra un crash

```
CI encuentra crash
       │
       ▼
┌──────────────────┐
│ Job falla        │
│ Artifacts subidos│
└──────┬───────────┘
       │
       ▼
Desarrollador recibe notificación
       │
       ├── Descargar artifacts
       │   gh run download <RUN_ID> --name crashes-fuzz_parse
       │
       ├── Reproducir localmente
       │   cargo fuzz run fuzz_parse fuzz/artifacts/fuzz_parse/crash-XXX
       │
       ├── Minimizar
       │   cargo fuzz tmin fuzz_parse crash-XXX
       │
       ├── Analizar y corregir
       │   RUST_BACKTRACE=1 cargo fuzz run fuzz_parse minimized-XXX
       │   → Editar src/...
       │
       ├── Crear regression test
       │   cp minimized-XXX fuzz/regression/fuzz_parse/
       │   → Escribir test unitario
       │
       └── Commit + PR
           git add fuzz/regression/ tests/regression.rs src/
           git commit -m "Fix crash in parser found by fuzzing"
```

### Script para procesar crashes de CI

```bash
#!/bin/bash
# process_ci_crashes.sh — descargar y procesar crashes de CI

RUN_ID=$1
TARGET=$2

if [ -z "$RUN_ID" ] || [ -z "$TARGET" ]; then
    echo "Usage: $0 <run_id> <target>"
    exit 1
fi

# Descargar artifacts
echo "=== Downloading crashes ==="
gh run download "$RUN_ID" --name "crashes-$TARGET" -D /tmp/ci_crashes

# Procesar cada crash
for crash in /tmp/ci_crashes/crash-*; do
    echo "=== Processing: $(basename $crash) ==="

    # Reproducir
    echo "--- Reproducing ---"
    cargo fuzz run "$TARGET" "$crash" 2>&1 | head -20

    # Minimizar
    echo "--- Minimizing ---"
    cargo fuzz tmin "$TARGET" "$crash"

    minimized=$(ls -t fuzz/artifacts/"$TARGET"/minimized-* | head -1)
    if [ -n "$minimized" ]; then
        echo "--- Minimized to $(wc -c < "$minimized") bytes ---"
        xxd "$minimized"

        # Copiar a regression
        mkdir -p fuzz/regression/"$TARGET"
        cp "$minimized" fuzz/regression/"$TARGET"/
        echo "--- Saved to regression corpus ---"
    fi
done

echo "=== Done ==="
echo "Remember to:"
echo "  1. Analyze each crash"
echo "  2. Fix the bug in src/"
echo "  3. Write a regression test in tests/"
echo "  4. Commit fuzz/regression/ and tests/"
```

---

## 18. OSS-Fuzz: fuzzing continuo a escala

### Qué es OSS-Fuzz

OSS-Fuzz es un servicio gratuito de Google que proporciona **fuzzing continuo** para proyectos open-source. Ejecuta tus fuzz targets 24/7 en la infraestructura de Google, con múltiples sanitizers, motores de fuzzing y máquinas.

```
┌──────────────────────────────────────────────────────────────┐
│                         OSS-Fuzz                              │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Qué proporciona:                                            │
│  ✓ Infraestructura de fuzzing 24/7 (miles de cores)          │
│  ✓ Múltiples sanitizers (ASan, MSan, UBSan)                  │
│  ✓ Múltiples motores (libFuzzer, AFL++, Honggfuzz)           │
│  ✓ Detección automática de crashes                           │
│  ✓ Minimización automática                                   │
│  ✓ Deduplicación de crashes                                  │
│  ✓ Notificación automática a mantenedores                    │
│  ✓ Dashboard web (oss-fuzz.com)                              │
│  ✓ Verificación automática de fixes                          │
│  ✓ Disclosure timeline (90 días)                             │
│  ✓ Corpus almacenado indefinidamente                         │
│                                                              │
│  Qué requiere:                                               │
│  - Proyecto open-source con usuarios significativos          │
│  - Al menos 1 fuzz target                                    │
│  - Dockerfile + build.sh                                     │
│  - Pull request a google/oss-fuzz                            │
│                                                              │
│  Proyectos Rust en OSS-Fuzz:                                 │
│  - serde, regex, image, ring, rustls, hyper, ...             │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 19. OSS-Fuzz: arquitectura

```
┌─────────────────────────────────────────────────────────────────┐
│                    Flujo de OSS-Fuzz                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Tu repositorio                        google/oss-fuzz           │
│  ┌──────────────┐                     ┌──────────────────┐      │
│  │ src/         │                     │ projects/        │      │
│  │ fuzz/        │                     │   my_project/    │      │
│  │   fuzz_      │                     │     Dockerfile   │      │
│  │   targets/   │                     │     build.sh     │      │
│  │              │                     │     project.yaml │      │
│  └──────┬───────┘                     └────────┬─────────┘      │
│         │                                      │                │
│         └──────────────┬───────────────────────┘                │
│                        │                                        │
│              ┌─────────▼──────────┐                             │
│              │  ClusterFuzz       │                              │
│              │  (infraestructura) │                              │
│              │                    │                              │
│              │  1. git clone repo │                              │
│              │  2. docker build   │                              │
│              │  3. ./build.sh     │                              │
│              │  4. Ejecutar fuzz  │                              │
│              │     targets 24/7   │                              │
│              │  5. Si crash:      │                              │
│              │     - Minimizar    │                              │
│              │     - Deduplicar   │                              │
│              │     - Notificar    │                              │
│              │  6. Verificar fix  │                              │
│              └────────────────────┘                              │
│                                                                 │
│  Dashboard: https://oss-fuzz.com (privado para mantenedores)    │
│  Bugs: Monorail (bugs.chromium.org/p/oss-fuzz)                  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 20. OSS-Fuzz: integrar un proyecto Rust

### Requisitos previos

1. Tu proyecto es open-source (GitHub, GitLab, etc.)
2. Tu proyecto tiene al menos un fuzz target con cargo-fuzz
3. Tu proyecto tiene usuarios significativos (no es un proyecto personal sin uso)

### Pasos para integrar

```bash
# 1. Fork de google/oss-fuzz
gh repo fork google/oss-fuzz

# 2. Crear directorio de tu proyecto
cd oss-fuzz
mkdir projects/my_rust_project

# 3. Crear 3 archivos:
#    - Dockerfile
#    - build.sh
#    - project.yaml

# 4. Probar localmente
python infra/helper.py build_image my_rust_project
python infra/helper.py build_fuzzers my_rust_project
python infra/helper.py run_fuzzer my_rust_project fuzz_parse

# 5. Crear PR a google/oss-fuzz
git add projects/my_rust_project/
git commit -m "Add my_rust_project"
git push origin main
# → Crear PR en GitHub
```

---

## 21. OSS-Fuzz: el archivo Dockerfile

```dockerfile
# projects/my_rust_project/Dockerfile

FROM gcr.io/oss-fuzz-base/base-builder-rust

# Clonar tu repositorio
RUN git clone --depth 1 https://github.com/user/my_rust_project.git /src/my_rust_project

WORKDIR /src/my_rust_project

# Si tienes dependencias del sistema:
# RUN apt-get update && apt-get install -y libssl-dev pkg-config

COPY build.sh $SRC/
```

### Explicación

- `base-builder-rust`: imagen base de OSS-Fuzz con Rust nightly, cargo-fuzz, y herramientas de compilación
- `git clone --depth 1`: clonar solo el último commit (rápido)
- `WORKDIR`: directorio de trabajo para build.sh
- `COPY build.sh`: copiar el script de build

---

## 22. OSS-Fuzz: el script build.sh

```bash
#!/bin/bash -eu
# projects/my_rust_project/build.sh

# OSS-Fuzz establece estas variables:
# $SRC    - directorio de código fuente (/src)
# $OUT    - directorio de output para fuzz targets
# $WORK   - directorio temporal de trabajo

cd $SRC/my_rust_project

# Compilar los fuzz targets
# cargo-fuzz compila cada target y los pone en fuzz/target/...
cargo fuzz build

# Copiar los binarios compilados a $OUT
# OSS-Fuzz espera los binarios en $OUT
for target in $(cargo fuzz list); do
    # Encontrar el binario compilado
    binary=$(find fuzz/target -name "$target" -type f -executable | head -1)
    if [ -n "$binary" ]; then
        cp "$binary" "$OUT/$target"
    fi
done

# Copiar corpus y diccionarios
for target in $(cargo fuzz list); do
    # Corpus
    if [ -d "fuzz/corpus/$target" ]; then
        zip -j "$OUT/${target}_seed_corpus.zip" fuzz/corpus/"$target"/*
    fi

    # Diccionarios
    dict_file="fuzz/dicts/${target}.dict"
    if [ -f "$dict_file" ]; then
        cp "$dict_file" "$OUT/${target}.dict"
    fi
done
```

### Convenciones de OSS-Fuzz

```
$OUT/fuzz_parse                    ← binario del fuzz target
$OUT/fuzz_parse_seed_corpus.zip    ← corpus inicial (zip)
$OUT/fuzz_parse.dict               ← diccionario
$OUT/fuzz_parse.options            ← opciones de libFuzzer (opcional)
```

### Archivo .options (opcional)

```
# $OUT/fuzz_parse.options
[libfuzzer]
max_len = 4096
timeout = 30
dict = fuzz_parse.dict
```

---

## 23. OSS-Fuzz: el archivo project.yaml

```yaml
# projects/my_rust_project/project.yaml

homepage: "https://github.com/user/my_rust_project"
language: rust
primary_contact: "maintainer@example.com"
auto_ccs:
  - "co-maintainer@example.com"
sanitizers:
  - address
  - memory
fuzzing_engines:
  - libfuzzer
main_repo: "https://github.com/user/my_rust_project.git"
vendor_ccs: []
```

### Campos

| Campo | Descripción |
|---|---|
| `homepage` | URL del proyecto |
| `language` | `rust` (determina la imagen base) |
| `primary_contact` | Email del responsable de recibir notificaciones de crashes |
| `auto_ccs` | Emails adicionales que reciben notificaciones |
| `sanitizers` | Lista de sanitizers a usar (`address`, `memory`, `undefined`) |
| `fuzzing_engines` | Motores de fuzzing (`libfuzzer`, `afl`, `honggfuzz`) |
| `main_repo` | URL del repositorio principal |

---

## 24. OSS-Fuzz: flujo después de la integración

### Ciclo diario automático

```
 6 AM: OSS-Fuzz hace git pull de tu repo
        │
        ▼
 6:05: docker build (Dockerfile)
        │
        ▼
 6:10: ./build.sh → compila fuzz targets
        │
        ▼
 6:15: Inicia fuzzing 24/7
        │
        ├── libFuzzer con ASan
        ├── libFuzzer con MSan
        ├── AFL++ con ASan
        └── Honggfuzz con ASan
        │
        ▼
 Si crash nuevo:
        │
        ├── Minimizar automáticamente
        ├── Deduplicar (¿es un crash nuevo o conocido?)
        ├── Crear bug en Monorail
        ├── Enviar email a primary_contact
        └── Empezar timer de disclosure (90 días)
        │
        ▼
 Cuando mandas fix:
        │
        ├── OSS-Fuzz verifica que el crash ya no ocurre
        ├── Marca el bug como "Verified-Fixed"
        └── Detiene el timer de disclosure
```

### Dashboard de OSS-Fuzz

Los mantenedores acceden al dashboard en `oss-fuzz.com` donde pueden:

- Ver todos los crashes encontrados
- Ver estadísticas de cobertura
- Descargar crashes minimizados
- Ver el estado de cada bug (new/verified/fixed)
- Ver el corpus acumulado

### Disclosure timeline

```
Día 0:   Crash encontrado, bug creado (privado)
Día 7:   Si no hay fix, recordatorio
Día 30:  Segundo recordatorio
Día 90:  Bug se hace PÚBLICO automáticamente
         (incluso si no hay fix)

Si el fix llega antes del día 90:
  - Bug se marca como Fixed
  - Se publica 30 días después del fix
```

---

## 25. ClusterFuzzLite: OSS-Fuzz para repositorios privados

### Qué es

ClusterFuzzLite es una versión lightweight de la infraestructura de OSS-Fuzz que puedes ejecutar en tu propio CI (GitHub Actions, GitLab CI, etc.). Es para proyectos que no califican para OSS-Fuzz (privados, corporativos, etc.).

### Diferencias con OSS-Fuzz

| Aspecto | OSS-Fuzz | ClusterFuzzLite |
|---|---|---|
| Infraestructura | Google (gratis) | Tu CI (tú pagas) |
| Requisito | Open source + usuarios | Cualquier proyecto |
| Setup | PR a google/oss-fuzz | Archivo YAML en tu repo |
| Fuzzing 24/7 | Sí (miles de cores) | No (solo cuando CI ejecuta) |
| Múltiples motores | libFuzzer, AFL++, Honggfuzz | libFuzzer |
| Deduplicación | Automática avanzada | Básica |
| Dashboard | oss-fuzz.com | Logs de CI |
| Disclosure | Automático (90 días) | Manual |

### GitHub Actions con ClusterFuzzLite

```yaml
# .github/workflows/cflite.yml
name: ClusterFuzzLite
on:
  pull_request:
    branches: [main]
  schedule:
    - cron: '0 3 * * *'

jobs:
  fuzzing:
    runs-on: ubuntu-latest
    strategy:
      fail-fast: false
      matrix:
        sanitizer: [address, undefined]
    steps:
      - uses: actions/checkout@v4

      - name: Build fuzzers
        uses: google/clusterfuzzlite/actions/build_fuzzers@v1
        with:
          language: rust
          sanitizer: ${{ matrix.sanitizer }}

      - name: Run fuzzers
        uses: google/clusterfuzzlite/actions/run_fuzzers@v1
        with:
          github-token: ${{ secrets.GITHUB_TOKEN }}
          fuzz-seconds: 300
          mode: 'code-change'  # Solo fuzzear código cambiado en el PR
          sanitizer: ${{ matrix.sanitizer }}
```

---

## 26. Gestión del corpus a largo plazo

### El corpus como activo del proyecto

```
El corpus es la "inteligencia acumulada" del fuzzer.
Cada input representa un camino de código descubierto.
Un corpus de 1000 inputs puede representar meses de fuzzing.

Perder el corpus = perder meses de trabajo.
```

### Estrategias de almacenamiento

```
┌────────────────────────────────────────────────────────────┐
│            Estrategias de almacenamiento de corpus          │
├──────────────────┬─────────────────────────────────────────┤
│ Estrategia       │ Detalles                                │
├──────────────────┼─────────────────────────────────────────┤
│ Git (repo)       │ ✓ Versionado                            │
│                  │ ✓ Accesible para todos                  │
│                  │ ✗ Archivos binarios → git se agranda     │
│                  │ Recomendado: < 10 MB de corpus          │
├──────────────────┼─────────────────────────────────────────┤
│ Git (rama sep.)  │ ✓ No contamina main                     │
│                  │ ✓ Puede ser grande                      │
│                  │ ✗ Más complejo de gestionar              │
│                  │ Recomendado: 10-100 MB                  │
├──────────────────┼─────────────────────────────────────────┤
│ CI cache         │ ✓ Transparente, automático              │
│                  │ ✓ No contamina git                      │
│                  │ ✗ Puede expirar (7-30 días)              │
│                  │ ✗ Límite de tamaño (10 GB en GH Actions)│
│                  │ Recomendado: para CI                    │
├──────────────────┼─────────────────────────────────────────┤
│ Cloud storage    │ ✓ Sin límite de tamaño                  │
│ (S3, GCS)        │ ✓ Persistente                           │
│                  │ ✗ Costo                                  │
│                  │ ✗ Setup adicional                        │
│                  │ Recomendado: proyectos grandes/críticos │
├──────────────────┼─────────────────────────────────────────┤
│ OSS-Fuzz         │ ✓ Automático, persistente, gratis       │
│                  │ ✓ Mantenido por Google                  │
│                  │ ✗ Solo para open source                  │
│                  │ Recomendado: si tu proyecto califica     │
└──────────────────┴─────────────────────────────────────────┘
```

### Mantenimiento periódico del corpus

```bash
# Cada semana/mes:

# 1. Minimizar (eliminar inputs redundantes)
cargo fuzz cmin fuzz_parse
echo "Corpus size: $(ls fuzz/corpus/fuzz_parse | wc -l) files"

# 2. Verificar cobertura
cargo fuzz coverage fuzz_parse
echo "Check coverage report"

# 3. Si la cobertura bajó después de un cambio de código:
#    Fuzzear de nuevo para rellenar los huecos
cargo fuzz run fuzz_parse -- -max_total_time=1800

# 4. Commitear el corpus minimizado
git add fuzz/corpus/fuzz_parse/
git commit -m "Update fuzzing corpus (minimized)"
```

---

## 27. Gestión de crashes a largo plazo

### Ciclo de vida de un crash

```
DESCUBIERTO (por fuzzer)
    │
    ▼
TRIAGED (analizado por desarrollador)
    │
    ├── Duplicado de crash existente → descartar
    ├── No reproducible → investigar, posiblemente descartar
    ├── En dependencia externa → reportar upstream
    └── Bug real en nuestro código → continuar
    │
    ▼
MINIMIZADO (cargo fuzz tmin)
    │
    ▼
DIAGNOSTICADO (entender la causa raíz)
    │
    ▼
CORREGIDO (fix en src/)
    │
    ▼
VERIFICADO (cargo fuzz run + crash no reproduce)
    │
    ▼
REGRESSION TEST CREADO
    │
    ├── Test unitario en tests/regression.rs
    └── Crash minimizado en fuzz/regression/
    │
    ▼
CERRADO (merge del fix + regression test)
```

### Base de datos de crashes

Para proyectos grandes, mantener un registro de crashes:

```markdown
<!-- fuzz/CRASHES.md -->
# Fuzzing Crashes Log

## Crash #001 — 2026-04-01
- **Target**: fuzz_parse
- **Sanitizer**: ASan
- **Tipo**: panic (unwrap on None)
- **Causa**: parser no maneja input vacío
- **Fix**: commit abc1234
- **Regression**: fuzz/regression/fuzz_parse/crash_001

## Crash #002 — 2026-04-03
- **Target**: fuzz_roundtrip
- **Sanitizer**: none
- **Tipo**: assertion failure
- **Causa**: encode/decode no preserva trailing whitespace
- **Fix**: commit def5678
- **Regression**: fuzz/regression/fuzz_roundtrip/crash_002
```

---

## 28. Fuzzing y el ciclo de vida del bug

### Integración con issue trackers

```
Fuzzer encuentra crash
       │
       ▼
┌──────────────────────┐
│ Crear issue          │
│                      │
│ Title: [Fuzz] Panic  │
│   en parse_value     │
│                      │
│ Labels: bug, fuzzing │
│                      │
│ Body:                │
│ - Target: fuzz_parse │
│ - Backtrace: ...     │
│ - Input minimizado   │
│ - Cómo reproducir    │
│ - Severidad estimada │
└──────────┬───────────┘
           │
           ▼
    Priorizar en sprint
           │
           ▼
    Fix + regression test
           │
           ▼
    Cerrar issue
```

### Template de issue para crashes de fuzzing

```markdown
## Crash de fuzzing: [descripción breve]

**Target**: `fuzz_parse`
**Sanitizer**: ASan
**Encontrado**: 2026-04-05

### Reproducir

```bash
cargo fuzz run fuzz_parse fuzz/artifacts/fuzz_parse/crash-abc123
```

### Backtrace

```
thread panicked at 'index out of bounds: the len is 0 but the index is 0'
   → src/parser.rs:42:15
```

### Input minimizado

```
[7 bytes]: {"":X}
```

### Análisis

La función `parse_value` llama `unwrap()` en el resultado de `peek()`
cuando el parser está al final del input.

### Severidad

Media: causa DoS en cualquier input con una key vacía seguida
de un carácter no válido.

### Fix propuesto

Reemplazar `self.peek().unwrap()` por `self.peek().ok_or(Error::UnexpectedEnd)?`
en `src/parser.rs:42`.
```

---

## 29. Fuzzing y code review

### Checklist de fuzzing para PRs

Cuando revisas un PR que modifica código que tiene fuzz targets:

```
□ ¿Los fuzz targets existentes siguen compilando?
□ ¿Los regression tests (runs=0) pasan?
□ Si el PR agrega funcionalidad nueva:
  □ ¿Se necesita un nuevo fuzz target?
  □ ¿Los targets existentes cubren el código nuevo?
□ Si el PR modifica un parser/decoder/validator:
  □ ¿Se fuzzeó localmente antes de abrir el PR?
  □ ¿Se agregaron semillas para la funcionalidad nueva?
□ Si el PR tiene código unsafe:
  □ ¿Se fuzzeó con ASan?
□ Si el PR corrige un bug encontrado por fuzzing:
  □ ¿Se creó regression test?
  □ ¿El crash minimizado está en fuzz/regression/?
```

### Comentarios de review útiles

```
"Este PR agrega un nuevo parser. ¿Puedes agregar un fuzz target?"

"El cambio en parse_value() toca código que fuzz_parse cubre.
¿Ejecutaste cargo fuzz run fuzz_parse localmente?"

"Este bug fue encontrado por fuzzing. El PR necesita un regression
test: copiar el crash minimizado a fuzz/regression/fuzz_parse/."

"El nuevo código unsafe en process_buffer() necesita fuzzing con ASan.
¿Puedes agregar un harness y ejecutar 10 minutos?"
```

---

## 30. Fuzzing y releases

### Pre-release checklist

```
Antes de cada release:

1. Ejecutar regression tests:
   cargo fuzz run fuzz_parse -- -runs=0
   para TODOS los targets

2. Fuzzing de release (más largo que normal):
   cargo fuzz run fuzz_parse -- -max_total_time=7200
   (2 horas mínimo para release)

3. Múltiples sanitizers:
   --sanitizer=address  (2 horas)
   --sanitizer=none     (2 horas)
   --sanitizer=memory   (si hay unsafe) (1 hora)

4. Verificar cobertura:
   cargo fuzz coverage fuzz_parse
   → Verificar que no bajó respecto al release anterior

5. Minimizar corpus:
   cargo fuzz cmin fuzz_parse
   → Commitear corpus minimizado

6. Documentar:
   "Fuzzing: X horas, Y targets, Z sanitizers, 0 crashes"
```

### Integrar fuzzing en el changelog

```markdown
## v1.5.0 — 2026-04-15

### Security
- Fixed panic in `parse_value` with empty keys (found by fuzzing)
- Fixed stack overflow with deeply nested input (found by fuzzing)

### Quality
- Fuzzing: 20 hours total, 4 targets, 3 sanitizers, 0 new crashes
- Corpus: 1,200 inputs, 85% code coverage
```

---

## 31. Cuándo agregar nuevos targets

### Triggers para crear un nuevo fuzz target

```
┌──────────────────────────────────────────────────────────────┐
│          Cuándo agregar un nuevo fuzz target                  │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  SIEMPRE crear target cuando agregas:                        │
│  ✓ Un parser de cualquier formato                            │
│  ✓ Un decoder/encoder                                       │
│  ✓ Un validador de input externo                             │
│  ✓ Código unsafe nuevo                                       │
│  ✓ Bindings FFI                                              │
│  ✓ Serialización/deserialización                             │
│                                                              │
│  CONSIDERAR crear target cuando:                             │
│  ? Una estructura de datos tiene operaciones complejas       │
│  ? Una función tiene muchos edge cases documentados          │
│  ? Un bug fue encontrado manualmente que fuzzing habría      │
│    encontrado                                                │
│                                                              │
│  NO NECESITAS target para:                                   │
│  ✗ Código glue (wiring, configuración)                      │
│  ✗ UI/CLI (no procesamiento de datos)                        │
│  ✗ Tests unitarios que ya cubren el espacio de inputs         │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Convención: un target por función pública de parsing

```bash
# Para una librería con esta API:
pub fn parse_json(input: &str) -> Result<Value, Error>;
pub fn parse_toml(input: &str) -> Result<Config, Error>;
pub fn validate_schema(value: &Value, schema: &Schema) -> Result<(), Vec<SchemaError>>;
pub fn serialize(value: &Value) -> String;

# Targets:
cargo fuzz add fuzz_json_parse
cargo fuzz add fuzz_toml_parse
cargo fuzz add fuzz_validate_schema
cargo fuzz add fuzz_json_roundtrip     # parse → serialize → parse
```

---

## 32. Métricas y dashboards

### Métricas clave a rastrear

| Métrica | Cómo medir | Meta |
|---|---|---|
| Cobertura de código | `cargo fuzz coverage` | > 80% |
| Tamaño del corpus | `ls fuzz/corpus/target/ \| wc -l` | Creciente |
| Crashes abiertos | Issues con label "fuzzing" | 0 |
| Tiempo acumulado de fuzzing | Logs de CI | Creciente |
| exec/s promedio | `-print_final_stats=1` | > 1000 |
| Tiempo medio de fix | Issue open → close | < 1 semana |

### Script de métricas

```bash
#!/bin/bash
# fuzz_metrics.sh — generar métricas de fuzzing

echo "=== Fuzzing Metrics ==="
echo ""

echo "--- Targets ---"
cargo fuzz list | while read target; do
    corpus_count=$(ls fuzz/corpus/"$target"/ 2>/dev/null | wc -l)
    regression_count=$(ls fuzz/regression/"$target"/ 2>/dev/null | wc -l)
    echo "  $target: corpus=$corpus_count regression=$regression_count"
done

echo ""
echo "--- Crash Summary ---"
for target in $(cargo fuzz list); do
    crashes=$(ls fuzz/artifacts/"$target"/crash-* 2>/dev/null | wc -l)
    if [ "$crashes" -gt 0 ]; then
        echo "  $target: $crashes unprocessed crashes!"
    fi
done

total_corpus=$(find fuzz/corpus -type f 2>/dev/null | wc -l)
total_regression=$(find fuzz/regression -type f 2>/dev/null | wc -l)
corpus_size=$(du -sh fuzz/corpus 2>/dev/null | cut -f1)

echo ""
echo "--- Totals ---"
echo "  Total corpus files: $total_corpus"
echo "  Total regression files: $total_regression"
echo "  Corpus disk usage: $corpus_size"
```

---

## 33. Comparación de estrategias de CI: C vs Rust vs Go

| Aspecto | C (AFL++/libFuzzer) | Rust (cargo-fuzz) | Go (go test -fuzz) |
|---|---|---|---|
| **CI setup** | Makefile custom + script | cargo-fuzz + GitHub Actions | `go test -fuzz` nativo |
| **Compilation** | `afl-cc` o `clang -fsanitize=fuzzer` | `cargo fuzz build` (automático) | `go test -fuzz` (automático) |
| **Regression** | Script custom + corpus dir | `cargo fuzz run -- -runs=0` | `go test` (corpus en testdata/) |
| **Corpus storage** | Manual (directorio + git/S3) | `fuzz/corpus/` + git/cache | `testdata/fuzz/` (en el repo) |
| **OSS-Fuzz** | Nativo (diseñado para C) | Soportado | Soportado |
| **CI fuzzing** | Script custom | GitHub Actions workflow | `go test -fuzz -fuzztime=60s` |
| **Crash → test** | Manual (crear archivo + script) | Manual (test unitario + corpus) | Automático (se guarda en testdata/) |
| **Sanitizers CI** | Manual en CFLAGS | `--sanitizer=X` | `-race` flag |
| **Coverage** | `llvm-cov` manual | `cargo fuzz coverage` | `go test -cover` |
| **Ergonomía CI** | Baja (mucho scripting) | Media (cargo-fuzz ayuda) | Alta (todo nativo) |

### Go: la ventaja de la integración nativa

```go
// Go no necesita cargo-fuzz ni setup especial.
// El corpus se guarda automáticamente en testdata/.
// Los regression tests se ejecutan con go test normal.

func FuzzParse(f *testing.F) {
    f.Add([]byte(`{"key": "value"}`))
    f.Fuzz(func(t *testing.T, data []byte) {
        _, _ = Parse(data)
    })
}

// Ejecutar: go test -fuzz=FuzzParse -fuzztime=60s
// Corpus: testdata/fuzz/FuzzParse/
// Regression: go test (automáticamente ejecuta el corpus)
```

### Rust: el equilibrio entre poder y ergonomía

```
Rust no tiene fuzzing nativo como Go, pero cargo-fuzz proporciona:
- Setup automático (cargo fuzz init)
- Gestión de corpus (fuzz/corpus/)
- Gestión de artifacts (fuzz/artifacts/)
- Minimización (cargo fuzz tmin)
- Cobertura (cargo fuzz coverage)
- Tipos estructurados (Arbitrary)

Lo que falta respecto a Go:
- Regression automático (Go ejecuta corpus con go test)
- Funcionar en stable (Rust requiere nightly)
- Integración con cargo test
```

---

## 34. Errores comunes

### Error 1: no commitear el corpus

```
Problema:
  Fuzzeaste 8 horas localmente.
  El CI no tiene el corpus.
  El CI empieza de cero cada vez.

Solución:
  1. Minimizar: cargo fuzz cmin target
  2. Commitear: git add fuzz/corpus/target/
  3. O cachear en CI con actions/cache
```

### Error 2: no crear regression tests de crashes

```
Problema:
  Encontraste un crash, lo arreglaste, no creaste regression test.
  6 meses después, un refactor reintroduce el bug.
  Nadie se da cuenta hasta producción.

Solución:
  SIEMPRE crear:
  1. Test unitario en tests/regression.rs
  2. Crash minimizado en fuzz/regression/target/
  3. Ambos se ejecutan en CI
```

### Error 3: fuzzing en CI demasiado largo

```
Problema:
  CI fuzzea 1 hora por PR.
  Los PRs tardan 1+ hora en pasar CI.
  Los desarrolladores se quejan.

Solución:
  - PR: 1-2 minutos de fuzzing (o solo regression)
  - Nightly: 1+ hora de fuzzing
  - Separar regression (rápido) de fuzzing (lento)
```

### Error 4: fuzzing sin semillas ni diccionario

```
Problema:
  Parser de JSON fuzzeado sin semillas ni diccionario.
  En 5 minutos, cobertura = 20%.
  Con semillas y diccionario, cobertura = 75%.

Solución:
  SIEMPRE crear:
  1. Semillas: 5-10 inputs representativos
  2. Diccionario: tokens del formato (llaves, comillas, etc.)
  3. Medir cobertura para verificar
```

### Error 5: ignorar crashes "conocidos"

```
Problema:
  "Ah, ese crash ya lo vi, es el mismo de siempre"
  Pero no lo arreglan ni lo suprimen.
  El fuzzer reporta el mismo crash en cada sesión.
  Los crashes reales se pierden entre el ruido.

Solución:
  1. Arreglar el crash → MEJOR
  2. Si no se puede arreglar ahora → crear issue + suppression
  3. NUNCA dejar crashes sin procesar indefinidamente
```

### Error 6: no medir cobertura

```
Problema:
  "Fuzzeé 8 horas y no encontré nada, el código es seguro"
  Pero la cobertura era 15% → el fuzzer no llegó a la mayoría del código.

Solución:
  cargo fuzz coverage target
  Si cobertura < 50%: mejorar semillas/diccionario/harness
  Si cobertura > 80%: buena confianza
  Documentar la cobertura en el checklist de release
```

### Error 7: un solo target para todo

```
Problema:
  Una librería con 10 funciones públicas tiene UN fuzz target
  que solo llama a parse().
  Las funciones validate(), transform(), serialize() nunca se fuzzean.

Solución:
  Un target por función pública que procesa input:
  cargo fuzz add fuzz_parse
  cargo fuzz add fuzz_validate
  cargo fuzz add fuzz_transform
  cargo fuzz add fuzz_roundtrip
```

---

## 35. Programa de práctica: integración completa

### Objetivo

Implementar el flujo completo de fuzzing integrado: targets, CI, regression tests, corpus management, métricas.

### Especificación

```
markdown_lite/
├── Cargo.toml
├── src/
│   └── lib.rs               ← parser de Markdown simplificado
├── tests/
│   ├── unit_tests.rs
│   └── regression.rs        ← tests de regression desde fuzzing
├── fuzz/
│   ├── Cargo.toml
│   ├── fuzz_targets/
│   │   ├── fuzz_parse.rs    ← parsear sin crash
│   │   ├── fuzz_roundtrip.rs← parse → render → parse → compare
│   │   └── fuzz_render.rs   ← parsear + renderizar a HTML
│   ├── corpus/
│   │   └── fuzz_parse/      ← semillas
│   ├── regression/
│   │   └── fuzz_parse/      ← crashes minimizados
│   └── dicts/
│       └── markdown.dict
├── .github/
│   └── workflows/
│       └── fuzz.yml          ← CI con los 3 niveles
└── scripts/
    └── fuzz_metrics.sh       ← métricas de fuzzing
```

### src/lib.rs

```rust
/// Parser de Markdown simplificado.
/// Soporta: headings (#), bold (**), italic (*), code (`),
/// links [text](url), listas (- item), párrafos.

#[derive(Debug, Clone, PartialEq)]
pub enum MdNode {
    Heading { level: u8, text: String },
    Paragraph(Vec<MdInline>),
    List(Vec<String>),
    CodeBlock { lang: String, code: String },
    HorizontalRule,
}

#[derive(Debug, Clone, PartialEq)]
pub enum MdInline {
    Text(String),
    Bold(String),
    Italic(String),
    Code(String),
    Link { text: String, url: String },
}

#[derive(Debug)]
pub struct MdDocument {
    pub nodes: Vec<MdNode>,
}

#[derive(Debug, Clone, PartialEq)]
pub enum ParseError {
    UnexpectedEnd,
    InvalidHeading,
    UnmatchedDelimiter(char),
    NestingTooDeep,
}

pub fn parse(input: &str) -> Result<MdDocument, ParseError> {
    let mut nodes = Vec::new();
    let lines: Vec<&str> = input.lines().collect();
    let mut i = 0;

    while i < lines.len() {
        let line = lines[i].trim_end();

        if line.is_empty() {
            i += 1;
            continue;
        }

        // Heading: # text
        if line.starts_with('#') {
            let level = line.chars().take_while(|&c| c == '#').count();
            // BUG 1: no verifica que level <= 6
            // "########" → level = 8, debería ser inválido
            let text = line[level..].trim_start().to_string();
            nodes.push(MdNode::Heading { level: level as u8, text });
            i += 1;
            continue;
        }

        // Horizontal rule: ---
        if line == "---" || line == "***" || line == "___" {
            nodes.push(MdNode::HorizontalRule);
            i += 1;
            continue;
        }

        // Code block: ```lang
        if line.starts_with("```") {
            let lang = line[3..].trim().to_string();
            let mut code_lines = Vec::new();
            i += 1;
            while i < lines.len() && !lines[i].starts_with("```") {
                code_lines.push(lines[i]);
                i += 1;
            }
            // BUG 2: si no hay cierre ```, i queda en lines.len()
            // pero no retorna error, simplemente consume todo como código
            if i < lines.len() {
                i += 1;  // saltar el cierre ```
            }
            nodes.push(MdNode::CodeBlock {
                lang,
                code: code_lines.join("\n"),
            });
            continue;
        }

        // Lista: - item
        if line.starts_with("- ") {
            let mut items = Vec::new();
            while i < lines.len() && lines[i].trim_end().starts_with("- ") {
                items.push(lines[i].trim_end()[2..].to_string());
                i += 1;
            }
            nodes.push(MdNode::List(items));
            continue;
        }

        // Párrafo (todo lo demás)
        let inlines = parse_inlines(line)?;
        nodes.push(MdNode::Paragraph(inlines));
        i += 1;
    }

    Ok(MdDocument { nodes })
}

fn parse_inlines(input: &str) -> Result<Vec<MdInline>, ParseError> {
    let mut result = Vec::new();
    let chars: Vec<char> = input.chars().collect();
    let mut i = 0;
    let mut current_text = String::new();

    while i < chars.len() {
        match chars[i] {
            '*' if i + 1 < chars.len() && chars[i + 1] == '*' => {
                // Bold: **text**
                if !current_text.is_empty() {
                    result.push(MdInline::Text(current_text.clone()));
                    current_text.clear();
                }
                i += 2;
                let mut bold_text = String::new();
                while i < chars.len() {
                    if i + 1 < chars.len() && chars[i] == '*' && chars[i + 1] == '*' {
                        break;
                    }
                    bold_text.push(chars[i]);
                    i += 1;
                }
                // BUG 3: si no hay cierre **, no retorna error
                // simplemente crea un Bold con el texto hasta el final
                if i + 1 < chars.len() {
                    i += 2;  // saltar **
                }
                result.push(MdInline::Bold(bold_text));
            }
            '*' => {
                // Italic: *text*
                if !current_text.is_empty() {
                    result.push(MdInline::Text(current_text.clone()));
                    current_text.clear();
                }
                i += 1;
                let mut italic_text = String::new();
                while i < chars.len() && chars[i] != '*' {
                    italic_text.push(chars[i]);
                    i += 1;
                }
                if i < chars.len() {
                    i += 1;  // saltar *
                }
                result.push(MdInline::Italic(italic_text));
            }
            '`' => {
                // Code: `text`
                if !current_text.is_empty() {
                    result.push(MdInline::Text(current_text.clone()));
                    current_text.clear();
                }
                i += 1;
                let mut code_text = String::new();
                while i < chars.len() && chars[i] != '`' {
                    code_text.push(chars[i]);
                    i += 1;
                }
                if i < chars.len() {
                    i += 1;
                }
                result.push(MdInline::Code(code_text));
            }
            '[' => {
                // Link: [text](url)
                if !current_text.is_empty() {
                    result.push(MdInline::Text(current_text.clone()));
                    current_text.clear();
                }
                i += 1;
                let mut link_text = String::new();
                while i < chars.len() && chars[i] != ']' {
                    link_text.push(chars[i]);
                    i += 1;
                }
                if i < chars.len() { i += 1; }  // saltar ]

                // BUG 4: si después de ] no hay (, panicea
                // porque accede a chars[i] sin verificar i < chars.len()
                if i < chars.len() && chars[i] == '(' {
                    i += 1;
                    let mut url = String::new();
                    while i < chars.len() && chars[i] != ')' {
                        url.push(chars[i]);
                        i += 1;
                    }
                    if i < chars.len() { i += 1; }
                    result.push(MdInline::Link { text: link_text, url });
                } else {
                    // No es un link, poner como texto
                    current_text.push('[');
                    current_text.push_str(&link_text);
                    current_text.push(']');
                }
            }
            ch => {
                current_text.push(ch);
                i += 1;
            }
        }
    }

    if !current_text.is_empty() {
        result.push(MdInline::Text(current_text));
    }

    Ok(result)
}

pub fn render_html(doc: &MdDocument) -> String {
    let mut html = String::new();

    for node in &doc.nodes {
        match node {
            MdNode::Heading { level, text } => {
                // BUG 5: no escapa HTML en el texto del heading
                // Si text contiene <script>, se renderiza como HTML real
                html.push_str(&format!("<h{}>{}</h{}>\n", level, text, level));
            }
            MdNode::Paragraph(inlines) => {
                html.push_str("<p>");
                for inline in inlines {
                    match inline {
                        MdInline::Text(t) => html.push_str(t),
                        MdInline::Bold(t) => {
                            html.push_str(&format!("<strong>{}</strong>", t));
                        }
                        MdInline::Italic(t) => {
                            html.push_str(&format!("<em>{}</em>", t));
                        }
                        MdInline::Code(t) => {
                            html.push_str(&format!("<code>{}</code>", t));
                        }
                        MdInline::Link { text, url } => {
                            html.push_str(&format!("<a href=\"{}\">{}</a>", url, text));
                        }
                    }
                }
                html.push_str("</p>\n");
            }
            MdNode::List(items) => {
                html.push_str("<ul>\n");
                for item in items {
                    html.push_str(&format!("  <li>{}</li>\n", item));
                }
                html.push_str("</ul>\n");
            }
            MdNode::CodeBlock { lang, code } => {
                if lang.is_empty() {
                    html.push_str(&format!("<pre><code>{}</code></pre>\n", code));
                } else {
                    html.push_str(&format!(
                        "<pre><code class=\"language-{}\">{}</code></pre>\n",
                        lang, code
                    ));
                }
            }
            MdNode::HorizontalRule => {
                html.push_str("<hr>\n");
            }
        }
    }

    html
}
```

### Bugs intencionales resumidos

```
Bug 1: Headings con level > 6 (HTML inválido: <h8>)
Bug 2: Code blocks sin cierre ``` no producen error
Bug 3: Bold sin cierre ** no produce error
Bug 4: Link [text] sin (url) puede causar panic o resultado incorrecto
Bug 5: render_html no escapa HTML → XSS potencial
```

### Semillas

```bash
mkdir -p fuzz/corpus/fuzz_parse

echo '# Heading 1' > fuzz/corpus/fuzz_parse/seed_h1
echo '## Heading 2' > fuzz/corpus/fuzz_parse/seed_h2
echo '**bold text**' > fuzz/corpus/fuzz_parse/seed_bold
echo '*italic text*' > fuzz/corpus/fuzz_parse/seed_italic
echo '`code`' > fuzz/corpus/fuzz_parse/seed_code
echo '[link](http://example.com)' > fuzz/corpus/fuzz_parse/seed_link
echo -e '- item 1\n- item 2\n- item 3' > fuzz/corpus/fuzz_parse/seed_list
echo -e '```rust\nfn main() {}\n```' > fuzz/corpus/fuzz_parse/seed_codeblock
echo '---' > fuzz/corpus/fuzz_parse/seed_hr
echo -e '# Title\n\nSome **bold** and *italic* text.\n\n- list item' > fuzz/corpus/fuzz_parse/seed_mixed
```

### Diccionario

```
# fuzz/dicts/markdown.dict
"#"
"##"
"###"
"####"
"#####"
"######"
"**"
"*"
"`"
"```"
"["
"]"
"("
")"
"- "
"---"
"***"
"___"
"\n"
"\r\n"
"<script>"
"</script>"
"<img"
"&amp;"
"&lt;"
"&gt;"
```

### Tareas

1. **Crea el proyecto** e implementa todo el código

2. **Escribe 3 harnesses**:
   - `fuzz_parse`: parsear sin crash
   - `fuzz_roundtrip`: parse → render → verificar HTML bien formado
   - `fuzz_render`: parse → render_html → verificar que no contiene `<script>` sin escapar

3. **Crea semillas y diccionario**

4. **Fuzzea localmente** cada target 10 minutos

5. **Para cada crash**:
   - Minimizar
   - Crear test unitario en `tests/regression.rs`
   - Copiar minimizado a `fuzz/regression/target/`
   - Corregir el bug
   - Verificar

6. **Crea el workflow de GitHub Actions** (`.github/workflows/fuzz.yml`) con los 3 niveles:
   - Regression en cada PR
   - Fuzzing breve (2 min) en cada PR
   - Fuzzing largo (1 hora) semanal

7. **Escribe `scripts/fuzz_metrics.sh`** que reporte:
   - Número de targets
   - Tamaño del corpus por target
   - Número de crashes sin procesar
   - Número de regression tests

8. **Documenta el flujo** en un breve `fuzz/README.md`:
   - Cómo fuzzear localmente
   - Cómo procesar un crash
   - Cómo agregar un nuevo target
   - Cómo ejecutar regression tests

---

## 36. Ejercicios

### Ejercicio 1: regression test pipeline

**Objetivo**: Implementar el pipeline completo de crash → regression test.

**Tareas**:

**a)** Toma el parser JSON de T01 (json_value) y fuzzea 10 minutos.

**b)** Para CADA crash encontrado (deberías encontrar al menos 2):
   1. Minimizar con `cargo fuzz tmin`
   2. Analizar con `xxd` y `cargo fuzz fmt`
   3. Escribir un test unitario con la convención de nombres:
      ```rust
      #[test]
      fn regression_HASH_DESCRIPTION() { ... }
      ```
   4. Copiar el minimizado a `fuzz/regression/target/`
   5. Corregir el bug
   6. Verificar: `cargo fuzz run target minimized` no crashea
   7. Verificar: `cargo test regression` pasa

**c)** Ejecutar el regression corpus completo:
   ```bash
   cargo fuzz run fuzz_parse fuzz/regression/fuzz_parse -- -runs=0
   ```

---

### Ejercicio 2: CI con GitHub Actions

**Objetivo**: Configurar CI fuzzing para un proyecto.

**Tareas**:

**a)** Crea un repositorio en GitHub con el parser JSON de T01.

**b)** Configura GitHub Actions con:
   - Nivel 1: regression tests en cada push
   - Nivel 2: fuzzing de 2 minutos en PRs
   - Cache del corpus entre runs

**c)** Abre un PR que introduce un bug:
   ```rust
   // Cambiar un .get() por un [] indexing → panic con input vacío
   ```

**d)** ¿El CI detecta el crash? ¿En qué nivel (regression o fuzzing breve)?

**e)** Corrige el bug, agrega regression test, y verifica que el CI pasa.

---

### Ejercicio 3: medición de cobertura y optimización

**Objetivo**: Usar cobertura para guiar la mejora del fuzzing.

**Tareas**:

**a)** Fuzzea un target durante 5 minutos.

**b)** Mide la cobertura: `cargo fuzz coverage target`

**c)** Identifica las líneas/funciones NO cubiertas.

**d)** Para cada zona no cubierta:
   - ¿Es dead code? → eliminar
   - ¿Le falta una semilla? → crear semilla que cubra esa rama
   - ¿Le falta un token en el diccionario? → agregar

**e)** Fuzzea otros 5 minutos con las mejoras.

**f)** Mide la cobertura de nuevo. ¿Mejoró? ¿Por cuánto?

---

### Ejercicio 4: comparar Go vs Rust fuzzing workflow

**Objetivo**: Experimentar con las diferencias de ergonomía.

**Tareas**:

**a)** Implementa el MISMO parser simple (ej: key=value) en Go y Rust.

**b)** Configura fuzzing en ambos:
   - Go: `func FuzzParse(f *testing.F) { ... }`
   - Rust: `cargo fuzz init` + `fuzz_target!`

**c)** Fuzzea cada uno 5 minutos. Compara:
   - ¿Cuántas líneas de setup necesitó cada uno?
   - ¿Cómo se gestionan los crashes en cada uno?
   - ¿Cómo se ejecutan los regression tests?
   - ¿Cuál es más ergonómico para CI?

**d)** Documenta las ventajas y desventajas de cada enfoque:
   - Go: fuzzing nativo, corpus en testdata/, regression automático
   - Rust: Arbitrary trait, múltiples sanitizers, cargo-fuzz ecosystem

---

## Navegación

- **Anterior**: [T03 - Fuzz + sanitizers](../T03_Fuzz_sanitizers/README.md)
- **Siguiente**: [C03/S03/T01 - AddressSanitizer (ASan)](../../S03_Sanitizers_Red_de_Seguridad/T01_AddressSanitizer/README.md)

---

> **Sección 2: Fuzzing en Rust** — Tópico 4 de 4 completado
>
> - T01: cargo-fuzz ✓
> - T02: Arbitrary trait ✓
> - T03: Fuzz + sanitizers ✓
> - T04: Integrar fuzzing en workflow ✓
>
> **Fin de la Sección 2: Fuzzing en Rust**
>
> Resumen de S02:
> 1. **cargo-fuzz**: instalación, fuzz_target!, corpus/artifacts, flujo completo
> 2. **Arbitrary trait**: derive automático, implementación manual, secuencias de operaciones, patrón command
> 3. **Fuzz + sanitizers**: ASan/MSan/TSan con RUSTFLAGS y cargo-fuzz, interpretar reportes, estrategia multi-sanitizer
> 4. **Integrar fuzzing en workflow**: cuánto tiempo, regression tests, CI (3 niveles), OSS-Fuzz, gestión de corpus/crashes
