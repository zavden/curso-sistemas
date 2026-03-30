# Matemáticas Necesarias para el Curso Linux-C-Rust-Go

## Resumen General

Este documento detalla los requisitos matemáticos de cada bloque del curso, indicando qué capítulos del **Rosen (Discrete Mathematics and Its Applications, 8th ed)** aplican y qué otros fundamentos matemáticos se necesitan.

```
┌──────────────────────────────────────────────────────────────┐
│         FUNDAMENTOS MATEMÁTICOS DEL CURSO                    │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Prerrequisitos que ya tienes:                               │
│  ├─ Álgebra                                                  │
│  ├─ Álgebra Lineal                                           │
│  └─ Cálculo Diferencial                                      │
│                                                              │
│  Lo que necesitas agregar:                                    │
│  ├─ Matemática Discreta (Rosen caps 2-5, 7, 10-12)          │
│  └─ Estadística básica (media, mediana, percentiles, σ)      │
│                                                              │
│  Lo que NO necesitas:                                        │
│  ├─ Cálculo integral / multivariable                         │
│  ├─ Ecuaciones diferenciales                                 │
│  ├─ Análisis real / complejo                                 │
│  └─ Álgebra abstracta (grupos, anillos, cuerpos)             │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## Capítulos del Rosen — Mapa de Uso

| Cap | Título | Bloques que lo usan | Prioridad |
|:---:|---|---|:---:|
| 1 | Logic and Proofs | Cultura general | Opcional |
| **2** | **Sets, Functions, Sequences** | **B19 Bases de datos** | Importante |
| **3** | **Algorithms (Big O)** | **B15 Estructuras de datos** | Esencial |
| **4** | **Number Theory & Cryptography** | **B11 Seguridad** | Esencial |
| **5** | **Induction & Recursion** | **B15, B03, B05** | Esencial |
| 6 | Counting | No se usa directamente | Saltar |
| **7** | **Discrete Probability** | **B17 Testing, B20 Observabilidad** | Importante |
| 8 | Advanced Counting | Overkill para el curso | Saltar |
| 9 | Relations | Teórico para este nivel | Saltar |
| **10** | **Graphs** | **B15 Estructuras de datos** | Esencial |
| **11** | **Trees** | **B15 Estructuras de datos** | Esencial |
| **12** | **Boolean Algebra** | **B03, B05, B06, B07, B09** | Útil |
| 13 | Modeling Computation | No aplica | Saltar |

---

## Matemáticas por Bloque — Detalle

---

### B01 Docker y Entorno de Laboratorio

**Matemáticas necesarias: ninguna**

Docker es operacional. Lo más "matemático" es entender unidades de memoria (KB, MB, GB) y porcentajes de CPU, que son aritmética básica.

---

### B02 Fundamentos de Linux

**Matemáticas necesarias: aritmética básica**

| Concepto | Dónde aparece | Nivel |
|---|---|---|
| Octal (base 8) | Permisos de archivo (chmod 755) | Aritmética |
| Hexadecimal (base 16) | Direcciones de memoria, colores, /proc | Aritmética |
| Binario (base 2) | Representación interna, umask | Aritmética |
| Porcentajes | df, free, top, monitoreo de recursos | Aritmética |

No necesitas Rosen para este bloque.

---

### B03 Fundamentos de C

**Matemáticas necesarias: álgebra + Rosen parcial**

| Concepto | Dónde aparece | Fuente |
|---|---|---|
| Sistemas numéricos (bin, oct, hex) | Literales, printf, bitwise ops | Álgebra |
| Aritmética de punteros | Offsets, indexación de arrays | Álgebra |
| Overflow/underflow | Límites de int, unsigned wrap | Álgebra |
| Recursión (conceptual) | Funciones recursivas, factorial, fibonacci | **Rosen Cap 5** |
| Álgebra booleana | Operadores bitwise (&, \|, ^, ~, <<, >>) | **Rosen Cap 12** |

**Rosen relevante:**
- **Cap 5 (secciones 5.3-5.4)**: Definiciones recursivas y algoritmos recursivos — entender cómo funciona la recursión antes de implementarla en C
- **Cap 12 (secciones 12.1-12.2)**: Funciones booleanas — fundamenta las operaciones bitwise

---

### B04 Sistemas de Compilación (Make/CMake)

**Matemáticas necesarias: ninguna**

Make y CMake son herramientas de build. El grafo de dependencias (DAG) es intuitivo sin necesitar la teoría formal de grafos.

---

### B05 Fundamentos de Rust

**Matemáticas necesarias: álgebra + Rosen parcial**

Mismo perfil que B03 C, con un añadido:

| Concepto | Dónde aparece | Fuente |
|---|---|---|
| Sistemas numéricos | Literales, formatting | Álgebra |
| Recursión | Pattern matching recursivo, enums recursivos | **Rosen Cap 5** |
| Álgebra booleana | Bitwise, pattern matching | **Rosen Cap 12** |
| Iteradores y composición de funciones | .map(), .filter(), .fold() | Concepto de funciones (**Rosen Cap 2.3**) |

---

### B06 Programación de Sistemas en C

**Matemáticas necesarias: álgebra + Rosen caps 3, 5, 12**

| Concepto | Dónde aparece | Fuente |
|---|---|---|
| Aritmética de punteros avanzada | Memoria, structs, alignment, padding | Álgebra |
| Bit manipulation | Flags del kernel, ioctl, mmap flags | **Rosen Cap 12** |
| Complejidad algorítmica | Elegir estructuras para system calls | **Rosen Cap 3** |
| Recursión | Recorrido de árboles de directorios, parsers | **Rosen Cap 5** |
| Representación binaria | Protocolos, serialización, endianness | Álgebra |

---

### B07 Programación de Sistemas en Rust

**Matemáticas necesarias: mismo perfil que B06**

Mismos fundamentos que B06 pero implementados en Rust. La diferencia es que Rust usa pattern matching y algebraic types, que se benefician de entender funciones y composición (Rosen Cap 2.3).

---

### B08 Almacenamiento y Sistemas de Archivos

**Matemáticas necesarias: aritmética + álgebra básica**

| Concepto | Dónde aparece | Nivel |
|---|---|---|
| Potencias de 2 | Tamaños de bloque (512, 4096), sectores | Aritmética |
| Aritmética de direcciones | LBA, offsets en particiones, inodes | Álgebra |
| Porcentajes y proporciones | Uso de disco, fragmentación | Aritmética |
| Paridad XOR | RAID 5/6, checksums | **Rosen Cap 12** (álgebra booleana) |

**Rosen relevante:**
- **Cap 12 (sección 12.1)**: XOR como función booleana — entender cómo RAID 5 reconstruye datos con paridad

---

### B09 Redes

**Matemáticas necesarias: álgebra + Rosen cap 12**

| Concepto | Dónde aparece | Fuente |
|---|---|---|
| Binario y hexadecimal | Direcciones IP, MAC, subnetting | Álgebra |
| Operaciones AND con máscaras | Cálculo de red/host en subnetting | **Rosen Cap 12** |
| Potencias de 2 | Tamaños de subred (/24 = 256 hosts) | Aritmética |
| CIDR y prefijos | Agregación de rutas, VLSM | Álgebra |
| Checksums | TCP/UDP checksum, CRC | **Rosen Cap 4** (aritmética modular) |

**Ejemplo concreto — subnetting:**
```
IP:       192.168.1.100  = 11000000.10101000.00000001.01100100
Máscara:  255.255.255.0  = 11111111.11111111.11111111.00000000
Red:      IP AND Máscara = 11000000.10101000.00000001.00000000
                         = 192.168.1.0
