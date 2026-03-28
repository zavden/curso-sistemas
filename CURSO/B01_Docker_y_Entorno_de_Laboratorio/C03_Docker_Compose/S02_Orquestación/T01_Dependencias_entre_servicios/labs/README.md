# Lab — Dependencias entre servicios

## Objetivo

Laboratorio práctico para entender el control de orden de arranque en
Docker Compose: por qué el arranque paralelo causa problemas, por qué
`depends_on` simple no es suficiente, cómo resolverlo con healthchecks,
y cómo usar `service_completed_successfully` para tareas de inicialización.

## Prerequisitos

- Docker y Docker Compose v2 instalados (`docker compose version`)
- Imágenes descargadas: `docker pull alpine:latest && docker pull postgres:16-alpine`
- Estar en el directorio `labs/` de este tópico

## Archivos del laboratorio

| Archivo | Propósito |
|---|---|
| `compose.parallel.yml` | Tres servicios sin `depends_on` (arranque paralelo) |
| `compose.deps-simple.yml` | API con `depends_on` simple hacia PostgreSQL |
| `compose.deps-healthy.yml` | API con `depends_on` + healthcheck |
| `compose.deps-init.yml` | Tarea init + app con `service_completed_successfully` |

---

## Parte 1 — Arranque paralelo (sin depends_on)

### Objetivo

Demostrar que sin `depends_on`, Compose arranca todos los servicios
en paralelo y no hay orden garantizado.

### Paso 1.1: Examinar el archivo

```bash
cat compose.parallel.yml
```

Tres servicios (`api`, `db`, `cache`) sin `depends_on`. Cada uno
imprime su timestamp de arranque.

### Paso 1.2: Levantar y observar

```bash
docker compose -f compose.parallel.yml up -d
```

### Paso 1.3: Ver timestamps

```bash
docker compose -f compose.parallel.yml logs
```

Salida esperada:

```
api-1    | API started at HH:MM:SS
db-1     | DB started at HH:MM:SS
cache-1  | Cache started at HH:MM:SS
```

Los tres servicios arrancan prácticamente al mismo tiempo. No hay orden
garantizado — cualquiera puede arrancar primero.

### Paso 1.4: Repetir para confirmar

```bash
docker compose -f compose.parallel.yml down
docker compose -f compose.parallel.yml up -d
docker compose -f compose.parallel.yml logs
```

El orden puede variar entre ejecuciones. Compose no garantiza ningún
orden sin `depends_on`.

### Paso 1.5: Limpiar

```bash
docker compose -f compose.parallel.yml down
```

---

## Parte 2 — depends_on simple

### Objetivo

Demostrar que `depends_on` sin condición espera a que el contenedor
esté `running`, pero **no** a que el servicio esté listo para aceptar
conexiones.

### Paso 2.1: Examinar el archivo

```bash
cat compose.deps-simple.yml
```

`api` tiene `depends_on: - db`. Compose arrancará `db` primero, y cuando
el **contenedor** esté running, arrancará `api`.

Antes de ejecutar, responde mentalmente:

- ¿API podrá conectar a PostgreSQL al arrancar?
- ¿Contenedor running = PostgreSQL aceptando conexiones?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.2: Levantar

```bash
docker compose -f compose.deps-simple.yml up -d
```

### Paso 2.3: Observar logs

```bash
sleep 2
docker compose -f compose.deps-simple.yml logs api
```

Salida esperada:

```
api-1  | API started at HH:MM:SS
api-1  | DB: connection FAILED
```

`api` arrancó después de que el contenedor `db` estuviera running.
Pero PostgreSQL aún estaba inicializando sus archivos de datos y no
aceptaba conexiones TCP.

```bash
docker compose -f compose.deps-simple.yml logs db | tail -3
```

Los logs de PostgreSQL muestran la inicialización que aún no ha
terminado cuando `api` intenta conectar.

### Paso 2.4: Verificar estado

```bash
docker compose -f compose.deps-simple.yml ps
```

Ambos contenedores están `Up`. El contenedor `db` está running, pero
PostgreSQL dentro de él tardó varios segundos en estar listo.

`depends_on` simple **solo ordena el arranque de contenedores**. No
espera a que el servicio dentro del contenedor esté funcional.

### Paso 2.5: Limpiar

```bash
docker compose -f compose.deps-simple.yml down -v
```

`-v` elimina los volúmenes de PostgreSQL para que el próximo arranque
haga la inicialización completa.

---

## Parte 3 — depends_on con healthcheck

### Objetivo

Demostrar que `depends_on` con `condition: service_healthy` espera a
que el healthcheck del servicio pase antes de arrancar los dependientes.

### Paso 3.1: Examinar el archivo

```bash
cat compose.deps-healthy.yml
```

Diferencias con la parte anterior:
- `db` tiene un `healthcheck:` que ejecuta `pg_isready -U postgres`
- `api` usa `depends_on: db: condition: service_healthy` en lugar de
  una lista simple

### Paso 3.2: Levantar y observar el proceso

```bash
docker compose -f compose.deps-healthy.yml up -d
```

