# DESIGN_LAB.md — Guía de diseño de laboratorios

Documento de referencia para cualquier agente o persona que cree laboratorios
prácticos para este curso. Define la estructura de archivos, convenciones de
contenido, enfoque pedagógico, y anti-patrones a evitar.

## Ejemplo de referencia

El lab canónico que establece el patrón se encuentra en:

```
CURSO/B01_Docker_y_Entorno_de_Laboratorio/
  C01_Arquitectura_de_Docker/
    S01_Modelo_de_Imágenes/
      T01_Imágenes_y_capas/
        README.md           ← Teoría del tópico
        LABS.md             ← Puente entre teoría y práctica
        labs/
          README.md         ← Ejercicios paso a paso
          hello.c           ← Archivo de soporte
          Dockerfile.layers ← Archivo de soporte
          ...
```

Usar como modelo para todos los labs futuros.

---

## 1. Filosofía

Los labs son **ejercicios resueltos paso a paso** que el estudiante ejecuta
manualmente. El objetivo es que el estudiante **escriba los comandos**, observe
los resultados, y conecte la práctica con la teoría del README.md del tópico.

No son demos, no son scripts automatizados, no son videos. Son guías que
asumen que el estudiante tiene una terminal abierta y sigue cada paso
escribiendo los comandos.

---

## 2. Estructura de archivos

Cada tópico que tiene laboratorio debe tener esta estructura:

```
T01_nombre_del_topico/
├── README.md       ← Teoría (ya existe para todos los tópicos)
├── LABS.md         ← Puente: describe el lab, prerequisitos, cómo ejecutar
└── labs/
    ├── README.md   ← Documento principal del lab (ejercicios paso a paso)
    └── (soporte)   ← Dockerfiles, código fuente, configs, scripts auxiliares
```

### README.md del tópico

Ya existe. Contiene la teoría. **No modificar** para agregar labs — ese es el
rol de LABS.md.

### LABS.md

Archivo nuevo al mismo nivel que el README.md del tópico. Sirve de puente entre
la teoría y la práctica. Su contenido:

1. **Descripción** — qué cubre el laboratorio (1-2 frases)
2. **Prerequisitos** — qué debe estar instalado, qué descargar previamente
3. **Contenido del laboratorio** — tabla con cada parte: número, concepto, qué
   se demuestra
4. **Archivos** — árbol del directorio `labs/` con descripción de cada archivo
5. **Cómo ejecutar** — instrucciones para entrar al directorio y seguir el lab
6. **Tiempo estimado** — aproximación del tiempo total
7. **Recursos creados** — tabla de imágenes Docker, contenedores, volúmenes,
   redes u otros recursos que el lab crea y si se eliminan al final

### labs/README.md

Documento principal del laboratorio. Contiene todos los ejercicios en un solo
archivo, organizados en partes progresivas. Estructura:

1. **Título y objetivo general**
2. **Prerequisitos** (breve, puede apuntar a LABS.md)
3. **Tabla de archivos del laboratorio**
4. **Partes**, cada una con:
   - Objetivo de la parte
   - Pasos numerados (`Paso X.Y`) con:
     - Comando(s) a ejecutar (en bloque de código bash)
     - Salida esperada (cuando aplica)
     - Análisis/explicación del resultado
   - Limpieza al final de la parte (cuando corresponda)
5. **Limpieza final** — sección explícita con todos los comandos para eliminar
   recursos creados durante el lab
6. **Conceptos reforzados** — lista numerada con los puntos clave demostrados

### Archivos de soporte

Archivos auxiliares dentro de `labs/` que el lab referencia:

- Dockerfiles
- Código fuente (C, Rust, Python, shell, etc.)
- Archivos de configuración (compose.yml, nginx.conf, etc.)
- Datos de prueba

---

## 3. Convenciones de contenido

### Idioma

- **Texto explicativo**: español
- **Código, comandos, nombres de archivo, variables**: inglés
- **Nombres de imágenes Docker**: prefijo `lab-` (ej. `lab-layers`, `lab-bad`)
- **Nombres de contenedores**: descriptivos en inglés (ej. `cow-test`,
  `wh-demo`)

### Nombres de archivos de soporte

- **Descriptivos**, basados en lo que demuestran, no numerados
- Correcto: `Dockerfile.layers`, `Dockerfile.bad`, `Dockerfile.good`
- Incorrecto: `Dockerfile.lab01`, `Dockerfile.lab03a`

### Bloques de código

Usar siempre el lenguaje correcto en el bloque:

~~~markdown
```bash
docker build -t lab-example .
```

```dockerfile
FROM debian:bookworm
RUN apt-get update
```

```c
#include <stdio.h>
int main(void) { return 0; }
```

```json
{
    "key": "value"
}
```
~~~

### Salidas esperadas

Incluir salidas esperadas después de los comandos cuando:
- El resultado no es obvio
- El estudiante necesita verificar que va por buen camino
- El resultado revela algo importante sobre el concepto

Formato:

```markdown
Salida esperada:

` ` `
REPOSITORY    TAG       SIZE
lab-bad       latest    ~170MB
lab-good      latest    ~117MB
` ` `
```

Usar `~` para valores aproximados que varían entre sistemas.

### Valores hardcodeados

Nunca hardcodear valores que dependan del entorno:
- Hashes de imágenes → usar `<hash>`
- Tamaños exactos → usar `~117MB`
- Paths absolutos del host → describir cómo obtenerlos
- Cantidad de capas → no hardcodear en código fuente

---

## 4. Enfoque pedagógico

### Predecir antes de ejecutar

