# T03 — Cobertura en CI: integrar gcov con Make/CMake, umbrales mínimos, trend tracking

> **Bloque 17 — Testing e Ingeniería de Software → C01 — Testing en C → S04 — Cobertura en C → T03**

---

## Índice

1. [Por qué cobertura en CI](#1-por-qué-cobertura-en-ci)
2. [El pipeline de cobertura en CI](#2-el-pipeline-de-cobertura-en-ci)
3. [Integración con Make: targets de CI](#3-integración-con-make-targets-de-ci)
4. [Integración con CMake: targets de CI](#4-integración-con-cmake-targets-de-ci)
5. [GitHub Actions: workflow completo con lcov](#5-github-actions-workflow-completo-con-lcov)
6. [GitHub Actions: workflow con gcovr](#6-github-actions-workflow-con-gcovr)
7. [GitLab CI: pipeline completo](#7-gitlab-ci-pipeline-completo)
8. [Umbrales mínimos: fail on drop](#8-umbrales-mínimos-fail-on-drop)
9. [Coverage ratchet: el trinquete monotónico](#9-coverage-ratchet-el-trinquete-monotónico)
10. [Trend tracking: visualizar la evolución](#10-trend-tracking-visualizar-la-evolución)
11. [Servicios externos: Codecov y Coveralls](#11-servicios-externos-codecov-y-coveralls)
12. [Badges de cobertura en el README](#12-badges-de-cobertura-en-el-readme)
13. [Cobertura en Pull Requests: comentarios automáticos](#13-cobertura-en-pull-requests-comentarios-automáticos)
14. [Artefactos: almacenar reportes HTML](#14-artefactos-almacenar-reportes-html)
15. [Cobertura diferencial: solo lo que cambió](#15-cobertura-diferencial-solo-lo-que-cambió)
16. [Multi-plataforma y matrix builds](#16-multi-plataforma-y-matrix-builds)
17. [Optimización del pipeline de cobertura](#17-optimización-del-pipeline-de-cobertura)
18. [Comparación CI de cobertura: C vs Rust vs Go](#18-comparación-ci-de-cobertura-c-vs-rust-vs-go)
19. [Errores comunes en CI de cobertura](#19-errores-comunes-en-ci-de-cobertura)
20. [Ejemplo completo: proyecto multi-módulo con CI full](#20-ejemplo-completo-proyecto-multi-módulo-con-ci-full)
21. [Programa de práctica](#21-programa-de-práctica)
22. [Ejercicios](#22-ejercicios)

---

## 1. Por qué cobertura en CI

La cobertura local es útil para el desarrollador individual, pero la cobertura en CI (Continuous Integration) protege al **equipo** y al **proyecto a lo largo del tiempo**.

### El problema sin CI

```
  Sin cobertura en CI:

  Semana 1: Dev A escribe módulo auth con 85% cobertura
  Semana 2: Dev B añade feature, rompe un test, lo comenta "temporalmente"
  Semana 3: Dev C refactoriza, no corre cobertura localmente
  Semana 4: Dev A hace merge de un PR grande sin tests
  Semana 5: La cobertura es 42%, nadie sabe cuándo bajó

  ┌─────────────────────────────────────────┐
  │  Cobertura  │                           │
  │  85% ─────  ■                           │
  │             │ ■                         │
  │  70% ─────  │   ■                       │
  │             │     ■                     │
  │  55% ─────  │       ■                   │
  │             │         ■                 │
  │  42% ─────  │           ■   ← nadie    │
  │             │               notó       │
  │  ───────────┼──┬──┬──┬──┬──┬──         │
  │             s1 s2 s3 s4 s5             │
  └─────────────────────────────────────────┘
```

### Con cobertura en CI

```
  Con cobertura en CI y umbral mínimo:

  Semana 1: 85% ✓
  Semana 2: Dev B intenta merge → CI falla "coverage dropped to 78%" → fix antes de merge
  Semana 3: Dev C refactoriza → CI pasa, 84%
  Semana 4: Dev A PR grande → CI falla "new code has 30% coverage" → añade tests
  Semana 5: 86% — tendencia estable o creciente

  ┌─────────────────────────────────────────┐
  │  Cobertura  │                           │
  │  85% ─────  ■─────■──■─────■──■         │
  │             │                           │
  │  Umbral     │- - - - - - - - - - -     │
  │  80% ─────  │                           │
  │             │  ✗ bloqueado              │
  │             │    (no merge)             │
  │  ───────────┼──┬──┬──┬──┬──┬──         │
  │             s1 s2 s3 s4 s5             │
  └─────────────────────────────────────────┘
```

### Qué aporta la cobertura en CI

```
Función                    Detalle
──────────────────────────────────────────────────────────────────────────
Gate de calidad            Bloquear merge si cobertura baja del umbral
Trend tracking             Ver evolución a lo largo del tiempo
Feedback en PR             Comentario automático con % de código nuevo cubierto
Accountability             Todo el equipo ve las métricas
Prevención de regresión    Detectar bajadas antes de que lleguen a main
Documentación viva         El badge en el README muestra el estado actual
Audit trail                Historial de cobertura por commit
```

---

## 2. El pipeline de cobertura en CI

### Flujo genérico

```
  ┌─────────────────────────────────────────────────────────────────────┐
  │                   PIPELINE DE COBERTURA EN CI                       │
  │                                                                     │
  │  ┌────────────┐    ┌──────────────┐    ┌──────────────────┐        │
  │  │  1. BUILD   │───►│  2. TEST     │───►│  3. CAPTURE      │        │
  │  │  con --cov  │    │  ejecutar    │    │  lcov/gcovr      │        │
  │  │  -O0 -g     │    │  binarios    │    │  capturar datos  │        │
  │  └────────────┘    └──────────────┘    └────────┬─────────┘        │
  │                                                  │                  │
  │                                                  ▼                  │
  │  ┌────────────┐    ┌──────────────┐    ┌──────────────────┐        │
  │  │  6. REPORT  │◄───│  5. CHECK    │◄───│  4. FILTER       │        │
  │  │  upload/    │    │  umbral      │    │  excluir /usr/   │        │
  │  │  artifact   │    │  mínimo      │    │  excluir tests   │        │
  │  └────────────┘    └──────────────┘    └──────────────────┘        │
  │                                                                     │
  │  Opcionales:                                                        │
  │  ┌──────────────────────────────────────────────────────────┐      │
  │  │  7. COMMENT PR con diff de cobertura                     │      │
  │  │  8. BADGE update en README                               │      │
  │  │  9. TREND almacenar histórico                            │      │
  │  │  10. ARTIFACT subir HTML report                          │      │
  │  └──────────────────────────────────────────────────────────┘      │
  │                                                                     │
  └─────────────────────────────────────────────────────────────────────┘
```

### Formatos de salida para CI

```
Formato               Herramienta generadora     Consumidores
──────────────────────────────────────────────────────────────────────────
lcov .info            lcov --capture             genhtml, Codecov, Coveralls
Cobertura XML         gcovr --cobertura-xml      GitLab CI, Jenkins, SonarQube
JUnit XML             ctest --output-junit       GitHub Actions, GitLab CI
HTML                  genhtml, gcovr --html      Artefactos CI, navegador
JSON                  gcovr --json               Scripts custom, APIs
Sonarqube XML         gcovr --sonarqube          SonarQube, SonarCloud
TAP                   Frameworks de test         Jenkins TAP plugin
texto plano           gcovr (default)            Logs de CI
```

---

## 3. Integración con Make: targets de CI

### Makefile diseñado para CI

```makefile
# Makefile — Targets para CI de cobertura

CC           = gcc
CFLAGS       = -Wall -Wextra -std=c11
SRC_DIR      = src
TEST_DIR     = tests
BUILD_DIR    = build
COV_DIR      = coverage_html

# Fuentes
SRCS         = $(wildcard $(SRC_DIR)/*.c)
TEST_SRCS    = $(wildcard $(TEST_DIR)/*.c)
OBJS         = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
TEST_OBJS    = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/%.o)
TEST_BIN     = $(BUILD_DIR)/run_tests

# Umbrales (sobrescribibles desde CI)
LINE_THRESHOLD   ?= 80
BRANCH_THRESHOLD ?= 60
FUNC_THRESHOLD   ?= 90

# ─────────────────────────────────────────
# Targets de desarrollo
# ─────────────────────────────────────────

.PHONY: all test clean

all: $(TEST_BIN)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -c $< -o $@

$(BUILD_DIR)/%.o: $(TEST_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -c $< -o $@

$(TEST_BIN): $(OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

test: $(TEST_BIN)
	./$(TEST_BIN)

# ─────────────────────────────────────────
# Targets de cobertura (CI)
# ─────────────────────────────────────────

# Build con instrumentación
.PHONY: build-coverage
build-coverage: CFLAGS += -O0 -g --coverage
build-coverage: clean $(TEST_BIN)

# Ejecutar tests y capturar cobertura
.PHONY: coverage-capture
coverage-capture: build-coverage
	@echo "=== Running tests ==="
	./$(TEST_BIN)
	@echo ""
	@echo "=== Capturing coverage ==="
	lcov --capture --initial -d $(BUILD_DIR) -o $(BUILD_DIR)/base.info -q
	lcov --capture -d $(BUILD_DIR) -o $(BUILD_DIR)/test.info -q
	lcov -a $(BUILD_DIR)/base.info -a $(BUILD_DIR)/test.info \
		-o $(BUILD_DIR)/total.info -q
	lcov --remove $(BUILD_DIR)/total.info \
		'/usr/*' '*/tests/*' '*/test_*' \
		-o $(BUILD_DIR)/filtered.info -q
	@echo "=== Coverage Summary ==="
	lcov --summary $(BUILD_DIR)/filtered.info

# Generar HTML (para artefactos)
.PHONY: coverage-html
coverage-html: coverage-capture
	genhtml $(BUILD_DIR)/filtered.info \
		-o $(COV_DIR) \
		--title "$(notdir $(CURDIR))" \
		--legend --branch-coverage -q
	@echo "Report: $(COV_DIR)/index.html"

# Generar Cobertura XML (para GitLab/Jenkins)
.PHONY: coverage-xml
coverage-xml: build-coverage
	./$(TEST_BIN)
	gcovr --root . \
		--object-directory $(BUILD_DIR) \
		--exclude '$(TEST_DIR)/.*' \
		--branches \
		--cobertura-xml \
		-o $(BUILD_DIR)/coverage.xml \
		--print-summary

# Verificar umbrales (falla si no se cumplen)
.PHONY: coverage-check
coverage-check: coverage-capture
	@echo ""
	@echo "=== Checking thresholds ==="
	@LINE=$$(lcov --summary $(BUILD_DIR)/filtered.info 2>&1 | \
		grep 'lines' | grep -oP '[\d.]+(?=%)'); \
	BRANCH=$$(lcov --summary $(BUILD_DIR)/filtered.info 2>&1 | \
		grep 'branches' | grep -oP '[\d.]+(?=%)'); \
	FUNC=$$(lcov --summary $(BUILD_DIR)/filtered.info 2>&1 | \
		grep 'functions' | grep -oP '[\d.]+(?=%)'); \
	echo "Lines:     $${LINE}%  (threshold: $(LINE_THRESHOLD)%)"; \
	echo "Branches:  $${BRANCH}%  (threshold: $(BRANCH_THRESHOLD)%)"; \
	echo "Functions: $${FUNC}%  (threshold: $(FUNC_THRESHOLD)%)"; \
	FAIL=0; \
	if [ $$(echo "$${LINE} < $(LINE_THRESHOLD)" | bc -l) -eq 1 ]; then \
		echo "FAIL: Line coverage $${LINE}% < $(LINE_THRESHOLD)%"; \
		FAIL=1; \
	fi; \
	if [ $$(echo "$${BRANCH} < $(BRANCH_THRESHOLD)" | bc -l) -eq 1 ]; then \
		echo "FAIL: Branch coverage $${BRANCH}% < $(BRANCH_THRESHOLD)%"; \
		FAIL=1; \
	fi; \
	if [ $$(echo "$${FUNC} < $(FUNC_THRESHOLD)" | bc -l) -eq 1 ]; then \
		echo "FAIL: Function coverage $${FUNC}% < $(FUNC_THRESHOLD)%"; \
		FAIL=1; \
	fi; \
	if [ $$FAIL -eq 1 ]; then exit 1; fi; \
	echo "ALL THRESHOLDS MET"

# Target principal para CI (combina todo)
.PHONY: ci-coverage
ci-coverage: coverage-check coverage-html
	@echo ""
	@echo "=== CI Coverage Complete ==="

# Verificar umbrales con gcovr (alternativa más simple)
.PHONY: coverage-check-gcovr
coverage-check-gcovr: build-coverage
	./$(TEST_BIN)
	gcovr --root . \
		--object-directory $(BUILD_DIR) \
		--exclude '$(TEST_DIR)/.*' \
		--branches \
		--fail-under-line $(LINE_THRESHOLD) \
		--fail-under-branch $(BRANCH_THRESHOLD) \
		--fail-under-function $(FUNC_THRESHOLD) \
		--print-summary

clean:
	rm -rf $(BUILD_DIR) $(COV_DIR)
```

### Uso desde CI

```bash
# Pipeline completo
make ci-coverage

# Solo verificar umbrales (sin HTML)
make coverage-check

# Con umbrales personalizados
make coverage-check LINE_THRESHOLD=90 BRANCH_THRESHOLD=75

# Alternativa con gcovr (más simple)
make coverage-check-gcovr

# Solo XML para GitLab
make coverage-xml
```

---

## 4. Integración con CMake: targets de CI

### CMakeLists.txt con soporte CI

```cmake
cmake_minimum_required(VERSION 3.16)
project(myproject C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─────────────────────────────────────────
# Opción de cobertura
# ─────────────────────────────────────────
option(ENABLE_COVERAGE "Build with coverage instrumentation" OFF)

if(ENABLE_COVERAGE)
    if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "Coverage requires GCC")
    endif()

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g --coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    message(STATUS "Coverage enabled")
endif()

# ─────────────────────────────────────────
# Biblioteca y tests
# ─────────────────────────────────────────
add_library(mylib
    src/parser.c
    src/validator.c
    src/utils.c
)
target_include_directories(mylib PUBLIC src)

add_executable(run_tests
    tests/test_parser.c
    tests/test_validator.c
    tests/test_utils.c
    tests/test_main.c
)
target_link_libraries(run_tests PRIVATE mylib)

enable_testing()
add_test(NAME unit_tests COMMAND run_tests)

# ─────────────────────────────────────────
# Targets de cobertura (solo si ENABLE_COVERAGE)
# ─────────────────────────────────────────
if(ENABLE_COVERAGE)
    find_program(LCOV lcov)
    find_program(GENHTML genhtml)
    find_program(GCOVR gcovr)

    # Umbrales configurables
    set(COV_LINE_THRESHOLD "80" CACHE STRING "Minimum line coverage %")
    set(COV_BRANCH_THRESHOLD "60" CACHE STRING "Minimum branch coverage %")
    set(COV_FUNC_THRESHOLD "90" CACHE STRING "Minimum function coverage %")

    if(LCOV AND GENHTML)
        # Target: coverage-html (lcov + genhtml)
        add_custom_target(coverage-html
            # Limpiar
            COMMAND ${LCOV} --zerocounters -d ${CMAKE_BINARY_DIR} -q

            # Línea base
            COMMAND ${LCOV} --capture --initial
                -d ${CMAKE_BINARY_DIR}
                -o ${CMAKE_BINARY_DIR}/base.info -q

            # Ejecutar tests
            COMMAND $<TARGET_FILE:run_tests>

            # Capturar
            COMMAND ${LCOV} --capture
                -d ${CMAKE_BINARY_DIR}
                -o ${CMAKE_BINARY_DIR}/test.info -q

            # Combinar
            COMMAND ${LCOV}
                -a ${CMAKE_BINARY_DIR}/base.info
                -a ${CMAKE_BINARY_DIR}/test.info
                -o ${CMAKE_BINARY_DIR}/total.info -q

            # Filtrar
            COMMAND ${LCOV} --remove ${CMAKE_BINARY_DIR}/total.info
                "/usr/*" "*/tests/*" "*/test_*"
                -o ${CMAKE_BINARY_DIR}/filtered.info -q

            # HTML
            COMMAND ${GENHTML} ${CMAKE_BINARY_DIR}/filtered.info
                -o ${CMAKE_BINARY_DIR}/coverage_html
                --title "${PROJECT_NAME}"
                --legend --branch-coverage -q

            # Resumen
            COMMAND ${LCOV} --summary ${CMAKE_BINARY_DIR}/filtered.info

            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS run_tests
            COMMENT "Generating coverage HTML report"
        )
    endif()

    if(GCOVR)
        # Target: coverage-check (gcovr con umbrales)
        add_custom_target(coverage-check
            COMMAND $<TARGET_FILE:run_tests>

            COMMAND ${GCOVR}
                --root ${CMAKE_SOURCE_DIR}
                --object-directory ${CMAKE_BINARY_DIR}
                --exclude "tests/.*"
                --branches
                --fail-under-line ${COV_LINE_THRESHOLD}
                --fail-under-branch ${COV_BRANCH_THRESHOLD}
                --fail-under-function ${COV_FUNC_THRESHOLD}
                --print-summary

            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS run_tests
            COMMENT "Checking coverage thresholds"
        )

        # Target: coverage-xml (Cobertura XML para CI)
        add_custom_target(coverage-xml
            COMMAND $<TARGET_FILE:run_tests>

            COMMAND ${GCOVR}
                --root ${CMAKE_SOURCE_DIR}
                --object-directory ${CMAKE_BINARY_DIR}
                --exclude "tests/.*"
                --branches
                --cobertura-xml
                -o ${CMAKE_BINARY_DIR}/coverage.xml
                --print-summary

            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS run_tests
            COMMENT "Generating Cobertura XML"
        )

        # Target: coverage-json (para APIs y scripting)
        add_custom_target(coverage-json
            COMMAND $<TARGET_FILE:run_tests>

            COMMAND ${GCOVR}
                --root ${CMAKE_SOURCE_DIR}
                --object-directory ${CMAKE_BINARY_DIR}
                --exclude "tests/.*"
                --branches
                --json
                -o ${CMAKE_BINARY_DIR}/coverage.json
                --print-summary

            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS run_tests
            COMMENT "Generating coverage JSON"
        )
    endif()
endif()
```

### Uso desde CI

```bash
# Configurar con cobertura
cmake -B build -DENABLE_COVERAGE=ON

# Build
cmake --build build

# Reporte HTML
cmake --build build --target coverage-html

# Verificar umbrales
cmake --build build --target coverage-check

# Con umbrales custom
cmake -B build -DENABLE_COVERAGE=ON \
    -DCOV_LINE_THRESHOLD=90 \
    -DCOV_BRANCH_THRESHOLD=75

# XML para CI
cmake --build build --target coverage-xml
```

### CMake Presets para cobertura

```json
{
    "version": 6,
    "configurePresets": [
        {
            "name": "default",
            "binaryDir": "build",
            "generator": "Unix Makefiles"
        },
        {
            "name": "coverage",
            "inherits": "default",
            "binaryDir": "build-cov",
            "cacheVariables": {
                "ENABLE_COVERAGE": "ON",
                "CMAKE_BUILD_TYPE": "Debug",
                "COV_LINE_THRESHOLD": "80",
                "COV_BRANCH_THRESHOLD": "60",
                "COV_FUNC_THRESHOLD": "90"
            }
        },
        {
            "name": "coverage-strict",
            "inherits": "coverage",
            "cacheVariables": {
                "COV_LINE_THRESHOLD": "90",
                "COV_BRANCH_THRESHOLD": "75",
                "COV_FUNC_THRESHOLD": "95"
            }
        }
    ],
    "buildPresets": [
        {
            "name": "coverage",
            "configurePreset": "coverage"
        }
    ],
    "testPresets": [
        {
            "name": "coverage",
            "configurePreset": "coverage",
            "output": {"outputOnFailure": true}
        }
    ]
}
```

```bash
# Usar presets
cmake --preset coverage
cmake --build --preset coverage
cmake --build build-cov --target coverage-check
```

---

## 5. GitHub Actions: workflow completo con lcov

### Workflow básico

```yaml
# .github/workflows/coverage.yml
name: Coverage

on:
  push:
    branches: [main, master]
  pull_request:
    branches: [main, master]

jobs:
  coverage:
    runs-on: ubuntu-latest

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y gcc lcov

      - name: Build with coverage
        run: |
          mkdir build && cd build
          gcc -O0 -g --coverage -I../src \
              ../tests/*.c ../src/*.c \
              -o run_tests

      - name: Run tests
        run: cd build && ./run_tests

      - name: Capture coverage
        run: |
          cd build
          lcov --capture --initial -d . -o base.info -q
          lcov --capture -d . -o test.info -q
          lcov -a base.info -a test.info -o total.info -q
          lcov --remove total.info \
              '/usr/*' '*/tests/*' \
              -o filtered.info -q
          lcov --summary filtered.info

      - name: Check thresholds
        run: |
          cd build
          LINE=$(lcov --summary filtered.info 2>&1 | \
              grep 'lines' | grep -oP '[\d.]+(?=%)')
          BRANCH=$(lcov --summary filtered.info 2>&1 | \
              grep 'branches' | grep -oP '[\d.]+(?=%)')

          echo "Line coverage: ${LINE}%"
          echo "Branch coverage: ${BRANCH}%"

          # Umbrales
          if (( $(echo "$LINE < 80" | bc -l) )); then
            echo "::error::Line coverage ${LINE}% is below 80%"
            exit 1
          fi

          if (( $(echo "$BRANCH < 60" | bc -l) )); then
            echo "::error::Branch coverage ${BRANCH}% is below 60%"
            exit 1
          fi

          echo "Coverage thresholds met"

      - name: Generate HTML report
        if: always()
        run: |
          cd build
          genhtml filtered.info \
              -o coverage_html \
              --title "${{ github.repository }}" \
              --legend --branch-coverage -q

      - name: Upload coverage report
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: coverage-report
          path: build/coverage_html/
          retention-days: 30

      - name: Upload coverage to Codecov
        if: github.event_name == 'push'
        uses: codecov/codecov-action@v4
        with:
          files: build/filtered.info
          fail_ci_if_error: false
          token: ${{ secrets.CODECOV_TOKEN }}
```

### Workflow con Make

```yaml
# .github/workflows/coverage-make.yml
name: Coverage (Make)

on:
  push:
    branches: [main]
  pull_request:

jobs:
  coverage:
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: sudo apt-get update && sudo apt-get install -y gcc lcov

      - name: Run coverage pipeline
        run: make ci-coverage

      - name: Upload HTML report
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: coverage-html
          path: coverage_html/
```

### Workflow con CMake

```yaml
# .github/workflows/coverage-cmake.yml
name: Coverage (CMake)

on:
  push:
    branches: [main]
  pull_request:

jobs:
  coverage:
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y gcc lcov cmake
          pip install gcovr

      - name: Configure
        run: cmake -B build -DENABLE_COVERAGE=ON

      - name: Build
        run: cmake --build build

      - name: Check thresholds
        run: cmake --build build --target coverage-check

      - name: Generate HTML
        if: always()
        run: cmake --build build --target coverage-html

      - name: Upload report
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: coverage-report
          path: build/coverage_html/
```

---

## 6. GitHub Actions: workflow con gcovr

gcovr simplifica el pipeline porque combina captura, filtrado y reporte en un solo comando.

### Workflow completo con gcovr

```yaml
# .github/workflows/coverage-gcovr.yml
name: Coverage (gcovr)

on:
  push:
    branches: [main]
  pull_request:

jobs:
  coverage:
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y gcc
          pip install gcovr

      - name: Build with coverage
        run: |
          mkdir build
          gcc -O0 -g --coverage -Isrc \
              tests/*.c src/*.c \
              -o build/run_tests

      - name: Run tests
        run: ./build/run_tests

      - name: Coverage summary
        run: |
          gcovr --root . \
              --object-directory build \
              --exclude 'tests/.*' \
              --branches \
              --print-summary

      - name: Check thresholds
        run: |
          gcovr --root . \
              --object-directory build \
              --exclude 'tests/.*' \
              --branches \
              --fail-under-line 80 \
              --fail-under-branch 60 \
              --fail-under-function 90

      - name: Generate HTML report
        if: always()
        run: |
          gcovr --root . \
              --object-directory build \
              --exclude 'tests/.*' \
              --branches \
              --html-details \
              -o build/coverage.html

      - name: Generate Cobertura XML
        if: always()
        run: |
          gcovr --root . \
              --object-directory build \
              --exclude 'tests/.*' \
              --branches \
              --cobertura-xml \
              -o build/coverage.xml

      - name: Upload HTML report
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: coverage-html
          path: build/coverage*.html
          retention-days: 30

      - name: Upload Cobertura XML
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: coverage-xml
          path: build/coverage.xml
```

### gcovr con multiple formatos en un solo paso

```yaml
      - name: Generate all reports
        if: always()
        run: |
          gcovr --root . \
              --object-directory build \
              --exclude 'tests/.*' \
              --branches \
              --html-details -o build/coverage.html \
              --cobertura-xml -o build/coverage.xml \
              --json -o build/coverage.json \
              --print-summary
```

---

## 7. GitLab CI: pipeline completo

### .gitlab-ci.yml con cobertura

```yaml
# .gitlab-ci.yml
stages:
  - build
  - test
  - coverage
  - deploy

variables:
  COV_LINE_THRESHOLD: "80"
  COV_BRANCH_THRESHOLD: "60"

# ─────────────────────────────────────────
# Build con cobertura
# ─────────────────────────────────────────
build:coverage:
  stage: build
  image: gcc:latest
  before_script:
    - apt-get update && apt-get install -y lcov
    - pip install gcovr || true
  script:
    - mkdir -p build
    - gcc -O0 -g --coverage -Isrc
          tests/*.c src/*.c
          -o build/run_tests
  artifacts:
    paths:
      - build/
    expire_in: 1 hour

# ─────────────────────────────────────────
# Tests + captura de cobertura
# ─────────────────────────────────────────
test:coverage:
  stage: test
  image: gcc:latest
  needs: [build:coverage]
  before_script:
    - apt-get update && apt-get install -y lcov
    - pip install gcovr
  script:
    - cd build && ./run_tests

    # Generar Cobertura XML (GitLab lo parsea nativamente)
    - gcovr --root ${CI_PROJECT_DIR}
            --object-directory ${CI_PROJECT_DIR}/build
            --exclude 'tests/.*'
            --branches
            --cobertura-xml
            -o coverage.xml
            --print-summary

  # GitLab parsea el XML para mostrar cobertura en MR
  artifacts:
    reports:
      coverage_report:
        coverage_format: cobertura
        path: build/coverage.xml
    paths:
      - build/
    expire_in: 1 week

  # Regex para extraer % de cobertura del log
  coverage: '/lines\.*:\s*(\d+\.\d+)%/'

# ─────────────────────────────────────────
# Verificar umbrales
# ─────────────────────────────────────────
check:thresholds:
  stage: coverage
  image: python:3-slim
  needs: [test:coverage]
  before_script:
    - pip install gcovr
  script:
    - gcovr --root ${CI_PROJECT_DIR}
            --object-directory ${CI_PROJECT_DIR}/build
            --exclude 'tests/.*'
            --branches
            --fail-under-line ${COV_LINE_THRESHOLD}
            --fail-under-branch ${COV_BRANCH_THRESHOLD}
            --print-summary

# ─────────────────────────────────────────
# Reporte HTML (solo en main)
# ─────────────────────────────────────────
pages:
  stage: deploy
  image: gcc:latest
  needs: [test:coverage]
  only:
    - main
  before_script:
    - apt-get update && apt-get install -y lcov
  script:
    - cd build
    - lcov --capture -d . -o coverage.info -q
    - lcov --remove coverage.info '/usr/*' '*/tests/*' -o filtered.info -q
    - genhtml filtered.info -o ${CI_PROJECT_DIR}/public
          --title "${CI_PROJECT_NAME}"
          --legend --branch-coverage -q
  artifacts:
    paths:
      - public
```

### Cobertura en Merge Requests de GitLab

GitLab tiene soporte nativo para mostrar cobertura en Merge Requests cuando usas el formato Cobertura XML:

```
  ┌─────────────────────────────────────────────────────────────────────┐
  │  Merge Request !42: Add user validation                            │
  │                                                                     │
  │  Coverage: 84.2%  (+2.3% from main)                                │
  │                                                                     │
  │  ┌─────────────────────────────────────────────────────────────┐   │
  │  │  Changed files:                                             │   │
  │  │                                                             │   │
  │  │  src/validator.c                                            │   │
  │  │   15  │ bool validate_email(const char *email) {            │   │
  │  │   16  █ █  if (!email) return false;          ← cubierta   │   │
  │  │   17  █ █  size_t len = strlen(email);        ← cubierta   │   │
  │  │   18  ░ ░  if (len > 254) return false;       ← NO cubierta│   │
  │  │                                                             │   │
  │  └─────────────────────────────────────────────────────────────┘   │
  └─────────────────────────────────────────────────────────────────────┘
```

---

## 8. Umbrales mínimos: fail on drop

### Estrategia de umbrales fijos

La forma más simple: definir un porcentaje mínimo y fallar si baja.

```
Umbral fijo:
  if (coverage < THRESHOLD) → CI FALLA

Ventaja: Simple, predecible
Desventaja: El equipo puede conformarse con el mínimo
```

### Implementación con lcov

```bash
#!/bin/bash
# scripts/check_coverage.sh
set -euo pipefail

LINE_MIN=${1:-80}
BRANCH_MIN=${2:-60}
FUNC_MIN=${3:-90}
INFO_FILE=${4:-build/filtered.info}

# Extraer porcentajes
LINE=$(lcov --summary "$INFO_FILE" 2>&1 | \
    grep 'lines' | grep -oP '[\d.]+(?=%)')
BRANCH=$(lcov --summary "$INFO_FILE" 2>&1 | \
    grep 'branches' | grep -oP '[\d.]+(?=%)')
FUNC=$(lcov --summary "$INFO_FILE" 2>&1 | \
    grep 'functions' | grep -oP '[\d.]+(?=%)')

echo "Coverage Results:"
echo "  Lines:     ${LINE}%  (min: ${LINE_MIN}%)"
echo "  Branches:  ${BRANCH}%  (min: ${BRANCH_MIN}%)"
echo "  Functions: ${FUNC}%  (min: ${FUNC_MIN}%)"
echo ""

FAILED=0

check() {
    local name=$1 actual=$2 min=$3
    if (( $(echo "$actual < $min" | bc -l) )); then
        echo "FAIL: $name coverage ${actual}% < ${min}%"
        FAILED=1
    else
        echo "PASS: $name coverage ${actual}% >= ${min}%"
    fi
}

check "Line"     "$LINE"   "$LINE_MIN"
check "Branch"   "$BRANCH" "$BRANCH_MIN"
check "Function" "$FUNC"   "$FUNC_MIN"

echo ""
if [ $FAILED -eq 1 ]; then
    echo "Coverage check FAILED"
    exit 1
else
    echo "Coverage check PASSED"
fi
```

### Implementación con gcovr

```bash
# Todo en un comando:
gcovr --root . \
    --object-directory build \
    --exclude 'tests/.*' \
    --branches \
    --fail-under-line 80 \
    --fail-under-branch 60 \
    --fail-under-function 90 \
    --print-summary

# Si cualquier umbral no se cumple, gcovr retorna exit code != 0
```

### Umbrales por módulo

No todo el código necesita el mismo umbral:

```bash
#!/bin/bash
# scripts/check_coverage_by_module.sh

# Módulo de seguridad: umbral estricto
gcovr --root . --object-directory build \
    --filter 'src/auth/.*' --filter 'src/crypto/.*' \
    --branches \
    --fail-under-line 95 \
    --fail-under-branch 85 \
    --print-summary
echo "=== Auth/Crypto: OK ==="

# Módulo de lógica de negocio: umbral normal
gcovr --root . --object-directory build \
    --filter 'src/business/.*' \
    --branches \
    --fail-under-line 80 \
    --fail-under-branch 60 \
    --print-summary
echo "=== Business: OK ==="

# Utilidades: umbral relajado
gcovr --root . --object-directory build \
    --filter 'src/utils/.*' \
    --branches \
    --fail-under-line 60 \
    --fail-under-branch 40 \
    --print-summary
echo "=== Utils: OK ==="
```

```
  Umbrales diferenciados:

  ┌──────────────────┬──────────┬──────────┐
  │  Módulo          │  Lines   │  Branch  │
  ├──────────────────┼──────────┼──────────┤
  │  auth / crypto   │  95%     │  85%     │
  │  business logic  │  80%     │  60%     │
  │  utils / helpers │  60%     │  40%     │
  │  debug / logging │  N/A     │  N/A     │
  └──────────────────┴──────────┴──────────┘

  Justificación:
  - auth/crypto: bugs = vulnerabilidades de seguridad
  - business logic: bugs = resultados incorrectos para el usuario
  - utils: bugs = inconvenientes, raramente críticos
  - debug: no merece inversión en cobertura
```

---

## 9. Coverage ratchet: el trinquete monotónico

### Concepto

El ratchet (trinquete) es un mecanismo que permite girar en una sola dirección. Aplicado a cobertura: **la cobertura solo puede subir, nunca bajar**.

```
  Sin ratchet:                    Con ratchet:

  85%  ■                          85%  ■
       │ ■                             │ ✗ bloqueado
  78%  │   ■                      84%  │   ■
       │     ■                         │     ■
  82%  │       ■                  86%  │       ■
       │                               │
  Permite fluctuaciones           Solo permite subir
```

### Implementación

```bash
#!/bin/bash
# scripts/coverage_ratchet.sh
#
# Uso: ./scripts/coverage_ratchet.sh build/filtered.info
#
# Almacena el umbral en .coverage_threshold (commitear al repo)

set -euo pipefail

INFO_FILE=${1:-build/filtered.info}
THRESHOLD_FILE=".coverage_threshold"

# Obtener cobertura actual
CURRENT_LINE=$(lcov --summary "$INFO_FILE" 2>&1 | \
    grep 'lines' | grep -oP '[\d.]+(?=%)')
CURRENT_BRANCH=$(lcov --summary "$INFO_FILE" 2>&1 | \
    grep 'branches' | grep -oP '[\d.]+(?=%)')

echo "Current:   lines=${CURRENT_LINE}%  branches=${CURRENT_BRANCH}%"

# Leer umbrales anteriores
if [ -f "$THRESHOLD_FILE" ]; then
    PREV_LINE=$(grep 'line=' "$THRESHOLD_FILE" | cut -d= -f2)
    PREV_BRANCH=$(grep 'branch=' "$THRESHOLD_FILE" | cut -d= -f2)
    echo "Threshold: lines=${PREV_LINE}%  branches=${PREV_BRANCH}%"
else
    PREV_LINE="0"
    PREV_BRANCH="0"
    echo "Threshold: (none, first run)"
fi

# Verificar que no bajó
FAILED=0

if (( $(echo "$CURRENT_LINE < $PREV_LINE" | bc -l) )); then
    echo "FAIL: Line coverage dropped from ${PREV_LINE}% to ${CURRENT_LINE}%"
    FAILED=1
fi

if (( $(echo "$CURRENT_BRANCH < $PREV_BRANCH" | bc -l) )); then
    echo "FAIL: Branch coverage dropped from ${PREV_BRANCH}% to ${CURRENT_BRANCH}%"
    FAILED=1
fi

if [ $FAILED -eq 1 ]; then
    echo ""
    echo "Coverage regression detected. Add tests before merging."
    exit 1
fi

# Actualizar umbrales si subieron
UPDATED=0

if (( $(echo "$CURRENT_LINE > $PREV_LINE" | bc -l) )); then
    echo "Line coverage increased: ${PREV_LINE}% → ${CURRENT_LINE}%"
    UPDATED=1
fi

if (( $(echo "$CURRENT_BRANCH > $PREV_BRANCH" | bc -l) )); then
    echo "Branch coverage increased: ${PREV_BRANCH}% → ${CURRENT_BRANCH}%"
    UPDATED=1
fi

# Escribir nuevo threshold
cat > "$THRESHOLD_FILE" << EOF
line=${CURRENT_LINE}
branch=${CURRENT_BRANCH}
updated=$(date -Iseconds)
EOF

if [ $UPDATED -eq 1 ]; then
    echo "Threshold file updated"
else
    echo "Coverage stable"
fi

echo "PASS"
```

### Archivo .coverage_threshold

```
line=84.2
branch=67.5
updated=2026-04-05T14:30:00+00:00
```

Este archivo se commitea al repositorio. Cada PR que sube la cobertura actualiza el archivo, y el siguiente PR no puede bajar por debajo del nuevo umbral.

### GitHub Actions con ratchet

```yaml
      - name: Coverage ratchet
        run: |
          ./scripts/coverage_ratchet.sh build/filtered.info

      - name: Commit updated threshold
        if: github.event_name == 'push' && github.ref == 'refs/heads/main'
        run: |
          if git diff --quiet .coverage_threshold; then
            echo "No threshold change"
          else
            git config user.name "github-actions[bot]"
            git config user.email "github-actions[bot]@users.noreply.github.com"
            git add .coverage_threshold
            git commit -m "ci: update coverage threshold"
            git push
          fi
```

### Tolerancia en el ratchet

Un ratchet estricto puede ser frustrante cuando un refactor legítimo baja la cobertura (por ejemplo, eliminar código muerto cambia los denominadores). Solución: permitir una tolerancia:

```bash
# Permitir caída de hasta 0.5% sin fallar
TOLERANCE="0.5"
ADJUSTED_PREV=$(echo "$PREV_LINE - $TOLERANCE" | bc -l)

if (( $(echo "$CURRENT_LINE < $ADJUSTED_PREV" | bc -l) )); then
    echo "FAIL: Coverage dropped more than ${TOLERANCE}%"
    exit 1
fi
```

---

## 10. Trend tracking: visualizar la evolución

### Almacenamiento de histórico

```bash
#!/bin/bash
# scripts/record_coverage.sh
# Guardar datos de cobertura en un archivo CSV

HISTORY_FILE="coverage_history.csv"
INFO_FILE=${1:-build/filtered.info}

# Crear header si no existe
if [ ! -f "$HISTORY_FILE" ]; then
    echo "date,commit,branch,lines_pct,lines_hit,lines_total,branches_pct,branches_hit,branches_total,functions_pct" \
        > "$HISTORY_FILE"
fi

# Extraer datos
DATE=$(date -Iseconds)
COMMIT=$(git rev-parse --short HEAD)
BRANCH=$(git branch --show-current)

LINE_PCT=$(lcov --summary "$INFO_FILE" 2>&1 | \
    grep 'lines' | grep -oP '[\d.]+(?=%)')
LINE_DETAIL=$(lcov --summary "$INFO_FILE" 2>&1 | \
    grep 'lines' | grep -oP '\d+ of \d+')
LINE_HIT=$(echo "$LINE_DETAIL" | cut -d' ' -f1)
LINE_TOTAL=$(echo "$LINE_DETAIL" | cut -d' ' -f3)

BRANCH_PCT=$(lcov --summary "$INFO_FILE" 2>&1 | \
    grep 'branches' | grep -oP '[\d.]+(?=%)')
BRANCH_DETAIL=$(lcov --summary "$INFO_FILE" 2>&1 | \
    grep 'branches' | grep -oP '\d+ of \d+')
BRANCH_HIT=$(echo "$BRANCH_DETAIL" | cut -d' ' -f1)
BRANCH_TOTAL=$(echo "$BRANCH_DETAIL" | cut -d' ' -f3)

FUNC_PCT=$(lcov --summary "$INFO_FILE" 2>&1 | \
    grep 'functions' | grep -oP '[\d.]+(?=%)')

# Append
echo "${DATE},${COMMIT},${BRANCH},${LINE_PCT},${LINE_HIT},${LINE_TOTAL},${BRANCH_PCT},${BRANCH_HIT},${BRANCH_TOTAL},${FUNC_PCT}" \
    >> "$HISTORY_FILE"

echo "Recorded: lines=${LINE_PCT}% branches=${BRANCH_PCT}% functions=${FUNC_PCT}%"
```

### Formato del CSV

```csv
date,commit,branch,lines_pct,lines_hit,lines_total,branches_pct,branches_hit,branches_total,functions_pct
2026-03-01T10:00:00+00:00,a1b2c3d,main,72.5,145,200,58.3,70,120,85.0
2026-03-08T10:00:00+00:00,e4f5g6h,main,75.0,157,210,60.1,73,122,87.5
2026-03-15T10:00:00+00:00,i7j8k9l,main,78.2,172,220,63.7,84,132,90.0
2026-03-22T10:00:00+00:00,m0n1o2p,main,80.1,185,231,65.2,89,137,90.0
2026-03-29T10:00:00+00:00,q3r4s5t,main,82.3,198,241,68.0,95,140,92.5
2026-04-05T10:00:00+00:00,u6v7w8x,main,84.2,210,250,70.3,102,145,92.5
```

### Visualización en terminal

```bash
#!/bin/bash
# scripts/coverage_trend.sh
# Mostrar tendencia de las últimas N entradas

HISTORY_FILE="coverage_history.csv"
N=${1:-10}

echo "=== Coverage Trend (last $N entries) ==="
echo ""

# Header
printf "%-12s %-8s %8s %8s %8s\n" "Date" "Commit" "Lines" "Branch" "Func"
printf "%-12s %-8s %8s %8s %8s\n" "────────────" "────────" "────────" "────────" "────────"

# Data (skip header, take last N)
tail -n +2 "$HISTORY_FILE" | tail -n "$N" | while IFS=',' read -r date commit branch lpct lhit ltot bpct bhit btot fpct; do
    short_date=$(echo "$date" | cut -dT -f1)
    printf "%-12s %-8s %7s%% %7s%% %7s%%\n" "$short_date" "$commit" "$lpct" "$bpct" "$fpct"
done

echo ""

# Gráfico ASCII simple
echo "Lines coverage:"
tail -n +2 "$HISTORY_FILE" | tail -n "$N" | while IFS=',' read -r date commit branch lpct rest; do
    short_date=$(echo "$date" | cut -dT -f1)
    bars=$(printf "%0.s█" $(seq 1 $(echo "$lpct / 2" | bc)))
    printf "  %s │%s %s%%\n" "$short_date" "$bars" "$lpct"
done
```

Salida:

```
=== Coverage Trend (last 6 entries) ===

Date         Commit     Lines  Branch     Func
──────────── ──────── ──────── ──────── ────────
2026-03-01   a1b2c3d   72.5%   58.3%   85.0%
2026-03-08   e4f5g6h   75.0%   60.1%   87.5%
2026-03-15   i7j8k9l   78.2%   63.7%   90.0%
2026-03-22   m0n1o2p   80.1%   65.2%   90.0%
2026-03-29   q3r4s5t   82.3%   68.0%   92.5%
2026-04-05   u6v7w8x   84.2%   70.3%   92.5%

Lines coverage:
  2026-03-01 │████████████████████████████████████ 72.5%
  2026-03-08 │█████████████████████████████████████ 75.0%
  2026-03-15 │███████████████████████████████████████ 78.2%
  2026-03-22 │████████████████████████████████████████ 80.1%
  2026-03-29 │█████████████████████████████████████████ 82.3%
  2026-04-05 │██████████████████████████████████████████ 84.2%
```

### GitHub Actions: almacenar histórico como artefacto

```yaml
      - name: Record coverage history
        if: github.event_name == 'push' && github.ref == 'refs/heads/main'
        run: ./scripts/record_coverage.sh build/filtered.info

      - name: Show trend
        if: github.event_name == 'push' && github.ref == 'refs/heads/main'
        run: ./scripts/coverage_trend.sh 10

      - name: Commit history
        if: github.event_name == 'push' && github.ref == 'refs/heads/main'
        run: |
          git add coverage_history.csv
          if ! git diff --cached --quiet; then
            git config user.name "github-actions[bot]"
            git config user.email "github-actions[bot]@users.noreply.github.com"
            git commit -m "ci: update coverage history"
            git push
          fi
```

---

## 11. Servicios externos: Codecov y Coveralls

### Codecov

Codecov es el servicio más popular para visualizar cobertura de código en proyectos open source.

#### Configuración

```yaml
# codecov.yml (raíz del repo)
codecov:
  require_ci_to_pass: true

coverage:
  precision: 2
  round: down
  range: "70...95"

  status:
    project:
      default:
        target: auto     # Comparar contra parent commit
        threshold: 1%    # Permitir caída de hasta 1%
    patch:
      default:
        target: 80%      # Código nuevo debe tener >= 80%

comment:
  layout: "reach,diff,flags,files"
  behavior: default
  require_changes: true

flags:
  unit:
    paths:
      - src/
    carryforward: true
  integration:
    paths:
      - src/
    carryforward: true
```

#### GitHub Actions con Codecov

```yaml
      - name: Upload to Codecov
        uses: codecov/codecov-action@v4
        with:
          files: build/filtered.info
          flags: unit
          name: unit-tests
          token: ${{ secrets.CODECOV_TOKEN }}
          fail_ci_if_error: true
```

#### Qué muestra Codecov en un PR

```
  ┌─────────────────────────────────────────────────────────────────────┐
  │  Codecov Report                                                     │
  │                                                                     │
  │  Merging #42 (feat/auth) into main will increase coverage by 2.3%. │
  │                                                                     │
  │  @@           Coverage Diff            @@                           │
  │  ##             main      #42    +/-   ##                           │
  │  ═════════════════════════════════════════                          │
  │    Coverage     81.2%    83.5%   +2.3%                             │
  │    Lines          240      268    +28                               │
  │    Branches       120      138    +18                               │
  │  ═════════════════════════════════════════                          │
  │                                                                     │
  │  Files changed (3):                                                 │
  │                                                                     │
  │  src/auth.c        85.7%  ████████████████░░░░  (new file)         │
  │  src/validator.c   92.0%  ██████████████████░░  (+4.0%)            │
  │  src/utils.c       78.3%  ███████████████░░░░░  (-1.2%)            │
  │                                                                     │
  │  Patch Coverage: 91.3% of 23 new lines covered                     │
  └─────────────────────────────────────────────────────────────────────┘
```

### Coveralls

Alternativa a Codecov, especialmente popular en proyectos Ruby/JS pero funciona con C.

#### GitHub Actions con Coveralls

```yaml
      - name: Upload to Coveralls
        uses: coverallsapp/github-action@v2
        with:
          github-token: ${{ secrets.GITHUB_TOKEN }}
          file: build/filtered.info
          format: lcov
```

### Comparación Codecov vs Coveralls

```
Característica          Codecov                    Coveralls
──────────────────────────────────────────────────────────────────
Gratuito para OSS       Sí                         Sí
PR comments             Sí (detallado)             Sí (básico)
Patch coverage          Sí                         Limitado
Flags (unit/integ)      Sí                         No
Branch comparison       Sí                         Sí
Trend graphs            Sí (excelentes)            Sí (básicos)
Formatos soportados     lcov, Cobertura, gcov,     lcov, Cobertura
                        JSON, y más
Self-hosted             Sí (Codecov Enterprise)    No
Sin servicio externo    No                         No
```

### Sin servicio externo: cobertura local en CI

Si no quieres depender de servicios externos:

```yaml
      # Publicar como GitHub Pages
      - name: Deploy coverage to GitHub Pages
        if: github.ref == 'refs/heads/main'
        uses: peaceiris/actions-gh-pages@v4
        with:
          github_token: ${{ secrets.GITHUB_TOKEN }}
          publish_dir: build/coverage_html
          destination_dir: coverage
```

Resultado: reporte HTML accesible en `https://usuario.github.io/repo/coverage/`

---

## 12. Badges de cobertura en el README

### Con Codecov

```markdown
[![codecov](https://codecov.io/gh/OWNER/REPO/branch/main/graph/badge.svg)](https://codecov.io/gh/OWNER/REPO)
```

### Con Coveralls

```markdown
[![Coverage Status](https://coveralls.io/repos/github/OWNER/REPO/badge.svg?branch=main)](https://coveralls.io/github/OWNER/REPO?branch=main)
```

### Badge auto-generado sin servicio externo

Puedes generar badges usando shields.io con datos de tu CI:

```bash
#!/bin/bash
# scripts/generate_badge.sh
# Genera un badge SVG dinámico usando shields.io

INFO_FILE=${1:-build/filtered.info}
BADGE_FILE=${2:-coverage_badge.svg}

LINE=$(lcov --summary "$INFO_FILE" 2>&1 | \
    grep 'lines' | grep -oP '[\d.]+(?=%)')

# Determinar color
COLOR="red"
if (( $(echo "$LINE >= 90" | bc -l) )); then
    COLOR="brightgreen"
elif (( $(echo "$LINE >= 80" | bc -l) )); then
    COLOR="green"
elif (( $(echo "$LINE >= 70" | bc -l) )); then
    COLOR="yellowgreen"
elif (( $(echo "$LINE >= 60" | bc -l) )); then
    COLOR="yellow"
elif (( $(echo "$LINE >= 50" | bc -l) )); then
    COLOR="orange"
fi

# Descargar badge
curl -s "https://img.shields.io/badge/coverage-${LINE}%25-${COLOR}" \
    -o "$BADGE_FILE"

echo "Badge generated: ${BADGE_FILE} (${LINE}%, ${COLOR})"
```

```yaml
      # En GitHub Actions
      - name: Generate coverage badge
        run: ./scripts/generate_badge.sh build/filtered.info coverage_badge.svg

      - name: Upload badge
        uses: actions/upload-artifact@v4
        with:
          name: coverage-badge
          path: coverage_badge.svg
```

### Badge con gist (persistente sin servicio)

```yaml
      - name: Update coverage badge via gist
        if: github.ref == 'refs/heads/main'
        uses: schneegans/dynamic-badges-action@v1.7.0
        with:
          auth: ${{ secrets.GIST_TOKEN }}
          gistID: YOUR_GIST_ID
          filename: coverage.json
          label: coverage
          message: "${{ env.COVERAGE_PCT }}%"
          valColorRange: ${{ env.COVERAGE_PCT }}
          minColorRange: 50
          maxColorRange: 95
```

En el README:

```markdown
![Coverage](https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/USER/GIST_ID/raw/coverage.json)
```

---

## 13. Cobertura en Pull Requests: comentarios automáticos

### Comentario con diff de cobertura

```yaml
# .github/workflows/pr-coverage.yml
name: PR Coverage Comment

on:
  pull_request:

jobs:
  coverage:
    runs-on: ubuntu-latest
    permissions:
      pull-requests: write

    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0  # Necesario para comparar con base

      - name: Install dependencies
        run: |
          sudo apt-get update && sudo apt-get install -y gcc lcov
          pip install gcovr

      - name: Build and test (PR branch)
        run: |
          mkdir build
          gcc -O0 -g --coverage -Isrc tests/*.c src/*.c -o build/run_tests
          ./build/run_tests

      - name: Capture coverage (PR branch)
        run: |
          gcovr --root . --object-directory build \
              --exclude 'tests/.*' --branches \
              --json -o build/pr_coverage.json \
              --print-summary 2>&1 | tee build/pr_summary.txt

      - name: Get base branch coverage
        run: |
          # Checkout base branch
          git stash || true
          git checkout ${{ github.event.pull_request.base.ref }}

          mkdir build-base
          gcc -O0 -g --coverage -Isrc tests/*.c src/*.c -o build-base/run_tests 2>/dev/null || true
          ./build-base/run_tests 2>/dev/null || true

          gcovr --root . --object-directory build-base \
              --exclude 'tests/.*' --branches \
              --json -o build/base_coverage.json \
              --print-summary 2>&1 | tee build/base_summary.txt || true

          # Volver al PR branch
          git checkout -
          git stash pop || true

      - name: Generate diff comment
        id: coverage-diff
        run: |
          # Extraer porcentajes
          PR_LINE=$(grep 'line' build/pr_summary.txt | head -1 | grep -oP '[\d.]+(?=%)' || echo "0")
          BASE_LINE=$(grep 'line' build/base_summary.txt | head -1 | grep -oP '[\d.]+(?=%)' || echo "0")

          PR_BRANCH=$(grep 'branch' build/pr_summary.txt | head -1 | grep -oP '[\d.]+(?=%)' || echo "0")
          BASE_BRANCH=$(grep 'branch' build/base_summary.txt | head -1 | grep -oP '[\d.]+(?=%)' || echo "0")

          DIFF_LINE=$(echo "$PR_LINE - $BASE_LINE" | bc -l)
          DIFF_BRANCH=$(echo "$PR_BRANCH - $BASE_BRANCH" | bc -l)

          # Formatear signo
          if (( $(echo "$DIFF_LINE >= 0" | bc -l) )); then
            DIFF_LINE_FMT="+${DIFF_LINE}"
          else
            DIFF_LINE_FMT="${DIFF_LINE}"
          fi

          if (( $(echo "$DIFF_BRANCH >= 0" | bc -l) )); then
            DIFF_BRANCH_FMT="+${DIFF_BRANCH}"
          else
            DIFF_BRANCH_FMT="${DIFF_BRANCH}"
          fi

          # Generar comentario
          COMMENT="## Coverage Report

          | Metric | Base | PR | Diff |
          |--------|------|-----|------|
          | Lines | ${BASE_LINE}% | ${PR_LINE}% | ${DIFF_LINE_FMT}% |
          | Branches | ${BASE_BRANCH}% | ${PR_BRANCH}% | ${DIFF_BRANCH_FMT}% |"

          echo "comment<<EOF" >> $GITHUB_OUTPUT
          echo "$COMMENT" >> $GITHUB_OUTPUT
          echo "EOF" >> $GITHUB_OUTPUT

      - name: Post comment
        uses: marocchino/sticky-pull-request-comment@v2
        with:
          message: ${{ steps.coverage-diff.outputs.comment }}
```

### Resultado en el PR

```
  ┌─────────────────────────────────────────────────────────────────┐
  │  github-actions[bot]                                            │
  │                                                                 │
  │  ## Coverage Report                                             │
  │                                                                 │
  │  | Metric   | Base   | PR     | Diff    |                      │
  │  |----------|--------|--------|---------|                      │
  │  | Lines    | 81.2%  | 83.5%  | +2.3%   |                      │
  │  | Branches | 67.5%  | 70.1%  | +2.6%   |                      │
  │                                                                 │
  └─────────────────────────────────────────────────────────────────┘
```

---

## 14. Artefactos: almacenar reportes HTML

### GitHub Actions: artefactos con retención

```yaml
      - name: Generate HTML report
        if: always()  # Generar incluso si los tests fallaron
        run: |
          cd build
          genhtml filtered.info \
              -o coverage_html \
              --title "Coverage — ${{ github.sha }}" \
              --legend --branch-coverage -q

      - name: Upload HTML report
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: coverage-report-${{ github.sha }}
          path: build/coverage_html/
          retention-days: 30

      - name: Upload lcov data
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: coverage-data-${{ github.sha }}
          path: build/filtered.info
          retention-days: 90
```

### GitHub Pages: reporte siempre disponible

```yaml
  # Job separado que solo corre en main
  deploy-coverage:
    needs: coverage
    if: github.ref == 'refs/heads/main'
    runs-on: ubuntu-latest
    permissions:
      pages: write
      id-token: write

    environment:
      name: github-pages
      url: ${{ steps.deployment.outputs.page_url }}

    steps:
      - name: Download coverage report
        uses: actions/download-artifact@v4
        with:
          name: coverage-report-${{ github.sha }}
          path: coverage_html

      - name: Upload Pages artifact
        uses: actions/upload-pages-artifact@v3
        with:
          path: coverage_html

      - name: Deploy to GitHub Pages
        id: deployment
        uses: actions/deploy-pages@v4
```

### GitLab: Pages automático

```yaml
# Ya mostrado en sección 7 — el artefacto en public/ se publica automáticamente
pages:
  artifacts:
    paths:
      - public   # genhtml output aquí → publicado como GitLab Pages
```

---

## 15. Cobertura diferencial: solo lo que cambió

### El problema

El porcentaje global puede ser engañoso en un PR:

```
  Proyecto con 10,000 líneas, 80% de cobertura (8,000 cubiertas)
  PR añade 100 líneas nuevas, 0 con tests

  Cobertura nueva: 8,000 / 10,100 = 79.2%
  "Solo bajó 0.8%, está bien"

  Pero: ¡el código NUEVO tiene 0% de cobertura!
```

### Cobertura de patch (solo código nuevo/modificado)

```bash
#!/bin/bash
# scripts/patch_coverage.sh
# Calcular cobertura solo de las líneas cambiadas en el PR

BASE_BRANCH=${1:-main}
INFO_FILE=${2:-build/filtered.info}

# Obtener líneas modificadas (solo adiciones)
git diff "$BASE_BRANCH"...HEAD --unified=0 --no-color | \
    grep -E '^\+\+\+ b/' | \
    sed 's/+++ b\///' | \
    while read -r file; do
        # Solo archivos .c del src/
        if [[ "$file" == src/*.c ]]; then
            # Obtener números de línea añadidos
            git diff "$BASE_BRANCH"...HEAD --unified=0 --no-color -- "$file" | \
                grep -E '^@@' | \
                grep -oP '\+\K\d+(,\d+)?' | \
                while read -r range; do
                    start=$(echo "$range" | cut -d, -f1)
                    count=$(echo "$range" | cut -d, -f2)
                    count=${count:-1}

                    echo "$file:$start:$count"
                done
        fi
    done > /tmp/changed_lines.txt

if [ ! -s /tmp/changed_lines.txt ]; then
    echo "No source lines changed"
    exit 0
fi

echo "=== Patch Coverage Analysis ==="
echo ""

TOTAL_NEW=0
TOTAL_COVERED=0

while IFS=: read -r file start count; do
    # Generar .gcov para este archivo
    gcov -o build "$file" > /dev/null 2>&1

    gcov_file="${file##*/}.gcov"
    if [ ! -f "$gcov_file" ]; then continue; fi

    for i in $(seq "$start" $((start + count - 1))); do
        line_data=$(sed -n "${i}p" "$gcov_file" 2>/dev/null || true)
        # Extraer contador
        counter=$(echo "$line_data" | awk '{print $1}' | tr -d ':')

        if [ "$counter" = "-" ]; then
            continue  # No ejecutable
        fi

        TOTAL_NEW=$((TOTAL_NEW + 1))

        if [ "$counter" != "#####" ] && [ "$counter" != "=====" ]; then
            TOTAL_COVERED=$((TOTAL_COVERED + 1))
        else
            echo "  NOT COVERED: $file:$i"
        fi
    done
done < /tmp/changed_lines.txt

echo ""
if [ $TOTAL_NEW -gt 0 ]; then
    PCT=$(echo "scale=1; $TOTAL_COVERED * 100 / $TOTAL_NEW" | bc -l)
    echo "Patch coverage: ${TOTAL_COVERED}/${TOTAL_NEW} lines = ${PCT}%"
else
    echo "No executable lines in patch"
fi

rm -f /tmp/changed_lines.txt *.gcov
```

### gcovr con diff

gcovr no tiene patch coverage nativo, pero Codecov sí:

```yaml
# codecov.yml
coverage:
  status:
    patch:
      default:
        target: 80%      # Código nuevo debe tener >= 80% de cobertura
        threshold: 5%     # Tolerancia
```

### Filosofía de la cobertura diferencial

```
 ╔══════════════════════════════════════════════════════════════════╗
 ║  REGLA: Es más importante que el código NUEVO tenga alta        ║
 ║  cobertura que subir la cobertura del código EXISTENTE.         ║
 ║                                                                  ║
 ║  Cobertura global: "¿cuánto del proyecto está cubierto?"        ║
 ║  → Útil para tendencia y visibilidad                            ║
 ║                                                                  ║
 ║  Cobertura de patch: "¿se testeó lo que acabo de escribir?"    ║
 ║  → Útil para calidad del PR actual                              ║
 ║                                                                  ║
 ║  Un proyecto con 60% global pero 90% en patches es más sano     ║
 ║  que uno con 85% global pero 20% en patches (cobertura legacy   ║
 ║  que nadie mantiene, código nuevo sin tests).                   ║
 ╚══════════════════════════════════════════════════════════════════╝
```

---

## 16. Multi-plataforma y matrix builds

### GitHub Actions: matrix con cobertura

```yaml
name: Multi-platform Coverage

on: [push, pull_request]

jobs:
  coverage:
    strategy:
      matrix:
        os: [ubuntu-latest, macos-latest]
        compiler: [gcc-12, gcc-13]
        exclude:
          - os: macos-latest
            compiler: gcc-12

    runs-on: ${{ matrix.os }}

    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies (Ubuntu)
        if: runner.os == 'Linux'
        run: |
          sudo apt-get update
          sudo apt-get install -y ${{ matrix.compiler }} lcov
          pip install gcovr

      - name: Install dependencies (macOS)
        if: runner.os == 'macOS'
        run: |
          brew install lcov
          pip3 install gcovr

      - name: Build with coverage
        env:
          CC: ${{ matrix.compiler }}
        run: |
          mkdir build
          $CC -O0 -g --coverage -Isrc tests/*.c src/*.c -o build/run_tests

      - name: Run tests
        run: ./build/run_tests

      - name: Generate coverage
        env:
          CC: ${{ matrix.compiler }}
        run: |
          GCOV_TOOL="gcov-$(echo $CC | grep -oP '\d+')"
          gcovr --root . --object-directory build \
              --exclude 'tests/.*' --branches \
              --gcov-executable "$GCOV_TOOL" \
              --print-summary

      - name: Upload coverage
        uses: codecov/codecov-action@v4
        with:
          files: build/filtered.info
          flags: ${{ matrix.os }}-${{ matrix.compiler }}
          token: ${{ secrets.CODECOV_TOKEN }}
```

### Combinar cobertura de múltiples plataformas

Codecov combina automáticamente los uploads con diferentes flags. Esto muestra qué código es específico de cada plataforma:

```
  ┌─────────────────────────────────────────────────────────────────┐
  │  Codecov — Combined Coverage                                    │
  │                                                                 │
  │  File              Linux    macOS    Combined                   │
  │  ─────────────────────────────────────────────                  │
  │  src/parser.c      85%      85%      85%                       │
  │  src/platform.c    92%      88%      95%     ← union           │
  │  src/linux_io.c    100%     N/A      100%    ← solo en Linux   │
  │  src/macos_io.c    N/A      100%     100%    ← solo en macOS   │
  └─────────────────────────────────────────────────────────────────┘
```

---

## 17. Optimización del pipeline de cobertura

### Problema: la cobertura es lenta

```
Pipeline típico:
  Build normal:     30 seg
  Tests:            10 seg
  Build coverage:   45 seg  (+50% por instrumentación)
  Tests coverage:   13 seg  (+30% por overhead)
  lcov capture:     8 seg
  genhtml:          5 seg
  Upload:           3 seg
  ─────────────────────────
  Total:            114 seg (vs 40 seg sin cobertura)
```

### Estrategia 1: Separar jobs

```yaml
jobs:
  # Job rápido: tests sin cobertura (feedback inmediato)
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: make test   # Build -O2, sin instrumentación

  # Job lento: cobertura (puede tardar más)
  coverage:
    runs-on: ubuntu-latest
    needs: test           # Solo si los tests pasan
    steps:
      - uses: actions/checkout@v4
      - run: make ci-coverage
```

### Estrategia 2: Cache de dependencias

```yaml
      - name: Cache build
        uses: actions/cache@v4
        with:
          path: build/
          key: coverage-${{ hashFiles('src/**', 'tests/**') }}
          restore-keys: coverage-

      - name: Build with coverage
        run: |
          # Solo recompila si cambió algo
          if [ ! -f build/run_tests ]; then
            make build-coverage
          fi
```

### Estrategia 3: Cobertura solo en PRs y main

```yaml
on:
  push:
    branches: [main]      # Cobertura en cada push a main
  pull_request:            # Cobertura en cada PR
  # NO en feature branches normales — corren solo make test
```

### Estrategia 4: Cobertura incremental

No recalcular todo desde cero si solo cambió un archivo:

```bash
# Solo ejecutar tests afectados por los cambios
CHANGED_FILES=$(git diff --name-only HEAD~1 -- src/)

# Determinar qué tests correr basado en dependencias
# (requiere mapeo de dependencias previo)
```

### Estrategia 5: Paralelizar tests

```yaml
      - name: Run tests in parallel
        run: |
          # Si usas CTest:
          cd build && ctest -j$(nproc) --output-on-failure
          # Los .gcda se acumulan de todos los tests
```

### Tabla de optimizaciones

```
Optimización                Ahorro típico    Complejidad    Riesgo
──────────────────────────────────────────────────────────────────────
Separar test/coverage jobs  Feedback rápido  Baja           Ninguno
Cache de build              20-50%           Baja           Stale cache
Solo en PR + main           60-80% de runs   Baja           Ninguno
Paralelizar tests (ctest)   30-60%           Media          Race en .gcda
Cobertura incremental       Variable         Alta           Datos incompletos
Skip HTML en PR             5-10 seg         Baja           Menos info visual
```

---

## 18. Comparación CI de cobertura: C vs Rust vs Go

```
Aspecto               C (gcov/lcov)                  Rust                       Go
──────────────────────────────────────────────────────────────────────────────────────────
Setup CI              10-20 líneas YAML              3-5 líneas YAML            2-3 líneas YAML
                      (build flags + lcov +          (cargo tarpaulin)          (go test -cover)
                       genhtml + filtros)

Comando mínimo        gcc --coverage + lcov          cargo tarpaulin            go test -coverprofile
                      + genhtml (3 tools)            --out Xml (1 tool)         (1 flag)

Formato CI            lcov .info o                   Cobertura XML, lcov,       coverprofile (text),
                      Cobertura XML (gcovr)          JSON, HTML                 go tool cover -html

Umbral en CI          Script manual o                tarpaulin --fail-under     Script manual
                      gcovr --fail-under-line        o cargo-llvm-cov

Upload a Codecov      codecov-action +               codecov-action +           codecov-action +
                      .info file                     Cobertura XML              coverprofile

Cobertura en PR       Codecov/script manual          Codecov/tarpaulin          Codecov/script manual

Tiempo adicional CI   +50-100% del build time        +30-60%                    +5-15%

Branch coverage       lcov --rc branch=1             llvm-cov (nativo)          No nativo
                      gcovr --branches

Patch coverage        Codecov o script manual        Codecov                    Codecov

Dependencias extra    lcov (apt), gcovr (pip)        cargo-tarpaulin (cargo     Ninguna (integrado)
                                                     install)
```

### Ejemplo mínimo en cada lenguaje

**C (GitHub Actions):**

```yaml
    - run: gcc -O0 --coverage -o tests tests.c src.c
    - run: ./tests
    - run: lcov --capture -d . -o cov.info
    - run: lcov --remove cov.info '/usr/*' -o filtered.info
    - uses: codecov/codecov-action@v4
      with: { files: filtered.info }
```

**Rust (GitHub Actions):**

```yaml
    - run: cargo install cargo-tarpaulin
    - run: cargo tarpaulin --out xml
    - uses: codecov/codecov-action@v4
      with: { files: cobertura.xml }
```

**Go (GitHub Actions):**

```yaml
    - run: go test -coverprofile=cover.out ./...
    - uses: codecov/codecov-action@v4
      with: { files: cover.out }
```

---

## 19. Errores comunes en CI de cobertura

### Error 1: gcov version mismatch en CI

```
gcov: error: version mismatch, expected 13, got 12
```

**Causa**: El runner de CI tiene múltiples versiones de GCC. `gcc` es v13 pero `gcov` apunta a v12.

**Solución**:

```yaml
      - name: Ensure matching gcov
        run: |
          GCC_VER=$(gcc -dumpversion | cut -d. -f1)
          sudo update-alternatives --set gcov /usr/bin/gcov-${GCC_VER}

      # O especificar en lcov:
      - run: lcov --gcov-tool gcov-13 --capture -d . -o cov.info
```

### Error 2: No .gcda files en CI

```
geninfo: WARNING: no .gcda files found in build/ - skipping!
```

**Causa**: Los tests no se ejecutaron, crashearon, o los .gcda se escribieron en otro directorio.

**Solución**:

```yaml
      # Verificar que los tests pasaron
      - name: Run tests
        run: |
          ./build/run_tests
          echo "Tests completed with exit code $?"

      # Verificar que se generaron .gcda
      - name: Verify coverage data
        run: |
          COUNT=$(find build -name "*.gcda" | wc -l)
          echo "Found $COUNT .gcda files"
          if [ "$COUNT" -eq 0 ]; then
            echo "ERROR: No coverage data generated"
            exit 1
          fi
```

### Error 3: Paths incorrectos en lcov

```
geninfo: WARNING: cannot find source file 'src/parser.c'
```

**Causa**: lcov busca los fuentes relativos al directorio donde se compiló. En CI, el workspace puede tener paths diferentes.

**Solución**:

```yaml
      - run: |
          lcov --capture \
              -d build \
              -b ${{ github.workspace }} \
              -o coverage.info
```

### Error 4: Cobertura inflada por headers inline

```
Summary: 95% lines
(pero los headers con funciones inline cuentan como "cubiertos"
 en cada translation unit que los incluye)
```

**Solución**:

```bash
# Excluir headers del reporte
lcov --remove coverage.info '*.h' -o filtered.info

# O solo incluir .c files
lcov --extract coverage.info '*/src/*.c' -o filtered.info
```

### Error 5: CI falla intermitentemente por race condition en .gcda

Cuando tests paralelos (ctest -j) escriben al mismo .gcda simultáneamente:

**Solución**:

```yaml
      # Opción 1: No paralelizar con cobertura
      - run: ctest --output-on-failure  # Sin -j

      # Opción 2: Usar GCOV_PREFIX para separar
      - run: |
          export GCOV_PREFIX=$PWD/gcov_data
          export GCOV_PREFIX_STRIP=3
          ctest -j$(nproc)
```

### Error 6: Artefactos demasiado grandes

```
Warning: Artifact upload size exceeds maximum (500 MB)
```

**Solución**:

```yaml
      # Comprimir antes de subir
      - run: tar czf coverage_html.tar.gz coverage_html/

      - uses: actions/upload-artifact@v4
        with:
          name: coverage
          path: coverage_html.tar.gz
          compression-level: 9
          retention-days: 14    # Reducir retención
```

### Error 7: El umbral es correcto localmente pero falla en CI

**Causa**: Diferencias en versión del compilador, flags, o archivos incluidos entre local y CI.

**Solución**:

```bash
# Mismos comandos exactos en Makefile (que CI usa)
# NO diferentes scripts para local y CI

# En CI:
make ci-coverage    # ← mismo target que el dev usa localmente
```

### Tabla de errores

```
Síntoma                           Causa                          Solución
──────────────────────────────────────────────────────────────────────────────────
version mismatch                  gcc ≠ gcov version              --gcov-tool o alternatives
no .gcda files                    Tests no corrieron/crashearon  Verificar exit code
cannot find source                Paths relativos incorrectos    -b workspace_dir
Cobertura > 100% en CI           Headers inline contados múltiple Excluir .h
Fallo intermitente                Race en .gcda con -j           Sin -j o GCOV_PREFIX
Artefacto muy grande              HTML sin comprimir             tar.gz + retention corta
Umbral OK local, falla en CI     Diferentes flags/versiones     Mismo Makefile target
Push falla por threshold file     Bot no tiene permisos push     PAT o github-actions bot
```

---

## 20. Ejemplo completo: proyecto multi-módulo con CI full

### Estructura del proyecto

```
myproject/
├── src/
│   ├── parser.h
│   ├── parser.c          # Parser de configuración
│   ├── validator.h
│   ├── validator.c       # Validador de datos
│   ├── store.h
│   └── store.c           # Almacenamiento key-value
├── tests/
│   ├── test_parser.c
│   ├── test_validator.c
│   ├── test_store.c
│   └── test_main.c       # Runner que ejecuta todos
├── scripts/
│   ├── check_coverage.sh
│   ├── coverage_ratchet.sh
│   └── record_coverage.sh
├── .github/
│   └── workflows/
│       └── ci.yml
├── .coverage_threshold
├── coverage_history.csv
├── Makefile
├── CMakeLists.txt
└── README.md
```

### Makefile completo

```makefile
CC       = gcc
CFLAGS   = -Wall -Wextra -Wpedantic -std=c11 -Isrc
SRC      = $(wildcard src/*.c)
TESTS    = $(wildcard tests/*.c)
BUILD    = build
COV_HTML = coverage_html

TEST_BIN = $(BUILD)/run_tests

LINE_THRESHOLD   ?= 80
BRANCH_THRESHOLD ?= 60
FUNC_THRESHOLD   ?= 90

# ─────────────────────────────────────────
# Desarrollo
# ─────────────────────────────────────────
.PHONY: all test clean

all: $(TEST_BIN)

$(BUILD):
	mkdir -p $@

$(TEST_BIN): $(SRC) $(TESTS) | $(BUILD)
	$(CC) $(CFLAGS) -O2 -o $@ $(TESTS) $(SRC)

test: $(TEST_BIN)
	./$<

# ─────────────────────────────────────────
# Cobertura
# ─────────────────────────────────────────
.PHONY: build-cov test-cov coverage coverage-html coverage-check

build-cov: | $(BUILD)
	$(CC) $(CFLAGS) -O0 -g --coverage -o $(TEST_BIN) $(TESTS) $(SRC)

test-cov: build-cov
	./$(TEST_BIN)

coverage: test-cov
	lcov --capture --initial -d . -o $(BUILD)/base.info -q
	lcov --capture -d . -o $(BUILD)/test.info -q
	lcov -a $(BUILD)/base.info -a $(BUILD)/test.info -o $(BUILD)/total.info -q
	lcov --remove $(BUILD)/total.info '/usr/*' '*/tests/*' -o $(BUILD)/filtered.info -q
	lcov --summary $(BUILD)/filtered.info

coverage-html: coverage
	genhtml $(BUILD)/filtered.info -o $(COV_HTML) \
		--title "$(notdir $(CURDIR))" --legend --branch-coverage -q
	@echo "Report: $(COV_HTML)/index.html"

coverage-check: coverage
	@./scripts/check_coverage.sh $(LINE_THRESHOLD) $(BRANCH_THRESHOLD) \
		$(FUNC_THRESHOLD) $(BUILD)/filtered.info

coverage-xml: test-cov
	gcovr --root . --exclude 'tests/.*' --branches \
		--cobertura-xml -o $(BUILD)/coverage.xml --print-summary

# ─────────────────────────────────────────
# CI
# ─────────────────────────────────────────
.PHONY: ci ci-quick

# CI completo: check + HTML + ratchet + history
ci: clean coverage-check coverage-html
	./scripts/coverage_ratchet.sh $(BUILD)/filtered.info
	./scripts/record_coverage.sh $(BUILD)/filtered.info
	@echo ""
	@echo "=== CI Complete ==="

# CI rápido: solo check
ci-quick: clean coverage-check
	@echo "=== CI Quick Complete ==="

clean:
	rm -rf $(BUILD) $(COV_HTML) *.gcno *.gcda *.gcov *.info
	rm -f src/*.gcno src/*.gcda tests/*.gcno tests/*.gcda
```

### GitHub Actions workflow

```yaml
# .github/workflows/ci.yml
name: CI

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  # ─────────────────────────────────────────
  # Job 1: Tests rápidos (feedback inmediato)
  # ─────────────────────────────────────────
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: make test

  # ─────────────────────────────────────────
  # Job 2: Cobertura (más lento)
  # ─────────────────────────────────────────
  coverage:
    runs-on: ubuntu-latest
    needs: test
    permissions:
      pull-requests: write
      contents: write

    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - name: Install lcov
        run: sudo apt-get update && sudo apt-get install -y lcov

      - name: Install gcovr
        run: pip install gcovr

      # === Build y test con cobertura ===
      - name: Coverage check
        run: make coverage-check

      # === Reporte HTML ===
      - name: Generate HTML report
        if: always()
        run: make coverage-html

      # === Cobertura XML para herramientas ===
      - name: Generate Cobertura XML
        if: always()
        run: make coverage-xml

      # === Subir artefactos ===
      - name: Upload HTML report
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: coverage-report
          path: coverage_html/
          retention-days: 30

      # === Upload a Codecov ===
      - name: Upload to Codecov
        if: always()
        uses: codecov/codecov-action@v4
        with:
          files: build/filtered.info
          token: ${{ secrets.CODECOV_TOKEN }}
          fail_ci_if_error: false

      # === Ratchet y histórico (solo en main) ===
      - name: Coverage ratchet
        if: github.event_name == 'push' && github.ref == 'refs/heads/main'
        run: |
          chmod +x scripts/coverage_ratchet.sh scripts/record_coverage.sh
          ./scripts/coverage_ratchet.sh build/filtered.info
          ./scripts/record_coverage.sh build/filtered.info

      - name: Commit updated thresholds
        if: github.event_name == 'push' && github.ref == 'refs/heads/main'
        run: |
          git config user.name "github-actions[bot]"
          git config user.email "github-actions[bot]@users.noreply.github.com"
          git add .coverage_threshold coverage_history.csv
          if git diff --cached --quiet; then
            echo "No changes to commit"
          else
            git commit -m "ci: update coverage data"
            git push
          fi

  # ─────────────────────────────────────────
  # Job 3: Deploy reporte (solo main)
  # ─────────────────────────────────────────
  deploy-report:
    needs: coverage
    if: github.ref == 'refs/heads/main'
    runs-on: ubuntu-latest
    permissions:
      pages: write
      id-token: write

    environment:
      name: github-pages

    steps:
      - name: Download report
        uses: actions/download-artifact@v4
        with:
          name: coverage-report
          path: public

      - name: Upload to Pages
        uses: actions/upload-pages-artifact@v3
        with:
          path: public

      - name: Deploy
        uses: actions/deploy-pages@v4
```

### Diagrama del pipeline completo

```
  ┌──────────────────────────────────────────────────────────────────────┐
  │                     CI PIPELINE COMPLETO                             │
  │                                                                      │
  │  push/PR ──► [test] ─── tests sin cobertura ──► PASS/FAIL (rápido) │
  │                 │                                                    │
  │                 ▼                                                    │
  │            [coverage] ── build --coverage                            │
  │                 │        run tests                                   │
  │                 │        lcov capture                                │
  │                 │        check thresholds ──► FAIL si bajó          │
  │                 │        genhtml (artefacto)                         │
  │                 │        upload Codecov                              │
  │                 │                                                    │
  │                 ├── (solo main) ── ratchet ── actualizar umbral     │
  │                 │                  record ─── guardar histórico     │
  │                 │                  commit ─── push datos            │
  │                 │                                                    │
  │                 ▼                                                    │
  │          [deploy-report] ── (solo main) ── GitHub Pages             │
  │                                                                      │
  │  Duración típica:                                                    │
  │    test:     ~30 seg                                                 │
  │    coverage: ~90 seg (paralelo con test si test pasa rápido)        │
  │    deploy:   ~15 seg                                                 │
  └──────────────────────────────────────────────────────────────────────┘
```

---

## 21. Programa de práctica

### Enunciado

Crea un proyecto C completo con CI de cobertura. El proyecto debe implementar una **lista enlazada genérica** (usando `void *` para los datos).

### Estructura requerida

```
linked_list/
├── src/
│   ├── list.h
│   └── list.c
├── tests/
│   ├── test_list.c
│   └── test_main.c
├── scripts/
│   ├── check_coverage.sh
│   └── coverage_ratchet.sh
├── .github/
│   └── workflows/
│       └── ci.yml
├── .coverage_threshold
├── Makefile
└── README.md
```

### API mínima

```c
typedef struct List List;

List *list_create(void (*free_fn)(void *));
void  list_destroy(List *list);
bool  list_push_front(List *list, void *data);
bool  list_push_back(List *list, void *data);
void *list_pop_front(List *list);
void *list_pop_back(List *list);
void *list_get(const List *list, size_t index);
size_t list_size(const List *list);
bool  list_is_empty(const List *list);
void  list_clear(List *list);
```

### Tareas

1. Implementa la lista enlazada y tests que alcancen al menos 80% de line coverage y 60% de branch coverage

2. Crea el Makefile con targets: `test`, `coverage`, `coverage-check`, `coverage-html`, `ci`, `clean`

3. Crea `scripts/check_coverage.sh` que verifique umbrales

4. Crea `scripts/coverage_ratchet.sh` que implemente el trinquete

5. Crea `.github/workflows/ci.yml` con:
   - Job de tests rápidos
   - Job de cobertura con verificación de umbrales
   - Upload de artefactos HTML
   - (Opcional) Ratchet en main

6. Verifica localmente:
   - `make test` pasa
   - `make coverage-check` pasa con los umbrales
   - `make coverage-html` genera reporte navegable

7. (Opcional) Inicializa el repositorio Git, push a GitHub, y verifica que el workflow funciona

---

## 22. Ejercicios

### Ejercicio 1: Makefile con cobertura para CI (20 min)

Dado un proyecto con `src/calculator.c` y `tests/test_calculator.c`:

**Tareas**:
- a) Escribe un Makefile con targets: `test`, `coverage`, `coverage-check`, `coverage-html`, `clean`
- b) `coverage-check` debe aceptar `LINE_THRESHOLD` y `BRANCH_THRESHOLD` como variables de Make
- c) Verifica que `make coverage-check LINE_THRESHOLD=90` falla si la cobertura es menor a 90%
- d) Verifica que `make coverage-check LINE_THRESHOLD=50` pasa

### Ejercicio 2: GitHub Actions workflow (25 min)

**Tareas**:
- a) Escribe un `.github/workflows/coverage.yml` que:
  - Se ejecute en push a main y en PRs
  - Instale gcc y lcov
  - Compile con `--coverage`, ejecute tests, capture cobertura
  - Verifique umbral de 80% líneas y 60% ramas
  - Suba el reporte HTML como artefacto
- b) Añade un step que genere Cobertura XML con gcovr
- c) Añade upload a Codecov (puede ser sin token para repos públicos)
- d) (Opcional) Prueba el workflow haciendo push a un repo real

### Ejercicio 3: Coverage ratchet (20 min)

**Tareas**:
- a) Implementa un script `coverage_ratchet.sh` que:
  - Lea el umbral actual de `.coverage_threshold`
  - Compare con la cobertura actual
  - Falle si bajó (sin tolerancia)
  - Actualice el umbral si subió
- b) Modifica el script para aceptar una tolerancia de 0.5%
- c) Integra el script en un Makefile target `coverage-ratchet`
- d) Simula el flujo: ejecuta 3 veces, bajando y subiendo cobertura, y verifica que el ratchet funciona

### Ejercicio 4: Cobertura diferencial en PR (25 min)

**Tareas**:
- a) En un repo Git, crea una rama `feature/add-validation`
- b) Añade una nueva función `validate_input()` en `src/validator.c` (sin tests)
- c) Ejecuta cobertura y observa:
  - La cobertura global baja poco (código nuevo sin tests en un proyecto grande)
  - La cobertura del archivo `validator.c` baja significativamente
- d) Escribe un script que calcule la cobertura **solo de las líneas nuevas** (diff contra main)
- e) Verifica que el script reporta 0% para las líneas nuevas
- f) Añade tests para `validate_input()` y verifica que el script reporta > 80%
- g) Reflexiona: ¿es más útil verificar cobertura global o cobertura de patch en un PR?

---

**Navegación**:
- ← Anterior: [T02 — Interpretar cobertura](../T02_Interpretar_cobertura/README.md)
- → Siguiente: [C02 — Testing en Rust](../../../C02_Testing_en_Rust/README.md)
- ↑ Sección: [S04 — Cobertura en C](../README.md)

---

> **Fin de la Sección 4 — Cobertura en C**
>
> **Fin del Capítulo 1 — Testing en C**
