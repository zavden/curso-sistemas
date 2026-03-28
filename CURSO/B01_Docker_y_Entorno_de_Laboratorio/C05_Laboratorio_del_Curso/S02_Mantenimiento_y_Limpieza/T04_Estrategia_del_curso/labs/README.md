# Lab — Estrategia del curso

## Objetivo

Establecer la rutina de mantenimiento del laboratorio del curso:
entender por que usar exec en lugar de run, practicar el ciclo diario,
ejecutar limpieza segura, y verificar que el lab sobrevive a la
limpieza.

## Prerequisitos

- Docker y Docker Compose instalados
- Entorno del curso levantado (compose.yml corriendo)
- Estar en el directorio del compose.yml del curso

## Archivos del laboratorio

Este lab no tiene archivos de soporte. Se ejecuta contra el entorno
del curso ya levantado.

---

## Parte 1 — exec vs run

### Objetivo

Demostrar por que `exec` es preferible a `run` para ejecutar comandos
durante las sesiones de trabajo.

### Paso 1.1: Ejecutar con run

```bash
docker compose run debian-dev echo "test 1"
docker compose run debian-dev echo "test 2"
docker compose run debian-dev echo "test 3"
```

### Paso 1.2: Contar contenedores creados

```bash
docker ps -a --format "table {{.Names}}\t{{.Status}}" | head -10
```

`run` crea un contenedor nuevo para cada ejecucion. Despues de 3
comandos hay 3 contenedores detenidos adicionales.

### Paso 1.3: Ejecutar con exec

```bash
docker compose exec debian-dev echo "test 1"
docker compose exec debian-dev echo "test 2"
docker compose exec debian-dev echo "test 3"
```

### Paso 1.4: Contar de nuevo

```bash
docker ps -a --format "table {{.Names}}\t{{.Status}}" | head -10
```

`exec` ejecuta dentro del contenedor existente. No crea nada nuevo.
Despues de 3 comandos, el numero de contenedores no cambio.

### Paso 1.5: Impacto despues de muchos ejercicios

```bash
echo "=== Contenedores acumulados por 'run' ==="
docker ps -a --filter status=exited | wc -l
```

Imagina 20 ejercicios al dia durante un mes: con `run`, serian ~600
contenedores detenidos acumulados. Con `exec`, cero.

### Paso 1.6: Limpiar contenedores de run

```bash
docker container prune -f
```

Regla para el curso: **usar exec para todo**. Solo usar `run` cuando
se necesita un contenedor aislado intencionalmente.

---

## Parte 2 — Ciclo diario

### Objetivo

Practicar el flujo de trabajo diario del laboratorio: levantar,
trabajar, detener.

### Paso 2.1: Verificar estado actual

```bash
docker compose ps
```

Si los contenedores estan corriendo, continua. Si no, levanta el
entorno:

```bash
docker compose up -d
```

### Paso 2.2: Sesion de trabajo simulada

```bash
docker compose exec debian-dev bash -c '
echo "=== Sesion de trabajo ==="
echo "User: $(whoami)"
echo "Hostname: $(hostname)"
echo "Dir: $(pwd)"
echo "GCC: $(gcc --version | head -1)"
echo "Rust: $(rustc --version)"
'
```

### Paso 2.3: Detener con stop (preserva estado)

```bash
docker compose stop
docker compose ps
```

Los contenedores quedan en estado "exited" pero referenciados por
Compose. Su estado interno (historial de shell, archivos temporales)
se preserva.

### Paso 2.4: Reanudar con start

```bash
docker compose start
docker compose ps
```

`start` reanuda los contenedores detenidos. Es instantaneo (no
recrea nada).

### Paso 2.5: Detener con down (elimina contenedores)

```bash
docker compose down
docker compose ps
```

`down` elimina los contenedores y la red. Los volumenes e imagenes
se preservan.

### Paso 2.6: Levantar despues de down

```bash
docker compose up -d
docker compose ps
```

`up -d` recrea los contenedores desde las imagenes existentes. El
estado interno (historial, archivos temporales en el contenedor) se
pierde, pero el volumen `workspace` conserva los datos persistentes.

### Paso 2.7: Resumen del ciclo

```
Empezar a trabajar:   docker compose up -d   (o start si usaste stop)
Durante la sesion:    docker compose exec debian-dev bash
Al terminar:          docker compose stop    (preserva estado)
                      docker compose down    (limpia, preserva volumenes)
Reset total:          docker compose down -v (elimina todo)
```

La recomendacion para el dia a dia es usar `stop` y `start`. Usar
`down` solo cuando se quiere limpiar. Usar `down -v` solo para
resetear el lab por completo.

---

## Parte 3 — Limpieza segura

### Objetivo

Ejecutar la rutina de limpieza segura y verificar que el lab no se
ve afectado.

### Paso 3.1: Espacio antes

```bash
echo "=== Espacio antes ==="
docker system df
```

### Paso 3.2: Crear basura simulada

```bash
for i in 1 2 3; do
    docker run --name "lab-trash-$i" alpine echo "trash $i"
done
```

### Paso 3.3: Limpieza segura (rutina diaria)

```bash
echo "=== Limpieza segura ==="
docker container prune -f
docker image prune -f
```

Estos dos comandos son seguros de ejecutar despues de cada sesion:

- `container prune` elimina contenedores detenidos (los del lab estan
  corriendo y no se ven afectados)
- `image prune` elimina imagenes dangling (las del lab tienen tag y
  estan en uso)

### Paso 3.4: Verificar que el lab sigue funcionando

```bash
docker compose ps
docker compose exec debian-dev gcc --version | head -1
docker compose exec alma-dev rustc --version
```

El lab no se vio afectado por la limpieza.

### Paso 3.5: Espacio despues

```bash
echo "=== Espacio despues ==="
docker system df
```

### Paso 3.6: Limpieza mensual (build cache)

```bash
docker builder prune --keep-storage 2GB -f
```

`--keep-storage 2GB` mantiene las capas de cache mas recientes (hasta
2GB) y elimina el exceso. Los rebuilds del lab siguen siendo rapidos.

### Paso 3.7: Verificar volumenes antes de prune

```bash
docker volume ls
```

Los volumenes del lab aparecen con labels de Compose. **Nunca ejecutar
`docker volume prune` sin verificar primero** que los volumenes del
lab no estan en riesgo (esto puede pasar si los contenedores estan
detenidos con `stop`).

---

## Limpieza final

```bash
docker rm -f lab-trash-1 lab-trash-2 lab-trash-3 2>/dev/null
```

No es necesario limpiar el entorno del curso — seguira usandose
en los siguientes bloques.

---

## Conceptos reforzados

1. **exec** ejecuta dentro del contenedor existente (sin crear nada
   nuevo). **run** crea un contenedor nuevo cada vez. Para el trabajo
   diario del curso, usar siempre exec.

2. **stop/start** preserva el estado del contenedor (historial,
   archivos temporales). **down** elimina contenedores pero preserva
   volumenes. **down -v** elimina todo.

3. La rutina de limpieza segura (`container prune` + `image prune`)
   no afecta al lab activo porque sus contenedores estan corriendo y
   sus imagenes estan en uso.

4. `docker builder prune --keep-storage 2GB` es un buen compromiso
   mensual: libera espacio pero mantiene cache reciente para que los
   rebuilds sean rapidos.

5. **Nunca ejecutar `docker volume prune`** sin verificar que los
   volumenes del lab no estan en riesgo. Si los contenedores se
   detuvieron con `stop` (no `down`), los volumenes siguen
   referenciados y estan seguros.