Antes de un paso revelador, pedir al estudiante que prediga el resultado:

```markdown
Antes de construir, responde mentalmente:

- ¿Cuántas capas con contenido se crearán?
- ¿Cuáles instrucciones solo generan metadata?

Intenta predecir antes de continuar al siguiente paso.
```

La respuesta viene en el paso siguiente, **nunca junto a la pregunta**. Dar
espacio para pensar.

### Progresión

Los conceptos se presentan de más simple a más complejo. Cada parte del lab
construye sobre las anteriores. El estudiante no debería necesitar saltar
partes.

### Conexión con la teoría

Los ejercicios deben reforzar los conceptos explicados en el README.md del
tópico. No introducir conceptos nuevos que no estén en la teoría — el lab
valida, no enseña desde cero.

### Limpieza explícita

La limpieza de recursos (imágenes, contenedores, volúmenes) se hace:

- Al final de cada parte (si los recursos de esa parte no se necesitan después)
- En una sección de "Limpieza final" al terminar el lab

La limpieza es **explícita** — el estudiante ejecuta los comandos de limpieza
manualmente. Esto refuerza la conciencia de los recursos creados.

### Conceptos reforzados

Al final del lab, una lista numerada con los puntos clave demostrados. Cada
punto debe ser una afirmación concreta y verificable, no una generalidad.

Correcto:
> Eliminar en un RUN separado **no ahorra espacio** — la capa anterior sigue
> existiendo.

Incorrecto:
> Las capas son importantes en Docker.

---

## 5. Anti-patrones

### NO crear scripts que auto-ejecuten los ejercicios

```bash
# INCORRECTO — elimina el aprendizaje práctico
#!/bin/bash
cmd() { echo "$ $*"; eval "$@"; }
trap cleanup EXIT
cmd docker build -t lab-example .
cmd docker run --rm lab-example
```

El lab ES la guía paso a paso. Un script que ejecuta todo automáticamente
convierte al estudiante en espectador. Además:

- `eval` es frágil e inseguro
- `trap cleanup EXIT` destruye los resultados antes de que el estudiante pueda
  inspeccionarlos
- El script duplica el contenido del lab .md (doble mantenimiento)

### NO crear archivos .md individuales por ejercicio

```
# INCORRECTO — fragmentación innecesaria
labs/
├── LAB_01_anatomia.md
├── LAB_02_cow.md
├── LAB_03_dedup.md
├── lab01.sh
├── lab02.sh
└── lab03.sh
```

Un solo `README.md` con todas las partes. Esto:
- Facilita la navegación (un archivo, no diez)
- Elimina redundancia entre .md y .sh
- Mantiene la progresión clara

### NO dar respuestas junto a las preguntas

```markdown
<!-- INCORRECTO -->
¿Cuántas capas se crearán?
**Respuesta**: 4 capas con contenido y 3 de metadata.
```

```markdown
<!-- CORRECTO -->
¿Cuántas capas se crearán?
Intenta predecir antes de continuar al siguiente paso.

### Paso 1.3: Verificar
(aquí se revela la respuesta al ejecutar docker history)
```

### NO usar emojis

Salvo que el usuario lo pida explícitamente. Ni en el texto, ni en scripts,
ni en salidas formateadas.

### NO descargar imágenes pesadas innecesariamente

Si el concepto se puede demostrar con imágenes pequeñas (alpine, Dockerfiles
de 2-3 líneas), no descargar imágenes de 1GB+ solo para un paso del lab.

### NO depender de acceso root para pasos esenciales

Si un paso requiere root (ej. inspeccionar `/var/lib/docker/`), documentarlo
claramente y ofrecer alternativa sin root (ej. `docker inspect`).

### NO hardcodear valores volátiles

- Hashes de imágenes cambian entre versiones
- Tamaños exactos varían entre arquitecturas y versiones
- Cantidad de capas depende del Dockerfile actual

Usar marcadores como `<hash>`, `~117MB`, o calcular dinámicamente.

---

## 6. Convenciones de nombrado de recursos Docker

### Imágenes

Prefijo `lab-` seguido de nombre descriptivo:

```
lab-layers       ← anatomía de capas
lab-dedup-curl   ← deduplicación (variante curl)
lab-bad          ← patrón incorrecto
lab-gcc          ← caso real con gcc
```

### Contenedores

Nombre descriptivo que indica el concepto:

```
cow-test         ← prueba de copy-on-write
cow-a, cow-b     ← contenedores paralelos para comparar
wh-demo          ← demostración de whiteout
wh-persist       ← prueba de persistencia
```

### Volúmenes y redes

Misma convención — prefijo `lab-` y nombre descriptivo cuando sea necesario.

---

## 7. Checklist para crear un lab nuevo

Antes de dar por terminado un lab, verificar:

- [ ] Existe `LABS.md` al nivel del README.md del tópico
- [ ] Existe `labs/README.md` con todos los ejercicios
- [ ] Los archivos de soporte tienen nombres descriptivos (no numerados)
- [ ] Cada parte tiene: objetivo, pasos numerados, salidas esperadas, limpieza
- [ ] Hay sección de "Limpieza final" al terminar el lab
- [ ] Hay sección de "Conceptos reforzados" al final
- [ ] No hay scripts que auto-ejecuten ejercicios
- [ ] No hay valores hardcodeados que dependan del entorno
- [ ] Los prerequisitos están documentados (en LABS.md y en labs/README.md)
- [ ] La tabla de recursos Docker creados está en LABS.md
- [ ] Se pide al estudiante predecir antes de revelar resultados
- [ ] Todo el texto está en español, todo el código en inglés
- [ ] No hay emojis
