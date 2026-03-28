# T01 — Definición y contrato de un TAD

---

## 1. ¿Qué es un Tipo Abstracto de Datos?

Un **Tipo Abstracto de Datos** (TAD) es un modelo matemático para un tipo de dato
definido exclusivamente por las **operaciones** que se pueden realizar sobre él y
el **comportamiento** observable de esas operaciones — no por cómo se almacena
internamente.

La idea central: el usuario del TAD conoce **qué** puede hacer, pero no **cómo**
se hace por dentro.

```
         ┌─────────────────────────────────────┐
         │           TAD "Conjunto"             │
         │                                      │
         │  Operaciones visibles:               │
         │    crear()      → Conjunto vacío     │
         │    insertar(e)  → agrega e           │
         │    eliminar(e)  → quita e            │
         │    pertenece(e) → sí/no              │
         │    tamaño()     → cantidad            │
         │    destruir()   → libera recursos    │
         │                                      │
         │  Implementación oculta:              │
         │    ¿Array? ¿Lista? ¿Árbol? ¿Hash?   │
         │    El usuario no necesita saberlo.    │
         └─────────────────────────────────────┘
```

El término fue formalizado por Barbara Liskov y Stephen Zilles (1974) en el
paper "Programming with Abstract Data Types". La idea: los programas se deben
escribir en función de las propiedades lógicas de los datos, no de su
representación concreta.

### TAD vs tipo primitivo

Los tipos primitivos (`int`, `float`, `char`) también son abstracciones — `int`
no expone si internamente es complemento a dos, y las operaciones (`+`, `-`, `*`)
están definidas por su comportamiento. Un TAD extiende esta filosofía a tipos
complejos definidos por el programador.

### TAD vs estructura de datos

Estos términos se confunden constantemente. La distinción es fundamental:

| | TAD | Estructura de datos |
|---|-----|---------------------|
| **Nivel** | Lógico / conceptual | Físico / de implementación |
| **Define** | Qué operaciones existen y cómo se comportan | Cómo se organizan los datos en memoria |
| **Ejemplo** | "Lista: secuencia ordenada con inserción en posición" | "Lista enlazada simple con nodos y punteros next" |
| **Puede cambiar** | No (es la especificación) | Sí (se puede reemplazar sin afectar al usuario) |

Un TAD puede tener múltiples implementaciones:

```
TAD "Diccionario" (mapear clave → valor)
   ├── Implementación 1: Array ordenado + búsqueda binaria
   ├── Implementación 2: Tabla hash con chaining
   ├── Implementación 3: Árbol AVL
   └── Implementación 4: Trie (si las claves son strings)

Todas respetan el mismo contrato:
  insertar(clave, valor), buscar(clave) → valor, eliminar(clave)
```

---

## 2. La separación interfaz / implementación

Esta es la propiedad más importante de un TAD: la **barrera de abstracción**.

```
    Código que USA el TAD          Barrera            Código que IMPLEMENTA el TAD
  ┌──────────────────────┐    ┌───────────┐    ┌────────────────────────────────┐
  │                      │    │           │    │                                │
  │  set_insert(s, 42);  │───▶│ INTERFAZ  │───▶│  // buscar posición en array   │
  │  set_contains(s, 42);│    │           │    │  // mover elementos            │
  │  set_remove(s, 42);  │    │ Operaciones│   │  // redimensionar si necesario  │
  │                      │    │ + contrato │   │                                │
  └──────────────────────┘    └───────────┘    └────────────────────────────────┘
        No sabe cómo                              Puede cambiar sin afectar
        funciona dentro                           al código de la izquierda
```

### ¿Por qué importa?

1. **Modularidad**: se puede desarrollar y testear la implementación independientemente
   del código que la usa.

2. **Reemplazabilidad**: si la tabla hash es lenta para tu caso de uso, la cambias
   por un árbol AVL — el código cliente no cambia ni una línea.

3. **Razonamiento local**: para usar un TAD solo necesitas conocer su interfaz.
   Para implementarlo solo necesitas respetar el contrato. Ninguno necesita saber
   del otro.

4. **Evolución**: la implementación puede mejorar (optimizaciones, correcciones)
   sin romper el código que depende de ella.

### Ejemplo concreto de reemplazabilidad

Supón que tienes un programa que usa un TAD "Cola de prioridad":

```
programa:
    pq = priority_queue_create()
    priority_queue_insert(pq, tarea_A, prioridad=3)
    priority_queue_insert(pq, tarea_B, prioridad=1)
    priority_queue_insert(pq, tarea_C, prioridad=2)
    while not priority_queue_empty(pq):
        tarea = priority_queue_extract_min(pq)
        ejecutar(tarea)
```

Este programa funciona igual si la cola de prioridad está implementada con:

| Implementación | insert | extract_min | Cuándo conviene |
|----------------|--------|-------------|-----------------|
| Array desordenado | O(1) | O(n) | Pocas extracciones |
| Array ordenado | O(n) | O(1) | Muchas extracciones |
| Heap binario | O(log n) | O(log n) | Caso general |
| Heap de Fibonacci | O(1) amortizado | O(log n) amortizado | Dijkstra |

El programa no cambia. Solo la implementación detrás de la barrera.

---

## 3. Clasificación de operaciones

Las operaciones de un TAD se clasifican según su rol:

### Constructores

Crean instancias del TAD.

```
crear()           → TAD vacío
crear_desde(datos) → TAD con datos iniciales
copiar(otro)       → TAD copia
```

Un TAD siempre necesita al menos un constructor.

### Modificadores (mutadores)

Alteran el estado del TAD.

```
insertar(elemento)
eliminar(elemento)
vaciar()
```

Un TAD **inmutable** no tiene modificadores — cada "cambio" crea una nueva
instancia. Esto es común en programación funcional.

### Observadores (consultas)

Examinan el estado sin modificarlo.

```
pertenece(elemento) → bool
tamaño()            → entero
esta_vacio()        → bool
obtener(posición)   → elemento
```

Los observadores son lo que distingue un TAD de una caja negra opaca: permiten
verificar el estado sin exponer la representación.

### Iteradores

Permiten recorrer los elementos.

```
primero()   → posición al inicio
siguiente() → avanzar posición
actual()    → elemento en posición
fin()       → ¿se acabaron los elementos?
```

### Destructores

Liberan los recursos del TAD.

```
destruir()  → libera memoria, cierra archivos, etc.
```

En C esto es explícito (`set_destroy(s)`). En Rust es automático vía `Drop`.

### Tabla de clasificación para el TAD "Pila"

```
TAD Pila:
  ┌─────────────┬──────────────┬──────────────┐
  │ Categoría   │ Operación    │ Efecto       │
  ├─────────────┼──────────────┼──────────────┤
  │ Constructor │ crear()      │ → pila vacía │
  │ Modificador │ push(e)      │ agrega arriba│
  │ Modificador │ pop()        │ quita arriba │
  │ Observador  │ top()        │ ve el tope   │
  │ Observador  │ esta_vacia() │ → bool       │
  │ Observador  │ tamaño()     │ → entero     │
  │ Destructor  │ destruir()   │ libera todo  │
  └─────────────┴──────────────┴──────────────┘
```

---

## 4. El contrato

El contrato de un TAD define formalmente qué se espera de cada operación.
Tiene tres componentes: **precondiciones**, **postcondiciones** e **invariantes**.

### 4.1 Precondiciones

Condiciones que **deben** ser verdaderas **antes** de invocar una operación.
Si el llamador viola una precondición, el comportamiento es indefinido — la
implementación no tiene obligación de manejar el caso.

```
Operación: pop(pila)
  Precondición: NOT esta_vacia(pila)

  Si la pila está vacía y se llama pop:
    ┌──────────────────────────────────────────────┐
    │ Opción 1: Comportamiento indefinido (C)      │
    │   → crash, basura, corrupción silenciosa     │
    │                                              │
    │ Opción 2: Abortar (assert en C)              │
    │   → assert(!stack_empty(s)) en stack_pop()   │
    │                                              │
    │ Opción 3: Retornar error (Result en Rust)    │
    │   → fn pop(&mut self) -> Option<T>           │
    └──────────────────────────────────────────────┘
```

La precondición define la **responsabilidad del llamador**. Es un acuerdo:
"yo (implementación) me comprometo a hacer X siempre que tú (usuario) cumplas Y".

### 4.2 Postcondiciones

Condiciones que **deben** ser verdaderas **después** de que la operación se complete
(asumiendo que las precondiciones se cumplieron).

```
Operación: push(pila, elemento)
  Precondición: (ninguna — siempre se puede hacer push, salvo sin memoria)
  Postcondiciones:
    1. top(pila) == elemento       ← el elemento está en el tope
    2. tamaño(pila) == tamaño_anterior + 1
    3. NOT esta_vacia(pila)
```

```
Operación: insertar(conjunto, elemento)
  Precondición: (ninguna)
  Postcondiciones:
    1. pertenece(conjunto, elemento) == true
    2. Si elemento ya estaba: tamaño no cambia
    3. Si elemento no estaba: tamaño == tamaño_anterior + 1
```

Las postcondiciones definen la **responsabilidad de la implementación**.

### 4.3 Invariantes

Propiedades que **siempre** son verdaderas para toda instancia válida del TAD,
antes y después de cada operación. Son la propiedad más poderosa del contrato
porque restringen el espacio de estados posibles.

```
TAD Conjunto:
  Invariante 1: No hay duplicados
    → para todo e: count(e) <= 1
  Invariante 2: tamaño >= 0
  Invariante 3: tamaño == cantidad de elementos distintos
```

