# Bisección: localizar regresiones con git bisect

## Índice

1. [¿Qué es una regresión?](#qué-es-una-regresión)
2. [git bisect: búsqueda binaria en el historial](#git-bisect-búsqueda-binaria-en-el-historial)
3. [Bisección manual paso a paso](#bisección-manual-paso-a-paso)
4. [Bisección automática con scripts](#bisección-automática-con-scripts)
5. [Bisección con cargo test](#bisección-con-cargo-test)
6. [Estrategias avanzadas](#estrategias-avanzadas)
7. [Bisección fuera de git](#bisección-fuera-de-git)
8. [Errores comunes](#errores-comunes)
9. [Cheatsheet](#cheatsheet)
10. [Ejercicios](#ejercicios)

---

## ¿Qué es una regresión?

Una regresión es un bug que **no existía antes**. Algo que funcionaba dejó de
funcionar después de un cambio. La pregunta clave es: **¿cuál commit introdujo
el bug?**

```
┌──────────────────────────────────────────────────────────────┐
│  Historial de commits                                        │
│                                                              │
│  A ─── B ─── C ─── D ─── E ─── F ─── G ─── H (HEAD)       │
│  ✅    ✅    ✅    ✅    ❌    ❌    ❌    ❌               │
│                          ▲                                   │
│                          │                                   │
│                    Este commit introdujo el bug               │
│                                                              │
│  Búsqueda lineal: probar H, G, F, E, D → 5 intentos         │
│  Búsqueda binaria (bisect): probar D, F, E → 3 intentos     │
│                                                              │
│  Con 1000 commits: lineal = hasta 1000, bisect = ~10         │
│  Complejidad: O(n) vs O(log₂ n)                             │
└──────────────────────────────────────────────────────────────┘
```

### Cuándo usar bisección

```
┌──────────────────────────────────────────────────────────────┐
│  Situación                          │ ¿Bisección?            │
├─────────────────────────────────────┼────────────────────────┤
│ "Esto funcionaba la semana pasada"  │ ✅ Caso ideal          │
│ "El test pasaba antes del merge"    │ ✅ Caso ideal          │
│ Bug existe desde siempre            │ ❌ No hay "good" commit│
│ Bug en código nuevo (sin historial) │ ❌ Nada que bisectar   │
│ Performance degradó gradualmente    │ ⚠️ Posible si puedes   │
│                                     │ medir automáticamente  │
│ Bug intermitente                    │ ⚠️ Solo si el test     │
│                                     │ lo detecta de forma    │
│                                     │ confiable              │
└─────────────────────────────────────┴────────────────────────┘
```

---

## git bisect: búsqueda binaria en el historial

`git bisect` automatiza la búsqueda binaria sobre el historial de commits.
Tú le dices qué commits son "buenos" (sin bug) y "malos" (con bug), y git
selecciona el punto medio para que lo pruebes.

### Concepto

```
┌──────────────────────────────────────────────────────────────┐
│  git bisect: búsqueda binaria                                │
│                                                              │
│  Paso 0: Marcar extremos                                     │
│  A ─── B ─── C ─── D ─── E ─── F ─── G ─── H              │
│  good                                          bad           │
│                                                              │
│  Paso 1: git prueba el punto medio (D)                       │
│  A ─── B ─── C ─── [D] ─── E ─── F ─── G ─── H            │
│  good              test?                       bad           │
│  → Tú pruebas D: funciona → "good"                          │
│                                                              │
│  Paso 2: git prueba entre D y H → F                          │
│                    D ─── E ─── [F] ─── G ─── H              │
│                    good        test?          bad             │
│  → Tú pruebas F: falla → "bad"                              │
│                                                              │
│  Paso 3: git prueba entre D y F → E                          │
│                    D ─── [E] ─── F                           │
│                    good  test?   bad                          │
│  → Tú pruebas E: falla → "bad"                              │
│                                                              │
│  Resultado: E es el primer commit malo                       │
│  E is the first bad commit                                   │
│  3 pruebas para 8 commits (log₂ 8 = 3)                      │
└──────────────────────────────────────────────────────────────┘
```

---

## Bisección manual paso a paso

### Paso 1: Iniciar bisect

```bash
# Iniciar el proceso de bisección
git bisect start

# Marcar el commit actual (HEAD) como malo
git bisect bad

# Marcar un commit antiguo donde funcionaba como bueno
# Puede ser un tag, un hash, o una referencia relativa
git bisect good v2.0
# o
git bisect good abc1234
# o
git bisect good HEAD~50
```

Git responde indicando cuántos pasos quedan:

```
Bisecting: 25 revisions left to test after this (roughly 5 steps)
[def5678...] Refactor parser module
```

### Paso 2: Probar cada commit

Git hace checkout del commit del punto medio. Tú lo pruebas:

```bash
# Compilar y probar
cargo build && cargo test

# O ejecutar el caso específico que falla
./target/debug/my_program < test_input.txt
echo $?  # 0 = funciona, ≠0 = falla
```

### Paso 3: Marcar como good o bad

```bash
# Si este commit funciona correctamente:
git bisect good
# Git selecciona el siguiente punto medio

# Si este commit tiene el bug:
git bisect bad
# Git reduce el rango por la otra mitad
```

### Paso 4: Repetir hasta encontrar el culpable

Git eventualmente identifica el commit exacto:

```
def5678abc is the first bad commit
commit def5678abc
Author: Developer <dev@example.com>
Date:   Mon Jan 15 14:30:00 2024

    Optimize parser to use raw pointers for speed

 src/parser.rs | 45 ++++++++++++++++++++++-----------------------
 1 file changed, 22 insertions(+), 23 deletions(-)
```

### Paso 5: Terminar y volver a HEAD

```bash
# Terminar bisección y volver al branch original
git bisect reset

# Ver el diff del commit culpable
git show def5678abc

# Investigar qué cambió
git diff def5678abc~1 def5678abc
```

### Manejar commits que no compilan

A veces git selecciona un commit que no compila (por estar en medio de un
refactor, por ejemplo):

```bash
# Si el commit actual no se puede probar (no compila, test irrelevante, etc.)
git bisect skip

# Git selecciona un commit cercano para probar en su lugar
# Si hay muchos commits contiguos que no compilan, puede que
# bisect no pueda determinar el exacto y dé un rango:
# "There are only 'skip'ped commits left to test.
#  The first bad commit could be any of: abc123 def456 ghi789"
```

---

## Bisección automática con scripts

La verdadera potencia de `git bisect` es la automatización: un script prueba
cada commit sin intervención humana.

### Sintaxis básica

```bash
git bisect start BAD_COMMIT GOOD_COMMIT
git bisect run SCRIPT
```

El script debe retornar:
- **0** → el commit es "good"
- **1-124, 126-127** → el commit es "bad"
- **125** → no se puede probar este commit ("skip")

### Ejemplo: encontrar qué commit rompió un test

```bash
# Un solo comando
git bisect start HEAD v2.0 -- && \
git bisect run cargo test test_parse_header
```

`cargo test` retorna 0 si pasa y no-cero si falla — exactamente lo que
`bisect run` necesita.

### Script personalizado

```bash
#!/bin/bash
# bisect_check.sh — Script para git bisect run

# Intentar compilar
cargo build 2>/dev/null
if [ $? -ne 0 ]; then
    echo "No compila, skip"
    exit 125  # skip
fi

# Ejecutar el test específico
cargo test test_parse_header -- --quiet 2>/dev/null
if [ $? -eq 0 ]; then
    echo "Test pasa → good"
    exit 0   # good
else
    echo "Test falla → bad"
    exit 1   # bad
fi
```

```bash
chmod +x bisect_check.sh
git bisect start HEAD v2.0
git bisect run ./bisect_check.sh
```

### Ejemplo: encontrar qué commit causó una regresión de rendimiento

```bash
#!/bin/bash
# bisect_perf.sh — ¿Este commit es más lento que el umbral?

cargo build --release 2>/dev/null || exit 125

# Ejecutar benchmark y capturar tiempo
TIME=$( { /usr/bin/time -f "%e" ./target/release/my_program \
    < bench_input.txt > /dev/null; } 2>&1 )

echo "Tiempo: ${TIME}s"

# Umbral: si tarda más de 2 segundos, es "bad"
if (( $(echo "$TIME > 2.0" | bc -l) )); then
    exit 1   # bad (lento)
else
    exit 0   # good (rápido)
fi
```

```bash
git bisect start HEAD v2.0
git bisect run ./bisect_perf.sh
```

### Ejemplo: encontrar qué commit introdujo un warning

```bash
#!/bin/bash
# bisect_warning.sh — ¿Este commit produce el warning?

OUTPUT=$(cargo build 2>&1)

if echo "$OUTPUT" | grep -q "unused variable"; then
    exit 1   # bad (tiene el warning)
else
    exit 0   # good (sin warning)
fi
```

### Ejemplo: bisección con Miri

```bash
#!/bin/bash
# bisect_miri.sh — ¿Este commit tiene UB?

# Verificar que compila
cargo build 2>/dev/null || exit 125

# Ejecutar Miri
cargo +nightly miri test test_buffer_ops 2>/dev/null
if [ $? -eq 0 ]; then
    exit 0   # good (sin UB)
else
    exit 1   # bad (UB detectado)
fi
```

---

## Bisección con cargo test

### Filtrar por test específico

```bash
# Bisectar hasta encontrar qué commit rompió un test específico
git bisect start HEAD v2.0
git bisect run cargo test test_specific_function

# Bisectar con un módulo de tests
git bisect start HEAD v2.0
git bisect run cargo test parser::tests

# Bisectar solo tests de integración
git bisect start HEAD v2.0
git bisect run cargo test --test integration_tests
```

### Manejar cambios en el test mismo

Si el test que usas para bisectar **no existía** en los commits antiguos,
necesitas traerlo desde fuera:

```bash
#!/bin/bash
# bisect_with_external_test.sh

# Copiar el test desde un archivo externo (no del repo)
cp /tmp/regression_test.rs tests/regression_test.rs

# Compilar y ejecutar
cargo test --test regression_test 2>/dev/null
RESULT=$?

# Limpiar (no dejar archivos modificados para el siguiente checkout)
git checkout -- tests/ 2>/dev/null
rm -f tests/regression_test.rs

exit $RESULT
```

### Limitar bisección a paths específicos

```bash
# Solo considerar commits que tocaron src/parser.rs
git bisect start HEAD v2.0 -- src/parser.rs

# Esto salta automáticamente commits que no modificaron ese archivo
# Reduce drásticamente el número de pruebas necesarias
```

---

## Estrategias avanzadas

### Bisección con merge commits

En historial con muchos merges, bisect puede entrar en branches laterales:

```
main:   A ─── B ─── M ─── C ─── D (HEAD)
                    /
feature: X ─── Y ──
```

```bash
# Opción 1: bisectar solo en el historial lineal (first-parent)
git bisect start HEAD v2.0 --first-parent
# Solo prueba A, B, M, C, D — no X, Y

# Opción 2: si bisect entra en un branch lateral que no compila
git bisect skip  # Saltar ese commit
```

### Bisección inversa: encontrar qué commit arregló un bug

A veces quieres saber cuándo se **arregló** algo (para backport, por ejemplo).
Invierte la lógica:

```bash
# El commit antiguo TIENE el bug (bad), el actual NO (good)
git bisect start
git bisect good HEAD        # Ahora funciona
git bisect bad v1.0         # Antes fallaba

# O con script: invertir exit codes
git bisect run bash -c \
    'cargo test test_the_bug 2>/dev/null && exit 1 || exit 0'
# Si el test PASA → el bug está arreglado → para bisect inverso es "bad"
# Si el test FALLA → el bug existe → para bisect inverso es "good"
```

### Guardar y restaurar sesiones

```bash
# Guardar el estado actual de la bisección
git bisect log > bisect_session.log

# Si necesitas interrumpir y continuar después:
git bisect reset

# Restaurar la sesión
git bisect replay bisect_session.log
```

### Bisección con submodules

```bash
#!/bin/bash
# bisect con submodules: actualizar submodules en cada paso

git submodule update --init --recursive 2>/dev/null || exit 125
cargo build 2>/dev/null || exit 125
cargo test test_name
```

### Visualizar el proceso

```bash
# Ver el log de la bisección actual
git bisect log

# Ver gráficamente qué commits se han probado
git bisect visualize
# Abre gitk (si disponible) mostrando el rango restante

# Ver en terminal (sin gitk)
git bisect visualize --oneline
```

---

## Bisección fuera de git

El concepto de bisección se aplica más allá de git.

### Bisección de input (complemento de T01)

```bash
#!/bin/bash
# binary_search_input.sh — Encontrar la línea mínima que causa crash
FILE="$1"
PROGRAM="$2"
TOTAL=$(wc -l < "$FILE")
LOW=1
HIGH=$TOTAL

echo "Buscando en $TOTAL líneas..."

while [ $LOW -lt $HIGH ]; do
    MID=$(( (LOW + HIGH) / 2 ))
    echo "Probando primeras $MID líneas..."

    head -$MID "$FILE" | $PROGRAM > /dev/null 2>&1
    if [ $? -eq 0 ]; then
        # Funciona con $MID líneas → el problema está después
        LOW=$((MID + 1))
    else
        # Falla con $MID líneas → el problema está antes o aquí
        HIGH=$MID
    fi
done

echo "La línea que causa el crash es la $LOW:"
sed -n "${LOW}p" "$FILE"
```

```bash
./binary_search_input.sh big_input.txt ./my_program
```

### Bisección de configuración

```bash
#!/bin/bash
# ¿Cuál flag de compilación causa el bug?
FLAGS=("-O1" "-O2" "-O3" "-flto" "-funroll-loops" "-fvectorize")

for flag in "${FLAGS[@]}"; do
    echo "Probando con $flag..."
    gcc -g "$flag" -o test_program program.c
    if ! ./test_program < test_input.txt > /dev/null 2>&1; then
        echo "BUG aparece con: $flag"
    fi
done
```

### Bisección de dependencias

```bash
#!/bin/bash
# ¿Cuál versión de una dependencia introdujo el bug?
VERSIONS=("0.8.0" "0.8.1" "0.8.2" "0.9.0" "0.9.1" "0.10.0")

for ver in "${VERSIONS[@]}"; do
    echo "Probando serde $ver..."
    # Modificar Cargo.toml con la versión específica
    sed -i "s/serde = \".*\"/serde = \"$ver\"/" Cargo.toml
    cargo test test_serialization 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "Bug aparece en serde $ver"
        break
    fi
done
# Restaurar
git checkout -- Cargo.toml
```

---

## Errores comunes

### 1. No verificar que el commit "good" realmente funciona

```bash
# ❌ Asumir que "la versión anterior funcionaba"
git bisect start HEAD v2.0

# ✅ Verificar primero
git stash  # Guardar cambios locales
git checkout v2.0
cargo test test_specific  # ¿Realmente pasa?
git checkout -  # Volver
git stash pop

# Luego iniciar bisect con confianza
git bisect start HEAD v2.0
```

### 2. Olvidar `git bisect reset` al terminar

```bash
# ❌ Después de bisect, estás en "detached HEAD"
# Si empiezas a trabajar aquí, tus commits quedan huérfanos

# ✅ Siempre terminar con:
git bisect reset
# Esto te devuelve al branch donde estabas antes de bisect
```

### 3. Script de bisect que modifica el working tree sin limpiar

```bash
# ❌ El script modifica archivos que git necesita limpios
#!/bin/bash
echo "test" >> src/main.rs  # Modifica un archivo tracked
cargo test test_name

# Cuando git intenta checkout del siguiente commit:
# error: Your local changes would be overwritten by checkout

# ✅ Limpiar siempre al final del script
#!/bin/bash
# Hacer cambios temporales si es necesario
cargo test test_name
RESULT=$?
git checkout -- .  # Restaurar todo
exit $RESULT
```

### 4. No usar exit code 125 para commits que no compilan

```bash
# ❌ Marcar como "bad" un commit que simplemente no compila
cargo build || exit 1  # bisect piensa que este commit tiene el bug

# ✅ Usar exit 125 para "no se puede probar"
cargo build 2>/dev/null
if [ $? -ne 0 ]; then
    exit 125  # skip — no compila, no es culpa de este commit
fi
cargo test test_name
```

### 5. Bisectar un bug intermitente sin repeticiones

```bash
# ❌ Probar una vez: puede dar falso "good"
cargo test test_flaky

# ✅ Ejecutar múltiples veces para confiabilidad
#!/bin/bash
cargo build 2>/dev/null || exit 125

# Ejecutar 10 veces: si falla al menos una, es "bad"
for i in $(seq 1 10); do
    cargo test test_flaky -- --quiet 2>/dev/null || exit 1
done
exit 0  # Pasó las 10 veces → probablemente good
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│              GIT BISECT CHEATSHEET                           │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  BISECCIÓN MANUAL                                            │
│  git bisect start                     Iniciar                │
│  git bisect bad [COMMIT]              Marcar como malo       │
│  git bisect good [COMMIT]             Marcar como bueno      │
│  git bisect skip                      Saltar (no compilable) │
│  git bisect reset                     Terminar y volver      │
│                                                              │
│  BISECCIÓN AUTOMÁTICA                                        │
│  git bisect start BAD GOOD                                   │
│  git bisect run SCRIPT                Ejecutar script        │
│  git bisect run cargo test NOMBRE     Con cargo test         │
│                                                              │
│  EXIT CODES DEL SCRIPT                                       │
│  0          → good (funciona)                                │
│  1-124      → bad (tiene el bug)                             │
│  125        → skip (no se puede probar)                      │
│  126-127    → bisect aborta (error del script)               │
│                                                              │
│  OPCIONES ÚTILES                                             │
│  git bisect start -- PATH             Limitar a archivos     │
│  git bisect start --first-parent      Solo main (no merges)  │
│  git bisect log                       Ver historial          │
│  git bisect log > session.log         Guardar sesión         │
│  git bisect replay session.log        Restaurar sesión       │
│  git bisect visualize                 Ver en gitk            │
│  git bisect visualize --oneline       Ver en terminal        │
│                                                              │
│  PATRONES COMUNES                                            │
│  ┌──────────────────────────────────────────────────┐       │
│  │ # Test que falla                                  │       │
│  │ git bisect start HEAD v2.0                        │       │
│  │ git bisect run cargo test test_name               │       │
│  │                                                   │       │
│  │ # Con script (maneja no-compila)                  │       │
│  │ git bisect start HEAD v2.0                        │       │
│  │ git bisect run ./bisect_check.sh                  │       │
│  │                                                   │       │
│  │ # Regresión de rendimiento                        │       │
│  │ git bisect start HEAD v2.0                        │       │
│  │ git bisect run ./bisect_perf.sh                   │       │
│  │                                                   │       │
│  │ # Bisección inversa (cuándo se arregló)           │       │
│  │ git bisect start                                  │       │
│  │ git bisect good HEAD   # ahora funciona           │       │
│  │ git bisect bad v1.0    # antes fallaba            │       │
│  └──────────────────────────────────────────────────┘       │
│                                                              │
│  TEMPLATE DE SCRIPT                                          │
│  #!/bin/bash                                                 │
│  cargo build 2>/dev/null || exit 125   # skip si no compila │
│  cargo test TEST_NAME -- --quiet 2>/dev/null                │
│  # exit code de cargo test se propaga automáticamente        │
│                                                              │
│  COMPLEJIDAD                                                 │
│  N commits → ~log₂(N) pruebas                               │
│  100 commits → ~7 pruebas                                    │
│  1000 commits → ~10 pruebas                                  │
│  10000 commits → ~14 pruebas                                 │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Bisección manual simulada

Crea un repositorio de práctica con una regresión conocida:

```bash
# Crear repo de prueba
mkdir /tmp/bisect_practice && cd /tmp/bisect_practice
git init

# Crear 10 commits, el 6to introduce un bug
for i in $(seq 1 10); do
    if [ $i -lt 6 ]; then
        echo "fn compute(x: i32) -> i32 { x * 2 }" > lib.rs
    else
        echo "fn compute(x: i32) -> i32 { x * 3 }" > lib.rs  # Bug
    fi
    echo "// version $i" >> lib.rs
    git add lib.rs
    git commit -m "Commit $i"
done
```

**Tareas:**
1. Usa `git bisect start`, marca HEAD como `bad` y el primer commit como `good`
2. En cada paso, verifica si `lib.rs` contiene `x * 2` (good) o `x * 3` (bad)
3. Anota cuántos pasos necesitaste para 10 commits
4. Verifica que el resultado coincide con lo esperado (commit 6)
5. No olvides `git bisect reset` al terminar

**Pregunta de reflexión:** Si en vez de 10 commits hubiera 1000, ¿cuántos pasos
necesitarías? ¿Y si hubiera 1,000,000?

---

### Ejercicio 2: Bisección automática con script

Usando el mismo repositorio del Ejercicio 1:

```bash
# Crear el script de prueba
cat > /tmp/bisect_check.sh << 'SCRIPT'
#!/bin/bash
# Verificar si compute usa x * 2 (correcto) o x * 3 (bug)
grep -q "x \* 2" lib.rs
SCRIPT
chmod +x /tmp/bisect_check.sh
```

**Tareas:**
1. Ejecuta la bisección automática:
   ```bash
   git bisect start HEAD $(git rev-list --max-parents=0 HEAD)
   git bisect run /tmp/bisect_check.sh
   ```
2. Observa el output: ¿cuántos pasos tomó?
3. Modifica el script para que también funcione si el archivo no existe (exit 125)
4. Ejecuta `git bisect log` antes de `reset` para ver el historial de decisiones

**Pregunta de reflexión:** ¿Qué pasaría si el script de bisección tuviera un bug
y siempre retornara 0? ¿Y si siempre retornara 1?

---

### Ejercicio 3: Bisección de un bug real

Crea un proyecto Rust con una regresión gradual:

```bash
mkdir /tmp/bisect_rust && cd /tmp/bisect_rust
cargo init --name bisect_demo
```

Crea estos commits secuencialmente:

```rust
// Commit 1: función original correcta
pub fn parse_value(s: &str) -> Result<i32, String> {
    s.trim().parse::<i32>().map_err(|e| e.to_string())
}

// Commit 2: agregar soporte para hex (correcto)
pub fn parse_value(s: &str) -> Result<i32, String> {
    let s = s.trim();
    if let Some(hex) = s.strip_prefix("0x") {
        i32::from_str_radix(hex, 16).map_err(|e| e.to_string())
    } else {
        s.parse::<i32>().map_err(|e| e.to_string())
    }
}

// Commit 3: agregar soporte para binario (correcto)
// (agregar branch para "0b" prefix)

// Commit 4: "optimizar" — introduce bug
pub fn parse_value(s: &str) -> Result<i32, String> {
    let s = s.trim();
    if s.starts_with("0x") {
        i32::from_str_radix(&s[2..], 16).map_err(|e| e.to_string())
    } else if s.starts_with("0b") {
        i32::from_str_radix(&s[2..], 2).map_err(|e| e.to_string())
    } else {
        s.parse::<i32>().map_err(|e| e.to_string())
    }
    // BUG: "0x" en el original usaba strip_prefix (Unicode-safe)
    // &s[2..] puede panic con input multibyte como "0x️42"
}

// Commits 5-8: cambios no relacionados (en otros archivos)
```

**Tareas:**
1. Crea el repositorio con los 8 commits
2. Escribe un test que falle con el bug del commit 4:
   ```rust
   #[test]
   fn test_parse_value_edge() {
       // Este input tiene un emoji invisible después de "0x"
       assert!(parse_value("  0x1F  ").is_ok());
   }
   ```
3. Usa `git bisect run cargo test test_parse_value_edge` para encontrar el commit
4. Verifica que bisect identifica el commit 4 como culpable
5. Corrige el bug y escribe un test de regresión

**Pregunta de reflexión:** En este caso, el "refactor" del commit 4 cambió
`strip_prefix` por indexación con `&s[2..]`. ¿Por qué `strip_prefix` es más
robusto? ¿Qué otros patrones de Rust son preferibles a la indexación directa
de strings?