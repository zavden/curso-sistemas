# Estructuras de Datos en C

Este bloque sólo debe de depender del bloque 1-4 (hasta sistemas de compilación).

## 0. Introducción y fundamentos transversales

> Estos conceptos deben introducirse al inicio del curso y aplicarse de forma
> continua a lo largo de todos los temas.

- Tipos abstractos de datos (TAD): definición y contrato
- Complejidad algorítmica: notación O, Ω y Θ
- Análisis de mejor caso, peor caso y caso promedio
- Uso de `struct`, punteros y gestión manual de memoria en C
- Herramientas de depuración: Valgrind, GDB
- Buenas prácticas: modularización, encabezados `.h` y separación interfaz/implementación

---

## 1. La pila

- Operaciones primitivas
- Operación pop
- Operación push
- Verificación de condiciones excepcionales (pila vacía, pila llena)
- Implementación con arreglo e implementación con lista vinculada en C
- Expresiones: interfija, posfija y prefija
- Conversión de expresión interfija a posfija
- Evaluación de expresiones posfijas usando pila
- Aplicaciones: verificación de paréntesis balanceados, historial de navegación

---

## 2. Recursión

- Propiedades de definiciones o algoritmos recursivos
- Casos base y casos recursivos
- Pila de llamadas y uso de memoria en recursión
- Cadenas recursivas
- Fibonacci (recursivo vs. iterativo: comparación de eficiencia)
- Búsqueda binaria recursiva
- Definición recursiva de expresiones algebraicas
- Traducción de prefija a posfija usando recursión
- Eliminación de transferencias incondicionales de control (`goto`)
- Recursión de cola (*tail recursion*) y su optimización
- Recursión mutua

---

## 3. Colas y listas

### 3.1 La cola y su representación secuencial

- La cola como tipo abstracto de datos
- Implementación en C de colas (arreglo circular)
- Operación insert y operación delete
- Cola de prioridad
- Implementación con arreglo de cola de prioridad

### 3.2 Listas vinculadas

- Lista vinculada como estructura de datos
- Inserción y remoción de nodos de una lista
- Operaciones `getnode` y `freenode`
- Implementación vinculada de pilas
- Implementación vinculada de colas
- Ejemplos de operaciones de lista
- Implementación de lista de cola de prioridades
- Nodos de encabezado (*dummy head*)

### 3.3 Listas en C

- Implementación con arreglo de listas
- Limitaciones de la implementación con arreglo
- Asignación y liberación de variables dinámicas (`malloc`, `free`, `calloc`, `realloc`)
- Listas vinculadas usando variable dinámica
- Colas como listas en C
- Ejemplos de operaciones de lista en C
- Listas no homogéneas y uso de `void *`
- Comparación de implementación dinámica y de arreglo de lista
- Puesta en práctica de nodos de encabezado

### 3.4 Ejemplo: simulación usando listas vinculadas

- Proceso de simulación
- Estructuras de datos del modelo
- Programa de simulación

### 3.5 Otras estructuras de lista

- Listas circulares
- La pila como lista circular
- Operaciones primitivas en listas circulares
- El problema de Josephus
- Nodos de encabezado en listas circulares
- Adición de enteros positivos largos usando listas circulares
- Listas doblemente vinculadas
- Adición de enteros largos usando listas doblemente vinculadas
- Comparación: lista simple vs. doble vs. circular

---

## 4. Árboles

### 4.1 Árboles binarios

- Definición, terminología y propiedades
- Operaciones sobre árboles binarios
- Aplicaciones de árboles binarios

### 4.2 Representaciones de árbol binario

- Nodos internos y externos
- Representación con arreglo implícito de árboles binarios
- Representación con punteros en C
- Elección de una representación de árbol binario
- Recorrido del árbol binario: inorden, preorden y postorden
- Recorrido por niveles (*BFS* con cola)
- Árboles binarios heterogéneos

