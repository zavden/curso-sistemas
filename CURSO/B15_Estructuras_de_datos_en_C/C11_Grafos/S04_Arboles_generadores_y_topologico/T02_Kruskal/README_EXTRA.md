# T02 — Demostración formal: Kruskal produce un MST por la propiedad de corte

> Complemento riguroso del README.md. Aquí se demuestra formalmente que
> el algoritmo de Kruskal produce un árbol generador de peso mínimo
> (MST), usando la propiedad de corte y la propiedad de ciclo.

---

## Modelo

**Grafo.** $G = (V, E)$ no dirigido, conexo, con función de peso
$w: E \to \mathbb{R}$. $|V| = n$, $|E| = m$.

**Árbol generador mínimo (MST).** Un subgrafo $T \subseteq E$ que es
un árbol (conexo, acíclico) que contiene todos los vértices, y cuyo
peso total $w(T) = \sum_{e \in T} w(e)$ es mínimo.

**Convención.** Asumimos que todos los pesos son distintos. (Si hay
empates, el resultado sigue siendo correcto pero el MST puede no ser
único; la prueba se adapta con una regla de desempate consistente.)

**Corte.** Una partición $(S, V \setminus S)$ de $V$ con
$S \neq \emptyset$ y $S \neq V$. Una arista **cruza** el corte si
tiene un extremo en $S$ y otro en $V \setminus S$.

---

## Lema fundamental: propiedad de corte

**Lema (Cut Property).** Sea $(S, V \setminus S)$ un corte de $G$, y
sea $e$ la arista de **peso mínimo** que cruza el corte. Entonces $e$
pertenece a todo MST de $G$.

**Prueba por contradicción.** Sea $T^*$ un MST que no contiene $e$.
Sea $e = (u, v)$ con $u \in S$ y $v \in V \setminus S$.

Como $T^*$ es un árbol generador, existe un camino único
$p: u \leadsto v$ en $T^*$. Este camino cruza el corte $(S, V \setminus S)$
al menos una vez. Sea $e' = (x, y)$ una arista de $p$ que cruza el
corte (con $x \in S$, $y \in V \setminus S$).

Consideremos $T' = T^* \setminus \{e'\} \cup \{e\}$:

1. **$T'$ es conexo.** Eliminar $e'$ de $T^*$ desconecta $T^*$ en
   dos componentes: una conteniendo $x$ (dentro de $S$) y otra
   conteniendo $y$ (dentro de $V \setminus S$). Pero $u$ está en la
   componente de $x$ y $v$ en la de $y$, así que agregar $e = (u,v)$
   reconecta las dos componentes.

2. **$T'$ es acíclico.** $T'$ tiene $n - 1$ aristas (misma cantidad
   que $T^*$) y es conexo, por lo tanto es un árbol.

