# C11 — Grafos

**Objetivo**: Dominar la representación de grafos (matriz y lista de adyacencia),
recorridos (BFS, DFS), caminos más cortos (Dijkstra, Bellman-Ford, Floyd-Warshall),
árboles generadores mínimos (Kruskal, Prim), ordenamiento topológico, y flujo
en redes.

**Prerequisitos**: C04 (colas — BFS usa cola), C05 (listas — lista de adyacencia),
C07 (heaps — Dijkstra y Prim usan cola de prioridad), C08 (ordenamiento — Kruskal
ordena aristas).

---

## S01 — Definición y representación

| Tópico | Contenido |
|--------|-----------|
| **T01 — Terminología** | Vértices, aristas, dirigido/no dirigido, ponderado, grado, camino, ciclo, componente conexo, bipartito. |
| **T02 — Matriz de adyacencia** | Array 2D, O(1) para arista, O(V²) espacio. Ventajas para grafos densos. |
| **T03 — Lista de adyacencia** | Array de listas (o `Vec`s), O(V + E) espacio. Ventajas para grafos dispersos. |
| **T04 — Representación en Rust** | `Vec<Vec<usize>>`, `HashMap<usize, Vec<(usize, Weight)>>`, crate `petgraph`. |
| **T05 — Comparación** | Tabla: espacio, verificar arista, listar vecinos, agregar arista. |

**Ejercicios**: Grafo con matriz y lista en C. Leer grafo desde archivo. Implementar
en Rust con `Vec<Vec<usize>>`.

---

## S02 — Recorridos

| Tópico | Contenido |
|--------|-----------|
| **T01 — BFS** | Por niveles con cola, árbol BFS, distancias (camino más corto no ponderado). En C y Rust. |
| **T02 — DFS** | En profundidad con pila/recursión, tiempos de descubrimiento/finalización, clasificación de aristas. |
| **T03 — Aplicaciones de DFS** | Detección de ciclos, componentes conexos, ordenamiento topológico (con DFS). |
| **T04 — Componentes fuertemente conexos** | Kosaraju o Tarjan en grafo dirigido. |

**Ejercicios**: BFS y DFS en C y Rust (grafo 20+ nodos). Detectar ciclos en
grafo dirigido. Componentes conexos en no dirigido.

---

## S03 — Caminos más cortos

| Tópico | Contenido |
|--------|-----------|
| **T01 — Dijkstra** | Desde una fuente, cola de prioridad (heap). O((V + E) log V). Sin pesos negativos. ▸ **Demostración formal**: prueba de correctitud completa — invariante: al extraer u del heap, dist[u] es la distancia mínima real; prueba por contradicción (si existiera camino más corto vía vértice no procesado x, entonces dist[x] ≥ dist[u] y w ≥ 0 implica que ese camino no es más corto). |
| **T02 — Bellman-Ford** | Pesos negativos permitidos, detecta ciclos negativos. O(V · E). ▸ **Demostración formal**: prueba por inducción — después de i iteraciones, dist[v] es la distancia más corta usando a lo sumo i aristas (base: dist[s]=0; paso: relajar todas las aristas extiende en una arista); después de V−1 iteraciones, correcto para todos los caminos simples. |
| **T03 — Floyd-Warshall** | Todos los pares. O(V³). Programación dinámica. |
| **T04 — Comparación** | Cuándo usar cada uno (tabla con restricciones y complejidades). |

**Ejercicios**: Dijkstra para mapa de ciudades en C. Bellman-Ford con ciclo negativo.
Floyd-Warshall para grafo pequeño. Dijkstra en Rust con `BinaryHeap`.

---

## S04 — Árboles generadores y topológico

| Tópico | Contenido |
|--------|-----------|
| **T01 — Árbol generador mínimo (MST)** | Definición, aplicaciones (diseño de redes). |
| **T02 — Kruskal** | Ordenar aristas, agregar si no ciclo. Union-Find como auxiliar. ▸ **Demostración formal**: probar la propiedad de corte (la arista más ligera que cruza cualquier corte pertenece a algún MST) y usarla para probar que Kruskal produce un MST (cada arista añadida es la más ligera que cruza el corte entre sus dos componentes). |
| **T03 — Prim** | Crecer MST desde un vértice con cola de prioridad. ▸ **Demostración formal**: probar correctitud por la misma propiedad de corte (en cada paso, la arista elegida es la más ligera que cruza el corte entre S y V−S). |
| **T04 — Ordenamiento topológico** | DAGs, algoritmo de Kahn (BFS), DFS post-order. Dependencias de tareas. |

**Ejercicios**: Kruskal con Union-Find en C. Prim con heap en C. Topológico de DAG
de dependencias. Union-Find en Rust.

---

## S05 — Flujo en redes (opcional)

| Tópico | Contenido |
|--------|-----------|
| **T01 — Redes de flujo** | Fuente, sumidero, capacidad, flujo. Flujo máximo. |
| **T02 — Ford-Fulkerson** | Caminos aumentantes, Edmonds-Karp (BFS). O(V · E²). |
| **T03 — Aplicaciones** | Matching bipartito, corte mínimo, asignación de recursos. |

**Ejercicios**: Ford-Fulkerson con BFS en C. Matching bipartito.

---

## Demostraciones formales pendientes (README_EXTRA.md)

| Ubicación | Contenido |
|-----------|-----------|
| `S03_Caminos_mas_cortos/T01_Dijkstra/README_EXTRA.md` | Prueba de correctitud — invariante: al extraer u del heap, dist[u] es la distancia mínima real; prueba por contradicción (si existiera camino más corto vía vértice no procesado x, w ≥ 0 implica que ese camino no es más corto). |
| `S03_Caminos_mas_cortos/T02_Bellman_Ford/README_EXTRA.md` | Prueba por inducción — después de i iteraciones, dist[v] es la distancia más corta usando a lo sumo i aristas; después de V−1 iteraciones, correcto para todos los caminos simples. |
| `S04_Arboles_generadores_y_topologico/T02_Kruskal/README_EXTRA.md` | Probar la propiedad de corte (la arista más ligera que cruza cualquier corte pertenece a algún MST) y usarla para probar que Kruskal produce un MST. |
| `S04_Arboles_generadores_y_topologico/T03_Prim/README_EXTRA.md` | Probar correctitud por la misma propiedad de corte (en cada paso, la arista elegida es la más ligera que cruza el corte entre S y V−S). |