```
TAD Pila:
  Invariante 1: tamaño >= 0
  Invariante 2: Los elementos mantienen orden LIFO
    → el último en entrar es el primero en salir
  Invariante 3: push seguido de pop restaura el estado anterior
    → pop(push(pila, e)) produce la misma pila sin e
```

```
TAD BST (Árbol Binario de Búsqueda):
  Invariante: Para todo nodo N:
    → todos los nodos en el subárbol izquierdo < N
    → todos los nodos en el subárbol derecho > N

  Esta invariante es la que permite búsqueda en O(log n).
  Si una operación la viola, el BST deja de funcionar correctamente.
```

### 4.4 Contrato completo: ejemplo TAD "Pila"

```
TAD Pila<T>:

  INVARIANTES:
    I1: tamaño() >= 0
    I2: LIFO — el último insertado es el primero extraído

  OPERACIONES:

    crear() → Pila
      Pre:  (ninguna)
      Post: esta_vacia() == true
            tamaño() == 0

    push(e: T)
      Pre:  (ninguna, salvo memoria disponible)
      Post: top() == e
            tamaño() == tamaño_anterior + 1
            esta_vacia() == false

    pop() → T
      Pre:  NOT esta_vacia()
      Post: tamaño() == tamaño_anterior - 1
            retorna el elemento que era top()
            el nuevo top() es el penúltimo insertado

    top() → T
      Pre:  NOT esta_vacia()
      Post: (sin cambios en la pila)
            retorna el último elemento insertado

    esta_vacia() → bool
      Pre:  (ninguna)
      Post: retorna (tamaño() == 0)

    tamaño() → entero
      Pre:  (ninguna)
      Post: retorna cantidad de elementos

    destruir()
      Pre:  (ninguna)
      Post: todos los recursos liberados
            la pila no debe usarse después
```

### 4.5 ¿Y si se viola el contrato?

Hay tres filosofías según el lenguaje y el contexto:

| Estrategia | Cuándo | Ejemplo |
|------------|--------|---------|
| **Comportamiento indefinido** | C, precondición viola contrato | `pop()` en pila vacía → crash o basura |
| **Verificar y abortar** | C con asserts, modo debug | `assert(!stack_empty(s))` en `stack_pop()` |
| **Codificar en el tipo** | Rust con `Option`/`Result` | `fn pop(&mut self) -> Option<T>` — `None` si vacía |

En C, la convención predominante es documentar las precondiciones y confiar en que
el llamador las cumpla (verificar con `assert` en debug, eliminar en release con
`-DNDEBUG`).

En Rust, el sistema de tipos puede codificar parte del contrato. `Option<T>`
como retorno de `pop()` obliga al llamador a manejar el caso vacío — la
precondición se convierte en una comprobación obligatoria.

---

## 5. Ejemplo completo: TAD "Conjunto"

### 5.1 Especificación abstracta

```
TAD Conjunto<T> donde T soporta igualdad:

  INVARIANTES:
    I1: No hay elementos duplicados
    I2: tamaño() >= 0
    I3: tamaño() == |{e : pertenece(e)}|

  OPERACIONES:

    crear() → Conjunto
      Pre:  (ninguna)
      Post: esta_vacio() == true

    insertar(e: T)
      Pre:  (ninguna)
      Post: pertenece(e) == true
            Si ya pertenecía: tamaño() sin cambio
            Si no pertenecía: tamaño() == anterior + 1

    eliminar(e: T)
      Pre:  (ninguna)
      Post: pertenece(e) == false
            Si pertenecía: tamaño() == anterior - 1
            Si no pertenecía: tamaño() sin cambio

    pertenece(e: T) → bool
      Pre:  (ninguna)
      Post: retorna true si e fue insertado y no eliminado

    tamaño() → entero
      Pre:  (ninguna)
      Post: retorna cantidad de elementos distintos

    esta_vacio() → bool
      Pre:  (ninguna)
      Post: retorna (tamaño() == 0)

    destruir()
      Pre:  (ninguna)
      Post: recursos liberados
```

### 5.2 Posibles implementaciones

La especificación anterior no dice nada sobre arrays, listas, árboles ni hashes.
Cualquiera de estas implementaciones es válida si respeta el contrato:

```
                        TAD Conjunto
                             │
          ┌──────────────────┼──────────────────┐
          │                  │                  │
     Array ordenado     Lista enlazada     Tabla hash
          │                  │                  │
  insertar: O(n)      insertar: O(n)*     insertar: O(1)*
  pertenece: O(log n)  pertenece: O(n)    pertenece: O(1)*
  eliminar: O(n)       eliminar: O(n)     eliminar: O(1)*
  espacio: O(n)        espacio: O(n)      espacio: O(n)

  * amortizado / promedio
  * lista: O(n) inserción por verificar duplicados
```

El usuario del TAD escribe `set_insert(s, 42)` y no le importa cuál de las
tres implementaciones está detrás. Si mañana se descubre que la tabla hash
tiene problemas de rendimiento para su caso, se reemplaza por el array
ordenado sin cambiar el código cliente.

