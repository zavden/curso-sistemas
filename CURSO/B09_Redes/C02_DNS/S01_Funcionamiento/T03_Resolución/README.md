# Resolución DNS

## Índice
1. [El proceso de resolución paso a paso](#el-proceso-de-resolución-paso-a-paso)
2. [Consulta recursiva](#consulta-recursiva)
3. [Consulta iterativa](#consulta-iterativa)
4. [El stub resolver](#el-stub-resolver)
5. [Caching multinivel](#caching-multinivel)
6. [TTL en profundidad](#ttl-en-profundidad)
7. [Negative caching](#negative-caching)
8. [Secciones de una respuesta DNS](#secciones-de-una-respuesta-dns)
9. [Flags y códigos de respuesta](#flags-y-códigos-de-respuesta)
10. [EDNS y extensiones modernas](#edns-y-extensiones-modernas)
11. [Errores comunes](#errores-comunes)
12. [Cheatsheet](#cheatsheet)
13. [Ejercicios](#ejercicios)

---

## El proceso de resolución paso a paso

Veamos el recorrido completo de una consulta DNS desde que el usuario escribe una URL hasta que obtiene la IP, con todos los actores involucrados:

```
┌──────────────────────────────────────────────────────────────┐
│         Resolución de www.example.com (sin caché)            │
│                                                              │
│  ┌──────────┐                                                │
│  │ Navegador │                                               │
│  │ (app)    │  1. "¿IP de www.example.com?"                  │
│  └────┬─────┘                                                │
│       │ llamada getaddrinfo() / gethostbyname()              │
│       ▼                                                      │
│  ┌──────────────┐                                            │
│  │ Stub resolver│  2. Consulta /etc/hosts (si nsswitch dice) │
│  │ (libc/SO)   │     No encontrado → consulta al recursivo  │
│  └────┬─────────┘                                            │
│       │ UDP/53 (consulta recursiva, flag RD=1)               │
│       ▼                                                      │
│  ┌──────────────────┐                                        │
│  │ Resolver recursivo│  3. ¿Tengo esto en caché?             │
│  │ (8.8.8.8)        │     No → inicio resolución iterativa  │
│  └────┬─────────────┘                                        │
│       │                                                      │
│       │  4. ¿Dónde están los servidores de .com?             │
│       ├───────────────────────►  Root server (.)             │
│       │  ◄─── Referral: NS de .com + glue records            │
│       │                                                      │
│       │  5. ¿Dónde están los NS de example.com?              │
│       ├───────────────────────►  TLD server (.com)           │
│       │  ◄─── Referral: NS de example.com + glue             │
│       │                                                      │
│       │  6. ¿Cuál es la IP de www.example.com?               │
│       ├───────────────────────►  Autoritativo (example.com)  │
│       │  ◄─── Answer: A 93.184.216.34, TTL=86400, aa=1      │
│       │                                                      │
│       │  7. Cachea la respuesta (TTL=86400)                  │
│       ▼                                                      │
│  Devuelve 93.184.216.34 al stub resolver                     │
│       │                                                      │
│       ▼                                                      │
│  Stub resolver → Navegador → conexión TCP a 93.184.216.34   │
└──────────────────────────────────────────────────────────────┘
```

---

## Consulta recursiva

En una consulta recursiva, el cliente pide al servidor que haga **todo el trabajo** y devuelva la respuesta final:

```
┌──────────────────────────────────────────────────────────────┐
│                  Consulta recursiva                           │
│                                                              │
│  Cliente (stub) ──── "dame la IP de www.example.com" ──────► │
│                      flag RD=1 (Recursion Desired)           │
│                                                              │
│                 ◄─── "93.184.216.34" ────────────────────── │
│                      flag RA=1 (Recursion Available)         │
│                                                              │
│  El cliente NO sabe ni le importa cuántos servidores         │
│  consultó el recursivo. Solo quiere la respuesta final.      │
│                                                              │
│  ¿Quién hace consultas recursivas?                           │
│  • Tu stub resolver (libc) → al resolver recursivo           │
│  • Aplicaciones → al resolver del SO                         │
│                                                              │
│  ¿Quién acepta consultas recursivas?                         │
│  • Resolvers recursivos (8.8.8.8, 1.1.1.1, DNS del ISP)     │
│  • systemd-resolved (resolver local)                         │
│                                                              │
│  ¿Quién NO acepta consultas recursivas?                      │
│  • Root servers (rechazarían el flag RD=1)                   │
│  • Servidores autoritativos (solo responden lo que tienen)    │
└──────────────────────────────────────────────────────────────┘
```

---

## Consulta iterativa

En una consulta iterativa, el servidor responde con lo **mejor que sabe**: la respuesta directa si la tiene, o una referencia (referral) al siguiente servidor que podría saberlo:

```
┌──────────────────────────────────────────────────────────────┐
│                  Consulta iterativa                           │
│                                                              │
│  Resolver                                                    │
│    │                                                         │
│    │── "¿www.example.com?" ──► Root                          │
│    │◄── "No sé. Prueba .com:                                 │
│    │     a.gtld-servers.net (192.5.6.30)"  ← REFERRAL        │
│    │                                                         │
│    │── "¿www.example.com?" ──► .com TLD                      │
│    │◄── "No sé. Prueba example.com:                          │
│    │     ns1.example.com (93.184.216.1)"   ← REFERRAL        │
│    │                                                         │
│    │── "¿www.example.com?" ──► ns1.example.com               │
│    │◄── "93.184.216.34"                    ← ANSWER          │
│    │                                                         │
│    ▼                                                         │
│  Fin. El resolver obtuvo la respuesta en 3 pasos.            │
│                                                              │
│  ¿Quién hace consultas iterativas?                           │
│  • El resolver recursivo (hacia root/TLD/autoritativos)      │
│                                                              │
│  Cada servidor consultado iterativamente:                    │
│  • Responde inmediatamente con lo que sabe                   │
│  • NO consulta otros servidores en tu nombre                 │
│  • Tipo de respuesta: referral (NS + glue) o answer         │
└──────────────────────────────────────────────────────────────┘
```

### Recursiva vs iterativa: resumen

```
┌─────────────────┬────────────────────┬─────────────────────┐
│                 │ Recursiva          │ Iterativa           │
├─────────────────┼────────────────────┼─────────────────────┤
│ Quién trabaja   │ El servidor        │ El cliente (resolver)│
│ Respuesta       │ Final (la IP)      │ Final o referral    │
│ Flag            │ RD=1               │ RD=0                │
│ Quién la usa    │ Stub → Recursivo   │ Recursivo → Auth    │
│ Carga           │ En el servidor     │ En el cliente       │
│ Analogía        │ "Resuelve esto     │ "No sé, pero       │
│                 │  por mí"           │  pregunta a este"   │
└─────────────────┴────────────────────┴─────────────────────┘
```

### ¿Por qué no todo es recursivo?

```
┌──────────────────────────────────────────────────────────────┐
│  Si los root servers aceptaran recursión:                    │
│                                                              │
│  • Cada root server haría el trabajo COMPLETO por cada       │
│    consulta de cada resolver del mundo.                      │
│  • Carga de CPU y red: imposible de escalar.                 │
│  • Amplificación DDoS: un atacante podría enviar millones    │
│    de consultas recursivas a los root servers.               │
│  • Caché inútil: los root servers cachearían respuestas      │
│    de millones de dominios diferentes (ineficiente).         │
│                                                              │
│  La iteración distribuye la carga:                           │
│  Root servers: solo responden "¿dónde está .com?" (trivial)  │
│  TLD servers: solo responden "¿dónde está example.com?"      │
│  Resolver recursivo: hace el recorrido y CACHEA el resultado │
│                                                              │
│  El caching del resolver protege a los autoritativos:        │
│  Si 1000 usuarios piden www.example.com en 1 hora,           │
│  solo la PRIMERA consulta llega al autoritativo.             │
│  Las 999 restantes se sirven desde caché.                    │
└──────────────────────────────────────────────────────────────┘
```

---

## El stub resolver

El **stub resolver** es el componente del sistema operativo que las aplicaciones usan para resolver nombres. No es un resolver completo — solo sabe enviar consultas recursivas a un resolver configurado:

```
┌──────────────────────────────────────────────────────────────┐
│                  Stub resolver                               │
│                                                              │
│  Aplicación (curl, firefox, nginx...)                        │
│      │                                                       │
│      │ getaddrinfo("www.example.com", ...)                   │
│      ▼                                                       │
│  ┌────────────────────────────────────────────────┐          │
│  │ Stub resolver (glibc / musl / systemd-resolved)│          │
│  │                                                │          │
│  │ 1. Consulta nsswitch.conf para saber el orden: │          │
│  │    hosts: files dns myhostname                 │          │
│  │    ─────  ─────  ───  ──────────               │          │
│  │     │      │      │      │                     │          │
│  │     │      │      │      └── hostname local    │          │
│  │     │      │      └── DNS (consulta al resolver)│         │
│  │     │      └── /etc/hosts (busca en archivo)   │          │
│  │     └── fuente: archivos locales               │          │
│  │                                                │          │
│  │ 2. Si /etc/hosts tiene el nombre → devuelve IP │          │
│  │ 3. Si no → envía consulta DNS al nameserver    │          │
│  │    configurado en /etc/resolv.conf              │          │
│  │ 4. Espera respuesta → devuelve IP a la app     │          │
│  └────────────────────────────────────────────────┘          │
│                                                              │
│  El stub resolver NO hace resolución iterativa.              │
│  NO contacta root servers ni autoritativos.                  │
│  Solo envía una consulta recursiva y espera la respuesta.    │
└──────────────────────────────────────────────────────────────┘
```

### nsswitch.conf

```bash
# /etc/nsswitch.conf controla el orden de resolución:
grep ^hosts /etc/nsswitch.conf
# hosts: files dns myhostname

# files    → buscar en /etc/hosts primero
# dns      → después consultar DNS
# myhostname → resolver el hostname local

# Con systemd-resolved puede aparecer:
# hosts: mymachines resolve [!UNAVAIL=return] files myhostname dns
#        ──────────  ───────                   ─────
#        containers  systemd-resolved          /etc/hosts
```

### /etc/resolv.conf

```bash
cat /etc/resolv.conf
# nameserver 192.168.1.1        ← resolver primario
# nameserver 8.8.8.8            ← resolver secundario (fallback)
# search home.local corp.local  ← search domains
# options ndots:5 timeout:2 attempts:3

# nameserver: IP del resolver recursivo (máximo 3)
# search: dominios a probar si el nombre no tiene suficientes puntos
# ndots: mínimo de puntos para tratar como FQDN
#   ndots:1 (default): "server" → prueba server.home.local primero
#   ndots:5: "api.v2.staging.myapp" tiene 3 puntos < 5
#            → prueba api.v2.staging.myapp.home.local primero
#            (relevante en Kubernetes, donde ndots:5 es el default)
```

### La trampa de ndots en Kubernetes

```
┌──────────────────────────────────────────────────────────────┐
│  En Kubernetes, /etc/resolv.conf típicamente tiene:          │
│                                                              │
│  nameserver 10.96.0.10                                       │
│  search default.svc.cluster.local svc.cluster.local          │
│         cluster.local                                        │
│  options ndots:5                                             │
│                                                              │
│  Resolución de "api.example.com" (2 puntos < 5):             │
│  1. api.example.com.default.svc.cluster.local → NXDOMAIN    │
│  2. api.example.com.svc.cluster.local → NXDOMAIN            │
│  3. api.example.com.cluster.local → NXDOMAIN                │
│  4. api.example.com. → ¡ÉXITO!                              │
│                                                              │
│  ¡3 consultas desperdiciadas antes de la correcta!           │
│                                                              │
│  Solución: usar FQDN con trailing dot en configuraciones:    │
│  api.example.com.  ← el punto final evita los search domains│
└──────────────────────────────────────────────────────────────┘
```

---

## Caching multinivel

Las respuestas DNS se cachean en múltiples niveles, cada uno reduciendo la carga en los servidores autoritativos:

```
┌──────────────────────────────────────────────────────────────┐
│                Niveles de caché DNS                           │
│                                                              │
│  Nivel 1: Caché de la aplicación                             │
│  ─────────────────────────────────                           │
│  • Navegadores: Chrome cachea DNS internamente (~1 min)      │
│    chrome://net-internals/#dns                               │
│  • Aplicaciones: algunas librerías cachean resoluciones      │
│  • JVM: cachea DNS agresivamente (puede ser un problema)     │
│                                                              │
│  Nivel 2: Caché del SO / stub resolver                       │
│  ─────────────────────────────────────                       │
│  • systemd-resolved (Fedora, Ubuntu): caché local            │
│  • nscd (Name Service Cache Daemon): caché legacy            │
│  • macOS: mDNSResponder                                      │
│  • Windows: DNS Client service                               │
│                                                              │
│  Nivel 3: Caché del resolver recursivo                       │
│  ─────────────────────────────────────                       │
│  • 8.8.8.8 / 1.1.1.1 / DNS del ISP                          │
│  • Este es el caché MÁS significativo                        │
│  • Compartido entre todos los clientes que usan ese resolver │
│  • Si 1 usuario resolvió google.com, los demás usan el caché│
│                                                              │
│  Nivel 4: Caché de referrals                                 │
│  ────────────────────────────                                │
│  • El resolver cachea los NS de .com (TTL ~48h)              │
│  • Evita consultar a root servers para cada dominio .com     │
│  • También cachea NS de example.com (según su TTL)           │
│                                                              │
│  Efecto cascada:                                             │
│  Primera consulta: 4 queries (root → TLD → auth → respuesta)│
│  Segunda consulta mismo dominio: 0 queries (caché nivel 3)   │
│  Consulta a otro .com: 1 query (TLD cacheado, solo auth)    │
└──────────────────────────────────────────────────────────────┘
```

### Ver y limpiar la caché

```bash
# systemd-resolved: ver estadísticas de caché
resolvectl statistics
# Cache:
#   Current Size: 142
#   Cache Hits: 1847
#   Cache Misses: 523

# Limpiar caché de systemd-resolved
resolvectl flush-caches

# Limpiar caché de nscd (si se usa)
sudo nscd -i hosts

# Limpiar caché de Chrome
# Navegar a: chrome://net-internals/#dns → "Clear host cache"
```

---

## TTL en profundidad

El TTL (Time To Live) controla cuánto tiempo un registro puede permanecer en caché. Es un equilibrio entre rendimiento y agilidad:

```
┌──────────────────────────────────────────────────────────────┐
│  dig www.example.com                                         │
│                                                              │
│  Primera consulta (desde el autoritativo):                   │
│  www.example.com.  86400  IN  A  93.184.216.34               │
│                    ─────                                     │
│                    TTL original: 24 horas                    │
│                                                              │
│  10 minutos después (desde caché del resolver):              │
│  www.example.com.  85800  IN  A  93.184.216.34               │
│                    ─────                                     │
│                    TTL restante: 23h 50m                     │
│                    (86400 - 600 = 85800)                     │
│                                                              │
│  El TTL DECREMENTA con el tiempo en la caché.                │
│  Cuando llega a 0, el registro se purga y la próxima         │
│  consulta va al autoritativo de nuevo.                       │
└──────────────────────────────────────────────────────────────┘
```

### Estrategias de TTL

```
┌────────────────────┬──────────┬────────────────────────────────┐
│ Escenario          │ TTL      │ Razonamiento                   │
├────────────────────┼──────────┼────────────────────────────────┤
│ Servicio estable   │ 86400    │ IP rara vez cambia, minimizar  │
│ (24h)              │          │ consultas al autoritativo      │
├────────────────────┼──────────┼────────────────────────────────┤
│ Servicio normal    │ 3600     │ Balance entre caché y agilidad │
│ (1h)               │          │                                │
├────────────────────┼──────────┼────────────────────────────────┤
│ Failover activo    │ 60-300   │ Cambio de IP rápido ante fallo │
│ (1-5 min)          │          │ (health checks + DNS update)   │
├────────────────────┼──────────┼────────────────────────────────┤
│ Registro NS de TLD │ 172800   │ Los TLDs cambian raramente     │
│ (48h)              │          │                                │
├────────────────────┼──────────┼────────────────────────────────┤
│ Pre-migración      │ 300→     │ Bajar TTL antes de cambiar IP  │
│                    │ cambio→  │ para que la propagación sea    │
│                    │ 3600     │ rápida. Subir después.         │
└────────────────────┴──────────┴────────────────────────────────┘
```

### "Propagación DNS" — lo que realmente significa

```
┌──────────────────────────────────────────────────────────────┐
│  No existe la "propagación DNS" como fenómeno activo.        │
│                                                              │
│  Cuando cambias un registro DNS:                             │
│  • El cambio es INSTANTÁNEO en el servidor autoritativo.     │
│  • Los resolvers recursivos siguen usando su caché           │
│    hasta que el TTL expire.                                  │
│  • Diferentes resolvers tienen diferentes tiempos restantes. │
│                                                              │
│  "Propagación" = esperar a que todos los cachés expiren.     │
│  Tiempo máximo = el TTL que tenía el registro ANTERIOR.      │
│                                                              │
│  Si el TTL era 86400 (24h) antes del cambio:                 │
│  • Algunos usuarios verán el cambio en minutos (su caché     │
│    estaba cerca de expirar)                                  │
│  • Otros tardarán hasta 24h (su caché se llenó justo antes)  │
│                                                              │
│  Por eso se baja el TTL ANTES de cambiar:                    │
│  1. Bajar TTL a 300 (5 min)                                  │
│  2. Esperar que el TTL anterior expire (hasta 24h)           │
│  3. Ahora todos los cachés tienen TTL=300                    │
│  4. Cambiar la IP                                            │
│  5. En ≤5 min, todos ven la IP nueva                         │
│  6. Subir TTL de nuevo                                       │
└──────────────────────────────────────────────────────────────┘
```

### Resolvers que no respetan TTL

```
┌──────────────────────────────────────────────────────────────┐
│  Algunos resolvers modifican los TTLs:                       │
│                                                              │
│  • Imponen TTL mínimo: si el autoritativo dice TTL=30,       │
│    el resolver puede cachearlo por 300 como mínimo.          │
│    (Algunos ISPs hacen esto para reducir carga)              │
│                                                              │
│  • Imponen TTL máximo: si el autoritativo dice TTL=604800,   │
│    el resolver puede limitarlo a 86400.                      │
│                                                              │
│  • Prefetching: Google DNS (8.8.8.8) renueva registros       │
│    populares ANTES de que el TTL expire, para que siempre    │
│    estén en caché (latencia 0 para consultas populares).     │
│                                                              │
│  Implicación: bajar el TTL a 60 NO garantiza que TODOS       │
│  los resolvers del mundo lo respeten. Pero la mayoría sí.    │
└──────────────────────────────────────────────────────────────┘
```

---

## Negative caching

Cuando un dominio no existe, la respuesta NXDOMAIN también se cachea. Esto evita que consultas repetidas a dominios inexistentes sobrecarguen a los autoritativos:

```
┌──────────────────────────────────────────────────────────────┐
│                 Negative caching                             │
│                                                              │
│  dig noexiste.example.com                                    │
│                                                              │
│  Respuesta del autoritativo:                                 │
│  ;; ->>HEADER<<- status: NXDOMAIN                            │
│  ;; AUTHORITY SECTION:                                       │
│  example.com.  3600  IN  SOA  ns1.example.com. admin...      │
│                ────                                          │
│                └── TTL del negative cache (del campo         │
│                    Minimum del SOA, o el TTL del SOA RR,     │
│                    el MENOR de los dos)                       │
│                                                              │
│  El resolver cachea:                                         │
│  "noexiste.example.com NO EXISTE" durante 3600 segundos.     │
│                                                              │
│  Consecuencia:                                               │
│  Si creas noexiste.example.com en el autoritativo,           │
│  nadie lo verá hasta que expire el negative cache.           │
│  Esto afecta especialmente a nuevos subdominios.             │
│                                                              │
│  NODATA (registro existe pero no del tipo consultado):       │
│  dig example.com AAAA  (si solo tiene A)                     │
│  → status: NOERROR, pero ANSWER vacía                        │
│  → También se cachea como negativo (misma duración)          │
└──────────────────────────────────────────────────────────────┘
```

```bash
# Ver negative cache en acción
dig noexistexxxyyy.example.com
# ;; ->>HEADER<<- status: NXDOMAIN

# Consultar inmediatamente de nuevo:
dig noexistexxxyyy.example.com
# El TTL será menor (decrementando desde el caché)
```

---

## Secciones de una respuesta DNS

Una respuesta DNS tiene una estructura bien definida con múltiples secciones:

```bash
dig www.example.com
```

```
┌──────────────────────────────────────────────────────────────┐
│ ;; ->>HEADER<<- opcode: QUERY, status: NOERROR, id: 12345   │
│ ;; flags: qr rd ra; QUERY: 1, ANSWER: 1, AUTHORITY: 2,      │
│ ;;                   ADDITIONAL: 3                           │
│                                                              │
│ ;; QUESTION SECTION:                    ← qué preguntaste   │
│ ;www.example.com.          IN    A                           │
│                                                              │
│ ;; ANSWER SECTION:                      ← la respuesta      │
│ www.example.com.    86400  IN    A    93.184.216.34           │
│                                                              │
│ ;; AUTHORITY SECTION:                   ← NS de la zona     │
│ example.com.        86400  IN    NS   ns1.example.com.       │
│ example.com.        86400  IN    NS   ns2.example.com.       │
│                                                              │
│ ;; ADDITIONAL SECTION:                  ← datos extra útiles │
│ ns1.example.com.    86400  IN    A    93.184.216.1            │
│ ns2.example.com.    86400  IN    A    93.184.216.2            │
│                                                              │
│ ;; Query time: 45 msec                                       │
│ ;; SERVER: 192.168.1.1#53(192.168.1.1) (UDP)                │
│ ;; WHEN: Fri Mar 21 10:30:00 CET 2026                       │
│ ;; MSG SIZE  rcvd: 156                                       │
└──────────────────────────────────────────────────────────────┘
```

| Sección | Contenido | Cuándo aparece |
|---------|-----------|----------------|
| QUESTION | Lo que preguntaste (nombre + tipo) | Siempre |
| ANSWER | La respuesta directa (RRs) | Si existe el registro |
| AUTHORITY | NS de la zona autoritativa | En referrals y respuestas |
| ADDITIONAL | IPs de los NS (glue records) | Si hay NS en authority |

### Referral vs Answer

```
┌──────────────────────────────────────────────────────────────┐
│  ANSWER (respuesta final, flag aa=1):                        │
│  • ANSWER section contiene el registro pedido                │
│  • AUTHORITY section puede tener NS (informativo)            │
│  • El servidor ES autoritativo para esta zona                │
│                                                              │
│  REFERRAL (derivación, flag aa=0):                           │
│  • ANSWER section VACÍA                                      │
│  • AUTHORITY section contiene NS del siguiente nivel         │
│  • ADDITIONAL section contiene IPs de esos NS (glue)         │
│  • "No sé, pero pregunta a estos servidores"                 │
└──────────────────────────────────────────────────────────────┘
```

---

## Flags y códigos de respuesta

### Flags del header DNS

```
┌──────────────────────────────────────────────────────────────┐
│  ;; flags: qr rd ra aa                                       │
│            ── ── ── ──                                       │
│            │  │  │  │                                        │
│            │  │  │  └── AA (Authoritative Answer)            │
│            │  │  │      1 = respuesta del autoritativo        │
│            │  │  │      0 = respuesta desde caché             │
│            │  │  │                                            │
│            │  │  └── RA (Recursion Available)                 │
│            │  │      1 = el servidor soporta recursión        │
│            │  │                                               │
│            │  └── RD (Recursion Desired)                      │
│            │      1 = el cliente quiere recursión             │
│            │                                                  │
│            └── QR (Query/Response)                            │
│                0 = query, 1 = response                        │
│                                                               │
│  Otros flags:                                                 │
│  TC (TrunCation): 1 = respuesta truncada (>512B UDP)          │
│    → el cliente debe reintentar por TCP                       │
│  AD (Authenticated Data): 1 = validado con DNSSEC             │
│  CD (Checking Disabled): 1 = no validar DNSSEC               │
└──────────────────────────────────────────────────────────────┘
```

### Códigos de respuesta (RCODE)

```
┌──────────────────────────────────────────────────────────────┐
│  status: XXXXX                                               │
│                                                              │
│  NOERROR (0)   → Consulta exitosa (puede tener o no answer) │
│  NXDOMAIN (3)  → El dominio NO existe                        │
│  SERVFAIL (2)  → Error en el servidor (no pudo resolver)     │
│  REFUSED (5)   → El servidor rechazó la consulta             │
│  FORMERR (1)   → Consulta malformada                         │
│                                                              │
│  Distinción importante:                                      │
│  NOERROR + answer vacía = el dominio existe pero NO tiene    │
│    un registro de ese tipo (NODATA). Ej: consultar AAAA      │
│    de un dominio que solo tiene A.                           │
│  NXDOMAIN = el nombre NO existe en absoluto.                 │
│                                                              │
│  SERVFAIL causas comunes:                                    │
│  • El resolver no pudo contactar al autoritativo              │
│  • Error de DNSSEC (firma inválida)                           │
│  • Timeout al recorrer la cadena de delegación                │
│  • Configuración errónea en el autoritativo                   │
└──────────────────────────────────────────────────────────────┘
```

```bash
# Ver flags y status
dig www.example.com | head -6
# ;; ->>HEADER<<- opcode: QUERY, status: NOERROR, id: 54321
# ;; flags: qr rd ra; QUERY: 1, ANSWER: 1, ...

# Forzar consulta directa al autoritativo (ver flag aa)
dig @ns1.example.com www.example.com
# ;; flags: qr aa rd; ...
#           ── ──
#           │  └── aa=1: respuesta autoritativa
#           └── qr=1: es una respuesta

# Consultar un dominio que no existe
dig noexiste.example.com
# ;; ->>HEADER<<- status: NXDOMAIN
```

---

## EDNS y extensiones modernas

El DNS original (RFC 1035) tenía un límite de **512 bytes** por paquete UDP. Esto se volvió insuficiente con IPv6 (respuestas más grandes) y DNSSEC (firmas criptográficas). EDNS (Extension mechanisms for DNS) resuelve esto:

### EDNS0 (RFC 6891)

```
┌──────────────────────────────────────────────────────────────┐
│  DNS original: máximo 512 bytes por UDP                      │
│  Si la respuesta excede 512B → flag TC=1 → reintentar por TCP│
│                                                              │
│  EDNS0: permite anunciar un buffer UDP mayor                 │
│                                                              │
│  ;; OPT PSEUDOSECTION:                                       │
│  ; EDNS: version: 0, flags:; udp: 4096                       │
│                                     ────                     │
│                                     "puedo recibir hasta     │
│                                      4096 bytes por UDP"     │
│                                                              │
│  El cliente anuncia su tamaño máximo UDP en la consulta.     │
│  El servidor responde hasta ese límite por UDP.              │
│  Si aun así excede → TCP.                                    │
│                                                              │
│  Valor estándar recomendado: 1232 bytes                      │
│  (cabe en un paquete IPv6 sin fragmentación:                 │
│   1280 MTU mínimo IPv6 - 40 IPv6 - 8 UDP = 1232)            │
└──────────────────────────────────────────────────────────────┘
```

### DNS sobre TCP

TCP no es solo para respuestas grandes. RFC 7766 establece que DNS sobre TCP es obligatorio, no opcional:

```
# Cuándo se usa TCP/53:
# • Respuestas que exceden el tamaño UDP
# • Transferencias de zona (AXFR/IXFR)
# • Cuando el flag TC=1 en la respuesta UDP
# • Cada vez más usado como transporte primario

# Firewall: SIEMPRE abrir TCP/53 y UDP/53
# Bloquear TCP/53 rompe DNSSEC, zonas transfer, y respuestas grandes
```

### DNS cifrado

```
┌──────────────────────────────────────────────────────────────┐
│  DNS tradicional (UDP/53): texto claro                       │
│  → Tu ISP ve cada dominio que resuelves                      │
│  → Vulnerable a manipulación (DNS spoofing)                  │
│                                                              │
│  Alternativas cifradas:                                      │
│                                                              │
│  DoT (DNS over TLS) — RFC 7858                               │
│  • Puerto 853/TCP                                            │
│  • TLS 1.3 estándar                                          │
│  • Fácil de bloquear (puerto dedicado)                       │
│                                                              │
│  DoH (DNS over HTTPS) — RFC 8484                             │
│  • Puerto 443/TCP (mismo que HTTPS)                          │
│  • Difícil de bloquear (se confunde con tráfico web)         │
│  • Controvertido: ¿el DNS debería ir por HTTPS?              │
│                                                              │
│  DoQ (DNS over QUIC) — RFC 9250                              │
│  • QUIC/UDP (mismo beneficio de 0-RTT que HTTP/3)            │
│  • Más nuevo, menos desplegado                               │
│                                                              │
│  Resolvers que soportan cifrado:                             │
│  • 1.1.1.1 (Cloudflare): DoT + DoH                          │
│  • 8.8.8.8 (Google): DoT + DoH                              │
│  • 9.9.9.9 (Quad9): DoT + DoH                               │
│                                                              │
│  Configurar en systemd-resolved:                             │
│  DNSOverTLS=yes  (en resolved.conf)                          │
└──────────────────────────────────────────────────────────────┘
```

---

## Errores comunes

### 1. No esperar a que el TTL anterior expire antes de cambiar

```
# Escenario: TTL actual = 86400 (24h)
# Quieres cambiar IP urgentemente

# INCORRECTO: cambiar la IP directamente
# → Algunos usuarios siguen viendo la IP vieja hasta 24 horas

# CORRECTO: planificar con antelación
# Día 1: bajar TTL a 300  (esperar 24h hasta que cachés expiren)
# Día 2: cambiar la IP    (en ≤5 min todos ven la nueva)
# Día 3: subir TTL a 3600

# Si la urgencia no permite esperar:
# Cambiar la IP + mantener ambos servidores funcionando
# durante el periodo de transición (hasta 24h)
```

### 2. Confundir NXDOMAIN con NODATA

```bash
# NXDOMAIN: el nombre NO EXISTE en absoluto
dig noexiste.example.com A
# status: NXDOMAIN
# → No hay ningún registro de ningún tipo para este nombre

# NODATA: el nombre EXISTE pero no tiene ese tipo de registro
dig example.com AAAA     # (si solo tiene A)
# status: NOERROR
# ANSWER section: vacía
# → example.com existe, pero no tiene registro AAAA

# Importancia: NXDOMAIN aplica a TODOS los tipos.
# Si consultas A y recibes NXDOMAIN, tampoco existe AAAA, MX, etc.
# Si recibes NODATA, puede tener otros tipos de registro.
```

### 3. Bloquear TCP/53 en el firewall

```bash
# Muchos administradores solo abren UDP/53 pensando que
# "DNS solo usa UDP". Esto rompe:
# • DNSSEC (las firmas hacen que las respuestas excedan 512B)
# • Transferencias de zona (AXFR siempre usa TCP)
# • Respuestas con muchos registros (round-robin extenso)
# • El fallback cuando UDP retorna TC=1

# SIEMPRE abrir AMBOS:
sudo firewall-cmd --add-service=dns --permanent   # abre 53/tcp + 53/udp
```

### 4. Ignorar el negative cache al crear subdominios nuevos

```bash
# Si alguien consultó new.example.com ANTES de que existiera:
# → NXDOMAIN cacheado por el Minimum TTL del SOA

# Si el Minimum es 86400 (24h):
# → Nadie verá new.example.com durante hasta 24h
# → Aunque ya lo creaste en el autoritativo

# Prevención: mantener el Minimum del SOA razonable (300-3600)
```

### 5. No verificar la respuesta de dig vs getent

```bash
# dig consulta SOLO DNS (ignora /etc/hosts)
dig myserver +short
# (puede dar NXDOMAIN)

# getent hosts consulta según nsswitch.conf (incluye /etc/hosts)
getent hosts myserver
# 192.168.1.50  myserver  (encontrado en /etc/hosts)

# Si una aplicación no puede resolver un nombre pero dig sí:
# → Problema con nsswitch.conf o /etc/hosts
# Si dig resuelve pero la app no:
# → Posible problema con el stub resolver o nscd
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│               Resolución DNS Cheatsheet                      │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  TIPOS DE CONSULTA:                                          │
│    Recursiva: cliente→resolver, "resuélvelo tú" (RD=1)       │
│    Iterativa: resolver→auth, "dime lo que sepas" (referral)  │
│    Stub resolver: solo envía recursivas, no itera            │
│                                                              │
│  FLUJO:                                                      │
│    App → getaddrinfo() → stub → /etc/hosts → resolver        │
│    Resolver → root → TLD → autoritativo → caché → respuesta  │
│                                                              │
│  CACHÉ (multinivel):                                         │
│    App (browser) → SO (systemd-resolved) → Resolver (8.8.8.8)│
│    TTL decrementa en caché; al llegar a 0 → nueva consulta   │
│    Negative cache: NXDOMAIN también se cachea (SOA Minimum)  │
│                                                              │
│  TTL:                                                        │
│    60-300s   = cambios rápidos / failover                    │
│    3600s     = valor normal                                  │
│    86400s    = muy estable                                   │
│    Bajar TTL ANTES de migrar, no después                     │
│    "Propagación" = esperar expiración de cachés              │
│                                                              │
│  STUB RESOLVER (/etc/resolv.conf):                           │
│    nameserver IP       resolver recursivo (máx 3)            │
│    search dom1 dom2    search domains                        │
│    options ndots:N     puntos mínimos para tratar como FQDN  │
│    FQDN con trailing dot: evita search domains               │
│                                                              │
│  NSSWITCH (/etc/nsswitch.conf):                              │
│    hosts: files dns    primero /etc/hosts, después DNS       │
│    getent hosts = nsswitch; dig = solo DNS                   │
│                                                              │
│  FLAGS:                                                      │
│    QR=1 respuesta   RD=1 recursión pedida                    │
│    RA=1 recursión OK   AA=1 autoritativo                     │
│    TC=1 truncado (reintentar TCP)                            │
│                                                              │
│  RCODE:                                                      │
│    NOERROR   éxito (ANSWER puede estar vacía = NODATA)       │
│    NXDOMAIN  nombre no existe (ningún tipo)                  │
│    SERVFAIL  error del servidor (DNSSEC, timeout, etc.)      │
│    REFUSED   consulta rechazada                              │
│                                                              │
│  SECCIONES DE RESPUESTA:                                     │
│    QUESTION    qué preguntaste                               │
│    ANSWER      registros pedidos                             │
│    AUTHORITY   NS de la zona                                 │
│    ADDITIONAL  IPs de los NS (glue)                          │
│                                                              │
│  DNS CIFRADO:                                                │
│    DoT (853/TCP) — fácil de bloquear                        │
│    DoH (443/TCP) — difícil de bloquear                      │
│    DoQ (QUIC/UDP) — más nuevo                               │
│                                                              │
│  FIREWALL:                                                   │
│    SIEMPRE abrir 53/tcp Y 53/udp                             │
│                                                              │
│  COMANDOS:                                                   │
│    dig domain TYPE             consulta DNS directa          │
│    dig +short domain           solo el valor                 │
│    dig @server domain          preguntar a NS específico     │
│    getent hosts domain         resolver como el SO           │
│    resolvectl statistics       ver caché de resolved         │
│    resolvectl flush-caches     limpiar caché                 │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Trazar una resolución paso a paso

Eres el resolver recursivo `1.1.1.1` y recibes esta consulta de un cliente:

```
;; QUESTION SECTION:
;docs.api.mycompany.com.    IN    A
```

Tu caché está **completamente vacío**. Traza la resolución completa:

1. ¿A quién consultas primero? ¿Qué preguntas exactamente?
2. ¿Qué tipo de respuesta recibes (answer o referral)? ¿Qué contiene?
3. Repite hasta obtener la IP final. ¿Cuántas consultas iterativas hiciste?

Ahora, 30 segundos después recibes una consulta para `www.mycompany.com`:
4. ¿Necesitas consultar al root server? ¿Por qué o por qué no?
5. ¿Cuántas consultas iterativas necesitas ahora?

10 minutos después, recibes otra consulta para `docs.api.mycompany.com`:
6. Si el TTL del registro A era 3600, ¿necesitas consultar al autoritativo?

**Pregunta de reflexión**: Si `mycompany.com` delegara `api.mycompany.com` a un servidor DNS diferente (como AWS Route53), ¿cambiaría el número de consultas iterativas en tu traza? ¿Cuántas serían?

---

### Ejercicio 2: Diagnosticar con flags y secciones

Analiza estas respuestas de `dig` e identifica qué está pasando en cada caso:

**Caso A**:
```
;; ->>HEADER<<- status: NOERROR
;; flags: qr aa rd
;; ANSWER SECTION:
www.example.com.    3600    IN    A    93.184.216.34
```

**Caso B**:
```
;; ->>HEADER<<- status: NOERROR
;; flags: qr rd ra
;; ANSWER SECTION:
www.example.com.    2847    IN    A    93.184.216.34
```

**Caso C**:
```
;; ->>HEADER<<- status: NXDOMAIN
;; flags: qr rd ra
;; AUTHORITY SECTION:
example.com.    3600    IN    SOA    ns1.example.com. admin.example.com. ...
```

**Caso D**:
```
;; ->>HEADER<<- status: SERVFAIL
;; flags: qr rd ra
```

**Para cada caso, responde**:
1. ¿La respuesta viene del autoritativo o de un caché?
2. ¿Se encontró el registro? Si es NXDOMAIN, ¿cuánto tiempo se cacheará?
3. ¿Qué indica el TTL en el caso B?
4. ¿Qué podría causar SERVFAIL en el caso D? (Lista 3 posibles causas.)

**Pregunta de reflexión**: En el caso B, ¿por qué el TTL es 2847 y no un número "redondo" como 3600? ¿Qué te dice esto sobre cuándo se cacheó la respuesta?

---

### Ejercicio 3: ndots y search domains

Tienes este `/etc/resolv.conf`:

```
nameserver 10.96.0.10
search prod.svc.cluster.local svc.cluster.local cluster.local
options ndots:5
```

**Para cada nombre**, lista TODAS las consultas DNS que el stub resolver hará (en orden) antes de obtener una respuesta exitosa:

1. `redis` (0 puntos)
2. `redis.prod` (1 punto)
3. `api.example.com` (2 puntos)
4. `api.example.com.` (con trailing dot)
5. `very.deep.sub.domain.example.com` (4 puntos)
6. `a.b.c.d.e.example.com` (5 puntos)

**Responde**:
1. ¿Cuántas consultas DNS se desperdician para resolver `api.example.com`?
2. ¿Cómo el trailing dot elimina este problema?
3. Si cambiaras `ndots:5` a `ndots:1`, ¿cómo cambiaría el comportamiento para `api.example.com`?
4. ¿Por qué Kubernetes usa `ndots:5` por defecto? ¿Qué problema resuelve para los servicios internos?

**Pregunta de reflexión**: Un servicio en Kubernetes tarda 500ms en resolver `api.external.com` (un dominio externo). Sin embargo, `api.external.com.` (con trailing dot) se resuelve en 50ms. ¿Por qué la diferencia es tan grande? ¿Qué recomendarías para mejorar el rendimiento de resolución DNS en Kubernetes?

---

> **Siguiente tema**: T04 — /etc/resolv.conf y /etc/hosts (configuración del cliente, nsswitch.conf)