```
Esto es directamente álgebra booleana (Rosen Cap 12).

---

### B10 Servicios de Red

**Matemáticas necesarias: mínimas**

| Concepto | Dónde aparece | Nivel |
|---|---|---|
| Números de puerto (0-65535) | Configuración de servicios | Aritmética |
| TTL, timeouts | DNS, HTTP, caching | Aritmética |
| Certificados X.509 | TLS/SSL — clave pública/privada conceptual | **Rosen Cap 4** (conceptual) |

La configuración de servicios es mayormente operacional. La criptografía de TLS la entenderás a nivel conceptual con Rosen Cap 4.

---

### B11 Seguridad, Kernel y Arranque

**Matemáticas necesarias: Rosen cap 4 (esencial)**

Este es el bloque que más depende de teoría de números:

| Concepto | Dónde aparece | Fuente |
|---|---|---|
| Aritmética modular | RSA, Diffie-Hellman, hashing | **Rosen Cap 4.1, 4.4** |
| Números primos | Generación de claves RSA | **Rosen Cap 4.3** |
| Exponenciación modular | Criptografía de clave pública | **Rosen Cap 4.2** |
| Criptografía completa | TLS, GPG, LUKS, SSH keys | **Rosen Cap 4.6** |
| Álgebra booleana | SELinux, capabilities, bitmasks | **Rosen Cap 12** |

**Rosen relevante:**
- **Cap 4 completo**: Es el capítulo más directamente aplicable de todo el libro para este bloque
- **Cap 4.6** en particular cubre RSA, criptografía simétrica/asimétrica, firma digital

**Ejemplo concreto — RSA simplificado:**
```
1. Elegir primos p=61, q=53
2. n = p × q = 3233
3. φ(n) = (p-1)(q-1) = 3120
4. Elegir e tal que gcd(e, φ(n)) = 1 → e = 17
5. Calcular d tal que d × e ≡ 1 (mod φ(n)) → d = 2753
6. Clave pública: (e=17, n=3233)
7. Clave privada: (d=2753, n=3233)
8. Cifrar: c = m^e mod n
9. Descifrar: m = c^d mod n
```
Todo esto es Rosen Cap 4.

---

### B12 GUI con Rust (egui)

**Matemáticas necesarias: álgebra lineal básica**

| Concepto | Dónde aparece | Fuente |
|---|---|---|
| Coordenadas 2D | Posicionamiento de widgets, layout | Álgebra lineal |
| Vectores 2D | Arrastrar, scroll, animaciones | Álgebra lineal |
| Interpolación lineal (lerp) | Animaciones, transiciones | Álgebra |
| Color como vector (R,G,B,A) | Temas, estilos | Álgebra lineal |

Ya tienes álgebra lineal — este bloque no requiere nada adicional.

---

### B13 Multimedia con GStreamer

**Matemáticas necesarias: conceptual**

| Concepto | Dónde aparece | Nivel |
|---|---|---|
| Frecuencia de muestreo (Hz) | Audio (44100 Hz, 48000 Hz) | Conceptual |
| Resolución y aspect ratio | Video (1920×1080, 16:9) | Aritmética |
| Bitrate | Compresión de audio/video | Aritmética |
| Transformada de Fourier | Solo conceptual — no la implementas | Cultura general |

No necesitas implementar FFT ni DSP. GStreamer abstrae todo eso. Solo necesitas entender los conceptos para elegir parámetros de codificación.

---

### B14 Interoperabilidad C/Rust y WebAssembly

**Matemáticas necesarias: ninguna adicional**

Lo que necesitabas ya lo aprendiste en B03, B05, B06 y B07. Este bloque es sobre FFI, ABI, y WASM — es ingeniería de software, no matemáticas.

---

### B15 Estructuras de Datos en C

**Matemáticas necesarias: Rosen caps 3, 5, 10, 11 (ESENCIAL)**

Este es el bloque más matemáticamente intenso del curso:

| Concepto | Dónde aparece | Fuente |
|---|---|---|
| **Notación Big O** | Análisis de TODA estructura | **Rosen Cap 3.2** |
| **Complejidad de algoritmos** | Comparar O(1) vs O(log n) vs O(n) vs O(n²) | **Rosen Cap 3.3** |
| **Recursión** | Árboles, listas enlazadas, quicksort, mergesort | **Rosen Cap 5.3-5.4** |
| **Inducción matemática** | Probar correctitud de algoritmos recursivos | **Rosen Cap 5.1-5.2** |
| **Grafos** | Listas de adyacencia, BFS, DFS, Dijkstra | **Rosen Cap 10** |
| **Árboles** | BST, AVL, B-trees, heaps, recorridos | **Rosen Cap 11** |
| Logaritmos | Altura de árboles balanceados = O(log n) | Álgebra |
| Sumatorias | Análisis de loops anidados | Álgebra / **Rosen Cap 2.4** |

**Mapeo detallado Rosen → B15:**

```
┌──────────────────────────────────────────────────────────────┐
│     ROSEN → B15 ESTRUCTURAS DE DATOS                         │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Rosen 3.2 (Big O)                                           │
│  └→ Analizar complejidad de: inserción en array vs lista     │
│     enlazada, búsqueda en hash table vs BST, sort algorithms │
│                                                              │
│  Rosen 3.3 (Complejidad)                                     │
│  └→ O(1) hash lookup, O(log n) BST search,                  │
│     O(n) linear search, O(n log n) mergesort,                │
│     O(n²) bubblesort                                         │
│                                                              │
│  Rosen 5.3-5.4 (Recursión)                                   │
│  └→ Recorridos de árboles (inorder, preorder, postorder),    │
│     mergesort, quicksort, DFS en grafos                      │
│                                                              │
│  Rosen 5.1-5.2 (Inducción)                                   │
│  └→ Probar que un BST balanceado tiene altura O(log n),      │
│     probar correctitud de algoritmos recursivos              │
│                                                              │
│  Rosen 10.1-10.6 (Grafos)                                    │
│  └→ Representación (adj matrix vs adj list),                 │
│     BFS, DFS, Dijkstra (shortest path),                      │
│     detección de ciclos, componentes conectados              │
│                                                              │
│  Rosen 11.1-11.5 (Árboles)                                   │
│  └→ BST, AVL, heaps, recorridos, spanning trees,            │
│     árbol de expresiones, Huffman coding                     │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