### 5.3 Verificación de invariantes

Las invariantes se pueden verificar programáticamente durante el desarrollo:

```c
// Pseudocódigo — verificar invariante "sin duplicados"
bool set_check_invariant(Set *s) {
    for (int i = 0; i < s->size; i++) {
        for (int j = i + 1; j < s->size; j++) {
            if (s->elements[i] == s->elements[j]) {
                return false;  // duplicado encontrado — invariante violada
            }
        }
    }
    return true;
}

// Usar en debug:
void set_insert(Set *s, int elem) {
    // ... implementación ...
    assert(set_check_invariant(s));  // verificar después de modificar
}
```

Esta verificación es O(n²) y solo se usa en desarrollo. En producción se compila
con `-DNDEBUG` para eliminar los `assert`.

---

## 6. TADs fundamentales del curso

A lo largo de B15 se implementarán estos TADs. Cada uno tiene su contrato
y múltiples implementaciones posibles:

| TAD | Operaciones clave | Implementaciones en B15 |
|-----|-------------------|------------------------|
| **Pila** | push, pop, top | Array (C02), lista enlazada (C05) |
| **Cola** | enqueue, dequeue, front | Array circular (C04), lista (C05) |
| **Deque** | push/pop en ambos extremos | Array circular, lista doble |
| **Lista** | insertar(pos), eliminar(pos), obtener(pos) | Enlazada simple, doble, circular (C05) |
| **Conjunto** | insertar, eliminar, pertenece | Array, BST (C06), hash (C10) |
| **Diccionario** | insertar(k,v), buscar(k), eliminar(k) | BST, AVL, hash, trie |
| **Cola de prioridad** | insertar(e, prio), extraer_min | Heap binario (C07) |
| **Grafo** | agregar_arista, vecinos, BFS, DFS | Matriz, lista de adyacencia (C11) |

---

## 7. Errores conceptuales comunes

### "TAD" = "struct"

Un struct es una representación concreta. Un TAD es la especificación abstracta.
Un mismo TAD puede implementarse con diferentes structs, o sin structs.

### "El TAD dicta la complejidad"

No. El TAD define operaciones y comportamiento. La complejidad depende de la
**implementación** elegida. El TAD "Conjunto" no es O(1) ni O(log n) — la
tabla hash es O(1) promedio, el BST es O(log n).

### "Encapsulación = TAD"

Encapsulación (ocultar campos) es un **mecanismo** para implementar la barrera
de abstracción de un TAD, pero no es suficiente. Un TAD también requiere un
contrato (precondiciones, postcondiciones, invariantes). Un struct con campos
privados pero sin contrato definido no es un TAD — es solo un struct encapsulado.

### "Si el test pasa, el contrato se cumple"

Los tests verifican casos específicos. El contrato es una propiedad universal
("para todo elemento insertado, pertenece() retorna true"). Los tests pueden
fallar en cubrir casos borde. Las invariantes deben razonarse, no solo testearse.

---

## Ejercicios

### Ejercicio 1 — Identificar TAD vs estructura de datos

Para cada par, indica cuál es el TAD y cuál es la estructura de datos:

```
a) "Cola" vs "Array circular con head y tail"
b) "Lista enlazada simple" vs "Secuencia con inserción por posición"
c) "Tabla hash con chaining" vs "Diccionario"
d) "Heap binario" vs "Cola de prioridad"
```

**Predicción**: Antes de ver la respuesta, clasifica cada uno y justifica
en una oración por qué.

<details><summary>Respuesta</summary>

| Par | TAD | Estructura de datos |
|-----|-----|---------------------|
| a) | Cola | Array circular con head y tail |
| b) | Secuencia con inserción por posición | Lista enlazada simple |
| c) | Diccionario | Tabla hash con chaining |
| d) | Cola de prioridad | Heap binario |

**Criterio**: El TAD define el **qué** (operaciones + comportamiento).
La estructura define el **cómo** (organización en memoria).

- "Cola" dice qué hacer (enqueue, dequeue, FIFO). "Array circular" dice cómo.
- "Secuencia con inserción por posición" es el contrato. "Lista enlazada" es la implementación.
- "Diccionario" (clave→valor) es el TAD. "Hash con chaining" es una forma de implementarlo.
- "Cola de prioridad" (extraer mínimo) es el TAD. "Heap binario" es la estructura.

</details>

---

### Ejercicio 2 — Escribir precondiciones y postcondiciones

Dado el siguiente TAD parcial:

```
TAD Cola<T>:
  crear()         → Cola vacía
  enqueue(e: T)   → agrega e al final
  dequeue()       → quita y retorna el primero
  front()         → retorna el primero sin quitar
  esta_vacia()    → bool
  tamaño()        → entero
```

Escribe las precondiciones y postcondiciones de `dequeue()` y `enqueue()`.

