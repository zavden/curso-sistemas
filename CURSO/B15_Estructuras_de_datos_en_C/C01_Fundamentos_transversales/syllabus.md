# C01 — Fundamentos transversales

**Objetivo**: Establecer las bases conceptuales y las herramientas que se usarán
a lo largo de todo el bloque. Definir qué es un TAD, cómo analizar complejidad,
cómo depurar estructuras dinámicas, y cómo usar `unsafe` Rust para
implementaciones con raw pointers.

**Prerequisitos**: B03 (punteros, malloc/free, structs), B05 (ownership, traits,
generics, Box/Rc/RefCell).

---

## S01 — Tipos abstractos de datos (TAD)

| Tópico | Contenido |
|--------|-----------|
| **T01 — Definición y contrato** | Qué es un TAD, separación de interfaz e implementación, invariantes, precondiciones, postcondiciones. Ejemplo: TAD "Conjunto" definido por operaciones sin fijar representación. |
| **T02 — TADs en C** | Headers `.h` como interfaz, `.c` como implementación, structs opacos, punteros opacos como encapsulación. Patrón `set_create()` / `set_destroy()`. |
| **T03 — TADs en Rust** | Traits como contrato, structs con campos privados, `impl` blocks, visibilidad `pub`/`pub(crate)`. Trait `Set<T>` como ejemplo. |
| **T04 — Genericidad** | `void *` + callbacks en C vs genéricos `<T>` + trait bounds en Rust. Tradeoffs: type-safety, ergonomía, rendimiento. `qsort` como ejemplo de genericidad con `void *`. |

**Ejercicios**: Diseñar TAD "Bolsa" (multiset) — definir operaciones, implementar
interfaz opaca en C y trait en Rust. Comparar ergonomía.

---

## S02 — Complejidad algorítmica

| Tópico | Contenido |
|--------|-----------|
| **T01 — Notación O, Ω, Θ** | Definición formal, interpretación intuitiva, reglas de simplificación (constantes, término dominante). Ejemplos con loops anidados. ▸ **Demostración formal**: probar con la definición (∃c, n₀) que 3n² + 5n ∈ O(n²) y que n² ∉ O(n); probar Θ como intersección de O y Ω. |
| **T02 — Análisis de mejor, peor y caso promedio** | Importancia del peor caso para garantías, del caso promedio para rendimiento real. Ejemplo: búsqueda lineal. |
| **T03 — Análisis amortizado** | Método del banquero, método del potencial. Ejemplo canónico: `push` en vector dinámico con duplicación → O(1) amortizado. ▸ **Demostración formal**: prueba completa por método del potencial (Φ = 2n − capacidad) de que la inserción en vector dinámico con duplicación es O(1) amortizado. |
| **T04 — Complejidad espacial** | Memoria extra vs total, in-place O(1), tradeoffs tiempo-espacio. |
| **T05 — Clases de complejidad comunes** | O(1), O(log n), O(n), O(n log n), O(n²), O(2ⁿ), O(n!) — tabla con ejemplos, gráfica mental de crecimiento. |

**Ejercicios**: Analizar complejidad de fragmentos de código. Comparar experimentalmente
O(n) vs O(n²) para n = 10³, 10⁴, 10⁵, 10⁶.

---

## S03 — Herramientas para estructuras dinámicas

| Tópico | Contenido |
|--------|-----------|
| **T01 — Valgrind para DS** | Patrones de leak en listas/árboles, use-after-free en destrucción, `--track-origins=yes`. |
| **T02 — GDB para punteros** | Inspeccionar nodos con `p node->next->data`, watchpoints en campos, scripts para recorrer listas. |
| **T03 — ASan y Miri** | `-fsanitize=address` en C, `cargo miri test` en Rust, cuándo usar cada herramienta. |
| **T04 — Testing de estructuras** | Tests unitarios para TADs, tests de estrés, verificación de invariantes, property-based testing (proptest). |

**Ejercicios**: Introducir bugs deliberados en lista enlazada C y detectar con Valgrind.
Mismo ejercicio en Rust unsafe + Miri.

---

## S04 — unsafe Rust para estructuras de datos

> Mini-módulo autocontenido. No requiere B07.

| Tópico | Contenido |
|--------|-----------|
| **T01 — Raw pointers** | `*const T`, `*mut T`, creación segura, desreferencia unsafe, `null/null_mut`, aritmética con `.add()/.offset()`. |
| **T02 — Bloques unsafe** | Qué garantiza el programador, `// SAFETY:` comments, minimizar scope. |
| **T03 — NonNull y Box raw** | `NonNull<T>`, `Box::into_raw`/`Box::from_raw`, patrón para listas con raw pointers. |
| **T04 — Cuándo usar unsafe** | Árbol de decisión: `Option<Box<Node<T>>>` (safe) → `Rc<RefCell<T>>` → raw pointers (unsafe). Benchmark de overhead. |

**Ejercicios**: Stack simple con raw pointers + unsafe. Comparar con versión `Vec<T>`.
Verificar con `cargo miri test`.

---

## Demostraciones formales pendientes (README_EXTRA.md)

Los siguientes archivos contienen las pruebas matemáticas rigurosas
que complementan los README.md existentes:

| Ubicación | Contenido |
|-----------|-----------|
| `S02_Complejidad_algoritmica/T01_Notacion_O_Omega_Theta/README_EXTRA.md` | Probar con la definición (∃c, n₀) que 3n² + 5n ∈ O(n²) y que n² ∉ O(n); probar Θ como intersección de O y Ω. |
| `S02_Complejidad_algoritmica/T03_Analisis_amortizado/README_EXTRA.md` | Prueba completa por método del potencial (Φ = 2n − capacidad) de que la inserción en vector dinámico con duplicación es O(1) amortizado. |
