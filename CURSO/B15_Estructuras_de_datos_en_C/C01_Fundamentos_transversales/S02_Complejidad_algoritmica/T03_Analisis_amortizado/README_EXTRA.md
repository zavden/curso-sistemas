# T03 — Demostración formal: inserción amortizada en vector dinámico

> Complemento riguroso del README.md. Aquí se prueba formalmente que la
> inserción en un vector dinámico con duplicación es O(1) amortizado
> usando el método del potencial.

---

## Preliminar: método del potencial

**Definición (Función de potencial).** Sea $D_i$ el estado de una
estructura de datos después de la $i$-ésima operación. Una función de
potencial $\Phi$ asigna a cada estado un valor real:

$$\Phi : \text{estados} \to \mathbb{R}$$

El **costo amortizado** de la $i$-ésima operación es:

$$\hat{c}_i = c_i + \Phi(D_i) - \Phi(D_{i-1})$$

donde $c_i$ es el costo real de la operación.

**Propiedad clave.** Si $\Phi(D_n) \geq \Phi(D_0)$ para todo $n$,
entonces la suma de costos amortizados es una cota superior de la suma
de costos reales:

$$\sum_{i=1}^{n} c_i = \sum_{i=1}^{n} \hat{c}_i - \Phi(D_n) + \Phi(D_0) \leq \sum_{i=1}^{n} \hat{c}_i$$

En particular, si $\Phi(D_0) = 0$ y $\Phi(D_i) \geq 0$ para todo $i$,
la desigualdad vale automáticamente.

---

## Modelo formal

Consideramos un vector dinámico con las siguientes convenciones:

- **size** ($s$): número de elementos almacenados.
- **capacity** ($C$): número de posiciones alocadas.
- **Invariante de capacidad**: $s \leq C$ siempre.
- **Estrategia de crecimiento**: cuando $s = C$ y se ejecuta un push,
  se aloca un nuevo arreglo de tamaño $2C$, se copian los $C$ elementos,
  y se inserta el nuevo. La capacidad inicial es $C_0 \geq 1$.
- **Modelo de costo**: cada escritura en una posición del arreglo
  (inserción o copia) cuesta 1. Un push sin realloc cuesta 1; un push
  con realloc que copia $k$ elementos cuesta $k + 1$.

---

## Demostración 1: O(1) amortizado por método del potencial

**Teorema.** La operación push en un vector dinámico con estrategia de
duplicación tiene costo amortizado $O(1)$.

**Prueba.** Definimos la función de potencial:

$$\Phi = 2s - C$$

donde $s$ es el size y $C$ la capacity del vector.

### Lema: $\Phi \geq 0$ siempre

Probamos que $\Phi(D_i) \geq 0$ para todo $i \geq 0$, por inducción
sobre las operaciones.

**Base.** Justo después del primer push (o con un vector vacío de
capacidad $C_0$ y $s = 0$): si $s = 0$ y $C = C_0$, entonces
$\Phi = -C_0 < 0$. Esto es problemático, así que refinamos:
asumimos que el vector se inicializa con $s = 0, C = 0$ (como
`Vec::new()` en Rust), y la primera inserción aloca con capacidad 1.

Para $s = 0, C = 0$: $\Phi = 0$. ✓

Tras la primera inserción: $s = 1, C = 1$: $\Phi = 2(1) - 1 = 1 \geq 0$. ✓

**Paso inductivo.** Supongamos que $\Phi \geq 0$ antes de la $i$-ésima
operación. Hay dos casos:

*Caso 1 (sin realloc)*: $s < C$ antes del push. Después: $s' = s + 1$,
$C' = C$. Entonces:

$$\Phi' = 2(s+1) - C = (2s - C) + 2 = \Phi + 2 \geq 0 + 2 > 0 \quad \checkmark$$

*Caso 2 (con realloc)*: $s = C$ antes del push. Después: $s' = s + 1$,
$C' = 2C = 2s$. Entonces:

$$\Phi' = 2(s+1) - 2s = 2 > 0 \quad \checkmark$$

En ambos casos, $\Phi' > 0$. Como $\Phi(D_0) = 0$ y $\Phi(D_i) \geq 0$
para todo $i$, la función de potencial es válida. $\square$

