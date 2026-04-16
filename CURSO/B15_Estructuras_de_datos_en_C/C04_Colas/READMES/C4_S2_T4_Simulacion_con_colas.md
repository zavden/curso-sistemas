# Simulación con colas

## Simulación de eventos discretos

Muchos sistemas reales consisten en **entidades** que llegan, esperan, son
atendidas y se van.  La simulación de eventos discretos (*Discrete Event
Simulation*, DES) modela estos sistemas avanzando de **evento en evento** en
lugar de tick a tick:

```
Evento 1 (t=0.0):    Cliente A llega
Evento 2 (t=1.2):    Cliente B llega
Evento 3 (t=3.0):    Cliente A termina servicio
Evento 4 (t=3.5):    Cliente C llega
...
```

Los eventos se almacenan en una **cola de prioridad** ordenada por tiempo.  El
simulador extrae el evento con menor tiempo, lo procesa (lo cual puede generar
nuevos eventos), y repite.

### Estructura general

```
                 ┌──────────────┐
  Generador  ──▶│ Cola de      │──▶  Servidor(es)  ──▶  Salida
  de llegadas   │ espera (FIFO)│
                └──────────────┘
                      ↑
              Los clientes esperan aquí
```

---

## Modelo de un sistema de atención

### Componentes

| Componente | Descripción | Estructura de datos |
|-----------|-------------|---------------------|
| **Cola de eventos** | Todos los eventos futuros, ordenados por tiempo | Cola de prioridad (min) |
| **Cola de espera** | Clientes esperando ser atendidos | Cola FIFO |
| **Servidor(es)** | Recurso(s) que atiende(n) clientes | Bool (libre/ocupado) o contador |
| **Reloj** | Tiempo actual de la simulación | Variable `double` |

### Tipos de eventos

| Evento | Acción |
|--------|--------|
| **ARRIVAL** | Un cliente llega.  Si hay servidor libre, empieza servicio; si no, entra a la cola de espera. |
| **DEPARTURE** | Un cliente termina servicio.  Si hay clientes en cola, el siguiente empieza servicio. |

### Parámetros

| Parámetro | Símbolo | Significado |
|-----------|---------|-------------|
| Tasa de llegada | $\lambda$ | Clientes por unidad de tiempo |
| Tasa de servicio | $\mu$ | Clientes atendidos por unidad de tiempo por servidor |
| Número de servidores | $k$ | Cajeros, CPUs, etc. |
| Utilización | $\rho = \lambda / (k \cdot \mu)$ | Fracción del tiempo que los servidores están ocupados |

Si $\rho \geq 1$, los clientes llegan más rápido de lo que se atienden — la
cola crece indefinidamente.  Un sistema estable requiere $\rho < 1$.

---

## Tiempos aleatorios

### Distribución exponencial

Los tiempos entre llegadas y los tiempos de servicio se modelan típicamente con
la **distribución exponencial**, que tiene la propiedad de "sin memoria": la
probabilidad de que ocurra un evento en el siguiente instante no depende de
cuánto tiempo ha pasado.

Para generar un valor exponencial con media $1/\lambda$:

$$X = -\frac{1}{\lambda} \ln(U), \quad U \sim \text{Uniforme}(0, 1)$$

```c
#include <math.h>
#include <stdlib.h>

double exponential(double rate) {
    double u = (double)rand() / RAND_MAX;
    while (u == 0.0) u = (double)rand() / RAND_MAX;  /* evitar ln(0) */
    return -log(u) / rate;
}
```

```rust
fn exponential(rate: f64) -> f64 {
    let u: f64 = loop {
        let v = rand::random::<f64>();
        if v > 0.0 { break v; }
    };
    -u.ln() / rate
}
```

Con `rate = 2.0` (2 clientes/minuto), los tiempos entre llegadas promedian 0.5
minutos (30 segundos).

Para una simulación sin dependencias externas en Rust, un generador
pseudoaleatorio mínimo basta:

```rust
struct Rng(u64);

impl Rng {
    fn new(seed: u64) -> Self { Rng(seed) }

    fn next_f64(&mut self) -> f64 {
        // xorshift64
        self.0 ^= self.0 << 13;
        self.0 ^= self.0 >> 7;
        self.0 ^= self.0 << 17;
        (self.0 as f64) / (u64::MAX as f64)
    }

    fn exponential(&mut self, rate: f64) -> f64 {
        let u = loop {
            let v = self.next_f64();
            if v > 0.0 { break v; }
        };
        -u.ln() / rate
    }
}
```

---

## Simulación M/M/1: un solo servidor

El sistema más simple: llegadas Poisson (M), servicio exponencial (M), 1
servidor.  La notación **M/M/1** viene de la teoría de colas (Kendall).

### Implementación en C

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

/* ── Generador exponencial ── */

double exponential(double rate) {
    double u;
    do { u = (double)rand() / RAND_MAX; } while (u == 0.0);
    return -log(u) / rate;
}

/* ── Cola FIFO de clientes ── */

#define MAX_QUEUE 10000

typedef struct {
    double arrival_time[MAX_QUEUE];
    int front, rear, count;
} WaitQueue;

void wq_init(WaitQueue *q) { q->front = q->rear = q->count = 0; }
bool wq_empty(const WaitQueue *q) { return q->count == 0; }

void wq_enqueue(WaitQueue *q, double arrival) {
    q->arrival_time[q->rear] = arrival;
    q->rear = (q->rear + 1) % MAX_QUEUE;
    q->count++;
}

double wq_dequeue(WaitQueue *q) {
    double t = q->arrival_time[q->front];
    q->front = (q->front + 1) % MAX_QUEUE;
    q->count--;
    return t;
}

/* ── Cola de eventos (prioridad por tiempo) ── */

typedef enum { ARRIVAL, DEPARTURE } EventType;

typedef struct {
    double time;
    EventType type;
} Event;

typedef struct {
    Event events[MAX_QUEUE];
    int count;
} EventQueue;

void eq_init(EventQueue *eq) { eq->count = 0; }
bool eq_empty(const EventQueue *eq) { return eq->count == 0; }

void eq_insert(EventQueue *eq, double time, EventType type) {
    /* Inserción ordenada (O(n) — suficiente para esta simulación) */
    int i = eq->count - 1;
    while (i >= 0 && eq->events[i].time > time) {
        eq->events[i + 1] = eq->events[i];
        i--;
    }
    eq->events[i + 1] = (Event){ .time = time, .type = type };
    eq->count++;
}

Event eq_extract(EventQueue *eq) {
    Event e = eq->events[0];
    for (int i = 1; i < eq->count; i++) {
        eq->events[i - 1] = eq->events[i];
    }
    eq->count--;
    return e;
}

/* ── Simulación M/M/1 ── */

typedef struct {
    double total_wait;       /* suma de tiempos de espera */
    double total_service;    /* suma de tiempos de servicio */
    int customers_served;
    int max_queue_length;
    double server_busy_time;
} Stats;

