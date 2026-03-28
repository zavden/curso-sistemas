# T01 — Demostraciones formales: O, Ω, Θ

> Complemento riguroso del README.md. Aquí se prueban formalmente las
> afirmaciones que en el README se presentan como ejemplos informales.

---

## Recordatorio: las definiciones

Antes de demostrar, fijemos las definiciones exactas.

**Definición (Big-O).** Sean $f, g : \mathbb{N} \to \mathbb{R}^{\geq 0}$.
Decimos que $f(n) \in O(g(n))$ si:

```
∃ c > 0, ∃ n₀ ≥ 0  tales que  f(n) ≤ c · g(n)  ∀ n ≥ n₀
```

**Definición (Big-Omega).** $f(n) \in \Omega(g(n))$ si:

```
∃ c > 0, ∃ n₀ ≥ 0  tales que  f(n) ≥ c · g(n)  ∀ n ≥ n₀
```

**Definición (Big-Theta).** $f(n) \in \Theta(g(n))$ si:

```
∃ c₁ > 0, c₂ > 0, ∃ n₀ ≥ 0  tales que  c₁·g(n) ≤ f(n) ≤ c₂·g(n)  ∀ n ≥ n₀
```

---

## Demostración 1: $3n^2 + 5n \in O(n^2)$

**Teorema.** Sea $f(n) = 3n^2 + 5n$. Entonces $f(n) \in O(n^2)$.

**Prueba.** Debemos exhibir constantes $c > 0$ y $n_0 \geq 0$ tales que
$3n^2 + 5n \leq c \cdot n^2$ para todo $n \geq n_0$.

Acotamos el término de menor orden:

```
Para n ≥ 1:   5n ≤ 5n²   (porque n ≤ n² cuando n ≥ 1)
```

Sumando $3n^2$ a ambos lados:

```
3n² + 5n  ≤  3n² + 5n²  =  8n²
```

Tomamos $c = 8$ y $n_0 = 1$.

**Verificación**: para todo $n \geq 1$:

```
3n² + 5n  ≤  8n²
```

Esto se cumple porque $5n \leq 5n^2$ cuando $n \geq 1$ (equivalente a
$5 \leq 5n$, que vale para $n \geq 1$).

$$\therefore f(n) = 3n^2 + 5n \in O(n^2) \quad \text{con } c = 8,\; n_0 = 1. \qquad \blacksquare$$

**Nota sobre la elección de testigos.** Los valores $c = 8, n_0 = 1$ no son
únicos. Otras elecciones válidas:

```
c = 4, n₀ = 5:    3n² + 5n ≤ 4n²  ⟺  5n ≤ n²  ⟺  5 ≤ n    ✓ para n ≥ 5
c = 3.1, n₀ = 50: 3n² + 5n ≤ 3.1n²  ⟺  5n ≤ 0.1n²  ⟺  50 ≤ n  ✓ para n ≥ 50
```

Mientras más ajustada sea la constante $c$ (cercana a 3, el coeficiente
líder de $f$), más grande necesitamos $n_0$. Esto refleja que la definición
es asintótica: para $n$ suficientemente grande, el término $5n$ se vuelve
despreciable frente a $3n^2$.

---

## Demostración 2: $n^2 \notin O(n)$

**Teorema.** $f(n) = n^2 \notin O(n)$.

**Prueba (por contradicción).** Supongamos, buscando una contradicción,
que $n^2 \in O(n)$. Entonces existen constantes $c > 0$ y $n_0 \geq 0$
tales que:

```
n² ≤ c · n    para todo n ≥ n₀
```

Dividiendo ambos lados por $n$ (válido para $n \geq 1$, y podemos
asumir $n_0 \geq 1$ sin pérdida de generalidad):

```
n ≤ c    para todo n ≥ n₀
```

Pero $c$ es una constante fija. Tomemos $n^* = \lceil c \rceil + 1$.
Claramente $n^* > c$, y si $n^* \geq n_0$, entonces la desigualdad
$n^* \leq c$ se viola.

Si $n^* < n_0$, tomemos $n^{**} = \max(n_0, \lceil c \rceil + 1)$.
Entonces $n^{**} \geq n_0$ y $n^{**} > c$, lo que viola la desigualdad.

En ambos casos llegamos a una contradicción con la hipótesis de que la
desigualdad vale para **todo** $n \geq n_0$.

$$\therefore n^2 \notin O(n). \qquad \blacksquare$$

**Técnica usada: contradicción.** Para demostrar que $f \notin O(g)$,
asumimos que sí y derivamos que una cantidad que crece sin límite ($n$)
estaría acotada por una constante fija ($c$). Esta es la técnica
estándar para probar que una función **no** pertenece a una clase
asintótica.