### Costo amortizado: caso sin realloc

Sea $s < C$ antes del push. Entonces:

```
Costo real:    c = 1
Φ_antes:       2s - C
Φ_después:     2(s+1) - C = 2s - C + 2

ΔΦ = 2

Costo amortizado = c + ΔΦ = 1 + 2 = 3
```

### Costo amortizado: caso con realloc

Sea $s = C = k$ antes del push. Se copian $k$ elementos y se inserta 1:

```
Costo real:    c = k + 1
Φ_antes:       2k - k = k
Φ_después:     2(k+1) - 2k = 2

ΔΦ = 2 - k

Costo amortizado = c + ΔΦ = (k + 1) + (2 - k) = 3
```

### Conclusión

En ambos casos, el costo amortizado es exactamente **3**, que es $O(1)$.

Por la propiedad del método del potencial, el costo total de $n$
operaciones push satisface:

$$\sum_{i=1}^{n} c_i \leq \sum_{i=1}^{n} \hat{c}_i = 3n$$

$$\therefore \text{push es } O(1) \text{ amortizado.} \qquad \blacksquare$$

---

## Demostración 2: O(1) amortizado por método agregado

**Teorema.** El costo total de $n$ operaciones push en un vector
dinámico con duplicación es menor que $3n$.

**Prueba.** Separamos el costo total en dos componentes:

$$T(n) = \underbrace{n}_{\text{inserciones}} + \underbrace{\sum_{\text{reallocs}} \text{copias}}_{\text{copias por realloc}}$$

Las inserciones contribuyen exactamente $n$ al costo total (una por push).

Los reallocs ocurren cuando el size alcanza una potencia de 2. Partiendo
de capacidad 1, los reallocs ocurren en los pushes 1, 2, 4, 8, ...,
$2^{\lfloor \log_2 n \rfloor}$, y en cada realloc del push $2^j$ se
copian $2^j$ elementos. La suma de copias es:

$$\sum_{j=0}^{\lfloor \log_2 n \rfloor} 2^j = 2^{\lfloor \log_2 n \rfloor + 1} - 1$$

Acotamos: como $2^{\lfloor \log_2 n \rfloor} \leq n$, tenemos:

$$2^{\lfloor \log_2 n \rfloor + 1} - 1 = 2 \cdot 2^{\lfloor \log_2 n \rfloor} - 1 \leq 2n - 1 < 2n$$

Por lo tanto:

$$T(n) < n + 2n = 3n$$

El costo amortizado por operación es:

$$\frac{T(n)}{n} < \frac{3n}{n} = 3 = O(1)$$

$$\therefore \text{push es } O(1) \text{ amortizado.} \qquad \blacksquare$$

---

## Demostración 3: generalización para factor de crecimiento $\alpha > 1$

**Teorema.** Sea $\alpha > 1$ el factor de crecimiento (la capacidad se
multiplica por $\alpha$ en cada realloc). Entonces push es $O(1)$
amortizado.

**Prueba (por método del potencial).** Definimos:

$$\Phi = \frac{\alpha}{\alpha - 1} \cdot s - C$$

**Verificación de $\Phi \geq 0$:**

*Sin realloc* ($s < C$): $\Phi$ crece en $\frac{\alpha}{\alpha-1}$ por
push. Si arranca $\geq 0$, se mantiene.

*Con realloc* ($s = C = k$): Después, $s' = k+1$, $C' = \alpha k$.

$$\Phi' = \frac{\alpha}{\alpha - 1}(k+1) - \alpha k = \frac{\alpha k + \alpha - \alpha^2 k + \alpha k}{\alpha - 1} = \frac{\alpha(1 - (\alpha - 2)k)}{\alpha - 1}$$

Esto requiere más cuidado. Usemos el método agregado que es más limpio.

**Prueba alternativa (método agregado).** Los reallocs ocurren cuando el
size alcanza $\alpha^0, \alpha^1, \alpha^2, \ldots$ (asumiendo capacidad
inicial 1 y que los reallocs se disparan cuando $s = C$). Sea
$m = \lfloor \log_\alpha n \rfloor$ el número de reallocs en $n$ pushes.
Las copias totales son:

$$\sum_{j=0}^{m} \alpha^j = \frac{\alpha^{m+1} - 1}{\alpha - 1}$$

Como $\alpha^m \leq n$:

$$\frac{\alpha^{m+1} - 1}{\alpha - 1} \leq \frac{\alpha \cdot n - 1}{\alpha - 1} < \frac{\alpha}{\alpha - 1} \cdot n$$

El costo total es:

$$T(n) < n + \frac{\alpha}{\alpha - 1} \cdot n = \left(1 + \frac{\alpha}{\alpha - 1}\right) n = \frac{2\alpha - 1}{\alpha - 1} \cdot n$$

El costo amortizado es:

$$\frac{T(n)}{n} < \frac{2\alpha - 1}{\alpha - 1} = O(1)$$

| Factor $\alpha$ | Cota amortizada $\frac{2\alpha - 1}{\alpha - 1}$ |
|:---:|:---:|
| 1.5 | 4.0 |
| 2 | 3.0 |
| 3 | 2.5 |
| 4 | 2.33... |

$$\therefore \text{Para todo } \alpha > 1, \text{ push es } O(1) \text{ amortizado.} \qquad \blacksquare$$

**Observación.** La cota mejora conforme $\alpha$ crece: menos reallocs
implica menos copias totales. Sin embargo, el desperdicio de memoria en
el peor caso es $(\alpha - 1)/\alpha$ (fracción de capacidad no usada
justo después de un realloc con un solo push). Para $\alpha = 2$ se
desperdicia hasta 50%; para $\alpha = 3$, hasta 67%.

---

## Demostración 4: crecimiento aditivo da $\Omega(n)$ amortizado

**Teorema.** Si la capacidad crece sumando una constante $d > 0$ (en
lugar de multiplicar), el costo amortizado de push es $\Theta(n)$.

**Prueba.** Con capacidad inicial $d$, los reallocs ocurren en los
pushes $d+1, 2d+1, 3d+1, \ldots$ En los primeros $n$ pushes hay
$m = \lfloor (n-1)/d \rfloor$ reallocs. El $j$-ésimo realloc copia
$jd$ elementos. Las copias totales son:

$$\sum_{j=1}^{m} jd = d \cdot \frac{m(m+1)}{2}$$

Como $m \geq (n-1)/d - 1 = (n - d - 1)/d$, para $n$ suficientemente
grande ($n \geq 2d$) tenemos $m \geq n/(2d)$, así que:

$$\sum_{j=1}^{m} jd \geq d \cdot \frac{(n/(2d))(n/(2d) + 1)}{2} \geq d \cdot \frac{n^2/(4d^2)}{2} = \frac{n^2}{8d}$$

El costo total satisface:

$$T(n) \geq \frac{n^2}{8d} = \Omega(n^2)$$

Y la cota superior:

$$T(n) = n + d \cdot \frac{m(m+1)}{2} \leq n + d \cdot \frac{(n/d)(n/d + 1)}{2} \leq n + \frac{n^2}{2d} + \frac{n}{2} = O(n^2)$$

Combinando: $T(n) = \Theta(n^2)$, y el costo amortizado es:

$$\frac{T(n)}{n} = \Theta(n)$$

$$\therefore \text{El crecimiento aditivo da costo amortizado } \Theta(n), \text{ no } O(1). \qquad \blacksquare$$

**Consecuencia.** La duplicación (o cualquier factor multiplicativo
$> 1$) es la condición necesaria y suficiente para obtener $O(1)$
amortizado. El crecimiento aditivo, sin importar cuán grande sea la
constante $d$, siempre resulta en costo amortizado lineal.

---

## Resumen de resultados

| Resultado | Método | Cota |
|-----------|--------|------|
| Push con duplicación es $O(1)$ amortizado | Potencial ($\Phi = 2s - C$) | $\hat{c} = 3$ |
| Costo total de $n$ pushes $< 3n$ | Agregado (serie geométrica) | $T(n) < 3n$ |
| Push con factor $\alpha > 1$ es $O(1)$ amortizado | Agregado | $\hat{c} < \frac{2\alpha-1}{\alpha-1}$ |
| Push con crecimiento $+d$ es $\Theta(n)$ amortizado | Agregado (serie aritmética) | $T(n) = \Theta(n^2)$ |