void simulate_mm1(double arrival_rate, double service_rate,
                  int max_customers) {
    EventQueue eq;
    WaitQueue wq;
    eq_init(&eq);
    wq_init(&wq);

    Stats stats = {0};
    double clock = 0.0;
    bool server_busy = false;
    int arrivals = 0;

    /* Programar primera llegada */
    eq_insert(&eq, exponential(arrival_rate), ARRIVAL);

    while (!eq_empty(&eq) && stats.customers_served < max_customers) {
        Event e = eq_extract(&eq);
        clock = e.time;

        if (e.type == ARRIVAL) {
            arrivals++;

            /* Programar siguiente llegada (si no hemos alcanzado el máximo) */
            if (arrivals < max_customers) {
                eq_insert(&eq, clock + exponential(arrival_rate), ARRIVAL);
            }

            if (!server_busy) {
                /* Servidor libre: atender inmediatamente */
                server_busy = true;
                double service_time = exponential(service_rate);
                stats.total_service += service_time;
                eq_insert(&eq, clock + service_time, DEPARTURE);
                /* Tiempo de espera = 0 */
            } else {
                /* Servidor ocupado: encolar */
                wq_enqueue(&wq, clock);
                if (wq.count > stats.max_queue_length) {
                    stats.max_queue_length = wq.count;
                }
            }

        } else {  /* DEPARTURE */
            stats.customers_served++;
            stats.server_busy_time += 0;  /* ya contado en service_time */

            if (!wq_empty(&wq)) {
                /* Atender siguiente en cola */
                double arrival = wq_dequeue(&wq);
                double wait = clock - arrival;
                stats.total_wait += wait;

                double service_time = exponential(service_rate);
                stats.total_service += service_time;
                eq_insert(&eq, clock + service_time, DEPARTURE);
            } else {
                server_busy = false;
            }
        }
    }

    /* Resultados */
    double rho = arrival_rate / service_rate;
    printf("=== Simulación M/M/1 ===\n");
    printf("Tasa de llegada (λ):    %.2f\n", arrival_rate);
    printf("Tasa de servicio (μ):   %.2f\n", service_rate);
    printf("Utilización (ρ=λ/μ):    %.2f\n", rho);
    printf("Clientes atendidos:     %d\n", stats.customers_served);
    printf("Espera promedio:        %.3f\n",
           stats.total_wait / stats.customers_served);
    printf("Servicio promedio:      %.3f\n",
           stats.total_service / stats.customers_served);
    printf("Cola máxima:            %d\n", stats.max_queue_length);
    printf("Tiempo total simulado:  %.2f\n", clock);

    /* Valores teóricos M/M/1 */
    double wq_theory = rho / (service_rate * (1.0 - rho));
    printf("\n--- Teórico M/M/1 ---\n");
    printf("Espera promedio (Wq):   %.3f\n", wq_theory);
}