**Predicción**: Antes de ver la respuesta, piensa: ¿cuáles operaciones tienen
precondiciones no triviales? ¿Qué relación hay entre `enqueue` y el resultado
de `front()`?

<details><summary>Respuesta</summary>

```
enqueue(e: T):
  Pre:  (ninguna, salvo memoria disponible)
  Post: tamaño() == tamaño_anterior + 1
        esta_vacia() == false
        Si la cola estaba vacía: front() == e
        Si no estaba vacía: front() == front_anterior  ← no cambia el frente

dequeue() → T:
  Pre:  NOT esta_vacia()  ← precondición no trivial
  Post: tamaño() == tamaño_anterior - 1
        retorna el elemento que era front()
        Si queda un elemento: front() == siguiente en orden FIFO
        Si queda vacía: esta_vacia() == true
```

La precondición clave es `NOT esta_vacia()` para `dequeue()` y `front()`.
`enqueue()` normalmente no tiene precondiciones (excepto falta de memoria,
que en C se maneja con NULL y en Rust con el allocator).

La postcondición sutil de `enqueue` es que **no cambia el frente** si la cola
ya tenía elementos — el nuevo elemento va al final, no al inicio.

</details>

---

### Ejercicio 3 — Encontrar la invariante violada

Observa estas secuencias de operaciones sobre un TAD "Conjunto" y determina si
alguna viola una invariante:

```
Secuencia A:
  s = crear()
  insertar(s, 5)
  insertar(s, 3)
  insertar(s, 5)    // insertar duplicado
  // Estado: ¿{3, 5} o {3, 5, 5}?

Secuencia B:
  s = crear()
  insertar(s, 10)
  eliminar(s, 10)
  eliminar(s, 10)   // eliminar algo que no está
  // Estado: ¿vacío, error, o indefinido?
```

**Predicción**: ¿Cuál es el estado correcto después de cada secuencia?
¿Alguna operación debería fallar?

<details><summary>Respuesta</summary>

**Secuencia A**: El estado correcto es `{3, 5}`.

La invariante del conjunto dice: **no hay duplicados**. `insertar(s, 5)` la
segunda vez debe verificar si 5 ya pertenece y no agregarlo de nuevo.
Si la implementación agregara un duplicado (`{3, 5, 5}`), estaría violando
la invariante — sería un bug de implementación, no un error del usuario.

El `tamaño()` debe ser 2, no 3.

**Secuencia B**: El estado correcto es `{}` (vacío).

`eliminar(s, 10)` la segunda vez no viola ninguna precondición (la postcondición
de `eliminar` dice "si no pertenecía: tamaño sin cambio"). Es una operación
válida que simplemente no hace nada. No es un error.

Ninguna operación de las dos secuencias debe fallar. Ambas son uso legítimo
del TAD. La implementación debe manejar ambos casos correctamente.

</details>

---

### Ejercicio 4 — Múltiples implementaciones, mismo contrato

Diseña en pseudocódigo dos implementaciones del TAD "Bolsa" (multiset — como
un conjunto pero permite duplicados):

```
TAD Bolsa<T>:
  crear()           → Bolsa vacía
  agregar(e: T)     → agrega e (puede haber duplicados)
  quitar(e: T)      → quita UNA ocurrencia de e
  contar(e: T)      → cuántas veces aparece e
  tamaño()          → total de elementos (con repeticiones)
```

- **Implementación A**: Array dinámico (los elementos se guardan uno por uno)
- **Implementación B**: Array de pares (elemento, cantidad)

**Predicción**: Antes de escribir, piensa: ¿cuál de las dos es más eficiente
para `contar()`? ¿Y para `agregar()`? ¿En cuál es más fácil mantener las
invariantes?

<details><summary>Respuesta</summary>

**Implementación A — Array de elementos**:

```
Bag_A:
  data: array dinámico de T
  size: entero

  agregar(e):
    data[size] = e
    size++
    // O(1) amortizado — sin verificación, se permite duplicar

  quitar(e):
    i = buscar primera ocurrencia de e en data
    si i encontrado:
      data[i] = data[size-1]    // swap con último
      size--
    // O(n) — búsqueda lineal

  contar(e):
    c = 0
    para cada x en data:
      si x == e: c++
    retornar c
    // O(n) — recorrer todo

  tamaño():
    retornar size
    // O(1)
```

**Implementación B — Array de pares (elemento, cantidad)**:

```
Bag_B:
  entries: array dinámico de (T, int)
  total: entero

  agregar(e):
    i = buscar e en entries
    si i encontrado:
      entries[i].count++
    si no:
      entries.append((e, 1))
    total++
    // O(n) — búsqueda para verificar existencia

  quitar(e):
    i = buscar e en entries
    si i encontrado y entries[i].count > 0:
      entries[i].count--
      total--
      si entries[i].count == 0:
        eliminar entries[i]
    // O(n) — búsqueda

  contar(e):
    i = buscar e en entries
    si i encontrado:
      retornar entries[i].count
    retornar 0
    // O(n) en esta versión, pero O(1) si se usa hash

  tamaño():
    retornar total
    // O(1)
```

