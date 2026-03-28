# C10 — Tablas hash (Dispersión)

**Objetivo**: Comprender hashing como técnica de búsqueda O(1) promedio,
implementar tablas hash con chaining y open addressing en C, dominar
`HashMap`/`HashSet` en Rust, y saber cuándo NO usar hash tables.

**Prerequisitos**: C05 (listas enlazadas — chaining usa listas), C01 S02
(complejidad — análisis amortizado).

---

## S01 — Fundamentos de hashing

| Tópico | Contenido |
|--------|-----------|
| **T01 — Concepto** | Mapear claves a índices, O(1) promedio. Función hash, tabla, colisiones. |
| **T02 — Funciones hash** | División (`key % m`), multiplicación (Knuth), para strings (djb2, FNV-1a). Propiedades: uniformidad, determinismo, eficiencia. |
| **T03 — Factor de carga** | α = n/m, relación con rendimiento, cuándo redimensionar (α > 0.7 open, α > 1.0 chaining). ▸ **Demostración formal**: probar que la longitud esperada de una cadena en chaining es α (bajo hashing uniforme: cada clave cae en bucket j con prob 1/m, por linealidad de la esperanza E[cadena] = n/m = α). |
| **T04 — Colisiones** | Inevitables (principio del palomar), estrategias de resolución. |

**Ejercicios**: Función hash djb2 para strings. Visualizar distribución para 10⁴
palabras. Experimentar con diferentes tamaños de tabla.

---

## S02 — Resolución de colisiones

| Tópico | Contenido |
|--------|-----------|
| **T01 — Encadenamiento separado** | Cada bucket = lista enlazada. O(1 + α) promedio. Implementación en C. ▸ **Demostración formal**: probar que la búsqueda fallida cuesta Θ(1 + α) y la exitosa Θ(1 + α/2) en promedio (bajo hashing uniforme simple). |
| **T02 — Sonda lineal** | Siguiente slot libre. Clustering primario. ▸ **Demostración formal**: resultado de Knuth — la búsqueda exitosa en sonda lineal requiere en promedio ½(1 + 1/(1−α)) sondas y la fallida ½(1 + 1/(1−α)²) sondas (presentar resultado; la derivación completa de Knuth es opcional por su complejidad). |
| **T03 — Sonda cuadrática y doble hashing** | Reducir clustering, secuencia de sonda, h₂(k) coprimo con m. |
| **T04 — Eliminación en open addressing** | Tombstones ("deleted"), por qué no se puede vaciar (rompe cadena). |
| **T05 — Redimensionamiento** | Cuándo crecer/shrink, factor ×2, rehash total, costo amortizado O(1). ▸ **Demostración formal**: prueba amortizada de que la inserción con redimensionamiento automático (duplicar al exceder α_max) tiene costo amortizado O(1) por método agregado (n inserciones + ≤ log n rehashes de costo Σ 2ⁱ = O(n)). |

**Ejercicios**: Hash table con chaining en C (genérica con `void *`). Hash table con
sonda lineal. Comparar chaining vs lineal vs cuadrática para α = 0.5, 0.7, 0.9, 0.95.

---

## S03 — Hash tables en la práctica

| Tópico | Contenido |
|--------|-----------|
| **T01 — HashMap en Rust** | SwissTable/hashbrown, `Hash` + `Eq` traits, entry API. |
| **T02 — HashSet en Rust** | `HashMap<K, ()>`, operaciones de conjuntos (unión, intersección, diferencia). |
| **T03 — Hash tables en C** | GLib `GHashTable`, uthash (macros intrusivas), implementación propia vs librería. |
| **T04 — Hashing perfecto y universal** | Sin colisiones para conjunto fijo, familias universales, hash aleatorizado contra DoS. |
| **T05 — Cuándo NO usar hash tables** | Datos ordenados → BST/B-tree, pocos elementos → array, claves no hashables, recorrido ordenado necesario. |

**Ejercicios**: Diccionario inglés-español en C y Rust (`HashMap`). Comparar tiempos.
Contador de frecuencia de palabras en archivo de texto.

---

## Demostraciones formales pendientes (README_EXTRA.md)

| Ubicación | Contenido |
|-----------|-----------|
| `S01_Fundamentos_de_hashing/T03_Factor_de_carga/README_EXTRA.md` | Probar que la longitud esperada de una cadena en chaining es α (bajo hashing uniforme: cada clave cae en bucket j con prob 1/m, por linealidad de la esperanza E[cadena] = n/m = α). |
| `S02_Resolucion_de_colisiones/T01_Encadenamiento_separado/README_EXTRA.md` | Probar que la búsqueda fallida cuesta Θ(1 + α) y la exitosa Θ(1 + α/2) en promedio (bajo hashing uniforme simple). |
| `S02_Resolucion_de_colisiones/T02_Sonda_lineal/README_EXTRA.md` | Resultado de Knuth — búsqueda exitosa ½(1 + 1/(1−α)) sondas, fallida ½(1 + 1/(1−α)²) sondas (presentar resultado; derivación completa opcional). |
| `S02_Resolucion_de_colisiones/T05_Redimensionamiento/README_EXTRA.md` | Prueba amortizada de que la inserción con redimensionamiento (duplicar al exceder α_max) es O(1) amortizado por método agregado (n inserciones + ≤ log n rehashes de costo Σ 2ⁱ = O(n)). |