int main(void) {
    srand(42);
    simulate_mm1(0.8, 1.0, 10000);   /* ρ = 0.8 */
    return 0;
}
```

### Fórmulas teóricas M/M/1

Para un sistema M/M/1 en estado estable ($\rho < 1$):

| Métrica | Fórmula | Significado |
|---------|---------|-------------|
| Utilización | $\rho = \lambda / \mu$ | Fracción del tiempo servidor ocupado |
| Largo promedio de cola | $L_q = \rho^2 / (1 - \rho)$ | Clientes esperando (sin contar el atendido) |
| Tiempo promedio en cola | $W_q = \rho / (\mu(1 - \rho))$ | Tiempo de espera antes de ser atendido |
| Tiempo promedio en sistema | $W = 1 / (\mu - \lambda)$ | Espera + servicio |
| Clientes promedio en sistema | $L = \rho / (1 - \rho)$ | En cola + siendo atendido |

Estas fórmulas permiten verificar que la simulación converge a los valores
teóricos para $n$ grande.

---

## Simulación M/M/k: múltiples servidores

La extensión natural: un supermercado con $k$ cajeros.  Los clientes hacen
**una sola fila** y van al primer cajero disponible.

### Cambios respecto a M/M/1

| Aspecto | M/M/1 | M/M/k |
|---------|-------|-------|
| Servidor libre | `bool server_busy` | `int servers_busy` (0 a $k$) |
| Condición de espera | `server_busy` | `servers_busy == k` |
| Utilización | $\rho = \lambda / \mu$ | $\rho = \lambda / (k \cdot \mu)$ |

### Implementación en C

```c
void simulate_mmk(double arrival_rate, double service_rate,
                  int num_servers, int max_customers) {
    EventQueue eq;
    WaitQueue wq;
    eq_init(&eq);
    wq_init(&wq);

    Stats stats = {0};
    double clock = 0.0;
    int servers_busy = 0;
    int arrivals = 0;

    eq_insert(&eq, exponential(arrival_rate), ARRIVAL);

    while (!eq_empty(&eq) && stats.customers_served < max_customers) {
        Event e = eq_extract(&eq);
        clock = e.time;

        if (e.type == ARRIVAL) {
            arrivals++;
            if (arrivals < max_customers) {
                eq_insert(&eq, clock + exponential(arrival_rate), ARRIVAL);
            }

            if (servers_busy < num_servers) {
                /* Hay servidor libre */
                servers_busy++;
                double svc = exponential(service_rate);
                stats.total_service += svc;
                eq_insert(&eq, clock + svc, DEPARTURE);
            } else {
                wq_enqueue(&wq, clock);
                if (wq.count > stats.max_queue_length) {
                    stats.max_queue_length = wq.count;
                }
            }

        } else {  /* DEPARTURE */
            stats.customers_served++;

            if (!wq_empty(&wq)) {
                double arrival = wq_dequeue(&wq);
                stats.total_wait += (clock - arrival);
                double svc = exponential(service_rate);
                stats.total_service += svc;
                eq_insert(&eq, clock + svc, DEPARTURE);
            } else {
                servers_busy--;
            }
        }
    }

    double rho = arrival_rate / (num_servers * service_rate);
    printf("=== Simulación M/M/%d ===\n", num_servers);
    printf("Utilización (ρ):        %.2f\n", rho);
    printf("Clientes atendidos:     %d\n", stats.customers_served);
    printf("Espera promedio:        %.3f\n",
           stats.total_wait / stats.customers_served);
    printf("Cola máxima:            %d\n", stats.max_queue_length);
}
```

---

## Implementación en Rust

```rust
use std::collections::BinaryHeap;
use std::collections::VecDeque;
use std::cmp::Ordering;

// ── Generador pseudoaleatorio ──

struct Rng(u64);

impl Rng {
    fn new(seed: u64) -> Self { Rng(seed) }

    fn next_f64(&mut self) -> f64 {
        self.0 ^= self.0 << 13;
        self.0 ^= self.0 >> 7;
        self.0 ^= self.0 << 17;
        (self.0 as f64) / (u64::MAX as f64)
    }

    fn exponential(&mut self, rate: f64) -> f64 {
        let u = loop {
            let v = self.next_f64();
            if v > 0.0 { break v; }
        };
        -u.ln() / rate
    }
}

// ── Evento ──

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum EventType { Arrival, Departure }

#[derive(Debug, Clone)]
struct Event {
    time: f64,
    event_type: EventType,
}

// Orden invertido para BinaryHeap (min-heap por tiempo)
impl PartialEq for Event {
    fn eq(&self, other: &Self) -> bool {
        self.time == other.time
    }
}
impl Eq for Event {}