3. **$w(T') < w(T^*)$.** Como $e$ es la arista de peso mínimo que
   cruza el corte y $e'$ también cruza el corte:
   $w(e) < w(e')$ (pesos distintos). Así:

   $$w(T') = w(T^*) - w(e') + w(e) < w(T^*)$$

Esto contradice que $T^*$ es un MST.

$$\blacksquare$$

---

## Lema fundamental: propiedad de ciclo

**Lema (Cycle Property).** Sea $C$ un ciclo en $G$, y sea $e$ la
arista de **peso máximo** en $C$. Entonces $e$ no pertenece a ningún
MST.

**Prueba por contradicción.** Supongamos que $e = (u, v)$ pertenece a
un MST $T^*$. Eliminar $e$ de $T^*$ desconecta el árbol en dos
componentes $S$ (conteniendo $u$) y $V \setminus S$ (conteniendo $v$).

Como $C$ es un ciclo que pasa por $e$, el camino
$C \setminus \{e\}: u \leadsto v$ conecta $S$ con $V \setminus S$.
Existe al menos una arista $e'$ en este camino que cruza el corte
$(S, V \setminus S)$, y $e' \neq e$.

Como $e$ es la arista de peso máximo en $C$ y $e' \in C$:
$w(e') < w(e)$. Entonces $T' = T^* \setminus \{e\} \cup \{e'\}$ es
un árbol generador con $w(T') < w(T^*)$. Contradicción.

$$\blacksquare$$

---

## Demostración 1: Kruskal produce un MST

**Teorema.** El algoritmo de Kruskal retorna un MST.

**Prueba.** Demostramos dos propiedades:

**(A) Kruskal produce un árbol generador.**

El algoritmo acepta una arista solo si sus extremos están en
componentes distintas (no crea ciclo). Así, el conjunto de aristas
aceptadas es siempre un bosque (acíclico). Como $G$ es conexo y cada
arista rechazada tiene ambos extremos en la misma componente, al
procesar todas las aristas, las $n$ componentes iniciales se reducen
a 1 (o el algoritmo se detiene al aceptar $n - 1$ aristas). El
resultado es un árbol generador. ✓

**(B) Cada arista aceptada pertenece a algún MST.**

Sea $e_k = (u, v)$ la $k$-ésima arista aceptada por Kruskal. Al
momento de aceptarla, $u$ y $v$ están en componentes distintas $C_u$
y $C_v$ del bosque parcial. Consideremos el corte
$(C_u,\, V \setminus C_u)$.

La arista $e_k$ cruza este corte. Afirmamos que $e_k$ es la arista
de **peso mínimo** que cruza el corte:

- Toda arista de peso menor que $w(e_k)$ ya fue procesada (las
  aristas se procesan en orden ascendente de peso).
- Si alguna de esas aristas cruzara el corte $(C_u, V \setminus C_u)$,
  habría sido aceptada (sus extremos estaban en componentes distintas
  en ese momento — si $u' \in C_u$ y $v' \notin C_u$, necesariamente
  estaban en componentes distintas). Pero si fue aceptada, ambos
  extremos estarían ahora en la misma componente... Analicemos con
  más cuidado:

  Sea $e' = (u', v')$ con $w(e') < w(e_k)$ y $e'$ cruza el corte
  actual $(C_u, V \setminus C_u)$. Entonces $u' \in C_u$ y
  $v' \notin C_u$. Cuando $e'$ fue procesada:

  - Si $e'$ fue **aceptada**: $u'$ y $v'$ fueron unidos. Pero
    ahora $u' \in C_u$ y $v' \notin C_u$, así que $e'$ no conectó
    $u'$ a la componente de $u$... Esto es posible: $e'$ pudo unir
    $u'$ a $C_u$ antes, o $v'$ a otra componente. Lo clave es que
    si $e'$ cruzara el corte **actual**, habría unido nodos que ahora
    están en distintas componentes, contradicción con la definición de
    componente.

  Simplificamos el argumento con la siguiente observación directa:

**Argumento limpio.** Al momento de procesar $e_k$, $u \in C_u$ y
$v \in C_v$ con $C_u \neq C_v$. Toda arista $e'$ con
$w(e') < w(e_k)$ ya fue procesada. Si $e'$ cruza el corte $(C_u, V \setminus C_u)$ actual, entonces $e'$ tiene un extremo en $C_u$ y
otro fuera. Pero $e'$ fue rechazada (porque ambos extremos estaban en
la misma componente cuando se procesó). Esto significa que cuando $e'$
se procesó, ambos extremos ya estaban conectados. Pero si ahora un
extremo está en $C_u$ y el otro fuera de $C_u$, no podrían haber
estado en la misma componente entonces — porque las componentes solo
se fusionan, nunca se dividen. **Contradicción.** Así que ninguna
arista más liviana cruza el corte actual.

Por la propiedad de corte, $e_k$ pertenece a todo MST. ✓

**(A) + (B):** Kruskal produce un árbol generador cuyas $n - 1$
aristas pertenecen cada una a algún MST. Como los pesos son distintos,
el MST es único, y las $n - 1$ aristas forman ese MST.

$$\blacksquare$$

---

## Demostración 2: cada arista rechazada no está en ningún MST

**Teorema.** Toda arista rechazada por Kruskal no pertenece a ningún
MST.

**Prueba.** Sea $e = (u, v)$ una arista rechazada. Al momento de
procesarla, $u$ y $v$ están en la misma componente $C$. Esto
significa que existe un camino $p: u \leadsto v$ formado por aristas
ya aceptadas (todas con peso $< w(e)$, pues fueron procesadas antes).

El camino $p$ junto con $e$ forma un ciclo. Como $e$ es la última
arista procesada en este ciclo (todas las del camino ya fueron
aceptadas), $e$ tiene el **peso máximo** del ciclo.

Por la propiedad de ciclo, $e$ no pertenece a ningún MST.

$$\blacksquare$$

---

## Demostración 3: unicidad del MST con pesos distintos

**Teorema.** Si todos los pesos de aristas son distintos, el MST es
único.

**Prueba por contradicción.** Supongamos que existen dos MSTs
distintos $T_1$ y $T_2$ con $w(T_1) = w(T_2)$. Sea $e$ la arista de
**menor peso** que está en uno pero no en el otro. Sin pérdida de
generalidad, $e \in T_1 \setminus T_2$.

Agregar $e$ a $T_2$ crea un ciclo $C$ en $T_2 \cup \{e\}$. Todas las
aristas de $C$ excepto $e$ pertenecen a $T_2$. Como $e$ tiene el
menor peso entre las aristas en las que $T_1$ y $T_2$ difieren, y
todas las aristas de $C \setminus \{e\}$ están en $T_2$:

- Si alguna arista $e' \in C \setminus \{e\}$ no está en $T_1$,
  entonces $e'$ es una arista donde difieren $T_1$ y $T_2$, con
  $w(e') > w(e)$ (porque $e$ era la de menor peso en la diferencia).

- Así, $e$ no es la arista de peso máximo en $C$ solo si existe
  alguna $e'' \in C \setminus \{e\}$ con $w(e'') > w(e)$... Pero
  necesitamos que dicha $e''$ esté en $T_2$ y no en $T_1$.

Argumento directo: sea $e'$ una arista de $C \setminus \{e\}$ que no
está en $T_1$ (debe existir al menos una, pues agregar $e$ a $T_1$ no
crea ciclo pero a $T_2$ sí). Entonces $w(e') > w(e)$. Pero
$T_2' = T_2 \setminus \{e'\} \cup \{e\}$ sería un árbol generador
con $w(T_2') = w(T_2) - w(e') + w(e) < w(T_2)$, contradiciendo que
$T_2$ es un MST.

$$\blacksquare$$

---

## Demostración 4: complejidad $O(m \log n)$

**Teorema.** El algoritmo de Kruskal ejecuta en $O(m \log n)$.

**Prueba.** Desglosamos por fases:

**1) Ordenar aristas:** $O(m \log m)$. Como $m \leq n(n-1)/2 < n^2$:

$$\log m \leq \log n^2 = 2\log n$$

Así $O(m \log m) = O(m \log n)$.

**2) Operaciones Union-Find.** Se realizan a lo sumo $m$ operaciones
`find` (dos por arista: una para cada extremo) y a lo sumo $n - 1$
operaciones `union`. Con unión por rango y compresión de caminos:

$$O((2m + n - 1) \cdot \alpha(n)) = O(m \cdot \alpha(n))$$

donde $\alpha$ es la inversa de Ackermann ($\alpha(n) \leq 4$ para
todo $n$ práctico).

**3) Total:**

$$T = O(m \log n) + O(m \cdot \alpha(n)) = O(m \log n)$$

El costo está dominado por el ordenamiento. Si las aristas llegan
pre-ordenadas, Kruskal corre en $O(m \cdot \alpha(n)) \approx O(m)$.

$$\blacksquare$$

---

## Demostración 5: invariante del bosque parcial

**Teorema.** Sea $F_k$ el conjunto de aristas aceptadas tras procesar
las primeras $k$ aristas. Entonces $F_k$ es un subconjunto de algún
MST.

**Prueba por inducción sobre $k$.**

*Base ($k = 0$).* $F_0 = \emptyset \subseteq T^*$ para cualquier
MST $T^*$. ✓

*Paso inductivo.* Supongamos $F_{k-1} \subseteq T^*$ para algún
MST $T^*$. Sea $e_k$ la $k$-ésima arista procesada.

- Si $e_k$ es **rechazada**: $F_k = F_{k-1} \subseteq T^*$. ✓

- Si $e_k$ es **aceptada**: por la Demostración 1, $e_k$ es la
  arista de peso mínimo que cruza algún corte. Si $e_k \in T^*$,
  entonces $F_k = F_{k-1} \cup \{e_k\} \subseteq T^*$. ✓

  Si $e_k \notin T^*$: agregar $e_k$ a $T^*$ crea un ciclo $C$.
  Alguna arista $e'$ de $C$ cruza el mismo corte que $e_k$ y
  $e' \in T^* \setminus F_{k-1}$. Definimos
  $T^{**} = T^* \setminus \{e'\} \cup \{e_k\}$. Como $w(e_k) \leq w(e')$
  (por la propiedad de corte), $T^{**}$ es un MST. Y
  $F_k = F_{k-1} \cup \{e_k\} \subseteq T^{**}$. ✓

En todos los casos, $F_k$ es subconjunto de algún MST.

$$\blacksquare$$

---

## Verificación numérica

Grafo del README.md: 6 vértices, 9 aristas.

| Arista procesada | Peso | ¿Aceptada? | Corte respetado | Propiedad |
|:---:|:---:|:---:|:---:|:---:|
| (4,5) | 1 | Sí | $(\{4\}, \{0,1,2,3,5\})$ | Mín. que cruza ✓ |
| (1,2) | 2 | Sí | $(\{1\}, \{0,2,3,4,5\})$ | Mín. que cruza ✓ |
| (0,3) | 3 | Sí | $(\{0\}, \{1,2,3,4,5\})$ | Mín. que cruza ✓ |
| (0,1) | 4 | Sí | $(\{0,3\}, \{1,2,4,5\})$ | Mín. que cruza ✓ |
| (2,4) | 5 | Sí | $(\{0,1,2,3\}, \{4,5\})$ | Mín. que cruza ✓ |
| (1,4) | 6 | No | — | Ciclo: 1-2-4 tiene máx. peso 6 ✓ |

Peso del MST: $1 + 2 + 3 + 4 + 5 = 15$. ✓

Ninguna arista de menor peso cruza el corte indicado en cada paso,
y la arista rechazada es la de mayor peso en su ciclo. ✓

---

## Resumen de resultados

| Resultado | Técnica | Clave |
|-----------|--------|-------|
| Propiedad de corte | Contradicción: intercambiar aristas | $T' = T^* \setminus \{e'\} \cup \{e\}$, $w(T') < w(T^*)$ |
| Propiedad de ciclo | Contradicción: eliminar arista máxima del ciclo | Arista de peso máximo en ciclo no está en MST |
| Kruskal produce MST | Corte + orden ascendente | Cada arista aceptada es mín. que cruza un corte |
| Aristas rechazadas $\notin$ MST | Propiedad de ciclo | Máxima en su ciclo formado con aristas previas |
| $O(m \log n)$ | Ordenamiento domina | Union-Find $O(m \cdot \alpha(n))$ es sublineal |
