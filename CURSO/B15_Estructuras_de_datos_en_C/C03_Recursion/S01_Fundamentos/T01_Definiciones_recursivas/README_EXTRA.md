# T01 — Demostraciones formales: recursión, inducción y Fibonacci

> Complemento riguroso del README.md. Aquí se prueba formalmente la
> correctitud de la definición recursiva de factorial y se deriva la
> fórmula cerrada de Fibonacci por ecuación característica.

---

## Recordatorio: inducción matemática

**Inducción simple.** Para probar $P(n)$ para todo $n \geq 0$:

```
1. Base: probar P(0).
2. Paso: suponer P(k) (hipótesis inductiva), probar P(k+1).
```

**Inducción fuerte (o completa).** Para probar $P(n)$ para todo $n \geq 0$:

```
1. Base: probar P(0) (y posiblemente P(1), P(2), ..., P(j) para
   algún j pequeño, según se necesite).
2. Paso: suponer P(0), P(1), ..., P(k) TODAS verdaderas,
   probar P(k+1).
```

La diferencia: en inducción simple se asume solo $P(k)$; en inducción
fuerte se asume $P(i)$ para todo $i \leq k$. La inducción fuerte es
más poderosa cuando el caso recursivo depende de más de un predecesor
(como Fibonacci, que depende de $F(n-1)$ y $F(n-2)$).

---

## Demostración 1: correctitud del factorial recursivo

### Definición recursiva

La función factorial se define como:

$$f(n) = \begin{cases} 1 & \text{si } n = 0 \\ n \cdot f(n-1) & \text{si } n \geq 1 \end{cases}$$

### Definición no recursiva

El factorial también tiene una definición directa como productoria:

$$n! = \prod_{i=1}^{n} i = 1 \cdot 2 \cdot 3 \cdots n$$

con la convención de que el producto vacío ($n = 0$) es 1.

### Teorema

**Teorema.** La función recursiva $f$ computa el factorial. Es decir,
$f(n) = n!$ para todo $n \geq 0$.

**Prueba (por inducción fuerte).** Sea $P(n)$: "$f(n) = n!$".

**Base ($n = 0$).**

```
f(0) = 1       (por definición recursiva, caso base)
0!   = 1       (producto vacío)

f(0) = 0!  ✓
```

$P(0)$ es verdadera.

**Paso inductivo.** Sea $k \geq 0$. Suponemos que $P(i)$ es verdadera
para todo $0 \leq i \leq k$ (hipótesis inductiva). Probamos $P(k+1)$.

Calculamos $f(k+1)$:

```
f(k+1) = (k+1) · f(k)        (por definición recursiva, caso k+1 ≥ 1)
       = (k+1) · k!           (por hipótesis inductiva: f(k) = k!)
       = (k+1)!               (por definición de factorial)
```

Por lo tanto $f(k+1) = (k+1)!$, y $P(k+1)$ es verdadera.

Por el principio de inducción fuerte, $P(n)$ vale para todo $n \geq 0$.

$$\therefore f(n) = n! \quad \forall n \geq 0. \qquad \blacksquare$$

**Observación.** Esta demostración también funciona con inducción
simple, ya que el caso recursivo solo depende de $f(n-1)$, no de valores
anteriores. Usamos inducción fuerte para mantener consistencia con la
Demostración 2, donde sí es necesaria.

---

## Demostración 2: terminación del factorial recursivo

Antes de probar correctitud, conviene probar que la función siempre
termina (no entra en recursión infinita).

**Teorema.** Para todo $n \geq 0$, la evaluación de $f(n)$ termina en
exactamente $n$ llamadas recursivas.

**Prueba (por inducción simple).**

**Base ($n = 0$).** $f(0) = 1$ retorna directamente. 0 llamadas
recursivas. ✓

**Paso inductivo.** Supongamos que $f(k)$ termina en $k$ llamadas
recursivas. Entonces $f(k+1)$ ejecuta:

```
f(k+1) = (k+1) · f(k)
```

Esto es una llamada recursiva a $f(k)$, que por hipótesis termina en
$k$ llamadas. En total: $1 + k = k + 1$ llamadas recursivas.

Por inducción, $f(n)$ termina en $n$ llamadas recursivas para todo
$n \geq 0$.

$$\blacksquare$$

**Función de decrecimiento (variant).** El argumento $n$ decrece
estrictamente en cada llamada ($n \to n-1$) y está acotado inferiormente
por 0. Todo entero no negativo decreciente eventualmente llega a 0,
donde la recursión se detiene. Esta es la justificación formal estándar
de terminación.

---

## Demostración 3: fórmula cerrada de Fibonacci

### La recurrencia

La secuencia de Fibonacci se define por:

$$F(n) = \begin{cases} 0 & \text{si } n = 0 \\ 1 & \text{si } n = 1 \\ F(n-1) + F(n-2) & \text{si } n \geq 2 \end{cases}$$

### La fórmula de Binet

**Teorema.** Para todo $n \geq 0$:

$$F(n) = \frac{\varphi^n - \psi^n}{\sqrt{5}}$$

donde $\varphi = \frac{1 + \sqrt{5}}{2}$ (proporción áurea) y
$\psi = \frac{1 - \sqrt{5}}{2}$.

La prueba se divide en dos partes:
1. **Derivación**: obtener la fórmula resolviendo la ecuación
   característica de la recurrencia.
2. **Verificación**: probar por inducción fuerte que la fórmula satisface
   la recurrencia.

### Parte 1: derivación por ecuación característica

**Paso 1: plantear la ecuación característica.**

La recurrencia $F(n) = F(n-1) + F(n-2)$ es una recurrencia lineal
homogénea de segundo orden con coeficientes constantes. Buscamos
soluciones de la forma $F(n) = r^n$ para alguna constante $r$:

```
r^n = r^(n-1) + r^(n-2)
```

Dividiendo por $r^{n-2}$ (asumiendo $r \neq 0$):

```
r² = r + 1
r² - r - 1 = 0
```

Esta es la **ecuación característica** de la recurrencia.

**Paso 2: resolver la ecuación característica.**

Aplicando la fórmula cuadrática:

$$r = \frac{1 \pm \sqrt{1 + 4}}{2} = \frac{1 \pm \sqrt{5}}{2}$$

Las dos raíces son:

$$\varphi = \frac{1 + \sqrt{5}}{2} \approx 1.618 \qquad \psi = \frac{1 - \sqrt{5}}{2} \approx -0.618$$

**Paso 3: formar la solución general.**

Por la teoría de recurrencias lineales, la solución general es una
combinación lineal de las soluciones fundamentales:

$$F(n) = A \cdot \varphi^n + B \cdot \psi^n$$

para constantes $A$ y $B$ determinadas por las condiciones iniciales.

**Paso 4: determinar $A$ y $B$ con las condiciones iniciales.**

```
F(0) = 0:    A · φ⁰ + B · ψ⁰ = 0    →    A + B = 0          ...(I)
F(1) = 1:    A · φ¹ + B · ψ¹ = 1    →    Aφ + Bψ = 1        ...(II)
```

De (I): $B = -A$. Sustituyendo en (II):

```
Aφ + (-A)ψ = 1
A(φ - ψ) = 1
```

Calculamos $\varphi - \psi$:

$$\varphi - \psi = \frac{1+\sqrt{5}}{2} - \frac{1-\sqrt{5}}{2} = \frac{2\sqrt{5}}{2} = \sqrt{5}$$

Entonces:

$$A \cdot \sqrt{5} = 1 \quad \Rightarrow \quad A = \frac{1}{\sqrt{5}}, \quad B = -\frac{1}{\sqrt{5}}$$

**Paso 5: escribir la fórmula cerrada.**

$$F(n) = \frac{1}{\sqrt{5}} \cdot \varphi^n - \frac{1}{\sqrt{5}} \cdot \psi^n = \frac{\varphi^n - \psi^n}{\sqrt{5}}$$

### Parte 2: verificación por inducción fuerte

Verificamos que $F(n) = \frac{\varphi^n - \psi^n}{\sqrt{5}}$ satisface
la recurrencia de Fibonacci. Sea $P(n)$: "$F(n) = \frac{\varphi^n - \psi^n}{\sqrt{5}}$".

**Base ($n = 0$ y $n = 1$).**

```
F(0) = (φ⁰ - ψ⁰)/√5 = (1 - 1)/√5 = 0    ✓
F(1) = (φ¹ - ψ¹)/√5 = (φ - ψ)/√5 = √5/√5 = 1    ✓
```

**Paso inductivo.** Sea $k \geq 1$. Suponemos $P(i)$ verdadera para
todo $0 \leq i \leq k$. Probamos $P(k+1)$.

Por la recurrencia de Fibonacci y la hipótesis inductiva:

$$F(k+1) = F(k) + F(k-1) = \frac{\varphi^k - \psi^k}{\sqrt{5}} + \frac{\varphi^{k-1} - \psi^{k-1}}{\sqrt{5}}$$

Factorizamos:

$$= \frac{\varphi^{k-1}(\varphi + 1) - \psi^{k-1}(\psi + 1)}{\sqrt{5}}$$

Aquí usamos la propiedad clave: como $\varphi$ y $\psi$ son raíces de
$r^2 = r + 1$, se cumple que:

$$\varphi + 1 = \varphi^2 \qquad \text{y} \qquad \psi + 1 = \psi^2$$

**Verificación de $\varphi + 1 = \varphi^2$:**

$$\varphi^2 = \left(\frac{1+\sqrt{5}}{2}\right)^2 = \frac{1 + 2\sqrt{5} + 5}{4} = \frac{6 + 2\sqrt{5}}{4} = \frac{3 + \sqrt{5}}{2}$$

$$\varphi + 1 = \frac{1+\sqrt{5}}{2} + 1 = \frac{3 + \sqrt{5}}{2} \quad \checkmark$$

Análogamente para $\psi$. Sustituyendo:

$$F(k+1) = \frac{\varphi^{k-1} \cdot \varphi^2 - \psi^{k-1} \cdot \psi^2}{\sqrt{5}} = \frac{\varphi^{k+1} - \psi^{k+1}}{\sqrt{5}}$$

que es exactamente la fórmula para $n = k+1$. Así, $P(k+1)$ es
verdadera.

Por inducción fuerte, la fórmula vale para todo $n \geq 0$.

$$\therefore F(n) = \frac{\varphi^n - \psi^n}{\sqrt{5}} \quad \forall n \geq 0. \qquad \blacksquare$$

**Nota.** Aquí la inducción fuerte es necesaria porque el paso inductivo
usa tanto $P(k)$ como $P(k-1)$, correspondiendo a que la recurrencia de
Fibonacci depende de los dos términos anteriores.

---

## Consecuencias de la fórmula de Binet

### Corolario 1: $F(n)$ siempre es entero

A pesar de que la fórmula involucra $\sqrt{5}$, los términos irracionales
se cancelan exactamente.

**Prueba.** Expandiendo $\varphi^n$ y $\psi^n$ con el binomio:

$$\varphi^n = \frac{1}{2^n} \sum_{j=0}^{n} \binom{n}{j} (\sqrt{5})^j, \qquad \psi^n = \frac{1}{2^n} \sum_{j=0}^{n} \binom{n}{j} (-\sqrt{5})^j$$

Al restar, los términos con $j$ par se cancelan (tienen el mismo signo
en ambos), y los términos con $j$ impar se duplican. Cada término con
$j$ impar contiene $(\sqrt{5})^j = 5^{(j-1)/2} \cdot \sqrt{5}$, que al
dividir por $\sqrt{5}$ da un racional. Todos los coeficientes son
enteros por las propiedades del binomio. $\square$

### Corolario 2: $F(n) = \text{round}(\varphi^n / \sqrt{5})$

**Prueba.** Como $|\psi| = \frac{\sqrt{5} - 1}{2} \approx 0.618 < 1$:

$$|\psi^n| < 1 \quad \text{para todo } n \geq 1$$

Por lo tanto:

$$\left| F(n) - \frac{\varphi^n}{\sqrt{5}} \right| = \frac{|\psi|^n}{\sqrt{5}} < \frac{1}{\sqrt{5}} < \frac{1}{2}$$

Como $F(n)$ es entero y $\varphi^n / \sqrt{5}$ está a distancia
$< 1/2$ de él, $F(n)$ es el entero más cercano a $\varphi^n / \sqrt{5}$.
$\square$

**Consecuencia práctica.** Esto significa que para $n$ grande,
$F(n) \approx \varphi^n / \sqrt{5}$, lo que confirma que Fibonacci
crece exponencialmente con base $\varphi \approx 1.618$.

### Corolario 3: complejidad de la recursión ingenua de Fibonacci

El número de llamadas $T(n)$ de la implementación recursiva directa
satisface:

```
T(0) = 1,  T(1) = 1,  T(n) = T(n-1) + T(n-2) + 1
```

Se puede probar que $T(n) = 2F(n+1) - 1$, lo que por la fórmula de
Binet da:

$$T(n) = \Theta(\varphi^n)$$

Esto formaliza la afirmación del README.md de que la recursión ingenua
es exponencial.

---

## Resumen de técnicas

| Qué se demuestra | Técnica | Tipo de inducción |
|-------------------|---------|-------------------|
| $f(n) = n!$ (correctitud de factorial) | Inducción | Simple (basta $P(k)$) |
| $f(n)$ termina en $n$ llamadas | Inducción + función de decrecimiento | Simple |
| Fórmula de Binet para $F(n)$ | Ecuación característica + inducción | Fuerte (necesita $P(k)$ y $P(k-1)$) |
| $F(n)$ es entero | Expansión binomial | Directa (algebraica) |
| $F(n) = \text{round}(\varphi^n/\sqrt{5})$ | Cota sobre $|\psi^n|$ | Directa |