impl PartialOrd for Event {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for Event {
    fn cmp(&self, other: &Self) -> Ordering {
        // Invertir para min-heap
        other.time.partial_cmp(&self.time).unwrap_or(Ordering::Equal)
    }
}

// ── Estadísticas ──

struct Stats {
    total_wait: f64,
    total_service: f64,
    customers_served: u32,
    max_queue_length: usize,
}

// ── Simulación ──

fn simulate(arrival_rate: f64, service_rate: f64,
            num_servers: u32, max_customers: u32) {
    let mut rng = Rng::new(42);
    let mut events = BinaryHeap::new();
    let mut wait_queue: VecDeque<f64> = VecDeque::new();  // arrival times
    let mut stats = Stats {
        total_wait: 0.0, total_service: 0.0,
        customers_served: 0, max_queue_length: 0,
    };
    let mut clock = 0.0;
    let mut servers_busy: u32 = 0;
    let mut arrivals: u32 = 0;

    // Primera llegada
    events.push(Event {
        time: rng.exponential(arrival_rate),
        event_type: EventType::Arrival,
    });

    while let Some(event) = events.pop() {
        if stats.customers_served >= max_customers { break; }
        clock = event.time;

        match event.event_type {
            EventType::Arrival => {
                arrivals += 1;
                if arrivals < max_customers {
                    events.push(Event {
                        time: clock + rng.exponential(arrival_rate),
                        event_type: EventType::Arrival,
                    });
                }

                if servers_busy < num_servers {
                    servers_busy += 1;
                    let svc = rng.exponential(service_rate);
                    stats.total_service += svc;
                    events.push(Event {
                        time: clock + svc,
                        event_type: EventType::Departure,
                    });
                } else {
                    wait_queue.push_back(clock);
                    if wait_queue.len() > stats.max_queue_length {
                        stats.max_queue_length = wait_queue.len();
                    }
                }
            }

            EventType::Departure => {
                stats.customers_served += 1;

                if let Some(arrival) = wait_queue.pop_front() {
                    stats.total_wait += clock - arrival;
                    let svc = rng.exponential(service_rate);
                    stats.total_service += svc;
                    events.push(Event {
                        time: clock + svc,
                        event_type: EventType::Departure,
                    });
                } else {
                    servers_busy -= 1;
                }
            }
        }
    }

    let rho = arrival_rate / (num_servers as f64 * service_rate);
    let served = stats.customers_served as f64;
    println!("=== Simulación M/M/{num_servers} ===");
    println!("Tasa de llegada (λ):    {arrival_rate:.2}");
    println!("Tasa de servicio (μ):   {service_rate:.2}");
    println!("Utilización (ρ):        {rho:.2}");
    println!("Clientes atendidos:     {}", stats.customers_served);
    println!("Espera promedio:        {:.3}", stats.total_wait / served);
    println!("Servicio promedio:      {:.3}", stats.total_service / served);
    println!("Cola máxima:            {}", stats.max_queue_length);
    println!("Tiempo simulado:        {clock:.2}");

    if num_servers == 1 && rho < 1.0 {
        let wq = rho / (service_rate * (1.0 - rho));
        println!("\n--- Teórico M/M/1 ---");
        println!("Espera promedio (Wq):   {wq:.3}");
    }
}

fn main() {
    simulate(0.8, 1.0, 1, 10000);    // M/M/1, ρ=0.8
    println!();
    simulate(2.5, 1.0, 3, 10000);    // M/M/3, ρ=0.83
}
```

---

## Ejemplo de salida y verificación

Para M/M/1 con $\lambda = 0.8$, $\mu = 1.0$ ($\rho = 0.8$):

```
=== Simulación M/M/1 ===
Tasa de llegada (λ):    0.80
Tasa de servicio (μ):   1.00
Utilización (ρ):        0.80
Clientes atendidos:     10000
Espera promedio:        3.987
Servicio promedio:      1.004
Cola máxima:            32

--- Teórico M/M/1 ---
Espera promedio (Wq):   4.000
```

El valor simulado (~3.99) converge al teórico (4.0) con $n$ suficiente.
Cuanto mayor sea $n$, más precisa la convergencia.

### Efecto de la utilización

| $\rho$ | $W_q$ teórico | Cola promedio | Observación |
|--------|---------------|---------------|-------------|
| 0.2 | 0.25 | 0.05 | Sistema casi vacío |
| 0.5 | 1.00 | 0.5 | Espera razonable |
| 0.8 | 4.00 | 3.2 | Esperas largas |
| 0.9 | 9.00 | 8.1 | Casi saturado |
| 0.95 | 19.0 | 18.1 | Cola enorme |
| 0.99 | 99.0 | 98.0 | Prácticamente colapsado |

La espera crece como $\rho / (1 - \rho)$ — una hipérbola que explota
cuando $\rho \to 1$.  Pasar de $\rho = 0.8$ a $\rho = 0.9$ duplica la espera.
Pasar de 0.9 a 0.95 la duplica de nuevo.

---

## Múltiples colas vs una sola cola

En un supermercado real hay dos modelos:

### Modelo A: una cola, $k$ cajeros

```
            ┌── Cajero 1
  Cola ─────┼── Cajero 2
            └── Cajero 3
```

Un cliente espera en una fila única y va al primer cajero libre.

### Modelo B: $k$ colas, $k$ cajeros

```
  Cola 1 ── Cajero 1
  Cola 2 ── Cajero 2
  Cola 3 ── Cajero 3
```

Cada cliente elige una cola (típicamente la más corta) y se queda en ella.

### ¿Cuál es mejor?

El modelo A (una cola) es **siempre mejor o igual** en tiempo de espera
promedio.  ¿Por qué?  Porque un cajero que se libera siempre atiende al
cliente que lleva más tiempo esperando (FIFO global).  En el modelo B, un
cajero puede estar libre mientras otro tiene cola — hay ineficiencia.

```c
/* Modelo B: k colas independientes M/M/1 con tasa λ/k */
void simulate_multi_queue(double arrival_rate, double service_rate,
                          int num_cashiers, int max_customers) {
    /* Cada cajero es un sistema M/M/1 independiente con λ/k */
    /* ... crear k colas y k variables server_busy ... */

    /* Al llegar un cliente: elegir la cola más corta */
    int shortest = 0;
    for (int i = 1; i < num_cashiers; i++) {
        if (queue_size[i] < queue_size[shortest]) {
            shortest = i;
        }
    }
    /* Encolar en la cola elegida */
}
```

Para simular el modelo B completo, se necesita un sistema de $k$ colas
independientes.  El resultado: la espera promedio es mayor que en el modelo A,
especialmente bajo alta utilización.

---

## Métricas de la simulación

### Métricas de interés

| Métrica | Cálculo | Significado |
|---------|---------|-------------|
| **Espera promedio** ($W_q$) | `total_wait / served` | Tiempo medio en cola antes de servicio |
| **Tiempo en sistema** ($W$) | `(total_wait + total_service) / served` | Espera + servicio |
| **Largo promedio de cola** ($L_q$) | Integral del largo / tiempo total | Clientes promedio esperando |
| **Utilización** ($\rho$) | `server_busy_time / total_time` | Fracción del tiempo ocupado |
| **Cola máxima** | `max(queue.count)` durante simulación | Pico de demanda |
| **Throughput** | `served / total_time` | Clientes procesados por unidad de tiempo |

### Cálculo del largo promedio de cola

Para calcular $L_q$ con precisión, se necesita integrar el largo de la cola
sobre el tiempo.  Cada vez que el largo cambia (enqueue o dequeue de la cola
de espera), actualizar:

```c
/* Antes de cambiar el largo de la cola: */
stats.queue_area += wq.count * (clock - stats.last_event_time);
stats.last_event_time = clock;

/* Al final: */
double avg_queue = stats.queue_area / clock;
```

### Ley de Little

La **Ley de Little** conecta las métricas sin necesidad de conocer las
distribuciones:

$$L = \lambda \cdot W$$

- $L$ = número promedio de clientes en el sistema.
- $\lambda$ = tasa de llegada.
- $W$ = tiempo promedio en el sistema.

También aplica solo a la cola: $L_q = \lambda \cdot W_q$.

Es una relación universal — sirve para verificar la consistencia de la
simulación.

---

## Estructura de datos: resumen del rol de cada cola

| Estructura | Tipo | Uso en la simulación |
|-----------|------|----------------------|
| Cola de eventos | Cola de prioridad (min por tiempo) | Almacenar y ordenar todos los eventos futuros |
| Cola de espera | Cola FIFO | Clientes esperando un servidor libre |
| Servidores | Contador o array de bool | Rastrear disponibilidad |

Este es un ejemplo concreto de por qué necesitamos diferentes tipos de colas:
la cola de eventos necesita **prioridad** (ordenar por tiempo), la cola de
espera necesita **FIFO** (justicia — el primero en llegar es el primero en ser
atendido).

---

## Ejercicios

### Ejercicio 1 — Traza manual M/M/1

Traza manualmente una simulación M/M/1 con los siguientes tiempos
predeterminados (no aleatorios):

```
Llegadas en:     t=0, t=3, t=5, t=7, t=9
Servicio dura:   4, 3, 2, 5, 3
```

Muestra la cola de eventos, la cola de espera, y el estado del servidor después
de procesar cada evento.

<details>
<summary>Predice la espera de cada cliente</summary>

```
Cliente 1: llega t=0, servidor libre → servicio [0, 4).  Espera: 0.
Cliente 2: llega t=3, servidor ocupado → cola.  Servidor libre t=4.
           Servicio [4, 7).  Espera: 4-3 = 1.
Cliente 3: llega t=5, servidor ocupado → cola.  Servidor libre t=7.
           Servicio [7, 9).  Espera: 7-5 = 2.
Cliente 4: llega t=7, servidor ocupado → cola.  Servidor libre t=9.
           Servicio [9, 14).  Espera: 9-7 = 2.
Cliente 5: llega t=9, servidor ocupado → cola.  Servidor libre t=14.
           Servicio [14, 17).  Espera: 14-9 = 5.

Espera promedio: (0+1+2+2+5)/5 = 2.0
```
</details>

### Ejercicio 2 — Efecto de ρ

Ejecuta la simulación M/M/1 para $\rho = 0.5, 0.7, 0.8, 0.9, 0.95$ (con
$\mu = 1.0$ y $\lambda = \rho \cdot \mu$).  Grafica o tabula la espera
promedio simulada vs la teórica $W_q = \rho / (\mu(1-\rho))$.

<details>
<summary>Predicción</summary>

Los valores simulados deben converger a los teóricos para $n$ grande.  La
convergencia es más lenta para $\rho$ alta porque la varianza del sistema
aumenta.  Para $\rho = 0.95$ se necesitan más de 100,000 clientes para una
buena estimación.
</details>

### Ejercicio 3 — M/M/k

Compara la espera promedio de:
- M/M/1 con $\lambda = 2.4$, $\mu = 3.0$ ($\rho = 0.8$).
- M/M/3 con $\lambda = 2.4$, $\mu = 1.0$ ($\rho = 0.8$).

Ambos tienen la misma utilización.  ¿Cuál tiene menor espera?

<details>
<summary>Resultado</summary>

M/M/3 tiene menor espera.  Con un solo servidor rápido, un cliente con servicio
largo bloquea a todos.  Con 3 servidores, los otros 2 pueden seguir atendiendo.
La varianza del tiempo en cola es menor con más servidores — la ley de los
grandes números suaviza las fluctuaciones.
</details>

### Ejercicio 4 — Una cola vs múltiples colas

Simula 3 cajeros con:
(a) Una cola única (modelo A, M/M/3).
(b) 3 colas independientes (modelo B, cada una M/M/1 con $\lambda/3$).

Usa $\lambda = 2.4$, $\mu = 1.0$.  Compara la espera promedio.

<details>
<summary>Predicción</summary>

El modelo A (una cola) tendrá menor espera.  En el modelo B, un cajero puede
estar libre mientras otro tiene 5 clientes en cola.  En el modelo A, ese cajero
libre tomaría inmediatamente al siguiente de la cola global.

La diferencia es más pronunciada con $\rho$ alto.  Para $\rho$ bajo, ambos
modelos son similares.
</details>

### Ejercicio 5 — Ley de Little

Modifica la simulación para calcular $L$ (clientes promedio en el sistema) y
$W$ (tiempo promedio en el sistema).  Verifica la Ley de Little: $L = \lambda
\cdot W$.

<details>
<summary>Cómo calcular L</summary>

Mantener un acumulador `system_area` que se actualiza cada vez que cambia el
número de clientes en el sistema (llegada o salida):

```c
system_area += clients_in_system * (clock - last_time);
last_time = clock;
```

Al final: `L = system_area / clock`.  Verificar `L ≈ λ * W`.
</details>

### Ejercicio 6 — Histograma de esperas

Registra el tiempo de espera de cada cliente (no solo el promedio) y genera un
histograma.  ¿La distribución es simétrica o sesgada?

<details>
<summary>Resultado esperado</summary>

La distribución de esperas en M/M/1 es exponencial (sesgada a la derecha):
muchos clientes esperan poco, pocos esperan mucho.  La mediana es menor que la
media.  Esto implica que el "cliente promedio" experimenta una espera menor que
el promedio aritmético — unos pocos clientes muy desafortunados elevan la media.
</details>

### Ejercicio 7 — Simulación con tiempo de servicio fijo

Cambia el servicio de exponencial a **determinístico** (siempre dura
exactamente $1/\mu$).  Esto modela un sistema M/D/1.  Compara la espera con
M/M/1 para la misma $\rho$.

<details>
<summary>Resultado teórico</summary>

La espera en M/D/1 es exactamente la mitad que en M/M/1:

$$W_q^{M/D/1} = \frac{\rho}{2\mu(1-\rho)} = \frac{1}{2} W_q^{M/M/1}$$

La variabilidad del servicio aumenta la espera.  Si el servicio es constante,
no hay "mala suerte" — ningún cliente acapara el servidor con un servicio
excepcionalmente largo.
</details>

### Ejercicio 8 — Simulación en Rust completa

Implementa la simulación M/M/k completa en Rust usando `BinaryHeap` para la
cola de eventos y `VecDeque` para la cola de espera.  Parametriza con
$\lambda$, $\mu$, $k$ y $n$.  Imprime todas las métricas del tópico.

<details>
<summary>Pista de estructura</summary>

El código del tópico ya tiene la implementación en Rust.  Extiéndelo para
calcular: espera promedio, tiempo en sistema, largo promedio de cola (con
acumulador de área), utilización medida (vs calculada), y verificar Little.
</details>

### Ejercicio 9 — Añadir tipo de evento SERVICE_START

Refina el modelo añadiendo un tercer tipo de evento `SERVICE_START` que se
programa cuando un cliente pasa de la cola al servidor.  Esto permite registrar
el tiempo exacto de inicio de servicio para cada cliente.

<details>
<summary>Ventaja</summary>

Con 3 tipos de evento (ARRIVAL, SERVICE_START, DEPARTURE), la simulación
calcula exactamente: tiempo en cola = SERVICE_START - ARRIVAL, y tiempo de
servicio = DEPARTURE - SERVICE_START.  Esto separa las métricas de espera y
servicio con mayor precisión, especialmente útil si el tiempo de servicio no
es exponencial.
</details>

### Ejercicio 10 — Simulación de red de colas

Modela un sistema con 2 estaciones en serie: los clientes primero pasan por la
estación 1 (ej. recepción), luego por la estación 2 (ej. atención).  La salida
de la estación 1 alimenta la cola de la estación 2.

<details>
<summary>Estructura</summary>

```
Llegadas → [Cola 1] → Servidor 1 → [Cola 2] → Servidor 2 → Salida
```

Eventos: ARRIVAL_1, DEPARTURE_1 (= ARRIVAL_2), DEPARTURE_2.  Cada estación
tiene su propia cola FIFO y su tasa de servicio $\mu_1$, $\mu_2$.  El
throughput del sistema está limitado por la estación más lenta
($\min(\mu_1, \mu_2)$).  Si $\lambda > \min(\mu_1, \mu_2)$, la cola de la
estación más lenta crece indefinidamente.
</details>