**Comparación**:

| Operación | A (array plano) | B (pares) |
|-----------|----------------|-----------|
| agregar | O(1) amortizado | O(n) |
| contar | O(n) | O(n)* |
| quitar | O(n) | O(n) |
| espacio con muchos duplicados | O(total) | O(distintos) |

`agregar()` es más eficiente en A (no necesita buscar). B gana en espacio
cuando hay muchos duplicados (guarda "manzana×1000" en vez de 1000 entradas).
La invariante de B es más compleja: `count > 0` para toda entrada, y
`sum(counts) == total`.

</details>

---

### Ejercicio 5 — Contrato de un TAD "String"

Define el contrato completo (invariantes, operaciones con pre/postcondiciones)
para un TAD simplificado de cadena de caracteres:

```
TAD String:
  crear(texto)       → String con contenido
  crear_vacia()      → String vacía
  longitud()         → entero
  caracter(pos)      → char en posición pos
  concatenar(otra)   → nueva String unida
  subcadena(ini, n)  → nueva String con n chars desde ini
  destruir()
```

**Predicción**: ¿Cuáles operaciones necesitan precondiciones? ¿Cuál es la
invariante más importante?

<details><summary>Respuesta</summary>

```
TAD String:

  INVARIANTES:
    I1: longitud() >= 0
    I2: Las posiciones válidas son [0, longitud()-1]
    I3: El contenido es inmutable (concatenar/subcadena crean nuevas instancias)

  crear(texto: char[]) → String
    Pre:  texto no es NULL
    Post: longitud() == strlen(texto)
          caracter(i) == texto[i] para todo i en [0, longitud()-1]

  crear_vacia() → String
    Pre:  (ninguna)
    Post: longitud() == 0

  longitud() → entero
    Pre:  (ninguna)
    Post: retorna cantidad de caracteres

  caracter(pos: entero) → char
    Pre:  0 <= pos < longitud()        ← precondición
    Post: retorna el carácter en posición pos

  concatenar(otra: String) → String
    Pre:  otra no es NULL
    Post: resultado.longitud() == this.longitud() + otra.longitud()
          resultado.caracter(i) == this.caracter(i) para i < this.longitud()
          resultado.caracter(this.longitud()+j) == otra.caracter(j)

  subcadena(ini: entero, n: entero) → String
    Pre:  0 <= ini
          ini + n <= longitud()        ← precondiciones
          n >= 0
    Post: resultado.longitud() == n
          resultado.caracter(i) == this.caracter(ini + i) para i en [0, n-1]

  destruir()
    Pre:  (ninguna)
    Post: recursos liberados
```

Las operaciones que necesitan precondiciones son `caracter()` (posición válida),
`subcadena()` (rango válido) y `crear()` (texto no NULL).

La invariante más importante es I2 (posiciones válidas en `[0, longitud()-1]`),
porque toda operación que indexa depende de ella. En C, violar esto es buffer
overflow. En Rust, produce un panic.

</details>

---

### Ejercicio 6 — Operaciones que rompen invariantes

Un programador implementa el TAD "Conjunto ordenado" (los elementos siempre
están en orden ascendente) con un array ordenado. Identifica cuál invariante
se rompe en cada caso:

```c
// Caso A: insertar sin buscar posición correcta
void sorted_set_insert(SortedSet *s, int elem) {
    s->data[s->size] = elem;   // agrega al final
    s->size++;
}

// Caso B: eliminar con swap del último
void sorted_set_remove(SortedSet *s, int elem) {
    int i = find_index(s, elem);
    if (i >= 0) {
        s->data[i] = s->data[s->size - 1];  // swap con último
        s->size--;
    }
}

// Caso C: insertar sin verificar duplicados
void sorted_set_insert_v2(SortedSet *s, int elem) {
    int pos = find_insert_position(s, elem);  // búsqueda binaria
    shift_right(s, pos);
    s->data[pos] = elem;
    s->size++;
}
```

**Predicción**: ¿Qué invariante se viola en cada caso? ¿Hay algún caso que
viola dos invariantes a la vez?

<details><summary>Respuesta</summary>

**Caso A**: Viola la invariante de **orden**.

Insertar al final sin buscar la posición correcta pone el elemento fuera de
orden. Si el conjunto era `{1, 3, 5}` y se inserta `2`, queda `{1, 3, 5, 2}`.
La búsqueda binaria (que depende del orden) dará resultados incorrectos.

**Caso B**: Viola la invariante de **orden**.

Hacer swap con el último elemento destruye el orden. Si el conjunto era
`{1, 3, 5, 7}` y se elimina `3`, queda `{1, 7, 5}` — desordenado. La
forma correcta es shift a la izquierda: `{1, 5, 7}`.