**Recomendación**: Estudiar Rosen caps 3 y 5 ANTES de empezar B15. Caps 10 y 11 se pueden estudiar en paralelo con los temas correspondientes de B15.

---

### B16 Patrones de Diseño

**Matemáticas necesarias: ninguna**

Patrones de diseño es ingeniería de software pura. Creational, Structural, Behavioral patterns no requieren matemáticas. La complejidad algorítmica que aprendiste en B15 te ayuda a evaluar el costo de ciertos patrones, pero no es un requisito.

---

### B17 Testing e Ingeniería de Software

**Matemáticas necesarias: Rosen cap 7 + estadística básica**

| Concepto | Dónde aparece | Fuente |
|---|---|---|
| Probabilidad básica | Cobertura de tests, fuzzing | **Rosen Cap 7.1-7.2** |
| Valor esperado | Análisis de benchmarks | **Rosen Cap 7.4** |
| Varianza y desviación estándar | Estabilidad de benchmarks | **Rosen Cap 7.4** |
| Percentiles | p50, p90, p95, p99 en latencia | Estadística básica |
| Media, mediana, moda | Interpretar resultados de benchmark | Estadística básica |
| Intervalos de confianza | ¿Es esta mejora real o ruido? | Estadística básica |

**Rosen relevante:**
- **Cap 7.1-7.2**: Probabilidad básica — entender distribuciones
- **Cap 7.4**: Valor esperado y varianza — interpretar benchmarks