### 4.3 Representación de listas como árboles binarios

- Localización del elemento k-ésimo
- Supresión de un elemento
- Implementación en C de listas representadas por árboles
- Construcción de una lista representada por árbol
- El problema de Josephus revisado

### 4.4 Los árboles y sus aplicaciones

- Representaciones en C de árboles generales
- Recorridos de árbol
- Expresiones generales como árboles
- Evaluación de un árbol de expresión
- Construcción de un árbol de expresión

### 4.5 Árboles AVL y árboles rojinegros *(añadido)*

- Motivación: degeneración de árboles de búsqueda binaria
- Rotaciones simples y dobles
- Árboles AVL: inserción y supresión con reequilibrado
- Árboles rojinegros: propiedades y operaciones básicas
- Comparación de desempeño

### 4.6 Ejemplo: árboles de juego

- Representación de estados de juego como árbol
- Algoritmo minimax

---

## 5. Ordenamiento

### 5.1 Antecedentes generales

- Consideraciones de eficiencia
- Notación O y análisis de complejidad aplicado a ordenamiento
- Eficiencia de ordenamiento: comparaciones vs. movimientos
- Ordenamiento estable vs. inestable
- Ordenamiento interno vs. externo

### 5.2 Ordenamiento de burbuja

- Algoritmo básico y análisis
- Variante optimizada (detención temprana)
- Quicksort
- Elección del pivote: estrategias y su impacto
- Quicksort aleatorizado
- Eficiencia de Quicksort: mejor, peor y caso promedio

### 5.3 Ordenamiento de selección y de árbol

- Ordenamiento de selección directa
- Clasificaciones con árbol binario
- Heapsort
- El montículo (*heap*) como cola de prioridad
- Ordenamiento usando un montículo
- Procedimiento heapsort completo

### 5.4 Ordenamientos de inserción

- Inserción simple
- Ordenamiento de Shell
- Ordenamiento de cálculo de dirección

### 5.5 Ordenamientos de intercalación y de raíz

- Ordenamientos de intercalación (*merge sort*)
- Merge sort externo para archivos grandes
- Algoritmo de Cook-Kim
- Ordenamiento de raíz (*radix sort*): LSD y MSD

### 5.6 Límites inferiores y consideraciones prácticas *(añadido)*

- Límite inferior Ω(n log n) para ordenamientos por comparación
- Cuándo usar cada algoritmo según el contexto
- Ordenamiento en la biblioteca estándar de C (`qsort`)

---

## 6. Búsqueda

### 6.1 Técnicas básicas de búsqueda

- El diccionario como tipo abstracto de datos
- Notación algorítmica
- Búsqueda secuencial y su eficiencia
- Reordenamiento de una lista para máxima eficiencia de búsqueda
- Búsqueda en una tabla ordenada
- Búsqueda de secuencia indizada
- Búsqueda binaria (iterativa y recursiva)
- Búsqueda de interpolación

### 6.2 Búsqueda en árboles

- Árbol de búsqueda binaria (ABB): definición y propiedades
- Inserción en un ABB
- Supresión en un ABB
- Eficiencia de operaciones en ABB
- Eficiencia en ABB no uniforme
- Árboles de búsqueda óptimos
- Árboles balanceados (AVL, rojinegros — ver sección 4.5)

### 6.3 Árboles de búsqueda general

- Árboles de búsqueda multidireccional
- Búsqueda en un árbol multidireccional
- Puesta en práctica de un árbol multidireccional
- Recorrido de un árbol multidireccional
- Inserción en un árbol de búsqueda multidireccional
- Árboles B
- Algoritmos para inserción en un árbol B
- Cómputo de `father` e `index`
- Supresión en árboles de búsqueda multidireccional
- Eficiencia de árboles de búsqueda multidireccional
- Mejoramiento del árbol B (árboles B+ y B*)
- Árboles de búsqueda digital (*tries*)
- Cujes (*Patricia tries*)