### Paso 3.3: Monitorear el estado de salud

```bash
docker compose -f compose.deps-healthy.yml ps
```

Ejecutar `ps` varias veces durante los primeros 10-15 segundos:

```bash
sleep 3
docker compose -f compose.deps-healthy.yml ps
```

Salida esperada (durante inicialización):

```
NAME        SERVICE   STATUS                    PORTS
labs-db-1   db        Up (health: starting)     5432/tcp
```

`api` aún no aparece — Compose está esperando a que `db` sea `healthy`.

```bash
sleep 10
docker compose -f compose.deps-healthy.yml ps
```

Salida esperada (después de healthcheck):

```
NAME         SERVICE   STATUS                PORTS
labs-api-1   api       Up
labs-db-1    db        Up (healthy)          5432/tcp
```

`db` pasó de `health: starting` a `healthy`. Solo entonces Compose
arrancó `api`.

### Paso 3.4: Verificar conexión

```bash
docker compose -f compose.deps-healthy.yml logs api
```

Salida esperada:

```
api-1  | API started at HH:MM:SS
api-1  | DB: connected
```

Esta vez la conexión **funciona** porque `api` no arrancó hasta que
`pg_isready` confirmó que PostgreSQL está aceptando conexiones.

### Paso 3.5: Comparar timestamps

```bash
docker compose -f compose.deps-healthy.yml logs | grep "started at"
```

El timestamp de `api` es varios segundos posterior al de `db`. Compose
esperó la inicialización completa.

### Paso 3.6: Orden de apagado

```bash
docker compose -f compose.deps-healthy.yml down -v 2>&1
```

Observar el orden: Compose detiene `api` primero (dependiente), luego
`db` (dependencia). El apagado sigue el **orden inverso** al arranque.

---

## Parte 4 — service_completed_successfully

### Objetivo

Demostrar `service_completed_successfully` para esperar a que una tarea
de inicialización termine con exit code 0 antes de arrancar el servicio
principal.

### Paso 4.1: Examinar el archivo

```bash
cat compose.deps-init.yml
```

Dos servicios:
- `init`: ejecuta una tarea de 3 segundos y termina
- `app`: espera a que `init` complete exitosamente

### Paso 4.2: Levantar

```bash
docker compose -f compose.deps-init.yml up -d
```

### Paso 4.3: Monitorear durante la inicialización

```bash
docker compose -f compose.deps-init.yml ps
```

Salida esperada (durante los primeros 3 segundos):

```
NAME          SERVICE   STATUS
labs-init-1   init      Up
```

Solo `init` está corriendo. `app` no arranca hasta que `init` termine.

### Paso 4.4: Después de la inicialización

```bash
sleep 5
docker compose -f compose.deps-init.yml ps -a
```

Salida esperada:

```
NAME          SERVICE   STATUS
labs-app-1    app       Up
labs-init-1   init      Exited (0)
```

`init` terminó con exit code 0 (`Exited (0)`). Solo entonces `app`
arrancó.

### Paso 4.5: Verificar logs

```bash
docker compose -f compose.deps-init.yml logs
```

Salida esperada:

```
init-1  | Running initialization...
init-1  | Init complete
app-1   | App started after init at HH:MM:SS
```

La secuencia es clara: init completa, luego app arranca.

### Paso 4.6: Qué pasa si init falla

Para ver el comportamiento con un fallo, modifica temporalmente el
comando de init:

```bash
docker compose -f compose.deps-init.yml down
```

```bash
docker compose -f compose.deps-init.yml run --rm init sh -c 'echo "Init failed" && exit 1' 2>&1
```

Si `init` termina con exit code != 0, `app` **no arranca** y Compose
muestra un error. Esto protege contra migraciones o seeds que fallan.

### Paso 4.7: Limpiar

```bash
docker compose -f compose.deps-init.yml down
```

---

## Limpieza final

```bash
# Eliminar recursos del lab (si alguno quedó)
docker compose -f compose.parallel.yml down 2>/dev/null
docker compose -f compose.deps-simple.yml down -v 2>/dev/null
docker compose -f compose.deps-healthy.yml down -v 2>/dev/null
docker compose -f compose.deps-init.yml down 2>/dev/null
```

---

## Conceptos reforzados

1. Sin `depends_on`, Compose arranca todos los servicios **en paralelo**.
   No hay garantía de orden, y un servicio puede intentar conectar a
   otro que aún no está listo.

2. `depends_on` simple (lista) solo espera a que el **contenedor** esté
   `running`. Un contenedor running no significa que el servicio dentro
   de él esté aceptando conexiones.

3. `depends_on` con `condition: service_healthy` espera a que el
   healthcheck del servicio pase. Para bases de datos, usar el comando
   nativo de readiness (`pg_isready`, `redis-cli ping`, etc.).

4. `service_completed_successfully` espera a que un servicio **termine**
   con exit code 0. Es ideal para tareas de inicialización (migraciones,
   seeds, configuración) que deben completarse antes de arrancar la app.

5. El orden de apagado (`down`) es el **inverso** del arranque: primero
   se detienen los servicios dependientes, luego las dependencias.