**Caso C**: Viola la invariante de **no duplicados**.

Encuentra la posición correcta (mantiene orden) pero no verifica si el
elemento ya existe. Si se inserta `3` en `{1, 3, 5}`, queda `{1, 3, 3, 5}`.
La solución: verificar en `find_insert_position` si `data[pos] == elem`
y no insertar en ese caso.

Ninguno viola dos invariantes simultáneamente, pero los tres son bugs reales
que se encuentran frecuentemente en implementaciones. En un conjunto ordenado
las dos invariantes (orden y unicidad) deben mantenerse en cada operación.

</details>

---

### Ejercicio 7 — ¿Precondición o postcondición?

Clasifica cada afirmación como precondición, postcondición o invariante:

```
a) "El array siempre está ordenado de menor a mayor"
b) "El parámetro index debe estar entre 0 y tamaño-1"
c) "Después de push(e), el tope de la pila es e"
d) "El número de elementos nunca es negativo"
e) "La lista no debe estar vacía para llamar a first()"
f) "Después de clear(), tamaño() retorna 0"
g) "No hay dos nodos con la misma clave"
h) "El argumento callback no debe ser NULL"
```

**Predicción**: Clasifica las 8 afirmaciones antes de ver la respuesta.

<details><summary>Respuesta</summary>

| Afirmación | Tipo | Razón |
|------------|------|-------|
| a) Array siempre ordenado | **Invariante** | "Siempre" — propiedad global, no ligada a una operación |
| b) Index entre 0 y tamaño-1 | **Precondición** | Restricción sobre un parámetro de entrada |
| c) Después de push, tope es e | **Postcondición** | Estado resultado de una operación específica |
| d) Elementos nunca negativo | **Invariante** | "Nunca" — propiedad que debe cumplirse siempre |
| e) Lista no vacía para first() | **Precondición** | Condición requerida antes de invocar |
| f) Después de clear(), tamaño 0 | **Postcondición** | Estado resultado de clear() |
| g) No hay claves duplicadas | **Invariante** | Propiedad estructural permanente |
| h) Callback no NULL | **Precondición** | Restricción sobre un parámetro |

**Regla de identificación rápida**:
- Si dice "**antes de** llamar a..." o restringe un **parámetro** → precondición
- Si dice "**después de** X..." o describe el **resultado** → postcondición
- Si dice "**siempre**", "**nunca**", "para **todo**..." → invariante

</details>

---

### Ejercicio 8 — Diseñar un TAD desde un problema

Un sistema de atención al cliente necesita manejar tickets de soporte.
Los tickets tienen un ID, prioridad (1-5, 1 = más urgente), y estado
(abierto/cerrado). Los agentes atienden siempre el ticket abierto de
mayor prioridad (número más bajo).

Diseña el TAD (nombre, operaciones, categoría de cada operación, invariantes,
pre/postcondiciones).

**Predicción**: ¿Cuál TAD conocido se parece más a este problema? ¿Qué
operaciones adicionales necesitarás respecto al TAD base?

<details><summary>Respuesta</summary>

El TAD base es una **cola de prioridad**, pero con operaciones adicionales para
cerrar tickets y consultar por ID.

```
TAD SistemaTickets:

  INVARIANTES:
    I1: Los IDs son únicos (no hay dos tickets con el mismo ID)
    I2: La prioridad está en [1, 5]
    I3: El estado es "abierto" o "cerrado"
    I4: atender() siempre selecciona el ticket abierto de menor
        número de prioridad (más urgente)

  OPERACIONES:

    Constructor:
      crear() → SistemaTickets
        Pre:  (ninguna)
        Post: no hay tickets

    Modificadores:
      agregar_ticket(id, prioridad) → Ticket
        Pre:  id no existe en el sistema
              1 <= prioridad <= 5
        Post: ticket con id existe, estado = "abierto"
              total_abiertos() == anterior + 1

      atender() → Ticket
        Pre:  total_abiertos() > 0
        Post: retorna ticket abierto de menor prioridad
              ese ticket pasa a estado "cerrado"
              total_abiertos() == anterior - 1

      cerrar_ticket(id)
        Pre:  ticket con id existe y está abierto
        Post: ticket con id tiene estado "cerrado"
              total_abiertos() == anterior - 1

    Observadores:
      total_abiertos() → entero
        Pre:  (ninguna)
        Post: retorna cantidad de tickets con estado "abierto"

      consultar(id) → Ticket
        Pre:  ticket con id existe
        Post: retorna ticket sin modificarlo

      hay_urgentes() → bool
        Pre:  (ninguna)
        Post: retorna true si hay ticket abierto con prioridad 1

    Destructor:
      destruir()
        Pre:  (ninguna)
        Post: recursos liberados
```

Esto combina una **cola de prioridad** (atender al más urgente) con un
**diccionario** (consultar por ID). Una implementación posible: heap binario
para la cola + hash table para acceso por ID.