**Prueba alternativa (directa, por definición negada).** La negación
de $f(n) \in O(g(n))$ es:

```
∀ c > 0, ∀ n₀ ≥ 0, ∃ n ≥ n₀  tal que  f(n) > c · g(n)
```

Debemos mostrar que para cualesquiera $c > 0$ y $n_0 \geq 0$, existe
un $n \geq n_0$ con $n^2 > c \cdot n$.

Sea $c > 0$ y $n_0 \geq 0$ arbitrarios. Tomemos $n = \max(n_0, \lceil c \rceil + 1)$.
Entonces:

```
n ≥ n₀         (por construcción)
n > c           (porque n ≥ ⌈c⌉ + 1 > c)
n² = n · n > c · n   (multiplicamos ambos lados por n > 0)
```

Así, para cualquier $c$ y $n_0$, hemos exhibido un $n$ que viola la
desigualdad. Por lo tanto, la condición de $O(n)$ no se puede
satisfacer.

$$\therefore n^2 \notin O(n). \qquad \blacksquare$$

---

## Demostración 3: $3n^2 + 5n \in \Omega(n^2)$

**Teorema.** Sea $f(n) = 3n^2 + 5n$. Entonces $f(n) \in \Omega(n^2)$.

**Prueba.** Debemos exhibir $c > 0$ y $n_0 \geq 0$ tales que
$3n^2 + 5n \geq c \cdot n^2$ para todo $n \geq n_0$.

Para todo $n \geq 0$, el término $5n$ es no negativo, así que:

```
3n² + 5n  ≥  3n²
```

Tomamos $c = 3$ y $n_0 = 0$.

**Verificación**: $3n^2 + 5n \geq 3n^2$ para todo $n \geq 0$.
Esto es inmediato: $5n \geq 0$ para $n \geq 0$.

$$\therefore f(n) = 3n^2 + 5n \in \Omega(n^2) \quad \text{con } c = 3,\; n_0 = 0. \qquad \blacksquare$$

---

## Demostración 4: $\Theta(g(n)) = O(g(n)) \cap \Omega(g(n))$

Este es un resultado fundamental que justifica por qué la notación
$\Theta$ captura el crecimiento "exacto" de una función. Probamos
ambas direcciones de la igualdad de conjuntos.

### Dirección $\Rightarrow$: $\Theta(g) \subseteq O(g) \cap \Omega(g)$

**Teorema.** Si $f(n) \in \Theta(g(n))$, entonces $f(n) \in O(g(n))$ y
$f(n) \in \Omega(g(n))$.

**Prueba.** Por hipótesis, existen $c_1, c_2 > 0$ y $n_0 \geq 0$ tales que:

```
c₁ · g(n)  ≤  f(n)  ≤  c₂ · g(n)    ∀ n ≥ n₀
```

La desigualdad derecha dice:

```
f(n) ≤ c₂ · g(n)    ∀ n ≥ n₀
```

Esto es exactamente la definición de $f(n) \in O(g(n))$ con $c = c_2$.

La desigualdad izquierda dice:

```
f(n) ≥ c₁ · g(n)    ∀ n ≥ n₀
```

Esto es exactamente la definición de $f(n) \in \Omega(g(n))$ con $c = c_1$.

$$\therefore f(n) \in O(g(n)) \cap \Omega(g(n)). \qquad \blacksquare$$

### Dirección $\Leftarrow$: $O(g) \cap \Omega(g) \subseteq \Theta(g)$

**Teorema.** Si $f(n) \in O(g(n))$ y $f(n) \in \Omega(g(n))$, entonces
$f(n) \in \Theta(g(n))$.

**Prueba.** Por hipótesis:

```
(1)  f(n) ∈ O(g(n)):   ∃ cₐ > 0, n₁ ≥ 0  t.q.  f(n) ≤ cₐ · g(n)  ∀ n ≥ n₁
(2)  f(n) ∈ Ω(g(n)):   ∃ c_b > 0, n₂ ≥ 0  t.q.  f(n) ≥ c_b · g(n)  ∀ n ≥ n₂
```

Tomamos:

```
c₁ = c_b,   c₂ = cₐ,   n₀ = max(n₁, n₂)
```

Para todo $n \geq n_0$:

- $n \geq n_0 \geq n_2$, así que por (2): $f(n) \geq c_b \cdot g(n) = c_1 \cdot g(n)$
- $n \geq n_0 \geq n_1$, así que por (1): $f(n) \leq c_a \cdot g(n) = c_2 \cdot g(n)$

Combinando:

```
c₁ · g(n)  ≤  f(n)  ≤  c₂ · g(n)    ∀ n ≥ n₀
```

que es la definición de $f(n) \in \Theta(g(n))$.