### 6.4 Dispersión (Hashing)

- Funciones de dispersión: división, multiplicación, plegamiento
- Elección de una función de dispersión
- Resolución de conflictos mediante direccionamiento abierto
  - Prueba lineal, cuadrática y doble dispersión
- Supresión de elementos de una tabla de dispersión
- Eficiencia de métodos de redispersión
- Reordenamiento de una tabla de dispersión
- Método de Brent
- Dispersión de árbol binario
- Mejoramiento con memoria adicional
- Dispersión fundida (*coalesced hashing*)
- Encadenamiento separado
- Dispersión en almacenamiento externo
- Método separador
- Dispersión dinámica y dispersión extendible
- Dispersión lineal
- Función de dispersión perfecta
- Clases universales de funciones de dispersión

---

## 7. Las gráficas y sus aplicaciones

### 7.1 Gráficas: definición y representación

- Terminología: vértices, aristas, grado, camino, ciclo, componente
- Gráficas dirigidas y no dirigidas
- Aplicaciones de gráficas
- Representación con matriz de adyacencia en C
- Representación con lista de adyacencia en C
- Comparación de representaciones

### 7.2 Cierre transitivo y rutas

- Cierre transitivo
- Algoritmo de Warshall
- Algoritmo de ruta más corta (Bellman-Ford)

### 7.3 Un problema de flujo

- Mejoramiento de una función de flujo
- Ejemplo de flujo máximo
- Algoritmo de Ford-Fulkerson y programa en C

### 7.4 Representación vinculada de gráfica

- Algoritmo de Dijkstra revisado
- Organización del conjunto de nodos de gráfica
- Aplicación a la planificación
- Programa en C

### 7.5 Recorrido de gráfica y bosques generadores

- Métodos de recorrido para gráficas
- Recorrido de primera profundidad (DFS)
- Aplicaciones de DFS: detección de ciclos, componentes fuertemente conexos
- Recorrido de primera amplitud (BFS)
- Árboles generadores mínimos
- Algoritmo de Kruskal
- Algoritmo de Prim *(añadido como complemento a Kruskal)*
- Algoritmo de todos contra todos (Floyd-Warshall)

### 7.6 Ordenamiento topológico *(añadido)*

- Grafos acíclicos dirigidos (DAG)
- Algoritmo de Kahn
- Aplicaciones: dependencias entre tareas, compilación

---

## 8. Administración de almacenamiento

### 8.1 Listas generales

- Operaciones que modifican una lista
- Ejemplos
- Representación como lista vinculada de una lista
- Representación de listas
- Operación `crlist`
- Uso de encabezados de lista
- Liberación de nodos de lista
- Listas generales en C
- Los lenguajes de programación y las listas

### 8.2 Administración automática de listas

- Método de conteo de referencias
- Recuperación de basura (*garbage collection*)
- Algoritmos para recuperación de basura
- Recuperación y compactación
- Variaciones de recuperación de basura

### 8.3 Administración dinámica de memoria

- Compactación de bloques de almacenamiento
- Primer ajuste, mejor ajuste y peor ajuste
- Liberación de bloques de almacenamiento
- Método de etiqueta de límite
- Sistema compañero (*buddy system*)
- Otros sistemas compañeros

### 8.4 Errores comunes y herramientas en C *(añadido)*

- Fugas de memoria (*memory leaks*)
- Accesos fuera de límites y punteros colgantes
- Uso de Valgrind para detección de errores de memoria
- Estrategias de prueba y depuración de estructuras dinámicas

---

## Apéndice: Temas complementarios sugeridos

- Tablas de símbolos y su relación con diccionarios y dispersión
- Estructuras de datos persistentes (introducción)
- Estructuras de datos en contextos concurrentes (introducción)
- Comparativa general: cuándo usar cada estructura de datos