**Estadística adicional** (fuera de Rosen): Para interpretar benchmarks necesitas entender percentiles y desviación estándar. Un recurso corto de estadística descriptiva (Khan Academy o similar) cubre esto en pocas horas.

---

### B18 Scripting y Automatización

**Matemáticas necesarias: ninguna adicional**

Bash scripting, Python para SysAdmin, Ansible — son herramientas operacionales. La aritmética de shell (`$(( ))`, `bc`) es trivial.

---

### B19 Bases de Datos

**Matemáticas necesarias: Rosen cap 2 + lógica básica**

| Concepto | Dónde aparece | Fuente |
|---|---|---|
| Teoría de conjuntos | Álgebra relacional (UNION, INTERSECT, EXCEPT) | **Rosen Cap 2.1-2.2** |
| Funciones | Mapeos, claves primarias/foráneas | **Rosen Cap 2.3** |
| Lógica proposicional | WHERE clauses (AND, OR, NOT) | **Rosen Cap 1.1-1.3** (opcional) |
| Producto cartesiano | JOINs en SQL | **Rosen Cap 2.1** |

**Ejemplo concreto — SQL como teoría de conjuntos:**
```sql
-- UNION = unión de conjuntos (Rosen 2.2)
SELECT name FROM employees UNION SELECT name FROM contractors;

-- INTERSECT = intersección (Rosen 2.2)
SELECT id FROM orders_2024 INTERSECT SELECT id FROM orders_2025;

-- JOIN = producto cartesiano filtrado (Rosen 2.1)
SELECT * FROM orders o JOIN customers c ON o.customer_id = c.id;

-- WHERE = predicado lógico (Rosen 1.1)
SELECT * FROM servers WHERE status = 'active' AND region = 'us-east';
```

---

### B20 Kubernetes y Observabilidad