</details>

---

### Ejercicio 9 — Axiomas algebraicos

Los TADs se pueden especificar con **axiomas**: ecuaciones que relacionan
operaciones entre sí. Son una forma alternativa (y más formal) de definir
pre/postcondiciones.

Dado el TAD Pila con operaciones `crear`, `push`, `pop`, `top`, `esta_vacia`:

```
Axioma 1:  esta_vacia(crear()) == true
Axioma 2:  esta_vacia(push(s, e)) == false
Axioma 3:  top(push(s, e)) == e
Axioma 4:  pop(push(s, e)) == s
```

Pregunta: ¿Qué pasa si aplicamos los axiomas a esta secuencia?

```
s0 = crear()
s1 = push(s0, 'A')
s2 = push(s1, 'B')
s3 = pop(s2)

¿s3 == s1?   ¿top(s2) == ?   ¿esta_vacia(s3) == ?
```

**Predicción**: Aplica los axiomas paso a paso antes de ver la respuesta.

<details><summary>Respuesta</summary>

Aplicación paso a paso:

```
s0 = crear()
  → esta_vacia(s0) == true              [Axioma 1]

s1 = push(s0, 'A')
  → esta_vacia(s1) == false             [Axioma 2]
  → top(s1) == 'A'                      [Axioma 3]

s2 = push(s1, 'B')
  → esta_vacia(s2) == false             [Axioma 2]
  → top(s2) == 'B'                      [Axioma 3]

s3 = pop(s2) = pop(push(s1, 'B'))
  → s3 == s1                            [Axioma 4: pop(push(s, e)) == s]
```

Respuestas:
- **s3 == s1**: Sí. Por Axioma 4, `pop(push(s1, 'B'))` devuelve `s1`.
- **top(s2)**: `'B'`. Por Axioma 3, `top(push(s1, 'B'))` == `'B'`.
- **esta_vacia(s3)**: `false`. Como `s3 == s1` y `s1 = push(s0, 'A')`, por Axioma 2 no está vacía.

Los axiomas permiten razonar sobre el TAD **sin conocer la implementación**.
Son usados en verificación formal de software. El Axioma 4 captura la esencia
de LIFO: hacer push y luego pop restaura la pila al estado anterior.

</details>

---

### Ejercicio 10 — C vs Rust: ¿quién protege el contrato?

Considera esta implementación parcial de una pila en C y su equivalente en Rust:

```c
// C — stack_pop sin verificar precondición
int stack_pop(Stack *s) {
    s->size--;
    return s->data[s->size];
}
```

```rust
// Rust — dos versiones de pop
impl<T> Stack<T> {
    fn pop_unchecked(&mut self) -> T { ... }  // unsafe, no verifica
    fn pop(&mut self) -> Option<T> { ... }    // safe, retorna None si vacía
}
```

Preguntas:
1. ¿Qué pasa en C si se llama `stack_pop` con la pila vacía?
2. ¿Cuál versión de Rust corresponde a "la precondición es responsabilidad
   del llamador" y cuál a "la precondición se codifica en el tipo de retorno"?
3. ¿Cuándo usarías cada enfoque?

**Predicción**: Razona sobre qué pasa con `size--` cuando `size == 0` en C
antes de ver la respuesta.

<details><summary>Respuesta</summary>

**1. C con pila vacía**:

Si `size == 0`:
- `s->size--` → `size` se convierte en `-1` (o `SIZE_MAX` si es `unsigned`)
- `s->data[s->size]` → acceso fuera de los límites del array
- **Comportamiento indefinido**: puede retornar basura, crash, o corromper
  memoria silenciosamente

No hay ningún mecanismo en C que prevenga esto. Las opciones son:
- Documentar la precondición y confiar en el llamador
- Agregar `assert(s->size > 0)` (verifica en debug, se elimina en release)
- Verificar con `if (s->size == 0) { /* manejar error */ }`

**2. Correspondencia**:

- `pop_unchecked()` → precondición es responsabilidad del llamador (requiere
  `unsafe` para llamarla, el programador acepta el riesgo)
- `pop()` → precondición codificada en el tipo (`Option<T>` obliga a manejar
  el caso `None`)

**3. Cuándo usar cada enfoque**:

| Enfoque | Cuándo |
|---------|--------|
| Sin verificar (C, `pop_unchecked`) | Hot path donde ya verificaste antes, rendimiento crítico |
| Assert (C) | Desarrollo, tests — detectar bugs temprano |
| `Option<T>` (Rust `pop`) | API pública, código general — seguridad sobre velocidad |

En la práctica, la convención de la biblioteca estándar de Rust es:
- `pop() -> Option<T>` como interfaz principal (safe)
- No ofrecer variante unchecked para pilas — el overhead de verificar es mínimo

En C, la convención dominante es documentar la precondición y opcionalmente
usar `assert` en debug.

</details>