$$\therefore f(n) \in \Theta(g(n)). \qquad \blacksquare$$

### Conclusión

Las dos direcciones juntas dan la igualdad de conjuntos:

$$\Theta(g(n)) = O(g(n)) \cap \Omega(g(n))$$

**Consecuencia práctica.** Para probar que $f(n) \in \Theta(g(n))$,
basta probar por separado que $f(n) \in O(g(n))$ y $f(n) \in \Omega(g(n))$.
Esto suele ser más sencillo que encontrar directamente las dos constantes
$c_1, c_2$ de la definición de $\Theta$.

---

## Aplicación: $3n^2 + 5n \in \Theta(n^2)$

Combinando las Demostraciones 1 y 3:

```
Demostración 1:  3n² + 5n ∈ O(n²)    con c = 8, n₀ = 1
Demostración 3:  3n² + 5n ∈ Ω(n²)    con c = 3, n₀ = 0

Por Demostración 4:  O(n²) ∩ Ω(n²) = Θ(n²)

∴  3n² + 5n ∈ Θ(n²)    con c₁ = 3, c₂ = 8, n₀ = max(1, 0) = 1
```

Esto significa que $3n^2 + 5n$ crece **exactamente** como $n^2$:

- Nunca más rápido que $8n^2$ (cota superior)
- Nunca más lento que $3n^2$ (cota inferior)

Los coeficientes $c_1 = 3$ y $c_2 = 8$ forman un "sándwich" asintótico
alrededor de $f(n)$.

---

## Resultado adicional: generalización para polinomios

Las demostraciones anteriores son casos particulares de un resultado general.

**Teorema.** Sea $f(n) = a_k n^k + a_{k-1} n^{k-1} + \cdots + a_1 n + a_0$
un polinomio de grado $k$ con coeficiente líder $a_k > 0$. Entonces
$f(n) \in \Theta(n^k)$.

**Prueba.**

*Cota superior* ($f(n) \in O(n^k)$): Para $n \geq 1$, cada término
$|a_i| n^i \leq |a_i| n^k$ (porque $n^i \leq n^k$ cuando $i \leq k$
y $n \geq 1$). Entonces:

```
f(n) ≤ |a_k|·nᵏ + |a_{k-1}|·nᵏ + ... + |a₀|·nᵏ
     = (|a_k| + |a_{k-1}| + ... + |a₀|) · nᵏ
```

Tomamos $c_2 = \sum_{i=0}^{k} |a_i|$ y $n_0 = 1$.

*Cota inferior* ($f(n) \in \Omega(n^k)$): Como $a_k > 0$, para $n$
suficientemente grande, el término $a_k n^k$ domina. Formalmente:

```
f(n) = aₖnᵏ + (a_{k-1}n^{k-1} + ... + a₀)
```

El residuo $r(n) = a_{k-1}n^{k-1} + \cdots + a_0$ satisface
$|r(n)| \leq M \cdot n^{k-1}$ para alguna constante $M > 0$ y $n \geq 1$
(por la cota superior aplicada a un polinomio de grado $k-1$). Entonces:

```
f(n) ≥ aₖnᵏ - |r(n)| ≥ aₖnᵏ - M·n^{k-1} = nᵏ(aₖ - M/n)
```

Para $n \geq 2M/a_k$, tenemos $a_k - M/n \geq a_k/2 > 0$, así que:

```
f(n) ≥ (aₖ/2) · nᵏ
```

Tomamos $c_1 = a_k/2$ y $n_0 = \lceil 2M/a_k \rceil$.

Combinando ambas cotas: $f(n) \in \Theta(n^k)$.

$$\blacksquare$$

**Consecuencia.** Para determinar la clase asintótica de un polinomio,
solo importa el grado y el signo del coeficiente líder. Los coeficientes
y términos de menor orden son irrelevantes asintóticamente. Esto
justifica formalmente la Regla 1 (eliminar constantes) y la Regla 2
(eliminar términos de menor orden) del README.md.

---

## Resumen de técnicas de demostración

| Qué demostrar | Técnica | Patrón |
|---------------|---------|--------|
| $f \in O(g)$ | Exhibir testigos $c, n_0$ | Acotar $f$ por arriba usando desigualdades |
| $f \in \Omega(g)$ | Exhibir testigos $c, n_0$ | Acotar $f$ por abajo (descartar negativos) |
| $f \in \Theta(g)$ | Probar $O$ + $\Omega$ por separado | Usar el teorema $\Theta = O \cap \Omega$ |
| $f \notin O(g)$ | Contradicción | Asumir que sí, derivar que $n$ está acotada |
| $f \notin O(g)$ | Directa (negación) | Para todo $c, n_0$, exhibir un $n$ que viola |