**Matemáticas necesarias: estadística básica**

| Concepto | Dónde aparece | Fuente |
|---|---|---|
| Percentiles (p50, p90, p95, p99) | SLOs, latencia de servicios | Estadística |
| Tasas (rate) | Requests/second, error rate | Aritmética |
| Media móvil | Dashboards de Grafana, alerting | Estadística |
| Histogramas | Distribución de latencia en Prometheus | Estadística |
| Probabilidad | Error budgets, SLA calculations | **Rosen Cap 7.1** |

**Ejemplo concreto — SLO/SLA:**
```
SLO: 99.9% de requests con latencia < 200ms
Budget de error: 0.1% = 43.2 minutos/mes de downtime permitido

Si p99 latency > 200ms → estás quemando budget
Si error rate > 0.1% → alerta
```

---

### B21 Cloud Computing

**Matemáticas necesarias: aritmética**

| Concepto | Dónde aparece | Nivel |
|---|---|---|
| Cálculo de costos | Instancias × horas × precio | Aritmética |
| Escalado | 2x CPU = ¿2x throughput? (ley de Amdahl conceptual) | Conceptual |
| Networking | CIDR blocks, VPCs, subnetting | Ya cubierto en B09 |

---

### B22 Fundamentos de Go

**Matemáticas necesarias: ninguna adicional**

Todo lo matemático que aparece en B22 (bit manipulation, aritmética, overflow) ya lo cubriste en B03/B05. El bloque de concurrencia (Cap 8) no requiere matemáticas formales — es ingeniería de software.

---

### Ruta Independiente

#### Bases de Datos
Ya cubierto en B19.

#### JavaScript / TypeScript
**Matemáticas necesarias: álgebra** — manipulación del DOM, event loop, async/await son conceptos de programación, no matemáticos.

#### Canvas
**Matemáticas necesarias: álgebra lineal**

| Concepto | Dónde aparece | Fuente |
|---|---|---|
| Coordenadas 2D | Posicionamiento, dibujo | Álgebra lineal |
| Transformaciones (translate, rotate, scale) | Matrices 2×2 y 3×3 | Álgebra lineal |
| Trigonometría | Rotaciones, arcos, animaciones circulares | Álgebra / Trigonometría |
| Interpolación | Animaciones, easing functions | Álgebra |

#### Shaders (GLSL)
**Matemáticas necesarias: álgebra lineal (intensiva)**

| Concepto | Dónde aparece | Fuente |
|---|---|---|
| Vectores 2D/3D/4D | Posición, color, normales | Álgebra lineal |
| Matrices 4×4 | Model-View-Projection transform | Álgebra lineal |
| Producto punto/cruz | Iluminación (Phong, Lambert) | Álgebra lineal |
| Normalización de vectores | Cálculos de iluminación | Álgebra lineal |
| Coordenadas homogéneas | Perspectiva, proyección | Álgebra lineal |
| Funciones trigonométricas | Ondas, distorsiones, ruido | Trigonometría |
| Funciones de ruido (Perlin, Simplex) | Texturas procedurales | Conceptual |

Ya tienes álgebra lineal — es el bloque donde más la aprovecharás.

---

## Plan de Estudio del Rosen

### Orden recomendado (alineado con el roadmap)

```
┌──────────────────────────────────────────────────────────────┐
│          CUÁNDO ESTUDIAR CADA CAPÍTULO DEL ROSEN             │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  FASE 1 — Con B03 C y B05 Rust (fundamentos)                │
│  ├─ Cap 12: Boolean Algebra (bitwise operations)             │
│  └─ Cap 5.3-5.4: Recursión (funciones recursivas)           │
│                                                              │
│  FASE 2 — Con B09 Redes y B11 Seguridad                      │
│  ├─ Cap 4: Number Theory & Cryptography (completo)           │
│  └─ Cap 12: Repaso de booleana para subnetting               │
│                                                              │
│  FASE 3 — Con B15 Estructuras de Datos (paralelo)            │
│  ├─ Cap 3: Algorithms y Big O (ANTES de empezar B15)         │
│  ├─ Cap 5.1-5.2: Inducción (para probar correctitud)        │
│  ├─ Cap 10: Grafos (en paralelo con grafos de B15)           │
│  └─ Cap 11: Árboles (en paralelo con árboles de B15)        │
│                                                              │
│  FASE 4 — Con B17 Testing y B19 Bases de Datos               │
│  ├─ Cap 7: Probabilidad discreta (benchmarks, fuzzing)       │
│  ├─ Cap 2.1-2.3: Conjuntos y funciones (álgebra relacional) │
│  └─ Estadística descriptiva (fuera de Rosen)                 │
│                                                              │
│  TOTAL: 8 capítulos de 13, distribuidos en 4 fases           │
│  Capítulos saltados: 1, 6, 8, 9, 13                         │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Secciones específicas por capítulo

| Capítulo | Secciones a estudiar | Secciones que puedes saltar |
|---|---|---|
| **2** | 2.1 (Sets), 2.2 (Set Operations), 2.3 (Functions) | 2.4-2.6 (Sequences, Cardinality, Matrices — ya tienes álgebra lineal) |
| **3** | 3.1, 3.2, 3.3 — **TODO** | Ninguna, todo es esencial |
| **4** | 4.1, 4.2, 4.3, 4.6 | 4.4, 4.5 (congruences avanzadas — overkill) |
| **5** | 5.1, 5.2, 5.3, 5.4 | 5.5 (Program Correctness — muy teórico) |
| **7** | 7.1, 7.2, 7.4 | 7.3 (Bayes — útil pero no esencial para el curso) |
| **10** | 10.1, 10.2, 10.3, 10.4, 10.6 | 10.5 (Euler/Hamilton), 10.7-10.8 (Planar, Coloring — teóricos) |
| **11** | 11.1, 11.2, 11.3 | 11.4-11.5 (Spanning trees — si tienes tiempo, útil pero no crítico) |
| **12** | 12.1, 12.2 | 12.3-12.4 (Logic gates, circuits — hardware, no necesario) |

### Estimación de tiempo

| Capítulo | Secciones | Horas estimadas |
|---|:---:|:---:|
| Cap 2 (parcial) | 3 secciones | 4-6 h |
| Cap 3 (completo) | 3 secciones | 6-8 h |
| Cap 4 (parcial) | 4 secciones | 8-10 h |
| Cap 5 (parcial) | 4 secciones | 8-10 h |
| Cap 7 (parcial) | 3 secciones | 4-6 h |
| Cap 10 (parcial) | 5 secciones | 10-14 h |
| Cap 11 (parcial) | 3 secciones | 6-8 h |
| Cap 12 (parcial) | 2 secciones | 3-4 h |
| **TOTAL** | **27 secciones** | **~50-65 h** |

---

## Resumen Visual por Bloque

```
Bloque              Álgebra  Á.Lineal  Cálculo  Rosen    Estadística
────────────────    ───────  ────────  ───────  ───────  ───────────
B01 Docker            —        —         —        —          —
B02 Linux             ✓        —         —        —          —
B03 C                 ✓        —         —       5,12        —
B04 Make/CMake        —        —         —        —          —
B05 Rust              ✓        —         —       5,12        —
B06 Sistemas C        ✓        —         —      3,5,12       —
B07 Sistemas Rust     ✓        —         —      3,5,12       —
B08 Almacenamiento    ✓        —         —       12          —
B09 Redes             ✓        —         —      4,12         —
B10 Servicios Red     ✓        —         —        —          —
B11 Seguridad         ✓        —         —     4(★),12       —
B12 GUI               ✓        ✓         —        —          —
B13 Multimedia        ✓        —         —        —          —
B14 Interop/WASM      —        —         —        —          —
B15 Estructuras(★)    ✓        —         —   3,5,10,11(★)    —
B16 Patrones          —        —         —        —          —
B17 Testing           ✓        —         —        7          ✓
B18 Scripting         —        —         —        —          —
B19 Bases de Datos    ✓        —         —       2           —
B20 Kubernetes        ✓        —         —       7           ✓
B21 Cloud             ✓        —         —        —          —
B22 Go                ✓        —         —        —          —
── Ruta Indep. ──
Canvas                ✓        ✓         —        —          —
Shaders               ✓       ✓(★)       —        —          —

(★) = uso intensivo
```

**Conclusión**: Con álgebra + álgebra lineal + cálculo diferencial (que ya tienes) + Rosen caps 2,3,4,5,7,10,11,12 (27 secciones, ~50-65 horas) + estadística descriptiva básica (~5-10 horas), tienes cubierto el 100% del curso.
